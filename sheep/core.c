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
#include <string.h>
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
	ret = do_compile_block(vm, &function->code, function,
			&env, context, body);
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
	sheep_emit(&function->code, SHEEP_RET, 0);

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

/* (set name value) */
static int compile_set(struct sheep_vm *vm, struct sheep_context *context,
		struct sheep_list *args)
{
	sheep_t name, value;

	if (unpack("set", args, "ao", &name, &value))
		return -1;

	if (value->type->compile(vm, context, value))
		return -1;

	return sheep_compile_set(vm, context, name);
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

/* (bool object) */
static sheep_t eval_bool(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("bool", vm, nr_args, "o", &sheep))
		return NULL;

	if (sheep_test(sheep))
		return &sheep_true;
	return &sheep_false;
}

/* (not object) */
static sheep_t eval_not(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t sheep;

	if (sheep_unpack_stack("not", vm, nr_args, "o", &sheep))
		return NULL;

	if (sheep_test(sheep))
		return &sheep_false;
	return &sheep_true;
}

/* (= a b) */
static sheep_t eval_equal(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t a, b;

	if (sheep_unpack_stack("=", vm, nr_args, "oo", &a, &b))
		return NULL;

	if (sheep_equal(a, b))
		return &sheep_true;
	return &sheep_false;
}

static sheep_t do_arith(struct sheep_vm *vm, unsigned int nr_args,
			const char *operation)
{
	double value, *a, *b;

	if (sheep_unpack_stack(operation, vm, nr_args, "NN", &a, &b))
		return NULL;

	switch (*operation) {
	case '+':
		value = *a + *b;
		break;
	case '-':
		value = *a - *b;
		break;
	case '*':
		value = *a * *b;
		break;
	case '/':
		value = *a / *b;
		break;
	case '%': {
		long la, lb;

		la = *a;
		lb = *b;
		if (la != *a || lb != *b) {
			fprintf(stderr, "%%: having problems...\n");
			return NULL;
		}
		value = la % lb;
		break;
	}
	default:
		sheep_bug("unknown arithmetic operation");
	}

	return sheep_make_number(vm, value);
}

/* (+ a b) */
static sheep_t eval_plus(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_arith(vm, nr_args, "+");
}

/* (- a &optional b) */
static sheep_t eval_minus(struct sheep_vm *vm, unsigned int nr_args)
{
	if (nr_args == 1) {
		double *num;

		if (sheep_unpack_stack("-", vm, nr_args, "N", &num))
			return NULL;
		return sheep_make_number(vm, -*num);
	}

	return do_arith(vm, nr_args, "-");
}

/* (* a b) */
static sheep_t eval_multiply(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_arith(vm, nr_args, "*");
}

/* (/ a b) */
static sheep_t eval_divide(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_arith(vm, nr_args, "/");
}

/* (% a b) */
static sheep_t eval_modulo(struct sheep_vm *vm, unsigned int nr_args)
{
	return do_arith(vm, nr_args, "%");
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

/* (cons item list) */
static sheep_t eval_cons(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t item, list, new;

	new = sheep_make_list(vm, NULL, NULL);

	if (sheep_unpack_stack("cons", vm, nr_args, "ol", &item, &list))
		return NULL;

	sheep_list(new)->head = item;
	sheep_list(new)->tail = list;

	return new;
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

/* (reverse list) */
static sheep_t eval_reverse(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_list *l_old;
	sheep_t old, new;

	if (sheep_unpack_stack("reverse", vm, nr_args, "l", &old))
		return NULL;
	sheep_protect(vm, old);

	new = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, new);

	l_old = sheep_list(old);
	while (l_old->head) {
		sheep_t new_head;

		new_head = sheep_make_list(vm, l_old->head, new);

		sheep_unprotect(vm, new);
		sheep_protect(vm, new_head);

		new = new_head;
		l_old = sheep_list(l_old->tail);
	}

	sheep_unprotect(vm, new);
	sheep_unprotect(vm, old);

	return new;
}

static char *do_split(char **stringp, const char *delim)
{
	char *match, *result;

	match = strstr(*stringp, delim);
	result = *stringp;
	if (!match) {
		*stringp = NULL;
		return result;
	}

	*match = 0;
	*stringp = match + strlen(delim);
	return result;
}

/* (split string string) */
static sheep_t eval_split(struct sheep_vm *vm, unsigned int nr_args)
{
	const char *string, *token;
	struct sheep_list *list;
	sheep_t list_, token_;
	char *pos, *orig;
	int empty;

	if (sheep_unpack_stack("split", vm, nr_args, "sS", &token_, &string))
		return NULL;
	sheep_protect(vm, token_);

	list_ = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, list_);

	token = sheep_rawstring(token_);
	empty = !strlen(token);

	list = sheep_list(list_);
	pos = orig = sheep_strdup(string);
	while (pos) {
		sheep_t item;
		/*
		 * Empty splitting separates out every single
		 * character: (split "" "foo") => ("f" "o" "o")
		 */
		if (empty) {
			char str[2] = { *pos, 0 };

			item = sheep_make_string(vm, str);
			if (!pos[0] || !pos[1])
				pos = NULL;
			else
				pos++;
		} else
			item = sheep_make_string(vm, do_split(&pos, token));

		list->head = item;
		list->tail = sheep_make_list(vm, NULL, NULL);
		list = sheep_list(list->tail);
	}
	sheep_free(orig);

	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, token_);

	return list_;
}

/* (map function list) */
static sheep_t eval_map(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t mapper, old, new, result = NULL;
	struct sheep_list *l_old, *l_new;

	if (sheep_unpack_stack("map", vm, nr_args, "cl", &mapper, &old))
		return NULL;
	sheep_protect(vm, mapper);
	sheep_protect(vm, old);

	new = sheep_make_list(vm, NULL, NULL);
	sheep_protect(vm, new);

	l_old = sheep_list(old);
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

/* (reduce function list) */
static sheep_t eval_reduce(struct sheep_vm *vm, unsigned int nr_args)
{
	sheep_t reducer, list_, a, b, value, result = NULL;
	struct sheep_list *list;

	if (sheep_unpack_stack("reduce", vm, nr_args, "cl", &reducer, &list_))
		return NULL;

	sheep_protect(vm, reducer);
	sheep_protect(vm, list_);

	list = sheep_list(list_);
	if (unpack("reduce", list, "oor!", &a, &b, &list))
		return NULL;

	value = sheep_call(vm, reducer, 2, a, b);
	if (!value)
		goto out;

	while (list->head) {
		value = sheep_call(vm, reducer, 2, value, list->head);
		if (!value)
			goto out;
		list = sheep_list(list->tail);
	}
	result = value;
out:
	sheep_unprotect(vm, list_);
	sheep_unprotect(vm, reducer);

	return result;
}

/* (disassemble function) */
static sheep_t eval_disassemble(struct sheep_vm *vm, unsigned int nr_args)
{
	struct sheep_function *function;
	unsigned int nr_foreigns;
	sheep_t callable;

	if (sheep_unpack_stack("disassemble", vm, nr_args, "c", &callable))
		return NULL;
	
	if (callable->type == &sheep_alien_type) {
		struct sheep_alien *alien;

		alien = sheep_data(callable);
		printf("disassemble: %s is an alien\n", alien->name);
		return &sheep_false;
	}

	function = sheep_data(callable);
	if (function->foreigns) {
		nr_foreigns = sheep_foreigns(function)->nr_items;
		if (!sheep_active_closure(function))
			nr_foreigns /= 2;
	} else
		nr_foreigns = 0;

	__sheep_ddump(callable);
	printf(", %u parameters, %u local slots, "
		"%u foreign references\n", function->nr_parms,
		function->nr_locals, nr_foreigns);

	sheep_code_disassemble(&function->code);
	return &sheep_true;
}

void sheep_core_init(struct sheep_vm *vm)
{
	sheep_map_set(&vm->specials, "quote", compile_quote);
	sheep_map_set(&vm->specials, "block", compile_block);
	sheep_map_set(&vm->specials, "with", compile_with);
	sheep_map_set(&vm->specials, "variable", compile_variable);
	sheep_map_set(&vm->specials, "function", compile_function);
	sheep_map_set(&vm->specials, "if", compile_if);
	sheep_map_set(&vm->specials, "set", compile_set);

	sheep_module_shared(vm, &vm->main, "true", &sheep_true);
	sheep_module_shared(vm, &vm->main, "false", &sheep_false);

	sheep_module_function(vm, &vm->main, "bool", eval_bool);
	sheep_module_function(vm, &vm->main, "not", eval_not);
	sheep_module_function(vm, &vm->main, "=", eval_equal);
	sheep_module_function(vm, &vm->main, "+", eval_plus);
	sheep_module_function(vm, &vm->main, "-", eval_minus);
	sheep_module_function(vm, &vm->main, "*", eval_multiply);
	sheep_module_function(vm, &vm->main, "/", eval_divide);
	sheep_module_function(vm, &vm->main, "%", eval_modulo);
	sheep_module_function(vm, &vm->main, "ddump", eval_ddump);
	sheep_module_function(vm, &vm->main, "cons", eval_cons);
	sheep_module_function(vm, &vm->main, "list", eval_list);
	sheep_module_function(vm, &vm->main, "head", eval_head);
	sheep_module_function(vm, &vm->main, "tail", eval_tail);
	sheep_module_function(vm, &vm->main, "reverse", eval_reverse);
	sheep_module_function(vm, &vm->main, "split", eval_split);
	sheep_module_function(vm, &vm->main, "map", eval_map);
	sheep_module_function(vm, &vm->main, "reduce", eval_reduce);
	sheep_module_function(vm, &vm->main, "disassemble", eval_disassemble);
}

void sheep_core_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->specials);
}
