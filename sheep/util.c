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

void sheep_strbuf_add(struct sheep_strbuf *sb, const char *str)
{
	size_t len;

	len = strlen(str);
	sb->bytes = sheep_realloc(sb->bytes, sb->nr_bytes + len + 1);
	memcpy(sb->bytes + sb->nr_bytes, str, len + 1);
	sb->nr_bytes += len;
}

#define DEFAULT_BUF 64

void sheep_strbuf_addf(struct sheep_strbuf *sb, const char *fmt, ...)
{
	va_list ap;
	size_t len;

	sb->bytes = sheep_realloc(sb->bytes, sb->nr_bytes + DEFAULT_BUF);

	va_start(ap, fmt);
	len = vsnprintf(sb->bytes + sb->nr_bytes, DEFAULT_BUF, fmt, ap);
	va_end(ap);

	if (len >= DEFAULT_BUF) {
		sb->bytes = sheep_realloc(sb->bytes, sb->nr_bytes + len + 1);

		va_start(ap, fmt);
		len = vsnprintf(sb->bytes + sb->nr_bytes, len + 1, fmt, ap);
		va_end(ap);
	}

	sb->nr_bytes += len;
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
