/*
 * sheep/number.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/unpack.h>
#include <sheep/bool.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/number.h>

static int number_test(sheep_t sheep)
{
	return !!sheep_fixnum(sheep);
}

static void number_format(sheep_t sheep, struct sheep_strbuf *sb, int repr)
{
	sheep_strbuf_addf(sb, "%ld", sheep_fixnum(sheep));
}

const struct sheep_type sheep_number_type = {
	.name = "number",
	.compile = sheep_compile_constant,
	.test = number_test,
	/*
	 * Fixnums are encoded in the reference pointer, equality is
	 * thus checked directly in sheep_equal().
	 */
	.format = number_format,
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
static sheep_t builtin_number(struct sheep_vm *vm, unsigned int nr_args)
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
			sheep_error(vm, "can not convert \"%s\"", str);
			return NULL;
		}
	}

	sheep_error(vm, "can not convert `%s'", sheep_type(sheep)->name);
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
static sheep_t builtin_less(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_cmp(vm, nr_args, "<", LESS);
}

/* (<= a b) */
static sheep_t builtin_lesseq(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_cmp(vm, nr_args, "<=", LESSEQ);
}

/* (>= a b) */
static sheep_t builtin_moreeq(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_cmp(vm, nr_args, ">=", MOREEQ);
}

/* (> a b) */
static sheep_t builtin_more(struct sheep_vm *vm, unsigned int nr_args)
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
static sheep_t builtin_plus(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "+");
}

/* (- a &optional b) */
static sheep_t builtin_minus(struct sheep_vm *vm, unsigned int nr_args)
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
static sheep_t builtin_multiply(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "*");
}

/* (/ a b) */
static sheep_t builtin_divide(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "/");
}

/* (% a b) */
static sheep_t builtin_modulo(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "%");
}

/* (~ number) */
static sheep_t builtin_lnot(struct sheep_vm *vm, unsigned int nr_args)
{
	long number;

	if (sheep_unpack_stack("~", vm, nr_args, "N", &number))
		return NULL;

	return sheep_make_number(vm, ~number);
}

/* (| a b) */
static sheep_t builtin_lor(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "|");
}

/* (& a b) */
static sheep_t builtin_land(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "&");
}

/* (^ a b) */
static sheep_t builtin_lxor(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "^");
}

/* (<< a b) */
static sheep_t builtin_shiftl(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, "<<");
}

/* (>> a b) */
static sheep_t builtin_shiftr(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_binop(vm, nr_args, ">>");
}

void sheep_number_builtins(struct sheep_vm *vm)
{
	sheep_vm_function(vm, "number", builtin_number);

	sheep_vm_function(vm, "<", builtin_less);
	sheep_vm_function(vm, "<=", builtin_lesseq);
	sheep_vm_function(vm, ">=", builtin_moreeq);
	sheep_vm_function(vm, ">", builtin_more);

	sheep_vm_function(vm, "+", builtin_plus);
	sheep_vm_function(vm, "-", builtin_minus);
	sheep_vm_function(vm, "*", builtin_multiply);
	sheep_vm_function(vm, "/", builtin_divide);
	sheep_vm_function(vm, "%", builtin_modulo);

	sheep_vm_function(vm, "~", builtin_lnot);
	sheep_vm_function(vm, "|", builtin_lor);
	sheep_vm_function(vm, "&", builtin_land);
	sheep_vm_function(vm, "^", builtin_lxor);

	sheep_vm_function(vm, "<<", builtin_shiftl);
	sheep_vm_function(vm, ">>", builtin_shiftr);
}
