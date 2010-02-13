/*
 * include/sheep/list.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_LIST_H
#define _SHEEP_LIST_H

#include <sheep/object.h>
#include <stdarg.h>

struct sheep_vm;

struct sheep_list {
	sheep_t head;
	sheep_t tail;
};

extern const struct sheep_type sheep_list_type;

sheep_t sheep_make_cons(struct sheep_vm *, sheep_t, sheep_t);
sheep_t sheep_make_list(struct sheep_vm *, unsigned int, ...);

static inline struct sheep_list *sheep_list(sheep_t sheep)
{
	return sheep_data(sheep);
}

int sheep_list_search(struct sheep_list *, sheep_t, unsigned long *);

void sheep_list_builtins(struct sheep_vm *);

#endif /* _SHEEP_LIST_H */
