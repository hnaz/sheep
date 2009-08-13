#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>

#include <sheep/module.h>

const struct sheep_type sheep_alien_type;

static sheep_t sheep_alien(struct sheep_vm *vm, sheep_alien_t alien)
{
	sheep_bug_on((unsigned long)alien & 1); /* Oh god... */
	return sheep_object(vm, &sheep_alien_type, alien);
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
	unsigned long slot;

	slot = sheep_vector_push(&vm->globals, sheep);
	sheep_map_set(&module->env, name, (void *)slot);
	return slot;
}

/**
 * sheep_module_function - register a global function
 * @vm: runtime
 * @module: binding environment
 * @name: name to associate the function with
 * @function: alien function to register
 *
 * In @module bind @name to @function.
 */
void sheep_module_function(struct sheep_vm *vm, struct sheep_module *module,
			const char *name, sheep_alien_t function)
{
	sheep_module_shared(vm, module, name, sheep_alien(vm, function));
}
