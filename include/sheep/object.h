/*
 * include/sheep/object.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_OBJECT_H
#define _SHEEP_OBJECT_H

#include <sheep/number.h>
#include <sheep/types.h>

sheep_t sheep_make_object(struct sheep_vm *, const struct sheep_type *, void *);

static inline const struct sheep_type *sheep_type(sheep_t sheep)
{
	if (sheep_is_fixnum(sheep))
		return &sheep_number_type;
	return sheep->type;
}

static inline void *sheep_data(sheep_t sheep)
{
	return (void *)(sheep->data & ~1);
}

int sheep_test(sheep_t);
int sheep_equal(sheep_t, sheep_t);

extern struct sheep_object sheep_nil;

void sheep_object_builtins(struct sheep_vm *);

#endif /* _SHEEP_OBJECT_H */
