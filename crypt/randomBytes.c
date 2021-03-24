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


int
randomBytes(uchar *buf, int numbytes, RandType type)
{
	
	if (type == REALLY)
		reallyRandomBytes(buf, numbytes);
	else
		pseudoRandomBytes(buf, numbytes);
	
	return 1;
}
