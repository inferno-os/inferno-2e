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
bigReallyRand(int numbytes, BigInt num)
{
	uchar *buf;
	int nunits;
	
	nunits = (numbytes/sizeof(NumType));
	if (numbytes % sizeof(NumType))
		nunits++;
	GUARANTEE(num, (ulong)nunits);
	LENGTH(num) = (ulong)nunits;
	
	buf = (uchar *)crypt_malloc(numbytes);
	reallyRandomBytes(buf, numbytes);
	bufToBig(buf, numbytes, num);
	crypt_free((char *)buf);
}

