/*
 * sheep/module.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/eval.h>
#include <sheep/read.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <unistd.h>
#include <stdio.h>

#include <sheep/module.h>

static void free_module(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_module *mod = sheep_data(sheep);

	sheep_free(mod->name);
	sheep_map_drain(&mod->env);
	sheep_free(mod);
}

static void format_module(sheep_t sheep, char **bufp, size_t *posp)
{
	struct sheep_module *mod = sheep_data(sheep);

	sheep_addprintf(bufp, posp, "#<module '%s'>", mod->name);
}

const struct sheep_type sheep_module_type = {
	.free = free_module,
	.format = format_module,
};

sheep_t sheep_module_load(struct sheep_vm *vm, const char *name)
{
	struct sheep_module *mod;
	char path[1024];
	FILE *fp = NULL;

	sprintf(path, "%s.sheep", name);
	if (access(path, R_OK))
		goto error;
	fp = fopen(path, "r");
	if (!fp)
		goto error;

	mod = sheep_zalloc(sizeof(struct sheep_module));
	mod->name = sheep_strdup(name);

	while (1) {
		sheep_t exp, fun;

		exp = sheep_read(vm, fp);
		if (!exp)
			goto error;
		if (exp == &sheep_eof)
			break;
		fun = __sheep_compile(vm, mod, exp);
		if (!fun)
			goto error;
		if (!sheep_eval(vm, fun))
			goto error;
	}
	fclose(fp);
	return sheep_make_object(vm, &sheep_module_type, mod);
error:
	if (fp)
		fclose(fp);
	fprintf(stderr, "can not load %s\n", name);
	return NULL;
}

/**
 * sheep_module_shared - register a shared global slot
 * @vm: runtime
 * @module: binding environment
 * @name: name to associate the slot with
 * @sheep: initial slot value
 *
 * Allocates a global slot, stores @sheep it in and binds the slot to
 * @name in @module.
 *
 * The slot can be accessed and modified from sheep code by @name and
 * from C code through the returned slot index into vm->globals.items.
 */
unsigned int sheep_module_shared(struct sheep_vm *vm,
				struct sheep_module *module,
				const char *name, sheep_t sheep)
{
	unsigned int slot;

	slot = sheep_vector_push(&vm->globals, sheep);
	sheep_map_set(&module->env, name, (void *)(unsigned long)slot);
	return slot;
}
