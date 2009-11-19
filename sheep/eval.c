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

static void note_pending(struct sheep_vm *vm, struct sheep_indirect *indirect)
{
	struct sheep_indirect *prev = NULL, *next = vm->pending;

	while (next) {
		if (indirect->value.live.index >= next->value.live.index)
			break;
		prev = next;
		next = next->value.live.next;
	}

	indirect->value.live.next = next;
	if (prev)
		prev->value.live.next = indirect;
	else
		vm->pending = indirect;
}

static void close_pending(struct sheep_vm *vm, unsigned long basep)
{
	while (vm->pending) {
		struct sheep_indirect *indirect = vm->pending;
		sheep_t value;

		if (indirect->value.live.index < basep)
			break;

		vm->pending = indirect->value.live.next;
		value = vm->stack.items[indirect->value.live.index];
		indirect->closed = 1;
		indirect->value.closed = value;
	}
}

static sheep_t closure(struct sheep_vm *vm, unsigned long basep, sheep_t sheep)
{
	struct sheep_function *function, *closure;
	struct sheep_vector *freevars, *indirects;
	unsigned int i;

	sheep_bug_on(sheep_type(sheep) != &sheep_function_type);
	function = sheep_data(sheep);

	if (!function->foreign)
		return sheep;

	indirects = sheep_malloc(sizeof(struct sheep_vector));
	sheep_vector_init(indirects);

	freevars = function->foreign;
	for (i = 0; i < freevars->nr_items; i++) {
		struct sheep_indirect *indirect;
		struct sheep_freevar *freevar;
		unsigned long owner, offset;
		unsigned int dist, slot;

		freevar = freevars->items[i];
		dist = freevar->dist;
		slot = freevar->slot;
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
			offset = vm->calls.nr_items - (3 * offset - 1);
			owner = (unsigned long)vm->calls.items[offset];
		}

		indirect = sheep_zalloc(sizeof(struct sheep_indirect));
		indirect->value.live.index = owner + slot;

		sheep_vector_push(indirects, indirect);
		note_pending(vm, indirect);
	}

	sheep = sheep_closure_function(vm, function);
	closure = sheep_data(sheep);
	closure->foreign = indirects;

	return sheep;
}

static int precall(struct sheep_vm *vm, sheep_t callable, unsigned int nr_args,
		sheep_t *value)
{
	struct sheep_function *function;

	if (sheep_type(callable) == &sheep_alien_type) {
		struct sheep_alien *alien;

		alien = sheep_data(callable);
		*value = alien->function(vm, nr_args);
		return 1 - 2 * !*value;
	}

	function = sheep_data(callable);
	if (function->nr_parms != nr_args) {
		fprintf(stderr, "%s: too %s arguments\n",
			function->name,
			function->nr_parms < nr_args ? "many" : "few");
		return -1;
	}
	return 0;
}

static void
splice_arguments(struct sheep_vm *vm, unsigned long basep, unsigned int nr_args)
{
	unsigned int t;

	for (t = nr_args; t; t--)
		vm->stack.items[basep + nr_args - t] =
			vm->stack.items[vm->stack.nr_items - t];
	vm->stack.nr_items = basep + nr_args;
}

static unsigned long
finalize_frame(struct sheep_vm *vm, struct sheep_function *function)
{
	unsigned int nr;

	nr = function->nr_locals - function->nr_parms;
	if (nr)
		sheep_vector_grow(&vm->stack, nr);
	return vm->stack.nr_items - function->nr_locals;
}

static unsigned long *function_codep(struct sheep_function *function)
{
	return (unsigned long *)function->code.code.items;
}

sheep_t sheep_eval(struct sheep_vm *vm, sheep_t function)
{
	struct sheep_function *current;
	unsigned long basep, *codep;
	unsigned int nesting = 0;

	sheep_protect(vm, function);

	current = sheep_function(function);
	codep = function_codep(current);
	basep = finalize_frame(vm, current);

	for (;;) {
		struct sheep_indirect *indirect;
		enum sheep_opcode op;
		unsigned int arg;
		sheep_t tmp;
		int done;

		sheep_decode(*codep, &op, &arg);
		//sheep_code_dump(vm, current, basep, op, arg);

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
			indirect = current->foreign->items[arg];
			if (indirect->closed)
				tmp = indirect->value.closed;
			else
				tmp = vm->stack.items[indirect->value.live.index];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_FOREIGN:
			tmp = sheep_vector_pop(&vm->stack);
			indirect = current->foreign->items[arg];
			if (indirect->closed)
				indirect->value.closed = tmp;
			else
				vm->stack.items[indirect->value.live.index] = tmp;
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
		case SHEEP_TAILCALL:
			tmp = sheep_vector_pop(&vm->stack);

			done = precall(vm, tmp, arg, &tmp);
			switch (done) {
			case -1:
				goto err;
			case 1:
				sheep_vector_push(&vm->stack, tmp);
				break;
			default:
				close_pending(vm, basep);
				splice_arguments(vm, basep, arg);

				sheep_unprotect(vm, function);
				function = tmp;
				sheep_protect(vm, function);

				current = sheep_function(function);
				codep = function_codep(current);

				finalize_frame(vm, current);
				continue;
			}
			break;
		case SHEEP_CALL:
			tmp = sheep_vector_pop(&vm->stack);

			done = precall(vm, tmp, arg, &tmp);
			switch (done) {
			case -1:
				goto err;
			case 1:
				sheep_vector_push(&vm->stack, tmp);
				break;
			default:
				sheep_vector_push(&vm->calls, codep);
				sheep_vector_push(&vm->calls, (void *)basep);
				sheep_vector_push(&vm->calls, function);

				function = tmp;
				sheep_protect(vm, function);

				current = sheep_function(function);
				codep = function_codep(current);
				basep = finalize_frame(vm, current);

				nesting++;
				continue;
			}
			break;
		case SHEEP_RET:
			sheep_bug_on(vm->stack.nr_items -
				basep - current->nr_locals != 1);

			close_pending(vm, basep);

			if (current->nr_locals) {
				vm->stack.items[basep] =
					vm->stack.items[basep +
							current->nr_locals];
				vm->stack.nr_items = basep + 1;
			}

			sheep_unprotect(vm, function);

			if (!nesting--)
				goto out;

			function = sheep_vector_pop(&vm->calls);
			current = sheep_function(function);
			basep = (unsigned long)sheep_vector_pop(&vm->calls);
			codep = sheep_vector_pop(&vm->calls);
			break;
		case SHEEP_BRT:
			tmp = vm->stack.items[vm->stack.nr_items - 1];
			if (!sheep_test(tmp))
				break;
			codep += arg;
			continue;
		case SHEEP_BRF:
			tmp = vm->stack.items[vm->stack.nr_items - 1];
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

sheep_t
sheep_call(struct sheep_vm *vm, sheep_t callable, unsigned int nr_args, ...)
{
	unsigned int nr = nr_args;
	sheep_t value;
	va_list ap;

	va_start(ap, nr_args);
	while (nr--)
		sheep_vector_push(&vm->stack, va_arg(ap, sheep_t));
	va_end(ap);

	switch (precall(vm, callable, nr_args, &value)) {
	case -1:
		return NULL;
	case 1:
		return value;
	case 0:
	default:
		return sheep_eval(vm, callable);
	}
}

void sheep_evaluator_exit(struct sheep_vm *vm)
{
	sheep_free(vm->calls.items);
	sheep_free(vm->stack.items);
	sheep_map_drain(&vm->main.env);
}
