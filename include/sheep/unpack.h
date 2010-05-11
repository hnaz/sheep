/*
 * include/sheep/unpack.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_UNPACK_H
#define _SHEEP_UNPACK_H

#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/list.h>
#include <stdarg.h>

struct sheep_vm;

int sheep_unpack(const char *, struct sheep_vm *, sheep_t, const char, ...);
int sheep_unpack_list(const char *, struct sheep_vm *,
			      struct sheep_list *, const char *, ...);
int sheep_unpack_stack(const char *, struct sheep_vm *, unsigned int,
			       const char *, ...);

#endif /* _SHEEP_UNPACK_H */
