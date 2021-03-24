/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>


void
crtCombine(BigInt a, BigInt b, BigInt p, BigInt q, BigInt c12, BigInt result)
{
	BigInt u1, u2, tmp;
	
	u1 = bigInit(0);
	u2 = bigInit(0);
	tmp = bigInit(0);
	
	bigCopy(a, u1);
	bigCopy(b, u2);
	
	bigSubtract(u2, u1, u2);
	bigMultiply(c12, u2, tmp);
	bigMod(tmp, q, tmp);
	bigMultiply(tmp, p, result);
	bigAdd(result, u1, result);
	if (SIGN(result) == NEG) {
		bigMultiply(p, q, tmp);
		negate(result, tmp, result);
	}
	
	freeBignum(u1);
	freeBignum(u2);
	freeBignum(tmp);
}
