/*
 * sheep/name.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

#include <sheep/name.h>

static void free_name(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_free(sheep_cname(sheep));
}

static int equal_name(sheep_t a, sheep_t b)
{
	return !strcmp(sheep_cname(a), sheep_cname(b));
}

static void format_name(sheep_t sheep, char **bufp, size_t *posp)
{
	sheep_addprintf(bufp, posp, "%s", sheep_cname(sheep));
}

const struct sheep_type sheep_name_type = {
	.name = "name",
	.free = free_name,
	.compile = sheep_compile_name,
	.equal = equal_name,
	.format = format_name,
};

sheep_t sheep_make_name(struct sheep_vm *vm, const char *cname)
{
	return sheep_make_object(vm, &sheep_name_type, sheep_strdup(cname));
}
