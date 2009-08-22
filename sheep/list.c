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

	list = sheep_data(sheep);
	while (list) {
		sheep_mark(list->head);
		list = list->tail;
	}
}

static void free_list(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_list *list, *tail;

	list = sheep_data(sheep);
	while (list) {
		tail = list->tail;
		sheep_free(list);
		list = tail;
	}
}

static int test_list(sheep_t sheep)
{
	return !!sheep_data(sheep);
}

static int equal_list(sheep_t a, sheep_t b)
{
	struct sheep_list *la, *lb;

	la = sheep_data(a);
	lb = sheep_data(b);
	while (la && lb) {
		if (!sheep_equal(la->head, lb->head))
			return 0;
		la = la->tail;
		lb = lb->tail;
	}
	return !(la || lb);
}

static void ddump_list(sheep_t sheep)
{
	struct sheep_list *list;

	list = sheep_data(sheep);
	printf("(");
	while (list) {
		sheep = list->head;
		sheep->type->ddump(sheep);
		if (!list->tail)
			break;
		printf(" ");
		list = list->tail;
	}
	printf(")");
}

const struct sheep_type sheep_list_type = {
	.mark = mark_list,
	.free = free_list,
	.compile = sheep_compile_list,
	.test = test_list,
	.equal = equal_list,
	.ddump = ddump_list,
};

struct sheep_list *sheep_cons(struct sheep_vm *vm, sheep_t head,
			struct sheep_list *tail)
{
	struct sheep_list *list;

	list = sheep_malloc(sizeof(struct sheep_list));
	list->head = head;
	list->tail = tail;
	return list;
}

struct sheep_list *__sheep_list(struct sheep_vm *vm, sheep_t item)
{
	return sheep_cons(vm, item, NULL);
}

sheep_t sheep_list(struct sheep_vm *vm, unsigned int nr, ...)
{
	struct sheep_list *list, *pos;
	va_list ap;

	if (!nr)
		return sheep_object(vm, &sheep_list_type, NULL);

	va_start(ap, nr);
	list = pos = sheep_malloc(sizeof(struct sheep_list));
	for (;;) {
		pos->head = va_arg(ap, sheep_t);
		if (!--nr)
			break;
		pos->tail = sheep_malloc(sizeof(struct sheep_list));
		pos = pos->tail;
	}
	pos->tail = NULL;
	va_end(ap);
	return sheep_object(vm, &sheep_list_type, list);
}
