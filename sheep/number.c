/*
 * sheep/number.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/number.h>

static inline double number(sheep_t sheep)
{
	return *(double *)sheep_data(sheep);
}

static void free_number(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_free(sheep_data(sheep));
}

static int test_number(sheep_t sheep)
{
	return !!number(sheep);
}

static int equal_number(sheep_t a, sheep_t b)
{
	return number(a) == number(b);
}

static void ddump_number(sheep_t sheep)
{
	printf("%.14g", number(sheep));
}

const struct sheep_type sheep_number_type = {
	.name = "number",
	.free = free_number,
	.compile = sheep_compile_constant,
	.test = test_number,
	.equal = equal_number,
	.ddump = ddump_number,
};

sheep_t sheep_make_number(struct sheep_vm *vm, double number)
{
	double *valuep;

	valuep = sheep_malloc(sizeof(double));
	*valuep = number;
	return sheep_make_object(vm, &sheep_number_type, valuep);
}
