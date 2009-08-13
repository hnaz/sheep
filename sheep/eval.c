#include <sheep/function.h>
#include <sheep/object.h>
#include <sheep/bool.h>
#include <sheep/code.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stdlib.h>
#include <stdio.h>

#include <sheep/eval.h>

static sheep_t closure(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_function *function, *fnew;
	struct sheep_vector *foreigns;
	unsigned int i;

	sheep_bug_on(sheep->type != &sheep_function_type);
	function = sheep_data(sheep);

	if (!function->foreigns)
		return sheep;

	foreigns = sheep_malloc(sizeof(struct sheep_vector));
	sheep_vector_init(foreigns, 4);

	sheep_bug_on(function->foreigns->nr_items % 2);
	for (i = 0; i < function->foreigns->nr_items; i += 2) {
		unsigned long fbasep, offset;
		unsigned int dist, slot;
		void **foreignp;

		dist = (unsigned long)function->foreigns->items[i];
		slot = (unsigned long)function->foreigns->items[i + 1];
		/*
		 * Okay, we have the static function level distance to
		 * our foreign slot owner and the actual intex into
		 * the stack locals of that owner.
		 *
		 * Every active stack frame has 3 entries in
		 * vm->calls:
		 *
		 *     vm->calls = [... lastpc lastbasep lastfunction]
		 *
		 * Work out the base pointer of the owning frame and
		 * establish the foreign slot reference to the live
		 * stack slot.
		 */
		dist = (dist - 1) * 3 + 1;
		offset = vm->calls.nr_items - 1 - dist;
		fbasep = (unsigned long)vm->calls.items[offset];
		foreignp = vm->stack.items + fbasep;
		sheep_vector_push(foreigns, foreignp + slot);
	}

	sheep = sheep_function(vm);
	fnew = sheep_data(sheep);

	*fnew = *function;
	fnew->foreigns = foreigns;

	return sheep;
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
			tmp = closure(vm, tmp);
			sheep_vector_push(&vm->stack, tmp);
			break;
		case SHEEP_CALL:
			tmp = sheep_vector_pop(&vm->stack);
			/*
			 * Alien type?  Call directly.  Otherwise, it
			 * has to be a function object and we do the
			 * stack stuff here.
			 */
			if (tmp->type == &sheep_alien_type) {
				if (sheep_calien(tmp)(vm))
					goto err;
				break;
			}

			sheep_bug_on(tmp->type != &sheep_function_type);

			/* Save the old context */
			sheep_vector_push(&vm->calls, (void *)pc);
			sheep_vector_push(&vm->calls, (void *)basep);
			sheep_vector_push(&vm->calls, function);

			function = sheep_data(tmp);
			if (function->nr_parms != arg) {
				fprintf(stderr, "wrong number of arguments\n");
				goto err;
			}

			/* Prepare the stack */
			basep = vm->stack.nr_items - arg;
			if (function->nr_locals)
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
			sheep_bug_on(vm->stack.nr_items -
				basep - function->nr_locals != 1);

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
	sheep_bug_on(vm->stack.nr_items != 1);
	return sheep_vector_pop(&vm->stack);
err:
	/* Unwind the stack */
	vm->stack.nr_items = 0;
	vm->calls.nr_items = 0;
	return NULL;
}

void sheep_evaluator_init(struct sheep_vm *vm)
{
	sheep_map_set(&vm->main.env, "true",
		(void *)(unsigned long)sheep_vector_push(&vm->globals,
							&sheep_true));
	sheep_map_set(&vm->main.env, "false",
		(void *)(unsigned long)sheep_vector_push(&vm->globals,
							&sheep_false));
	vm->stack.blocksize = 32;
	vm->calls.blocksize = 16;
}

void sheep_evaluator_exit(struct sheep_vm *vm)
{
	sheep_free(vm->calls.items);
	sheep_free(vm->stack.items);
	sheep_map_drain(&vm->main.env);
}
