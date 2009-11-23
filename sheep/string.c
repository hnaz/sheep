/*
 * sheep/string.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

#include <sheep/string.h>

static unsigned long string_length(sheep_t sheep)
{
	return strlen(sheep_rawstring(sheep));
}

static sheep_t string_concat(struct sheep_vm *vm, sheep_t a, sheep_t b)
{
	unsigned long len_a, len;
	const char *sa, *sb;
	char *result;

	sa = sheep_rawstring(a);
	sb = sheep_rawstring(b);

	len_a = strlen(sa);
	len = len_a + strlen(sb);

	result = sheep_malloc(len + 1);
	memcpy(result, sa, len_a);
	strcpy(result + len_a, sb);

	return __sheep_make_string(vm, result);
}

static sheep_t string_reverse(struct sheep_vm *vm, sheep_t sheep)
{
	unsigned long len, pos;
	const char *string;
	char *result;

	string = sheep_rawstring(sheep);
	len = strlen(string);
	result = sheep_malloc(len + 1);

	for (pos = 0; pos < len; pos++)
		result[pos] = string[len - pos - 1];
	result[pos] = 0;

	return __sheep_make_string(vm, result);
}

static const struct sheep_sequence string_sequence = {
	.length = string_length,
	.concat = string_concat,
	.reverse = string_reverse,
};

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

static void format_string(sheep_t sheep, char **bufp, size_t *posp)
{
	sheep_addprintf(bufp, posp, "\"%s\"", sheep_rawstring(sheep));
}

const struct sheep_type sheep_string_type = {
	.name = "string",
	.free = free_string,
	.compile = sheep_compile_constant,
	.test = test_string,
	.equal = equal_string,
	.sequence = &string_sequence,
	.format = format_string,
};

sheep_t __sheep_make_string(struct sheep_vm *vm, const char *str)
{
	return sheep_make_object(vm, &sheep_string_type, (char *)str);
}

sheep_t sheep_make_string(struct sheep_vm *vm, const char *str)
{
	return __sheep_make_string(vm, sheep_strdup(str));
}

void __sheep_format(sheep_t sheep, char **bufp, size_t *posp)
{
	return sheep_type(sheep)->format(sheep, bufp, posp);
}

char *sheep_format(sheep_t sheep)
{
	char *buf = NULL;
	size_t pos = 0;

	__sheep_format(sheep, &buf, &pos);
	return buf;
}
