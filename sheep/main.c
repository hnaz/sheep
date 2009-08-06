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
	/*
	 * (variable foo (quote bar))
	 * foo
	 */
	list = sheep_list(&vm, 3,
			sheep_name(&vm, "variable"),
			sheep_name(&vm, "foo"),
			sheep_list(&vm, 2,
				sheep_name(&vm, "quote"),
				sheep_name(&vm, "bar")));
	sheep_ddump(list);
	code = sheep_compile(&vm, list);
	if (code) {
		list = sheep_eval(&vm, code);
		sheep_ddump(list);
		sheep_free(code->code.items);
		sheep_free(code);
	}
	list = sheep_name(&vm, "foo");
	sheep_ddump(list);
	code = sheep_compile(&vm, list);
	if (code) {
		list = sheep_eval(&vm, code);
		list->type->ddump(list);
		puts("");
		sheep_free(code->code.items);
		sheep_free(code);
	}
	/*
	 * (with (a (quote (1 2 3))) a)
	 */
	list = sheep_list(&vm, 3,
			sheep_name(&vm, "with"),
			sheep_list(&vm, 2,
				sheep_name(&vm, "a"),
				sheep_list(&vm, 2,
					sheep_name(&vm, "quote"),
					sheep_list(&vm, 3,
						sheep_name(&vm, "1"),
						sheep_name(&vm, "2"),
						sheep_name(&vm, "3")))),
			sheep_name(&vm, "a"));
	sheep_ddump(list);
	code = sheep_compile(&vm, list);
	if (code) {
		list = sheep_eval(&vm, code);
		sheep_ddump(list);
		sheep_free(code->code.items);
		sheep_free(code);
	}
	/*
	 * (block
	 *   (function id (x) x)
	 *   (id (quote a)))
	 */
	list = sheep_list(&vm, 3,
			sheep_name(&vm, "block"),
			sheep_list(&vm, 4,
				sheep_name(&vm, "function"),
				sheep_name(&vm, "id"),
				sheep_list(&vm, 1,
					sheep_name(&vm, "x")),
				sheep_name(&vm, "x")),
			sheep_list(&vm, 2,
				sheep_name(&vm, "id"),
				sheep_list(&vm, 2,
					sheep_name(&vm, "quote"),
					sheep_name(&vm, "a"))));
	sheep_ddump(list);
	code = sheep_compile(&vm, list);
	if (code) {
		list = sheep_eval(&vm, code);
		sheep_ddump(list);
		sheep_free(code->code.items);
		sheep_free(code);
	}
	/*
	 * ((function (x) x) (quote a))
	 */
	list = sheep_list(&vm, 2,
			sheep_list(&vm, 3,
				sheep_name(&vm, "function"),
				sheep_list(&vm, 1, sheep_name(&vm, "x")),
				sheep_name(&vm, "x")),
			sheep_list(&vm, 2,
				sheep_name(&vm, "quote"),
				sheep_name(&vm, "b")));
	sheep_ddump(list);
	code = sheep_compile(&vm, list);
	if (code) {
		list = sheep_eval(&vm, code);
		sheep_ddump(list);
		sheep_free(code->code.items);
		sheep_free(code);
	}
	sheep_vm_exit(&vm);
	return 0;
}
