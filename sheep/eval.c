/*
 * sheep/eval.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/foreign.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/alien.h>
#include <sheep/bool.h>
#include <sheep/code.h>
#include <sheep/name.h>
#include <sheep/type.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/eval.h>

static sheep_t hash(struct sheep_vm *vm, sheep_t container,
		unsigned int key_slot, sheep_t value)
{
	const char *key, *obj;
	struct sheep_map *map;
	sheep_t *slots;
	void *entry;

	key = vm->keys[key_slot];

	if (sheep_type(container) == &sheep_module_type) {
		struct sheep_module *mod = sheep_data(container);

		slots = (sheep_t *)vm->globals.items;
		map = &mod->env;
	} else if (sheep_type(container) == &sheep_typeobject_type) {
		struct sheep_typeobject *object = sheep_data(container);

		slots = object->values;
		map = &object->map;
	} else
		goto err;

	if (sheep_map_get(map, key, &entry))
		goto err;

	if (value)
		slots[(unsigned long)entry] = value;

	return slots[(unsigned long)entry];
err:
	obj = sheep_repr(container);
	sheep_error(vm, "can not find `%s' in `%s'", key, obj);
	sheep_free(obj);
	return NULL;
}

static sheep_t closure(struct sheep_vm *vm, unsigned long basep,
		struct sheep_function *parent, sheep_t sheep)
{
	struct sheep_function *function = sheep_data(sheep);

	if (function->foreign) {
		struct sheep_function *closure;

		sheep = sheep_closure_function(vm, function);
		closure = sheep_function(sheep);
		closure->foreign =
			sheep_foreign_open(vm, basep, parent, function);
	}
	return sheep;
}

static enum sheep_call precall(struct sheep_vm *vm, sheep_t callable,
			unsigned int nr_args, sheep_t *valuep)
{
	const struct sheep_type *type;

	type = sheep_type(callable);
	if (!type->call) {
		sheep_error(vm, "can not call `%s'", type->name);
		return SHEEP_CALL_FAIL;
	}
	return type->call(vm, callable, nr_args, valuep);
}

static void splice_arguments(struct sheep_vm *vm, unsigned long basep,
			unsigned int nr_args)
{
	unsigned int t;

	for (t = nr_args; t; t--)
		vm->stack.items[basep + nr_args - t] =
			vm->stack.items[vm->stack.nr_items - t];
	vm->stack.nr_items = basep + nr_args;
}

static unsigned long finalize_frame(struct sheep_vm *vm,
				struct sheep_function *function)
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
	sheep_t problem = NULL;

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
		case SHEEP_DUP:
			tmp = vm->stack.items[vm->stack.nr_items - 1];
			sheep_vector_push(&vm->stack, tmp);
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
			if (indirect->count < 0)
				tmp = indirect->value.closed;
			else {
				unsigned long index;

				index = indirect->value.live.index;
				tmp = vm->stack.items[index];
			}
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_FOREIGN:
			tmp = sheep_vector_pop(&vm->stack);
			indirect = current->foreign->items[arg];
			if (indirect->count < 0)
				indirect->value.closed = tmp;
			else {
				unsigned long index;

				index = indirect->value.live.index;
				vm->stack.items[index] = tmp;
			}
			break;
		case SHEEP_GLOBAL:
			tmp = vm->globals.items[arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_GLOBAL:
			tmp = sheep_vector_pop(&vm->stack);
			vm->globals.items[arg] = tmp;
			break;
		case SHEEP_HASH:
			tmp = sheep_vector_pop(&vm->stack);
			tmp = hash(vm, tmp, arg, NULL);
			if (!tmp)
				goto err;
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_HASH:
			tmp = sheep_vector_pop(&vm->stack);
			if (!hash(vm, tmp, arg, sheep_vector_pop(&vm->stack)))
				goto err;
			break;
		case SHEEP_CLOSURE:
			tmp = vm->globals.items[arg];
			tmp = closure(vm, basep, current, tmp);
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_TAILCALL:
			tmp = sheep_vector_pop(&vm->stack);

			done = precall(vm, tmp, arg, &tmp);
			switch (done) {
			case SHEEP_CALL_FAIL:
				problem = tmp;
				goto err;
			case SHEEP_CALL_DONE:
				sheep_vector_push(&vm->stack, tmp);
				break;
			case SHEEP_CALL_EVAL:
				sheep_foreign_save(vm, basep);
				splice_arguments(vm, basep, arg);

				sheep_unprotect(vm, function);
				function = tmp;
				sheep_protect(vm, function);

				current = sheep_function(function);
				finalize_frame(vm, current);
				codep = function_codep(current);
				continue;
			}
			break;
		case SHEEP_CALL:
			tmp = sheep_vector_pop(&vm->stack);

			done = precall(vm, tmp, arg, &tmp);
			switch (done) {
			case SHEEP_CALL_FAIL:
				problem = tmp;
				goto err;
			case SHEEP_CALL_DONE:
				sheep_vector_push(&vm->stack, tmp);
				break;
			case SHEEP_CALL_EVAL:
				sheep_vector_push(&vm->calls, codep);
				sheep_vector_push(&vm->calls, (void *)basep);
				sheep_vector_push(&vm->calls, function);

				sheep_unprotect(vm, function);
				function = tmp;
				sheep_protect(vm, function);

				current = sheep_function(function);
				basep = finalize_frame(vm, current);
				codep = function_codep(current);

				nesting++;
				continue;
			}
			break;
		case SHEEP_RET:
			sheep_bug_on(vm->stack.nr_items -
				basep - current->nr_locals != 1);

			sheep_foreign_save(vm, basep);

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
			sheep_protect(vm, function);

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
		case SHEEP_LOAD:
			tmp = sheep_module_load(vm, vm->keys[arg]);
			if (!tmp)
				goto err;
			sheep_vector_push(&vm->stack, tmp);
			break;
		default:
			abort();
		}
		codep++;
	}
out:
	return sheep_vector_pop(&vm->stack);
err:
	vm->stack.nr_items = 0;
	vm->calls.nr_items -= 3 * nesting;
	if (!vm->calls.nr_items)
		sheep_report_error(vm, problem);
	sheep_unprotect(vm, function);
	return NULL;
}

static sheep_t call(struct sheep_vm *vm, sheep_t callable, unsigned int nr_args)
{
	sheep_t value;

	switch (precall(vm, callable, nr_args, &value)) {
	case SHEEP_CALL_FAIL:
		return NULL;
	case SHEEP_CALL_DONE:
		return value;
	case SHEEP_CALL_EVAL:
		return sheep_eval(vm, callable);
	}
	sheep_bug("precall returned bull");
}

sheep_t sheep_apply(struct sheep_vm *vm, sheep_t callable,
		struct sheep_list *args)
{
	unsigned int nr_args = 0;

	while (args->head) {
		sheep_vector_push(&vm->stack, args->head);
		nr_args++;
		args = sheep_list(args->tail);
	}
	return call(vm, callable, nr_args);
}

sheep_t sheep_call(struct sheep_vm *vm, sheep_t callable,
		unsigned int nr_args, ...)
{
	unsigned int nr = nr_args;
	va_list ap;

	va_start(ap, nr_args);
	while (nr--)
		sheep_vector_push(&vm->stack, va_arg(ap, sheep_t));
	va_end(ap);
	return call(vm, callable, nr_args);
}

void sheep_evaluator_exit(struct sheep_vm *vm)
{
	sheep_free(vm->calls.items);
	sheep_free(vm->stack.items);
	sheep_map_drain(&vm->main.env);
}
