#include <sheep/config.h>
#include <sheep/module.h>
#include <sheep/name.h>
#include <sheep/vm.h>

#include <sheep/core.h>

static int do_sheep_version(struct sheep_vm *vm)
{
	sheep_vector_push(&vm->stack, sheep_name(vm, SHEEP_VERSION));
	return 0;
}

void sheep_core_register(struct sheep_vm *vm)
{
	sheep_module_function(vm, &vm->main, "sheep-version", do_sheep_version);
}
