#ifndef _SHEEP_VECTOR_H
#define _SHEEP_VECTOR_H

struct sheep_vector {
	void **items;
	unsigned long nr_items;
	unsigned int blocksize;
};

#endif /* _SHEEP_VECTOR_H */
