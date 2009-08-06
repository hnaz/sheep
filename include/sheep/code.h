#ifndef _SHEEP_CODE_H
#define _SHEEP_CODE_H

#include <sheep/vector.h>

enum sheep_opcode {
	/*0*/SHEEP_DROP,
	/*1*/SHEEP_LOCAL,
	/*2*/SHEEP_SET_LOCAL,
	/*3*/SHEEP_FOREIGN,
	/*4*/SHEEP_SET_FOREIGN,
	/*5*/SHEEP_GLOBAL,
	/*6*/SHEEP_SET_GLOBAL,
	/*7*/SHEEP_CLOSURE,
	/*8*/SHEEP_CALL,
	/*9*/SHEEP_RET,
};

#define SHEEP_OPCODE_BITS	5
#define SHEEP_OPCODE_SHIFT	(sizeof(long) * 8 - SHEEP_OPCODE_BITS)

struct sheep_code {
	struct sheep_vector code;
};

#define SHEEP_CODE_INITIALIZER			\
	{ .code = SHEEP_VECTOR_INITIALIZER(32) }

#define SHEEP_DEFINE_CODE(name)				\
	struct sheep_code name = SHEEP_CODE_INITIALIZER

static inline void sheep_code_init(struct sheep_code *code)
{
	*code = (struct sheep_code)SHEEP_CODE_INITIALIZER;
}

static inline unsigned long
sheep_encode(enum sheep_opcode op, unsigned int arg)
{
	unsigned long code;

	code = (unsigned long)op << SHEEP_OPCODE_SHIFT;
	code |= arg;
	return code;
}

static inline void
sheep_decode(unsigned long code, enum sheep_opcode *op, unsigned int *arg)
{
	*op = code >> SHEEP_OPCODE_SHIFT;
	*arg = code & ((1UL << SHEEP_OPCODE_SHIFT) - 1);
}

static inline void
sheep_emit(struct sheep_code *code, enum sheep_opcode op, unsigned int arg)
{
	sheep_vector_push(&code->code, (void *)sheep_encode(op, arg));
}

#endif /* _SHEEP_CODE_H */
