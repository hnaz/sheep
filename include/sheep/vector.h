/*
 * include/sheep/vector.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_VECTOR_H
#define _SHEEP_VECTOR_H

struct sheep_vector {
	void **items;
	unsigned long nr_items;
	unsigned long nr_alloc;
};

unsigned long sheep_vector_push(struct sheep_vector *, void *);
void sheep_vector_grow(struct sheep_vector *, unsigned long);
void *sheep_vector_pop(struct sheep_vector *);

#endif /* _SHEEP_VECTOR_H */
