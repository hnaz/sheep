#ifndef _SHEEP_MODULE_H
#define _SHEEP_MODULE_H

#include <sheep/map.h>

struct sheep_module {
	const char *name;
	struct sheep_map env;
};

#endif /* _SHEEP_MODULE_H */
