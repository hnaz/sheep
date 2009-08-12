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
