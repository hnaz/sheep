/*
 * include/sheep/core.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_CORE_H
#define _SHEEP_CORE_H

struct sheep_vm;

void sheep_core_init(struct sheep_vm *);
void sheep_core_exit(struct sheep_vm *);

#endif /* _SHEEP_CORE_H */
