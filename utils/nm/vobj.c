/*
 * vobj.c
 *
 * Stub functions to hold the places of mips functions
 * required by obj.c. This architecture is not currently
 * supported.
 */

#include <lib9.h>
#include "bio.h"
#include "obj.h"

int
_isv(char *t)
{
	return 0;
}

int
_readv(Biobuf *bp, Prog *p)
{
	return 0;
}

