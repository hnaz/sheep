#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

#include <sheep/string.h>

static void free_string(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_free(sheep_data(sheep));
}

static int test_string(sheep_t sheep)
{
	return strlen(sheep_rawstring(sheep)) > 0;
}

static int equal_string(sheep_t a, sheep_t b)
{
	return !strcmp(sheep_rawstring(a), sheep_rawstring(b));
}

static void ddump_string(sheep_t sheep)
{
	printf("\"%s\"", sheep_rawstring(sheep));
}

const struct sheep_type sheep_string_type = {
	.name = "string",
	.free = free_string,
	.compile = sheep_compile_constant,
	.test = test_string,
	.equal = equal_string,
	.ddump = ddump_string,
};

sheep_t sheep_string(struct sheep_vm *vm, const char *str)
{
	return sheep_object(vm, &sheep_string_type, sheep_strdup(str));
}
