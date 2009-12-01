/*
 * include/sheep/unpack.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_UNPACK_H
#define _SHEEP_UNPACK_H

#include <sheep/object.h>
#include <sheep/vector.h>
#include <sheep/list.h>
#include <stdarg.h>

enum sheep_unpack {
	SHEEP_UNPACK_OK,
	SHEEP_UNPACK_MISMATCH,
	SHEEP_UNPACK_TOO_MANY,
	SHEEP_UNPACK_TOO_FEW,
};

enum sheep_unpack __sheep_unpack_list(const char **, sheep_t *,
				struct sheep_list *, const char *, va_list);
int sheep_unpack_list(const char *, struct sheep_list *, const char *, ...);

enum sheep_unpack __sheep_unpack_stack(const char **, sheep_t *,
				struct sheep_vector *, const char *, va_list);
int sheep_unpack_stack(const char *, struct sheep_vm *, unsigned int,
		const char *, ...);


#endif /* _SHEEP_UNPACK_H */
