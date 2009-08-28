/*
 * sheep/vm.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/core.h>
#include <sheep/eval.h>
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
		sheep_mark(vm->stack.items[i]);
}

void sheep_vm_init(struct sheep_vm *vm)
{
	memset(vm, 0, sizeof(*vm));
	vm->protected.blocksize = 32;
	vm->globals.blocksize = 32;
	sheep_code_init(&vm->code);
	sheep_evaluator_init(vm);
	sheep_core_init(vm);
}

void sheep_vm_exit(struct sheep_vm *vm)
{
	sheep_core_exit(vm);
	sheep_evaluator_exit(vm);
	sheep_free(vm->code.code.items);
	sheep_free(vm->globals.items);
	sheep_objects_exit(vm);
}
