/*
 * include/sheep/read.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_READ_H
#define _SHEEP_READ_H

#include <sheep/object.h>
#include <sheep/vm.h>
#include <stdio.h>

extern struct sheep_object sheep_eof;

sheep_t sheep_read(struct sheep_vm *, FILE *);

#endif /* _SHEEP_READ_H */
