#ifndef _SHEEP_OBJECT_H
#define _SHEEP_OBJECT_H

#include <sheep/vector.h>

typedef struct sheep_object * sheep_t;

struct sheep_objects;
struct sheep_context;
struct sheep_vm;

struct sheep_type {
	void (*mark)(sheep_t);
	void (*free)(struct sheep_vm *, sheep_t);
	int (*compile)(struct sheep_vm *, struct sheep_context *, sheep_t);

	void (*ddump)(sheep_t);
};

struct sheep_object {
	const struct sheep_type *type;
	unsigned long data;
};

sheep_t sheep_object(struct sheep_vm *, const struct sheep_type *, void *);

void sheep_ddump(sheep_t);

static inline void *sheep_data(sheep_t sheep)
{
	return (void *)(sheep->data & ~1);
}

static inline void sheep_set_data(sheep_t sheep, void *data)
{
	sheep->data = (unsigned long)data | (sheep->data & 1);
}

void sheep_mark(sheep_t);

void sheep_protect(struct sheep_vm *, sheep_t);
void sheep_unprotect(struct sheep_vm *, sheep_t);

void sheep_gc_disable(struct sheep_vm *);
void sheep_gc_enable(struct sheep_vm *);

void sheep_objects_exit(struct sheep_vm *);

#endif /* _SHEEP_OBJECT_H */
