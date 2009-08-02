#include <sheep/name.h>
#include <sheep/vm.h>

int main(void)
{
	struct sheep_vm vm;	

	sheep_vm_init(&vm);
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_name(&vm, "hallo");
	sheep_vm_exit(&vm);

	return 0;
}
