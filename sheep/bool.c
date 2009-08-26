#include <sheep/compile.h>
#include <sheep/object.h>
#include <stdio.h>

#include <sheep/bool.h>

static int test_bool(sheep_t sheep)
{
	return sheep != &sheep_false;
}

static void ddump_bool(sheep_t sheep)
{
	if (sheep == &sheep_false)
		printf("false");
	else
		printf("true");
}

const struct sheep_type sheep_bool_type = {
	.name = "bool",
	.compile = sheep_compile_constant,
	.test = test_bool,
	.ddump = ddump_bool,
};

struct sheep_object sheep_true = {
	.type = &sheep_bool_type,
};

struct sheep_object sheep_false = {
	.type = &sheep_bool_type,
};
