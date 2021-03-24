/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

#define lmult LMULT

#define SETUP(b, st, nums) {\
	b = &st;\
	b->space = sizeof(nums)/sizeof(ulong);\
	b->num = nums;\
	b->sign = POS;\
}

ulong
longBigDivide(BigInt big, ulong d)
{
	BigInt div, x, tmp, tmp2, ms2;
	Bignum divstore, xstore, tmpstore, tmp2store, ms2store;
	ulong divnum[2], xnum[2], tmpnum[3], tmp2num[2], ms2num[1];
	ushort dhi, q;
	ulong r, s;
	int normbits, changed;

	SETUP(div, divstore, divnum);
	SETUP(x, xstore, xnum);
	SETUP(tmp, tmpstore, tmpnum);
	SETUP(tmp2, tmp2store, tmp2num);
	SETUP(ms2, ms2store, ms2num);

	normbits = 0;
	changed = 0;
	if (d < 0x80000000UL) {
		normbits = 32 - msb(d);
		bigLeftShift(big, normbits, big);
		d = d << (ulong)normbits;
		changed = 1;
	}
	
	reset_big(ms2, d);
	divnum[0] = 0;
	divnum[1] = d;
	div->length = 2;
	
	reset_big(x, 0);
	reset_big(tmp, 0);
	reset_big(tmp2, 0);
	
	dhi = (d>>16UL)&0xFFFFU;
	
	bigCopy(big, x);
	if (bigCompare(x, div) > 0) {
		bigsub(x, div, x);
	}
	s = xnum[1];
	bigRightShift(x, 16, tmp2);
	divnum[1] = (ulong)dhi;
	divnum[0] = (d << 16UL);
	div->length = 2;
	
	if (((s>>16UL)&0xFFFFUL) == (ulong)dhi)
		q = (ushort)0xFFFF;
	else
		q = (ushort)(s/(ulong)dhi);
	
	tmpnum[0] = tmpnum[1] = 0UL;
	tmpnum[1] = lmult(tmpnum, (ulong)q, ms2num, 1);
	tmp->length = 2;
	while (bigCompare(tmp, tmp2) > 0) {
		q = q - 1UL;
		tmpnum[0] = tmpnum[1] = 0UL;
		tmpnum[1] = lmult(tmpnum, (ulong)q, ms2num, 1);
	}
	tmpnum[0] = 0UL;
	tmpnum[1] = 0UL;
	tmpnum[2] = 0UL;
	tmpnum[2] = lmult(tmpnum, (ulong)q, divnum, 2);
	tmp->length = 3;
	bigSubtract(x, tmp, x);
	if (x->sign == NEG)
		bigAdd(x, div, x);
	
	r = (ulong)((ulong)q << 16UL);
	s = ((xnum[1] & 0xFFFFUL) << 16UL) + (xnum[0]>>16UL);
	if ((xnum[1]&0xFFFFUL) == (ulong)dhi)
		q = 0xFFFFU;
	else
		q = (ushort)(s/(ulong)dhi);
	
	tmpnum[0] = 0UL;
	tmpnum[1] = 0UL;
	tmpnum[1] = lmult(tmpnum, (ulong)q, ms2num, 1);
	tmp->length = 2;
	
	while (bigCompare(tmp, x) > 0) {
		q = q - 1;
		tmpnum[0] = 0UL;
		tmpnum[1] = 0UL;
		tmpnum[1] = lmult(tmpnum, (ulong)q, ms2num, 1);
	}
	r += (ulong)q;
	
	if (changed) {
		bigRightShift(big, normbits, big);
		r = r >> normbits;
	}
	return r;
}
