/*
 * sheep/function.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>

#include <sheep/function.h>

static void free_function(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->foreigns) {
		struct sheep_vector *foreigns;

		foreigns = sheep_foreigns(function);
		if (sheep_active_closure(function)) {
			unsigned int i;

			for (i = 0; i < foreigns->nr_items; i++) {
				sheep_t *start, *end, *slot;
				
				start = (sheep_t *)vm->stack.items;
				end = start + vm->stack.nr_items;
				slot = foreigns->items[i];
				
				if (slot < start || slot >= end)
					sheep_free(slot);
			}
		}
		sheep_free(foreigns->items);
		sheep_free(foreigns);
	}
	sheep_free(function);
}

const struct sheep_type sheep_function_type = {
	.name = "function",
	.free = free_function,
};

sheep_t sheep_make_function(struct sheep_vm *vm)
{
	struct sheep_function *function;

	function = sheep_zalloc(sizeof(struct sheep_function));
	return sheep_make_object(vm, &sheep_function_type, function);
}
