/*
 * include/hseep/parse.h
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_PARSE_H
#define _SHEEP_PARSE_H

#include <sheep/compile.h> /* mooh */
#include <sheep/object.h>
#include <sheep/list.h>
#include <stdarg.h>

void sheep_parser_error(struct sheep_compile *, sheep_t, const char *, ...);

int __sheep_parse(struct sheep_compile *,
		  struct sheep_list *,
		  struct sheep_list *,
		  const char *,
		  ...);
int sheep_parse(struct sheep_compile *,
		struct sheep_list *,
		const char *,
		...);

#endif /* _SHEEP_PARSE_H */
