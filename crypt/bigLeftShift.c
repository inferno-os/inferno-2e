/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>
#include "bigmath.h"

#define LSHIFT(i) {\
			    el = ap[i];\
			    s = (NumType)(el << n);\
			    bp[i] = s + c;\
			    c = (NumType)(el >> (NumTypeBits-n));\
	    }

static void
bl_shift(BigInt a, BigInt b, int n)
{
	int i;
	BigData ap, bp;
	NumType s, c;
	NumType el;
	
	i = LENGTH(a);
	GUARANTEE(b, (ulong)(i + 1));
	
	c = 0;
	ap = NUM(a);
	bp = NUM(b);
	
	ap -= 4 - (i&3);
	bp -= 4 - (i&3);
	switch(i&3) {
	case 3:        LSHIFT(1);
	case 2:        LSHIFT(2);
	case 1:        LSHIFT(3);
	case 0:
		ap += 4;
		bp += 4;
		i -= 4;
	}
	while (i >= 0) {
		LSHIFT(0);
		LSHIFT(1);
		LSHIFT(2);
		LSHIFT(3);
		ap += 4;
		bp += 4;
		i -= 4;
	}
	
	*bp = c;
	LENGTH(b) = LENGTH(a)+1;
	
	trim(b);
	
}

void
bigLeftShift(BigInt a, int n, BigInt b)
{
	int i;
	
	if (n == 0) {
		if (a != b)
			bigCopy(a, b);
		return;
	}
	
	i = (n/NumTypeBits);
	GUARANTEE(b, LENGTH(a) + (ulong)(i+1));
	if (i > 0) {
		bigleftshift(a, b, i);
		if (n%NumTypeBits)
			bl_shift(b, b, n%NumTypeBits);
	}
	else
		bl_shift(a, b, n%NumTypeBits);
	
	trim(b);
}
