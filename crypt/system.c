#include "lib9.h"
#include <libcrypt.h>

/*
 *  interface to system
 */

void
handle_exception(int type, char *exception)
{
	if (type == CRITICAL)
		print("crypt library error: %s\n", exception);
	else
		print("crypt library warning: %s\n", exception);
}

void*
crypt_malloc(int size)
{
	void *x;

	x = malloc(size);
	if(x == 0)
		handle_exception(CRITICAL, "out of memory");
	return x;
}

char*
crypt_strdup(char *str)
{
	char *x;

	x = crypt_malloc(strlen(str)+1);
	strcpy(x, str);
	return x;
}

void
crypt_free(void *x)
{
	if(x == 0)
		handle_exception(CRITICAL, "freeing null pointer");
	free(x);
}
