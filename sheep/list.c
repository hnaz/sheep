/*
 * sheep/list.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/unpack.h>
#include <sheep/eval.h>
#include <sheep/util.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <stdio.h>

#include <sheep/list.h>

static unsigned long list_length(sheep_t sheep)
{
	struct sheep_list *list;
	unsigned long len;

	list = sheep_list(sheep);
	for (len = 0; list->head; list = sheep_list(list->tail))
		len++;
	return len;
}

static sheep_t do_list_concat(struct sheep_vm *vm, sheep_t base, sheep_t tail)
{
	struct sheep_list *pos;

	for (pos = sheep_list(tail); pos->head; pos = sheep_list(pos->tail)) {
		struct sheep_list *node;

		node = sheep_list(base);
		node->head = pos->head;
		node->tail = sheep_make_cons(vm, NULL, NULL);
		base = node->tail;
	}
	return base;
}

static sheep_t list_concat(struct sheep_vm *vm, sheep_t a, sheep_t b)
{
	sheep_t result, pos;

	sheep_protect(vm, a);
	sheep_protect(vm, b);

	result = pos = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, result);

	pos = do_list_concat(vm, result, a);
	do_list_concat(vm, pos, b);

	sheep_unprotect(vm, result);
	sheep_unprotect(vm, b);
	sheep_unprotect(vm, a);

	return result;
}

static sheep_t list_reverse(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_list *old;
	sheep_t new;

	sheep_protect(vm, sheep);

	new = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, new);

	for (old = sheep_list(sheep); old->head; old = sheep_list(old->tail)) {
		sheep_t new_head;

		new_head = sheep_make_cons(vm, old->head, new);

		sheep_unprotect(vm, new);
		sheep_protect(vm, new_head);

		new = new_head;
	}

	sheep_unprotect(vm, new);
	sheep_unprotect(vm, sheep);

	return new;
}

static sheep_t list_nth(struct sheep_vm *vm, unsigned long n, sheep_t sheep)
{
	struct sheep_list *list;

	list = sheep_list(sheep);
	while (n-- && list->head) {
		sheep = list->tail;
		list = sheep_list(sheep);
	}
	if (list->head)
		sheep = list->head;
	return sheep;
}

static sheep_t list_position(struct sheep_vm *vm, sheep_t item, sheep_t sheep)
{
	unsigned long position = 0;
	struct sheep_list *list;

	list = sheep_list(sheep);
	while (list->head) {
		if (sheep_equal(item, list->head))
			return sheep_make_number(vm, position);
		position++;
		list = sheep_list(list->tail);
	}

	return &sheep_nil;
}

static const struct sheep_sequence list_sequence = {
	.length = list_length,
	.concat = list_concat,
	.reverse = list_reverse,
	.nth = list_nth,
	.position = list_position,
};

static void mark_list(sheep_t sheep)
{
	struct sheep_list *list;

	list = sheep_list(sheep);
	if (!list->head)
		return;
	sheep_mark(list->head);
	/*
	 * Usually, list->head and list->tail are both NULL or both
	 * set.  But to facilitate simpler, non-atomic construction
	 * sites, handle garbage collection cycles between setting the
	 * head and setting the tail.
	 */
	if (list->tail)
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

static void format_list(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	struct sheep_list *list;

	list = sheep_list(sheep);
	sheep_addprintf(bufp, posp, "(");
	while (list->head) {
		sheep = list->head;
		__sheep_format(sheep, bufp, posp, 1);
		list = sheep_list(list->tail);
		if (!list->head)
			break;
		sheep_addprintf(bufp, posp, " ");
	}
	sheep_addprintf(bufp, posp, ")");
}

const struct sheep_type sheep_list_type = {
	.name = "list",
	.mark = mark_list,
	.free = free_list,
	.compile = sheep_compile_list,
	.test = test_list,
	.equal = equal_list,
	.sequence = &list_sequence,
	.format = format_list,
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
	struct sheep_list *p;
	sheep_t list;
	va_list ap;

	list = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, list);

	p = sheep_list(list);

	va_start(ap, nr);
	while (nr--) {
		p->head = va_arg(ap, sheep_t);
		p->tail = sheep_make_cons(vm, NULL, NULL);
		p = sheep_list(p->tail);
	}
	va_end(ap);

	sheep_unprotect(vm, list);
	return list;
}

int sheep_list_search(struct sheep_list *list, sheep_t object,
		unsigned long *offset)
{
	while (list->head) {
		if (list->head == object)
			return 1;
		(*offset)++;
		if (sheep_type(list->head) == &sheep_list_type)
			if (sheep_list_search(sheep_list(list->head),
						object, offset))
				return 1;
		list = sheep_list(list->tail);
	}
	return 0;
}

/* (cons item list) */
static sheep_t builtin_cons(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t item, list, new;

	new = sheep_make_cons(vm, NULL, NULL);

	if (sheep_unpack_stack("cons", vm, nr_args, "ol", &item, &list))
		return NULL;

	sheep_list(new)->head = item;
	sheep_list(new)->tail = list;

	return new;
}

/* (list expr*) */
static sheep_t builtin_list(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t list;

	list = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, list);

	while (nr_args--) {
		sheep_t new;

		new = sheep_make_cons(vm, NULL, list);
		sheep_list(new)->head = sheep_vector_pop(&vm->stack);

		sheep_unprotect(vm, list);
		sheep_protect(vm, new);

		list = new;
	}

	sheep_unprotect(vm, list);
	return list;
}

/* (head list) */
static sheep_t builtin_head(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_list *list;
	sheep_t sheep;

	if (sheep_unpack_stack("head", vm, nr_args, "l", &sheep))
		return NULL;

	list = sheep_list(sheep);
	if (list->head)
		return list->head;
	return sheep;
}

/* (tail list) */
static sheep_t builtin_tail(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_list *list;
	sheep_t sheep;

	if (sheep_unpack_stack("tail", vm, nr_args, "l", &sheep))
		return NULL;

	list = sheep_list(sheep);
	if (list->head)
		return list->tail;
	return sheep;
}

/* (find predicate list) */
static sheep_t builtin_find(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t predicate, list_, result = &sheep_nil;
	struct sheep_list *list;

	if (sheep_unpack_stack("find", vm, nr_args, "cl", &predicate, &list_))
		return NULL;
	sheep_protect(vm, predicate);
	sheep_protect(vm, list_);

	list = sheep_list(list_);
	while (list->head) {
		sheep_t val;

		val = sheep_call(vm, predicate, 1, list->head);
		if (sheep_test(val)) {
			result = list->head;
			break;
		}

		list = sheep_list(list->tail);
	}

	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, predicate);
	return result;
}

/* (apply function list) */
static sheep_t builtin_apply(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_list *list;
	sheep_t callable;

	if (sheep_unpack_stack("apply", vm, nr_args, "cL", &callable, &list))
		return NULL;

	return sheep_apply(vm, callable, list);
}

/* (map function list) */
static sheep_t builtin_map(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t mapper, old, new, result = NULL;
	struct sheep_list *l_old, *l_new;

	if (sheep_unpack_stack("map", vm, nr_args, "cl", &mapper, &old))
		return NULL;
	sheep_protect(vm, mapper);
	sheep_protect(vm, old);

	new = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, new);

	l_old = sheep_list(old);
	l_new = sheep_list(new);

	while (l_old->head) {
		l_new->head = sheep_call(vm, mapper, 1, l_old->head);
		if (!l_new->head)
			goto out;
		l_new->tail = sheep_make_cons(vm, NULL, NULL);
		l_new = sheep_list(l_new->tail);
		l_old = sheep_list(l_old->tail);
	}
	result = new;
out:
	sheep_unprotect(vm, new);
	sheep_unprotect(vm, old);
	sheep_unprotect(vm, mapper);

	return result;
}

/* (reduce function list) */
static sheep_t builtin_reduce(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t reducer, list_, a, b, value, result = NULL;
	struct sheep_list *list;

	if (sheep_unpack_stack("reduce", vm, nr_args, "cl", &reducer, &list_))
		return NULL;

	sheep_protect(vm, reducer);
	sheep_protect(vm, list_);

	list = sheep_list(list_);
	if (sheep_unpack_list("reduce", list, "oor", &a, &b, &list))
		goto out;

	value = sheep_call(vm, reducer, 2, a, b);
	if (!value)
		goto out;

	while (list->head) {
		value = sheep_call(vm, reducer, 2, value, list->head);
		if (!value)
			goto out;
		list = sheep_list(list->tail);
	}
	result = value;
out:
	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, reducer);

	return result;
}

void sheep_list_builtins(struct sheep_vm *vm)
{
	sheep_vm_function(vm, "cons", builtin_cons);
	sheep_vm_function(vm, "list", builtin_list);
	sheep_vm_function(vm, "head", builtin_head);
	sheep_vm_function(vm, "tail", builtin_tail);
	sheep_vm_function(vm, "find", builtin_find);
	sheep_vm_function(vm, "apply", builtin_apply);
	sheep_vm_function(vm, "map", builtin_map);
	sheep_vm_function(vm, "reduce", builtin_reduce);
}
