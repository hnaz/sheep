/*
 * sheep/number.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/number.h>

static int test_number(sheep_t sheep)
{
	return !!sheep_cnumber(sheep);
}

static int equal_number(sheep_t a, sheep_t b)
{
	return sheep_cnumber(a) == sheep_cnumber(b);
}

static void ddump_number(sheep_t sheep)
{
	printf("%ld", sheep_cnumber(sheep));
}

const struct sheep_type sheep_number_type = {
	.name = "number",
	.compile = sheep_compile_constant,
	.test = test_number,
	.equal = equal_number,
	.ddump = ddump_number,
};

sheep_t sheep_number(struct sheep_vm *vm, long cnumber)
{
	return sheep_object(vm, &sheep_number_type,
			(void *)(cnumber << 1));
}
