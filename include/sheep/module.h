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

typedef int (*sheep_alien_t)(struct sheep_vm *);

extern const struct sheep_type sheep_alien_type;

unsigned int sheep_module_shared(struct sheep_vm *, struct sheep_module *,
				const char *, sheep_t);
void sheep_module_function(struct sheep_vm *, struct sheep_module *,
			const char *, sheep_alien_t);

static inline sheep_alien_t sheep_calien(sheep_t sheep)
{
	return sheep_data(sheep);
}

#endif /* _SHEEP_MODULE_H */
