/*
 * include/sheep/foreign.h
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_FOREIGN_H
#define _SHEEP_FOREIGN_H

#include <sheep/function.h>
#include <sheep/vector.h>
#include <sheep/vm.h>

/**
 * struct sheep_freevar - lexical free variable location
 * @dist: functional distance
 * @slot: index of an immediate parent slot
 *
 * @slot indexes a local slot if @dist is 1 and a foreign slot
 * otherwise.
 */
struct sheep_freevar {
	unsigned int dist;
	unsigned int slot;
};

/**
 * struct sheep_indirect - indirect slot pointer
 * @count: user count, negative if slot is live
 * @index: live stack slot index
 * @next: list linkage of live slots
 * @closed: preserved value slot
 */
struct sheep_indirect {
	int count;
	union {
		struct {
			unsigned int index;
			struct sheep_indirect *next;
		} live;
		sheep_t closed;
	} value;
};

/* compile-time */
unsigned int sheep_foreign_slot(struct sheep_function *,
				unsigned int,
				unsigned int);
void sheep_foreign_propagate(struct sheep_function *, struct sheep_function *);

/* eval-time */
struct sheep_vector *sheep_foreign_open(struct sheep_vm *,
					unsigned long,
					struct sheep_function *,
					struct sheep_function *);
void sheep_foreign_save(struct sheep_vm *, unsigned long);

/* life-time */
void sheep_foreign_mark(struct sheep_vector *);
void sheep_foreign_release(struct sheep_vm *, struct sheep_vector *);

#endif /* _SHEEP_FOREIGN_H */
