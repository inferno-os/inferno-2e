/*
 *  Modular exponentiation using Montgomery reduction and
 *  Addition chaining.
 *  coded by Jack Lacy 11/91.
 *  Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

static Table *buildAddChainTable(BigInt,  Mont_set *);
static void print_table(Table *);
static void bigpow(BigInt, BigInt, BigInt, BigInt);
static void bigpow2(BigInt, BigInt, BigInt, BigInt);
static void bigpow_crt(BigInt, BigInt, BigInt, BigInt, BigInt);
static void bigmod2(BigInt, BigInt);
static Table *buildCoeffTable(BigInt, Mont_set *, Table *);

extern int bigNumsAllocated;

NumType
modInverse(NumType x)
{
	int i;
	NumType y[NumTypeBits+1], od, d, xx;
	
	y[1] = 1;
	od = 2;
	for (i=2; i<NumTypeBits+1; i++) {
		d = od << 1;
		xx = x * y[i-1];
		if (d)
			xx %= d;
		if (xx < od)
			y[i] = y[i-1];
		else
			y[i] = y[i-1] + od;
		od = d;
	}
	
	return y[NumTypeBits]*NumTypeMask;
}

Mont_set *
mont_set(BigInt N)
{
	Mont_set *ms;
	ms = (Mont_set *) crypt_malloc(sizeof(Mont_set));
	ms->N = N;
	ms->n0p = modInverse(NUM(N)[0]);
	return ms;
}

/* Convert a to a*R mod N */

BigInt
res_form(BigInt a, Mont_set *ms)
{
	BigInt N, r, tmp;
	int shiftbits;
	
	N = ms->N;
	tmp = bigInit(0);
	
	/* Multiply by R */
	shiftbits = (int)(LENGTH(N)*sizeof(NumType)*BIT2BYTE);
	bigLeftShift(a, shiftbits, tmp);
	
	r = bigInit(0);
	bigMod(tmp, N, r);
	
	freeBignum(tmp);
	
	return r;
}

/* Special form of redc which reduces arg 1 in place.
   Convert T to T*R^(-1) mod N. See coremult.c.
   */

void REDC(BigInt T, Mont_set *ms)
{
	NumType n0p, m;
	BigInt N, t;
	BigData tp;
	int i, n;
	int shiftbits;
	
	t = T;
	N = ms->N;
	n0p = ms->n0p;
	
	/* Main loop */
	
	n = (int)LENGTH(N);
	for (i=0; i < n; i++) {
		tp = NUM(t);
		m = tp[i] * n0p;
		numtype_bigmult(N, m, t, i);
	}
	
	/* divide by R */
	shiftbits = (int)(n * sizeof(NumType) * BIT2BYTE);
	bigRightShift(t, shiftbits, t);
	
	if (bigCompare(t, N) > 0)
		bigSubtract(t, N, t);
	
	trim(t);
}

static Table *
buildAddChainTable(BigInt a, Mont_set *ms)
{
	int i;
	Table *tp;
	BigInt x;
	
	tp = (Table *)crypt_malloc(sizeof(Table) + sizeof(Bignum)*(16 - 2));
	x = res_form(a, ms);
	
	tp->length = 16;
	tp->t[0] = bigInit(1);        /* x^0 */
	tp->t[1] = bigInit(0);        /* x^1 */
	bigCopy(x, tp->t[1]);
	for (i = 2; i < 16; i++) {        /* x^[2-15] */
		tp->t[i] = bigInit(0);
		bigMultiply(x, tp->t[i-1], tp->t[i]);
		REDC(tp->t[i], ms);
	}
	freeBignum(x);
	return tp;
}


static Table *
buildAddChainTable2(BigInt a, BigInt m)
{
	int i;
	Table *tp;
	
	tp = (Table *)crypt_malloc(sizeof(Table) + sizeof(Bignum)*(16 - 2));
	
	tp->length = 16;
	tp->t[0] = bigInit(1);        /* x^0 */
	tp->t[1] = bigInit(0);        /* x^1 */
	bigCopy(a, tp->t[1]);
	for (i = 2; i < 16; i++) {        /* x^[2-15] */
		tp->t[i] = bigInit(0);
		bigMultiply(a, tp->t[i-1], tp->t[i]);
		bigmod2(tp->t[i], m);
	}
	
	return tp;
}

static void
print_table(Table *t)
{
	int i;
	
	for (i = 0; i < t->length; i++) {
		print("t[%d] = %B", i, t->t[i]);
	}
}

void
freeTable(Table *t)
{
	int i;

	if(t == 0)
		return;
	for (i = 0; i < (int)t->length; i++) {
		freeBignum(t->t[i]);
	}
	crypt_free((char *)t);
}

void
freeMs(Mont_set *ms)
{
	crypt_free((char *)ms);
}

#define NIBBLE(B,N) (((NUM(B)[(N) >> 3] >> (((N) & 7) << 2)) & 15))
#define NIBSPERCHUNK 8

static void
bigpow(BigInt a, BigInt exp, BigInt modulus, BigInt result)
{
	Table *at;
	BigInt d, one;
	Mont_set *ms;
	int i, nib;
	
	if (ZERO(a)) {
		reset_big(result, (NumType)0);
		return;
	}
	else if (ZERO(exp)) {
		reset_big(result, (NumType)1);
		return;
	}
	ms = mont_set(modulus);
	at = buildAddChainTable(a, ms);
	
	for (i = (int)(NIBSPERCHUNK*LENGTH(exp) - 1); i >= 0; --i)
		if (NIBBLE(exp, i))
			break;
	
	one = bigInit(1);
	d = res_form(one, ms);
	freeBignum(one);
	for (;; --i) {
		nib = (int)(NIBBLE(exp, i));
		if (nib) {
			bigMultiply(at->t[nib], d, result);
			REDC(result, ms);
		}
		else
			bigCopy(d, result);
		if (i == 0)
			break;
		
		bigMultiply(result, result, d);
		REDC(d, ms);
		
		bigMultiply(d, d, result);
		REDC(result, ms);
		
		bigMultiply(result, result, d);
		REDC(d, ms);
		
		bigMultiply(d, d, result);
		REDC(result, ms);
		
		bigCopy(result, d);
		
	}
	REDC(result, ms);
	
	freeBignum(d);
	freeTable(at);
	freeMs(ms);
}


static void
bigpow2(BigInt a, BigInt exp, BigInt modulus, BigInt result)
{
	Table *at;
	BigInt d;
	int i, nib;
	
	if (ZERO(a)) {
		reset_big(result, (NumType)0);
		return;
	}
	else if (ZERO(exp)) {
		reset_big(result, (NumType)1);
		return;
	}
	at = buildAddChainTable2(a, modulus);
	for (i = (int)(8*LENGTH(exp)-1); i >= 0; --i)
		if (NIBBLE(exp, i))
			break;
	
	d = bigInit(1);
	
	for (;; --i) {
		nib = (int)(NIBBLE(exp, i));
		if (nib) {
			bigMultiply(at->t[nib], d, result);
			bigmod2(result, modulus);
		}
		else
			bigCopy(d, result);
		
		if (i == 0)
			break;
		
		bigMultiply(result, result, d);
		bigmod2(d, modulus);
		
		bigMultiply(d, d, result);
		bigmod2(result, modulus);
		
		bigMultiply(result, result, d);
		bigmod2(d, modulus);
		
		bigMultiply(d, d, result);
		bigmod2(result, modulus);
		
		bigCopy(result, d);
	}
	freeBignum(d);
	freeTable(at);
	
}

static void
bigpow_crt(BigInt m, BigInt f1, BigInt f2, BigInt exp, BigInt result)
{
	BigInt u1, u2, tmp, c12;
	
	u1 = bigInit(0);
	u2 = bigInit(0);
	tmp = bigInit(0);
	c12 = bigInit(0);
	
	/* We assume here that f1 is a power of 2 */
	
	bigCopy(m, u1);
	bigmod2(u1, f1);
	bigMod(m, f2, u2);
	
	bigpow2(u1, exp, f1, u1);
	bigpow(u2, exp, f2, u2);
	
	getInverse(f1, f2, c12);
	crtCombine(u1, u2, f1, f2, c12, result);
	
	if (SIGN(result) == NEG) {
		bigMultiply(f1, f2, tmp);
		negate(result, tmp, result);
	}
	
	freeBignum(u1);
	freeBignum(u2);
	freeBignum(tmp);
	freeBignum(c12);
}


void
bigPow(BigInt m, BigInt exp, BigInt modulus, BigInt result)
{
	BigInt f1, f2, newm;
	int k = 0;
	
	newm = bigInit(0);
	if (bigCompare(m, modulus) > 0)
		bigMod(m, modulus, newm);
	else
		bigCopy(m, newm);
	
	if (EVEN(modulus)) {
		f1 = bigInit(1);
		f2 = bigInit(0);
		bigCopy(modulus, f2);
		while (EVEN(f2)) {
			bigRightShift(f2, (int)1, f2);
			k++;
		}
		bigLeftShift(f1, k, f1);
		if (!ONE(f2)) {
			bigpow_crt(newm, f1, f2, exp, result);
		}
		else
			bigpow2(newm, exp, f1, result);
		freeBignum(f1);
		freeBignum(f2);
	}
	else {
		bigpow(newm, exp, modulus, result);
	}
	freeBignum(newm);
}


/* Modulus operation for m a power of 2 */
static void
bigmod2(BigInt a, BigInt m)
{
	NumType mask;
	
	mask = NUM(m)[LENGTH(m)-1] - 1;
	LENGTH(a) = LENGTH(m);
	NUM(a)[LENGTH(a)-1] &= mask;
	
	while ((NUM(a)[LENGTH(a)-1] == 0) && LENGTH(a) > 1)
		LENGTH(a) -= 1;
}
