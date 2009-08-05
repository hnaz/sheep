#include <sheep/compile.h>
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>

#include <sheep/string.h>

static void free_string(sheep_t sheep)
{
	sheep_free(sheep_data(sheep));
}

const struct sheep_type sheep_string_type = {
	.free = free_string,
	.compile = sheep_compile_constant,
};

sheep_t sheep_string(struct sheep_vm *vm, const char *str)
{
	return sheep_object(vm, &sheep_string_type, sheep_strdup(str));
}
