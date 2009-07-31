#ifndef _SHEEP_EVAL_H
#define _SHEEP_EVAL_H

#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>

sheep_t sheep_eval(struct sheep_vm *, struct sheep_code *);

#endif /* _SHEEP_EVAL_H */
