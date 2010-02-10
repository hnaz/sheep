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

static void free_function(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->foreign)
		free_freevar(function->foreign);
	sheep_code_exit(&function->code);
	sheep_free(function->name);
	sheep_free(function);
}

static void format_function(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->name)
		sheep_addprintf(bufp, posp, "#<function '%s'>", function->name);
	else
		sheep_addprintf(bufp, posp, "#<function '%p'>", function);
}

const struct sheep_type sheep_function_type = {
	.name = "function",
	.free = free_function,
	.format = format_function,
};

static void mark_closure(sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	sheep_foreign_mark(closure->foreign);
}

static void free_closure(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	sheep_foreign_release(vm, closure->foreign);
	sheep_free(closure->name);
	sheep_free(closure);
}

const struct sheep_type sheep_closure_type = {
	.name = "function",
	.mark = mark_closure,
	.free = free_closure,
	.format = format_function,
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

	if (sheep_unpack_stack("disassemble", vm, nr_args, "F", &function))
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
