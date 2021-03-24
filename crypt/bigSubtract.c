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
bigSubtract(BigInt a1, BigInt a2, BigInt ret)
{
	Sign sign1, sign2;
	
	sign1 = SIGN(a1);
	sign2 = SIGN(a2);
	
	if ((sign1 == POS) && (sign2 == POS)) {
		if (bigCompare(a1, a2) >= 0) {
			bigsub(a1, a2, ret);
			SIGN(ret) = POS;
			return;
		}
		else {
			bigsub(a2, a1, ret);
			SIGN(ret) = NEG;
			return;
		}
	}
	else if ((sign1 == POS) && (sign2 == NEG)) {
		bigaddition(a1, a2, ret);
		SIGN(ret) = POS;
		return;
	}
	else if ((sign1 == NEG) && (sign2 == NEG)) {
		if (bigCompare(a1, a2) >= 0) {
			bigsub(a1, a2, ret);
			SIGN(ret) = NEG;
			return;
		}
		else {
			bigsub(a2, a1, ret);
			SIGN(ret) = POS;
			return;
		}
	}
	else if ((sign1 == NEG) && (sign2 == POS)) {
		bigaddition(a1, a2, ret);
		SIGN(ret) = NEG;
		return;
	}
}
