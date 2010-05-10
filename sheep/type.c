/*
 * sheep/type.c
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/object.h>
#include <sheep/util.h>
#include <sheep/map.h>
#include <sheep/gc.h>
#include <sheep/vm.h>
#include <stdio.h>

#include <sheep/type.h>

static void typeobject_mark(sheep_t sheep)
{
	struct sheep_typeobject *object;
	struct sheep_typeclass *class;
	unsigned int i;

	object = sheep_data(sheep);
	class = sheep_data(object->class);

	sheep_mark(object->class);
	for (i = 0; i < class->nr_slots; i++)
		sheep_mark(object->values[i]);
}

static void typeobject_free(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_typeobject *object;

	object = sheep_data(sheep);
	sheep_free(object->values);
	sheep_map_drain(&object->map);
	sheep_free(object);
}

static void typeobject_format(sheep_t sheep, struct sheep_strbuf *sb, int repr)
{
	struct sheep_typeobject *object;

	object = sheep_data(sheep);
	sheep_strbuf_addf(sb, "#<object '%p'>", object);
}

const struct sheep_type sheep_typeobject_type = {
	.mark = typeobject_mark,
	.free = typeobject_free,
	.format = typeobject_format,
};

static void typeclass_free(struct sheep_vm *vm, sheep_t sheep)
{
	struct sheep_typeclass *class;
	unsigned int i;

	class = sheep_data(sheep);
	sheep_free(class->name);
	for (i = 0; i < class->nr_slots; i++)
		sheep_free(class->names[i]);
	sheep_free(class->names);
	sheep_free(class);
}

static enum sheep_call typeclass_call(struct sheep_vm *vm, sheep_t callable,
				unsigned int nr_args, sheep_t *valuep)
{
	struct sheep_typeobject *object;
	struct sheep_typeclass *class;

	class = sheep_data(callable);
	if (nr_args != class->nr_slots) {
		fprintf(stderr, "%s has %d slots, %d values given\n",
			class->name, class->nr_slots, nr_args);
		return SHEEP_CALL_FAIL;
	}

	object = sheep_zalloc(sizeof(struct sheep_typeobject));
	object->class = callable;
	object->values = sheep_malloc(sizeof(sheep_t *) * class->nr_slots);
	while (nr_args--) {
		object->values[nr_args] = sheep_vector_pop(&vm->stack);
		sheep_map_set(&object->map, class->names[nr_args],
			(void *)(unsigned long)nr_args);
	}
	*valuep = sheep_make_object(vm, &sheep_typeobject_type, object);
	return SHEEP_CALL_DONE;
}

static void typeclass_format(sheep_t sheep, struct sheep_strbuf *sb, int repr)
{
	struct sheep_typeclass *class;

	class = sheep_data(sheep);
	sheep_strbuf_addf(sb, "#<type '%s'>", class->name);
}

const struct sheep_type sheep_typeclass_type = {
	.free = typeclass_free,
	.call = typeclass_call,
	.format = typeclass_format,
};

sheep_t sheep_make_typeclass(struct sheep_vm *vm, const char *name,
			const char **names, unsigned int nr_slots)
{
	struct sheep_typeclass *class;

	class = sheep_malloc(sizeof(struct sheep_typeclass));
	class->name = sheep_strdup(name);
	class->names = names;
	class->nr_slots = nr_slots;

	return sheep_make_object(vm, &sheep_typeclass_type, class);
}
