/*
 * include/sheep/read.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_READ_H
#define _SHEEP_READ_H

#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/vm.h>
#include <stdio.h>

struct sheep_reader {
	const char *filename;
	unsigned long lineno;
	FILE *file;
};

static inline void sheep_reader_init(struct sheep_reader *reader,
				     const char *filename,
				     FILE *file)
{
	reader->filename = filename;
	reader->lineno = 1;
	reader->file = file;
}

struct sheep_expr {
	sheep_t object;
	const char *filename;
	struct sheep_vector lines;
};

extern struct sheep_object sheep_eof;

void sheep_free_expr(struct sheep_expr *);
struct sheep_expr *sheep_read(struct sheep_reader *, struct sheep_vm *);

#endif /* _SHEEP_READ_H */
