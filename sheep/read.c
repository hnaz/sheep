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
	return sheep_string(vm, buf);
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

	ungetc(c, fp);
	if (read_token(fp, buf, 512, 0) < 0)
		return NULL;
	if (!parse_number(buf, &number))
		return sheep_number(vm, number);
	return sheep_name(vm, buf);
}

static sheep_t read_sexp(struct sheep_vm *vm, FILE *fp, int c);

static sheep_t read_list(struct sheep_vm *vm, FILE *fp)
{
	struct sheep_list *list = NULL, *pos;
	int c;

	for (c = next(fp); c != EOF; c = next(fp)) {
		sheep_t item;

		if (c == ')') {
			if (list)
				return sheep_object(vm, &sheep_list_type, list);
			return sheep_list(vm, 0);
		}

		item = read_sexp(vm, fp, c);
		if (!item)
			return NULL;
		if (item == &sheep_eof)
			break;

		if (!list)
			list = pos = __sheep_list(vm, item);
		else
			pos = pos->tail = __sheep_list(vm, item);
	}

	fprintf(stderr, "EOF while reading list\n");
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
