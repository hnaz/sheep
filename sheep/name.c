#include <sheep/object.h>
#include <sheep/util.h>

#include <sheep/name.h>

static void name_free(sheep_t sheep)
{
	sheep_free(sheep_cname(sheep));
}

struct sheep_type sheep_type_name = {
	.free = name_free,
};

sheep_t sheep_name(struct sheep_vm *vm, const char *cname)
{
	return sheep_object(vm, &sheep_type_name, sheep_strdup(cname));
}
