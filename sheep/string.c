/*
 * sheep/string.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/unpack.h>
#include <sheep/bool.h> /* &sheep_true... fix me */
#include <sheep/util.h>
#include <sheep/gc.h>
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

static sheep_t string_nth(struct sheep_vm *vm, unsigned long n, sheep_t sheep)
{
	char new[2] = { 0, 0 };
	const char *string;

	string = sheep_rawstring(sheep);
	if (strlen(string) > n)
		new[0] = string[n];
	return sheep_make_string(vm, new);
}

static const struct sheep_sequence string_sequence = {
	.length = string_length,
	.concat = string_concat,
	.reverse = string_reverse,
	.nth = string_nth,
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

static void format_string(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	const char *fmt = repr ? "\"%s\"" : "%s";

	sheep_addprintf(bufp, posp, fmt, sheep_rawstring(sheep));
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

void __sheep_format(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	sheep_type(sheep)->format(sheep, bufp, posp, repr);
}

char *sheep_format(sheep_t sheep)
{
	char *buf = NULL;
	size_t pos = 0;

	__sheep_format(sheep, &buf, &pos, 0);
	return buf;
}

char *sheep_repr(sheep_t sheep)
{
	char *buf = NULL;
	size_t pos = 0;

	__sheep_format(sheep, &buf, &pos, 1);
	return buf;
}

/* (string expression) */
static sheep_t builtin_string(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;
	char *buf;

	if (sheep_unpack_stack("string", vm, nr_args, "o", &sheep))
		return NULL;

	if (sheep_type(sheep) == &sheep_string_type)
		return sheep;

	buf = sheep_repr(sheep);
	return __sheep_make_string(vm, buf);
}

static char *do_split(char **stringp, const char *delim)
{
	char *match, *result;

	match = strstr(*stringp, delim);
	result = *stringp;
	if (!match) {
		*stringp = NULL;
		return result;
	}

	*match = 0;
	*stringp = match + strlen(delim);
	return result;
}

/* (split string string) */
static sheep_t builtin_split(struct sheep_vm *vm, unsigned int nr_args)
{
	const char *string, *token;
	struct sheep_list *list;
	sheep_t list_, token_;
	char *pos, *orig;
	int empty;

	if (sheep_unpack_stack("split", vm, nr_args, "sS", &token_, &string))
		return NULL;
	sheep_protect(vm, token_);
	pos = orig = sheep_strdup(string);

	list_ = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, list_);

	token = sheep_rawstring(token_);
	empty = !strlen(token);

	list = sheep_list(list_);
	while (pos) {
		sheep_t item;
		/*
		 * Empty splitting separates out every single
		 * character: (split "" "foo") => ("f" "o" "o")
		 */
		if (empty) {
			char str[2] = { *pos, 0 };

			item = sheep_make_string(vm, str);
			if (!pos[0] || !pos[1])
				pos = NULL;
			else
				pos++;
		} else
			item = sheep_make_string(vm, do_split(&pos, token));

		list->head = item;
		list->tail = sheep_make_list(vm, NULL, NULL);
		list = sheep_list(list->tail);
	}
	sheep_free(orig);

	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, token_);

	return list_;
}

/* (join delimiter list-of-strings) */
static sheep_t builtin_join(struct sheep_vm *vm, unsigned int nr_args)
{
	char *new = NULL, *result = NULL;
	size_t length = 0, dlength;
	struct sheep_list *list;
	sheep_t delim_, list_;
	const char *delim;

	if (sheep_unpack_stack("join", vm, nr_args, "sl", &delim_, &list_))
		return NULL;

	sheep_protect(vm, delim_);
	sheep_protect(vm, list_);

	delim = sheep_rawstring(delim_);
	list = sheep_list(list_);

	dlength = strlen(delim);

	while (list->head) {
		const char *string;
		size_t newlength;

		if (sheep_unpack_list("join", list, "Sr", &string, &list))
			goto out;

		newlength = length + strlen(string);
		if (list->head)
			newlength += dlength;
		new = sheep_realloc(new, newlength + 1);
		strcpy(new + length, string);
		if (list->head)
			strcpy(new + newlength - dlength, delim);
		length = newlength;
	}

	if (new)
		result = new;
	else
		result = sheep_strdup("");
out:
	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, delim_);

	if (result)
		return __sheep_make_string(vm, result);
	return NULL;
}

/* (print &rest objects) */
static sheep_t builtin_print(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned int offset = nr_args;

	while (offset) {
		unsigned long index;
		char *tmp;

		index = vm->stack.nr_items - offset;
		tmp = sheep_format(vm->stack.items[index]);
		printf("%s", tmp);
		sheep_free(tmp);
		offset--;
	}
	puts("");
	vm->stack.nr_items -= nr_args;

	return &sheep_nil;
}

void sheep_string_builtins(struct sheep_vm *vm)
{
	sheep_vm_function(vm, "string", builtin_string);
	sheep_vm_function(vm, "split", builtin_split);
	sheep_vm_function(vm, "join", builtin_join);
	sheep_vm_function(vm, "print", builtin_print);
}
