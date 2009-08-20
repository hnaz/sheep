#ifndef _SHEEP_FUNCTION_H
#define _SHEEP_FUNCTION_H

#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/vm.h>

struct sheep_function {
	unsigned long offset;	/* in vm->code */
	unsigned int nr_parms;
	unsigned int nr_locals;	/* nr_parms + private slots */
	/*
	 * This is probably the most versatile field in the whole VM:
	 *
	 * After compilation, this contains pairs of (leveldistance,
	 * slotindex), where leveldistance is the relative lexical
	 * nesting distance from this function to the function owning
	 * the slot the foreign reference refers to.
	 *
	 * After loading the function with the CLOSURE opcode, this
	 * contains pointers to live stack slots.
	 *
	 * After the death of the original stack slots, the entries
	 * refer to the preserved stack slots sitting in a private
	 * array.
	 */
	struct sheep_vector *foreigns;
};

extern const struct sheep_type sheep_function_type;

sheep_t sheep_function(struct sheep_vm *);

#endif /* _SHEEP_FUNCTION_H */
