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

static void name_free(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_name *name;

	name = sheep_name(sheep);
	sheep_free(*name->parts);
	sheep_free(name->parts);
	sheep_free(name);
}

static int name_equal(sheep_t a, sheep_t b)
{
	struct sheep_name *na, *nb;
	unsigned int i;

	na = sheep_name(a);
	nb = sheep_name(b);

	if (na->nr_parts != nb->nr_parts)
		return 0;

	for (i = 0; i < na->nr_parts; i++)
		if (strcmp(na->parts[i], nb->parts[i]))
			return 0;
	return 1;
}

static void name_format(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	struct sheep_name *name;
	unsigned int i;

	name = sheep_name(sheep);
	sheep_addprintf(bufp, posp, "%s", name->parts[0]);
	for (i = 1; i < name->nr_parts; i++)
		sheep_addprintf(bufp, posp, ":%s", name->parts[i]);
}

const struct sheep_type sheep_name_type = {
	.name = "name",
	.free = name_free,
	.compile = sheep_compile_name,
	.equal = name_equal,
	.format = name_format,
};

sheep_t sheep_make_name(struct sheep_vm *vm, const char *string)
{
	struct sheep_name *name;
	char *work;

	name = sheep_malloc(sizeof(struct sheep_name));
	name->parts = sheep_malloc(sizeof(char *));
	name->parts[0] = work = sheep_strdup(string);
	name->nr_parts = 1;

	while (1) {
		char *p;

		p = strchr(work, ':');
		if (!p || p[1] == 0)
			return sheep_make_object(vm, &sheep_name_type, name);
		if (p == work) {
			work++;
			continue;
		}
		*p = 0;
		work = p + 1;
		name->nr_parts++;
		name->parts = sheep_realloc(name->parts,
					sizeof(char *) * name->nr_parts);
		name->parts[name->nr_parts - 1] = work;
	}
}
