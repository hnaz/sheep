/*
 * include/sheep/list.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_LIST_H
#define _SHEEP_LIST_H

#include <sheep/object.h>
#include <sheep/vm.h>
#include <stdarg.h>

struct sheep_list {
	sheep_t head;
	struct sheep_list *tail;
};

extern const struct sheep_type sheep_list_type;

struct sheep_list *sheep_make_cons(struct sheep_vm *, sheep_t,
				struct sheep_list *);
struct sheep_list *__sheep_make_list(struct sheep_vm *, sheep_t);
sheep_t sheep_make_list(struct sheep_vm *, unsigned int, ...);

#endif /* _SHEEP_LIST_H */
