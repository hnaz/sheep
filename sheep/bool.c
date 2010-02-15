/*
 * sheep/bool.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/unpack.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/bool.h>

static int bool_test(sheep_t sheep)
{
	return sheep != &sheep_false;
}

static void bool_format(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	if (sheep == &sheep_false)
		sheep_addprintf(bufp, posp, "false");
	else
		sheep_addprintf(bufp, posp, "true");
}

const struct sheep_type sheep_bool_type = {
	.name = "bool",
	.compile = sheep_compile_constant,
	.test = bool_test,
	.format = bool_format,
};

struct sheep_object sheep_true = {
	.type = &sheep_bool_type,
};

struct sheep_object sheep_false = {
	.type = &sheep_bool_type,
};

/* (= a b) */
static sheep_t builtin_equal(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t a, b;

	if (sheep_unpack_stack("=", vm, nr_args, "oo", &a, &b))
		return NULL;

	if (sheep_equal(a, b))
		return &sheep_true;
	return &sheep_false;
}

/* (bool object) */
static sheep_t builtin_bool(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("bool", vm, nr_args, "o", &sheep))
		return NULL;

	if (sheep_test(sheep))
		return &sheep_true;
	return &sheep_false;
}

/* (not object) */
static sheep_t builtin_not(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("not", vm, nr_args, "o", &sheep))
		return NULL;

	if (sheep_test(sheep))
		return &sheep_false;
	return &sheep_true;
}

void sheep_bool_builtins(struct sheep_vm *vm)
{
	sheep_vm_variable(vm, "true", &sheep_true);
	sheep_vm_variable(vm, "false", &sheep_false);

	sheep_vm_function(vm, "=", builtin_equal);
	sheep_vm_function(vm, "bool", builtin_bool);
	sheep_vm_function(vm, "not", builtin_not);
}
