#include <sheep/module.h>
#include <sheep/string.h>
#include <sheep/unpack.h>
#include <openssl/sha.h>
#include <sheep/util.h>
#include <sheep/vm.h>
#include <string.h>
#include <stdio.h>

static sheep_t sha1(struct sheep_vm *vm, unsigned int nr_args)
{
	char hexdigest[SHA_DIGEST_LENGTH * 2 + 1];
	unsigned char hash[SHA_DIGEST_LENGTH];
	const char *text;
	unsigned int i;

	if (sheep_unpack_stack(vm, nr_args, "S", &text))
		return NULL;

	SHA1((const unsigned char *)text, strlen(text), hash);

	for (i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf(hexdigest + i * 2, "%02x", hash[i]);

	return sheep_make_string(vm, hexdigest);
}

int init(struct sheep_vm *vm, struct sheep_module *module)
{
	sheep_module_function(vm, module, "sha1", sha1);
	return 0;
}
