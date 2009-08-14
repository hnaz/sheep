#include <sheep/compile.h>
#include <sheep/config.h>
#include <sheep/eval.h>
#include <sheep/read.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <sys/time.h>
#include <stdio.h>

static void test(struct sheep_vm *vm, sheep_t src)
{
	struct sheep_code *code;
	sheep_t res;

	printf("executing "), sheep_ddump(src);
	code = sheep_compile(vm, src);
	if (!code)
		return;
	res = sheep_eval(vm, code);
	sheep_free(code->code.items);
	sheep_free(code);
	if (res)
		sheep_ddump(res);
}

int main(void)
{
	struct timeval start, end, diff;
	struct sheep_vm vm;
	sheep_t src;

	gettimeofday(&start, NULL);

	sheep_vm_init(&vm);

	gettimeofday(&end, NULL);
	timersub(&end, &start, &diff);
	printf("sheep v%s '%s' initialized in %lu.%.6lus\n",
		SHEEP_VERSION, SHEEP_NAME, diff.tv_sec, diff.tv_usec);

	while ((src = sheep_read(&vm, stdin)) != &sheep_eof)
		if (src)
			test(&vm, src);

	sheep_vm_exit(&vm);

	return 0;
}
