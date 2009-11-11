/*
 * sheep/unit.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */

#include <sheep/util.h>
#include <sheep/vm.h>

#include <sheep/unit.h>

void sheep_unit_mark_foreigns(struct sheep_unit *unit)
{
	unsigned int i;

	for (i = 0; i < unit->foreigns->nr_items; i++) {
		struct sheep_foreign *foreign;

		foreign = unit->foreigns->items[i];
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
			return;
		}
		prev = this;
		this = next;
	}
	sheep_bug("unqueued live foreign reference");
}

void sheep_unit_free_foreigns(struct sheep_vm *vm, struct sheep_unit *unit)
{
	unsigned int i;

	for (i = 0; i < unit->foreigns->nr_items; i++) {
		struct sheep_foreign *foreign;

		foreign = unit->foreigns->items[i];
		if (foreign->state == SHEEP_FOREIGN_LIVE)
			unlink_pending(vm, foreign);
		sheep_free(foreign);
	}
	sheep_free(unit->foreigns->items);
	sheep_free(unit->foreigns);
}
