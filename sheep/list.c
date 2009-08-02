#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>

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

const struct sheep_type sheep_list_type = {
	.mark = mark_list,
	.free = free_list,
	.compile = sheep_compile_list,
};

sheep_t sheep_list(struct sheep_vm *vm, sheep_t head, struct sheep_list *tail)
{
	struct sheep_list *list;

	list = sheep_malloc(sizeof(struct sheep_list));
	list->head = head;
	list->tail = tail;

	return sheep_object(vm, &sheep_list_type, list);
}
