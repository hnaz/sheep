/*
 * sheep/vm.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/number.h>
#include <sheep/object.h>
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

void sheep_vm_init(struct sheep_vm *vm)
{
	memset(vm, 0, sizeof(*vm));
	sheep_core_init(vm);
	sheep_bool_builtins(vm);
	sheep_number_builtins(vm);
	sheep_list_builtins(vm);
}

void sheep_vm_exit(struct sheep_vm *vm)
{
	sheep_core_exit(vm);
	sheep_evaluator_exit(vm);
	sheep_free(vm->globals.items);
	sheep_objects_exit(vm);
}
