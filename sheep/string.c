/*
 * sheep/string.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/unpack.h>
#include <sheep/util.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

#include <sheep/string.h>

static void string_free(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_string *string;

	string = sheep_string(sheep);
	sheep_free(string->bytes);
	sheep_free(string);
}

static int string_test(sheep_t sheep)
{
	struct sheep_string *string;

	string = sheep_string(sheep);
	return string->nr_bytes != 0;
}

static int string_equal(sheep_t a, sheep_t b)
{
	struct sheep_string *sa, *sb;

	sa = sheep_string(a);
	sb = sheep_string(b);
	if (sa->nr_bytes != sb->nr_bytes)
		return 0;
	return !memcmp(sa->bytes, sb->bytes, sa->nr_bytes);
}

static void string_format(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	const char *fmt = repr ? "\"%s\"" : "%s";

	sheep_addprintf(bufp, posp, fmt, sheep_rawstring(sheep));
}

static size_t string_length(sheep_t sheep)
{
	struct sheep_string *string;

	string = sheep_string(sheep);
	return string->nr_bytes;
}

static sheep_t string_concat(struct sheep_vm *vm, sheep_t a, sheep_t b)
{
	struct sheep_string *sa, *sb;
	char *result;
	size_t len;

	sa = sheep_string(a);
	sb = sheep_string(b);
	len = sa->nr_bytes + sb->nr_bytes;
	result = sheep_malloc(len + 1);
	memcpy(result, sa->bytes, sa->nr_bytes);
	memcpy(result + sa->nr_bytes, sb->bytes, sb->nr_bytes);
	result[len] = 0;
	return __sheep_make_string(vm, result, len);
}

static sheep_t string_reverse(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_string *string;
	char *result;
	size_t pos;

	string = sheep_string(sheep);
	result = sheep_malloc(string->nr_bytes + 1);
	for (pos = 0; pos < string->nr_bytes; pos++)
		result[pos] = string->bytes[string->nr_bytes - pos - 1];
	result[pos] = 0;
	return __sheep_make_string(vm, result, pos);
}

static sheep_t string_nth(struct sheep_vm *vm, size_t n, sheep_t sheep)
{
	struct sheep_string *string;
	char *new;

	string = sheep_string(sheep);
	new = sheep_zalloc(1 + 1);
	if (string->nr_bytes > n)
		new[0] = string->bytes[n];
	return __sheep_make_string(vm, new, 1);
}

static sheep_t string_position(struct sheep_vm *vm, sheep_t item, sheep_t sheep)
{
	const char *str, *pos;

	if (sheep_type(item) != &sheep_string_type) {
		fprintf(stderr, "position: string can not contain %s\n",
			sheep_type(item)->name);
		return NULL;
	}

	str = sheep_rawstring(sheep);
	pos = strstr(str, sheep_rawstring(item));
	if (pos)
		return sheep_make_number(vm, pos - str);
	return &sheep_nil;
}

static const struct sheep_sequence string_sequence = {
	.length = string_length,
	.concat = string_concat,
	.reverse = string_reverse,
	.nth = string_nth,
	.position = string_position,
};

const struct sheep_type sheep_string_type = {
	.name = "string",
	.free = string_free,
	.compile = sheep_compile_constant,
	.test = string_test,
	.equal = string_equal,
	.format = string_format,
	.sequence = &string_sequence,
};

sheep_t __sheep_make_string(struct sheep_vm *vm, const char *str, size_t len)
{
	struct sheep_string *string;

	string = sheep_malloc(sizeof(struct sheep_string));
	string->bytes = str;
	string->nr_bytes = len;
	return sheep_make_object(vm, &sheep_string_type, string);
}

sheep_t sheep_make_string(struct sheep_vm *vm, const char *str)
{
	return __sheep_make_string(vm, sheep_strdup(str), strlen(str));
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
	return __sheep_make_string(vm, buf, strlen(buf));
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

/* (split delimiter string) */
static sheep_t builtin_split(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t string_, delim_, list_;
	struct sheep_list *list;
	const char *delim;
	char *pos, *orig;
	int empty;

	if (sheep_unpack_stack("split", vm, nr_args, "ss", &delim_, &string_))
		return NULL;

	sheep_protect(vm, delim_);
	pos = orig = sheep_strdup(sheep_rawstring(string_));

	list_ = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, list_);

	delim = sheep_rawstring(delim_);
	empty = sheep_string(delim_)->nr_bytes == 0;

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
			item = sheep_make_string(vm, do_split(&pos, delim));

		list->head = item;
		list->tail = sheep_make_cons(vm, NULL, NULL);
		list = sheep_list(list->tail);
	}
	sheep_free(orig);

	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, delim_);

	return list_;
}

/* (join delimiter list-of-strings) */
static sheep_t builtin_join(struct sheep_vm *vm, unsigned int nr_args)
{
	char *new = NULL, *result = NULL;
	struct sheep_string *delim;
	struct sheep_list *list;
	sheep_t delim_, list_;
	size_t length = 0;

	if (sheep_unpack_stack("join", vm, nr_args, "sl", &delim_, &list_))
		return NULL;

	sheep_protect(vm, delim_);
	sheep_protect(vm, list_);

	delim = sheep_string(delim_);
	list = sheep_list(list_);

	while (list->head) {
		struct sheep_string *string;
		size_t newlength;

		if (sheep_unpack_list("join", list, "Sr", &string, &list))
			goto out;

		newlength = length + string->nr_bytes;
		if (list->head)
			newlength += delim->nr_bytes;
		new = sheep_realloc(new, newlength + 1);
		strcpy(new + length, string->bytes);
		if (list->head)
			strcpy(new + newlength - delim->nr_bytes, delim->bytes);
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
		return __sheep_make_string(vm, result, length);
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
