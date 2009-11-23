/*
 * sheep/util.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/config.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sheep/util.h>

static void *massert(void *mem)
{
	if (mem)
		return mem;
	fprintf(stderr, "sheep: out of memory\n");
	abort();
}

void *sheep_malloc(size_t size)
{
	return massert(malloc(size));
}

void *sheep_zalloc(size_t size)
{
	return massert(calloc(1, size));
}

void *sheep_realloc(void *mem, size_t size)
{
	return massert(realloc(mem, size));
}

char *sheep_strdup(const char *str)
{
	return massert(strdup(str));
}

void sheep_free(const void *mem)
{
	free((void *)mem);
}

#define DEFAULT_BUF		64

int sheep_addprintf(char **bufp, size_t *pos, const char *fmt, ...)
{
	va_list ap;
	int len;

	*bufp = sheep_realloc(*bufp, *pos + DEFAULT_BUF);

	va_start(ap, fmt);
	len = vsnprintf(*bufp + *pos, DEFAULT_BUF, fmt, ap);
	va_end(ap);

	sheep_bug_on(len < 0);

	if (len >= DEFAULT_BUF) {
		int bytes = len + 1;

		*bufp = sheep_realloc(*bufp, *pos + bytes);

		va_start(ap, fmt);
		len = vsnprintf(*bufp + *pos, bytes, fmt, ap);
		va_end(ap);

		sheep_bug_on(len < 0);
		sheep_bug_on(len > bytes - 1);
	}
	*pos += len;
	return len;
}

void sheep_bug(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "*** sheep v%s BUG: ", SHEEP_VERSION);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	abort();
}
