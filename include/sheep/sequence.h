/*
 * include/sheep/sequence.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_SEQUENCE_H
#define _SHEEP_SEQUENCE_H

#include <sheep/object.h>

struct sheep_vm;

static inline const struct sheep_sequence *sheep_sequence(sheep_t sheep)
{
	return sheep_type(sheep)->sequence;
}

void sheep_sequence_builtins(struct sheep_vm *);

#endif /* _SHEEP_SEQUENCE_H */
