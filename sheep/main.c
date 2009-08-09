#include <sheep/compile.h>
#include <sheep/bool.h>
#include <sheep/eval.h>
#include <sheep/list.h>
#include <sheep/name.h>
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

	sheep_vm_init(&vm);
	/*
	 * ()
	 */
	test(&vm, sheep_list(&vm, 0));
	/*
	 * true
	 */
	test(&vm, &sheep_true);
	/*
	 * a
	 */
	test(&vm, sheep_name(&vm, "a"));
	/*
	 * (quote a)
	 */
	test(&vm, sheep_list(&vm, 2,
				sheep_name(&vm, "quote"),
				sheep_name(&vm, "a")));
	/*
	 * (variable foo (quote bar))
	 * foo
	 */
	test(&vm, sheep_list(&vm, 3,
				sheep_name(&vm, "variable"),
				sheep_name(&vm, "foo"),
				sheep_list(&vm, 2,
					sheep_name(&vm, "quote"),
					sheep_name(&vm, "bar"))));
	test(&vm, sheep_name(&vm, "foo"));

	/*
	 * (with (a (quote (1 2 3))) a)
	 */
	test(&vm, sheep_list(&vm, 3,
				sheep_name(&vm, "with"),
				sheep_list(&vm, 2,
					sheep_name(&vm, "a"),
					sheep_list(&vm, 2,
						sheep_name(&vm, "quote"),
						sheep_list(&vm, 3,
							sheep_name(&vm, "1"),
							sheep_name(&vm, "2"),
							sheep_name(&vm, "3")))),
				sheep_name(&vm, "a")));
	/*
	 * (block
	 *   (function id (x) x)
	 *   (id (quote a)))
	 */
	test(&vm, sheep_list(&vm, 3,
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
						sheep_name(&vm, "a")))));
	/*
	 * ((function (x) x) (quote a))
	 */
	test(&vm, sheep_list(&vm, 2,
				sheep_list(&vm, 3,
					sheep_name(&vm, "function"),
					sheep_list(&vm, 1, sheep_name(&vm, "x")),
					sheep_name(&vm, "x")),
				sheep_list(&vm, 2,
					sheep_name(&vm, "quote"),
					sheep_name(&vm, "b"))));
	/*
	 * ((function () (quote x)))
	 */
	test(&vm, sheep_list(&vm, 1,
				sheep_list(&vm, 3,
					sheep_name(&vm, "function"),
					sheep_list(&vm, 0),
					sheep_list(&vm, 2,
						sheep_name(&vm, "quote"),
						sheep_name(&vm, "x")))));
	/*
	 * (function foo (x)
	 *   (foo x))
	 * (foo foo)
	 */
	test(&vm, sheep_list(&vm, 4,
				sheep_name(&vm, "function"),
				sheep_name(&vm, "foo"),
				sheep_list(&vm, 1, sheep_name(&vm, "x")),
				sheep_list(&vm, 2,
					sheep_name(&vm, "foo"),
					sheep_name(&vm, "x"))));
	/*test(&vm, sheep_list(&vm, 2,
				sheep_name(&vm, "foo"),
				sheep_name(&vm, "foo")));*/
	sheep_vm_exit(&vm);
	return 0;
}
