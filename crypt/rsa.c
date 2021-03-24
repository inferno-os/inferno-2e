/*
 *        Code for generating and manipulating RSA keys
 *        and doing encryption and decryption using RSA.
 *        AT&T recognizes that RSA is patented
 *        (Rivest et. al. U.S. Patent 4,405,829, issued 9/20/83).
 *        This code is for internal comparison testing only.
 *
 *        coded by Jack Lacy, December, 1991
 *
 */
#include "lib9.h"
#include <libcrypt.h>

static Key_exps *genKeyExps(BigInt, BigInt, int);
static void chineseRemTheorem(BigInt , RSAPrivateKey *, BigInt);
static void genPrimesFor3(int, BigInt, BigInt);

BigInt
getPubExp(RSAPublicKey *key)
{
	return key->publicExponent;
}

BigInt
getModulus(RSAPublicKey *key)
{
	return key->modulus;
}

static Key_exps *
genKeyExps(BigInt p, BigInt q, int ebits)
{
	BigInt e, phi, p1, q1;
	BigInt u1, ngcd, ignore;
	Key_exps *exps;
	int ebytes;

	exps = (Key_exps *)crypt_malloc(sizeof(Key_exps));
	p1 = bigInit(0);
	q1 = bigInit(0);
	phi = bigInit(0);
	u1  = bigInit(0);
	ngcd = bigInit(0);
	ignore = bigInit(0);
	e = bigInit(3);
	
	bigSubtract(p, one, p1);
	bigSubtract(q, one, q1);
	bigMultiply(p1, q1, phi);
	freeBignum(p1);
	freeBignum(q1);

	if (ebits > 2) {
		ebytes = (ebits/8) + (ebits%8? 1: 0);
		bigPseudoRand(ebytes, e);
		if (EVEN(e))
			bigAdd(e, one, e);
	}
	extendedGcd(e, phi, u1, ignore, ngcd);
	while (bigCompare(ngcd, one) != 0) {
		bigAdd(e, two, e);
		extendedGcd(e, phi, u1, ignore, ngcd);
	}
	if (SIGN(u1) == NEG)
		negate(u1, phi, u1);
	
	exps->d = u1;
	exps->e = e;
	
	freeBignum(phi);
	freeBignum(ngcd);
	freeBignum(ignore);
	
	return exps;
}

RSAPublicKey *
buildRSAPublicKey(BigInt e, BigInt n)
{
	RSAPublicKey *pk;

	pk = (RSAPublicKey *)crypt_malloc(sizeof(RSAPublicKey));
	pk->publicExponent = e;
	pk->modulus = n;
	return pk;
}

RSAPrivateKey *
buildRSAPrivateKey(BigInt e, BigInt d, BigInt p, BigInt q, BigInt dp, BigInt dq, BigInt c12)
{
	RSAPrivateKey *pk;
	ChineseRemStruct *crt;

	crt = (ChineseRemStruct *)crypt_malloc(sizeof(ChineseRemStruct));
	pk = (RSAPrivateKey *)crypt_malloc(sizeof(RSAPrivateKey));
	
	pk->publicExponent = e;
	pk->privateExponent = d;
	pk->modulus = bigInit(0);
	bigMultiply(p, q, pk->modulus);
	
	pk->crt = crt;
	pk->crt->p = p;
	pk->crt->q = q;
	pk->crt->dp = dp;
	pk->crt->dq = dq;
	pk->crt->c12 = c12;
	
	return pk;
}

RSAKeySet *
buildKeySet(BigInt e, BigInt d, BigInt p, BigInt q)
{
	BigInt pminus1, qminus1, n, dp, dq, c12;
	BigInt ecopy, dcopy;
	RSAKeySet *ks;

	ks = (RSAKeySet *)crypt_malloc(sizeof(RSAKeySet));
	n = bigInit(0);
	bigMultiply(p, q, n);
	
	ecopy = bigInit(0);
	bigCopy(e, ecopy);
	ks->publicKey = buildRSAPublicKey(ecopy, n);
	
	pminus1 = bigInit(0);
	qminus1 = bigInit(0);
	bigSubtract(p, one, pminus1);
	bigSubtract(q, one, qminus1);
	
	dp = bigInit(0);
	dq = bigInit(0);
	bigMod(d, pminus1, dp);
	bigMod(d, qminus1, dq);
	
	c12 = bigInit(0);
	getInverse(p, q, c12);
	if (SIGN(c12) == NEG) {
		negate(c12, q, c12);
	}
	
	ecopy = bigInit(0);
	bigCopy(e, ecopy);
	dcopy = bigInit(0);
	bigCopy(d, dcopy);
	ks->privateKey = buildRSAPrivateKey(ecopy, dcopy, p, q,
					    dp, dq, c12);
	
	freeBignum(pminus1);
	freeBignum(qminus1);
	
	return ks;
}


static void
genPrimesFor3(int nbits, BigInt p, BigInt q)
{
	BigInt ngcd, ignore, three, pminus1, qminus1;
	
	ignore = bigInit(0);
	three = bigInit(3);
	pminus1 = bigInit(0);
	qminus1 = bigInit(0);
	
#ifdef _GORDON
	genStrongPrimeSet(nbits/2, p, (int)(int)NULL, ignore, GORDON); /* Gordon algorithm doesn't care about the p-1 factor size */
#else
	genStrongPrimeSet(nbits/2, p, 160, ignore, NIST);
#endif
	bigSubtract(p, one, pminus1);
	ngcd = gcd(three, pminus1);
	while (bigCompare(ngcd, one) != 0) {
		freeBignum(ngcd);
#ifdef _GORDON
		genStrongPrimeSet(nbits/2, p, (int)NULL, ignore, GORDON);
#else
		genStrongPrimeSet(nbits/2, p, 160, ignore, NIST);
#endif
		bigSubtract(p, one, pminus1);
		ngcd = gcd(three, pminus1);
	}
	freeBignum(ngcd);
	
#ifdef _GORDON
	genStrongPrimeSet(nbits/2, q, (int)NULL, ignore, GORDON);
#else
	genStrongPrimeSet(nbits/2, p, 160, ignore, NIST);
#endif
	bigSubtract(q, one, qminus1);
	ngcd = gcd(three, qminus1);
	while (bigCompare(ngcd, one) != 0) {
		freeBignum(ngcd);
#ifdef _GORDON
		genStrongPrimeSet(nbits/2, q, (int)NULL, ignore, GORDON);
#else
		genStrongPrimeSet(nbits/2, p, 160, ignore, NIST);
#endif
		bigSubtract(q, one, qminus1);
		ngcd = gcd(three, qminus1);
	}
	freeBignum(ngcd);
	freeBignum(pminus1);
	freeBignum(qminus1);
	freeBignum(ignore);
	freeBignum(three);
}


RSAKeySet *
genRSAKeySet(int nbits, int ebits)
{
	BigInt p, q, ignore;
	Key_exps *exps;
	RSAKeySet *key_set;
	
	p = bigInit(0);
	q = bigInit(0);
	if (ebits == 2)
		genPrimesFor3(nbits, p, q);
	
	else {
		ignore = bigInit(0);
#ifdef _GORDON
		genStrongPrimeSet(nbits/2, p, (int)NULL, ignore, GORDON);
		genStrongPrimeSet(nbits/2, q, (int)NULL, ignore, GORDON);
#else
		genStrongPrimeSet(nbits/2, p, 160, ignore, NIST);
		genStrongPrimeSet(nbits/2, q, 160, ignore, NIST);
#endif
		freeBignum(ignore);
	}
	exps = genKeyExps(p, q, ebits);
	key_set = buildKeySet(exps->e, exps->d, p, q);
	freeBignum(exps->e);
	freeBignum(exps->d);
	crypt_free((char *)exps);
	return key_set;
}


/*
   Chinese Remainder Theorem reconstruction of m^d mod n, using
   m^dp mod p and m^dq mod q with dp = d mod p-1, dq = d mod q-1.
   */
static void
chineseRemTheorem(BigInt m, RSAPrivateKey *key, BigInt em)
{
	BigInt u1, u2;
	BigInt p, q, dp, dq, c12;
	
	p = key->crt->p;
	q = key->crt->q;
	dp = key->crt->dp;
	dq = key->crt->dq;
	c12 = key->crt->c12;
	
	u1 = bigInit(0);
	u2 = bigInit(0);
	bigMod(m, p, u1);
	bigMod(m, q, u2);
	
	bigPow(u1, dp, p, u1);
	bigPow(u2, dq, q, u2);
	
	crtCombine(u1, u2, p, q, c12, em);
	
	freeBignum(u1);
	freeBignum(u2);
	
}

void
freeRSAPublicKey(RSAPublicKey *pk)
{
	freeBignum(pk->publicExponent);
	freeBignum(pk->modulus);
	crypt_free((char *)pk);
}

void
freeRSAPrivateKey(RSAPrivateKey *pk)
{
	freeBignum(pk->publicExponent);
	freeBignum(pk->privateExponent);
	freeBignum(pk->modulus);
	freeBignum(pk->crt->p);
	freeBignum(pk->crt->q);
	freeBignum(pk->crt->dp);
	freeBignum(pk->crt->dq);
	freeBignum(pk->crt->c12);
	crypt_free((char *)pk->crt);
	crypt_free((char *)pk);
}

void
freeRSAKeys(RSAKeySet *ks)
{
	
	freeRSAPublicKey(ks->publicKey);
	freeRSAPrivateKey(ks->privateKey);
	crypt_free((char *)ks);
}

BigInt
RSAEncrypt(BigInt message, RSAPublicKey *key)
{
	BigInt result;
	
	result = bigInit(3);
	if (bigCompare(key->publicExponent, result) == 0) {
		reset_big(result, 0);
		bigCube(message, key->modulus, result);
	}
	else {
		reset_big(result, 0);
		bigPow(message, key->publicExponent, key->modulus, result);
	}
	return result;
}

BigInt
RSADecrypt(BigInt message, RSAPrivateKey *key)
{
	BigInt result;
	
	result = bigInit(0);
	/*
	   bigPow(message, key->privateExponent, key->modulus, result);
	   */
	
	chineseRemTheorem(message, key, result);
	return result;
	
}


RSASignature *
RSASign(BigInt message,  RSAPrivateKey *key)
{
	return (RSASignature *)RSADecrypt(message, key);
}


Boolean
RSAVerify(BigInt message, RSASignature *sig, RSAPublicKey *key)
{
	Boolean retval;
	BigInt cmp;
	
	cmp = (BigInt)RSAEncrypt((BigInt)sig, key);
	
	if (bigCompare(message, cmp) == 0)
		retval = TRUE;
	else
		retval = FALSE;
	
	freeBignum(cmp);
	
	return retval;
}

void
freeRSASig(RSASignature *sig)
{
	freeBignum((BigInt)sig);
}
