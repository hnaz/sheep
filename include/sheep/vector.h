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

static inline void *sheep_vector_pop(struct sheep_vector *vec)
{
	return vec->items[--vec->nr_items];
}

void sheep_vector_concat(struct sheep_vector *, struct sheep_vector *);

#endif /* _SHEEP_VECTOR_H */
