/*
 * sheep/compile.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/vector.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/compile.h>

struct sheep_unit *__sheep_compile(struct sheep_vm *vm,
				struct sheep_module *module, sheep_t expr)
{
	struct sheep_context context = {
		.env = &module->env,
	};
	struct sheep_unit *unit;
	int err;

	unit = sheep_zalloc(sizeof(struct sheep_unit));
	sheep_code_init(&unit->code);

	sheep_protect(vm, expr);
	err = expr->type->compile(vm, unit, &context, expr);
	sheep_unprotect(vm, expr);

	if (err) {
		sheep_code_exit(&unit->code);
		sheep_free(unit);
		return NULL;
	}

	sheep_code_finish(&unit->code);
	return unit;
}

int sheep_compile_constant(struct sheep_vm *vm, struct sheep_unit *unit,
			struct sheep_context *context, sheep_t expr)
{
	unsigned int slot;

	slot = sheep_slot_constant(vm, expr);
	sheep_emit(&unit->code, SHEEP_GLOBAL, slot);
	return 0;
}

enum env_level {
	ENV_LOCAL,
	ENV_GLOBAL,
	ENV_FOREIGN,
};

static int lookup(struct sheep_context *context, const char *name,
		unsigned int *dist, unsigned int *slot,
		enum env_level *env_level)
{
	struct sheep_context *current = context;
	unsigned int distance = 0;
	void *entry;

	while (sheep_map_get(current->env, name, &entry)) {
		if (!current->parent)
			return -1;
		if (current->flags & SHEEP_CONTEXT_FUNCTION)
			distance++;
		current = current->parent;
	}

	*dist = distance;
	*slot = (unsigned long)entry;

	if (!current->parent)
		*env_level = ENV_GLOBAL;
	else if (!distance)
		*env_level = ENV_LOCAL;
	else
		*env_level = ENV_FOREIGN;

	return 0;
}

static unsigned int slot_foreign(struct sheep_unit *unit,
				unsigned int dist, unsigned int slot)
{
	struct sheep_vector *foreigns = unit->foreigns;
	struct sheep_foreign *foreign;

	if (!foreigns) {
		foreigns = sheep_malloc(sizeof(struct sheep_vector));
		sheep_vector_init(foreigns);
		unit->foreigns = foreigns;
	} else {
		unsigned int i;

		for (i = 0; i < foreigns->nr_items; i++) {
			foreign = foreigns->items[i];
			if (foreign->value.template.dist != dist)
				continue;
			if (foreign->value.template.slot != slot)
				continue;
			return i;
		}
	}

	foreign = sheep_malloc(sizeof(struct sheep_foreign));
	foreign->state = SHEEP_FOREIGN_TEMPLATE;
	foreign->value.template.dist = dist;
	foreign->value.template.slot = slot;

	return sheep_vector_push(foreigns, foreign);
}

static int __sheep_compile_name(struct sheep_vm *vm,
				struct sheep_unit *unit,
				struct sheep_context *context,
				sheep_t expr, int set)
{
	unsigned int dist, slot;
	enum env_level level;
	const char *name;

	name = sheep_cname(expr);
	if (lookup(context, name, &dist, &slot, &level)) {
		fprintf(stderr, "unbound name: %s\n", name);
		return -1;
	}

	switch (level) {
	case ENV_LOCAL:
		if (set)
			sheep_emit(&unit->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(&unit->code, SHEEP_LOCAL, slot);
		break;
	case ENV_GLOBAL:
		if (set)
			sheep_emit(&unit->code, SHEEP_SET_GLOBAL, slot);
		sheep_emit(&unit->code, SHEEP_GLOBAL, slot);
		break;
	case ENV_FOREIGN:
		slot = slot_foreign(unit, dist, slot);
		if (set)
			sheep_emit(&unit->code, SHEEP_SET_FOREIGN, slot);
		sheep_emit(&unit->code, SHEEP_FOREIGN, slot);
		break;
	}
	return 0;
}

int sheep_compile_name(struct sheep_vm *vm, struct sheep_unit *unit,
		struct sheep_context *context, sheep_t expr)
{
	return __sheep_compile_name(vm, unit, context, expr, 0);
}

int sheep_compile_set(struct sheep_vm *vm, struct sheep_unit *unit,
		struct sheep_context *context, sheep_t expr)
{
	return __sheep_compile_name(vm, unit, context, expr, 1);
}

static int compile_call(struct sheep_vm *vm, struct sheep_unit *unit,
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
		if (args->head->type->compile(vm, unit, &block, args->head))
			goto out;

	if (form->head->type->compile(vm, unit, context, form->head))
		goto out;

	if (context->flags & SHEEP_CONTEXT_TAILFORM) {
		printf("** tailcall to "), sheep_ddump(form->head);
		sheep_emit(&unit->code, SHEEP_TAILCALL, nargs);
	} else
		sheep_emit(&unit->code, SHEEP_CALL, nargs);
	ret = 0;
out:
	sheep_map_drain(&env);
	return ret;
}

int sheep_compile_list(struct sheep_vm *vm, struct sheep_unit *unit,
		struct sheep_context *context, sheep_t expr)
{
	struct sheep_list *list;

	list = sheep_list(expr);
	/* The empty list is a constant */
	if (!list->head)
		return sheep_compile_constant(vm, unit, context, expr);
	if (list->head->type == &sheep_name_type) {
		const char *op;
		void *entry;

		op = sheep_data(list->head);
		if (!sheep_map_get(&vm->specials, op, &entry)) {
			int (*compile_special)(struct sheep_vm *,
					struct sheep_unit *,
					struct sheep_context *,
					struct sheep_list *) = entry;
			struct sheep_list *args;

			args = sheep_list(list->tail);
			return compile_special(vm, unit, context, args);
		}
	}
	return compile_call(vm, unit, context, list);
}
