#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>

#include <sheep/function.h>

static void free_function(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;

	function = sheep_data(sheep);
	if (function->nativep) {
		struct sheep_native_function *native;

		native = function->function.native;
		if (native->foreigns) {
			/* XXX: free private container of preserved slots */
			sheep_free(native->foreigns->items);
			sheep_free(native->foreigns);
		}
		sheep_free(native);
	}
	sheep_free(function);
}

const struct sheep_type sheep_function_type = {
	.free = free_function,
};

sheep_t sheep_native_function(struct sheep_vm *vm)
{
	struct sheep_function *function;

	function = sheep_zalloc(sizeof(struct sheep_function));
	function->nativep = 1;
	function->function.native =
		sheep_zalloc(sizeof(struct sheep_native_function));
	return sheep_object(vm, &sheep_function_type, function);
}

sheep_t sheep_foreign_function(struct sheep_vm *vm,
			sheep_foreign_function_t foreign,
			unsigned int nr_parms)
{
	struct sheep_function *function;

	function = sheep_zalloc(sizeof(struct sheep_function));
	function->nr_parms = nr_parms;
	function->function.foreign = foreign;
	return sheep_object(vm, &sheep_function_type, function);
}
