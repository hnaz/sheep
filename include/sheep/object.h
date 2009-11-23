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

static inline const struct sheep_sequence *sheep_sequence(sheep_t sheep)
{
	return sheep_type(sheep)->sequence;
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

void sheep_mark(sheep_t);

void sheep_protect(struct sheep_vm *, sheep_t);
void sheep_unprotect(struct sheep_vm *, sheep_t);

void sheep_gc_disable(struct sheep_vm *);
void sheep_gc_enable(struct sheep_vm *);

void sheep_objects_exit(struct sheep_vm *);

#endif /* _SHEEP_OBJECT_H */
