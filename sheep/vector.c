#include <sheep/vector.h>
#include <sheep/util.h>

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
