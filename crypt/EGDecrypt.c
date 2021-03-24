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


BigInt
EGDecrypt(BigInt message, EGPrivateKey *key)
{
	BigInt c1, c2, K, result;
	int shiftbits, oldlen;
	
	shiftbits = (int)(LENGTH(key->p)*sizeof(NumType)*BIT2BYTE);
	c1 = bigInit(0);
	bigRightShift(message, shiftbits, c1);
	c2 = bigInit(0);
	oldlen = LENGTH(message);
	LENGTH(message) = LENGTH(key->p);
	bigCopy(message, c2);
	LENGTH(message) = (ulong)oldlen;
	
	K = bigInit(0);
	bigPow(c1, key->secret, key->p, K);
	
	result = bigInit(0);
	
	/*
	   BigInt Kinv;
	   Kinv = bigInit(0);
	   getInverse(K, key->p, Kinv);
	   
	   if (SIGN(Kinv) == NEG)
	   negate(Kinv, key->p, Kinv);
	   
	   bigMultiply(Kinv, c2, result);
	   bigMod(result, key->p, result);
	   freeBignum(Kinv);
	   */
	
	bigXor(c2, K, result);
	
	freeBignum(c1);
	freeBignum(c2);
	freeBignum(K);
	
	return result;
}
