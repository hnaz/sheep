/*
 * sheep/read.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/number.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <ctype.h>
#include <stdio.h>

#include <sheep/read.h>

struct sheep_object sheep_eof;

static void barf(struct sheep_reader *reader, const char *msg)
{
	fprintf(stderr, "%s:%lu: %s\n", reader->filename, reader->lineno, msg);
}

static int next(struct sheep_reader *reader, int raw)
{
	int c, comment = 0;

	do {
		c = fgetc(reader->file);
		if (c == '\n') {
			reader->lineno++;
			comment = 0;
		} else if (c == '#')
			comment = 1;
	} while (!raw && c != EOF && (isspace(c) || comment));
	return c;
}

static void prev(struct sheep_reader *reader, int c)
{
	if (ungetc(c, reader->file) == '\n')
		reader->lineno--;
}

static int issep(int c, int string)
{
	if (c == EOF)
		return 1;
	if (string && c == '"')
		return 1;
	if (!string && (c == ')' || c == '(' || isspace(c)))
		return 1;
	return 0;
}

static int read_token(struct sheep_reader *reader,
		      char *buf,
		      int len,
		      int string)
{
	int i, c = c;

	for (i = 0; i < len - 1; i++) {
		c = next(reader, 1);
		if (string && c == EOF) {
			barf(reader, "end of file while reading string");
			return -1;
		}
		if (issep(c, string))
			break;
		buf[i] = c;
	}
	buf[i] = 0;
	if (!string)
		prev(reader, c);
	return 0;
}

static sheep_t read_string(struct sheep_reader *reader, struct sheep_vm *vm)
{
	char buf[512];

	if (read_token(reader, buf, 512, 1))
		return NULL;
	return sheep_make_string(vm, buf);
}

static sheep_t read_atom(struct sheep_reader *reader,
			 struct sheep_vm *vm,
			 int c)
{
	sheep_t sheep;
	char buf[512];

	buf[0] = c;
	if (read_token(reader, buf + 1, 511, 0) < 0)
		return NULL;
	switch (sheep_parse_number(vm, buf, &sheep)) {
	case 0:
		return sheep;
	case -1:
		return sheep_make_name(vm, buf);
	case -2:
		barf(reader, "number literal too large");
	}
	return NULL;
}

static sheep_t read_sexp(struct sheep_reader *reader,
			 struct sheep_vector *lines,
			 struct sheep_vm *vm,
			 int c);

static sheep_t read_list(struct sheep_reader *reader,
			 struct sheep_vector *lines,
			 struct sheep_vm *vm)
{
	sheep_t list, pos;
	int c;

	list = pos = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, list);

	for (c = next(reader, 0); c != EOF; c = next(reader, 0)) {
		struct sheep_list *node;

		if (c == ')') {
			sheep_unprotect(vm, list);
			return list;
		}

		node = sheep_list(pos);
		node->head = read_sexp(reader, lines, vm, c);
		if (!node->head)
			return NULL;
		if (node->head == &sheep_eof)
			break;
		pos = node->tail = sheep_make_cons(vm, NULL, NULL);
	}

	barf(reader, "end of file while reading list");
	sheep_unprotect(vm, list);
	return NULL;
}

static sheep_t read_sexp(struct sheep_reader *reader,
			 struct sheep_vector *lines,
			 struct sheep_vm *vm,
			 int c)
{
	if (c == EOF)
		return &sheep_eof;

	sheep_vector_push(lines, (void *)reader->lineno);

	if (c == '"')
		return read_string(reader, vm);
	else if (c == '(')
		return read_list(reader, lines, vm);
	else if (c == ')') {
		barf(reader, "stray closing parenthesis");
		return NULL;
	}
	return read_atom(reader, vm, c);
}

void sheep_free_expr(struct sheep_expr *expr)
{
	sheep_free(expr->lines.items);
	sheep_free(expr);
}

struct sheep_expr *sheep_read(struct sheep_reader *reader, struct sheep_vm *vm)
{
	struct sheep_expr *expr;

	expr = sheep_zalloc(sizeof(struct sheep_expr));
	expr->filename = reader->filename;
	expr->object = read_sexp(reader, &expr->lines, vm, next(reader, 0));
	if (expr->object)
		return expr;
	sheep_free_expr(expr);
	return NULL;
}
