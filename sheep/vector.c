/*
 * sheep/vector.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/vector.h>
#include <sheep/util.h>
#include <string.h>

static void vector_resize(struct sheep_vector *vec)
{
	unsigned long nr;

	nr = vec->nr_items + vec->blocksize;
	vec->items = sheep_realloc(vec->items, nr * sizeof(void *));
}

unsigned long sheep_vector_push(struct sheep_vector *vec, void *item)
{
	if (!(vec->nr_items % vec->blocksize))
		vector_resize(vec);
	vec->items[vec->nr_items] = item;
	return vec->nr_items++;
}

void sheep_vector_grow(struct sheep_vector *vec, unsigned long delta)
{
	unsigned long want, next;

	want = vec->nr_items + delta;
	next = (want + vec->blocksize) / vec->blocksize * vec->blocksize;

	if (next - vec->nr_items >= vec->blocksize)
		vec->items = sheep_realloc(vec->items, next * sizeof(void *));
	vec->nr_items = want;
}
