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

static void free_list(sheep_t sheep)
{
	struct sheep_list *list, *tail;

	list = sheep_data(sheep);
	while (list) {
		tail = list->tail;
		sheep_free(list);
		list = tail;
	}
}

static void ddump_list(sheep_t sheep)
{
	struct sheep_list *list;

	list = sheep_data(sheep);
	printf("(");
	for (;;) {
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
	.ddump = ddump_list,
};

sheep_t __sheep_list(struct sheep_vm *vm, sheep_t head, struct sheep_list *tail)
{
	struct sheep_list *list;

	list = sheep_malloc(sizeof(struct sheep_list));
	list->head = head;
	list->tail = tail;

	return sheep_object(vm, &sheep_list_type, list);
}

sheep_t sheep_list(struct sheep_vm *vm, unsigned int nr, ...)
{
	struct sheep_list *list, *pos;
	va_list ap;

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
