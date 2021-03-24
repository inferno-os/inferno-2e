/*
 *  Modular exponentiation using Montgomery reduction and
 *  Addition chaining.
 *  coded by Jack Lacy 11/91.
 *  Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

void
bigCube(BigInt a, BigInt m, BigInt result)
{
	BigInt d;
	
	d = bigInit(0);
	bigMultiply(a, a, d);
	bigMod(d, m, d);
	bigMultiply(d, a, result);
	bigMod(result, m, result);
	
	freeBignum(d);
}
