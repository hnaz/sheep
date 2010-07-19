/*
 * sheep/function.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/foreign.h>
#include <sheep/object.h>
#include <sheep/unpack.h>
#include <sheep/bool.h>
#include <sheep/code.h>
#include <sheep/util.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/function.h>

static void free_freevar(struct sheep_vector *foreign)
{
	unsigned int i;

	for (i = 0; i < foreign->nr_items; i++)
		sheep_free(foreign->items[i]);
	sheep_free(foreign->items);
	sheep_free(foreign);
}

static void function_free(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->foreign)
		free_freevar(function->foreign);
	sheep_code_exit(&function->code);
	sheep_free(function->name);
	sheep_free(function);
}

static enum sheep_call function_call(struct sheep_vm *vm,
				     sheep_t callable,
				     unsigned int nr_args,
				     sheep_t *valuep)
{
	struct sheep_function *function;

	function = sheep_function(callable);
	if (function->nr_parms != nr_args) {
		sheep_error(vm, "too %s arguments",
			function->nr_parms < nr_args ? "many" : "few");
		return SHEEP_CALL_FAIL;
	}
	return SHEEP_CALL_EVAL;
}

static void function_format(sheep_t sheep, struct sheep_strbuf *sb, int repr)
{
	struct sheep_function *function;

	function = sheep_function(sheep);
	if (repr) {
		if (function->name)
			sheep_strbuf_addf(sb, "#<function '%s'>", function->name);
		else
			sheep_strbuf_addf(sb, "#<function '%p'>", function);
	} else {
		if (function->name)
			sheep_strbuf_add(sb, function->name);
		else
			sheep_strbuf_add(sb, "<anonymous function>");
	}
}

const struct sheep_type sheep_function_type = {
	.name = "function",
	.free = function_free,
	.call = function_call,
	.format = function_format,
};

static void closure_mark(sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	sheep_foreign_mark(closure->foreign);
}

static void closure_free(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	sheep_foreign_release(vm, closure->foreign);
	sheep_free(closure->name);
	sheep_free(closure);
}

const struct sheep_type sheep_closure_type = {
	.name = "function",
	.mark = closure_mark,
	.free = closure_free,
	.call = function_call,
	.format = function_format,
};

sheep_t sheep_make_function(struct sheep_vm *vm, const char *name)
{
	struct sheep_function *function;

	function = sheep_zalloc(sizeof(struct sheep_function));
	if (name)
		function->name = sheep_strdup(name);
	return sheep_make_object(vm, &sheep_function_type, function);
}

sheep_t sheep_closure_function(struct sheep_vm *vm,
			       struct sheep_function *function)
{
	struct sheep_function *closure;

	closure = sheep_malloc(sizeof(struct sheep_function));
	*closure = *function;
	if (function->name)
		closure->name = sheep_strdup(function->name);
	return sheep_make_object(vm, &sheep_closure_type, closure);
}

/* (disassemble function) */
static sheep_t builtin_disassemble(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_function *function;
	unsigned int nr_foreigns;

	if (sheep_unpack_stack(vm, nr_args, "F", &function))
		return NULL;

	if (function->foreign)
		nr_foreigns = function->foreign->nr_items;
	else
		nr_foreigns = 0;

	printf("%u parameters, %u local slots, %u foreign references\n",
		function->nr_parms, function->nr_locals, nr_foreigns);

	sheep_code_disassemble(&function->code);
	return &sheep_nil;
}

void sheep_function_builtins(struct sheep_vm *vm)
{
	sheep_vm_function(vm, "disassemble", builtin_disassemble);
}
