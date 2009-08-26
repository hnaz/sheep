/*
 * include/sheep/number.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_NUMBER_H
#define _SHEEP_NUMBER_H

#include <sheep/object.h>

extern const struct sheep_type sheep_number_type;

sheep_t sheep_number(struct sheep_vm *, long);

static inline long sheep_cnumber(sheep_t sheep)
{
	return (long)sheep_data(sheep) >> 1;
}

#endif /* _SHEEP_NUMBER_H */
