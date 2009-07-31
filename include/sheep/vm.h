#ifndef _SHEEP_VM_H
#define _SHEEP_VM_H

#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/block.h>

struct sheep_vm {
	struct sheep_objects objects;
	struct sheep_code code;
	struct sheep_vector globals;	/* sheep_t [] */
	struct sheep_module main;
	struct sheep_vector stack;	/* sheep_t [] */
	struct sheep_vector calls;	/* [lastpc lastblock] */
	struct sheep_block *block;
};

#endif /* _SHEEP_VM_H */
