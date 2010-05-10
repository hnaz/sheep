/*
 * include/sheep/string.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_STRING_H
#define _SHEEP_STRING_H

#include <sheep/object.h>
#include <sheep/util.h>
#include <stdlib.h>

struct sheep_vm;

struct sheep_string {
	const char *bytes;
	size_t nr_bytes;
};

extern const struct sheep_type sheep_string_type;

sheep_t __sheep_make_string(struct sheep_vm *, const char *, size_t);
sheep_t sheep_make_string(struct sheep_vm *, const char *);

static inline struct sheep_string *sheep_string(sheep_t sheep)
{
	return sheep_data(sheep);
}

static inline const char *sheep_rawstring(sheep_t sheep)
{
	return sheep_string(sheep)->bytes;
}

void __sheep_format(sheep_t, struct sheep_strbuf *, int);
char *sheep_format(sheep_t);
char *sheep_repr(sheep_t);

void sheep_string_builtins(struct sheep_vm *);

#endif /* _SHEEP_STRING_H */
