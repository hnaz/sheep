/*
 * lib/regex.c
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/unpack.h>
#include <sheep/list.h>
#include <sheep/util.h>
#include <sys/types.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <string.h>
#include <regex.h>
#include <stdio.h>

#define MAX_MATCHES	32

/* (match regex string) */
static sheep_t match(struct sheep_vm *vm, unsigned int nr_args)
{
	regmatch_t matches[MAX_MATCHES];
	sheep_t regex_, string_, result = NULL;
	const char *regex, *string;
	struct sheep_list *list;
	unsigned int i;
	regex_t reg;
	int status;

	if (sheep_unpack_stack(vm, nr_args, "ss", &regex_, &string_))
		return NULL;

	sheep_protect(vm, string_);

	regex = sheep_rawstring(regex_);
	if (regcomp(&reg, regex, REG_EXTENDED)) {
		fprintf(stderr, "match: invalid regular expression\n");
		goto out;
	}

	result = sheep_make_cons(vm, NULL, NULL);
	string = sheep_rawstring(string_);

	status = regexec(&reg, string, MAX_MATCHES, matches, 0);
	regfree(&reg);
	if (status == REG_NOMATCH)
		goto out;

	list = sheep_list(result);
	for (i = 0; i < MAX_MATCHES && matches[i].rm_so != -1; i++) {
		unsigned long start, end, len;
		char *sub;

		start = matches[i].rm_so;
		end = matches[i].rm_eo;
		len = end - start;
		sub = sheep_malloc(len + 1);
		memcpy(sub, string + start, len);
		sub[len] = 0;
		list->head = __sheep_make_string(vm, sub, len);

		list->tail = sheep_make_cons(vm, NULL, NULL);
		list = sheep_list(list->tail);
	}
out:
	sheep_unprotect(vm, string_);
	return result;
}

int init(struct sheep_vm *vm, struct sheep_module *module)
{
	sheep_module_function(vm, module, "match", match);
	return 0;
}
