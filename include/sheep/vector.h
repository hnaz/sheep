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
	unsigned int blocksize;
};

#define SHEEP_VECTOR_INITIALIZER(_blocksize)				\
	{ .items = 0, .nr_items = 0, .blocksize = (_blocksize) }

#define SHEEP_DEFINE_VECTOR(name, blocksize)				\
	struct sheep_vector name = SHEEP_VECTOR_INITIALIZER(blocksize)

static inline void
sheep_vector_init(struct sheep_vector *vec, unsigned int blocksize)
{
	*vec = (struct sheep_vector)SHEEP_VECTOR_INITIALIZER(blocksize);
}

unsigned long sheep_vector_push(struct sheep_vector *, void *);

void sheep_vector_grow(struct sheep_vector *, unsigned long);

static inline void *sheep_vector_pop(struct sheep_vector *vec)
{
	return vec->items[--vec->nr_items];
}

#endif /* _SHEEP_VECTOR_H */
