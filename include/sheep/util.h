#ifndef _SHEEP_UTIL_H
#define _SHEEP_UTIL_H

#include <stdarg.h>
#include <stddef.h>

void *sheep_malloc(size_t);
void *sheep_zalloc(size_t);
void *sheep_realloc(void *, size_t);
char *sheep_strdup(const char *);
void sheep_free(const void *);

void sheep_bug(const char *, ...);

#define sheep_bug_on(cond)						\
	do if (cond)							\
		   sheep_bug("Unexpected condition `%s' in %s:%d",	\
			   #cond, __FILE__, __LINE__);			\
	while (0)

#endif /* _SHEEP_UTIL_H */
