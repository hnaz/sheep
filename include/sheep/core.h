/*
 * include/sheep/core.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_CORE_H
#define _SHEEP_CORE_H

#include <sheep/vm.h>
#include <stdarg.h>

int sheep_unpack_stack(const char *, struct sheep_vm *, unsigned int,
		const char *, ...);

void sheep_core_init(struct sheep_vm *);
void sheep_core_exit(struct sheep_vm *);

#endif /* _SHEEP_CORE_H */
