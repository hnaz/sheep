#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/name.h>

static void free_name(struct sheep_vm *vm, sheep_t sheep)
{
	sheep_free(sheep_cname(sheep));
}

static void ddump_name(sheep_t sheep)
{
	printf("%s", sheep_cname(sheep));
}

const struct sheep_type sheep_name_type = {
	.free = free_name,
	.compile = sheep_compile_name,
	.ddump = ddump_name,
};

sheep_t sheep_name(struct sheep_vm *vm, const char *cname)
{
	return sheep_object(vm, &sheep_name_type, sheep_strdup(cname));
}
