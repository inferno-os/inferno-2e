/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>
#include "bigmath.h"

#define SETUP(b, st, nums) {\
	b = &st;\
	b->space = sizeof(nums)/sizeof(ulong);\
	b->num = nums;\
	b->length = 1;\
	b->num[0] = 0;\
	b->sign = POS;\
}

void
bigMod(BigInt a, BigInt m, BigInt x)
{
	BigData xp, mp, tp, tp2;
	long lendiff;
	ulong q, ms;
	BigInt ms2, div, tmp, tmp2;
	Bignum ms2store, tmp2store;
	ulong ms2num[2], tmp2num[4];
	int i, l, k, changed, normbits, savesign;

	if (ZERO(m))
		handle_exception(CRITICAL, "bigMod: modulus is zero.");
	
	if (ONE(m)) {
		reset_big(x, 0);
		return;
	}
	
	if (a != x)
		bigCopy(a, x);
	
	savesign = SIGN(a);
	if (bigCompare(a, m) < 0L) {
		return;
	}
	
	SIGN(x) = POS;
	changed = 0;
	normbits = 0;
	if (NUM(m)[LENGTH(m)-1] < (ulong)0x80000000UL) {
		normbits = (32-msb(NUM(m)[LENGTH(m)-1]));
		bigLeftShift(m, normbits, m);
		bigLeftShift(x, normbits, x);
		changed = 1;
	}
	
	l = LENGTH(x);
	k = LENGTH(m);
	lendiff = l - k;
	
	if (lendiff < 0)
		return;

	SETUP(ms2, ms2store, ms2num);
	SETUP(tmp2, tmp2store, tmp2num);
	tmp = bigInit(0);
	GUARANTEE(tmp, 4);
	div = bigInit(0);
	
	bigleftshift(m, div, lendiff);
	
	mp = NUM(m);
	ms = mp[k-1];
	NUM(ms2)[1] = ms;
	NUM(ms2)[0] = mp[k-2];
	LENGTH(ms2) = 2;
	
	if (bigCompare(x, div) > 0)
		bigsub(x, div, x);
	
	for (i=l-1; i>k-1; i--) {
		xp = NUM(x);
		bigrightshift(div, div, 1);
		if (xp[i] == ms)
			q = (ulong)0xFFFFFFFFUL;
		else {
			tp = NUM(tmp);
			tp[1] = xp[i];
			tp[0] = xp[i-1];
			LENGTH(tmp) = 2;
			q = longBigDivide(tmp, ms);
		}
		reset_big(tmp, 0);
		numtype_bigmult(ms2, q, tmp, 0);
		tp2 = NUM(tmp2);
		tp2[2] = xp[i];
		tp2[1] = xp[i-1];
		tp2[0] = xp[i-2];
		LENGTH(tmp2) = 3;
		while (bigCompare(tmp, tmp2) > 0) {
			q = q - (ulong)1;
			reset_big(tmp, 0);
			numtype_bigmult(ms2, q, tmp, 0);
		}
		reset_big(tmp, 0);
		numtype_bigmult(div, q, tmp, 0);
		bigSubtract(x, tmp, x);
		if (SIGN(x) == NEG)
			bigAdd(x, div, x);
	}
	
	if (changed) {
		bigRightShift(m, normbits, m);
		bigRightShift(x, normbits, x);
	}
	SIGN(x) = savesign;
	
	freeBignum(div);
	freeBignum(tmp);
}
