#include <sheep/block.h>
#include <sheep/code.h>
#include <sheep/list.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sheep/compile.h>

struct sheep_context {
	struct sheep_code *code;
	struct sheep_block *block;
	struct sheep_map *env;
	struct sheep_context *parent;
};

static void code_init(struct sheep_code *code)
{
	memset(&code->code, 0, sizeof(struct sheep_vector));
	code->code.blocksize = 64;
}

struct sheep_code *__sheep_compile(struct sheep_compile *compile, sheep_t expr)
{
	struct sheep_context context;
	struct sheep_code *code;

	code = sheep_malloc(sizeof(struct sheep_code));
	code_init(code);

	memset(&context, 0, sizeof(context));
	context.code = code;
	context.env = &compile->vm->main.env;

	if (expr->type->compile(compile, &context, expr)) {
		sheep_free(code);
		return NULL;
	}

	sheep_emit(code, SHEEP_RET, 0);
	return code;
}

int sheep_compile_constant(struct sheep_compile *compile,
			struct sheep_context *context, sheep_t expr)
{
	unsigned int slot;

	slot = sheep_vector_push(&compile->vm->globals, expr);
	sheep_emit(context->code, SHEEP_GLOBAL, slot);
	return 0;
}

enum env_level {
	ENV_LOCAL,
	ENV_GLOBAL,
	ENV_FOREIGN,
};

static int lookup(struct sheep_context *context, const char *name,
		struct sheep_block **block, unsigned int *slot,
		enum env_level *env_level)
{
	struct sheep_context *current = context;
	void *entry;

	while (sheep_map_get(current->env, name, &entry)) {
		if (!current->parent)
			return -1;
		current = current->parent;
	}

	*slot = (unsigned long)entry;
	*block = current->block;

	if (!current->parent)
		*env_level = ENV_GLOBAL;
	else if (current == context)
		*env_level = ENV_LOCAL;
	else
		*env_level = ENV_FOREIGN;

	return 0;
}

int sheep_compile_name(struct sheep_compile *compile,
		struct sheep_context *context, sheep_t expr)
{
	const char *name = sheep_cname(expr);
	struct sheep_block *block;
	enum env_level level;
	unsigned int slot;

	if (lookup(context, name, &block, &slot, &level)) {
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
		slot = sheep_vector_push(&context->block->foreigns,
					&block->locals.items[slot]);
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
	va_end(ap);
	if (*items) {
		fprintf(stderr, "%s: too few arguments\n", caller);
		return -1;
	}
	if (list) {
		fprintf(stderr, "%s: too many arguments\n", caller);
		return -1;
	}
	return 0;
}

static int compile_quote(struct sheep_compile *compile,
			struct sheep_context *context, struct sheep_list *args)
{
	unsigned int slot;
	sheep_t obj;

	if (unpack("quote", args, "o", &obj))
		return -1;
	slot = sheep_vector_push(&compile->vm->globals, obj);
	sheep_emit(context->code, SHEEP_GLOBAL, slot);
	return 0;
}

int sheep_compile_list(struct sheep_compile *compile,
		struct sheep_context *context, sheep_t expr)
{
	struct sheep_list *form = sheep_data(expr);
	const char *op;
	void *entry;

	if (unpack("function call", form, "cr", &op, &form))
		return -1;

	if (!sheep_map_get(&compile->vm->specials, op, &entry)) {
		int (*special)(struct sheep_compile *, struct sheep_context *,
			struct sheep_list *) = entry;

		return special(compile, context, form);
	}

	fprintf(stderr, "function calls not implemented\n");
	return -1;
}

void sheep_compiler_init(struct sheep_vm *vm)
{
	code_init(&vm->code);
	sheep_map_set(&vm->specials, "quote", compile_quote);
}

void sheep_compiler_exit(struct sheep_vm *vm)
{
	sheep_map_drain(&vm->specials);
}
