/*
 * sheep/function.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/unit.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/function.h>

static void free_function(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->unit.foreigns)
		sheep_unit_free_foreigns(vm, &function->unit);
	sheep_code_exit(&function->unit.code);
	sheep_free(function->name);
	sheep_free(function);
}

static void ddump_function(sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->name)
		printf("#<function '%s'>", function->name);
	else
		printf("#<function '%p'>", function);
}

const struct sheep_type sheep_function_type = {
	.name = "function",
	.free = free_function,
	.ddump = ddump_function,
};

static void mark_closure(sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	sheep_unit_mark_foreigns(&closure->unit);
}


static void free_closure(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	sheep_unit_free_foreigns(vm, &closure->unit);
	sheep_free(closure->name);
	sheep_free(closure);
}

const struct sheep_type sheep_closure_type = {
	.name = "function",
	.mark = mark_closure,
	.free = free_closure,
	.ddump = ddump_function,
};

sheep_t sheep_make_function(struct sheep_vm *vm, const char *name)
{
	struct sheep_function *function;

	function = sheep_zalloc(sizeof(struct sheep_function));
	sheep_code_init(&function->unit.code);
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
