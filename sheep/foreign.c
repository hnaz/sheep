/*
 * sheep/foreign.c
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/vector.h>
#include <sheep/util.h>
#include <sheep/gc.h>
#include <sheep/vm.h>

#include <sheep/foreign.h>

/* foreign slot allocation at compile time */
unsigned int sheep_foreign_slot(struct sheep_function *function,
				unsigned int dist,
				unsigned int slot)
{
	struct sheep_freevar *freevar;
	struct sheep_vector *foreign;

	foreign = function->foreign;
	if (!foreign) {
		foreign = sheep_zalloc(sizeof(struct sheep_vector));
		function->foreign = foreign;
	} else {
		unsigned int i;

		for (i = 0; i < foreign->nr_items; i++) {
			freevar = foreign->items[i];

			if (freevar->dist != dist)
				continue;
			if (freevar->slot != slot)
				continue;

			return i;
		}
	}

	freevar = sheep_malloc(sizeof(struct sheep_freevar));
	freevar->dist = dist;
	freevar->slot = slot;

	return sheep_vector_push(foreign, freevar);
}

/* foreign slot upward propagation at function finalization */
void sheep_foreign_propagate(struct sheep_function *parent,
			     struct sheep_function *child)
{
	unsigned int i;

	for (i = 0; i < child->foreign->nr_items; i++) {
		struct sheep_freevar *freevar;
		unsigned int dist, slot;

		freevar = child->foreign->items[i];

		/* Child refers to parent slot */
		if (freevar->dist == 1)
			continue;
		/*
		 * Child refers to grand-parent slot, relay through
		 * parent.
		 */
		dist = freevar->dist - 1;
		slot = freevar->slot;
		freevar->slot = sheep_foreign_slot(parent, dist, slot);
	}
}

static struct sheep_indirect *open_indirect(struct sheep_vm *vm,
					    unsigned long index)
{
	struct sheep_indirect *new, *prev = NULL, *next = vm->pending;

	while (next) {
		if (index == next->value.live.index) {
			next->count++;
			return next;
		}
		if (index > next->value.live.index)
			break;
		prev = next;
		next = next->value.live.next;
	}

	new = sheep_malloc(sizeof(struct sheep_indirect));
	new->count = 1;
	new->value.live.index = index;
	new->value.live.next = next;
	if (prev)
		prev->value.live.next = new;
	else
		vm->pending = new;

	return new;
}

/* indirect pointer establishing at closure creation */
struct sheep_vector *sheep_foreign_open(struct sheep_vm *vm,
					unsigned long basep,
					struct sheep_function *parent,
					struct sheep_function *child)
{
	struct sheep_vector *freevars = child->foreign;
	struct sheep_vector *indirects;
	unsigned int i;

	indirects = sheep_zalloc(sizeof(struct sheep_vector));

	for (i = 0; i < freevars->nr_items; i++) {
		struct sheep_indirect *indirect;
		struct sheep_freevar *freevar;

		freevar = freevars->items[i];
		if (freevar->dist == 1)
			indirect = open_indirect(vm, basep + freevar->slot);
		else {
			indirect = parent->foreign->items[freevar->slot];
			if (indirect->count < 0)
				indirect->count--;
			else
				indirect->count++;
		}
		sheep_vector_push(indirects, indirect);
	}
	return indirects;
}

/* indirect pointer preservation at owner death */
void sheep_foreign_save(struct sheep_vm *vm, unsigned long basep)
{
	while (vm->pending) {
		struct sheep_indirect *indirect = vm->pending;
		unsigned long index;

		index = indirect->value.live.index;
		if (index < basep)
			break;

		vm->pending = indirect->value.live.next;

		indirect->count = -indirect->count;
		indirect->value.closed = vm->stack.items[index];
	}
}

/* mark reachable indirect pointers */
void sheep_foreign_mark(struct sheep_vector *foreign)
{
	unsigned int i;

	for (i = 0; i < foreign->nr_items; i++) {
		struct sheep_indirect *indirect;

		indirect = foreign->items[i];
		if (indirect->count < 0)
			sheep_mark(indirect->value.closed);
	}
}

static void unlink_live(struct sheep_vm *vm, struct sheep_indirect *indirect)
{
	struct sheep_indirect *prev = NULL, *this = vm->pending;

	while (this) {
		struct sheep_indirect *next;

		next = this->value.live.next;
		if (this == indirect) {
			if (prev)
				prev->value.live.next = next;
			else
				vm->pending = next;
			return;
		}
		prev = this;
		this = next;
	}
	sheep_bug("unqueued live indirect pointer");
}

/* indirect pointer release at closure death */
void sheep_foreign_release(struct sheep_vm *vm, struct sheep_vector *foreign)
{
	unsigned int i;

	for (i = 0; i < foreign->nr_items; i++) {
		struct sheep_indirect *indirect;

		indirect = foreign->items[i];
		if (indirect->count > 0) {
			if (--indirect->count == 0) {
				unlink_live(vm, indirect);
				sheep_free(indirect);
			}
		} else {
			if (++indirect->count == 0)
				sheep_free(indirect);
		}
	}
	sheep_free(foreign->items);
	sheep_free(foreign);
}
