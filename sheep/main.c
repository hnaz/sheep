#include <sheep/compile.h>
#include <sheep/config.h>
#include <sheep/eval.h>
#include <sheep/read.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>

static int do_file(const char *path)
{
	struct sheep_vm vm;
	int ret = 1;
	FILE *in;

	in = fopen(path, "r");
	if (!in) {
		perror(path);
		return 1;
	}

	sheep_vm_init(&vm);
	while (1) {
		sheep_t exp, fun, val;

		exp = sheep_read(&vm, in);
		if (!exp)
			goto out;
		if (exp == &sheep_eof)
			break;
		fun = sheep_compile(&vm, exp);
		if (!fun)
			goto out;
		val = sheep_eval(&vm, fun);
		if (!val)
			goto out;
	}
	ret = 0;
out:
	fclose(in);
	sheep_vm_exit(&vm);
	return ret;
}

static int do_stdin(void)
{
	struct timeval start, end, diff;
	struct sheep_vm vm;

	gettimeofday(&start, NULL);
	sheep_vm_init(&vm);
	gettimeofday(&end, NULL);

	timersub(&end, &start, &diff);
	printf("sheep v%s '%s' initialized in %lu.%.6lus\n",
		SHEEP_VERSION, SHEEP_NAME, diff.tv_sec, diff.tv_usec);

	while (1) {
		sheep_t exp, fun, val;

		printf("> ");
		fflush(stdout);
		exp = sheep_read(&vm, stdin);
		if (!exp)
			continue;
		if (exp == &sheep_eof)
			break;
		fun = sheep_compile(&vm, exp);
		if (!fun)
			continue;
		val = sheep_eval(&vm, fun);
		if (val)
			sheep_ddump(val);
	}

	puts("bye");
	sheep_vm_exit(&vm);
	return 0;
}

int main(int ac, char **av)
{
	if (ac == 2)
		return do_file(av[1]);
	else
		return do_stdin();
}
