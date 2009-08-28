/*
 * include/sheep/list.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_LIST_H
#define _SHEEP_LIST_H

#include <sheep/object.h>
#include <sheep/vm.h>

struct sheep_list {
	sheep_t head;
	sheep_t tail;
};

extern const struct sheep_type sheep_list_type;

sheep_t sheep_make_list(struct sheep_vm *, sheep_t, sheep_t);

static inline struct sheep_list *sheep_list(sheep_t sheep)
{
	return sheep_data(sheep);
}

#endif /* _SHEEP_LIST_H */
