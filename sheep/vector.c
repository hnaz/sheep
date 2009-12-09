/*
 * sheep/vector.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 *
 * fls() taken from the Linux kernel sources.
 */
#include <sheep/vector.h>
#include <sheep/util.h>
#include <string.h>

#define BITS_PER_LONG	(sizeof(long) * 8)
/**
 * fls - find position of the last (most-significant) set bit in a long word
 * @word: the word to search
 *
 * Returns 1 for 0 as well.  Callsite has to deal with ith.
 */
static unsigned long fls(unsigned long word)
{
	int num = BITS_PER_LONG;

	if (BITS_PER_LONG == 64 && !(word & (~0UL << 32))) {
		num -= 32;
		word <<= 32;
	}
	if (!(word & (~0UL << (BITS_PER_LONG - 16)))) {
		num -= 16;
		word <<= 16;
	}
	if (!(word & (~0UL << (BITS_PER_LONG - 8)))) {
		num -= 8;
		word <<= 8;
	}
	if (!(word & (~0UL << (BITS_PER_LONG - 4)))) {
		num -= 4;
		word <<= 4;
	}
	if (!(word & (~0UL << (BITS_PER_LONG - 2)))) {
		num -= 2;
		word <<= 2;
	}
	if (!(word & (~0UL << (BITS_PER_LONG - 1))))
		num -= 1;
	return num;
}

static void vector_resize(struct sheep_vector *vec, unsigned long goal)
{
	vec->nr_alloc = 1UL << fls(goal - 1);
	vec->items = sheep_realloc(vec->items, vec->nr_alloc * sizeof(void *));
}

unsigned long sheep_vector_push(struct sheep_vector *vec, void *item)
{
	if (vec->nr_items == vec->nr_alloc)
		vector_resize(vec, vec->nr_items + 1);
	vec->items[vec->nr_items] = item;
	return vec->nr_items++;
}

void *sheep_vector_pop(struct sheep_vector *vec)
{
	void *item;

	vec->nr_items -= 1;
	item = vec->items[vec->nr_items];
	if (vec->nr_items && vec->nr_items < (vec->nr_alloc >> 3))
		vector_resize(vec, vec->nr_items);
	return item;
}

void sheep_vector_grow(struct sheep_vector *vec, unsigned long delta)
{
	unsigned long want;

	want = vec->nr_items + delta;
	if (want >= vec->nr_alloc)
		vector_resize(vec, want);
	while (vec->nr_items < want)
		vec->items[vec->nr_items++] = NULL;
}
