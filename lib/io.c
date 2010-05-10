/*
 * lib/io.c
 *
 * Copyright (c) 2010 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/module.h>
#include <sheep/object.h>
#include <sheep/string.h>
#include <sheep/unpack.h>
#include <sheep/bool.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

struct file {
	FILE *filp;
	int write;
};

static int file_close(struct file *file)
{
	if (!file->filp)
		return 0;
	fclose(file->filp);
	file->filp = NULL;
	return 1;
}

static void file_free(struct sheep_vm *vm, sheep_t sheep)
{
	struct file *file;

	file = sheep_data(sheep);
	file_close(file);
	sheep_free(file);
}

static void file_format(sheep_t sheep, struct sheep_strbuf *sb, int repr)
{
	sheep_strbuf_addf(sb, "#<file '%p'>", sheep);
}

static const struct sheep_type file_type = {
	.free = file_free,
	.format = file_format,
};

/* (open pathname mode) */
static sheep_t open(struct sheep_vm *vm, unsigned int nr_args)
{
	struct file *file;
	const char *path;
	sheep_t write;
	FILE *filp;

	if (sheep_unpack_stack("open", vm, nr_args, "Sb", &path, &write))
		return NULL;

	if (sheep_test(write))
		filp = fopen(path, "w");
	else
		filp = fopen(path, "r");

	if (!filp) {
		fprintf(stderr, "%s: can not open file\n", path);
		return NULL;
	}

	file = sheep_malloc(sizeof(struct file));
	file->filp = filp;
	file->write = sheep_test(write);

	return sheep_make_object(vm, &file_type, file);
}

/* (close file) */
static sheep_t close(struct sheep_vm *vm, unsigned int nr_args)
{
	struct file *file;

	if (sheep_unpack_stack("close", vm, nr_args, "T", &file_type, &file))
		return NULL;

	if (file_close(file))
		return &sheep_true;
	return &sheep_false;
}

/* (read file number-of-bytes) */
static sheep_t read(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned long nr_bytes;
	struct file *file;
	char *buf;

	if (sheep_unpack_stack("read", vm, nr_args, "TN",
				&file_type, &file, &nr_bytes))
		return NULL;

	if (!file->filp) {
		fprintf(stderr, "reading from closed file\n");
		return NULL;
	}

	buf = sheep_malloc(nr_bytes + 1);
	nr_bytes = fread(buf, 1, nr_bytes, file->filp);
	buf[nr_bytes] = 0;

	return __sheep_make_string(vm, buf, nr_bytes);
}

/* (write file string) */
static sheep_t write(struct sheep_vm *vm, unsigned int nr_args)
{
	unsigned long nr_bytes;
	const char *string;
	struct file *file;

	if (sheep_unpack_stack("write", vm, nr_args, "TS",
				&file_type, &file, &string))
		return NULL;

	if (!file->filp) {
		fprintf(stderr, "writing to closed file\n");
		return NULL;
	}

	nr_bytes = strlen(string);
	nr_bytes = fwrite(string, 1, nr_bytes, file->filp);

	return sheep_make_number(vm, nr_bytes);
}

int init(struct sheep_vm *vm, struct sheep_module *module)
{
	sheep_module_function(vm, module, "open", open);
	sheep_module_function(vm, module, "close", close);
	sheep_module_function(vm, module, "read", read);
	sheep_module_function(vm, module, "write", write);
	return 0;
}
