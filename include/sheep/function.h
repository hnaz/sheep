/*
 * include/sheep/function.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
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
	 * memory location.
	 *
	 * Also see the comment in eval.c.
	 */
	struct sheep_vector *foreigns;
};

extern const struct sheep_type sheep_function_type;

sheep_t sheep_make_function(struct sheep_vm *);

/*
 * Having function->foreigns always heap allocated and thus always
 * aligned to more than a single byte, the least significant bit is
 * used as a flag for whether the foreign slots vector actually refers
 * to value slots or just contains lexical distance information.
 */
static inline void sheep_activate_closure(struct sheep_function *function)
{
	unsigned long value;

	value = (unsigned long)function->foreigns | 1;
	function->foreigns = (struct sheep_vector *)value;
}

static inline int sheep_active_closure(struct sheep_function *function)
{
	unsigned long value;

	value = (unsigned long)function->foreigns;
	return value & 1;
}

static inline struct sheep_vector *
sheep_foreigns(struct sheep_function *function)
{
	unsigned long value;

	value = (unsigned long)function->foreigns & ~1;
	return (struct sheep_vector *)value;
}

#endif /* _SHEEP_FUNCTION_H */
