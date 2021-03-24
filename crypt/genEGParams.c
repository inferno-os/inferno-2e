/*
 *        Code for generating and manipulating El Gamal keys
 *        and doing encryption and decryption using El Gamal
 *        and generating and verifying digital signatures.
 *
 *        coded by Jack Lacy, December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

/*  Uses NIST's structure for keys. */
EGParams *
genEGParams(int primeLen, int subprimelen)
{
	EGParams *params;
	BigInt h, quotient, pminus1, ignore, p, q, alpha;
	params = (EGParams *)crypt_malloc(sizeof(EGParams));
	
	p	= bigInit(0);
	q	= bigInit(0);
	alpha	= bigInit(1);
	h	= bigInit(0);
	quotient	= bigInit(0);
	ignore	= bigInit(0);
	pminus1	= bigInit(0);
	
	genStrongPrimeSet(primeLen, p, subprimelen, q, NIST);
	
	bigSubtract(p, one, pminus1);
	bigDivide(pminus1, q, quotient, ignore);
	while (bigCompare(alpha, one) == 0) {
		getRandBetween(pminus1, zero, h, PSEUDO);
		bigPow(h, quotient, p, alpha);
	}
	
	freeBignum(h);
	freeBignum(quotient);
	freeBignum(pminus1);
	freeBignum(ignore);
	
	params->p = p;
	params->q = q;
	params->alpha = alpha;
	
	return params;
}
