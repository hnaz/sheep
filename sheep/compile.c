/*
 * sheep/compile.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/vector.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

#include <sheep/compile.h>

/**
 * __sheep_compile - compile an expression
 * @vm: runtime
 * @module: module namespace
 * @exp: expression to compile
 *
 * Compiles @exp to bytecode, using @module as the namespace to
 * establish and resolve bindings.
 *
 * Returns a code object that can be executed by sheep_eval() with the
 * same @vm argument.
 */
struct sheep_code *__sheep_compile(struct sheep_vm *vm,
				struct sheep_module *module, sheep_t expr)
{
	struct sheep_context context;
	struct sheep_code *code;
	int err;

	code = sheep_malloc(sizeof(struct sheep_code));
	sheep_code_init(code);

	memset(&context, 0, sizeof(context));
	context.code = code;
	context.env = &module->env;

	sheep_protect(vm, expr);
	err = expr->type->compile(vm, &context, expr);
	sheep_unprotect(vm, expr);

	if (err) {
		sheep_free(code->code.items);
		sheep_free(code);
		return NULL;
	}

	sheep_emit(code, SHEEP_RET, 0);
	return code;
}

/**
 * sheep_compile_constant - compile a constant reference
 * @vm: runtime
 * @context: compilation context
 * @exp: constant expression
 *
 * Compilation helper that emits a direct load for a constant value.
 */
int sheep_compile_constant(struct sheep_vm *vm, struct sheep_context *context,
			sheep_t expr)
{
	unsigned int slot;

	slot = sheep_slot_constant(vm, expr);
	sheep_emit(context->code, SHEEP_GLOBAL, slot);
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
	struct sheep_function *last = context->function;
	struct sheep_context *current = context;
	unsigned int distance = 0;
	void *entry;

	while (sheep_map_get(current->env, name, &entry)) {
		if (!current->parent)
			return -1;
		current = current->parent;
		/*
		 * There can be several environment nestings within
		 * one function level, but for foreign slots we are
		 * only interested in the latter.
		 */
		if (current->function != last) {
			last = current->function;
			distance++;
		}
	}

	*dist = distance;
	*slot = (unsigned long)entry;

	if (!current->function)
		*env_level = ENV_GLOBAL;
	else if (current == context)
		*env_level = ENV_LOCAL;
	else
		*env_level = ENV_FOREIGN;

	return 0;
}

static unsigned int slot_foreign(struct sheep_context *context,
				unsigned int dist, unsigned int slot)
{
	struct sheep_function *function = context->function;
	struct sheep_vector *foreigns;

	foreigns = sheep_foreigns(function);
	if (!foreigns) {
		foreigns = sheep_malloc(sizeof(struct sheep_vector));
		sheep_vector_init(foreigns, 4);
		function->foreigns = foreigns;
	} else {
		unsigned int i;

		for (i = 0; i < foreigns->nr_items; i += 2) {
			unsigned int idist, islot;

			idist = (unsigned long)foreigns->items[i];
			islot = (unsigned long)foreigns->items[i + 1];

			if (idist == dist && islot == slot)
				return i / 2;
		}
	}
	sheep_vector_push(foreigns, (void *)(unsigned long)dist);
	sheep_vector_push(foreigns, (void *)(unsigned long)slot);
	return (foreigns->nr_items - 1) / 2;
}

/**
 * sheep_compile_name - compile a name reference
 * @vm: runtime
 * @context: compilation context
 * @exp: name expression
 *
 * Compilation helper that emits an appropriate load for the slot
 * bound to the name in @exp, given that a binding exists.
 *
 * Returns 0 on success, -1 otherwise.
 */
int sheep_compile_name(struct sheep_vm *vm, struct sheep_context *context,
		sheep_t expr)
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
		sheep_emit(context->code, SHEEP_LOCAL, slot);
		break;
	case ENV_GLOBAL:
		sheep_emit(context->code, SHEEP_GLOBAL, slot);
		break;
	case ENV_FOREIGN:
		slot = slot_foreign(context, dist, slot);
		sheep_emit(context->code, SHEEP_FOREIGN, slot);
		break;
	}
	return 0;
}

int sheep_compile_set(struct sheep_vm *vm, struct sheep_context *context,
		sheep_t expr)
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
		sheep_emit(context->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(context->code, SHEEP_LOCAL, slot);
		break;
	case ENV_GLOBAL:
		sheep_emit(context->code, SHEEP_SET_GLOBAL, slot);
		sheep_emit(context->code, SHEEP_GLOBAL, slot);
		break;
	case ENV_FOREIGN:
		slot = slot_foreign(context, dist, slot);
		sheep_emit(context->code, SHEEP_SET_FOREIGN, slot);
		sheep_emit(context->code, SHEEP_FOREIGN, slot);
		break;
	}
	return 0;
}

static int compile_call(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *form)
{
	struct sheep_list *args;
	int nargs;

	args = sheep_list(form->tail);
	for (nargs = 0; args->head; args = sheep_list(args->tail), nargs++)
		if (args->head->type->compile(vm, context, args->head))
			return -1;

	if (form->head->type->compile(vm, context, form->head))
		return -1;
	sheep_emit(context->code, SHEEP_CALL, nargs);
	return 0;
}

/**
 * sheep_compile_list - compile a list form
 * @vm: runtime
 * @context: compilation context
 * @expr: list
 *
 * Compilation helper for list expressions, which can be the empty
 * list constant, a special form or a function call.
 *
 * Returns 0 on success, -1 otherwise.
 */
int sheep_compile_list(struct sheep_vm *vm, struct sheep_context *context,
		sheep_t expr)
{
	struct sheep_list *list;

	list = sheep_list(expr);
	/* The empty list is a constant */
	if (!list->head)
		return sheep_compile_constant(vm, context, expr);
	if (list->head->type == &sheep_name_type) {
		const char *op;
		void *entry;

		op = sheep_data(list->head);
		if (!sheep_map_get(&vm->specials, op, &entry)) {
			int (*compile_special)(struct sheep_vm *,
					struct sheep_context *,
					struct sheep_list *) = entry;
			struct sheep_list *args;

			args = sheep_list(list->tail);
			return compile_special(vm, context, args);
		}
	}
	return compile_call(vm, context, list);
}
