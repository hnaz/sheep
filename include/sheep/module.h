/*
 * include/sheep/module.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_MODULE_H
#define _SHEEP_MODULE_H

#include <sheep/object.h>
#include <sheep/alien.h>
#include <sheep/map.h>

struct sheep_vm;

struct sheep_module {
	const char *name;
	struct sheep_map env;
	void *handle;
};

extern const struct sheep_type sheep_module_type;

sheep_t sheep_module_load(struct sheep_vm *, const char *);

unsigned int sheep_module_shared(struct sheep_vm *, struct sheep_module *,
				const char *, sheep_t);

static inline void sheep_module_function(struct sheep_vm *vm,
					struct sheep_module *module,
					const char *name, sheep_alien_t alien)
{
	sheep_module_shared(vm, module, name,
			sheep_make_alien(vm, alien, name));
}

void sheep_module_builtins(struct sheep_vm *);

#endif /* _SHEEP_MODULE_H */
