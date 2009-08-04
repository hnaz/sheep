#ifndef _SHEEP_COMPILE_H
#define _SHEEP_COMPILE_H

#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>

struct sheep_context;

struct sheep_code *__sheep_compile(struct sheep_vm *, struct sheep_module *,
				sheep_t);

static inline struct sheep_code *sheep_compile(struct sheep_vm *vm, sheep_t s)
{
	return __sheep_compile(vm, &vm->main, s);
}

int sheep_compile_constant(struct sheep_vm *, struct sheep_context *, sheep_t);
int sheep_compile_name(struct sheep_vm *, struct sheep_context *, sheep_t);
int sheep_compile_list(struct sheep_vm *, struct sheep_context *, sheep_t);

void sheep_compiler_init(struct sheep_vm *);
void sheep_compiler_exit(struct sheep_vm *);

#endif /* _SHEEP_COMPILE_H */
