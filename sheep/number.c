/*
 * sheep/number.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/bool.h>
#include <sheep/core.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/number.h>

static int test_number(sheep_t sheep)
{
	return !!sheep_fixnum(sheep);
}

static int equal_number(sheep_t a, sheep_t b)
{
	return a == b;
}

static void format_number(sheep_t sheep, char **bufp, size_t *posp)
{
	sheep_addprintf(bufp, posp, "%ld", sheep_fixnum(sheep));
}

const struct sheep_type sheep_number_type = {
	.name = "number",
	.compile = sheep_compile_constant,
	.test = test_number,
	.equal = equal_number,
	.format = format_number,
};

sheep_t sheep_make_number(struct sheep_vm *vm, long number)
{
	return (sheep_t)((number << 1) | 1);
}

int sheep_parse_number(struct sheep_vm *vm, const char *buf, sheep_t *sheepp)
{
	long number, min, max;
	char *end;

	number = strtol(buf, &end, 0);
	if (*end)
		return -1;
	min = LONG_MIN >> 1;
	max = LONG_MAX >> 1;
	if (number < min || number > max)
		return -2;
	*sheepp = sheep_make_number(vm, number);
	return 0;
}

/* (number expression) */
static sheep_t eval_number(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("number", vm, nr_args, "o", &sheep))
		return NULL;

	if (sheep_type(sheep) == &sheep_number_type)
		return sheep;

	if (sheep_type(sheep) == &sheep_string_type) {
		const char *str = sheep_rawstring(sheep);
		sheep_t number;

		switch (sheep_parse_number(vm, str, &number)) {
		case 0:
			return number;
		default:
			fprintf(stderr, "number: can not convert \"%s\"\n",
				str);
			return NULL;
		}
	}

	fprintf(stderr, "number: can not convert %s\n",
		sheep_type(sheep)->name);
	return NULL;
}

enum relation {
	LESS,
	LESSEQ,
	MOREEQ,
	MORE,
};

static sheep_t do_cmp(struct sheep_vm *vm, unsigned int nr_args,
		const char *name, enum relation relation)
{
	int result = result;
	long a, b;

	if (sheep_unpack_stack(name, vm, nr_args, "NN", &a, &b))
		return NULL;

	switch (relation) {
	case LESS:
		result = a < b;
		break;
	case LESSEQ:
		result = a <= b;
		break;
	case MOREEQ:
		result = a >= b;
		break;
	case MORE:
		result = a > b;
		break;
	}

	if (result)
		return &sheep_true;
	return &sheep_false;
}

/* (< a b) */
static sheep_t eval_less(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_cmp(vm, nr_args, "<", LESS);
}

/* (<= a b) */
static sheep_t eval_lesseq(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_cmp(vm, nr_args, "<=", LESSEQ);
}

/* (>= a b) */
static sheep_t eval_moreeq(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_cmp(vm, nr_args, ">=", MOREEQ);
}

/* (> a b) */
static sheep_t eval_more(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_cmp(vm, nr_args, ">", MORE);
}

static sheep_t do_binop(struct sheep_vm *vm, unsigned int nr_args,
			const char *operation)
{
	long value, a, b;

	if (sheep_unpack_stack(operation, vm, nr_args, "NN", &a, &b))
		return NULL;

	switch (*operation) {
	case '+':
		value = a + b;
		break;
	case '-':
		value = a - b;
		break;
	case '*':
		value = a * b;
		break;
	case '/':
		value = a / b;
		break;
	case '%':
		value = a % b;
		break;
	case '|':
		value = a | b;
		break;
	case '&':
		value = a & b;
		break;
	case '^':
		value = a ^ b;
		break;
	case '<':
		value = a << b;
		break;
	case '>':
		value = a >> b;
		break;
	default:
		sheep_bug("unknown arithmetic operation");
	}

	return sheep_make_number(vm, value);
}

/* (+ a b) */
static sheep_t eval_plus(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "+");
}

/* (- a &optional b) */
static sheep_t eval_minus(struct sheep_vm *vm, unsigned int nr_args)
{
	if (nr_args == 1) {
		long number;

		if (sheep_unpack_stack("-", vm, nr_args, "N", &number))
			return NULL;
		return sheep_make_number(vm, -number);
	}

	return do_binop(vm, nr_args, "-");
}

/* (* a b) */
static sheep_t eval_multiply(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "*");
}

/* (/ a b) */
static sheep_t eval_divide(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "/");
}

/* (% a b) */
static sheep_t eval_modulo(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "%");
}

/* (~ number) */
static sheep_t eval_lnot(struct sheep_vm *vm, unsigned int nr_args)
{
	long number;

	if (sheep_unpack_stack("~", vm, nr_args, "N", &number))
		return NULL;

	return sheep_make_number(vm, ~number);
}

/* (| a b) */
static sheep_t eval_lor(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "|");
}

/* (& a b) */
static sheep_t eval_land(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "&");
}

/* (^ a b) */
static sheep_t eval_lxor(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "^");
}

/* (<< a b) */
static sheep_t eval_shiftl(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "<<");
}

/* (>> a b) */
static sheep_t eval_shiftr(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, ">>");
}

void sheep_number_builtins(struct sheep_vm *vm)
{
	sheep_module_function(vm, &vm->main, "number", eval_number);

	sheep_module_function(vm, &vm->main, "<", eval_less);
	sheep_module_function(vm, &vm->main, "<=", eval_lesseq);
	sheep_module_function(vm, &vm->main, ">=", eval_moreeq);
	sheep_module_function(vm, &vm->main, ">", eval_more);

	sheep_module_function(vm, &vm->main, "+", eval_plus);
	sheep_module_function(vm, &vm->main, "-", eval_minus);
	sheep_module_function(vm, &vm->main, "*", eval_multiply);
	sheep_module_function(vm, &vm->main, "/", eval_divide);
	sheep_module_function(vm, &vm->main, "%", eval_modulo);

	sheep_module_function(vm, &vm->main, "~", eval_lnot);
	sheep_module_function(vm, &vm->main, "|", eval_lor);
	sheep_module_function(vm, &vm->main, "&", eval_land);
	sheep_module_function(vm, &vm->main, "^", eval_lxor);

	sheep_module_function(vm, &vm->main, "<<", eval_shiftl);
	sheep_module_function(vm, &vm->main, ">>", eval_shiftr);
}
