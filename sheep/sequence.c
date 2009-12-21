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

/* (concat a b) */
static sheep_t builtin_concat(struct sheep_vm *vm, unsigned int nr_args)
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
static sheep_t builtin_reverse(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t seq;

	if (sheep_unpack_stack("reverse", vm, nr_args, "q", &seq))
		return NULL;

	return sheep_sequence(seq)->reverse(vm, seq);
}

static sheep_t builtin_nth(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned long n;
	sheep_t seq;

	if (sheep_unpack_stack("nth", vm, nr_args, "Nq", &n, &seq))
		return NULL;

	return sheep_sequence(seq)->nth(vm, n, seq);
}

void sheep_sequence_builtins(struct sheep_vm *vm)
{
	sheep_vm_function(vm, "length", builtin_length);
	sheep_vm_function(vm, "concat", builtin_concat);
	sheep_vm_function(vm, "reverse", builtin_reverse);
	sheep_vm_function(vm, "nth", builtin_nth);
}
