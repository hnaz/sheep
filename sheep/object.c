/*
 * sheep/object.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/string.h>
#include <sheep/vector.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

#include <sheep/object.h>

#define PAGE_SIZE	sysconf(_SC_PAGE_SIZE)
#define POOL_SIZE	(PAGE_SIZE / sizeof(struct sheep_object))

struct sheep_objects {
	struct sheep_object *mem;
	struct sheep_object *free;
	unsigned int nr_used;
	struct sheep_objects *next;
};

static struct sheep_objects *alloc_pool(void)
{
	struct sheep_objects *pool;
	unsigned int i;

	pool = sheep_malloc(sizeof(struct sheep_objects));
	pool->mem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	pool->free = pool->mem;
	for (i = 0; i < POOL_SIZE - 1; i++)
		pool->mem[i].data = (unsigned long)&pool->mem[i + 1];
	pool->nr_used = 0;
	pool->next = NULL;
	return pool;
}

static void free_pool(struct sheep_objects *pool)
{
	munmap(pool->mem, PAGE_SIZE);
	sheep_free(pool);
}

static void unmark_pools(struct sheep_objects *pool)
{
	while (pool) {
		unsigned int i;

		for (i = 0; i < POOL_SIZE; i++) {
			struct sheep_object *sheep = &pool->mem[i];

			if (sheep->type)
				sheep->data &= ~1;
		}
		pool = pool->next;
	}
}

static void unmark(struct sheep_vm *vm)
{
	unmark_pools(vm->parts);
	unmark_pools(vm->fulls);
}

static void mark_protected(struct sheep_vector *protected)
{
	unsigned long i;

	for (i = 0; i < protected->nr_items; i++)
		sheep_mark(protected->items[i]);
}

static unsigned int collect_pool(struct sheep_vm *vm,
				struct sheep_objects *pool)
{
	unsigned int i, moved;

	for (i = moved = 0; i < POOL_SIZE; i++) {
		struct sheep_object *sheep = &pool->mem[i];

		if (sheep->data & 1) {
			sheep->data &= ~1;
			continue;
		}

		if (sheep->type->free)
			sheep->type->free(vm, sheep);

		sheep->data = (unsigned long)pool->free;
		sheep->type = NULL;
		pool->free = &pool->mem[i];
		pool->nr_used--;
		moved++;
	}
	return moved;
}

static void collect(struct sheep_vm *vm)
{
	struct sheep_objects *pool, *next,
		*cache_part = NULL, *last_full = NULL;

	if (vm->gc_disabled)
		goto alloc;

	unmark(vm);
	sheep_vm_mark(vm);
	mark_protected(&vm->protected);

	for (pool = vm->fulls; pool; pool = next) {
		unsigned int moved;

		moved = collect_pool(vm, pool);
		next = pool->next;

		if (!moved) {
			last_full = pool;
			continue;
		}

		if (last_full)
			last_full->next = pool->next;
		else
			vm->fulls = pool->next;

		if (moved < POOL_SIZE || !cache_part) {
			pool->next = vm->parts;
			vm->parts = cache_part = pool;
		} else
			free_pool(pool);
	}

alloc:
	if (!vm->parts)
		vm->parts = alloc_pool();
}

static sheep_t alloc(struct sheep_vm *vm)
{
	struct sheep_object *sheep = vm->parts->free;

	if (++vm->parts->nr_used < POOL_SIZE)
		vm->parts->free = (struct sheep_object *)sheep->data;
	else {
		struct sheep_objects *pool;

		pool = vm->parts;
		vm->parts = vm->parts->next;
		pool->next = vm->fulls;
		vm->fulls = pool;
	}
	return sheep;
}

sheep_t sheep_make_object(struct sheep_vm *vm, const struct sheep_type *type,
			void *data)
{
	struct sheep_object *sheep;

#if 1
	if (!vm->parts)
#endif
		collect(vm);

	sheep = alloc(vm);
	sheep->type = type;
	sheep->data = (unsigned long)data;
	return sheep;
}

int sheep_test(sheep_t sheep)
{
	if (sheep_type(sheep)->test)
		return sheep_type(sheep)->test(sheep);
	return 1;
}

int sheep_equal(sheep_t a, sheep_t b)
{
	if (a == b)
		return 1;
	if (sheep_type(a) != sheep_type(b))
		return 0;
	if (sheep_type(a)->equal)
		return sheep_type(a)->equal(a, b);
	return 0;
}

void __sheep_ddump(sheep_t sheep)
{
	char *str;

	str = sheep_format(sheep);
	fputs(str, stdout);
	sheep_free(str);
}

void sheep_ddump(sheep_t sheep)
{
	__sheep_ddump(sheep);
	puts("");
}

void sheep_mark(sheep_t sheep)
{
	if (sheep_is_fixnum(sheep))
		return;
	if (sheep->data & 1)
		return;
	sheep->data |= 1;
	if (sheep_type(sheep)->mark)
		sheep_type(sheep)->mark(sheep);
}

void sheep_protect(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_vector_push(&vm->protected, sheep);
}

void sheep_unprotect(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_t prot;

	prot = sheep_vector_pop(&vm->protected);
	sheep_bug_on(prot != sheep);
}

void sheep_gc_disable(struct sheep_vm *vm)
{
	vm->gc_disabled = 1;
}

void sheep_gc_enable(struct sheep_vm *vm)
{
	vm->gc_disabled = 0;
}

static void drain_pool(struct sheep_vm *vm, struct sheep_objects *pool)
{
	unsigned int i;

	for (i = 0; i < POOL_SIZE; i++) {
		struct sheep_object *sheep = &pool->mem[i];

		if (sheep->type && sheep->type->free)
			sheep->type->free(vm, sheep);
	}
}

void sheep_objects_exit(struct sheep_vm *vm)
{
	struct sheep_objects *next;

	sheep_free(vm->protected.items);
	while (vm->parts) {
		next = vm->parts->next;
		drain_pool(vm, vm->parts);
		free_pool(vm->parts);
		vm->parts = next;
	}
	while (vm->fulls) {
		next = vm->fulls->next;
		drain_pool(vm, vm->fulls);
		free_pool(vm->fulls);
		vm->fulls = next;
	}
}
