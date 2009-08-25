#ifndef _SHEEP_CODE_H
#define _SHEEP_CODE_H

#include <sheep/vector.h>

struct sheep_function;

enum sheep_opcode {
	/* 0*/SHEEP_DROP,
	/* 1*/SHEEP_LOCAL,
	/* 2*/SHEEP_SET_LOCAL,
	/* 3*/SHEEP_FOREIGN,
	/* 4*/SHEEP_SET_FOREIGN,
	/* 5*/SHEEP_GLOBAL,
	/* 6*/SHEEP_SET_GLOBAL,
	/* 7*/SHEEP_CLOSURE,
	/* 8*/SHEEP_CALL,
	/* 9*/SHEEP_RET,
	/*10*/SHEEP_BRN,
	/*11*/SHEEP_BR,
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

static inline unsigned long
sheep_emit(struct sheep_code *code, enum sheep_opcode op, unsigned int arg)
{
	return sheep_vector_push(&code->code, (void *)sheep_encode(op, arg));
}

void sheep_code_dump(struct sheep_vm *, struct sheep_function *,
		unsigned long, enum sheep_opcode, unsigned int);

#endif /* _SHEEP_CODE_H */
