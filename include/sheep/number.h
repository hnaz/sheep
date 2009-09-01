/*
 * include/sheep/number.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_NUMBER_H
#define _SHEEP_NUMBER_H

#include <sheep/object.h>

extern const struct sheep_type sheep_number_type;

sheep_t sheep_make_number(struct sheep_vm *, double);

#endif /* _SHEEP_NUMBER_H */
