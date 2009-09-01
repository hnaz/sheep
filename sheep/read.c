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
#include <sheep/vm.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include <sheep/read.h>

struct sheep_object sheep_eof;

static int next(FILE *fp)
{
	int c, comment = 0;

	do {
		c = fgetc(fp);
		if (c == '#')
			comment = 1;
		if (c == '\n' && comment)
			comment = 0;
	} while (c != EOF && (isspace(c) || comment));
	return c;
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

static int read_token(FILE *fp, char *buf, int len, int string)
{
	int i, c = c;

	for (i = 0; i < len; i++) {
		c = fgetc(fp);
		if (string && c == EOF) {
			fprintf(stderr, "EOF while reading string\n");
			return -1;
		}
		if (issep(c, string))
			break;
		buf[i] = c;
	}
	buf[i] = 0;
	if (!string)
		ungetc(c, fp);
	return 0;
}

static sheep_t read_string(struct sheep_vm *vm, FILE *fp)
{
	char buf[512];

	if (read_token(fp, buf, 512, 1))
		return NULL;
	return sheep_make_string(vm, buf);
}

static int parse_number(const char *buf, long *number)
{
	char *end;
	long val;

	val = strtol(buf, &end, 0);
	if (*end)
		return -1;
	if (val < (LONG_MIN >> 1))
		return -1;
	if (val > (LONG_MAX >> 1))
		return -1;
	*number = val;
	return 0;
}

static sheep_t read_atom(struct sheep_vm *vm, FILE *fp, int c)
{
	char buf[512];
	long number;

	buf[0] = c;
	if (read_token(fp, buf + 1, 511, 0) < 0)
		return NULL;
	if (!parse_number(buf, &number))
		return sheep_make_number(vm, number);
	return sheep_make_name(vm, buf);
}

static sheep_t read_sexp(struct sheep_vm *vm, FILE *fp, int c);

static sheep_t read_list(struct sheep_vm *vm, FILE *fp)
{
	sheep_t list, pos;
	int c;

	list = pos = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, list);

	for (c = next(fp); c != EOF; c = next(fp)) {
		struct sheep_list *node;

		if (c == ')') {
			sheep_unprotect(vm, list);
			return list;
		}

		node = sheep_list(pos);
		node->head = read_sexp(vm, fp, c);
		if (!node->head)
			return NULL;
		if (node->head == &sheep_eof)
			break;
		pos = node->tail = sheep_make_list(vm, NULL, NULL);
	}

	fprintf(stderr, "EOF while reading list\n");
	sheep_unprotect(vm, list);
	return NULL;
}

static sheep_t read_sexp(struct sheep_vm *vm, FILE *fp, int c)
{
	if (c == EOF)
		return &sheep_eof;
	else if (c == '"')
		return read_string(vm, fp);
	else if (c == '(')
		return read_list(vm, fp);
	else if (c == ')') {
		fprintf(stderr, "stray closing paren\n");
		return NULL;
	}
	return read_atom(vm, fp, c);
}

sheep_t sheep_read(struct sheep_vm *vm, FILE *fp)
{
	return read_sexp(vm, fp, next(fp));
}
