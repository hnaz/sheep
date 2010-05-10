/*
 * sheep/object.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/vector.h>
#include <sheep/util.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

#include <sheep/object.h>

sheep_t sheep_make_object(struct sheep_vm *vm, const struct sheep_type *type,
			void *data)
{
	struct sheep_object *sheep;

	sheep = sheep_gc_alloc(vm);
	sheep->type = type;
	sheep->data = (unsigned long)data;
	return sheep;
}

int sheep_test(sheep_t sheep)
{
	if (sheep_type(sheep)->test)
		return sheep_type(sheep)->test(sheep);
	return 1;
}

int sheep_equal(sheep_t a, sheep_t b)
{
	if (a == b)
		return 1;
	if (sheep_type(a) != sheep_type(b))
		return 0;
	if (sheep_type(a)->equal)
		return sheep_type(a)->equal(a, b);
	return 0;
}

static int test_nil(sheep_t sheep)
{
	return 0;
}

static void format_nil(sheep_t sheep, struct sheep_strbuf *sb, int repr)
{
	sheep_strbuf_addf(sb, "nil");
}

const struct sheep_type sheep_nil_type = {
	.name = "nil",
	.compile = sheep_compile_constant,
	.test = test_nil,
	.format = format_nil,
};

struct sheep_object sheep_nil = {
	.type = &sheep_nil_type,
};

void sheep_object_builtins(struct sheep_vm *vm)
{
	sheep_vm_variable(vm, "nil", &sheep_nil);
}
