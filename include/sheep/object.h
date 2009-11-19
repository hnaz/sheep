/*
 * include/sheep/object.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_OBJECT_H
#define _SHEEP_OBJECT_H

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

sheep_t sheep_make_object(struct sheep_vm *, const struct sheep_type *, void *);

static inline const struct sheep_type *sheep_type(sheep_t sheep)
{
	return sheep->type;
}

static inline const struct sheep_sequence *sheep_sequence(sheep_t sheep)
{
	return sheep_type(sheep)->sequence;
}

static inline void *sheep_data(sheep_t sheep)
{
	return (void *)(sheep->data & ~1);
}

static inline void sheep_set_data(sheep_t sheep, void *data)
{
	sheep->data = (unsigned long)data | (sheep->data & 1);
}

int sheep_test(sheep_t);
int sheep_equal(sheep_t, sheep_t);

void __sheep_ddump(sheep_t);
void sheep_ddump(sheep_t);

void sheep_mark(sheep_t);

void sheep_protect(struct sheep_vm *, sheep_t);
void sheep_unprotect(struct sheep_vm *, sheep_t);

void sheep_gc_disable(struct sheep_vm *);
void sheep_gc_enable(struct sheep_vm *);

void sheep_objects_exit(struct sheep_vm *);

#endif /* _SHEEP_OBJECT_H */
