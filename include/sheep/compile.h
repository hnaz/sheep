#ifndef _SHEEP_COMPILE_H
#define _SHEEP_COMPILE_H

#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>

struct sheep_context;

struct sheep_compile {
	struct sheep_vm *vm;
	struct sheep_module *module;
};

struct sheep_code *__sheep_compile(struct sheep_compile *, sheep_t);

static inline struct sheep_code *sheep_compile(struct sheep_vm *vm, sheep_t s)
{
	struct sheep_compile compile = {
		.vm = vm,
		.module = &vm->main,
	};

	return __sheep_compile(&compile, s);
}

int sheep_compile_constant(struct sheep_compile *, struct sheep_context *,
			sheep_t);
int sheep_compile_name(struct sheep_compile *, struct sheep_context *, sheep_t);
int sheep_compile_list(struct sheep_compile *, struct sheep_context *, sheep_t);

void sheep_compiler_init(struct sheep_vm *);

#endif /* _SHEEP_COMPILE_H */
