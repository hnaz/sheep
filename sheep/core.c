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
	sheep_t obj;

	if (sheep_unpack_form(compile, "quote", args, "o", &obj))
		return -1;

	slot = sheep_vm_constant(compile->vm, obj);
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
	if (sheep_unpack_form(compile, "block", args, "r!", &args))
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
	struct sheep_name *name;
	sheep_t name_, value;
	unsigned int slot;
	int ret;

	if (sheep_unpack_form(compile, "with", args, "Lr!", &binding, &body))
		return -1;

	if (sheep_unpack_form(compile, "with", binding, "ao", &name_, &value))
		return -1;

	name = sheep_name(name_);
	if (name->nr_parts != 1) {
		sheep_compiler_error(compile, name_, "with: invalid name");
		return -1;
	}

	if (sheep_compile_object(compile, function, &block, value))
		return -1;

	slot = sheep_function_local(function);
	sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
	sheep_map_set(&env, *name->parts, (void *)(unsigned long)slot);

	ret = do_compile_forms(compile, function, &block, body);

	sheep_map_drain(&env);
	return ret;
}

/* (variable name expr) */
static int compile_variable(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	struct sheep_name *name;
	sheep_t name_, value;
	unsigned int slot;

	if (sheep_unpack_form(compile, "variable", args, "ao", &name_, &value))
		return -1;

	name = sheep_name(name_);
	if (name->nr_parts != 1) {
		sheep_compiler_error(compile, name_, "variable: invalid name");
		return -1;
	}

	if (sheep_compile_object(compile, function, context, value))
		return -1;

	if (context->parent) {
		slot = sheep_function_local(function);
		sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(&function->code, SHEEP_LOCAL, slot);
	} else {
		slot = sheep_vm_global(compile->vm);
		sheep_emit(&function->code, SHEEP_SET_GLOBAL, slot);
		sheep_emit(&function->code, SHEEP_GLOBAL, slot);
	}
	sheep_map_set(context->env, *name->parts, (void *)(unsigned long)slot);
	return 0;
}

/* (function name? (arg*) expr*) */
static int compile_function(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	struct sheep_function *childfun;
	struct sheep_list *parms, *body;
	SHEEP_DEFINE_MAP(env);
	unsigned int cslot;
	const char *name;
	sheep_t sheep;
	int ret = -1;

	if (args->head && sheep_type(args->head) == &sheep_name_type) {
		struct sheep_name *n;

		n = sheep_name(args->head);
		if (n->nr_parts != 1) {
			sheep_compiler_error(compile, args->head,
					"function: invalid name");
			return -1;
		}
		name = *n->parts;
		args = sheep_list(args->tail);
	} else
		name = NULL;

	if (sheep_unpack_form(compile, "function", args, "Lr!", &parms, &body))
		return -1;

	sheep = sheep_make_function(compile->vm, name);
	childfun = sheep_data(sheep);

	while (parms->head) {
		struct sheep_name *parm;
		unsigned int slot;
		sheep_t parm_;

		if (sheep_unpack_form(compile, "function", parms,
					"ar", &parm_, &parms))
			goto out;

		parm = sheep_name(parm_);
		if (parm->nr_parts != 1) {
			sheep_compiler_error(compile, parm_,
					"function: invalid parameter name");
			goto out;
		}

		slot = sheep_function_local(childfun);
		if (sheep_map_set(&env, *parm->parts, (void *)(unsigned long)slot)) {
			sheep_compiler_error(compile, parm_,
					"function: duplicate parameter: %s",
					*parm->parts);
			goto out;
		}

		childfun->nr_parms++;
	}

	cslot = sheep_vm_constant(compile->vm, sheep);
	sheep_emit(&function->code, SHEEP_CLOSURE, cslot);

	if (name) {
		unsigned int slot;

		if (context->parent) {
			slot = sheep_function_local(function);
			sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
			sheep_emit(&function->code, SHEEP_LOCAL, slot);
		} else {
			slot = sheep_vm_global(compile->vm);
			sheep_emit(&function->code, SHEEP_SET_GLOBAL, slot);
			sheep_emit(&function->code, SHEEP_GLOBAL, slot);
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
out:
	sheep_map_drain(&env);
	return ret;
}

static int do_compile_chain(struct sheep_compile *compile,
			struct sheep_function *function,
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

	if (sheep_unpack_form(compile, name, args, "r!", &args))
		goto out;

	Lend = sheep_code_jump(&function->code);
	for (;;) {
		sheep_t exp;

		if (tailposition(context, args))
			block.flags |= SHEEP_CONTEXT_TAILFORM;

		if (sheep_unpack_form(compile, name, args, "or", &exp, &args))
			goto out;

		if (sheep_compile_object(compile, function, &block, exp))
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
static int compile_or(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	return do_compile_chain(compile, function, context,
				args, "or", SHEEP_BRT);
}

/* (and one two three*?) */
static int compile_and(struct sheep_compile *compile,
		struct sheep_function *function,
		struct sheep_context *context, struct sheep_list *args)
{
	return do_compile_chain(compile, function, context,
				args, "and", SHEEP_BRF);
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

	if (sheep_unpack_form(compile, "if", args, "oor", &cond, &then, &elseform))
		goto out;

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
	sheep_t name, value;

	if (sheep_unpack_form(compile, "set", args, "ao", &name, &value))
		return -1;

	if (sheep_compile_object(compile, function, context, value))
		return -1;

	return sheep_compile_set(compile, function, context, name);
}

/* (load name) */
static int compile_load(struct sheep_compile *compile,
			struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	struct sheep_name *name;
	sheep_t mod, name_;
	unsigned int slot;
	int ret = -1;

	if (context->parent) {
		fprintf(stderr, "load: not on toplevel\n");
		return -1;
	}

	if (sheep_unpack_form(compile, "load", args, "a", &name_))
		return -1;

	sheep_protect(compile->vm, name_);
	name = sheep_name(name_);

	if (name->nr_parts != 1) {
		sheep_compiler_error(compile, name_,
				"load: invalid module name");
		goto out;
	}

	mod = sheep_module_load(compile->vm, *name->parts);
	if (!mod)
		goto out;

	slot = sheep_vm_constant(compile->vm, mod);
	sheep_emit(&function->code, SHEEP_GLOBAL, slot);
	sheep_map_set(context->env, *name->parts, (void *)(unsigned long)slot);
	ret = 0;
out:
	sheep_unprotect(compile->vm, name_);
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
