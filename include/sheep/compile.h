/*
 * include/sheep/compile.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_COMPILE_H
#define _SHEEP_COMPILE_H

#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/list.h>
#include <sheep/read.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stdarg.h>

sheep_t __sheep_compile(struct sheep_vm *, struct sheep_module *,
			struct sheep_expr *);

static inline sheep_t sheep_compile(struct sheep_vm *vm,
				struct sheep_expr *expr)
{
	return __sheep_compile(vm, &vm->main, expr);
}

struct sheep_compile {
	struct sheep_vm *vm;
	struct sheep_module *module;
	struct sheep_expr *expr;
};

struct sheep_context {
	struct sheep_map *env;
#define SHEEP_CONTEXT_FUNCTION	1
#define SHEEP_CONTEXT_TAILFORM	2
	unsigned int flags;
	struct sheep_context *parent;
};

/**
 * Compiler API call signature
 *
 * @compile: describes current compiler invocation
 * @function: describes current function
 * @context: describes current form lexically
 * @sheep: object to compile
 */

void sheep_compiler_error(struct sheep_compile *, sheep_t, const char *, ...);

static inline int sheep_compile_object(struct sheep_compile *compile,
				struct sheep_function *function,
				struct sheep_context *context, sheep_t sheep)
{
	return sheep_type(sheep)->compile(compile, function, context, sheep);
}

int sheep_compile_constant(struct sheep_compile *, struct sheep_function *,
			struct sheep_context *, sheep_t);
int sheep_compile_name(struct sheep_compile *, struct sheep_function *,
		struct sheep_context *, sheep_t);
int sheep_compile_set(struct sheep_compile *, struct sheep_function *,
		struct sheep_context *, sheep_t);
int sheep_compile_list(struct sheep_compile *, struct sheep_function *,
		struct sheep_context *, sheep_t);

int sheep_unpack_form(struct sheep_compile *, const char *,
		struct sheep_list *, const char *, ...);

#endif /* _SHEEP_COMPILE_H */
