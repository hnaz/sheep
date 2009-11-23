/*
 * sheep/number.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/vm.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/number.h>

static int test_number(sheep_t sheep)
{
	return !!sheep_fixnum(sheep);
}

static int equal_number(sheep_t a, sheep_t b)
{
	return a == b;
}

static void ddump_number(sheep_t sheep)
{
	printf("%ld", sheep_fixnum(sheep));
}

const struct sheep_type sheep_number_type = {
	.name = "number",
	.compile = sheep_compile_constant,
	.test = test_number,
	.equal = equal_number,
	.ddump = ddump_number,
};

sheep_t sheep_make_number(struct sheep_vm *vm, long number)
{
	return (sheep_t)((number << 1) | 1);
}

int sheep_parse_number(struct sheep_vm *vm, const char *buf, sheep_t *sheepp)
{
	long number, min, max;
	char *end;

	number = strtol(buf, &end, 0);
	if (*end)
		return -1;
	min = LONG_MIN >> 1;
	max = LONG_MAX >> 1;
	if (number < min || number > max)
		return -2;
	*sheepp = sheep_make_number(vm, number);
	return 0;
}
