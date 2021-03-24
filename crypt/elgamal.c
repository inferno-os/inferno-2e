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

EGSignature *
EGSign(BigInt m, EGPrivateKey *egKey)
{
	BigInt k, kinverse, r, s, pminus1, tmp;
	BigInt p, alpha, xgcd, ignore;
	EGSignature *sig;

	sig = (EGSignature *)crypt_malloc(sizeof(EGSignature));
	p = egKey->p;
	alpha = egKey->alpha;
	ignore = bigInit(0);
	pminus1 = bigInit(0);
	bigSubtract(p, one, pminus1);
	
	/* signature = (r,s) */
	/* get k */
	k = bigInit(0);
	getRandBetween(egKey->q, one, k, PSEUDO);
	if (EVEN(k))
		bigAdd(k, one, k);
	
	kinverse = bigInit(0);
	xgcd = bigInit(0);
	extendedGcd(k, pminus1, kinverse, ignore, xgcd);
	while (bigCompare(xgcd, one) != 0) {
		bigSubtract(k, two, k);
		extendedGcd(k, pminus1, kinverse, ignore, xgcd);
	}
	
	/* get r */
	r = bigInit(0);
	bigPow(alpha, k, p, r);
	
	/* get s */
	s = bigInit(0);
	tmp = bigInit(0);
	bigMultiply(egKey->secret, r, tmp);
	bigMod(tmp, pminus1, tmp);
	bigSubtract(m, tmp, tmp);
	
	bigMultiply(kinverse, tmp, s);
	bigMod(s, pminus1, s);
	
	if (SIGN(s) == NEG) {
		negate(s, pminus1, s);
	}
	freeBignum(ignore);
	freeBignum(pminus1);
	freeBignum(k);
	freeBignum(kinverse);
	freeBignum(xgcd);
	freeBignum(tmp);
	
	sig->r = r;
	sig->s = s;
	return sig;
}

Boolean
EGVerify(BigInt m, EGSignature *sig, EGPublicKey *key)
{
	BigInt alpha, p, y, tmp1, tmp2, tmp3, tmp4;
	Boolean retval;
	
	tmp1 = bigInit(0);
	tmp2 = bigInit(0);
	tmp3 = bigInit(0);
	tmp4 = bigInit(0);
	
	alpha = key->alpha;
	p = key->p;
	y = key->publicKey;
	
	bigPow(alpha, m, p, tmp1);
	
	bigPow(y, sig->r, p, tmp2);
	bigPow(sig->r, sig->s, p, tmp3);
	
	bigMultiply(tmp2, tmp3, tmp4);
	bigMod(tmp4, p, tmp2);
	
	if (bigCompare(tmp1, tmp2) == 0)
		retval = TRUE;
	else
		retval = FALSE;
	
	freeBignum(tmp1);
	freeBignum(tmp2);
	freeBignum(tmp3);
	freeBignum(tmp4);
	
	return retval;
}

void
freeEGPublicKey(EGPublicKey *pk)
{
	freeBignum(pk->p);
	freeBignum(pk->q);
	freeBignum(pk->alpha);
	freeBignum(pk->publicKey);
	freeTable(pk->g_table);
	freeTable(pk->y_table);
	crypt_free((char *)pk);
}

void
freeEGPrivateKey(EGPrivateKey *pk)
{
	freeBignum(pk->p);
	freeBignum(pk->q);
	freeBignum(pk->alpha);
	freeBignum(pk->publicKey);
	freeBignum(pk->secret);
	freeTable(pk->g_table);
	crypt_free((char *)pk);
}

void
freeEGKeys(EGKeySet *ks)
{
	freeEGPublicKey(ks->publicKey);
	freeEGPrivateKey(ks->privateKey);
	crypt_free((char *)ks);
}

void
freeEGSig(EGSignature *sig)
{
	freeBignum(sig->r);
	freeBignum(sig->s);
	crypt_free((char *)sig);
}

void
freeEGParams(EGParams *params)
{
	freeBignum(params->p);
	freeBignum(params->q);
	freeBignum(params->alpha);
	crypt_free((char *)params);
}
