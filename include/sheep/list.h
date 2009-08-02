#ifndef _SHEEP_LIST_H
#define _SHEEP_LIST_H

#include <sheep/object.h>
#include <sheep/vm.h>
#include <stdarg.h>

struct sheep_list {
	sheep_t head;
	struct sheep_list *tail;
};

sheep_t sheep_list(struct sheep_vm *, sheep_t, struct sheep_list *);

#endif /* _SHEEP_LIST_H */
