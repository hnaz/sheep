#include <sheep/function.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <sheep/compile.h>

struct sheep_context {
	struct sheep_code *code;
	struct sheep_function *function;
	struct sheep_map *env;
	struct sheep_context *parent;
};

static unsigned int constant_slot(struct sheep_vm *vm, void *data)
{
	return sheep_vector_push(&vm->globals, data);
}

static unsigned int global_slot(struct sheep_vm *vm)
{
	return sheep_vector_push(&vm->globals, NULL);
}

static unsigned int local_slot(struct sheep_context *context)
{
	return context->function->nr_locals++;
}

static unsigned int foreign_slot(struct sheep_context *context,
				unsigned int dist, unsigned int slot)
{
	if (!context->function->foreigns) {
		context->function->foreigns =
			sheep_malloc(sizeof(struct sheep_vector));
		context->function->foreigns->items = NULL;
		context->function->foreigns->nr_items = 0;
		context->function->foreigns->blocksize = 8;
	}
	sheep_vector_push(context->function->foreigns,
			(void *)(unsigned long)dist);
	sheep_vector_push(context->function->foreigns,
			(void *)(unsigned long)slot);
	return context->function->foreigns->nr_items / 2;
}

static void code_init(struct sheep_code *code)
{
	memset(&code->code, 0, sizeof(struct sheep_vector));
	code->code.blocksize = 32;
}

struct sheep_code *__sheep_compile(struct sheep_vm *vm,
				struct sheep_module *module, sheep_t expr)
{
	struct sheep_context context;
	struct sheep_code *code;

	code = sheep_malloc(sizeof(struct sheep_code));
	code_init(code);

	memset(&context, 0, sizeof(context));
	context.code = code;
	context.env = &module->env;

	if (expr->type->compile(vm, &context, expr)) {
		sheep_free(code->code.items);
		sheep_free(code);
		return NULL;
	}

	sheep_emit(code, SHEEP_RET, 0);
	return code;
}

int sheep_compile_constant(struct sheep_vm *vm, struct sheep_context *context,
			sheep_t expr)
{
	unsigned int slot;

	slot = constant_slot(vm, expr);
	sheep_emit(context->code, SHEEP_GLOBAL, slot);
	return 0;
}

enum env_level {
	ENV_LOCAL,
	ENV_GLOBAL,
	ENV_FOREIGN,
};

static int lookup(struct sheep_context *context, const char *name,
		unsigned int *dist, unsigned int *slot,
		enum env_level *env_level)
{
	struct sheep_function *last = context->function;
	struct sheep_context *current = context;
	unsigned int distance = 0;
	void *entry;

	while (sheep_map_get(current->env, name, &entry)) {
		if (!current->parent)
			return -1;
		/*
		 * There can be several environment nestings within
		 * one function level, but for foreign slots we are
		 * only interested in the latter.
		 */
		if (current->function != last) {
			last = current->function;
			distance++;
		}
		current = current->parent;
	}

	*dist = distance;
	*slot = (unsigned long)entry;

	if (!current->function)
		*env_level = ENV_GLOBAL;
	else if (current == context)
		*env_level = ENV_LOCAL;
	else
		*env_level = ENV_FOREIGN;

	return 0;
}

int sheep_compile_name(struct sheep_vm *vm, struct sheep_context *context,
		sheep_t expr)
{
	unsigned int dist, slot;
	enum env_level level;
	const char *name;

	name = sheep_cname(expr);
	if (lookup(context, name, &dist, &slot, &level)) {
		fprintf(stderr, "unbound name: %s\n", name);
		return -1;
	}

	switch (level) {
	case ENV_LOCAL:
		sheep_emit(context->code, SHEEP_LOCAL, slot);
		break;
	case ENV_GLOBAL:
		sheep_emit(context->code, SHEEP_GLOBAL, slot);
		break;
	case ENV_FOREIGN:
		slot = foreign_slot(context, dist, slot);
		sheep_emit(context->code, SHEEP_FOREIGN, slot);
		break;
	}

	return 0;
}

struct unpack_map {
	char control;
	const struct sheep_type *type;
	const char *type_name;
};

static struct unpack_map unpack_table[] = {
	{ 'o',	NULL,			"object" },
	{ 'c',	&sheep_name_type,	"name" },
	{ 'a',	&sheep_name_type,	"name" },
	{ 'l',	&sheep_list_type,	"list" },
	{ 0,	NULL,			NULL },
};

static struct unpack_map *map_control(char control)
{
	struct unpack_map *map;

	for (map = unpack_table; map->control; map++)
		if (map->control == control)
			break;
	assert(map->control);
	return map;
}

static struct unpack_map *map_type(const struct sheep_type *type)
{
	struct unpack_map *map;

	for (map = unpack_table; map->control; map++)
		if (map->type == type)
			break;
	return map;
}

static int verify(const char *caller, char control, sheep_t object)
{
	struct unpack_map *want;

	want = map_control(control);
	if (want->type && object->type != want->type) {
		struct unpack_map *got = map_type(object->type);

		fprintf(stderr, "%s: expected %s, got %s\n",
			caller, want->type_name, got->type_name);
		return -1;
	}
	return 0;
}	

static int unpack(const char *caller, struct sheep_list *list,
		const char *items, ...)
{
	struct sheep_object *object;
	va_list ap;

	va_start(ap, items);
	while (*items) {
		if (*items == 'r') {
			*va_arg(ap, struct sheep_list **) = list;
			va_end(ap);
			return 0;
		}

		if (!list)
			break;

		object = list->head;
		if (verify(caller, *items, object) < 0) {
			va_end(ap);
			return -1;
		}
		if (*items == 'c')
			*va_arg(ap, const char **) = sheep_cname(object);
		else
			*va_arg(ap, sheep_t *) = object;

		items++;
		list = list->tail;
	}

	if (*items) {
		fprintf(stderr, "%s: too few arguments\n", caller);
		return -1;
	}
	if (list) {
		fprintf(stderr, "%s: too many arguments\n", caller);
		return -1;
	}

	va_end(ap);
	return 0;
}

/* (quote expr) */
static int compile_quote(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	unsigned int slot;
	sheep_t obj;

	if (unpack("quote", args, "o", &obj))
		return -1;
	slot = constant_slot(vm, obj);
	sheep_emit(context->code, SHEEP_GLOBAL, slot);
	return 0;
}

static int do_compile_block(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	while (args) {
		sheep_t value = args->head;

		if (value->type->compile(vm, context, value))
			return -1;
		if (!args->tail)
			break;
		sheep_emit(context->code, SHEEP_DROP, 0);
		args = args->tail;
	}
	return 0;
}

/* (block &rest expr) */
static int compile_block(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	struct sheep_context bcontext;
	struct sheep_map benv;
	int ret;

	memset(&benv, 0, sizeof(struct sheep_map));

	bcontext.code = context->code;
	bcontext.function = context->function;
	bcontext.env = &benv;
	bcontext.parent = context;

	ret = do_compile_block(vm, &bcontext, args);

	sheep_map_drain(&benv);
	return ret;
}

/* (with (name expr name expr ...) &rest expr) */
static int compile_with(struct sheep_vm *vm, struct sheep_context *context,
			struct sheep_list *args)
{
	struct sheep_list *bindings, *body;
	struct sheep_context wcontext;
	struct sheep_map wenv;
	sheep_t pairs;
	int ret = -1;

	memset(&wenv, 0, sizeof(struct sheep_map));

	wcontext.code = context->code;
	wcontext.function = context->function;
	wcontext.env = &wenv;
	wcontext.parent = context;

	if (unpack("with", args, "lr", &pairs, &body))
		return -1;
	bindings = sheep_data(pairs);
	do {
		unsigned int slot;
		const char *name;
		sheep_t value;

		if (unpack("with", bindings, "cor", &name, &value, &bindings))
			goto out;
		if (value->type->compile(vm, &wcontext, value))
			goto out;
		if (wcontext.function) {
			slot = local_slot(&wcontext);
			sheep_emit(context->code, SHEEP_SET_LOCAL, slot);
		} else {
			slot = global_slot(vm);
			sheep_emit(context->code, SHEEP_SET_GLOBAL, slot);
		}
		sheep_map_set(wcontext.env, name, (void *)(unsigned long)slot);
	} while (bindings);

	ret = do_compile_block(vm, &wcontext, body);
out:
	sheep_map_drain(&wenv);
	return ret;
}

/* (variable name value) */
int compile_variable(struct sheep_vm *vm, struct sheep_context *context,
		struct sheep_list *args)
{
	unsigned int slot;
	const char *name;
	sheep_t value;

	if (unpack("variable", args, "co", &name, &value))
		return -1;
	if (value->type->compile(vm, context, value))
		return -1;
	if (context->function) {
		slot = local_slot(context);
		sheep_emit(context->code, SHEEP_SET_LOCAL, slot);
		sheep_emit(context->code, SHEEP_LOCAL, slot);
	} else {
		slot = global_slot(vm);
		sheep_emit(context->code, SHEEP_SET_GLOBAL, slot);
		sheep_emit(context->code, SHEEP_GLOBAL, slot);
	}
	sheep_map_set(context->env, name, (void *)(unsigned long)slot);
	return 0;
}

int sheep_compile_list(struct sheep_vm *vm, struct sheep_context *context,
		sheep_t expr)
{
	struct sheep_list *form;
	const char *op;
	void *entry;

	form = sheep_data(expr);
	if (unpack("function call", form, "cr", &op, &form))
		return -1;

	if (!sheep_map_get(&vm->specials, op, &entry)) {
		int (*special)(struct sheep_vm *, struct sheep_context *,
			struct sheep_list *) = entry;

		return special(vm, context, form);
	}

	fprintf(stderr, "function calls not implemented\n");
	return -1;
}

void sheep_compiler_init(struct sheep_vm *vm)
{
	code_init(&vm->code);
	sheep_map_set(&vm->specials, "quote", compile_quote);
	sheep_map_set(&vm->specials, "block", compile_block);
	sheep_map_set(&vm->specials, "with", compile_with);
	sheep_map_set(&vm->specials, "variable", compile_variable);
}

void sheep_compiler_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->main.env);
	sheep_map_drain(&vm->specials);
}
