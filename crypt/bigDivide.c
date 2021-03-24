/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>
#include "bigmath.h"


void
bigDivide(BigInt a, BigInt m, BigInt qu, BigInt x)
{
	BigData xp, mp, tp, tp2;
	BigInt div, tmp, tmp2, ms2;
	ulong lendiff, q, ms;
	int i, l, k, changed, normbits, asavesign, msavesign;
	
	if (ZERO(m))
		handle_exception(CRITICAL, "bigDivide: divisor is zero.");

	SIGN(qu) = SIGN(a) * SIGN(m);
	
	if (a != x)
		bigCopy(a, x);
	
	reset_big(qu, 0);
	i = bigCompare(a, m);
	if (i < 0) {
		return;
	}
	if (i == 0) {
		reset_big(qu, 1);
		reset_big(x, 0);
		return;
	}
	if (ONE(m)) {
		bigCopy(a, qu);
		reset_big(x, (NumType)0);
		return;
	}
	
	asavesign = SIGN(a);
	msavesign = SIGN(m);
	
	SIGN(x) = POS;
	SIGN(m) = POS;
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
	
	ms2 = bigInit(0);
	GUARANTEE(ms2, 2);
	tmp = bigInit(0);
	GUARANTEE(tmp, 4);
	tmp2 = bigInit(0);
	GUARANTEE(tmp2, 4);
	div = bigInit(0);
	
	bigleftshift(m, div, lendiff);
	
	mp = NUM(m);
	ms = mp[k-1];
	
	NUM(ms2)[1] = ms;
	if(k >= 2)
		NUM(ms2)[0] = mp[k-2];
	else
		NUM(ms2)[0] = 0;
	LENGTH(ms2) = 2;
	
	if (bigCompare(x, div) >= 0) {
		bigsub(x, div, x);
		reset_big(qu, 1);
	}
	
	for (i=l-1; i>k-1; i--) {
		xp = NUM(x);
		bigrightshift(div, div, 1);
		if (xp[i] == ms){
			q = (ulong)0xFFFFFFFFUL;
		}else {
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
		if(i >= 2)
			tp2[0] = xp[i-2];
		else
			tp2[0] = 0;
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
		bigLeftShift(qu, 32, qu);
		NUM(qu)[0] = q;
	}
	
	if (changed) {
		bigRightShift(m, normbits, m);
		bigRightShift(x, normbits, x);
	}
	SIGN(x) = asavesign;
	SIGN(m) = msavesign;
	
	freeBignum(ms2);
	freeBignum(div);
	freeBignum(tmp);
	freeBignum(tmp2);
	
}
