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
#include <stdarg.h>
#include <ctype.h>

#include <sheep/unpack.h>

static int verify(char control, sheep_t object)
{
	switch (control) {
	case 'o':
		return 1;
	case 'b':
		return object->type == &sheep_bool_type;
	case 'n':
		return object->type == &sheep_number_type;
	case 'a':
		return object->type == &sheep_name_type;
	case 's':
		return object->type == &sheep_string_type;
	case 'l':
		return object->type == &sheep_list_type;
	case 'q':
		return object->type == &sheep_string_type ||
			object->type == &sheep_list_type;
	case 'c':
		return object->type == &sheep_alien_type ||
			object->type == &sheep_function_type ||
			object->type == &sheep_closure_type;
	}
	sheep_bug("unexpected unpack control %c", control);
}

static const char *map_type(char type)
{
	static const char *controlnames[] = {
		"bbool", "nnumber", "aname", "sstring",
		"llist", "qsequence", "ccallable", NULL
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

		if (isupper(*items))
			*va_arg(ap, void **) = sheep_data(object);
		else
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

enum sheep_unpack __sheep_unpack_stack(const char **wanted, sheep_t *mismatch,
				struct sheep_vector *stack,
				const char *items, va_list ap)
{
	unsigned long top = stack->nr_items;

	while (*items) {
		sheep_t object;
		char type;

		if (!top)
			break;

		type = tolower(*items);
		object = stack->items[top - 1];

		if (!verify(type, object)) {
			*wanted = map_type(type);
			*mismatch = object;
			return SHEEP_UNPACK_MISMATCH;
		}

		if (isupper(*items))
			*va_arg(ap, void **) = sheep_data(object);
		else
			*va_arg(ap, sheep_t *) = object;

		items++;
		top--;
	}

	if (*items)
		return SHEEP_UNPACK_TOO_MANY;

	stack->nr_items = top;
	return SHEEP_UNPACK_OK;
}
