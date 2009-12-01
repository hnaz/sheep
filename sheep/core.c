/*
 * sheep/core.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/compile.h>
#include <sheep/module.h>
#include <sheep/unpack.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <sheep/core.h>

/* (quote expr) */
static int compile_quote(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	unsigned int slot;
	sheep_t obj;

	if (sheep_unpack_list("quote", args, "o", &obj))
		return -1;

	slot = sheep_vm_constant(vm, obj);
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

static int do_compile_forms(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	for (;;) {
		sheep_t value = args->head;

		if (tailposition(context, args))
			context->flags |= SHEEP_CONTEXT_TAILFORM;

		if (sheep_compile_object(vm, function, context, value))
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

static int do_compile_block(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *parent, struct sheep_map *env,
			struct sheep_list *args, int function_body)
{
	struct sheep_context context = {
		.env = env,
		.parent = parent,
	};

	if (function_body)
		context.flags |= SHEEP_CONTEXT_FUNCTION;

	return do_compile_forms(vm, function, &context, args);
}

/* (block expr*) */
static int compile_block(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	SHEEP_DEFINE_MAP(env);
	int ret;

	/* Just make sure the block is not empty */
	if (sheep_unpack_list("block", args, "r!", &args))
		return -1;

	ret = do_compile_block(vm, function, context, &env, args, 0);

	sheep_map_drain(&env);
	return ret;
}

/* (with ([name expr]*) expr*) */
static int compile_with(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	struct sheep_list *bindings, *body;
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	int ret = -1;

	if (sheep_unpack_list("with", args, "Lr!", &bindings, &body))
		return -1;

	while (bindings->head) {
		unsigned int slot;
		const char *name;
		sheep_t value;

		if (sheep_unpack_list("with", bindings, "Aor", &name, &value,
					&bindings))
			goto out;

		if (sheep_compile_object(vm, function, &block, value))
			goto out;

		slot = sheep_function_local(function);
		sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
		sheep_map_set(&env, name, (void *)(unsigned long)slot);
	}

	ret = do_compile_forms(vm, function, &block, body);
out:
	sheep_map_drain(&env);
	return ret;
}

/* (variable name expr) */
static int compile_variable(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	unsigned int slot;
	const char *name;
	sheep_t value;

	if (sheep_unpack_list("variable", args, "Ao", &name, &value))
		return -1;

	if (sheep_compile_object(vm, function, context, value))
		return -1;

	if (context->parent) {
		slot = sheep_function_local(function);
		sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(&function->code, SHEEP_LOCAL, slot);
	} else {
		slot = sheep_vm_global(vm);
		sheep_emit(&function->code, SHEEP_SET_GLOBAL, slot);
		sheep_emit(&function->code, SHEEP_GLOBAL, slot);
	}
	sheep_map_set(context->env, name, (void *)(unsigned long)slot);
	return 0;
}

/* (function name? (arg*) expr*) */
static int compile_function(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	unsigned int cslot, bslot = bslot;
	struct sheep_function *childfun;
	struct sheep_list *parms, *body;
	SHEEP_DEFINE_MAP(env);
	const char *name;
	sheep_t sheep;
	int ret = -1;

	if (args->head && sheep_type(args->head) == &sheep_name_type) {
		name = sheep_cname(args->head);
		args = sheep_list(args->tail);
	} else
		name = NULL;

	if (sheep_unpack_list("function", args, "Lr!", &parms, &body))
		return -1;

	sheep = sheep_make_function(vm, name);
	childfun = sheep_data(sheep);

	while (parms->head) {
		unsigned int slot;
		const char *parm;

		if (sheep_unpack_list("function", parms, "Ar", &parm, &parms))
			goto out;
		slot = sheep_function_local(childfun);
		sheep_map_set(&env, parm, (void *)(unsigned long)slot);
		childfun->nr_parms++;
	}

	cslot = sheep_vm_constant(vm, sheep);
	if (name) {
		if (context->parent)
			bslot = sheep_function_local(function);
		else
			bslot = sheep_vm_global(vm);
		sheep_map_set(context->env, name, (void *)(unsigned long)bslot);
	}

	sheep_protect(vm, sheep);
	ret = do_compile_block(vm, childfun, context, &env, body, 1);
	sheep_unprotect(vm, sheep);
	if (ret) {
		/*
		 * Potentially sacrifice a global slot, but take out
		 * the binding to it if it doesn't contain anything
		 * useful.  This can probably be done better one
		 * day...
		 */
		if (name)
			sheep_map_del(context->env, name);
		goto out;
	}
	sheep_code_finalize(&childfun->code);

	sheep_emit(&function->code, SHEEP_CLOSURE, cslot);
	if (name) {
		if (context->parent) {
			sheep_emit(&function->code, SHEEP_SET_LOCAL, bslot);
			sheep_emit(&function->code, SHEEP_LOCAL, bslot);
		} else {
			sheep_emit(&function->code, SHEEP_SET_GLOBAL, bslot);
			sheep_emit(&function->code, SHEEP_GLOBAL, bslot);
		}
	}
out:
	sheep_map_drain(&env);
	return ret;
}

static int do_compile_chain(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args,
			const char *name, enum sheep_opcode endbranch)
{
	SHEEP_DEFINE_MAP(env);
	struct sheep_context block = {
		.env = &env,
		.parent = context,
	};
	unsigned long Lend;
	int ret = -1;

	if (sheep_unpack_list(name, args, "r!", &args))
		goto out;

	Lend = sheep_code_jump(&function->code);
	for (;;) {
		sheep_t exp;

		if (tailposition(context, args))
			block.flags |= SHEEP_CONTEXT_TAILFORM;

		if (sheep_unpack_list(name, args, "or", &exp, &args))
			goto out;

		if (sheep_compile_object(vm, function, &block, exp))
			goto out;

		if (!args->head)
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
static int compile_or(struct sheep_vm *vm, struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	return do_compile_chain(vm, function, context, args, "or", SHEEP_BRT);
}

/* (and one two three*?) */
static int compile_and(struct sheep_vm *vm, struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	return do_compile_chain(vm, function, context, args, "and", SHEEP_BRF);
}

/* (if cond then else*?) */
static int compile_if(struct sheep_vm *vm, struct sheep_function *function,
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

	if (sheep_unpack_list("if", args, "oor", &cond, &then, &elseform))
		goto out;

	Lelse = sheep_code_jump(&function->code);

	if (sheep_compile_object(vm, function, &block, cond))
		goto out;
	sheep_emit(&function->code, SHEEP_BRF, Lelse);

	sheep_emit(&function->code, SHEEP_DROP, 0);
	if (context->flags & SHEEP_CONTEXT_TAILFORM)
		block.flags |= SHEEP_CONTEXT_TAILFORM;
	if (sheep_compile_object(vm, function, &block, then))
		goto out;
	block.flags &= ~SHEEP_CONTEXT_TAILFORM;

	if (elseform->head) {
		unsigned long Lend;

		Lend = sheep_code_jump(&function->code);
		sheep_emit(&function->code, SHEEP_BR, Lend);

		sheep_code_label(&function->code, Lelse);

		sheep_emit(&function->code, SHEEP_DROP, 0);
		if (do_compile_forms(vm, function, &block, elseform))
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
static int compile_set(struct sheep_vm *vm, struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	sheep_t name, value;

	if (sheep_unpack_list("set", args, "ao", &name, &value))
		return -1;

	if (sheep_compile_object(vm, function, context, value))
		return -1;

	return sheep_compile_set(vm, function, context, name);
}

/* (load name) */
static int compile_load(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	sheep_t mod, name_;
	unsigned int slot;
	const char *name;
	int ret = -1;

	if (context->parent) {
		fprintf(stderr, "load not on toplevel form\n");
		return -1;
	}

	if (sheep_unpack_list("load", args, "a", &name_))
		return -1;

	sheep_protect(vm, name_);
	name = sheep_cname(name_);

	mod = sheep_module_load(vm, name);
	if (!mod)
		goto out;

	slot = sheep_vm_constant(vm, mod);
	sheep_emit(&function->code, SHEEP_GLOBAL, slot);
	sheep_map_set(context->env, name, (void *)(unsigned long)slot);
	ret = 0;
out:
	sheep_unprotect(vm, name_);
	return ret;
}

void sheep_core_init(struct sheep_vm *vm)
{
	sheep_map_set(&vm->specials, "quote", compile_quote);
	sheep_map_set(&vm->specials, "block", compile_block);
	sheep_map_set(&vm->specials, "with", compile_with);
	sheep_map_set(&vm->specials, "variable", compile_variable);
	sheep_map_set(&vm->specials, "function", compile_function);
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
