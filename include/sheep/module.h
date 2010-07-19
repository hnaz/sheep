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

unsigned int sheep_module_variable(struct sheep_vm *,
				   struct sheep_module *,
				   const char *,
				   sheep_t);
void sheep_module_function(struct sheep_vm *,
			   struct sheep_module *,
			   const char *,
			   sheep_alien_t);

void sheep_module_builtins(struct sheep_vm *);

#endif /* _SHEEP_MODULE_H */
