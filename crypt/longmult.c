/*
 *      32 bit by 32 bit multiplication primitives.
 *      Jack Lacy
 *      Copyright (c) 1993 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

#define add64(sh, sl, ah, al, bh, bl) {{\
	ulong __c;\
	__c = (al) + (bl);\
	(sh) = (ah) + (bh);\
	if(__c < (al))\
		(sh)++;\
	(sl) = __c;\
}}

#define add32(sh, sl, al, bl) {{\
	ulong __c;\
	__c = (al) + (bl);\
	if(__c < (al))\
		(sh) = 1;\
	else\
		(sh) = 0;\
	(sl) = __c;\
}}

ulong umult(ulong, ulong, ulong*);

#define mul32(dhi, dlo, m1, m2)  dlo = umult(m1, m2, &dhi)

NumType
LMULT(NumType *dst, NumType m, NumType *src, int N)
{
	ulong sumh, suml;
	NumType carry, *ap, *cp, mm;
	int i;
	
	ap = src;
	cp = dst;
	mm = m;
	
	carry = 0;
	for (i=0; i<N; i++) {
		mul32(sumh, suml, mm, ap[i]);
		
		suml += carry;
		if(suml < carry)
			sumh++;
		
		cp[i] += suml;
		carry = sumh;
		if(cp[i] < suml)
			carry++;
	}
	
	return carry;
	
}

void
BUILDDIAG(NumType *dst, NumType *src, int N)
{
	NumType *ap, *cp, m;
	int i;
	
	ap = src;
	cp = dst;
	
	i = 0;
	do {
		m = ap[i];
		mul32(cp[1], cp[0], m, m);
		cp += 2;
	} while (++i < N);
	
}


void
SQUAREINNERLOOP(NumType *dst, NumType m, NumType *src, int start, int end)
{
	NumType *ap, *cp;
	int j;
	ulong prodhi, prodlo;
	ulong sumh, suml, carry;
	
	cp = dst;
	ap = src;
	carry = 0;
	j = start;
	
	do {
		mul32(prodhi, prodlo, m, ap[j]);
		
		add32(sumh, suml, prodlo, prodlo);
		add64(carry, cp[0], sumh, suml, carry, cp[0]);
		
		add32(sumh, suml, prodhi, prodhi);
		add64(sumh, suml, sumh, suml, 0, cp[1]);
		add64(carry, cp[1], sumh, suml, 0, carry);
		cp++;
	} while (++j<end);
	
	cp++;
	
	while ((carry != 0) && (j<2*end)) {
		add32(carry, cp[0], cp[0], carry);
		cp++;
		j++;
	}
}
