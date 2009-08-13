#ifndef _SHEEP_COMPILE_H
#define _SHEEP_COMPILE_H

#include <sheep/function.h>
#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stddef.h>

struct sheep_context {
	struct sheep_code *code;
	struct sheep_function *function;
	struct sheep_map *env;
	struct sheep_context *parent;
};

struct sheep_code *__sheep_compile(struct sheep_vm *, struct sheep_module *,
				sheep_t);

/**
 * sheep_compile - compile an expression
 * @vm: runtime
 * @exp: expression to compile
 *
 * Returns the bytecode object representing the expression.
 */
static inline struct sheep_code *sheep_compile(struct sheep_vm *vm, sheep_t exp)
{
	return __sheep_compile(vm, &vm->main, exp);
}

static inline unsigned int sheep_slot_constant(struct sheep_vm *vm, void *data)
{
	return sheep_vector_push(&vm->globals, data);
}

static inline unsigned int sheep_slot_global(struct sheep_vm *vm)
{
	return sheep_vector_push(&vm->globals, NULL);
}

static inline unsigned int sheep_slot_local(struct sheep_context *context)
{
	return context->function->nr_locals++;
}

unsigned int sheep_slot_foreign(struct sheep_context *, unsigned int,
				unsigned int);

int sheep_compile_constant(struct sheep_vm *, struct sheep_context *, sheep_t);
int sheep_compile_name(struct sheep_vm *, struct sheep_context *, sheep_t);
int sheep_compile_list(struct sheep_vm *, struct sheep_context *, sheep_t);

void sheep_compiler_init(struct sheep_vm *);
void sheep_compiler_exit(struct sheep_vm *);

#endif /* _SHEEP_COMPILE_H */
