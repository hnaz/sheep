#include <sheep/vector.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>

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

static void mark_protected(struct sheep_vector *protected)
{
	unsigned long i;

	for (i = 0; i < protected->nr_items; i++)
		sheep_mark(protected->items[i]);
}

static void collect(struct sheep_vm *vm)
{
	struct sheep_objects *pool, *cache = NULL, *last = NULL;

	if (vm->gc_disabled)
		goto alloc;
	
	sheep_mark_vm(vm);
	mark_protected(&vm->protected);

	for (pool = vm->fulls; pool; last = pool, pool = pool->next) {
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
			vm->fulls = pool->next;

		if (moved < POOL_SIZE || !cache) {
			pool->next = vm->parts;
			vm->parts = cache = pool;
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

sheep_t sheep_object(struct sheep_vm *vm, struct sheep_type *type, void *data)
{
	struct sheep_object *sheep;

	if (!vm->parts)
		collect(vm);

	sheep = alloc(vm);
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
	sheep_vector_push(&vm->protected, sheep);
}

void sheep_unprotect(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_t prot;

	prot = sheep_vector_pop(&vm->protected);
	assert(prot == sheep);
}

void sheep_gc_disable(struct sheep_vm *vm)
{
	vm->gc_disabled = 1;
}

void sheep_gc_enable(struct sheep_vm *vm)
{
	vm->gc_disabled = 0;
}
