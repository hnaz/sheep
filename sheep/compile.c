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
#include <sheep/read.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

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

void sheep_parser_error(struct sheep_compile *compile, sheep_t culprit,
			const char *fmt, ...)
{
	struct sheep_expr *expr = compile->expr;
	unsigned long position;
	const char *repr;
	va_list ap;

	if (sheep_type(expr->object) == &sheep_list_type) {
		struct sheep_list *list = sheep_list(expr->object);

		position = 1; /* list itself is at 0 */
		sheep_list_search(list, culprit, &position);
	} else
		position = 0;

	fprintf(stderr, "%s:%lu: ", expr->filename,
		(unsigned long)expr->lines.items[position]);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	repr = sheep_repr(culprit);
	fprintf(stderr, " `%s'\n", repr);
	sheep_free(repr);
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
		slot = slot_foreign(function, dist, slot);
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

void sheep_propagate_foreigns(struct sheep_function *function,
			struct sheep_function *childfun)
{
	unsigned long i;

	for (i = 0; i < childfun->foreign->nr_items; i++) {
		struct sheep_freevar *var;

		var = childfun->foreign->items[i];
		/* var->slot indexes a local slot of @function */
		if (var->dist == 1)
			continue;
		/* var->slot indexes a foreign slot of @function */
		var->slot = slot_foreign(function, var->dist - 1, var->slot);
	}
}

enum {
	PARSE_OK,
	PARSE_MISMATCH,
	PARSE_TOO_FEW,
	PARSE_TOO_MANY
};

static unsigned int __parse(struct sheep_list *form, const char *items,
			sheep_t *mismatchp, va_list ap)
{
	while (*items) {
		struct sheep_name *name;
		struct sheep_list *list;
		sheep_t thing;

		if (!form->head && *items != 'r')
			return PARSE_TOO_FEW;

		if (tolower(*items) == 'r') {
			*va_arg(ap, struct sheep_list **) = form;
			return PARSE_OK;
		}

		thing = form->head;

		switch (*items) {
		case 'e':
			*va_arg(ap, sheep_t *) = thing;
			break;
		case 's':
			if (sheep_type(thing) != &sheep_name_type)
				goto mismatch;
			name = sheep_name(thing);
			if (name->nr_parts != 1)
				goto mismatch;
			*va_arg(ap, const char **) = name->parts[0];
			break;
		case 'n':
			if (sheep_type(thing) != &sheep_name_type)
				goto mismatch;
			name = sheep_name(thing);
			*va_arg(ap, struct sheep_name **) = name;
			break;
		case 'l':
			if (sheep_type(thing) != &sheep_list_type)
				goto mismatch;
			list = sheep_list(thing);
			*va_arg(ap, struct sheep_list **) = list;
			break;
		default:
			sheep_bug("invalid parser instruction");
		}

		form = sheep_list(form->tail);
		items++;
		continue;
	mismatch:
		*mismatchp = thing;
		return PARSE_MISMATCH;
	}

	if (form->head)
		return PARSE_TOO_MANY;

	return PARSE_OK;
}

static int parse(struct sheep_compile *compile, struct sheep_list *form,
		struct sheep_list *child, const char *items, va_list ap)
{
	unsigned int ret;
	sheep_t mismatch;

	ret = __parse(child, items, &mismatch, ap);
	if (ret == PARSE_OK)
		return 0;

	if (ret == PARSE_MISMATCH)
		sheep_parser_error(compile, mismatch, "parser error at");
	else
		sheep_parser_error(compile, form->head, "too %s arguments to",
				ret == PARSE_TOO_MANY ? "many" : "few");

	return -1;
}

int __sheep_parse(struct sheep_compile *compile, struct sheep_list *form,
		struct sheep_list *child, const char *items, ...)
{
	va_list ap;
	int ret;

	va_start(ap, items);
	ret = parse(compile, form, child, items, ap);
	va_end(ap);
	return ret;
}

int sheep_parse(struct sheep_compile *compile, struct sheep_list *form,
		const char *items, ...)
{
	va_list ap;
	int ret;

	va_start(ap, items);
	ret = parse(compile, form, sheep_list(form->tail), items, ap);
	va_end(ap);
	return ret;
}
