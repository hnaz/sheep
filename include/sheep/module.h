#ifndef _SHEEP_MODULE_H
#define _SHEEP_MODULE_H

#include <sheep/object.h>
#include <sheep/map.h>

/* We get included by vm.h */
struct sheep_vm;

struct sheep_module {
	const char *name;
	struct sheep_map env;
};

unsigned int sheep_module_shared(struct sheep_vm *, struct sheep_module *,
				const char *, sheep_t);

#endif /* _SHEEP_MODULE_H */
