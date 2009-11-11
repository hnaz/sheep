/*
 * include/sheep/unit.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_UNIT_H
#define _SHEEP_UNIT_H

#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/code.h>

enum sheep_foreign_state {
	SHEEP_FOREIGN_TEMPLATE,
	SHEEP_FOREIGN_LIVE,
	SHEEP_FOREIGN_CLOSED,
};

struct sheep_foreign {
	enum sheep_foreign_state state;
	union {
		struct {
			unsigned int dist;
			unsigned int slot;
		} template;
		struct {
			unsigned int index;
			struct sheep_foreign *next;
		} live;
		sheep_t closed;
	} value;
};

struct sheep_unit {
	struct sheep_code code;
	unsigned int nr_locals;
	struct sheep_vector *foreigns;
};

void sheep_unit_mark_foreigns(struct sheep_unit *);
void sheep_unit_free_foreigns(struct sheep_vm *, struct sheep_unit *);

#endif /* _SHEEP_UNIT_H */
