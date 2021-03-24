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

/* result = (c1 << bits(p) + c2) */
BigInt
EGEncrypt(BigInt message, EGPublicKey *key)
{
	BigInt x, K, c1, c2;
	BigInt result;
	
	x = bigInit(0);
	getRandBetween(key->q, one, x, PSEUDO);
	if (EVEN(x))
		bigAdd(x, one, x);
	
	K = bigInit(0);
#ifdef BRICKELL
	brickell_bigpow(key->y_table, x, key->p, K);
#else
	bigPow(key->publicKey, x, key->p, K);
#endif
	
	result = bigInit(0);
	c1 = bigInit(0);
	c2 = bigInit(0);
	
#ifdef BRICKELL
	brickell_bigpow(key->g_table, x, key->p, c1);
#else
	bigPow(key->alpha, x, key->p, c1);
#endif
	
	/*
	   bigMultiply(message, K, c2);
	   bigMod(c2, key->p, c2);
	   */
	bigXor(message, K, c2);
	
	bigLeftShift(c1, (int)(LENGTH(key->p)*sizeof(NumType)*BIT2BYTE), result);
	bigAdd(result, c2, result);
	
	freeBignum(x);
	freeBignum(c1);
	freeBignum(c2);
	freeBignum(K);
	
	return result;
	
}
