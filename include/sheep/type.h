/*
 * include/sheep/type.h
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_TYPE_H
#define _SHEEP_TYPE_H

#include <sheep/object.h>
#include <sheep/map.h>

struct sheep_vm;

struct sheep_typeobject {
	sheep_t class;
	sheep_t *values;
	struct sheep_map map;
};

extern const struct sheep_type sheep_typeobject_type;

struct sheep_typeclass {
	const char *name;
	const char **names;
	unsigned int nr_slots;
};

sheep_t sheep_make_typeclass(struct sheep_vm *, const char *,
			const char **, unsigned int);

#endif /* _SHEEP_TYPE_H */
