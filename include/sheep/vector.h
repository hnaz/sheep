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

#define SHEEP_VECTOR_INITIALIZER					\
	{ .items = 0, .nr_items = 0, .nr_alloc = 0 }

#define SHEEP_DEFINE_VECTOR(name)					\
	struct sheep_vector name = SHEEP_VECTOR_INITIALIZER

static inline void
sheep_vector_init(struct sheep_vector *vec)
{
	*vec = (struct sheep_vector)SHEEP_VECTOR_INITIALIZER;
}

unsigned long sheep_vector_push(struct sheep_vector *, void *);
void sheep_vector_grow(struct sheep_vector *, unsigned long);
void *sheep_vector_pop(struct sheep_vector *);

#endif /* _SHEEP_VECTOR_H */
