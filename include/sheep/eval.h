/*
 * include/sheep/eval.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_EVAL_H
#define _SHEEP_EVAL_H

#include <sheep/object.h>
#include <sheep/list.h>
#include <sheep/vm.h>
#include <stdarg.h>

sheep_t sheep_eval(struct sheep_vm *, sheep_t, int);
sheep_t sheep_apply(struct sheep_vm *, sheep_t, struct sheep_list *);
sheep_t sheep_call(struct sheep_vm *, sheep_t, unsigned int, ...);

void sheep_evaluator_exit(struct sheep_vm *);

#endif /* _SHEEP_EVAL_H */
