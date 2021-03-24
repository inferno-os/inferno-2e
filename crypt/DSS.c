/*
 *	Code for generating and manipulating keys
 *	and generating and verifying digital signatures
 *	according to NIST's digital signature standard (DSS)
 *	NIST, 1991
 *
 *	coded by Jack Lacy, December, 1991
 *
 *	Copyright (c) 1991 Bell Laboratories
 *
 *	9/14/93 Made compatible with El Gamal Keys (Actually
 *	made El Gamal Keys compatible with DSS).  The DSS
 *	signature looks just like the El Gamal signature (r,s).
 *	DSSSignature is typdef'ed to EGSignature.
 *
 *	2/94 Included Ernie Brickell's speedup for constant base and prime.
 *	Increases size of key substantially but signing and verifying are
 *	roughly 3 times faster.
 */
#include "lib9.h"
#include <libcrypt.h>

DSSSignature *
DSSSign(BigInt md, EGPrivateKey *key)
{
	BigInt k, r, s, q, p, xr, tmp;
	BigInt kinverse;
	DSSSignature *sig;
	
	sig = (DSSSignature *)crypt_malloc(sizeof(DSSSignature));
	q = key->q;
	p = key->p;
	
	/* signature = (r,s) */
	/* get k -- relatively prime to q (gcd(k, q) = 1) */
	
	k = bigInit(0);
	getRandBetween(key->q, zero, k, REALLY);
	if (EVEN(k))
		bigAdd(k, one, k);
	
	kinverse = bigInit(0);
	getInverse(k, q, kinverse);
	if (SIGN(kinverse) == NEG)
		negate(kinverse, q, kinverse);
	
	/* get r */
	r = bigInit(0);
#ifdef BRICKELL
	brickell_bigpow(key->g_table, k, p, r);
#else
	bigPow(key->alpha, k, p, r);
#endif BRICKELL
	bigMod(r, q, r);
	
	/* get s */
	s = bigInit(0);
	tmp = bigInit(0);
	xr = bigInit(0);
	bigMultiply(key->secret, r, xr);
	bigAdd(md, xr, tmp);
	bigMultiply(kinverse, tmp, s);
	bigMod(s, q, s);
	
	freeBignum(k);
	freeBignum(kinverse);
	freeBignum(xr);
	freeBignum(tmp);
	
	sig->r = r;
	sig->s = s;
	return sig;
}


Boolean
DSSVerify(BigInt md, DSSSignature *sig, EGPublicKey *key)
{
	BigInt r, s, q, p;
#ifdef BRICKELL
	Table *g_table, *y_table;
#else
	BigInt y, g;
#endif BRICKELL
	BigInt w, u1, u2, v, tmp1, tmp2;
	Boolean retval;
	
	s = sig->s;
	r = sig->r;
	q = key->q;
	p = key->p;
	w = bigInit(0);
	
#ifdef BRICKELL
	g_table = key->g_table;
	y_table = key->y_table;
#else
	g = key->alpha;
	y = key->publicKey;
#endif BRICKELL
	
	getInverse(s, q, w);
	if (SIGN(w) == NEG)
		negate(w, q, w);
	
	u1 = bigInit(0);
	bigMultiply(md, w, u1);
	bigMod(u1, q, u1);
	
	u2 = bigInit(0);
	bigMultiply(r, w, u2);
	bigMod(u2, q, u2);
	
	tmp1 = bigInit(0);
	tmp2 = bigInit(0);
#ifdef BRICKELL
	brickell_bigpow(g_table, u1, p, tmp1);
	brickell_bigpow(y_table, u2, p, tmp2);
#else
	bigPow(g, u1, p, tmp1);
	bigPow(y, u2, p, tmp2);
#endif BRICKELL
	
	v = bigInit(0);
	bigMultiply(tmp1, tmp2, v);
	bigMod(v, p, v);
	bigMod(v, q, v);
	
	if (bigCompare(r, v) == 0)
		retval = TRUE;
	else
		retval = FALSE;
	
	freeBignum(w);
	freeBignum(u1);
	freeBignum(u2);
	freeBignum(v);
	freeBignum(tmp1);
	freeBignum(tmp2);
	
	return retval;
}

void
freeDSSSig(DSSSignature *sig)
{
	freeBignum(sig->r);
	freeBignum(sig->s);
	crypt_free((char *)sig);
}
