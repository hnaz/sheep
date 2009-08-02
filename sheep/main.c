#include <sheep/compile.h>
#include <sheep/name.h>
#include <sheep/vm.h>

int main(void)
{
	struct sheep_code *code;
	struct sheep_vm vm;
	sheep_t name;

	sheep_vm_init(&vm);
	name = sheep_name(&vm, "hallo");
	code = sheep_compile(&vm, name);
	sheep_vm_exit(&vm);

	return 0;
}
