#ifndef _SHEEP_COMPILE_H
#define _SHEEP_COMPILE_H

#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>

struct sheep_code *sheep_compile(struct sheep_vm *, sheep_t);

struct sheep_level;
int sheep_compile_constant(struct sheep_vm *, struct sheep_level *, sheep_t);
int sheep_compile_name(struct sheep_vm *, struct sheep_level *, sheep_t);
int sheep_compile_list(struct sheep_vm *, struct sheep_level *, sheep_t);

#endif /* _SHEEP_COMPILE_H */
