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
bigPseudoRand(int  numbytes, BigInt num)
{
	int nunits, i;
	BigData np;
	ulong mask;
	
	nunits = (numbytes/sizeof(NumType));
	
	if (numbytes % sizeof(NumType))
		nunits++;
	GUARANTEE(num, (ulong)nunits);
	LENGTH(num) = (ulong)nunits;
	
	i = numbytes%sizeof(NumType);
	switch(i) {
	default:
		mask = 0xffffffUL;
		break;
	case 2:
		mask = 0xffffUL;
		break;
	case 1:
		mask = 0xffUL;
		break;
	case 0:
		mask = 0xffffffffUL;
		break;
	}
	
	np = NUM(num);
	for (i=0; i<LENGTH(num); i++)
		np[i] = pseudoRandom();
	
	np[i-1] &= mask;
}
