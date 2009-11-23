/*
 * sheep/core.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/compile.h>
#include <sheep/module.h>
#include <sheep/number.h>
#include <sheep/string.h>
#include <sheep/unpack.h>
#include <sheep/alien.h>
#include <sheep/bool.h>
#include <sheep/code.h>
#include <sheep/eval.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sheep/core.h>

static int unpack(const char *caller, struct sheep_list *list,
		const char *items, ...)
{
	enum sheep_unpack status;
	const char *wanted;
	sheep_t mismatch;
	va_list ap;

	va_start(ap, items);
	status = __sheep_unpack_list(&wanted, &mismatch, list, items, ap);
	va_end(ap);

	switch (status) {
	case SHEEP_UNPACK_OK:
		return 0;
	case SHEEP_UNPACK_MISMATCH:
		fprintf(stderr, "%s: expected %s, got %s\n",
			caller, wanted, sheep_type(mismatch)->name);
		return -1;
	case SHEEP_UNPACK_TOO_MANY:
		fprintf(stderr, "%s: too few arguments\n", caller);
		return -1;
	case SHEEP_UNPACK_TOO_FEW:
		fprintf(stderr, "%s: too many arguments\n", caller);
	default: /* weird compiler... */
		return -1;
	}
}

/* (quote expr) */
static int compile_quote(struct sheep_vm *vm, struct sheep_function *function,
			struct sheep_context *context, struct sheep_list *args)
{
	unsigned int slot;
	sheep_t obj;

	if (unpack("quote", args, "o", &obj))
		return -1;

	slot = sheep_slot_constant(vm, obj);
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
	if (unpack("block", args, "r!", &args))
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

	if (unpack("with", args, "Lr!", &bindings, &body))
		return -1;

	while (bindings->head) {
		unsigned int slot;
		const char *name;
		sheep_t value;

		if (unpack("with", bindings, "Aor", &name, &value, &bindings))
			goto out;

		if (sheep_compile_object(vm, function, &block, value))
			goto out;

		slot = sheep_slot_local(function);
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

	if (unpack("variable", args, "Ao", &name, &value))
		return -1;

	if (sheep_compile_object(vm, function, context, value))
		return -1;

	if (context->parent) {
		slot = sheep_slot_local(function);
		sheep_emit(&function->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(&function->code, SHEEP_LOCAL, slot);
	} else {
		slot = sheep_slot_global(vm);
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

	if (unpack("function", args, "Lr!", &parms, &body))
		return -1;

	sheep = sheep_make_function(vm, name);
	childfun = sheep_data(sheep);

	while (parms->head) {
		unsigned int slot;
		const char *parm;

		if (unpack("function", parms, "Ar", &parm, &parms))
			goto out;
		slot = sheep_slot_local(childfun);
		sheep_map_set(&env, parm, (void *)(unsigned long)slot);
		childfun->nr_parms++;
	}

	cslot = sheep_slot_constant(vm, sheep);
	if (name) {
		if (context->parent)
			bslot = sheep_slot_local(function);
		else
			bslot = sheep_slot_global(vm);
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

	if (unpack(name, args, "r!", &args))
		goto out;

	Lend = sheep_code_jump(&function->code);
	for (;;) {
		sheep_t exp;

		if (tailposition(context, args))
			block.flags |= SHEEP_CONTEXT_TAILFORM;

		if (unpack(name, args, "or", &exp, &args))
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

	if (unpack("if", args, "oor", &cond, &then, &elseform))
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

	if (unpack("set", args, "ao", &name, &value))
		return -1;

	if (sheep_compile_object(vm, function, context, value))
		return -1;

	return sheep_compile_set(vm, function, context, name);
}

int sheep_unpack_stack(const char *caller, struct sheep_vm *vm,
		unsigned int nr_args, const char *items, ...)
{
	enum sheep_unpack status;
	const char *wanted;
	sheep_t mismatch;
	va_list ap;

	if (strlen(items) != nr_args) {
		fprintf(stderr, "%s: too %s arguments\n", caller,
			strlen(items) > nr_args ? "few" : "many");
		return -1;
	}

	va_start(ap, items);
	status = __sheep_unpack_stack(&wanted, &mismatch, &vm->stack, items,ap);
	va_end(ap);

	switch (status) {
	case SHEEP_UNPACK_OK:
		return 0;
	case SHEEP_UNPACK_MISMATCH:
		fprintf(stderr, "%s: expected %s, got %s\n",
			caller, wanted, sheep_type(mismatch)->name);
		return -1;
	default: /* should not happen, nr_args is trustworthy */
		return -1;
	}
}

/* (string expression) */
static sheep_t eval_string(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;
	char *buf;

	if (sheep_unpack_stack("string", vm, nr_args, "o", &sheep))
		return NULL;

	if (sheep_type(sheep) == &sheep_string_type)
		return sheep;

	buf = sheep_format(sheep);
	return __sheep_make_string(vm, buf);
}

static char *do_split(char **stringp, const char *delim)
{
	char *match, *result;

	match = strstr(*stringp, delim);
	result = *stringp;
	if (!match) {
		*stringp = NULL;
		return result;
	}

	*match = 0;
	*stringp = match + strlen(delim);
	return result;
}

/* (split string string) */
static sheep_t eval_split(struct sheep_vm *vm, unsigned int nr_args)
{
	const char *string, *token;
	struct sheep_list *list;
	sheep_t list_, token_;
	char *pos, *orig;
	int empty;

	if (sheep_unpack_stack("split", vm, nr_args, "sS", &token_, &string))
		return NULL;
	sheep_protect(vm, token_);
	pos = orig = sheep_strdup(string);

	list_ = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, list_);

	token = sheep_rawstring(token_);
	empty = !strlen(token);

	list = sheep_list(list_);
	while (pos) {
		sheep_t item;
		/*
		 * Empty splitting separates out every single
		 * character: (split "" "foo") => ("f" "o" "o")
		 */
		if (empty) {
			char str[2] = { *pos, 0 };

			item = sheep_make_string(vm, str);
			if (!pos[0] || !pos[1])
				pos = NULL;
			else
				pos++;
		} else
			item = sheep_make_string(vm, do_split(&pos, token));

		list->head = item;
		list->tail = sheep_make_list(vm, NULL, NULL);
		list = sheep_list(list->tail);
	}
	sheep_free(orig);

	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, token_);

	return list_;
}

/* (join delimiter list-of-strings) */
static sheep_t eval_join(struct sheep_vm *vm, unsigned int nr_args)
{
	char *new = NULL, *result = NULL;
	size_t length = 0, dlength;
	struct sheep_list *list;
	sheep_t delim_, list_;
	const char *delim;

	if (sheep_unpack_stack("join", vm, nr_args, "sl", &delim_, &list_))
		return NULL;

	sheep_protect(vm, delim_);
	sheep_protect(vm, list_);

	delim = sheep_rawstring(delim_);
	list = sheep_list(list_);

	dlength = strlen(delim);

	while (list->head) {
		const char *string;
		size_t newlength;

		if (unpack("join", list, "Sr", &string, &list))
			goto out;

		newlength = length + strlen(string);
		if (list->head)
			newlength += dlength;
		new = sheep_realloc(new, newlength + 1);
		strcpy(new + length, string);
		if (list->head)
			strcpy(new + newlength - dlength, delim);
		length = newlength;
	}

	if (new)
		result = new;
	else
		result = sheep_strdup("");
out:
	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, delim_);

	if (result)
		return __sheep_make_string(vm, result);
	return NULL;
}

/* (map function list) */
static sheep_t eval_map(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t mapper, old, new, result = NULL;
	struct sheep_list *l_old, *l_new;

	if (sheep_unpack_stack("map", vm, nr_args, "cl", &mapper, &old))
		return NULL;
	sheep_protect(vm, mapper);
	sheep_protect(vm, old);

	new = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, new);

	l_old = sheep_list(old);
	l_new = sheep_list(new);

	while (l_old->head) {
		l_new->head = sheep_call(vm, mapper, 1, l_old->head);
		if (!l_new->head)
			goto out;
		l_new->tail = sheep_make_list(vm, NULL, NULL);
		l_new = sheep_list(l_new->tail);
		l_old = sheep_list(l_old->tail);
	}
	result = new;
out:
	sheep_unprotect(vm, new);
	sheep_unprotect(vm, old);
	sheep_unprotect(vm, mapper);

	return result;
}

/* (reduce function list) */
static sheep_t eval_reduce(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t reducer, list_, a, b, value, result = NULL;
	struct sheep_list *list;

	if (sheep_unpack_stack("reduce", vm, nr_args, "cl", &reducer, &list_))
		return NULL;

	sheep_protect(vm, reducer);
	sheep_protect(vm, list_);

	list = sheep_list(list_);
	if (unpack("reduce", list, "oor!", &a, &b, &list))
		return NULL;

	value = sheep_call(vm, reducer, 2, a, b);
	if (!value)
		goto out;

	while (list->head) {
		value = sheep_call(vm, reducer, 2, value, list->head);
		if (!value)
			goto out;
		list = sheep_list(list->tail);
	}
	result = value;
out:
	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, reducer);

	return result;
}

/* (length sequence) */
static sheep_t eval_length(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned int len;
	sheep_t seq;
	
	if (sheep_unpack_stack("length", vm, nr_args, "q", &seq))
		return NULL;

	len = sheep_sequence(seq)->length(seq);
	return sheep_make_number(vm, len);
}

/* (concat a b) */
static sheep_t eval_concat(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t a, b;

	if (sheep_unpack_stack("concat", vm, nr_args, "qq", &a, &b))
		return NULL;

	if (sheep_type(a) != sheep_type(b)) {
		fprintf(stderr, "concat: can not concat %s and %s\n",
			sheep_type(a)->name, sheep_type(b)->name);
		return NULL;
	}

	return sheep_sequence(a)->concat(vm, a, b);
}

/* (reverse sequence) */
static sheep_t eval_reverse(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("reverse", vm, nr_args, "q", &sheep))
		return NULL;

	return sheep_sequence(sheep)->reverse(vm, sheep);
}

/* (ddump expr) */
static sheep_t eval_ddump(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("ddump", vm, nr_args, "o", &sheep))
		return NULL;

	sheep_ddump(sheep);
	return &sheep_true;
}

/* (disassemble function) */
static sheep_t eval_disassemble(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_function *function;
	unsigned int nr_foreigns;
	sheep_t callable;

	if (sheep_unpack_stack("disassemble", vm, nr_args, "c", &callable))
		return NULL;
	
	if (sheep_type(callable) == &sheep_alien_type) {
		struct sheep_alien *alien;

		alien = sheep_data(callable);
		printf("disassemble: %s is an alien\n", alien->name);
		return &sheep_false;
	}

	function = sheep_data(callable);
	if (function->foreign)
		nr_foreigns = function->foreign->nr_items;
	else
		nr_foreigns = 0;

	__sheep_ddump(callable);
	printf(", %u parameters, %u local slots, "
		"%u foreign references\n", function->nr_parms,
		function->nr_locals, nr_foreigns);

	sheep_code_disassemble(&function->code);
	return &sheep_true;
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

	sheep_module_function(vm, &vm->main, "string", eval_string);
	sheep_module_function(vm, &vm->main, "split", eval_split);
	sheep_module_function(vm, &vm->main, "join", eval_join);

	sheep_module_function(vm, &vm->main, "length", eval_length);
	sheep_module_function(vm, &vm->main, "concat", eval_concat);
	sheep_module_function(vm, &vm->main, "reverse", eval_reverse);
	sheep_module_function(vm, &vm->main, "map", eval_map);
	sheep_module_function(vm, &vm->main, "reduce", eval_reduce);

	sheep_module_function(vm, &vm->main, "ddump", eval_ddump);
	sheep_module_function(vm, &vm->main, "disassemble", eval_disassemble);
}

void sheep_core_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->specials);
}
