/*
 * include/sheep/gc.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_GC_H
#define _SHEEP_GC_H

#include <sheep/types.h>

struct sheep_vm;

struct sheep_object *sheep_gc_alloc(struct sheep_vm *);

void sheep_mark(sheep_t);
void sheep_protect(struct sheep_vm *, sheep_t);
void sheep_unprotect(struct sheep_vm *, sheep_t);

void sheep_gc_exit(struct sheep_vm *);

#endif /* _SHEEP_GC_H */
