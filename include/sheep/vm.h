/*
 * include/sheep/vm.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_VM_H
#define _SHEEP_VM_H

#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/code.h>

struct sheep_vm {
	/* Object management */
	struct sheep_objects *fulls;
	struct sheep_objects *parts;
	struct sheep_vector protected;
	int gc_disabled;

	struct sheep_vector globals;
	struct sheep_code code;

	/* Compiler */
	struct sheep_map specials;
	struct sheep_module main;

	/* Evaluator */
	struct sheep_vector stack;
	struct sheep_vector calls;	/* [lastpc lastbasep lastfunction] */
};

void sheep_vm_init(struct sheep_vm *);
void sheep_vm_exit(struct sheep_vm *);

void sheep_vm_mark(struct sheep_vm *);

#endif /* _SHEEP_VM_H */
