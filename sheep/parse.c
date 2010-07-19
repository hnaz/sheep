/*
 * sheep/parse.c
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/read.h>
#include <sheep/util.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#include <sheep/parse.h>

void sheep_parser_error(struct sheep_compile *compile,
			sheep_t culprit,
			const char *fmt,
			...)
{
	struct sheep_expr *expr = compile->expr;
	const char *repr;
	size_t position;
	va_list ap;

	if (sheep_type(expr->object) == &sheep_list_type) {
		struct sheep_list *list = sheep_list(expr->object);

		position = 1; /* list itself is at 0 */
		sheep_list_search(list, culprit, &position);
	} else
		position = 0;

	repr = sheep_repr(culprit);
	fprintf(stderr, "%s:%lu: %s: ", expr->filename,
		(unsigned long)expr->lines.items[position],
		repr);
	sheep_free(repr);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
}

enum {
	PARSE_OK,
	PARSE_MISMATCH,
	PARSE_TOO_FEW,
	PARSE_TOO_MANY
};

static unsigned int __parse(struct sheep_list *form,
			    const char *items,
			    sheep_t *mismatchp,
			    va_list ap)
{
	while (*items) {
		struct sheep_name *name;
		struct sheep_list *list;
		sheep_t thing;

		if (!form->head && *items != 'r')
			return PARSE_TOO_FEW;

		if (tolower(*items) == 'r') {
			*va_arg(ap, struct sheep_list **) = form;
			return PARSE_OK;
		}

		thing = form->head;

		switch (*items) {
		case 'e':
			*va_arg(ap, sheep_t *) = thing;
			break;
		case 's':
			if (sheep_type(thing) != &sheep_name_type)
				goto mismatch;
			name = sheep_name(thing);
			if (name->nr_parts != 1)
				goto mismatch;
			*va_arg(ap, const char **) = name->parts[0];
			break;
		case 'n':
			if (sheep_type(thing) != &sheep_name_type)
				goto mismatch;
			name = sheep_name(thing);
			*va_arg(ap, struct sheep_name **) = name;
			break;
		case 'l':
			if (sheep_type(thing) != &sheep_list_type)
				goto mismatch;
			list = sheep_list(thing);
			*va_arg(ap, struct sheep_list **) = list;
			break;
		default:
			sheep_bug("invalid parser instruction");
		}

		form = sheep_list(form->tail);
		items++;
		continue;
	mismatch:
		*mismatchp = thing;
		return PARSE_MISMATCH;
	}

	if (form->head)
		return PARSE_TOO_MANY;

	return PARSE_OK;
}

static int parse(struct sheep_compile *compile,
		 struct sheep_list *form,
		 struct sheep_list *subform,
		 const char *items,
		 va_list ap)
{
	unsigned int ret;
	sheep_t mismatch;

	ret = __parse(subform, items, &mismatch, ap);
	if (ret == PARSE_OK)
		return 0;

	if (ret == PARSE_MISMATCH)
		sheep_parser_error(compile, mismatch, "unexpected expression");
	else
		sheep_parser_error(compile, form->head, "too %s arguments",
				ret == PARSE_TOO_MANY ? "many" : "few");

	return -1;
}

int __sheep_parse(struct sheep_compile *compile,
		  struct sheep_list *form,
		  struct sheep_list *subform,
		  const char *items,
		  ...)
{
	va_list ap;
	int ret;

	va_start(ap, items);
	ret = parse(compile, form, subform, items, ap);
	va_end(ap);
	return ret;
}

int sheep_parse(struct sheep_compile *compile,
		struct sheep_list *form,
		const char *items,
		...)
{
	va_list ap;
	int ret;

	va_start(ap, items);
	ret = parse(compile, form, sheep_list(form->tail), items, ap);
	va_end(ap);
	return ret;
}
