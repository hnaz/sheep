/*
 * sheep/module.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/map.h>
#include <sheep/vm.h>

#include <sheep/module.h>

/**
 * sheep_module_shared - register a shared global slot
 * @vm: runtime
 * @module: binding environment
 * @name: name to associate the slot with
 * @sheep: initial slot value
 *
 * Allocates a global slot, stores @sheep it in and binds the slot to
 * @name in @module.
 *
 * The slot can be accessed and modified from sheep code by @name and
 * from C code through the returned slot index into vm->globals.items.
 */
unsigned int sheep_module_shared(struct sheep_vm *vm,
				struct sheep_module *module,
				const char *name, sheep_t sheep)
{
	unsigned long slot;

	slot = sheep_vector_push(&vm->globals, sheep);
	sheep_map_set(&module->env, name, (void *)slot);
	return slot;
}
