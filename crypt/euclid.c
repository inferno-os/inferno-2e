/*
 *        Euclid's extended gcd algorithm from Knuth Vol 2.
 *
 *        coded by Jack Lacy, December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

void
extendedGcd(BigInt u, BigInt v, BigInt up, BigInt vp, BigInt gcd)
{
	BigInt u1, u2, u3, v1, v2, v3, t1, t2, t3;
	BigInt q, rem, u1save, u2save, u3save;
	
	if (ZERO(u) || ZERO(v))  {
		reset_big(gcd, (NumType)0);
		return;
	}
	
	q = bigInit(0);
	rem = bigInit(0);
	
	u1 = bigInit(1);
	u2 = bigInit(0);
	u3 = bigInit(0);
	bigCopy(u, u3);
	
	v1 = bigInit(0);
	v2 = bigInit(1);
	v3 = bigInit(0);
	bigCopy(v, v3);
	
	t1 = bigInit(0);
	t2 = bigInit(0);
	t3 = bigInit(0);
	
	while (!ZERO(v3)) {
		bigDivide(u3, v3, q, rem);
		
		bigMultiply(v1, q, t1);
		bigMultiply(v2, q, t2);
		bigMultiply(v3, q, t3);
		
		bigSubtract(u1, t1, t1);
		bigSubtract(u2, t2, t2);
		bigSubtract(u3, t3, t3);
		
		u1save = u1;
		u2save = u2;
		u3save = u3;
		
		u1 = v1;
		u2 = v2;
		u3 = v3;
		
		v1 = t1;
		v2 = t2;
		v3 = t3;
		
		t1 = u1save;
		t2 = u2save;
		t3 = u3save;
	}
	
	bigCopy(u1, up);
	bigCopy(u2, vp);
	bigCopy(u3, gcd);
	
	freeBignum(u1);
	freeBignum(u2);
	freeBignum(u3);
	freeBignum(v1);
	freeBignum(v2);
	freeBignum(v3);
	freeBignum(t1);
	freeBignum(t2);
	freeBignum(t3);
	freeBignum(q);
	freeBignum(rem);
}

void
getInverse(BigInt u, BigInt v, BigInt up)
{
	BigInt u1, u3, v1, v3, t1, t3;
	BigInt q, rem, u1save, u3save;
	
	q = bigInit(0);
	rem = bigInit(0);
	
	u1 = bigInit(1);
	u3 = bigInit(0);
	bigCopy(u, u3);
	
	v1 = bigInit(0);
	v3 = bigInit(0);
	bigCopy(v, v3);
	
	t1 = bigInit(0);
	t3 = bigInit(0);
	
	while (!ZERO(v3)) {
		
		bigDivide(u3, v3, q, rem);
		
		bigMultiply(v1, q, t1);
		bigMultiply(v3, q, t3);
		
		bigSubtract(u1, t1, t1);
		bigSubtract(u3, t3, t3);
		
		u1save = u1;
		u3save = u3;
		
		u1 = v1;
		u3 = v3;
		
		v1 = t1;
		v3 = t3;
		
		t1 = u1save;
		t3 = u3save;
	}
	
	bigCopy(u1, up);
	
	freeBignum(u1);
	freeBignum(u3);
	freeBignum(v1);
	freeBignum(v3);
	freeBignum(t1);
	freeBignum(t3);
	freeBignum(q);
	freeBignum(rem);
}



BigInt
gcd(BigInt a, BigInt b)
{
	BigInt aa, bb, gcd;
	
	aa = bigInit(0);
	bb = bigInit(0);
	gcd = bigInit(0);
	
	extendedGcd(a, b, aa, bb, gcd);
	
	freeBignum(aa);
	freeBignum(bb);
	
	return gcd;
	
}
