/*
 *  Multiplication and squaring using Knuth's n^log3 algorithm
 *  in conjunction with n^2 multiplication and regular squaring
 *  speedups (bigmult and bigsquare) to terminate recursion.
 *  User function bigMultiply is made available here.
 *  If the multiplicands are equal (the same pointer) then
 *  squaring is done.
 *
 *  By D. P. Mitchell and Jack Lacy 11/91.
 *
 *  Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

static int get_recurse_len(int);
static int bigCompareLongs(BigData, BigData, int);
static void recursiveMultiply(NumType *, NumType *, NumType *, NumType *, int);
static void recursiveSquare(NumType *, NumType *, NumType *, int);
static void fast_mult(BigInt, BigInt, BigInt);
static void fast_square(BigInt, BigInt);

#define ADDSTEP(i, C, A, B) {\
	suml = (A)[(i)] + (B)[(i)];\
	sumh = (suml < (A)[(i)]);\
	(C)[(i)] = carry + suml;\
	carry = ((C)[(i)] < suml) + sumh;\
	}

#define SUBSTEP(i, C, A, B) {\
	suml = ((long)(A)[(i)] - (long)(B)[(i)]);\
	sumh = - ((ulong)suml > (A)[(i)]);\
	C[i] = suml + carry;\
	carry = (-((C)[(i)] > (ulong)suml)) + sumh;\
}

#define ADD3STEP(i, T, C, A, B) {\
	(T)[(i)] = (A)[(i)] + (B)[(i)];\
	sumh = ((T)[(i)] < (A)[(i)]);\
	suml = (T)[(i)] + carry;\
	sumh += ((ulong)suml < (T)[(i)]);\
	(T)[(i)] = (ulong)suml + (C)[(i)];\
	carry = sumh + ((T)[(i)] < (ulong)suml);\
}


#ifdef NO_ASM
#define RECURSIONCUTOFF 2
#define SQRECCUTOFF 2
#else
#define RECURSIONCUTOFF 32
#define SQRECCUTOFF 16
#endif

#define RLEN(a, i) ((a >= i) && (a < 2*i))

static int
get_recurse_len(int alen)
{
	int recurse_len;
	
	recurse_len = 2;
	
	while (1) {
		if (RLEN(alen, recurse_len))
			return recurse_len;
		else
			recurse_len *= 2;
	}
	return recurse_len;
}


#define LESSTHAN(A,B,i) ((A[i-1] < B[i-1]) ? 1 :\
                         ((A[i-1] > B[i-1]) ? 0 :\
                          (bigCompareLongs(A,B,i) < 0)))

static int
bigCompareLongs(NumType *a, NumType *b, int N)
{
	int i;
	NumType *ap, *bp;
	
	i = (int)N-1;
	ap = a;
	bp = b;
	while ((i >= 0L) && (ap[i] == bp[i]))
		i--;
	
	if (i < 0)
		return 0;
	if (ap[i] < bp[i])
		return -1;
	else
		return 1;
	
}


/* MULTIPLICATION (not squaring) */
/* Recursive Karatsuba multiplication - for A, B length N
 * A = | a1 | a0 |
 * B = | b1 | b0 |
 * C = | c3 | c2 | c1 | c0 |
 * T = | t3 | t2 | t1 | t0 | (tmp scratch space)
 * each segment is Nover2 long and t1 and t3 are unused
 *      (and undeclared)
 * A*B = 2^2N(a1b1) + 2^N(a1b1 + (a1-a0)(b0-b1) + a0b0) + a0b0
 * Before the 3 recursive calls:
 * c = | (u1-u0) | (v0-v1) | --scratch N bits-- |
 * Afterwards:
 * c = | u1v1 (N bits) | u0v0 (N bits) |
 * t = | --scratch N bits-- | (u1-u0)*(v0-v1) (N bits) |
 */
static void
recursiveMultiply(NumType a[], NumType b[], NumType c[], NumType t[], int N)
{
	long sumh, suml, carry;
	int i, Nover2;
	NumType *a0, *a1, *b0, *b1, *c0, *c1, *c2, *c3;
	NumType *u, *v, *t0, *t2;
	long signAdiff, signBdiff, carryABAB;
	
	if (N == RECURSIONCUTOFF) {
		numtype_bigmultN(a, b, c, (int)N);
		return;
	}
	Nover2 = N >> 1;
	
	a0 = a; a1 = a0 + (int)Nover2;
	b0 = b; b1 = b0 + (int)Nover2;
	c0 = c; c1 = c0 + (int)Nover2; c2 = c1 + (int)Nover2; c3 = c2 + (int)Nover2;
	t0 = t; t2 = t0 + (int)N;
	
	u = a1; v = a0;
	signAdiff = 1;
	if (LESSTHAN(u, v, (int)Nover2) == 1) {
		u = a0;        v = a1;
		signAdiff = 0;
	}
	carry = 0;
	for (i=0; i<Nover2; i++)
		SUBSTEP((int)i,c3,u,v);
	
	u = b0; v = b1;
	signBdiff = 1;
	if (LESSTHAN(u, v, (int)Nover2) == 1) {
		u = b1;        v = b0;
		signBdiff = 0;
	}
	carry = 0;
	for (i=0; i<Nover2; i++)
		SUBSTEP((int)i,c2,u,v);
	
	recursiveMultiply(c3, c2, t0, t2, (int)Nover2);
	recursiveMultiply(a0, b0, c0, t2, (int)Nover2);
	recursiveMultiply(a1, b1, c2, t2, (int)Nover2);
	
	carry = 0;
	for (i=0; i<N; i++)
		ADD3STEP(i,t2,c0,c1,c2);
	carryABAB = carry;
	
	carry = 0;
	if ((signAdiff ^ signBdiff) != 0) {
		for (i=0; i<N; i++)
			SUBSTEP(i,c1,t2,t0);
	}
	else {
		for (i=0; i<N; i++)
			ADDSTEP(i,c1,t2,t0);
	}
	
	carry += carryABAB;
	for (i=N; (i<N+Nover2) && (carry != 0); i++) {
		c1[i] += carry;
		carry = (c1[i] < carry);
	}
	
}


/* SQUARING */
/* Recursive Karatsuba squaring - for A length N
 * A = | a1 | a0 |
 * C = | c3 | c2 | c1 | c0 |
 * T = | t3 | t2 | t1 | t0 | (tmp scratch space)
 * each segment is Nover2 long and t1, t3 and c3 are unused
 *      (and undeclared)
 * A*B = (2^2N)*(a1a1) + (2^N)*(a1a1 + (a1-a0)(a0-a1) + a0a0) + a0a0
 * Before the 3 recursive calls:
 * c = | (a1-a0) (N/2 bits) | scratch (N bits) |
 * 
 * Afterwards:
 * c = | a1a1 (N bits) | a0a0 (N bits) |
 * t = | --scratch N bits-- | (a1-a0)^2 (Nbits) |
 */
static void
recursiveSquare(NumType a[], NumType c[], NumType t[], int N)
{
	long sumh, suml, carry;
	int i, Nover2;
	NumType *a0, *a1, *u, *v;
	NumType *t0, *t2, *c0, *c1, *c2;
	long carryAA;
	
	if (N == SQRECCUTOFF) {
		numtype_bigsquareN(a, c, N);
		return;
	}
	Nover2 = N >> 1;
	
	a0 = a; a1 = a0 + Nover2;
	c0 = c; c1 = c0 + Nover2; c2 = c1 + Nover2;
	t0 = t; t2 = t0 + N;
	
	u = a1; v = a0;
	if (LESSTHAN(u, v, Nover2) == 1) {
		u = a0; v = a1;
	}
	carry = 0;
	for (i=0; i<Nover2; i++)
		SUBSTEP(i,c2,u,v);
	
	recursiveSquare(c2, t0, t2, Nover2);
	recursiveSquare(a0, c0, t2, Nover2);
	recursiveSquare(a1, c2, t2, Nover2);
	
	carry = 0;
	for (i=0; i<N; i++)
		ADD3STEP(i,t2,c0,c1,c2);
	carryAA = carry;
	
	carry = 0;
	for (i=0; i<N; i++)
		SUBSTEP(i,c1,t2,t0);
	
	carry += carryAA;
	for (i=N; (i<N+Nover2) && (carry != 0); i++) {
		c1[i] += carry;
		carry = (c1[i] < carry);
	}
}


static void
cleanMult(BigInt a, BigInt b, BigInt c, int L)
{
	int i, j, k;
	int alen, blen;
	BigData ap, bp;
	NumType m;
	
	alen = LENGTH(a);
	blen = LENGTH(b);
	
	/* A1 * B */
	ap = NUM(a)+L;
	j = (alen-L);
	k = L;
	for (i=0; i<j; i++) {
		m = ap[0];
		numtype_bigmult(b, m, c, k);
		ap++;
		k++;
	}
	
	/* A0 * B1 */
	LENGTH(a) = L;
	bp = NUM(b)+L;
	j = blen-L;
	k = L;
	for (i=0; i<j; i++) {
		m = bp[0];
		numtype_bigmult(a, m, c, k);
		bp++;
		k++;
	}
	LENGTH(a) = (ulong)alen;
	
}



static void
fast_mult(BigInt a, BigInt b, BigInt c)
{
	int alen, blen, recurse_len;
	NumType *tmp;
	
	alen = LENGTH(a);
	blen = LENGTH(b);
	
	if ((alen <= RECURSIONCUTOFF) || (blen <= RECURSIONCUTOFF) ||
	    (alen < 2*blen/3) || (blen < 2*alen/3)) {
		lbigmult(a, b, c);
		return;
	}
	GUARANTEE(c, (ulong)(alen+blen));
	
	if (alen > blen)
		alen = blen;
	
	recurse_len = get_recurse_len(alen);
	
	tmp = crypt_malloc(recurse_len*2*sizeof(NumType));
	recursiveMultiply(NUM(a), NUM(b), NUM(c), tmp, recurse_len);
	crypt_free(tmp);
	
	LENGTH(c) = (ulong)(2*recurse_len);
	while ((NUM(c)[LENGTH(c)-1] == 0) && (LENGTH(c) > 1))
		LENGTH(c)--;
	
	if ((LENGTH(a) != (ulong)recurse_len) || (LENGTH(b) != (ulong)recurse_len))
		cleanMult(a, b, c, recurse_len);
	
}

static void
cleanSquare(BigInt a, BigInt c, int L)
{
	int i, j, k;
	BigData ap;
	int alen;
	NumType m;
	
	alen = LENGTH(a);
	
	/* A1 * A */
	j = alen-L;
	ap = NUM(a)+L;
	k = L;
	for (i=0; i<j; i++) {
		m = ap[0];
		numtype_bigmult(a, m, c, k);
		ap++;
		k++;
	}
	
	/* A0 * A1 */
	ap = NUM(a)+L;
	LENGTH(a) = (ulong)L;
	k = L;
	for (i=0; i<j; i++) {
		m = ap[0];
		numtype_bigmult(a, m, c, k);
		ap++;
		k++;
	}
	LENGTH(a) = (ulong)alen;
	
}

static void
fast_square(BigInt a, BigInt c)
{
	int alen, recurse_len;
	NumType *tmp;
	
	alen = LENGTH(a);
	
	if (alen <= SQRECCUTOFF) {
		bigsquare(a, c);
		return;
	}
	
	GUARANTEE(c, (ulong)(2*alen));
	recurse_len = get_recurse_len(alen);
	
	tmp = crypt_malloc(recurse_len*2*sizeof(NumType));
	recursiveSquare(NUM(a), NUM(c), tmp, recurse_len);
	crypt_free(tmp);

	LENGTH(c) = (ulong)(2*recurse_len);
	while ((NUM(c)[LENGTH(c)-1] == 0) && (LENGTH(c) > 1))
		LENGTH(c)--;
	
	if (alen != recurse_len)
		cleanSquare(a, c, recurse_len);
}



void
bigMultiply(BigInt a, BigInt b, BigInt result)
{
	
	if (a == b) {
		fast_square(a, result);
		SIGN(result) = POS;
	}
	else {
		fast_mult(a, b, result);
		SIGN(result) = (int)SIGN(a)*(int)SIGN(b);
	}
}
