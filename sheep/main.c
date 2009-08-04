#include <sheep/compile.h>
#include <sheep/eval.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

int main(void)
{
	struct sheep_code *code;
	struct sheep_vm vm;
	sheep_t list;

	sheep_vm_init(&vm);
	list = sheep_list(&vm, 3,
			sheep_name(&vm, "with"),
			sheep_list(&vm, 2,
				sheep_name(&vm, "a"),
				sheep_list(&vm, 2,
					sheep_name(&vm, "quote"),
					sheep_name(&vm, "b"))),
			sheep_name(&vm, "a"));
	code = sheep_compile(&vm, list);
	if (code) {
		list = sheep_eval(&vm, code);
		puts(sheep_cname(list));
		sheep_free(code->code.items);
		sheep_free(code);
	}
	sheep_vm_exit(&vm);
	return 0;
}
