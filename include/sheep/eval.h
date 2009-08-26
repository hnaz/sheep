/*
 * include/sheep/eval.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_EVAL_H
#define _SHEEP_EVAL_H

#include <sheep/object.h>
#include <sheep/code.h>
#include <sheep/vm.h>
#include <stdarg.h>

sheep_t sheep_eval(struct sheep_vm *, struct sheep_code *);
sheep_t sheep_call(struct sheep_vm *, sheep_t, unsigned int, ...);

void sheep_evaluator_init(struct sheep_vm *);
void sheep_evaluator_exit(struct sheep_vm *);

#endif /* _SHEEP_EVAL_H */
