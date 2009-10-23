/*
 * sheep/eval.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/object.h>
#include <sheep/alien.h>
#include <sheep/bool.h>
#include <sheep/code.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/eval.h>

static void note_pending(struct sheep_vm *vm, struct sheep_foreign *foreign)
{
	struct sheep_foreign *prev = NULL, *next = vm->pending;

	while (next) {
		if (foreign->value.live.index >= next->value.live.index)
			break;
		prev = next;
		next = next->value.live.next;
	}

	foreign->value.live.next = next;
	if (prev)
		prev->value.live.next = foreign;
	else
		vm->pending = foreign;
}

static void close_pending(struct sheep_vm *vm, unsigned long basep)
{
	while (vm->pending) {
		struct sheep_foreign *foreign = vm->pending;
		sheep_t value;

		if (foreign->value.live.index < basep)
			break;

		vm->pending = foreign->value.live.next;

		value = vm->stack.items[foreign->value.live.index];
		foreign->state = SHEEP_FOREIGN_CLOSED;
		foreign->value.closed = value;
	}
}

static sheep_t closure(struct sheep_vm *vm, unsigned long basep, sheep_t sheep)
{
	struct sheep_function *function, *closure;
	struct sheep_vector *template, *foreigns;
	unsigned int i;

	sheep_bug_on(sheep->type != &sheep_function_type);
	function = sheep_data(sheep);

	if (!function->foreigns)
		return sheep;

	foreigns = sheep_malloc(sizeof(struct sheep_vector));
	sheep_vector_init(foreigns, 4);

	template = function->foreigns;
	for (i = 0; i < template->nr_items; i++) {
		struct sheep_foreign *foreign;
		unsigned long owner, offset;
		unsigned int dist, slot;

		foreign = template->items[i];
		dist = foreign->value.template.dist;
		slot = foreign->value.template.slot;
		/*
		 * Closure instantiation happens in the owner scope,
		 * thus if the owner distance is 1, the current frame
		 * is the owner.
		 *
		 * Otherwise, the frame is on the call stack.
		 */
		offset = dist - 1;
		if (!offset)
			owner = basep;
		else {
			/*
			 * The callstack contains
			 *	[codep basep function]
			 * for every frame.
			 */
			offset = vm->calls.nr_items - 3 * offset - 1;
			owner = (unsigned long)vm->calls.items[offset];
		}

		foreign = sheep_malloc(sizeof(struct sheep_foreign));
		foreign->state = SHEEP_FOREIGN_LIVE;
		foreign->value.live.index = owner + slot;

		sheep_vector_push(foreigns, foreign);
		note_pending(vm, foreign);
	}

	sheep = sheep_copy_function(vm, function);
	closure = sheep_data(sheep);
	closure->foreigns = foreigns;

	return sheep;
}

static int call(struct sheep_vm *vm, sheep_t callable, unsigned int nr_args,
		sheep_t *value)
{
	struct sheep_function *function;

	if (callable->type == &sheep_alien_type) {
		struct sheep_alien *alien;

		alien = sheep_data(callable);
		*value = alien->function(vm, nr_args);
		return 1 - 2 * !*value;
	}

	sheep_bug_on(callable->type != &sheep_function_type);

	function = sheep_data(callable);
	if (function->nr_parms != nr_args) {
		fprintf(stderr, "%s: too %s arguments\n",
			function->name,
			function->nr_parms < nr_args ? "many" : "few");
		return -1;
	}

	if (function->nr_locals > nr_args)
		sheep_vector_grow(&vm->stack, function->nr_locals - nr_args);

	return 0;
}

static sheep_t __sheep_eval(struct sheep_vm *vm, struct sheep_code *code,
			struct sheep_function *function)
{
	unsigned long *codep = (unsigned long *)code->code.items;
	unsigned long basep = vm->stack.nr_items;
	unsigned int nesting = 0;

	if (function)
		basep -= function->nr_locals;

	for (;;) {
		struct sheep_foreign *forin;
		enum sheep_opcode op;
		unsigned int arg;
		sheep_t tmp;
		int done;

		sheep_decode(*codep, &op, &arg);
		//sheep_code_dump(vm, function, basep, op, arg);

		switch (op) {
		case SHEEP_DROP:
			sheep_vector_pop(&vm->stack);
			break;
		case SHEEP_LOCAL:
			tmp = vm->stack.items[basep + arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_LOCAL:
			tmp = sheep_vector_pop(&vm->stack);
			vm->stack.items[basep + arg] = tmp;
			break;
		case SHEEP_FOREIGN:
			forin = function->foreigns->items[arg];
			if (forin->state == SHEEP_FOREIGN_LIVE)
				tmp = vm->stack.items[forin->value.live.index];
			else
				tmp = forin->value.closed;
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_FOREIGN:
			tmp = sheep_vector_pop(&vm->stack);
			forin = function->foreigns->items[arg];
			if (forin->state == SHEEP_FOREIGN_LIVE)
				vm->stack.items[forin->value.live.index] = tmp;
			else
				forin->value.closed = tmp;
			break;
		case SHEEP_GLOBAL:
			tmp = vm->globals.items[arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_GLOBAL:
			tmp = sheep_vector_pop(&vm->stack);
			vm->globals.items[arg] = tmp;
			break;
		case SHEEP_CLOSURE:
			tmp = vm->globals.items[arg];
			tmp = closure(vm, basep, tmp);
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_CALL:
			tmp = sheep_vector_pop(&vm->stack);

			done = call(vm, tmp, arg, &tmp);
			if (done < 0)
				goto err;
			else if (done) {
				sheep_vector_push(&vm->stack, tmp);
				break;
			}

			sheep_vector_push(&vm->calls, codep);
			sheep_vector_push(&vm->calls, (void *)basep);
			sheep_vector_push(&vm->calls, function);

			function = sheep_data(tmp);
			basep = vm->stack.nr_items - function->nr_locals;
			codep = (unsigned long *)function->code.code.items;
			nesting++;
			continue;
		case SHEEP_RET:
			sheep_bug_on(vm->stack.nr_items - basep -
				(function ? function->nr_locals : 0) != 1);

			close_pending(vm, basep);

			if (function && function->nr_locals) {
				vm->stack.items[basep] =
					vm->stack.items[basep +
							function->nr_locals];
				vm->stack.nr_items = basep + 1;
			}

			if (!nesting--)
				goto out;

			function = sheep_vector_pop(&vm->calls);
			basep = (unsigned long)sheep_vector_pop(&vm->calls);
			codep = sheep_vector_pop(&vm->calls);
			break;
		case SHEEP_BRN:
			tmp = sheep_vector_pop(&vm->stack);
			if (sheep_test(tmp))
				break;
		case SHEEP_BR:
			codep += arg;
			continue;
		default:
			abort();
		}
		codep++;
	}
out:
	return sheep_vector_pop(&vm->stack);
err:
	vm->stack.nr_items = 0;
	vm->calls.nr_items = 0;
	return NULL;
}

sheep_t sheep_eval(struct sheep_vm *vm, struct sheep_code *code)
{
	return __sheep_eval(vm, code, NULL);
}

sheep_t sheep_call(struct sheep_vm *vm, sheep_t callable,
		unsigned int nr_args, ...)
{
	struct sheep_function *function;
	unsigned int nr = nr_args;
	sheep_t value;
	va_list ap;

	va_start(ap, nr_args);
	while (nr--)
		sheep_vector_push(&vm->stack, va_arg(ap, sheep_t));
	va_end(ap);

	switch (call(vm, callable, nr_args, &value)) {
	case -1:
		return NULL;
	case 1:
		return value;
	case 0:
	default:
		function = sheep_data(callable);
		return __sheep_eval(vm, &function->code, function);
	}
}

void sheep_evaluator_init(struct sheep_vm *vm)
{
	vm->stack.blocksize = 32;
	vm->calls.blocksize = 16;
}

void sheep_evaluator_exit(struct sheep_vm *vm)
{
	sheep_free(vm->calls.items);
	sheep_free(vm->stack.items);
	sheep_map_drain(&vm->main.env);
}
