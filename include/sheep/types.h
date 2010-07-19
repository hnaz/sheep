/*
 * include/sheep/types.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_TYPES_H
#define _SHEEP_TYPES_H

#include <stdlib.h>

typedef struct sheep_object * sheep_t;

struct sheep_function;
struct sheep_compile;
struct sheep_context;
struct sheep_strbuf;
struct sheep_vm;

struct sheep_sequence {
	size_t (*length)(sheep_t);
	sheep_t (*concat)(struct sheep_vm *, sheep_t, unsigned int);
	sheep_t (*reverse)(struct sheep_vm *, sheep_t);
	sheep_t (*nth)(struct sheep_vm *, size_t, sheep_t);
	sheep_t (*slice)(struct sheep_vm *, sheep_t, size_t, size_t);
	sheep_t (*position)(struct sheep_vm *, sheep_t, sheep_t);
};

enum sheep_call {
	SHEEP_CALL_DONE,
	SHEEP_CALL_EVAL,
	SHEEP_CALL_FAIL,
};

struct sheep_type {
	const char *name;

	void (*mark)(sheep_t);
	void (*free)(struct sheep_vm *, sheep_t);

	int (*compile)(struct sheep_compile *,
		       struct sheep_function *,
		       struct sheep_context *,
		       sheep_t);

	enum sheep_call (*call)(struct sheep_vm *,
				sheep_t,
				unsigned int,
				sheep_t *);

	int (*test)(sheep_t);
	int (*equal)(sheep_t, sheep_t);

	void (*format)(sheep_t, struct sheep_strbuf *, int);

	const struct sheep_sequence *sequence;
};

struct sheep_object {
	const struct sheep_type *type;
	unsigned long data;
};

#endif /* _SHEEP_TYPES_H */
