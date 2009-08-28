/*
 * sheep/core.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/function.h>
#include <sheep/compile.h>
#include <sheep/config.h>
#include <sheep/module.h>
#include <sheep/number.h>
#include <sheep/string.h>
#include <sheep/alien.h>
#include <sheep/bool.h>
#include <sheep/code.h>
#include <sheep/eval.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#include <sheep/core.h>

static int do_verify(const char *caller, char control, sheep_t object)
{
	switch (control) {
	case 'o':
		return 1;
	case 'b':
		return object->type == &sheep_bool_type;
	case 'n':
		return object->type == &sheep_number_type;
	case 'a':
		return object->type == &sheep_name_type;
	case 's':
		return object->type == &sheep_string_type;
	case 'l':
		return object->type == &sheep_list_type;
	case 'q':
		return object->type == &sheep_string_type ||
			object->type == &sheep_list_type;
	case 'c':
		return object->type == &sheep_alien_type ||
			object->type == &sheep_function_type;
	}
	sheep_bug("unexpected unpack control %c", control);
}

static const char *map_control(char control)
{
	static const char *controlnames[] = {
		"bbool", "nnumber", "aname", "sstring",
		"llist", "qsequence", "ccallable", NULL
	};
	const char **p;

	for (p = controlnames; *p; p++)
		if (*p[0] == control)
			return p[0] + 1;

	sheep_bug("incomplete controlnames table");
}

static int verify(const char *caller, char control, sheep_t object)
{
	if (do_verify(caller, control, object))
		return 1;

	fprintf(stderr, "%s: expected %s, got %s\n",
		caller, map_control(control), object->type->name);
	return 0;
}

static int unpack(const char *caller, struct sheep_list *list,
		const char *items, ...)
{
	struct sheep_object *object;
	int ret = -1;
	va_list ap;

	va_start(ap, items);
	while (*items) {
		if (*items == 'r') {
			/*
			 * "r!" makes sure the rest is non-empty
			 */
			if (items[1] == '!' && !list)
				break;
			*va_arg(ap, struct sheep_list **) = list;
			ret = 0;
			goto out;
		}

		if (!list->head)
			break;

		object = list->head;
		if (!verify(caller, tolower(*items), object))
			goto out;
		if (isupper(*items))
			*va_arg(ap, void **) = sheep_data(object);
		else
			*va_arg(ap, sheep_t *) = object;

		items++;
		list = sheep_list(list->tail);
	}

	if (*items)
		fprintf(stderr, "%s: too few arguments\n", caller);
	else if (list->head)
		fprintf(stderr, "%s: too many arguments\n", caller);
	else
		ret = 0;
out:
	va_end(ap);
	return ret;
}

/* (quote expr) */
static int compile_quote(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	unsigned int slot;
	sheep_t obj;

	if (unpack("quote", args, "o", &obj))
		return -1;

	slot = sheep_slot_constant(vm, obj);
	sheep_emit(context->code, SHEEP_GLOBAL, slot);
	return 0;
}

static int do_compile_block(struct sheep_vm *vm, struct sheep_code *code,
			struct sheep_function *function, struct sheep_map *env,
			struct sheep_context *parent, struct sheep_list *args)
{
	struct sheep_context context = {
		.code = code,
		.function = function,
		.env = env,
		.parent = parent,
	};

	for (;;) {
		sheep_t value = args->head;

		if (value->type->compile(vm, &context, value))
			return -1;

		args = sheep_list(args->tail);
		if (!args->head)
			break;
		/*
		 * The last value is the value of the whole block,
		 * drop everything in between to keep the stack
		 * balanced.
		 */
		sheep_emit(code, SHEEP_DROP, 0);
	}
	return 0;
}

/* (block expr*) */
static int compile_block(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	SHEEP_DEFINE_MAP(env);
	int ret;

	/* Just make sure the block is not empty */
	if (unpack("block", args, "r!", &args))
		return -1;

	ret = do_compile_block(vm, context->code, context->function,
			&env, context, args);

	sheep_map_drain(&env);
	return ret;
}

/* (with ([name expr]*) expr*) */
static int compile_with(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	struct sheep_list *bindings, *body;
	SHEEP_DEFINE_MAP(env);
	int ret = -1;

	if (unpack("with", args, "Lr!", &bindings, &body))
		return -1;

	while (bindings->head) {
		unsigned int slot;
		const char *name;
		sheep_t value;

		if (unpack("with", bindings, "Aor", &name, &value, &bindings))
			goto out;

		if (value->type->compile(vm, context, value))
			goto out;

		if (context->function) {
			slot = sheep_slot_local(context);
			sheep_emit(context->code, SHEEP_SET_LOCAL, slot);
		} else {
			slot = sheep_slot_global(vm);
			sheep_emit(context->code, SHEEP_SET_GLOBAL, slot);
		}
		sheep_map_set(&env, name, (void *)(unsigned long)slot);
	}

	ret = do_compile_block(vm, context->code, context->function,
			&env, context, body);
out:
	sheep_map_drain(&env);
	return ret;
}

/* (variable name expr) */
static int compile_variable(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	unsigned int slot;
	const char *name;
	sheep_t value;

	if (unpack("variable", args, "Ao", &name, &value))
		return -1;

	if (value->type->compile(vm, context, value))
		return -1;

	if (context->function) {
		slot = sheep_slot_local(context);
		sheep_emit(context->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(context->code, SHEEP_LOCAL, slot);
	} else {
		slot = sheep_slot_global(vm);
		sheep_emit(context->code, SHEEP_SET_GLOBAL, slot);
		sheep_emit(context->code, SHEEP_GLOBAL, slot);
	}
	sheep_map_set(context->env, name, (void *)(unsigned long)slot);
	return 0;
}

/* (function name? (arg*) expr*) */
static int compile_function(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	unsigned int cslot, bslot = bslot;
	struct sheep_function *function;
	struct sheep_list *parms, *body;
	SHEEP_DEFINE_CODE(code);
	SHEEP_DEFINE_MAP(env);
	const char *name;
	sheep_t sheep;
	int ret = -1;

	if (args->head && args->head->type == &sheep_name_type) {
		name = sheep_cname(args->head);
		args = sheep_list(args->tail);
	} else
		name = NULL;

	if (unpack("function", args, "Lr!", &parms, &body))
		return -1;

	sheep = sheep_make_function(vm, name);
	function = sheep_data(sheep);

	while (parms->head) {
		unsigned int slot;
		const char *parm;

		if (unpack("function", parms, "Ar", &parm, &parms))
			goto out;
		slot = function->nr_locals++;
		sheep_map_set(&env, parm, (void *)(unsigned long)slot);
		function->nr_parms++;
	}

	cslot = sheep_slot_constant(vm, sheep);
	if (name) {
		if (context->function)
			bslot = sheep_slot_local(context);
		else
			bslot = sheep_slot_global(vm);
		sheep_map_set(context->env, name, (void *)(unsigned long)bslot);
	}

	sheep_protect(vm, sheep);
	ret = do_compile_block(vm, &code, function, &env, context, body);
	sheep_unprotect(vm, sheep);
	if (ret) {
		/*
		 * Potentially sacrifice a global slot, but take out
		 * the binding to it if it doesn't contain anything
		 * useful.  This can probably be done better one
		 * day...
		 */
		if (name)
			sheep_map_del(context->env, name);
		goto out;
	}

	sheep_emit(&code, SHEEP_RET, 0);
	function->offset = vm->code.code.nr_items;
	sheep_vector_concat(&vm->code.code, &code.code);

	sheep_emit(context->code, SHEEP_CLOSURE, cslot);
	if (name) {
		if (context->function) {
			sheep_emit(context->code, SHEEP_SET_LOCAL, bslot);
			sheep_emit(context->code, SHEEP_LOCAL, bslot);
		} else {
			sheep_emit(context->code, SHEEP_SET_GLOBAL, bslot);
			sheep_emit(context->code, SHEEP_GLOBAL, bslot);
		}
	}
out:
	sheep_free(code.code.items);
	sheep_map_drain(&env);
	return ret;
}

/* (if cond then else*?) */
static int compile_if(struct sheep_vm *vm, struct sheep_context *context,
		struct sheep_list *args)
{
	struct sheep_list *elseform;
	unsigned long belse, bend;
	sheep_t cond, then;

	if (unpack("if", args, "oor", &cond, &then, &elseform))
		return -1;
	if (cond->type->compile(vm, context, cond))
		return -1;
	belse = sheep_emit(context->code, SHEEP_BRN, 0);
	if (then->type->compile(vm, context, then))
		return -1;
	bend = sheep_emit(context->code, SHEEP_BR, 0);
	context->code->code.items[belse] =
		(void *)sheep_encode(SHEEP_BRN, bend + 1 - belse);
	if (elseform->head) {
		if (do_compile_block(vm, context->code, context->function,
					context->env, context, elseform))
			return -1;
	} else {
		if (sheep_compile_name(vm, context,
					sheep_make_name(vm, "false")))
			return -1;
	}
	context->code->code.items[bend] =
		(void *)sheep_encode(SHEEP_BR,
				context->code->code.nr_items - bend);
	return 0;
}

int sheep_unpack_stack(const char *caller, struct sheep_vm *vm,
		unsigned int nr_args, const char *items, ...)
{
	unsigned int nr = nr_args;
	int ret = -1;
	va_list ap;

	va_start(ap, items);
	while (*items && nr) {
		struct sheep_object *object;

		object = vm->stack.items[vm->stack.nr_items - nr];
		if (!verify(caller, tolower(*items), object))
			goto out;
		if (isupper(*items))
			*va_arg(ap, void **) = sheep_data(object);
		else
			*va_arg(ap, sheep_t *) = object;
		items++;
		nr--;
	}
	if (*items)
		fprintf(stderr, "%s: too few arguments\n", caller);
	else if (nr)
		fprintf(stderr, "%s: too many arguments\n", caller);
	else {
		vm->stack.nr_items -= nr_args;
		ret = 0;
	}
out:
	va_end(ap);
	return ret;
}

/* (ddump expr) */
static sheep_t eval_ddump(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("ddump", vm, nr_args, "o", &sheep))
		return NULL;

	sheep_ddump(sheep);
	return &sheep_true;
}

/* (list expr*) */
static sheep_t eval_list(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t list;

	list = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, list);

	while (nr_args--) {
		sheep_t new;

		new = sheep_make_list(vm, NULL, list);
		sheep_list(new)->head = sheep_vector_pop(&vm->stack);

		sheep_unprotect(vm, list);
		sheep_protect(vm, new);

		list = new;
	}

	sheep_unprotect(vm, list);
	return list;
}

/* (head list) */
static sheep_t eval_head(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_list *list;
	sheep_t sheep;

	if (sheep_unpack_stack("head", vm, nr_args, "l", &sheep))
		return NULL;

	list = sheep_list(sheep);
	if (list->head)
		return list->head;
	return sheep;
}

/* (tail list) */
static sheep_t eval_tail(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_list *list;
	sheep_t sheep;

	if (sheep_unpack_stack("tail", vm, nr_args, "l", &sheep))
		return NULL;

	list = sheep_list(sheep);
	if (list->head)
		return list->tail;
	return sheep;
}

/* (map fun list) */
static sheep_t eval_map(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t mapper, old, new, result = NULL;
	struct sheep_list *l_old, *l_new;

	if (sheep_unpack_stack("map", vm, nr_args, "cl", &mapper, &old))
		return NULL;

	sheep_protect(vm, mapper);
	sheep_protect(vm, old);
	l_old = sheep_list(old);

	new = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, new);
	l_new = sheep_list(new);

	while (l_old->head) {
		l_new->head = sheep_call(vm, mapper, 1, l_old->head);
		if (!l_new->head)
			goto out;
		l_new->tail = sheep_make_list(vm, NULL, NULL);
		l_new = sheep_list(l_new->tail);
		l_old = sheep_list(l_old->tail);
	}

	result = new;
out:
	sheep_unprotect(vm, new);
	sheep_unprotect(vm, old);
	sheep_unprotect(vm, mapper);
	return result;
}

void sheep_core_init(struct sheep_vm *vm)
{
	sheep_map_set(&vm->specials, "quote", compile_quote);
	sheep_map_set(&vm->specials, "block", compile_block);
	sheep_map_set(&vm->specials, "with", compile_with);
	sheep_map_set(&vm->specials, "variable", compile_variable);
	sheep_map_set(&vm->specials, "function", compile_function);
	sheep_map_set(&vm->specials, "if", compile_if);

	sheep_module_shared(vm, &vm->main, "true", &sheep_true);
	sheep_module_shared(vm, &vm->main, "false", &sheep_false);

	sheep_module_shared(vm, &vm->main, "ddump",
			sheep_make_alien(vm, eval_ddump, "ddump"));
	sheep_module_shared(vm, &vm->main, "list",
			sheep_make_alien(vm, eval_list, "list"));
	sheep_module_shared(vm, &vm->main, "head",
			sheep_make_alien(vm, eval_head, "head"));
	sheep_module_shared(vm, &vm->main, "tail",
			sheep_make_alien(vm, eval_tail, "tail"));
	sheep_module_shared(vm, &vm->main, "map",
			sheep_make_alien(vm, eval_map, "map"));
}

void sheep_core_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->specials);
}
