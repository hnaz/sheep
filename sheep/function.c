#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>

#include <sheep/function.h>

static void free_function(sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->foreigns)
		/* XXX free foreigns */;
}

const struct sheep_type sheep_function_type = {
	.free = free_function,
};

sheep_t sheep_function(struct sheep_vm *vm)
{
	struct sheep_function *function;

	function = sheep_malloc(sizeof(struct sheep_function));
	return sheep_object(vm, &sheep_function_type, function);
}
