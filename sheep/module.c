/*
 * sheep/module.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/compile.h>
#include <sheep/config.h>
#include <sheep/object.h>
#include <sheep/string.h>
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

static void free_module(struct sheep_module *mod)
{
	sheep_free(mod->name);
	sheep_map_drain(&mod->env);
#if 0
	/*
	 * XXX: dlclose() is not a good idea when there are still
	 * objects pointing into that memory.
	 */
	if (mod->handle)
		dlclose(mod->handle);
#endif
	sheep_free(mod);
}

static void module_free(struct sheep_vm *vm, sheep_t sheep)
{
	free_module(sheep_data(sheep));
}

static void module_format(sheep_t sheep, struct sheep_strbuf *sb, int repr)
{
	struct sheep_module *mod = sheep_data(sheep);

	sheep_strbuf_addf(sb, "#<module '%s'>", mod->name);
}

const struct sheep_type sheep_module_type = {
	.free = module_free,
	.format = module_format,
};

enum {
	LOAD_OK,
	LOAD_SKIP,
	LOAD_FAIL,
};

static unsigned int load_so(struct sheep_vm *vm,
			    const char *path,
			    struct sheep_module *mod)
{
	int (*init)(struct sheep_vm *, struct sheep_module *);
	void *handle;

	handle = dlopen(path, RTLD_NOW);
	if (!handle) {
		sheep_error(vm, "dlopen(%s) failed: %s", path, dlerror());
		goto err;
	}

	init = dlsym(handle, "init");
	if (!init) {
		sheep_error(vm, "%s has no init()", path);
		goto err_handle;
	}

	if (init(vm, mod))
		goto err_handle;

	mod->handle = handle;
	return LOAD_OK;

err_handle:
	dlclose(handle);
err:
	return LOAD_FAIL;
}

static unsigned int load_sheep(struct sheep_vm *vm,
			       const char *path,
			       struct sheep_module *mod)
{
	struct sheep_reader reader;
	int ret = LOAD_FAIL;
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp)
		goto out;

	sheep_reader_init(&reader, path, fp);
	while (1) {
		struct sheep_expr *expr;
		sheep_t fun;

		expr = sheep_read(&reader, vm);
		if (!expr)
			goto out_file;
		if (expr->object == &sheep_eof) {
			sheep_free_expr(expr);
			break;
		}
		fun = __sheep_compile(vm, mod, expr);
		sheep_free_expr(expr);
		if (!fun)
			goto out_file;
		if (!sheep_eval(vm, fun, 0))
			goto out_file;
	}
	ret = LOAD_OK;
out_file:
	fclose(fp);
out:
	return ret;
}

static unsigned int module_load(struct sheep_vm *vm,
				const char *path,
				const char *name,
				struct sheep_module *mod)
{
	char filename[1024];

	snprintf(filename, sizeof(filename), "%s/%s.so", path, name);
	if (!access(filename, R_OK))
		return load_so(vm, filename, mod);

	snprintf(filename, sizeof(filename), "%s/%s.sheep", path, name);
	if (!access(filename, R_OK))
		return load_sheep(vm, filename, mod);

	return LOAD_SKIP;
}

static unsigned int load_path;

sheep_t sheep_module_load(struct sheep_vm *vm, const char *name)
{
	struct sheep_module *mod;
	struct sheep_list *paths;
	sheep_t paths_;

	mod = sheep_zalloc(sizeof(struct sheep_module));
	mod->name = sheep_strdup(name);
	sheep_module_variable(vm, mod, "module", sheep_make_string(vm, name));

	paths_ = vm->globals.items[load_path];
	if (sheep_type(paths_) != &sheep_list_type) {
		sheep_error(vm, "`load-path' is not a list");
		goto err;
	}

	paths = sheep_list(paths_);
	while (paths->head) {
		const char *path;

		if (sheep_type(paths->head) != &sheep_string_type) {
			sheep_error(vm, "bogus paths in `load-path'");
			goto err;
		}

		path = sheep_rawstring(paths->head);
		switch (module_load(vm, path, name, mod)) {
		case LOAD_OK:
			goto found;
		case LOAD_FAIL:
			goto err;
		case LOAD_SKIP:
		default:
			paths = sheep_list(paths->tail);
		}
	}

	sheep_error(vm, "module `%s' not found", name);
err:
	free_module(mod);
	return NULL;
found:
	return sheep_make_object(vm, &sheep_module_type, mod);
}

unsigned int sheep_module_variable(struct sheep_vm *vm,
				   struct sheep_module *module,
				   const char *name,
				   sheep_t sheep)
{
	unsigned int slot;

	slot = sheep_vector_push(&vm->globals, sheep);
	sheep_map_set(&module->env, name, (void *)(unsigned long)slot);
	return slot;
}

void sheep_module_function(struct sheep_vm *vm,
			   struct sheep_module *module,
			   const char *name,
			   sheep_alien_t alien)
{
	sheep_t sheep;

	sheep = sheep_make_alien(vm, alien, name);
	sheep_module_variable(vm, module, name, sheep);
}

static sheep_t builtin_load_path(struct sheep_vm *vm)
{
	return sheep_make_list(vm, 2,
			sheep_make_string(vm, "."),
			sheep_make_string(vm, SHEEP_MODDIR));
}

void sheep_module_builtins(struct sheep_vm *vm)
{
	load_path = sheep_vm_variable(vm, "load-path", builtin_load_path(vm));
	sheep_module_variable(vm, &vm->main, "module", &sheep_nil);
}
