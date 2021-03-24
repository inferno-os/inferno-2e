/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>
#include "bigmath.h"

void
negate(BigInt a, BigInt p, BigInt result)
{
	BigInt minus1, tmp, one;
	
	if ((bigCompare(p, a) > 0) && (SIGN(a) == POS)) {
		bigSubtract(p, a, result);
	}
	else {
		minus1 = bigInit(0);
		tmp = bigInit(0);
		one = bigInit(1);
		
		bigSubtract(p, one, minus1);
		bigMultiply(a, minus1, tmp);
		
		bigMod(tmp, p, result);
		
		SIGN(result) = POS;
		
		freeBignum(minus1);
		freeBignum(tmp);
		freeBignum(one);
	}
}
