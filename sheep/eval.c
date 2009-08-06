#include <sheep/function.h>
#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/eval.h>

sheep_t sheep_eval(struct sheep_vm *vm, struct sheep_code *code)
{
	struct sheep_function *function = NULL;
	struct sheep_code *current = code;
	unsigned long pc = 0, basep = 0;

	for (;;) {
		enum sheep_opcode op;
		unsigned int arg;
		sheep_t tmp;

		sheep_decode((unsigned long)current->code.items[pc], &op, &arg);
		switch (op) {
		case SHEEP_DROP:
			sheep_vector_pop(&vm->stack);
			break;
		case SHEEP_LOCAL:
			tmp = vm->stack.items[basep + arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_LOCAL:
			tmp = sheep_vector_pop(&vm->stack);
			vm->stack.items[basep + arg] = tmp;
			break;
		case SHEEP_FOREIGN:
			tmp = function->foreigns->items[arg];
			sheep_vector_push(&vm->stack, *(sheep_t *)tmp);
			break;
		case SHEEP_SET_FOREIGN:
			tmp = sheep_vector_pop(&vm->stack);
			*(sheep_t *)function->foreigns->items[arg] = tmp;
			break;
		case SHEEP_GLOBAL:
			tmp = vm->globals.items[arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_GLOBAL:
			tmp = sheep_vector_pop(&vm->stack);
			vm->globals.items[arg] = tmp;
			break;
		case SHEEP_CLOSURE:
			tmp = vm->globals.items[arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_CALL:
			/* Save context */
			sheep_vector_push(&vm->calls, (void *)pc);
			sheep_vector_push(&vm->calls, (void *)basep);
			sheep_vector_push(&vm->calls, function);

			/* Get callable */
			tmp = sheep_vector_pop(&vm->stack);
			assert(tmp->type == &sheep_function_type);

			/* Set up new context */
			basep = vm->stack.nr_items - arg;
			function = sheep_data(tmp);
			pc = function->offset;
			current = &vm->code;
			break;
		case SHEEP_RET:
			/* Toplevel RET */
			if (!vm->calls.nr_items)
				goto out;

			/* Restore old context */
			function = sheep_vector_pop(&vm->calls);
			basep = (unsigned long)sheep_vector_pop(&vm->calls);
			pc = (unsigned long)sheep_vector_pop(&vm->calls);

			if (!function) {
				/* Switch back to toplevel code */
				current = code;
			}
			break;
		default:
			abort();
		}
		pc++;
	}
out:
	assert(vm->stack.nr_items == 1);
	return sheep_vector_pop(&vm->stack);
}
