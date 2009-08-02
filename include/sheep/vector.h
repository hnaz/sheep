#ifndef _SHEEP_VECTOR_H
#define _SHEEP_VECTOR_H

struct sheep_vector {
	void **items;
	unsigned long nr_items;
	unsigned int blocksize;
};

unsigned long sheep_vector_push(struct sheep_vector *, void *);

static inline void *sheep_vector_pop(struct sheep_vector *vec)
{
	return vec->items[--vec->nr_items];
}

#endif /* _SHEEP_VECTOR_H */
