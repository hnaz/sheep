/*
 * sheep/alien.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/alien.h>

static void alien_free(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_free(sheep_data(sheep));
}

static void alien_format(sheep_t sheep, char **bufp, size_t *posp, int repr)
{
	struct sheep_alien *alien;

	alien = sheep_data(sheep);
	if (alien->name)
		sheep_addprintf(bufp, posp, "#<alien '%s'>", alien->name);
	else
		sheep_addprintf(bufp, posp, "#<alien '%p'>", alien);
}

const struct sheep_type sheep_alien_type = {
	.name = "alien",
	.free = alien_free,
	.format = alien_format,
};

sheep_t sheep_make_alien(struct sheep_vm *vm, sheep_alien_t function,
			const char *name)
{
	struct sheep_alien *alien;

	alien = sheep_malloc(sizeof(struct sheep_alien));
	alien->function = function;
	alien->name = name;
	return sheep_make_object(vm, &sheep_alien_type, alien);
}
