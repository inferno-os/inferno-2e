/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

void
bigAnd(BigInt a,  BigInt b, BigInt c)
{
	BigData ap, bp, cp;
	int i;
	
	if (LENGTH(a) > LENGTH(b))
		i = LENGTH(b);
	else
		i = LENGTH(a);
	GUARANTEE(c, (ulong)i);
	LENGTH(c) = (ulong)i;
	SIGN(c) = SIGN(a);
	ap = NUM(a);
	bp = NUM(b);
	cp = NUM(c);
	
	ap -= 4 - (i&3);
	bp -= 4 - (i&3);
	cp -= 4 - (i&3);
	switch (i&3) {
	case 3:  cp[1] = ap[1] & bp[1];
	case 2:  cp[2] = ap[2] & bp[2];
	case 1:  cp[3] = ap[3] & bp[3];
	case 0:
		ap += 4;
		bp += 4;
		cp += 4;
		i -= 4;
	};
	while (i >= 0) {
		cp[0] = ap[0] & bp[0];
		cp[1] = ap[1] & bp[1];
		cp[2] = ap[2] & bp[2];
		cp[3] = ap[3] & bp[3];
		ap += 4;
		bp += 4;
		cp += 4;
		i -= 4;
	}
}
