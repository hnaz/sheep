/*
 * include/sheep/function.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_FUNCTION_H
#define _SHEEP_FUNCTION_H

#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/code.h>

struct sheep_vm;

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

struct sheep_function {
	struct sheep_code code;
	unsigned int nr_parms;
	unsigned int nr_locals;	/* nr_parms + private slots */
	struct sheep_vector *foreigns;
	const char *name;
};

extern const struct sheep_type sheep_function_type;
extern const struct sheep_type sheep_closure_type;

sheep_t sheep_make_function(struct sheep_vm *, const char *);
sheep_t sheep_closure_function(struct sheep_vm *, struct sheep_function *);

#endif /* _SHEEP_FUNCTION_H */
