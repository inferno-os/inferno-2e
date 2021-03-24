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

EGPrivateKey *
genEGPrivateKey(EGParams *params)
{
	EGPrivateKey *key;

	key = (EGPrivateKey *)crypt_malloc(sizeof(EGPrivateKey));
	
	key->p = params->p;
	key->q = params->q;
	key->alpha = params->alpha;
	key->publicKey = bigInit(0);
	key->secret = bigInit(0);
	
	getRandBetween(params->q, one, key->secret, PSEUDO);
	if (EVEN(key->secret))
		bigAdd(key->secret, one, key->secret);
	
	bigPow(params->alpha, key->secret, params->p, key->publicKey);
	
	return key;
}
