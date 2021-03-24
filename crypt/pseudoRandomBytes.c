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


void
pseudoRandomBytes(uchar *buf, int numbytes)
{
	int i;
	uchar *bp;
	ulong num;
	
	i = numbytes;
	bp = buf;
	bp -= (4 - (i&3));

	num = 0;	
	if (i&3)
		num = pseudoRandom();
	switch(i&3) {
	case 3:
		bp[1] = (uchar)((num>>8) & 0xff);
	case 2:
		bp[2] = (uchar)((num>>16) & 0xff);
	case 1:
		bp[3] = (uchar)((num>>24) & 0xff);
	case 0:
		bp += 4;
		i -= 4;
	}
	while (i >= 0) {
		num = pseudoRandom();
		bp[0] = (uchar)(num & 0xff);
		bp[1] = (uchar)((num>>8) & 0xff);
		bp[2] = (uchar)((num>>16) & 0xff);
		bp[3] = (uchar)((num>>24) & 0xff);
		bp += 4;
		i -= 4;
	}
}

