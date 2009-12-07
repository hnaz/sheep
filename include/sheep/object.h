/*
 * include/sheep/object.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_OBJECT_H
#define _SHEEP_OBJECT_H

#include <sheep/object_types.h>
#include <sheep/number.h>

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

static inline void sheep_set_data(sheep_t sheep, void *data)
{
	sheep->data = (unsigned long)data | (sheep->data & 1);
}

int sheep_test(sheep_t);
int sheep_equal(sheep_t, sheep_t);

#endif /* _SHEEP_OBJECT_H */
