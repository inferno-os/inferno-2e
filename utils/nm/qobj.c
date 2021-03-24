/*
 * qobj.c
 *
 * Stub functions to hold the places of Power PC functions
 * required by obj.c. This architecture is not currently
 * supported.
 */

#include <lib9.h>
#include "bio.h"
#include "obj.h"

int
_isq(char *t)
{
	return 0;
}

int
_readq(Biobuf *bp, Prog *p)
{
	return 0;
}

