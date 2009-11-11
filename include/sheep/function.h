/*
 * include/sheep/function.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_FUNCTION_H
#define _SHEEP_FUNCTION_H

#include <sheep/object.h>
#include <sheep/unit.h>

struct sheep_vm;

struct sheep_function {
	struct sheep_unit unit;
	unsigned int nr_parms;
	const char *name;
};

extern const struct sheep_type sheep_function_type;
extern const struct sheep_type sheep_closure_type;

static inline struct sheep_function *sheep_function(sheep_t sheep)
{
	return sheep_data(sheep);
}

sheep_t sheep_make_function(struct sheep_vm *, const char *);
sheep_t sheep_closure_function(struct sheep_vm *, struct sheep_function *);

#endif /* _SHEEP_FUNCTION_H */
