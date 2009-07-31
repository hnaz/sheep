#ifndef _SHEEP_UTIL_H
#define _SHEEP_UTIL_H

#include <stddef.h>

void *sheep_malloc(size_t);
void *sheep_zalloc(size_t);
void *sheep_realloc(void *, size_t);
char *sheep_strdup(const char *);

#endif /* _SHEEP_UTIL_H */
