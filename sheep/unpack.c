/*
 * sheep/unpack.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/number.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/vector.h>
#include <sheep/alien.h>
#include <sheep/bool.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include <sheep/unpack.h>

static int verify(char control, sheep_t object)
{
	const struct sheep_type *type = sheep_type(object);

	switch (control) {
	case 'o':
		return 1;
	case 'b':
		return type == &sheep_bool_type;
	case 'n':
		return type == &sheep_number_type;
	case 'a':
		return type == &sheep_name_type;
	case 's':
		return type == &sheep_string_type;
	case 'l':
		return type == &sheep_list_type;
	case 'q':
		return type == &sheep_string_type ||
			type == &sheep_list_type;
	case 'f':
		return type == &sheep_function_type ||
			type == &sheep_closure_type;
	case 'c':
		return type == &sheep_alien_type ||
			type == &sheep_function_type ||
			type == &sheep_closure_type;
	}
	sheep_bug("unexpected unpack control %c", control);
}

static const char *map_type(char type)
{
	static const char *controlnames[] = {
		"bbool", "nnumber", "aname",
		"sstring", "llist", "qsequence",
		"ffunction", "ccallable", NULL
	};
	const char **p;

	for (p = controlnames; *p; p++)
		if (*p[0] == type)
			return p[0] + 1;

	sheep_bug("incomplete controlnames table");
}

enum sheep_unpack __sheep_unpack_list(const char **wanted, sheep_t *mismatch,
				struct sheep_list *list, const char *items,
				va_list ap)
{
	while (*items) {
		sheep_t object;
		char type;

		if (*items == 'r') {
			if (items[1] == '!' && !list->head)
				break;
			*va_arg(ap, struct sheep_list **) = list;
			return SHEEP_UNPACK_OK;
		}

		if (!list->head)
			break;

		type = tolower(*items);
		object = list->head;

		if (!verify(type, object)) {
			*wanted = map_type(type);
			*mismatch = object;
			return SHEEP_UNPACK_MISMATCH;
		}

		if (isupper(*items)) {
			if (sheep_is_fixnum(object))
				*va_arg(ap, long *) = sheep_fixnum(object);
			else
				*va_arg(ap, void **) = sheep_data(object);
		} else
			*va_arg(ap, sheep_t *) = object;

		items++;
		list = sheep_list(list->tail);
	}

	if (*items)
		return SHEEP_UNPACK_TOO_MANY;
	else if (list->head)
		return SHEEP_UNPACK_TOO_FEW;
	return SHEEP_UNPACK_OK;
}

int sheep_unpack_list(const char *caller, struct sheep_list *list,
		const char *items, ...)
{
	enum sheep_unpack status;
	const char *wanted;
	sheep_t mismatch;
	va_list ap;

	va_start(ap, items);
	status = __sheep_unpack_list(&wanted, &mismatch, list, items, ap);
	va_end(ap);

	switch (status) {
	case SHEEP_UNPACK_OK:
		return 0;
	case SHEEP_UNPACK_MISMATCH:
		fprintf(stderr, "%s: expected %s, got %s\n",
			caller, wanted, sheep_type(mismatch)->name);
		return -1;
	case SHEEP_UNPACK_TOO_MANY:
		fprintf(stderr, "%s: too few arguments\n", caller);
		return -1;
	case SHEEP_UNPACK_TOO_FEW:
		fprintf(stderr, "%s: too many arguments\n", caller);
	default: /* weird compiler... */
		return -1;
	}
}

enum sheep_unpack __sheep_unpack_stack(const char **wanted, sheep_t *mismatch,
				struct sheep_vector *stack,
				const char *items, va_list ap)
{
	unsigned long base = stack->nr_items - strlen(items);
	unsigned long index = base;

	while (*items) {
		sheep_t object;
		char type;

		if (index == stack->nr_items)
			break;

		type = tolower(*items);
		object = stack->items[index];

		if (!verify(type, object)) {
			*wanted = map_type(type);
			*mismatch = object;
			return SHEEP_UNPACK_MISMATCH;
		}

		if (isupper(*items)) {
			if (sheep_is_fixnum(object))
				*va_arg(ap, long *) = sheep_fixnum(object);
			else
				*va_arg(ap, void **) = sheep_data(object);
		} else
			*va_arg(ap, sheep_t *) = object;

		items++;
		index++;
	}

	if (*items)
		return SHEEP_UNPACK_TOO_MANY;

	stack->nr_items = base;
	return SHEEP_UNPACK_OK;
}

int sheep_unpack_stack(const char *caller, struct sheep_vm *vm,
		unsigned int nr_args, const char *items, ...)
{
	enum sheep_unpack status;
	const char *wanted;
	sheep_t mismatch;
	va_list ap;

	if (strlen(items) != nr_args) {
		fprintf(stderr, "%s: too %s arguments\n", caller,
			strlen(items) > nr_args ? "few" : "many");
		return -1;
	}

	va_start(ap, items);
	status = __sheep_unpack_stack(&wanted, &mismatch, &vm->stack, items,ap);
	va_end(ap);

	switch (status) {
	case SHEEP_UNPACK_OK:
		return 0;
	case SHEEP_UNPACK_MISMATCH:
		fprintf(stderr, "%s: expected %s, got %s\n",
			caller, wanted, sheep_type(mismatch)->name);
		return -1;
	default: /* should not happen, nr_args is trustworthy */
		return -1;
	}
}
