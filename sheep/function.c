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
			return;
		}
		prev = this;
		this = next;
	}
	sheep_bug("unqueued live foreign reference");
}

static void free_foreigns(struct sheep_vm *vm, struct sheep_vector *foreigns)
{
	unsigned int i;

	for (i = 0; i < foreigns->nr_items; i++) {
		struct sheep_foreign *foreign;

		foreign = foreigns->items[i];
		if (foreign->state == SHEEP_FOREIGN_LIVE)
			unlink_pending(vm, foreign);
		sheep_free(foreign);
	}
	sheep_free(foreigns->items);
	sheep_free(foreigns);
}

static void free_function(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->foreigns)
		free_foreigns(vm, function->foreigns);
	sheep_code_exit(&function->code);
	sheep_free(function->name);
	sheep_free(function);
}

static void ddump_function(sheep_t sheep)
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
	.free = free_function,
	.ddump = ddump_function,
};

static void mark_foreigns(struct sheep_vector *foreigns)
{
	unsigned int i;

	for (i = 0; i < foreigns->nr_items; i++) {
		struct sheep_foreign *foreign;

		foreign = foreigns->items[i];
		if (foreign->state == SHEEP_FOREIGN_CLOSED)
			sheep_mark(foreign->value.closed);
	}
}

static void mark_closure(sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	mark_foreigns(closure->foreigns);
}


static void free_closure(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *closure;

	closure = sheep_data(sheep);
	free_foreigns(vm, closure->foreigns);
	sheep_free(closure->name);
	sheep_free(closure);
}

const struct sheep_type sheep_closure_type = {
	.name = "function",
	.mark = mark_closure,
	.free = free_closure,
	.ddump = ddump_function,
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

sheep_t sheep_closure_function(struct sheep_vm *vm,
			struct sheep_function *function)
{
	struct sheep_function *closure;

	closure = sheep_malloc(sizeof(struct sheep_function));
	*closure = *function;
	if (function->name)
		closure->name = sheep_strdup(function->name);
	return sheep_make_object(vm, &sheep_closure_type, closure);
}
