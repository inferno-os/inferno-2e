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

static EGPublicKey *
buildEGPublicKey(EGParams *params, BigInt x)
{
	EGPublicKey *key;
	key = (EGPublicKey *)crypt_malloc(sizeof(EGPublicKey));
	
	key->p = params->p;
	key->q = params->q;
	key->alpha = params->alpha;
	key->publicKey = bigInit(0);
	bigPow(params->alpha, x, params->p, key->publicKey);
	
	return key;
}


static EGPrivateKey *
buildEGPrivateKey(EGParams *params, BigInt secret)
{
	EGPrivateKey *key;
	key = (EGPrivateKey *)crypt_malloc(sizeof(EGPrivateKey));
	
	key->p = params->p;
	key->q = params->q;
	key->alpha = params->alpha;
	key->publicKey = bigInit(0);
	key->secret = secret;
	
	bigPow(params->alpha, secret, params->p, key->publicKey);
	
	return key;
}

EGKeySet *
genEGKeySet(EGParams *params)
{
	EGKeySet *ks;
	EGParams *params1, *params2;
	BigInt secret;

	params1 = (EGParams *)crypt_malloc(sizeof(EGParams));
	params2 = (EGParams *)crypt_malloc(sizeof(EGParams));
	params1->p = bigInit(0);
	bigCopy(params->p, params1->p);
	params1->q = bigInit(0);
	bigCopy(params->q, params1->q);
	params1->alpha = bigInit(0);
	bigCopy(params->alpha, params1->alpha);
	
	params2->p = bigInit(0);
	bigCopy(params->p, params2->p);
	params2->q = bigInit(0);
	bigCopy(params->q, params2->q);
	params2->alpha = bigInit(0);
	bigCopy(params->alpha, params2->alpha);
	
	ks = (EGKeySet *)crypt_malloc(sizeof(EGKeySet));
	secret = bigInit(0);
	getRandBetween(params->q, one, secret, REALLY);
	if (EVEN(secret))
		bigAdd(secret, one, secret);
	
	ks->publicKey = buildEGPublicKey(params1, secret);
	ks->privateKey = buildEGPrivateKey(params2, secret);

#ifdef BRICKELL
	ks->publicKey->g_table = g16_bigpow(ks->publicKey->alpha, params1->p, 8*LENGTH(params1->q));
	ks->publicKey->y_table = g16_bigpow(ks->publicKey->publicKey, params1->p, 8*LENGTH(params1->q));
	ks->privateKey->g_table = g16_bigpow(ks->privateKey->alpha, params1->p, 8*LENGTH(params1->q));
#endif BRICKELL
	
	return ks;
	
}
