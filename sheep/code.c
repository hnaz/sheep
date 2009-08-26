/*
 * sheep/code.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/object.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/code.h>

static const char *opnames[] = {
	"DROP", "LOCAL", "SET_LOCAL", "FOREIGN", "SET_FOREIGN",
	"GLOBAL", "SET_GLOBAL", "CLOSURE", "CALL", "RET", "BRN", "BR",
};

void sheep_code_dump(struct sheep_vm *vm, struct sheep_function *function,
		unsigned long basep, enum sheep_opcode op, unsigned int arg)
{
	sheep_t sheep;

	printf("  %-10s %5u ", opnames[op], arg);

	switch (op) {
	case SHEEP_LOCAL:
		sheep = vm->stack.items[basep + arg];
		break;
	case SHEEP_FOREIGN:
		sheep = *(sheep_t *)sheep_foreigns(function)->items[arg];
		break;
	case SHEEP_GLOBAL:
	case SHEEP_CLOSURE:
		sheep = vm->globals.items[arg];
		break;
	default:
		puts("");
		return;
	}

	printf("; ");
	sheep_ddump(sheep);
}
		
