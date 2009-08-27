/*
 * include/sheep/alien.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_ALIEN_H
#define _SHEEP_ALIEN_H

#include <sheep/object.h>
#include <sheep/vm.h>

typedef sheep_t (*sheep_alien_t)(struct sheep_vm *, unsigned int);

struct sheep_alien {
	sheep_alien_t function;
	const char *name;
};

extern const struct sheep_type sheep_alien_type;

sheep_t sheep_make_alien(struct sheep_vm *, sheep_alien_t, const char *);

#endif /* _SHEEP_ALIEN_H */
