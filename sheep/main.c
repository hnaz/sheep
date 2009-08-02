#include <sheep/compile.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/vm.h>
#include <stdio.h>

int main(void)
{
	struct sheep_code *code;
	struct sheep_vm vm;
	sheep_t name, list;

	sheep_vm_init(&vm);
	name = sheep_name(&vm, "hallo");
	list = sheep_list(&vm, name, NULL);
	code = sheep_compile(&vm, name);
	code = sheep_compile(&vm, list);
	sheep_vm_exit(&vm);

	return 0;
}
