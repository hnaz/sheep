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

static inline int sheep_compile_object(struct sheep_vm *vm,
				struct sheep_function *function,
				struct sheep_context *context, sheep_t sheep)
{
	return sheep_type(sheep)->compile(vm, function, context, sheep);
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
