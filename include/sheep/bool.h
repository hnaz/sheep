/*
 * include/sheep/bool.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_BOOL_H
#define _SHEEP_BOOL_H

#include <sheep/object.h>

struct sheep_vm;

extern const struct sheep_type sheep_bool_type;

extern struct sheep_object sheep_true;
extern struct sheep_object sheep_false;

void sheep_bool_builtins(struct sheep_vm *);

#endif /* _SHEEP_BOOL_H */
