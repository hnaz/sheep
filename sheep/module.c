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
#include <dlfcn.h>
#include <stdio.h>

#include <sheep/module.h>

static void do_free_module(struct sheep_module *mod)
{
	sheep_free(mod->name);
	sheep_map_drain(&mod->env);
	if (mod->handle)
		dlclose(mod->handle);
	sheep_free(mod);
}

static void free_module(struct sheep_vm *vm, sheep_t sheep)
{
	do_free_module(sheep_data(sheep));
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

static int load_so(struct sheep_vm *vm, const char *path,
		struct sheep_module *mod)
{
	int (*init)(struct sheep_vm *, struct sheep_module *);
	void *handle;

	handle = dlopen(path, RTLD_NOW);
	if (!handle)
		return -1;

	init = dlsym(handle, "init");
	if (!init)
		goto err;

	if (init(vm, mod))
		goto err;

	mod->handle = handle;
	return 0;
err:
	dlclose(handle);
	return -1;
}

static int load_sheep(struct sheep_vm *vm, const char *path,
		struct sheep_module *mod)
{
	int ret = -1;
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp)
		goto out;

	while (1) {
		sheep_t exp, fun;

		exp = sheep_read(vm, fp);
		if (!exp)
			goto out_file;
		if (exp == &sheep_eof)
			break;
		fun = __sheep_compile(vm, mod, exp);
		if (!fun)
			goto out_file;
		if (!sheep_eval(vm, fun))
			goto out_file;
	}
	ret = 0;
out_file:
	fclose(fp);
out:
	return ret;
}

sheep_t sheep_module_load(struct sheep_vm *vm, const char *name)
{
	struct sheep_module *mod;
	char path[1024];

	mod = sheep_zalloc(sizeof(struct sheep_module));
	mod->name = sheep_strdup(name);

	sprintf(path, "./%s.so", name);
	if (!access(path, R_OK)) {
		if (!load_so(vm, path, mod))
			goto found;
	} else {
		sprintf(path, "%s.sheep", name);
		if (!access(path, R_OK)) {
			if (!load_sheep(vm, path, mod))
				goto found;
		}
	}
	do_free_module(mod);
	return NULL;
found:
	return sheep_make_object(vm, &sheep_module_type, mod);
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
