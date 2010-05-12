/*
 * include/sheep/vm.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_VM_H
#define _SHEEP_VM_H

#include <sheep/function.h>
#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/alien.h>
#include <sheep/map.h>
#include <stdarg.h>

struct sheep_vm {
	/* Object management */
	struct sheep_objects *fulls;
	struct sheep_objects *parts;
	struct sheep_vector protected;
	int gc_disabled;

	char **keys;
	struct sheep_vector globals;

	/* Compiler */
	struct sheep_map specials;
	struct sheep_map builtins;
	struct sheep_module main;

	/* Evaluator */
	struct sheep_indirect *pending;
	struct sheep_vector stack;
	struct sheep_vector calls;	/* [lastpc lastbasep lastfunction] */
	char *error;
};

void sheep_error(struct sheep_vm *, const char *, ...);
void sheep_report_error(struct sheep_vm *, sheep_t);

unsigned int sheep_vm_key(struct sheep_vm *, const char *);

void sheep_vm_mark(struct sheep_vm *);

static inline unsigned int sheep_vm_constant(struct sheep_vm *vm, sheep_t sheep)
{
	return sheep_vector_push(&vm->globals, sheep);
}

static inline unsigned int sheep_vm_global(struct sheep_vm *vm)
{
	/*
	 * Default value for slots: bindings are compile-time static
	 * but initialization happens at runtime.  The only situation
	 * where this is needed is for interactive mode, when the
	 * initialization form fails evaluation and we continue with
	 * the slot bound.
	 */
	return sheep_vector_push(&vm->globals, &sheep_nil);
}

unsigned int sheep_vm_variable(struct sheep_vm *, const char *, sheep_t);
void sheep_vm_function(struct sheep_vm *, const char *, sheep_alien_t);

void sheep_vm_init(struct sheep_vm *, int, char **);
void sheep_vm_exit(struct sheep_vm *);

#endif /* _SHEEP_VM_H */
