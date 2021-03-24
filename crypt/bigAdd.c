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
bigAdd(BigInt a, BigInt b, BigInt c)
{
	if ((SIGN(a) == NEG) && (SIGN(b) == POS)) {
		if (bigCompare(b, a) > 0) {
			bigsub(b, a, c);
			SIGN(c) = POS;
		}
		else {
			bigsub(a, b, c);
			SIGN(c) = NEG;
		}
	}
	else if ((SIGN(b) == NEG) && (SIGN(a) == POS)) {
		if (bigCompare(a, b) > 0) {
			bigsub(a, b, c);
			SIGN(c) = POS;
		}
		else {
			bigsub(b, a, c);
			SIGN(c) = NEG;
		}
	}
	else {
		bigaddition(a, b, c);
		SIGN(c) = SIGN(a);
	}
}
