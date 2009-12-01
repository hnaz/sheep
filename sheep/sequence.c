/*
 * sheep/sequence.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/number.h>
#include <sheep/object.h>
#include <sheep/core.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/sequence.h>

/* (length sequence) */
static sheep_t eval_length(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned int len;
	sheep_t seq;
	
	if (sheep_unpack_stack("length", vm, nr_args, "q", &seq))
		return NULL;

	len = sheep_sequence(seq)->length(seq);
	return sheep_make_number(vm, len);
}

/* (concat a b) */
static sheep_t eval_concat(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t a, b;

	if (sheep_unpack_stack("concat", vm, nr_args, "qq", &a, &b))
		return NULL;

	if (sheep_type(a) != sheep_type(b)) {
		fprintf(stderr, "concat: can not concat %s and %s\n",
			sheep_type(a)->name, sheep_type(b)->name);
		return NULL;
	}

	return sheep_sequence(a)->concat(vm, a, b);
}

/* (reverse sequence) */
static sheep_t eval_reverse(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("reverse", vm, nr_args, "q", &sheep))
		return NULL;

	return sheep_sequence(sheep)->reverse(vm, sheep);
}

void sheep_sequence_builtins(struct sheep_vm *vm)
{
	sheep_vm_function(vm, "length", eval_length);
	sheep_vm_function(vm, "concat", eval_concat);
	sheep_vm_function(vm, "reverse", eval_reverse);
}
