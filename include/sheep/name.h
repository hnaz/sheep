/*
 * include/sheep/name.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_NAME_H
#define _SHEEP_NAME_H

#include <sheep/object.h>

struct sheep_name {
	const char **parts;
	unsigned int nr_parts;
};

extern const struct sheep_type sheep_name_type;

sheep_t sheep_make_name(struct sheep_vm *, const char *);

static inline struct sheep_name *sheep_name(sheep_t sheep)
{
	return sheep_data(sheep);
}

#endif /* _SHEEP_NAME_H */
