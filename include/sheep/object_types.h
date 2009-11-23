/*
 * include/sheep/object_types.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_OBJECT_TYPES_H
#define _SHEEP_OBJECT_TYPES_H

typedef struct sheep_object * sheep_t;

struct sheep_function;
struct sheep_context;
struct sheep_vm;

struct sheep_sequence {
	unsigned long (*length)(sheep_t);
	sheep_t (*concat)(struct sheep_vm *, sheep_t, sheep_t);
	sheep_t (*reverse)(struct sheep_vm *, sheep_t);
};

struct sheep_type {
	const char *name;

	void (*mark)(sheep_t);
	void (*free)(struct sheep_vm *, sheep_t);

	int (*compile)(struct sheep_vm *, struct sheep_function *,
		struct sheep_context *, sheep_t);

	int (*test)(sheep_t);
	int (*equal)(sheep_t, sheep_t);

	const struct sheep_sequence *sequence;

	void (*ddump)(sheep_t);
};

struct sheep_object {
	const struct sheep_type *type;
	unsigned long data;
};

#endif /* _SHEEP_OBJECT_TYPES_H */
