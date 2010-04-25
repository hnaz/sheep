/*
 * sheep/core.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/foreign.h>
#include <sheep/compile.h>
#include <sheep/module.h>
#include <sheep/parse.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/type.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <sheep/core.h>

/* (quote expr) */
static int compile_quote(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	unsigned int slot;
	sheep_t expr;

	if (sheep_parse(compile, args, "e", &expr))
		return -1;

	slot = sheep_vm_constant(compile->vm, expr);
	sheep_emit(&function->code, SHEEP_GLOBAL, slot);
	return 0;
}

static int tailposition(struct sheep_context *context, struct sheep_list *block)
{
	struct sheep_list *next;

	next = sheep_list(block->tail);
	if (next->head)
		return 0;

	if (context->flags & SHEEP_CONTEXT_FUNCTION)
		return 1;

	if (!context->parent)
		return 0;

	if (context->parent->flags & SHEEP_CONTEXT_TAILFORM)
		return 1;

	return 0;
}

static int do_compile_forms(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	for (;;) {
		sheep_t value = args->head;

		if (tailposition(context, args))
			context->flags |= SHEEP_CONTEXT_TAILFORM;

		if (sheep_compile_object(compile, function, context, value))
			return -1;

		args = sheep_list(args->tail);
		if (!args->head)
			break;
		/*
		 * The last value is the value of the whole block,
		 * drop everything in between to keep the stack
		 * balanced.
		 */
		sheep_emit(&function->code, SHEEP_DROP, 0);
	}
	return 0;
}

static int do_compile_block(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *parent, struct sheep_map *env,
			struct sheep_list *args, int function_body)
{
	struct sheep_context context = {
		.env = env,
		.parent = parent,
	};

	if (function_body)
		context.flags |= SHEEP_CONTEXT_FUNCTION;

	return do_compile_forms(compile, function, &context, args);
}

/* (block expr*) */
static int compile_block(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	SHEEP_DEFINE_MAP(env);
	int ret;

	/* Just make sure the block is not empty */
	if (sheep_parse(compile, args, "R", &args))
		return -1;

	ret = do_compile_block(compile, function, context, &env, args, 0);

	sheep_map_drain(&env);
	return ret;
}

/* (with (name value) expr*) */
static int compile_with(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	struct sheep_list *binding, *body;
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	unsigned int slot;
	const char *name;
	sheep_t value;
	int ret;

	if (sheep_parse(compile, args, "lR", &binding, &body))
		return -1;

	if (__sheep_parse(compile, args, binding, "se", &name, &value))
		return -1;

	if (sheep_compile_object(compile, function, &block, value))
		return -1;

	slot = sheep_function_local(function);
	sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
	sheep_map_set(&env, name, (void *)(unsigned long)slot);

	ret = do_compile_forms(compile, function, &block, body);

	sheep_map_drain(&env);
	return ret;
}

/* (variable name expr) */
static int compile_variable(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	unsigned int slot;
	const char *name;
	sheep_t value;
	int ret = -1;

	if (sheep_parse(compile, args, "se", &name, &value))
		goto out;

	if (sheep_compile_object(compile, function, &block, value))
		goto out;

	sheep_emit(&function->code, SHEEP_DUP, 0);

	if (context->parent) {
		slot = sheep_function_local(function);
		sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
	} else {
		slot = sheep_vm_global(compile->vm);
		sheep_emit(&function->code, SHEEP_SET_GLOBAL, slot);
	}
	sheep_map_set(context->env, name, (void *)(unsigned long)slot);
	ret = 0;
out:
	sheep_map_drain(&env);
	return ret;
}

/* (function name? (arg*) expr*) */
static int compile_function(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	struct sheep_list *parms, *body;
	struct sheep_function *childfun;
	SHEEP_DEFINE_MAP(env);
	unsigned int cslot;
	sheep_t maybe_name;
	const char *name;
	sheep_t sheep;
	int ret = -1;

	maybe_name = sheep_list(args->tail)->head;
	if (maybe_name && sheep_type(maybe_name) == &sheep_name_type) {
		if (sheep_parse(compile, args, "slR", &name, &parms, &body))
			return -1;
	} else {
		if (sheep_parse(compile, args, "lR", &parms, &body))
			return -1;
		name = NULL;
	}

	sheep = sheep_make_function(compile->vm, name);
	childfun = sheep_data(sheep);

	while (parms->head) {
		struct sheep_list *rest;
		unsigned int slot;
		const char *parm;

		if (__sheep_parse(compile, args, parms, "sr", &parm, &rest))
			goto out;

		slot = sheep_function_local(childfun);
		if (sheep_map_set(&env, parm, (void *)(unsigned long)slot)) {
			sheep_parser_error(compile, parms->head,
					"duplicate function parameter");
			goto out;
		}

		childfun->nr_parms++;
		parms = rest;
	}

	cslot = sheep_vm_constant(compile->vm, sheep);
	sheep_emit(&function->code, SHEEP_CLOSURE, cslot);

	if (name) {
		unsigned int slot;

		sheep_emit(&function->code, SHEEP_DUP, 0);

		if (context->parent) {
			slot = sheep_function_local(function);
			sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
		} else {
			slot = sheep_vm_global(compile->vm);
			sheep_emit(&function->code, SHEEP_SET_GLOBAL, slot);
		}
		sheep_map_set(context->env, name, (void *)(unsigned long)slot);
	}

	sheep_protect(compile->vm, sheep);
	ret = do_compile_block(compile, childfun, context, &env, body, 1);
	sheep_unprotect(compile->vm, sheep);
	if (ret) {
		/* Do not leave the dead slot bound... */
		if (name)
			sheep_map_del(context->env, name);
		goto out;
	}
	sheep_code_finalize(&childfun->code);
	if (childfun->foreign)
		sheep_foreign_propagate(function, childfun);
out:
	sheep_map_drain(&env);
	return ret;
}

/* (type name slotnames*) */
static int compile_type(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	const char **slotnames = NULL;
	unsigned int nr_slots = 0;
	struct sheep_list *names;
	unsigned int cslot;
	const char *name;
	sheep_t class;

	if (sheep_parse(compile, args, "sR", &name, &names))
		return -1;

	do {
		const char *slotname;

		if (__sheep_parse(compile, args, names, "sr", &slotname, &names))
			goto err;

		slotnames = sheep_realloc(slotnames, sizeof(char *) * ++nr_slots);
		slotnames[nr_slots - 1] = sheep_strdup(slotname);
	} while (names->head);

	class = sheep_make_typeclass(compile->vm, name, slotnames, nr_slots);

	cslot = sheep_vm_constant(compile->vm, class);
	sheep_emit(&function->code, SHEEP_GLOBAL, cslot);
	sheep_emit(&function->code, SHEEP_DUP, 0);
	if (context->parent) {
		cslot = sheep_function_local(function);
		sheep_emit(&function->code, SHEEP_SET_LOCAL, cslot);
	} else {
		cslot = sheep_vm_global(compile->vm);
		sheep_emit(&function->code, SHEEP_SET_GLOBAL, cslot);
	}
	sheep_map_set(context->env, name, (void *)(unsigned long)cslot);
	return 0;
err:
	while (nr_slots--)
		sheep_free(slotnames[nr_slots]);
	sheep_free(slotnames);
	return -1;
}

static int do_compile_chain(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context,
			struct sheep_list *args,
			enum sheep_opcode endbranch)
{
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	struct sheep_list *args2;
	unsigned long Lend;
	int ret = -1;

	if (sheep_parse(compile, args, "R", &args2))
		return -1;

	Lend = sheep_code_jump(&function->code);
	for (;;) {
		sheep_t expr;

		if (tailposition(context, args2))
			block.flags |= SHEEP_CONTEXT_TAILFORM;

		if (__sheep_parse(compile, args, args2, "er", &expr, &args2))
			goto out;

		if (sheep_compile_object(compile, function, &block, expr))
			goto out;

		if (!args2->head)
			break;

		sheep_emit(&function->code, endbranch, Lend);
		sheep_emit(&function->code, SHEEP_DROP, 0);
	}

	sheep_code_label(&function->code, Lend);
	ret = 0;
out:
	sheep_map_drain(&env);
	return ret;
}

/* (or one two three*?) */
static int compile_or(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	return do_compile_chain(compile, function, context, args, SHEEP_BRT);
}

/* (and one two three*?) */
static int compile_and(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	return do_compile_chain(compile, function, context, args, SHEEP_BRF);
}

/* (if cond then else*?) */
static int compile_if(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	struct sheep_list *elseform;
	unsigned long Lelse;
	sheep_t cond, then;
	int ret = -1;

	if (sheep_parse(compile, args, "eer", &cond, &then, &elseform))
		return -1;

	Lelse = sheep_code_jump(&function->code);

	if (sheep_compile_object(compile, function, &block, cond))
		goto out;
	sheep_emit(&function->code, SHEEP_BRF, Lelse);

	sheep_emit(&function->code, SHEEP_DROP, 0);
	if (context->flags & SHEEP_CONTEXT_TAILFORM)
		block.flags |= SHEEP_CONTEXT_TAILFORM;
	if (sheep_compile_object(compile, function, &block, then))
		goto out;
	block.flags &= ~SHEEP_CONTEXT_TAILFORM;

	if (elseform->head) {
		unsigned long Lend;

		Lend = sheep_code_jump(&function->code);
		sheep_emit(&function->code, SHEEP_BR, Lend);

		sheep_code_label(&function->code, Lelse);

		sheep_emit(&function->code, SHEEP_DROP, 0);
		if (do_compile_forms(compile, function, &block, elseform))
			goto out;

		sheep_code_label(&function->code, Lend);
	} else
		sheep_code_label(&function->code, Lelse);

	ret = 0;
out:
	sheep_map_drain(&env);
	return ret;
}

/* (set name value) */
static int compile_set(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	struct sheep_name *name;
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	sheep_t value;
	int ret = -1;

	if (sheep_parse(compile, args, "ne", &name, &value))
		goto out;

	if (sheep_compile_object(compile, function, &block, value))
		goto out;

	ret = sheep_compile_set(compile, function, context,
				sheep_list(args->tail)->head);
out:
	sheep_map_drain(&env);
	return ret;
}

/* (load name) */
static int compile_load(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	unsigned int slot;
	const char *name;

	if (sheep_parse(compile, args, "s", &name))
		return -1;

	slot = sheep_vm_key(compile->vm, name);
	sheep_emit(&function->code, SHEEP_LOAD, slot);

	sheep_emit(&function->code, SHEEP_DUP, 0);

	if (context->parent) {
		slot = sheep_function_local(function);
		sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
	} else {
		slot = sheep_vm_global(compile->vm);
		sheep_emit(&function->code, SHEEP_SET_GLOBAL, slot);
	}
	sheep_map_set(context->env, name, (void *)(unsigned long)slot);
	return 0;
}

void sheep_core_init(struct sheep_vm *vm)
{
	sheep_map_set(&vm->specials, "quote", compile_quote);
	sheep_map_set(&vm->specials, "block", compile_block);
	sheep_map_set(&vm->specials, "with", compile_with);
	sheep_map_set(&vm->specials, "variable", compile_variable);
	sheep_map_set(&vm->specials, "function", compile_function);
	sheep_map_set(&vm->specials, "type", compile_type);
	sheep_map_set(&vm->specials, "or", compile_or);
	sheep_map_set(&vm->specials, "and", compile_and);
	sheep_map_set(&vm->specials, "if", compile_if);
	sheep_map_set(&vm->specials, "set", compile_set);
	sheep_map_set(&vm->specials, "load", compile_load);
}

void sheep_core_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->specials);
}
