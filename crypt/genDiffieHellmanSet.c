/*
 *        Code for generating and manipulating El Gamal keys
 *        and doing encryption and decryption using El Gamal
 *        and generating and verifying digital signatures.
 *
 *        coded by Jack Lacy, December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

void
genDiffieHellmanSet(BigInt p, BigInt alpha, int primelen, int subprimelen)
{
	EGParams *params;
	
	params = genEGParams(primelen, subprimelen);
	
	bigCopy(params->p, p);
	bigCopy(params->alpha, alpha);
	
	freeEGParams(params);
}

