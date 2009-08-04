#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>
#include <stdlib.h>

#include <sheep/eval.h>

sheep_t sheep_eval(struct sheep_vm *vm, struct sheep_code *code)
{
	unsigned long pc = 0;

	vm->block = &vm->root;

	for (;;) {
		enum sheep_opcode op;
		unsigned int arg;
		sheep_t tmp;

		sheep_decode((unsigned long)code->code.items[pc], &op, &arg);
		switch (op) {
		case SHEEP_DROP:
			sheep_vector_pop(&vm->stack);
			break;
		case SHEEP_LOCAL:
			tmp = vm->block->locals.items[arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_LOCAL:
			tmp = sheep_vector_pop(&vm->stack);
			vm->block->locals.items[arg] = tmp;
			break;
		case SHEEP_FOREIGN:
			tmp = vm->block->foreigns.items[arg];
			sheep_vector_push(&vm->stack, *(sheep_t *)tmp);
			break;
		case SHEEP_SET_FOREIGN:
			tmp = sheep_vector_pop(&vm->stack);
			*(sheep_t *)vm->block->foreigns.items[arg] = tmp;
			break;
		case SHEEP_GLOBAL:
			tmp = vm->root.locals.items[arg];
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_SET_GLOBAL:
			tmp = sheep_vector_pop(&vm->stack);
			vm->root.locals.items[arg] = tmp;
			break;
		case SHEEP_CLOSURE:
			abort();
		case SHEEP_CALL:
			sheep_vector_push(&vm->calls, (void *)pc);
			sheep_vector_push(&vm->calls, vm->block);
			abort();
		case SHEEP_RET:
			if (!vm->calls.nr_items)
				goto out;
			vm->block = sheep_vector_pop(&vm->calls);
			pc = (unsigned long)sheep_vector_pop(&vm->calls);
			break;
		default:
			abort();
		}
		pc++;
	}
out:
	return sheep_vector_pop(&vm->stack);
}
