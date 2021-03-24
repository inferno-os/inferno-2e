/*
 *        Prime number generator and tests utilizing Rabin's
 *        compositeness test and Gordon's strong prime concept.
 *
 *        coded by Jack Lacy, December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

static int long_log(int);
static int fakelog(int);
static Boolean probPrimeTest(BigInt, BigInt*);
static Boolean first53Test(BigInt);
static Boolean genGordonPrimeSet(int, BigInt, int, BigInt);
static Boolean genNISTPrimeSet(int, BigInt, int, BigInt);

static int first_53_primes[53] = {
        3, 5, 7, 11, 13, 17, 19, 23, 29, 31,
        37, 41, 43, 47, 53, 59, 61, 67, 71, 73,
        79, 83, 89, 97, 101, 103, 107, 109, 113, 127,
        131, 137, 139, 149, 151, 157, 163, 167, 173, 179,
        181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
        239, 241, 251,
};

#define RLEN(a, i) ((a >= i) && (a < 2*i))

static int
long_log(int n)
{
	
	int i, j;
	
	i = 8192;
	j = 13;
	for(;;) {
		if (RLEN(n, i))
			break;
		else {
			j++;
			i *= 2;
		}
	}
	return j;
}

static int
fakelog(int n)
{
	
	switch(n) {
	case 2:    return(1);
	case 4:    return(2);
	case 8:    return(3);
	case 16:   return(4);
	case 32:   return(5);
	case 64:   return(6);
	case 128:  return(7);
	case 256:  return(8);
	case 512:  return(9);
	case 1024: return(10);
	case 2048: return(11);
	case 4096: return(12);
	default:
		if (RLEN(n, 2))
			return fakelog(2);
		else if (RLEN(n, 4))
			return fakelog(4);
		else if (RLEN(n, 8))
			return fakelog(8);
		else if (RLEN(n, 16))
			return fakelog(16);
		else if (RLEN(n, 32))
			return fakelog(32);
		else if (RLEN(n, 64))
			return fakelog(64);
		else if (RLEN(n, 128))
			return fakelog(128);
		else if (RLEN(n, 256))
			return fakelog(256);
		else if (RLEN(n, 512))
			return fakelog(512);
		else if (RLEN(n, 1024))
			return fakelog(1024);
		else if (RLEN(n, 2048))
			return fakelog(2048);
		else
			return long_log(n);
	}
}

int primeTestAttempts = 5;

void
setPrimeTestAttempts(int i)
{
	primeTestAttempts = i;
}


static Boolean
probPrimeTest(BigInt n, BigInt *bigs)
{
	Boolean retval = FALSE;
	int j, k = 0;
	BigInt nminus1, x, y, q;
	
	nminus1 = bigs[0];
	q       = bigs[1];
	x       = bigs[2];
	y       = bigs[3];

	bigsub(n, one, nminus1);
	bigCopy(nminus1, q);
	
	while (even(q)) {
		k++;
		bigRightShift(q, (int)1, q);
	}
	
	getRandBetween(one, n, x, PSEUDO);
	bigPow(x, q, n, y);
	
	j = 0;
	if (bigCompare(y, one) == 0) {
		retval = TRUE;
	}
	
	while ((j < k) && (retval == FALSE)) {
		if (bigCompare(y, nminus1) == 0) {
			retval = TRUE;
			break;
		}
		if (bigCompare(y, one) == 0) {
			retval = FALSE;
			break;
		}
		j++;
		bigMultiply(y, y, q);
		bigMod(q, n, y);
	}
	return retval;
}


static  Boolean
first53Test(BigInt n)
{
	BigData np;
	int i, j;
	int N;
	ulong m, divisor;
	ushort m2;
	
	N = (int)LENGTH(n);
	np = NUM(n);
	i = 0;
	do {
		divisor = first_53_primes[i++];
		m = 0;
		for (j=N-1; j >= 0; j--) {
			m2 = (ushort)(np[j]>>16) & 0xffff;
			m = ((m<<16) + m2)%divisor;
			m2 = (ushort)(np[j] & 0xffff);
			m = ((m<<16) + m2)%divisor;
		}
		if (m == 0) {
			return(FALSE);
		}
	} while (i < 53);
	return(TRUE);
}


static Boolean
primeTest1(BigInt n, BigInt *bigs)
{
	int k;
	int accuracy;
	BigData np = NUM(n);
	
	if (LENGTH(n) == 1) {
		k = 0;
		do {
			if (np[0] == (NumType)first_53_primes[k++])
				return(TRUE);
		} while (k < 53);
	}
	
	if (even(n))
		return(FALSE);
	
	if (first53Test(n) == FALSE)
		return(FALSE);
	
	accuracy = primeTestAttempts;
	while ((accuracy > 0) && probPrimeTest(n, bigs) == TRUE)
		accuracy--;
	
	if (accuracy == 0)
		return(TRUE);
	else
		return(FALSE);
}

static BigInt*
getprimebigs(void)
{
	BigInt *bigs;

	bigs = crypt_malloc(4*sizeof(BigInt));
	bigs[0] = bigInit(0);
	bigs[1] = bigInit(0);
	bigs[2] = bigInit(0);
	bigs[3] = bigInit(0);
	return bigs;
}

static void
freeprimebigs(BigInt *bigs)
{
	freeBignum(bigs[0]);
	freeBignum(bigs[1]);
	freeBignum(bigs[2]);
	freeBignum(bigs[3]);
	free(bigs);
}

Boolean
primeTest(BigInt n)
{
	BigInt *bigs;
	Boolean rv;

	bigs = getprimebigs();
	rv = primeTest1(n, bigs);
	freeprimebigs(bigs);

	return rv;
}

void
getPrime(int numbits, BigInt prime)
{
	int numbytes;
	int shiftdiff;
	BigInt *bigs;
	
	numbytes = (numbits/8);
	if (numbits%8 != 0)
		numbytes++;
	
	bigRand(numbytes, prime, PSEUDO);
	
	shiftdiff = (bigBits(prime) - numbits);
	if (shiftdiff > 0)
		bigRightShift(prime, (int)shiftdiff, prime);
	else if (shiftdiff < 0)
		bigLeftShift(prime, (int)(-shiftdiff), prime);
	
	if (EVEN(prime)) {
		NUM(prime)[0] |= 1;
	}

	bigs = getprimebigs();
	while (1) {
		if (primeTest1(prime, bigs) == TRUE)
			break;
		NUM(prime)[0] += 2;
	}
	freeprimebigs(bigs);
}

/*
   Generate strong primes using Gordon's method and
   Rabin's probabilistic primality test.  This function
   returns the strong prime p as well as r, a prime factor
   of p-1.
   */
static Boolean
genGordonPrimeSet(int numbits, BigInt prime, int factorbits, BigInt factor)
{
	BigInt r, pzero, twors;
	BigInt s, t, p, rs, ss, rr;
	BigInt rminus1, sminus1, twot;
	int pbits, tbits, rbits, sbits;
	Boolean found;
	BigInt *bigs;
	
	if (numbits < 32)
		return genNISTPrimeSet(numbits, prime, factorbits, factor);
	
	/* the two return values */
	p = prime;
	r = factor;
	
	pbits = numbits;
	rbits = (pbits - fakelog(pbits) - 1);
	sbits = rbits/2;
	rbits = sbits;
	tbits = rbits - fakelog(rbits) - 1;
	
	s = bigInit(0);
	t = bigInit(0);
	getPrime(sbits, s);
	getPrime(tbits, t);
	
	/* find r -- r = 2Lt + 1 and is prime */
	twot = bigInit(0);
	bigLeftShift(t, 1, twot);
	
	reset_big(r, 1);
	bigAdd(r, twot, r);

	bigs = getprimebigs();
	while (primeTest1(r, bigs) == FALSE)
		bigAdd(r, twot, r);
	
	freeBignum(t);
	freeBignum(twot);
	
	/*
	   find p -- p = p0 + 2krs where:
	   p0 = u(r,s);        u(r,s) odd
	   p0 = u(r,s) + rs;   u(r,s) even
	   u(r,s) = (s^(r-1) - r^(s-1))mod rs.
	   */
	rs = bigInit(0);
	rminus1 = bigInit(0);
	sminus1 = bigInit(0);
	
	bigMultiply(r, s, rs);
	bigSubtract(r, one, rminus1);
	bigSubtract(s, one, sminus1);
	
	ss = bigInit(0);
	rr = bigInit(0);
	bigPow(s, rminus1, rs, ss);
	bigPow(r, sminus1, rs, rr);
	
	pzero = bigInit(0);
	bigSubtract(ss, rr, pzero);
	
	if (SIGN(pzero) == NEG) {
		negate(pzero, rs, pzero);
	}
	
	if (EVEN(pzero))
		bigAdd(pzero, rs, pzero);
	
	freeBignum(s);
	freeBignum(ss);
	freeBignum(rr);
	freeBignum(rminus1);
	freeBignum(sminus1);
	
	twors = bigInit(0);
	bigLeftShift(rs, 1, twors);
	freeBignum(rs);
	
	reset_big(p, (NumType)0);
	bigAdd(pzero, p, p);
	
	while (bigBits(p) < pbits)
		bigAdd(p, twors, p);
	
	found = TRUE;
	
	while (primeTest1(p, bigs) == FALSE) {
		bigAdd(p, twors, p);
		if (bigBits(p) > pbits) {
			found = FALSE;
			break;
		}
	}

	freeprimebigs(bigs);
	
	freeBignum(twors);
	
#ifndef NDEBUG
	if (found) {
		BigInt pminus1;
		/* verify that p and r are ok */
		pminus1 = bigInit(0);
		bigSubtract(p, one, pminus1);
		bigMod(pminus1, r, pzero);
		freeBignum(pminus1);
		
		if (bigCompare(pzero, zero) != 0)
			handle_exception(WARNING, "genGordonPrimeSet: DEBUG: pzero non-zero.");
	}
#endif
	
	freeBignum(pzero);
	
	return found;
}

static Boolean
genNISTPrimeSet(int numbits, BigInt prime, int facbits, BigInt factor)
{
	int qlen;
	BigInt p, q, twoq, tmp, ignore, n;
	BigInt smallseed, bigseed;
	Boolean primeVal;
	BigInt *bigs;

	p                = prime;
	q                = factor;
	twoq        = bigInit(0);
	tmp                = bigInit(0);
	ignore        = bigInit(0);
	n                = bigInit(0);
	smallseed        = bigInit(0);
	bigseed        = bigInit(0);
	
	qlen = facbits;
	/*
	   if (numbits >= 512)
	   qlen = 160;
	   else
	   qlen = (numbits/2) - ((3*numbits)/16);
	   */
	getPrime(qlen, q);
	bigLeftShift(q, (int)1, twoq);
	
	bigLeftShift(one, numbits, tmp);
	bigSubtract(tmp, one, tmp);
	bigDivide(tmp, twoq, bigseed, ignore);
	
	bigLeftShift(one, (int)(numbits-1), tmp);
	bigSubtract(tmp, one, tmp);
	bigDivide(tmp, twoq, smallseed, ignore);
	
	freeBignum(tmp);
	freeBignum(ignore);
	
	primeVal = FALSE;
	getRandBetween(bigseed, smallseed, n, PSEUDO);
	bigSubtract(n, one, n);

	bigs = getprimebigs();
	while (primeVal == FALSE) {
		bigAdd(n, one, n);
		bigMultiply(n, twoq, p);
		bigAdd(p, one, p);
		primeVal = primeTest1(p, bigs);
	}
	freeprimebigs(bigs);
	freeBignum(smallseed);
	freeBignum(bigseed);
	freeBignum(twoq);
	freeBignum(n);
	
	return primeVal;
	
}


void
genStrongPrimeSet(int numbits, BigInt prime, int facbits, BigInt factor, PrimeType type)
{
	if (type == NIST)
		genNISTPrimeSet(numbits, prime, facbits, factor);
	else if (type == GORDON) {
		while (genGordonPrimeSet(numbits, prime, facbits, factor) == FALSE) {
			reset_big(prime, (NumType)0);
			reset_big(factor, (NumType)0);
		}
	}
}

void
genStrongPrime(int numbits, BigInt prime)
{
	BigInt factor;
	
	factor = bigInit(0);
	
	genStrongPrimeSet(numbits, prime, 160, factor, NIST);
	
	freeBignum(factor);
}

/* f is a factor of p-1 */
void
getPrimitiveElement(BigInt a, BigInt p, BigInt f)
{
	BigInt pminus1, tmp, d;
	
	pminus1 = bigInit(0);
	tmp = bigInit(0);
	d = bigInit(0);
	
	bigSubtract(p, one, pminus1);
	bigDivide(pminus1, f, d, tmp);
	getRandBetween(p, one, a, PSEUDO);
	bigPow(a, d, p, tmp);
	while (bigCompare(tmp, one) == 0) {
		bigAdd(a, one, a);
		bigPow(a, d, p, tmp);
	}
	
	freeBignum(pminus1);
	freeBignum(tmp);
	freeBignum(d);
}

