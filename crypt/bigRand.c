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
bigRand(int numbytes, BigInt big, RandType type)
{
	if (type == REALLY)
		bigReallyRand(numbytes, big);
	else
		bigPseudoRand(numbytes, big);
}
