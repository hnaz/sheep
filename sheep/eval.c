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

/*
 * How closures are implemented
 *
 * The distance between a free variable reference and its binding
 * lexical environment is a compile time constant which the compiler
 * remembers in function->foreigns (see compile.c:slot_foreign())
 *
 * When the evaluator instantiates a closure by means of the CLOSURE
 * instruction, it creates a new function object and sets its foreign
 * slots in turn to point to live stack slots.  They can be determined
 * by means of the call stack and the relative lexical distance (see
 * closure()).
 *
 * Since these foreign slots are now pointing to value references on
 * the stack, those references will get preserved before the stack is
 * unwound.  The runtime notes those live references when
 * instantiating the closure (see note_pending()) and preserves the
 * references when the RET instruction is about to tear down a stack
 * frame that has live references (see close_pending()).
 */

struct sheep_pending {
	sheep_t **slotp;
	unsigned long basep;
	struct sheep_pending *next;
};

static void note_pending(struct sheep_vm *vm, sheep_t **slotp,
			unsigned long basep)
{
	struct sheep_pending *prev = NULL, *next, *new;

	new = sheep_malloc(sizeof(struct sheep_pending));
	new->slotp = slotp;
	new->basep = basep;

	for (next = vm->pending; next && next->basep > basep; next = next->next)
		prev = next;

	if (!prev) {
		new->next = next;
		vm->pending = new;
	} else {
		new->next = prev->next;
		prev->next = new;
	}
}

static void close_pending(struct sheep_vm *vm, unsigned long basep)
{
	while (vm->pending && vm->pending->basep == basep) {
		struct sheep_pending *next;
		sheep_t value;

		value = **vm->pending->slotp;
		*vm->pending->slotp = sheep_malloc(sizeof(sheep_t *));
		**vm->pending->slotp = value;

		next = vm->pending->next;
		sheep_free(vm->pending);
		vm->pending = next;
	}
}

static sheep_t closure(struct sheep_vm *vm, unsigned long basep, sheep_t sheep)
{
	struct sheep_function *old, *new;
	struct sheep_vector *foreigns;
	unsigned int i;

	sheep_bug_on(sheep->type != &sheep_function_type);
	old = sheep_data(sheep);

	if (!old->foreigns)
		return sheep;

	foreigns = sheep_malloc(sizeof(struct sheep_vector));
	sheep_vector_init(foreigns, 4);

	sheep_bug_on(old->foreigns->nr_items % 2);
	for (i = 0; i < old->foreigns->nr_items; i += 2) {
		unsigned long fbasep, offset;
		unsigned int dist, slot;
		sheep_t *foreignp;

		dist = (unsigned long)old->foreigns->items[i];
		slot = (unsigned long)old->foreigns->items[i + 1];
		/*
		 * Closure instantiation happens in the outer
		 * function, thus if the owner distance is 1, it
		 * refers to the current base pointer.  Otherwise, it
		 * refers to a base pointer on the call stack.
		 */
		offset = dist - 1;
		if (!offset)
			fbasep = basep;
		else {
			/*
			 * The callstack contains triplets of
			 * [pc basep function]
			 */
			offset = vm->calls.nr_items - 3 * offset - 1;
			fbasep = (unsigned long)vm->calls.items[offset];
		}

		foreignp = (sheep_t *)vm->stack.items + fbasep;
		slot = sheep_vector_push(foreigns, foreignp + slot);
		note_pending(vm, (sheep_t **)foreigns->items + slot, fbasep);
	}

	sheep = sheep_function(vm);
	new = sheep_data(sheep);
	*new = *old;
	new->foreigns = foreigns;

	return sheep;
}

static int call(struct sheep_vm *vm, sheep_t callable, unsigned int nr_args,
		sheep_t *value)
{
	struct sheep_function *function;

	if (callable->type == &sheep_alien_type) {
		sheep_alien_t alien;

		alien = *(sheep_alien_t *)sheep_data(callable);
		*value = alien(vm, nr_args);
		return 1 - 2 * !*value;
	}

	sheep_bug_on(callable->type != &sheep_function_type);

	function = sheep_data(callable);
	if (function->nr_parms != nr_args) {
		fprintf(stderr, "wrong number of arguments\n");
		return -1;
	}

	if (function->nr_locals > nr_args)
		sheep_vector_grow(&vm->stack, function->nr_locals - nr_args);

	return 0;
}

static sheep_t __sheep_eval(struct sheep_vm *vm, struct sheep_code *code,
			struct sheep_function *function)
{
	struct sheep_code *current = code;
	unsigned int nesting = 0;
	unsigned long basep, pc;

	if (function) {
		basep = vm->stack.nr_items - function->nr_locals;
		pc = function->offset;
	} else
		basep = pc = 0;

	for (;;) {
		enum sheep_opcode op;
		unsigned int arg;
		sheep_t tmp;
		int done;

		sheep_decode((unsigned long)current->code.items[pc], &op, &arg);
		sheep_code_dump(vm, function, basep, op, arg);

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
			tmp = function->foreigns->items[arg];
			sheep_vector_push(&vm->stack, *(sheep_t *)tmp);
			break;
		case SHEEP_SET_FOREIGN:
			tmp = sheep_vector_pop(&vm->stack);
			*(sheep_t *)function->foreigns->items[arg] = tmp;
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

			sheep_vector_push(&vm->calls, (void *)pc);
			sheep_vector_push(&vm->calls, (void *)basep);
			sheep_vector_push(&vm->calls, function);

			function = sheep_data(tmp);
			basep = vm->stack.nr_items - function->nr_locals;
			pc = function->offset;
			current = &vm->code;
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
			pc = (unsigned long)sheep_vector_pop(&vm->calls);

			if (!function)
				current = code;
			break;
		case SHEEP_BRN:
			tmp = sheep_vector_pop(&vm->stack);
			if (sheep_test(tmp))
				break;
		case SHEEP_BR:
			pc += arg;
			continue;
		default:
			abort();
		}
		pc++;
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
		return __sheep_eval(vm, &vm->code, sheep_data(callable));
	}
	return (void *)0xdead;
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
