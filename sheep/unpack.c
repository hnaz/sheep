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

static const char *unpack(int control, sheep_t object,
			void **itemp, void **nextp)
{
	const struct sheep_type *type = sheep_type(object);
	int want;

	want = tolower(control);
	switch (want) {
	case 'o':
		break;
	case 'b':
		if (type == &sheep_bool_type)
			break;
		return "bool";
	case 'n':
		if (type == &sheep_number_type)
			break;
		return "number";
	case 'a':
		if (type == &sheep_name_type)
			break;
		return "name";
	case 's':
		if (type == &sheep_string_type)
			break;
		return "string";
	case 'l':
		if (type == &sheep_list_type)
			break;
		return "list";
	case 'q':
		if (type == &sheep_string_type)
			break;
		if (type == &sheep_list_type)
			break;
		return "sequence";
	case 'f':
		if (type == &sheep_function_type)
			break;
		if (type == &sheep_closure_type)
			break;
		return "function";
	case 'c':
		if (type == &sheep_alien_type)
			break;
		if (type == &sheep_function_type)
			break;
		if (type == &sheep_closure_type)
			break;
		return "callable";
	case 't': {
		struct sheep_type *custom;

		custom = (struct sheep_type *)itemp;
		if (type != custom)
			return custom->name;
		itemp = nextp;
		break;
	}
	}

	if (want == control)
		*itemp = object;
	else {
		if (sheep_is_fixnum(object))
			*itemp = (void *)sheep_fixnum(object);
		else
			*itemp = sheep_data(object);
	}

	return NULL;
}

int sheep_unpack_list(const char *caller, struct sheep_list *list,
		const char *items, ...)
{
	va_list ap;

	va_start(ap, items);
	while (*items) {
		void **itemp, *next;
		const char *wanted;

		if (*items == 'r') {
			if (items[1] == '!' && !list->head)
				break;
			*va_arg(ap, struct sheep_list **) = list;
			return 0;
		}

		if (!list->head)
			break;

		itemp = va_arg(ap, void **);
		next = NULL;

		wanted = unpack(*items, list->head, itemp, &next);
		if (wanted) {
			fprintf(stderr, "%s: expected %s, got %s\n",
				caller, wanted, sheep_type(list->head)->name);
			va_end(ap);
			return -1;
		}

		if (next)
			*va_arg(ap, void **) = next;

		items++;
		list = sheep_list(list->tail);
	}
	va_end(ap);

	if (!*items && !list->head)
		return 0;

	fprintf(stderr, "%s: too %s arguments\n",
		caller, !!*items - !!list->head > 0 ? "few" : "many");
	return -1;
}

int sheep_unpack_stack(const char *caller, struct sheep_vm *vm,
		unsigned int nr_args, const char *items, ...)
{
	unsigned long base, index;
	va_list ap;

	if (strlen(items) != nr_args) {
		fprintf(stderr, "%s: too %s arguments\n", caller,
			strlen(items) > nr_args ? "few" : "many");
		return -1;
	}

	base = vm->stack.nr_items - strlen(items);
	index = base;

	va_start(ap, items);
	while (*items) {
		sheep_t object = vm->stack.items[index];
		void **itemp, *next;
		const char *wanted;

		itemp = va_arg(ap, void **);
		next = NULL;

		wanted = unpack(*items, object, itemp, &next);
		if (wanted) {
			fprintf(stderr, "%s: expected %s, got %s\n",
				caller, wanted, sheep_type(object)->name);
			va_end(ap);
			return -1;
		}

		if (next)
			*va_arg(ap, void **) = next;

		items++;
		index++;
	}
	va_end(ap);

	vm->stack.nr_items = base;
	return 0;
}
