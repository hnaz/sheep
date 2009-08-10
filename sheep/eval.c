#include <sheep/function.h>
#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/eval.h>

static void closure(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function;
	unsigned int i;

	assert(sheep->type == &sheep_function_type);
	function = sheep_data(sheep);

	if (!function->foreigns)
		return;

	assert(!(function->foreigns->nr_items % 2));
	for (i = 0; i < function->foreigns->nr_items; i += 2) {
		unsigned long fbasep, offset;
		unsigned int dist, slot;
		void **foreigns;

		dist = (unsigned long)function->foreigns->items[i];
		slot = (unsigned long)function->foreigns->items[i + 1];
		/*
		 * Okay, we have the static function level distance to
		 * our foreign slot owner and the actual intex into
		 * the stack locals of that owner.
		 *
		 * Work out the base pointer of the owning frame and
		 * set the foreign pointer to the live stack slot.
		 *
		 * Every active stack frame has 3 entries in
		 * vm->calls:
		 *
		 *     vm->calls = [... lastpc lastbasep lastfunction]
		 */
		dist = (dist - 1) * 3 + 1;
		offset = vm->calls.nr_items - 1 - dist;
		fbasep = (unsigned long)vm->calls.items[offset];

		foreigns = vm->stack.items + fbasep;
		function->foreigns->items[i / 2] = foreigns + slot;
	}
	function->foreigns->nr_items /= 2;
}

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
			closure(vm, tmp);
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_CALL:
			/* Save the old context */
			sheep_vector_push(&vm->calls, (void *)pc);
			sheep_vector_push(&vm->calls, (void *)basep);
			sheep_vector_push(&vm->calls, function);

			/* Get the callee */
			tmp = sheep_vector_pop(&vm->stack);
			assert(tmp->type == &sheep_function_type);
			function = sheep_data(tmp);

			/* Prepare the stack */
			basep = vm->stack.nr_items - arg;
			sheep_vector_grow(&vm->stack,
					function->nr_locals - arg);

			/* Execute the function body */
			current = &vm->code;
			pc = function->offset;
			continue;
		case SHEEP_RET:
			/* Toplevel RET */
			if (!vm->calls.nr_items)
				goto out;

			/* Sanity-check: exactly one return value */
			assert(vm->stack.nr_items -
				basep - function->nr_locals == 1);

			/* Nip the locals */
			if (function->nr_locals) {
				vm->stack.items[basep] =
					vm->stack.items[basep +
							function->nr_locals];
				vm->stack.nr_items = basep + 1;
			}

			/* Restore the old context */
			function = sheep_vector_pop(&vm->calls);
			basep = (unsigned long)sheep_vector_pop(&vm->calls);
			pc = (unsigned long)sheep_vector_pop(&vm->calls);

			/* Switch back to toplevel code? */
			if (!function)
				current = code;
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
