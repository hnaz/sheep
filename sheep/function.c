/*
 * sheep/function.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/function.h>

static void mark_function(sheep_t sheep)
{
	struct sheep_function *function;
	unsigned int i;

	function = sheep_data(sheep);
	if (!function->foreigns)
		return;

	for (i = 0; i < function->foreigns->nr_items; i++) {
		struct sheep_foreign *foreign;

		foreign = function->foreigns->items[i];
		/*
		 * Live and closed do not mix with template.
		 */
		if (foreign->state == SHEEP_FOREIGN_TEMPLATE)
			return;
		if (foreign->state == SHEEP_FOREIGN_CLOSED)
			sheep_mark(foreign->value.closed);
	}
}

static void unlink_pending(struct sheep_vm *vm, struct sheep_foreign *foreign)
{
	struct sheep_foreign *prev = NULL, *this = vm->pending;

	while (this) {
		struct sheep_foreign *next;

		next = this->value.live.next;
		if (this == foreign) {
			if (prev)
				prev->value.live.next = next;
			else
				vm->pending = next;
			break;
		}
		prev = this;
		this = next;
	}
	sheep_bug("unqueued live foreign reference");
}

static void free_function(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;
	struct sheep_code *code = NULL;
	struct sheep_vector *foreigns;

	function = sheep_data(sheep);
	foreigns = function->foreigns;
	if (foreigns) {
		unsigned int i;

		for (i = 0; i < foreigns->nr_items; i++) {
			struct sheep_foreign *foreign;

			foreign = foreigns->items[i];
			if (foreign->state == SHEEP_FOREIGN_TEMPLATE)
				code = &function->code;
			else if (foreign->state == SHEEP_FOREIGN_LIVE)
				unlink_pending(vm, foreign);
			sheep_free(foreign);
		}
		sheep_free(foreigns->items);
		sheep_free(foreigns);
		if (code)
			sheep_code_exit(code);
	} else
		sheep_code_exit(&function->code);
	sheep_free(function->name);
	sheep_free(function);
}

static void function_ddump(sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->name)
		printf("#<function '%s'>", function->name);
	else
		printf("#<function '%p'>", function);
}

const struct sheep_type sheep_function_type = {
	.name = "function",
	.mark = mark_function,
	.free = free_function,
	.ddump = function_ddump,
};

sheep_t sheep_make_function(struct sheep_vm *vm, const char *name)
{
	struct sheep_function *function;

	function = sheep_zalloc(sizeof(struct sheep_function));
	sheep_code_init(&function->code);
	if (name)
		function->name = sheep_strdup(name);
	return sheep_make_object(vm, &sheep_function_type, function);
}

sheep_t sheep_copy_function(struct sheep_vm *vm,
			struct sheep_function *function)
{
	struct sheep_function *copy;

	copy = sheep_malloc(sizeof(struct sheep_function));
	*copy = *function;
	if (function->name)
		copy->name = sheep_strdup(function->name);
	return sheep_make_object(vm, &sheep_function_type, copy);
}
