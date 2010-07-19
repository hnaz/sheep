/*
 * include/sheep/code.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_CODE_H
#define _SHEEP_CODE_H

#include <sheep/vector.h>
#include <sheep/util.h>

/* the sheep_code_dump bastard */
struct sheep_function;
struct sheep_vm;

enum sheep_opcode {
	/* 0*/SHEEP_DROP,
	/* 1*/SHEEP_DUP,
	/* 2*/SHEEP_LOCAL,
	/* 3*/SHEEP_SET_LOCAL,
	/* 4*/SHEEP_FOREIGN,
	/* 5*/SHEEP_SET_FOREIGN,
	/* 6*/SHEEP_GLOBAL,
	/* 7*/SHEEP_SET_GLOBAL,
	/* 8*/SHEEP_HASH,
	/* 9*/SHEEP_SET_HASH,
	/*10*/SHEEP_CLOSURE,
	/*11*/SHEEP_CALL,
	/*12*/SHEEP_TAILCALL,
	/*13*/SHEEP_RET,
	/*14*/SHEEP_BRT,
	/*15*/SHEEP_BRF,
	/*16*/SHEEP_BR,
	/*17*/SHEEP_LOAD,
};

#define SHEEP_OPCODE_BITS	5
#define SHEEP_OPCODE_SHIFT	(sizeof(long) * 8 - SHEEP_OPCODE_BITS)

struct sheep_code {
	struct sheep_vector code;
	struct sheep_vector labels;
};

static inline void sheep_code_exit(struct sheep_code *code)
{
	sheep_free(code->code.items);
	sheep_free(code->labels.items);
}

static inline unsigned long sheep_encode(enum sheep_opcode op, unsigned int arg)
{
	unsigned long code;

	code = (unsigned long)op << SHEEP_OPCODE_SHIFT;
	code |= arg;
	return code;
}

static inline void sheep_decode(unsigned long code,
				enum sheep_opcode *op,
				unsigned int *arg)
{
	*op = code >> SHEEP_OPCODE_SHIFT;
	*arg = code & ((1UL << SHEEP_OPCODE_SHIFT) - 1);
}

static inline unsigned long sheep_emit(struct sheep_code *code,
				       enum sheep_opcode op,
				       unsigned int arg)
{
	return sheep_vector_push(&code->code, (void *)sheep_encode(op, arg));
}

unsigned long sheep_code_jump(struct sheep_code *);
void sheep_code_label(struct sheep_code *, unsigned long);
void sheep_code_finalize(struct sheep_code *);

void sheep_code_dump(struct sheep_vm *,
		     struct sheep_function *,
		     unsigned long,
		     enum sheep_opcode,
		     unsigned int);

void sheep_code_disassemble(struct sheep_code *);

#endif /* _SHEEP_CODE_H */
