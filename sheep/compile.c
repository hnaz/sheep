/*
 * sheep/compile.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/string.h>
#include <sheep/vector.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

#include <sheep/compile.h>

sheep_t __sheep_compile(struct sheep_vm *vm, struct sheep_module *module,
			sheep_t expr)
{
	struct sheep_function *function;
	struct sheep_context context = {
		.env = &module->env,
	};
	int err;

	sheep_protect(vm, expr);

	function = sheep_zalloc(sizeof(struct sheep_function));
	err = sheep_compile_object(vm, function, &context, expr);
	if (err) {
		sheep_code_exit(&function->code);
		sheep_free(function);

		sheep_unprotect(vm, expr);
		return NULL;
	}
	sheep_code_finalize(&function->code);

	sheep_unprotect(vm, expr);
	return sheep_make_object(vm, &sheep_function_type, function);
}

int sheep_compile_constant(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, sheep_t expr)
{
	unsigned int slot;

	slot = sheep_vm_constant(vm, expr);
	sheep_emit(&function->code, SHEEP_GLOBAL, slot);
	return 0;
}

enum env_level {
	ENV_NONE,
	ENV_LOCAL,
	ENV_GLOBAL,
	ENV_FOREIGN,
	ENV_BUILTIN,
};

static enum env_level lookup_env(struct sheep_vm *vm,
				struct sheep_context *context, const char *name,
				unsigned int *dist, unsigned int *slot)
{
	struct sheep_context *current = context;
	unsigned int distance = 0;
	void *entry;

	while (sheep_map_get(current->env, name, &entry)) {
		if (!current->parent) {
			if (sheep_map_get(&vm->builtins, name, &entry))
				return ENV_NONE;
			current = NULL;
			break;
		}
		if (current->flags & SHEEP_CONTEXT_FUNCTION)
			distance++;
		current = current->parent;
	}

	*dist = distance;
	*slot = (unsigned long)entry;

	if (!current)
		return ENV_BUILTIN;
	if (!current->parent)
		return ENV_GLOBAL;
	if (!distance)
		return ENV_LOCAL;
	return ENV_FOREIGN;
}

static unsigned int slot_foreign(struct sheep_function *function,
				unsigned int dist, unsigned int slot)
{
	struct sheep_vector *foreign = function->foreign;
	struct sheep_freevar *freevar;

	if (!foreign) {
		foreign = sheep_zalloc(sizeof(struct sheep_vector));
		function->foreign = foreign;
	} else {
		unsigned int i;

		for (i = 0; i < foreign->nr_items; i++) {
			freevar = foreign->items[i];
			if (freevar->dist != dist)
				continue;
			if (freevar->slot != slot)
				continue;
			return i;
		}
	}

	freevar = sheep_malloc(sizeof(struct sheep_freevar));
	freevar->dist = dist;
	freevar->slot = slot;

	return sheep_vector_push(foreign, freevar);
}

static int compile_simple_name(struct sheep_vm *vm,
			struct sheep_function *function, 
			struct sheep_context *context,
			const char *name, int set)
{
	unsigned int dist, slot;

	switch (lookup_env(vm, context, name, &dist, &slot)) {
	case ENV_NONE:
		fprintf(stderr, "unbound name: %s\n", name);
		return -1;
	case ENV_LOCAL:
		if (set)
			sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(&function->code, SHEEP_LOCAL, slot);
		break;
	case ENV_GLOBAL:
		if (set)
			sheep_emit(&function->code, SHEEP_SET_GLOBAL, slot);
		sheep_emit(&function->code, SHEEP_GLOBAL, slot);
		break;
	case ENV_FOREIGN:
		slot = slot_foreign(function, dist, slot);
		if (set)
			sheep_emit(&function->code, SHEEP_SET_FOREIGN, slot);
		sheep_emit(&function->code, SHEEP_FOREIGN, slot);
		break;
	case ENV_BUILTIN:
		if (set) {
			fprintf(stderr, "read-only bound: %s\n", name);
			return -1;
		}
		sheep_emit(&function->code, SHEEP_GLOBAL, slot);
		break;
	}
	return 0;
}

static int compile_name(struct sheep_vm *vm,
			struct sheep_function *function, 
			struct sheep_context *context,
			sheep_t expr, int set)
{
	struct sheep_module *mod;
	struct sheep_name *name;
	unsigned int i = 0;
	void *entry;
	char *tmp;

	name = sheep_name(expr);
	if (name->nr_parts == 1)
		return compile_simple_name(vm, function, context,
					name->parts[0], set);
	mod = &vm->main;
	do {
		sheep_t m;

		if (sheep_map_get(&mod->env, name->parts[i], &entry))
			goto err;

		m = vm->globals.items[(unsigned long)entry];
		if (sheep_type(m) != &sheep_module_type)
			goto err;
		mod = sheep_data(m);
	} while (++i < name->nr_parts - 1);

	if (sheep_map_get(&mod->env, name->parts[i], &entry))
		goto err;

	if (set)
		sheep_emit(&function->code, SHEEP_SET_GLOBAL,
			(unsigned long)entry);
	sheep_emit(&function->code, SHEEP_GLOBAL, (unsigned long)entry);
	return 0;
err:
	tmp = sheep_format(expr);
	fprintf(stderr, "unbound name: %s\n", tmp);
	sheep_free(tmp);
	return -1;
}

int sheep_compile_name(struct sheep_vm *vm, struct sheep_function *function,
		struct sheep_context *context, sheep_t expr)
{
	return compile_name(vm, function, context, expr, 0);
}

int sheep_compile_set(struct sheep_vm *vm, struct sheep_function *function,
		struct sheep_context *context, sheep_t expr)
{
	return compile_name(vm, function, context, expr, 1);
}

static int compile_call(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *form)
{
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	struct sheep_list *args;
	int nargs, ret = -1;

	args = sheep_list(form->tail);
	for (nargs = 0; args->head; args = sheep_list(args->tail), nargs++)
		if (sheep_compile_object(vm, function, &block, args->head))
			goto out;

	if (sheep_compile_object(vm, function, context, form->head))
		goto out;

	if (context->flags & SHEEP_CONTEXT_TAILFORM)
		sheep_emit(&function->code, SHEEP_TAILCALL, nargs);
	else
		sheep_emit(&function->code, SHEEP_CALL, nargs);
	ret = 0;
out:
	sheep_map_drain(&env);
	return ret;
}

int sheep_compile_list(struct sheep_vm *vm, struct sheep_function *function,
		struct sheep_context *context, sheep_t expr)
{
	struct sheep_list *list;

	list = sheep_list(expr);
	/* The empty list is a constant */
	if (!list->head)
		return sheep_compile_constant(vm, function, context, expr);
	if (sheep_type(list->head) == &sheep_name_type) {
		struct sheep_name *name;
		void *entry;

		name = sheep_name(list->head);
		if (name->nr_parts == 1 &&
		    !sheep_map_get(&vm->specials, *name->parts, &entry)) {
			int (*compile_special)(struct sheep_vm *,
					struct sheep_function *,
					struct sheep_context *,
					struct sheep_list *) = entry;
			struct sheep_list *args;

			args = sheep_list(list->tail);
			return compile_special(vm, function, context, args);
		}
	}
	return compile_call(vm, function, context, list);
}
