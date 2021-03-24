/*
 *        Pseudo random number generator and bignum interfaces
 *        to pseudo and true random number generators.
 *        by Jack Lacy and D.P. Mitchell December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>
#include "rand.h"

/* Get a random number between a and b. */
void
getRandBetween(BigInt a, BigInt b, BigInt result, RandType type)
{
	BigInt T, slop, r, diff, p, q, one, two;
	int length;

	diff         = bigInit(0);
	p                = bigInit(0);
	q                = bigInit(0);
	one                = bigInit(1);
	two                = bigInit(2);
	
	if (bigCompare(b, a) > 0) {
		bigCopy(a, p);
		bigCopy(b, q);
	}
	else {
		bigCopy(b, p);
		bigCopy(a, q);
	}
	
	bigSubtract(q, p, diff);
	freeBignum(q);
	
	if (bigCompare(diff, two) < 0) {
		handle_exception(CRITICAL, "getRandBetween Error: numbers must differ by at least 2");
	}
	
	/* generate a random number between 0 and diff */
	T = bigInit(0);
	slop = bigInit(0);
	bigLeftShift(one, bigBits(diff), T);
	length = (LENGTH(T)*sizeof(NumType));
	
	bigMod(T, diff, slop);
	freeBignum(T);
	
	r = bigInit(0);
	do {
		bigRand(length, r, type);
	} while (bigCompare(r, slop) < 0);
	freeBignum(slop);
	
	bigMod(r, diff, result);
	freeBignum(r);
	freeBignum(diff);
	freeBignum(one);
	freeBignum(two);
	
	/* add smaller number back in */
	bigAdd(result, p, result);
	
	freeBignum(p);
}
