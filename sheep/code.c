/*
 * sheep/code.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/foreign.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/code.h>

unsigned long sheep_code_jump(struct sheep_code *code)
{
	return sheep_vector_push(&code->labels, NULL);
}

void sheep_code_label(struct sheep_code *code, unsigned long jump)
{
	unsigned long offset = code->code.nr_items;

	code->labels.items[jump] = (void *)offset;
}

void sheep_code_finalize(struct sheep_code *code)
{
	unsigned long offset;

	sheep_emit(code, SHEEP_RET, 0);
	for (offset = 0; offset < code->code.nr_items; offset++) {
		unsigned long label, insn;
		enum sheep_opcode op;
		unsigned int arg;

		insn = (unsigned long)code->code.items[offset];
		sheep_decode(insn, &op, &arg);

		if (op != SHEEP_BRT && op != SHEEP_BRF && op != SHEEP_BR)
			continue;

		label = (unsigned long)code->labels.items[arg];
		insn = sheep_encode(op, label - offset);

		code->code.items[offset] = (void *)insn;
	}
}

static const char *opnames[] = {
	"DROP", "DUP", "LOCAL", "SET_LOCAL", "FOREIGN", "SET_FOREIGN",
	"GLOBAL", "SET_GLOBAL", "HASH", "SET_HASH",
	"CLOSURE", "CALL", "TAILCALL", "RET",
	"BRT", "BRF", "BR",
	"LOAD",
};

void sheep_code_dump(struct sheep_vm *vm, struct sheep_function *function,
		unsigned long basep, enum sheep_opcode op, unsigned int arg)
{
	struct sheep_indirect *indirect;
	sheep_t sheep;
	char *str;

	printf("  %-10s %5u ", opnames[op], arg);

	switch (op) {
	case SHEEP_LOCAL:
		sheep = vm->stack.items[basep + arg];
		break;
	case SHEEP_FOREIGN:
		indirect = function->foreign->items[arg];
		if (indirect->count > 0)
			sheep = indirect->value.closed;
		else
			sheep = vm->stack.items[indirect->value.live.index];
		break;
	case SHEEP_GLOBAL:
	case SHEEP_CLOSURE:
		sheep = vm->globals.items[arg];
		break;
	case SHEEP_HASH:
	case SHEEP_SET_HASH:
		printf("; %s\n", vm->keys[arg]);
		return;
	default:
		puts("");
		return;
	}

	str = sheep_repr(sheep);
	printf("; %s\n", str);
	sheep_free(str);
}

void sheep_code_disassemble(struct sheep_code *code)
{
	unsigned long *codep = (unsigned long *)code->code.items;
	enum sheep_opcode op;
	unsigned int arg;

	do {
		sheep_decode(*codep, &op, &arg);
		printf("  %-12s %5u\n", opnames[op], arg);
		codep++;
	} while (op != SHEEP_RET);
}
