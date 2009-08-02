#include <sheep/vector.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <sheep/object.h>

#define PAGE_SIZE	sysconf(_SC_PAGE_SIZE)
#define POOL_SIZE	(PAGE_SIZE / sizeof(struct sheep_object))

struct sheep_object_pool {
	struct sheep_object *mem;
	struct sheep_object *free;
	unsigned int nr_used;
	struct sheep_object_pool *next;
};

static struct sheep_object_pool *alloc_pool(void)
{
	struct sheep_object_pool *pool;
	unsigned int i;

	pool = sheep_malloc(sizeof(struct sheep_object_pool));
	pool->mem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	pool->free = pool->mem;
	for (i = 0; i < POOL_SIZE - 1; i++)
		pool->mem[i].data = (unsigned long)&pool->mem[i + 1];
	pool->nr_used = 0;
	pool->next = NULL;
	return pool;
}

static void free_pool(struct sheep_object_pool *pool)
{
	munmap(pool->mem, PAGE_SIZE);
	free(pool);
}

static void mark_protected(struct sheep_vector *protected)
{
	unsigned long i;

	for (i = 0; i < protected->nr_items; i++)
		sheep_mark(protected->items[i]);
}

static void collect(struct sheep_vm *vm)
{
	struct sheep_object_pool *pool, *cache = NULL, *last = NULL;
	struct sheep_objects *objects = &vm->objects;
	
	sheep_mark_vm(vm);
	mark_protected(&objects->protected);

	for (pool = objects->fulls; pool; last = pool, pool = pool->next) {
		unsigned int i, moved;

		for (i = moved = 0; i < POOL_SIZE; i++) {
			struct sheep_object *sheep = &pool->mem[i];

			if (sheep->data & 1) {
				sheep->data &= ~1;
				continue;
			}

			if (sheep->type->free)
				sheep->type->free(sheep);

			sheep->data = (unsigned long)pool->free;
			pool->free = &pool->mem[i];
			pool->nr_used--;
			moved++;
		}

		if (!moved)
			continue;

		if (last)
			last->next = pool->next;
		else
			objects->fulls = pool->next;

		if (moved < POOL_SIZE || !cache) {
			pool->next = objects->parts;
			objects->parts = cache = pool;
		} else
			free_pool(pool);
	}

	if (!objects->parts)
		objects->parts = alloc_pool();
}

static sheep_t alloc(struct sheep_objects *objects)
{
	struct sheep_object *sheep = objects->parts->free;

	if (++objects->parts->nr_used < POOL_SIZE)
		objects->parts->free = (struct sheep_object *)sheep->data;
	else {
		struct sheep_object_pool *pool;

		pool = objects->parts;
		objects->parts = objects->parts->next;
		pool->next = objects->fulls;
		objects->fulls = pool;
	}
	return sheep;
}

sheep_t sheep_make(struct sheep_vm *vm, struct sheep_type *type, void *data)
{
	struct sheep_object *sheep;

	if (!vm->objects.parts)
		collect(vm);

	sheep = alloc(&vm->objects);
	sheep->type = type;
	sheep->data = (unsigned long)data;
	return sheep;
}

void sheep_mark(sheep_t sheep)
{
	if (sheep->data & 1)
		return;
	sheep->data |= 1;
	if (sheep->type->mark)
		sheep->type->mark(sheep);
}

void sheep_protect(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_vector_push(&vm->objects.protected, sheep);
}

void sheep_unprotect(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_t prot;

	prot = sheep_vector_pop(&vm->objects.protected);
	assert(prot == sheep);
}

void sheep_gc_disable(struct sheep_vm *vm)
{
	vm->objects.gc_disabled = 1;
}

void sheep_gc_enable(struct sheep_vm *vm)
{
	vm->objects.gc_disabled = 0;
}

