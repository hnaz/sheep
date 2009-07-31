#ifndef _SHEEP_BLOCK_H
#define _SHEEP_BLOCK_H

#include <sheep/vector.h>

struct sheep_block {
	unsigned long code_offset;
	struct sheep_vector locals;	/* sheep_t []  */
	struct sheep_vector foreigns;	/* sheep_t *[] */
};

#endif /* _SHEEP_BLOCK_H */
