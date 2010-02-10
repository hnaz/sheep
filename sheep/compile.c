/*
 * sheep/compile.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/foreign.h>
#include <sheep/vector.h>
#include <sheep/parse.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/read.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/gc.h>
#include <sheep/vm.h>

#include <sheep/compile.h>

sheep_t __sheep_compile(struct sheep_vm *vm, struct sheep_module *module,
			struct sheep_expr *expr)
{
	struct sheep_function *function;
	struct sheep_compile compile = {
		.vm = vm,
		.module = module,
		.expr = expr,
	};
	struct sheep_context context = {
		.env = &module->env,
	};
	int err;

	sheep_protect(vm, expr->object);

	function = sheep_zalloc(sizeof(struct sheep_function));
	err = sheep_compile_object(&compile, function, &context, expr->object);
	if (err) {
		sheep_code_exit(&function->code);
		sheep_free(function);

		sheep_unprotect(vm, expr->object);
		return NULL;
	}
	sheep_code_finalize(&function->code);

	sheep_unprotect(vm, expr->object);
	return sheep_make_object(vm, &sheep_function_type, function);
}

int sheep_compile_constant(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, sheep_t sheep)
{
	unsigned int slot;

	slot = sheep_vm_constant(compile->vm, sheep);
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

static enum env_level lookup_env(struct sheep_compile *compile,
				struct sheep_context *context, const char *name,
				unsigned int *dist, unsigned int *slot)
{
	struct sheep_context *current = context;
	unsigned int distance = 0;
	void *entry;

	while (sheep_map_get(current->env, name, &entry)) {
		if (!current->parent) {
			if (sheep_map_get(&compile->vm->builtins, name, &entry))
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

static int compile_simple_name(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context,
			sheep_t sheep, int set)
{
	unsigned int dist, slot;
	const char *name;

	name = sheep_name(sheep)->parts[0];

	switch (lookup_env(compile, context, name, &dist, &slot)) {
	case ENV_NONE:
		sheep_parser_error(compile, sheep, "unbound name");
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
		slot = sheep_foreign_slot(function, dist, slot);
		if (set)
			sheep_emit(&function->code, SHEEP_SET_FOREIGN, slot);
		sheep_emit(&function->code, SHEEP_FOREIGN, slot);
		break;
	case ENV_BUILTIN:
		if (set) {
			sheep_parser_error(compile, sheep,
					"can not change read-only binding to");
			return -1;
		}
		sheep_emit(&function->code, SHEEP_GLOBAL, slot);
		break;
	}
	return 0;
}

static int compile_name(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context,
			sheep_t sheep, int set)
{
	struct sheep_module *mod;
	struct sheep_name *name;
	unsigned int i = 0;
	void *entry;

	name = sheep_name(sheep);
	if (name->nr_parts == 1)
		return compile_simple_name(compile, function,
					context, sheep, set);

	mod = compile->module;
	do {
		sheep_t m;

		if (sheep_map_get(&mod->env, name->parts[i], &entry))
			goto err;

		m = compile->vm->globals.items[(unsigned long)entry];
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
	sheep_parser_error(compile, sheep, "unbound");
	return -1;
}

int sheep_compile_name(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, sheep_t sheep)
{
	return compile_name(compile, function, context, sheep, 0);
}

int sheep_compile_set(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, sheep_t sheep)
{
	return compile_name(compile, function, context, sheep, 1);
}

static int compile_call(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *form)
{
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	struct sheep_list *args;
	int nargs, ret = -1;
	unsigned int tail;

	args = sheep_list(form->tail);
	for (nargs = 0; args->head; args = sheep_list(args->tail), nargs++)
		if (sheep_compile_object(compile, function, &block, args->head))
			goto out;

	/* Do not propagate to subcalls as in call to foo in ((foo x) n) */
	tail = context->flags & SHEEP_CONTEXT_TAILFORM;
	context->flags &= ~SHEEP_CONTEXT_TAILFORM;

	if (sheep_compile_object(compile, function, context, form->head))
		goto out;

	if (tail)
		sheep_emit(&function->code, SHEEP_TAILCALL, nargs);
	else
		sheep_emit(&function->code, SHEEP_CALL, nargs);
	ret = 0;
out:
	sheep_map_drain(&env);
	return ret;
}

int sheep_compile_list(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, sheep_t sheep)
{
	struct sheep_list *list;

	list = sheep_list(sheep);

	/* The empty list is a constant */
	if (!list->head)
		return sheep_compile_constant(compile, function, context, sheep);

	if (sheep_type(list->head) == &sheep_name_type) {
		struct sheep_name *name;
		void *entry;

		name = sheep_name(list->head);
		if (name->nr_parts == 1 &&
		    !sheep_map_get(&compile->vm->specials, *name->parts, &entry)) {
			int (*compile_special)(struct sheep_compile *,
					struct sheep_function *,
					struct sheep_context *,
					struct sheep_list *) = entry;

			return compile_special(compile, function, context, list);
		}
	}
	return compile_call(compile, function, context, list);
}
