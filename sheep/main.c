#include <sheep/compile.h>
#include <sheep/config.h>
#include <sheep/string.h>
#include <sheep/eval.h>
#include <sheep/read.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>

static int do_file(int ac, char **av)
{
	struct sheep_vm vm;
	int ret = 1;
	FILE *in;

	in = fopen(av[0], "r");
	if (!in) {
		perror(av[0]);
		return 1;
	}

	sheep_vm_init(&vm, ac, av);
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

static int do_stdin(int ac, char **av)
{
	struct timeval start, end, diff;
	struct sheep_vm vm;

	gettimeofday(&start, NULL);
	sheep_vm_init(&vm, ac, av);
	gettimeofday(&end, NULL);

	timersub(&end, &start, &diff);
	printf("sheep v%s '%s' initialized in %lu.%.6lus\n",
		SHEEP_VERSION, SHEEP_NAME, diff.tv_sec, diff.tv_usec);

	while (1) {
		sheep_t exp, fun, val;
		char *repr;

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
		if (!val)
			continue;
		repr = sheep_repr(val);
		puts(repr);
		sheep_free(repr);
	}

	puts("bye");
	sheep_vm_exit(&vm);
	return 0;
}

int main(int ac, char **av)
{
	ac--, av++;
	if (ac)
		return do_file(ac, av);
	else
		return do_stdin(ac, av);
}
