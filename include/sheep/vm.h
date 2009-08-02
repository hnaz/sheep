#ifndef _SHEEP_VM_H
#define _SHEEP_VM_H

#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/block.h>
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
	struct sheep_vector calls;	/* [lastpc lastblock] */
	struct sheep_block *block;
};

static inline void sheep_mark_vm(struct sheep_vm *vm)
{
	/*
	 * vm->data
	 *
	 * vm->stack
	 *
	 * every block in vm->calls
	 *
	 * vm->block
	 */
}

#endif /* _SHEEP_VM_H */
