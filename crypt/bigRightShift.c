/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>
#include "bigmath.h"


#define BITSHIFT(i) {\
			     el = ap[i];\
			     s = el >> n;\
			     bp[i] = s + c;\
			     c = el << j;\
	     }

static void
br_shift(BigInt a, BigInt b, int n)
{
	BigData ap, bp;
	int i,j;
	NumType s, c;
	NumType el;
	
	GUARANTEE(b, LENGTH(a));
	LENGTH(b) = LENGTH(a);
	i = LENGTH(a);
	ap = NUM(a);
	bp = NUM(b);
	j = NumTypeBits - n;
	c = 0;
	
	ap += i - (i&3);
	bp += i - (i&3);
	switch (i & 3) {
	case 3:            BITSHIFT(2);
	case 2:            BITSHIFT(1);
	case 1:            BITSHIFT(0);
	case 0:
		i -= 4;
		ap -= 4;
		bp -= 4;
	}
	while (i >= 0) {
		BITSHIFT(3);
		BITSHIFT(2);
		BITSHIFT(1);
		BITSHIFT(0);
		i -= 4;
		ap -= 4;
		bp -= 4;
	}
	
	trim(b);
}

void
bigRightShift(BigInt a, int n, BigInt b)
{
	
	bigrightshift(a, b, n/NumTypeBits);
	
	if (n%NumTypeBits)
		br_shift(b, b, n%NumTypeBits);
	
	trim(b);
}
