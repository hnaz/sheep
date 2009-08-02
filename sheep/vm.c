#include <sheep/compile.h>
#include <sheep/object.h>
#include <string.h>

#include <sheep/vm.h>

void sheep_vm_init(struct sheep_vm *vm)
{
	memset(vm, 0, sizeof(*vm));
	sheep_compiler_init(vm);
}

void sheep_vm_exit(struct sheep_vm *vm)
{
	sheep_objects_exit(vm);
}
