/*
 * include/sheep/compile.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_COMPILE_H
#define _SHEEP_COMPILE_H

#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/map.h>
#include <sheep/vm.h>

struct sheep_context {
	struct sheep_map *env;
#define SHEEP_CONTEXT_FUNCTION	1
#define SHEEP_CONTEXT_TAILFORM	2
	unsigned int flags;
	struct sheep_context *parent;
};

sheep_t __sheep_compile(struct sheep_vm *, struct sheep_module *, sheep_t);

static inline sheep_t sheep_compile(struct sheep_vm *vm, sheep_t exp)
{
	return __sheep_compile(vm, &vm->main, exp);
}

static inline unsigned int sheep_slot_constant(struct sheep_vm *vm, sheep_t obj)
{
	return sheep_vector_push(&vm->globals, obj);
}

static inline unsigned int sheep_slot_global(struct sheep_vm *vm)
{
	return sheep_vector_push(&vm->globals, NULL);
}

static inline unsigned int sheep_slot_local(struct sheep_function *function)
{
	return function->nr_locals++;
}

int sheep_compile_constant(struct sheep_vm *, struct sheep_function *,
			struct sheep_context *, sheep_t);
int sheep_compile_name(struct sheep_vm *, struct sheep_function *,
		struct sheep_context *, sheep_t);
int sheep_compile_set(struct sheep_vm *, struct sheep_function *,
		struct sheep_context *, sheep_t);
int sheep_compile_list(struct sheep_vm *, struct sheep_function *,
		struct sheep_context *, sheep_t);

#endif /* _SHEEP_COMPILE_H */
