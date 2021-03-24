/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

void
bigCopy(BigInt a, BigInt b)
{
	BigData ap, bp;
	int i;
	
	GUARANTEE(b, LENGTH(a));
	i = LENGTH(a);
	LENGTH(b) = LENGTH(a);
	SIGN(b) = SIGN(a);
	ap = NUM(a) + i;
	bp = NUM(b) + i;
	
	ap -= (i&3);
	bp -= (i&3);
	switch (i&3) {
	case 3:  bp[2] = ap[2];
	case 2:  bp[1] = ap[1];
	case 1:  bp[0] = ap[0];
	case 0:
		i -= 4;
	}
	while (i >= 0) {
		ap -= 4;
		bp -= 4;
		bp[3] = ap[3];
		bp[2] = ap[2];
		bp[1] = ap[1];
		bp[0] = ap[0];
		i -= 4;
	}
}
