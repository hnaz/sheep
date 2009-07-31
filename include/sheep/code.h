#ifndef _SHEEP_CODE_H
#define _SHEEP_CODE_H

enum sheep_opcode {
	SHEEP_DROP,
	SHEEP_LOCAL,
	SHEEP_SET_LOCAL,
	SHEEP_FOREIGN,
	SHEEP_SET_FOREIGN,
	SHEEP_GLOBAL,
	SHEEP_SET_GLOBAL,
	SHEEP_CLOSURE,
	SHEEP_CALL,
	SHEEP_RET,
};

struct sheep_code {
	unsigned long *bytes;
	unsigned long length;
};

void sheep_emit(struct sheep_code *, enum sheep_opcode, unsigned int);

#endif /* _SHEEP_CODE_H */
