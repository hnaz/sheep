/*
 * sheep/list.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <stdio.h>

#include <sheep/list.h>

static void mark_list(sheep_t sheep)
{
	struct sheep_list *list;

	list = sheep_list(sheep);
	if (!list->head)
		return;
	sheep_mark(list->head);
	sheep_mark(list->tail);
}

static void free_list(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_free(sheep_list(sheep));
}

static int test_list(sheep_t sheep)
{
	return !!sheep_list(sheep)->head;
}

static int equal_list(sheep_t a, sheep_t b)
{
	struct sheep_list *la, *lb;

	la = sheep_list(a);
	lb = sheep_list(b);
	while (la->head && lb->head) {
		if (!sheep_equal(la->head, lb->head))
			return 0;
		la = sheep_list(la->tail);
		lb = sheep_list(lb->tail);
	}
	return !(la->head || lb->head);
}

static void ddump_list(sheep_t sheep)
{
	struct sheep_list *list;

	list = sheep_list(sheep);
	printf("(");
	while (list->head) {
		sheep = list->head;
		__sheep_ddump(sheep);
		list = sheep_list(list->tail);
		if (!list->head)
			break;
		printf(" ");
	}
	printf(")");
}

const struct sheep_type sheep_list_type = {
	.name = "list",
	.mark = mark_list,
	.free = free_list,
	.compile = sheep_compile_list,
	.test = test_list,
	.equal = equal_list,
	.ddump = ddump_list,
};

sheep_t sheep_make_cons(struct sheep_vm *vm, sheep_t head, sheep_t tail)
{
	struct sheep_list *list;

	list = sheep_malloc(sizeof(struct sheep_list));
	list->head = head;
	list->tail = tail;
	return sheep_make_object(vm, &sheep_list_type, list);
}

sheep_t sheep_make_list(struct sheep_vm *vm, unsigned int nr, ...)
{
	sheep_t list, pos;
	va_list ap;

	list = pos = sheep_make_cons(vm, NULL, NULL);

	va_start(ap, nr);
	while (nr--) {
		struct sheep_list *node;

		node = sheep_list(pos);
		node->head = va_arg(ap, sheep_t);
		pos = node->tail = sheep_make_cons(vm, NULL, NULL);
	}
	va_end(ap);
	return list;
}
