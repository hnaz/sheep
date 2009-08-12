#include <sheep/compile.h>
#include <sheep/bool.h>
#include <sheep/eval.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/read.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

static void test(struct sheep_vm *vm, sheep_t src)
{
	struct sheep_code *code;

	printf("executing "), sheep_ddump(src);
	code = sheep_compile(vm, src);
	if (code) {
		sheep_t res = sheep_eval(vm, code);
		sheep_ddump(res);
		sheep_free(code->code.items);
		sheep_free(code);
	}
}

int main(void)
{
	struct sheep_vm vm;
	sheep_t src;

	sheep_vm_init(&vm);
	while ((src = sheep_read(&vm, stdin)) != &sheep_eof)
		if (src)
			test(&vm, src);
	sheep_vm_exit(&vm);
	return 0;
}
