/*
 * sheep/vm.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/sequence.h>
#include <sheep/number.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/alien.h>
#include <sheep/bool.h>
#include <sheep/core.h>
#include <sheep/eval.h>
#include <sheep/list.h>
#include <sheep/type.h>
#include <sheep/util.h>
#include <sheep/gc.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <sheep/vm.h>

#define DEFAULT_ERRBUF		128

void sheep_error(struct sheep_vm *vm, const char *fmt, ...)
{
	va_list ap;
	size_t len;

	sheep_bug_on(vm->error);
	vm->error = sheep_malloc(DEFAULT_ERRBUF);

	va_start(ap, fmt);
	len = vsnprintf(vm->error, DEFAULT_ERRBUF, fmt, ap);
	va_end(ap);

	if (len >= DEFAULT_ERRBUF) {
		vm->error = sheep_realloc(vm->error, len + 1);
		va_start(ap, fmt);
		vsprintf(vm->error, fmt, ap);
		va_end(ap);
	}
}

void sheep_report_error(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_bug_on(!vm->error);

	if (sheep) {
		char *context;

		context = sheep_format(sheep);
		fprintf(stderr, "%s: ", context);
		sheep_free(context);
	}
	fprintf(stderr, "%s\n", vm->error);

	sheep_free(vm->error);
	vm->error = NULL;
}

unsigned int sheep_vm_key(struct sheep_vm *vm, const char *key)
{
	unsigned int slot = 0;

	if (vm->keys) {
		while (vm->keys[slot]) {
			if (!strcmp(vm->keys[slot], key))
				return slot;
			slot++;
		}
	}

	vm->keys = sheep_realloc(vm->keys, sizeof(char *) * (slot + 2));
	vm->keys[slot] = sheep_strdup(key);
	vm->keys[slot + 1] = NULL;
	return slot;
}

static void drain_keys(struct sheep_vm *vm)
{
	unsigned int slot;

	if (!vm->keys)
		return;

	for (slot = 0; vm->keys[slot]; slot++)
		sheep_free(vm->keys[slot]);

	sheep_free(vm->keys);
}

void sheep_vm_mark(struct sheep_vm *vm)
{
	unsigned int i;

	for (i = 0; i < vm->globals.nr_items; i++)
		if (vm->globals.items[i])
			sheep_mark(vm->globals.items[i]);

	for (i = 0; i < vm->stack.nr_items; i++)
		if (vm->stack.items[i])
			sheep_mark(vm->stack.items[i]);

	for (i = 2; i < vm->calls.nr_items; i += 3)
		sheep_mark(vm->calls.items[i]);
}

unsigned int sheep_vm_variable(struct sheep_vm *vm,
			       const char *name,
			       sheep_t value)
{
	unsigned int slot;

	slot = sheep_vm_constant(vm, value);
	sheep_map_set(&vm->builtins, name, (void *)(unsigned long)slot);
	return slot;
}

void sheep_vm_function(struct sheep_vm *vm, const char *name, sheep_alien_t f)
{
	sheep_vm_variable(vm, name, sheep_make_alien(vm, f, name));
}

static void setup_argv(struct sheep_vm *vm, int ac, char **av)
{
	struct sheep_list *p;
	sheep_t list;

	list = sheep_make_cons(vm, NULL, NULL);
	sheep_protect(vm, list);

	p = sheep_list(list);
	while (ac--) {
		p->head = sheep_make_string(vm, *av);
		p->tail = sheep_make_cons(vm, NULL, NULL);
		p = sheep_list(p->tail);
		av++;
	}

	sheep_vm_variable(vm, "argv", list);
	sheep_unprotect(vm, list);
}

void sheep_vm_init(struct sheep_vm *vm, int ac, char **av)
{
	memset(vm, 0, sizeof(*vm));
	sheep_core_init(vm);
	sheep_object_builtins(vm);
	sheep_bool_builtins(vm);
	sheep_number_builtins(vm);
	sheep_string_builtins(vm);
	sheep_list_builtins(vm);
	sheep_sequence_builtins(vm);
	sheep_function_builtins(vm);
	sheep_module_builtins(vm);
	setup_argv(vm, ac, av);
}

void sheep_vm_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->builtins);
	sheep_core_exit(vm);
	sheep_evaluator_exit(vm);
	sheep_free(vm->globals.items);
	drain_keys(vm);
	sheep_gc_exit(vm);
}
