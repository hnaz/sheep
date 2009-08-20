#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/vm.h>

#include <sheep/alien.h>

const struct sheep_type sheep_alien_type;

sheep_t sheep_alien(struct sheep_vm *vm, sheep_alien_t function)
{
	sheep_alien_t *foo;

	foo = sheep_malloc(sizeof(sheep_alien_t));
	sheep_bug_on((unsigned long)foo & 1);
	*foo = function;
	return sheep_object(vm, &sheep_alien_type, foo);
}
