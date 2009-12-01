/*
 * sheep/vm.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/number.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/alien.h>
#include <sheep/bool.h>
#include <sheep/core.h>
#include <sheep/eval.h>
#include <sheep/list.h>
#include <sheep/util.h>
#include <string.h>

#include <sheep/vm.h>

void sheep_vm_mark(struct sheep_vm *vm)
{
	unsigned int i;

	for (i = 0; i < vm->globals.nr_items; i++)
		/*
		 * Empty global slots occur with variable slots that
		 * get reserved at compile time and initialized during
		 * evaluation.
		 */
		if (vm->globals.items[i])
			sheep_mark(vm->globals.items[i]);

	for (i = 0; i < vm->stack.nr_items; i++)
		if (vm->stack.items[i])
			sheep_mark(vm->stack.items[i]);
}

unsigned int sheep_vm_bind(struct sheep_vm *vm, struct sheep_map *env,
			const char *name, sheep_t value)
{
	unsigned int slot;

	slot = sheep_vm_constant(vm, value);
	sheep_map_set(env, name, (void *)(unsigned long)slot);
	return slot;
}

void sheep_vm_variable(struct sheep_vm *vm, const char *name, sheep_t value)
{
	sheep_vm_bind(vm, &vm->builtins, name, value);
}

void sheep_vm_function(struct sheep_vm *vm, const char *name, sheep_alien_t f)
{
	sheep_vm_variable(vm, name, sheep_make_alien(vm, f, name));
}

void sheep_vm_init(struct sheep_vm *vm)
{
	memset(vm, 0, sizeof(*vm));
	sheep_core_init(vm);
	sheep_bool_builtins(vm);
	sheep_number_builtins(vm);
	sheep_string_builtins(vm);
	sheep_list_builtins(vm);
}

void sheep_vm_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->builtins);
	sheep_core_exit(vm);
	sheep_evaluator_exit(vm);
	sheep_free(vm->globals.items);
	sheep_objects_exit(vm);
}
