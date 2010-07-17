/*
 * include/sheep/number.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_NUMBER_H
#define _SHEEP_NUMBER_H

#include <sheep/types.h>

struct sheep_vm;

static inline int sheep_is_fixnum(sheep_t sheep)
{
	return (long)sheep & 1;
}

static inline long sheep_fixnum(sheep_t sheep)
{
	return (long)sheep >> 1;
}

extern const struct sheep_type sheep_number_type;

sheep_t sheep_make_number(struct sheep_vm *, long);
int sheep_parse_number(struct sheep_vm *, const char *, sheep_t *);

void sheep_number_builtins(struct sheep_vm *);

#endif /* _SHEEP_NUMBER_H */
