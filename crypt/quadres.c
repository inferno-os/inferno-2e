/*
 *	Code for determining quadratic residues and square roots
 *	modulo a prime and a product of 2 primes.
 *	By Jack Lacy
 *	Copyright (c) 1993 Bell Labs.
 */
#include "lib9.h"
#include <libcrypt.h>

/* a is a quad residue if x^2 mod n = a has a soln x.
 * if n is prime, a is a quadratic residue mod n if
 * a ^ (n-1)/2 mod n = 1 (for n prime)
 */

Boolean
quadResidue(BigInt a, BigInt n)
{
	BigInt d, q;
	Boolean retval;
	
	d = bigInit(0);
	q = bigInit(0);
	
	bigSubtract(n, one, q);
	bigRightShift(q, (int)1, q);
	bigPow(a, q, n, d);
	
	if (bigCompare(d, one) == 0)
		retval = TRUE;
	else
		retval = FALSE;
	
	freeBignum(d);
	freeBignum(q);
	
	return retval;
}

Boolean
compositeQuadResidue(BigInt a, BigInt p, BigInt q)
{
	if (quadResidue(a, p) == FALSE)
		return FALSE;
	else
		return quadResidue(a, q);
}

/* squareRoot() assumes a is a quadratic residue mod p and
 * that p is a prime of the form p = 4k + 3.  result is one
 * of the roots of a.  The other is (p-1)*result mod p.
 */
void
squareRoot(BigInt a, BigInt p, BigInt result)
{
	BigInt k;
	
	if ((p->num[0]&3) != 3)
		handle_exception(WARNING, "squareRoot: Prime must be of form, p mod 4 == 3.");
	
	k = bigInit(0);
	
	bigRightShift(p, (int)2, k);
	bigAdd(k, one, k);
	
	bigPow(a, k, p, result);
	
	freeBignum(k);
}

Boolean
valid_sqroot(BigInt res, BigInt a, BigInt n)
{
	BigInt tmp;
	Boolean retval;
	
	tmp = bigInit(0);
	bigMultiply(res, res, tmp);
	bigMod(tmp, n, tmp);
	
	if (bigCompare(tmp, a) == 0)
		retval = TRUE;
	else
		retval = FALSE;
	
	freeBignum(tmp);
	
	return retval;
}

void
compositeSquareRoot(BigInt a, BigInt p, BigInt q, BigInt r1, BigInt r2)
{
	BigInt srp, srq, nsrp, nsrq, c12;
	
	srp = bigInit(0);
	srq = bigInit(0);
	nsrp = bigInit(0);
	nsrq = bigInit(0);
	c12 = bigInit(0);
	
	squareRoot(a, p, srp);
	squareRoot(a, q, srq);
	
	negate(srp, p, nsrp);
	/*    negate(srq, q, nsrq);*/
	
	getInverse(p, q, c12);
	crtCombine(srp, srq, p, q, c12, r1);
	crtCombine(nsrp, srq, p, q, c12, r2);
	
	freeBignum(srp);
	freeBignum(srq);
	freeBignum(nsrp);
	freeBignum(nsrq);
	freeBignum(c12);
}
