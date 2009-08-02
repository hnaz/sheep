#include <sheep/block.h>
#include <sheep/code.h>
#include <sheep/name.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/vm.h>
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

void sheep_compiler_init(struct sheep_vm *vm)
{
	code_init(&vm->code);
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

int sheep_compile_list(struct sheep_compile *compile,
		struct sheep_context *context, sheep_t expr)
{
	return -1;
}
