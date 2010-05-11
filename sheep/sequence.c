/*
 * sheep/sequence.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/number.h>
#include <sheep/object.h>
#include <sheep/unpack.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/sequence.h>

/* (length sequence) */
static sheep_t builtin_length(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned int len;
	sheep_t seq;

	if (sheep_unpack_stack("length", vm, nr_args, "q", &seq))
		return NULL;

	len = sheep_sequence(seq)->length(seq);
	return sheep_make_number(vm, len);
}

/* (concat first &rest rest) */
static sheep_t builtin_concat(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t object;

	if (!nr_args) {
		fprintf(stderr, "concat: too few arguments\n");
		return NULL;
	}

	object = vm->stack.items[vm->stack.nr_items - nr_args];
	if (sheep_unpack("concat", object, 'q', &object))
		return NULL;

	return sheep_sequence(object)->concat(vm, object, nr_args);
}

/* (reverse sequence) */
static sheep_t builtin_reverse(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t seq;

	if (sheep_unpack_stack("reverse", vm, nr_args, "q", &seq))
		return NULL;

	return sheep_sequence(seq)->reverse(vm, seq);
}

/* (nth index sequence) */
static sheep_t builtin_nth(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned long n;
	sheep_t seq;

	if (sheep_unpack_stack("nth", vm, nr_args, "Nq", &n, &seq))
		return NULL;

	return sheep_sequence(seq)->nth(vm, n, seq);
}

/* (position item sequence) */
static sheep_t builtin_position(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t item, seq;

	if (sheep_unpack_stack("position", vm, nr_args, "oq", &item, &seq))
		return NULL;

	return sheep_sequence(seq)->position(vm, item, seq);
}

void sheep_sequence_builtins(struct sheep_vm *vm)
{
	sheep_vm_function(vm, "length", builtin_length);
	sheep_vm_function(vm, "concat", builtin_concat);
	sheep_vm_function(vm, "reverse", builtin_reverse);
	sheep_vm_function(vm, "nth", builtin_nth);
	sheep_vm_function(vm, "position", builtin_position);
}
