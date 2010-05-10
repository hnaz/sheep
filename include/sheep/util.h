/*
 * include/sheep/util.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_UTIL_H
#define _SHEEP_UTIL_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __GNUC__
#define __noreturn	__attribute__((noreturn))
#else
#define __noreturn
#endif

void *sheep_malloc(size_t);
void *sheep_zalloc(size_t);
void *sheep_realloc(void *, size_t);
char *sheep_strdup(const char *);
void sheep_free(const void *);

struct sheep_strbuf {
	char *bytes;
	size_t nr_bytes;
};
void sheep_strbuf_add(struct sheep_strbuf *, const char *);
void sheep_strbuf_addf(struct sheep_strbuf *, const char *, ...);

void __noreturn sheep_bug(const char *, ...);

#define sheep_bug_on(cond)						\
	do if (cond)							\
		   sheep_bug("Unexpected condition `%s' in %s:%d",	\
			   #cond, __FILE__, __LINE__);			\
	while (0)

#endif /* _SHEEP_UTIL_H */
