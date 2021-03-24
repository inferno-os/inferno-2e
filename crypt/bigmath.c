/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

#define LONG_ADDSTEP(i, C, A, B) {\
	suml = (A)[(i)] + (B)[(i)];\
	sumh = (suml < (A)[(i)]);\
	(C)[(i)] = carry + suml;\
	carry = ((C)[(i)] < suml) + sumh;\
}

#define CARRYSTEP(i) {\
	cp[(i)] = carry + ap[(i)];\
	carry = (cp[(i)] < carry);\
}

void
bigaddition(BigInt a, BigInt b, BigInt c)
{
	NumType carry, sumh, suml;
	BigData ap, bp, cp;
	int i;
	BigInt tmp;
	
	if (LENGTH(a) < LENGTH(b)) {
		tmp = a;
		a = b;
		b = tmp;
	}
	
	GUARANTEE(c, LENGTH(a) + 1);
	ap = NUM(a);
	bp = NUM(b);
	cp = NUM(c);
	i = LENGTH(b);
	carry = 0;
	ap -= 4 - (i&3);
	bp -= 4 - (i&3);
	cp -= 4 - (i&3);
	switch (i & 3) {
	case 3:         LONG_ADDSTEP(1,cp,ap,bp);
	case 2:         LONG_ADDSTEP(2,cp,ap,bp);
	case 1:         LONG_ADDSTEP(3,cp,ap,bp);
	case 0:
		ap += 4;
		bp += 4;
		cp += 4;
		i -= 4;
	}
	while (i >= 0) {
		LONG_ADDSTEP(0,cp,ap,bp);
		LONG_ADDSTEP(1,cp,ap,bp);
		LONG_ADDSTEP(2,cp,ap,bp);
		LONG_ADDSTEP(3,cp,ap,bp);
		ap += 4;
		bp += 4;
		cp += 4;
		i -= 4;
	}
	
	i = LENGTH(a) - LENGTH(b);
	ap -= 4 - (i&3);
	cp -= 4 - (i&3);
	switch (i & 3) {
	case 3:         CARRYSTEP(1);
	case 2:         CARRYSTEP(2);
	case 1:         CARRYSTEP(3);
	case 0:
		ap += 4;
		cp += 4;
		i -= 4;
	}
	while (i >= 0) {
		CARRYSTEP(0);
		CARRYSTEP(1);
		CARRYSTEP(2);
		CARRYSTEP(3);
		ap += 4;
		cp += 4;
		i -= 4;
	}
	
	if (carry) {
		*cp = carry;
		LENGTH(c) = LENGTH(a) + 1;
	} else
		LENGTH(c) = LENGTH(a);
	
	SIGN(c) = SIGN(a);
	trim(c);
}

#define LONG_SUBSTEP(i, C, A, B) {\
	suml = (long)((A)[(i)] - (B)[(i)]);\
	sumh = (long)(-((ulong)suml > (A)[(i)]));\
	(C)[(i)] = (ulong)((long)suml + carry);\
	carry = (long)(-((C)[(i)] > (ulong)suml)) + sumh;\
}

#define LONG_BORROWSTEP(i, C, A) {\
	(C)[(i)] = (ulong)((long)(A)[(i)] + carry);\
	carry = (long)(-((C)[(i)] > (A)[(i)]));\
}

void
bigsub(BigInt a, BigInt b, BigInt c)
{
	long sumh, suml, carry;
	BigData ap, bp, cp;
	int i;
	
	GUARANTEE(c, LENGTH(a));
	ap = NUM(a);
	bp = NUM(b);
	cp = NUM(c);
	i = LENGTH(b);
	carry = 0;
	ap -= 4 - (i&3);
	bp -= 4 - (i&3);
	cp -= 4 - (i&3);
	switch (i & 3) {
	case 3:         LONG_SUBSTEP(1,cp,ap,bp);
	case 2:         LONG_SUBSTEP(2,cp,ap,bp);
	case 1:         LONG_SUBSTEP(3,cp,ap,bp);
	case 0:
		ap += 4;
		bp += 4;
		cp += 4;
		i -= 4;
	}
	while (i >= 0) {
		LONG_SUBSTEP(0,cp,ap,bp);
		LONG_SUBSTEP(1,cp,ap,bp);
		LONG_SUBSTEP(2,cp,ap,bp);
		LONG_SUBSTEP(3,cp,ap,bp);
		ap += 4;
		bp += 4;
		cp += 4;
		i -= 4;
	}
	
	i = (int)(LENGTH(a) - LENGTH(b));
	ap -= 4 - (i&3);
	cp -= 4 - (i&3);
	switch (i & 3) {
	case 3:         LONG_BORROWSTEP(1,cp,ap);
	case 2:         LONG_BORROWSTEP(2,cp,ap);
	case 1:         LONG_BORROWSTEP(3,cp,ap);
	case 0:
		ap += 4;
		cp += 4;
		i -= 4;
	}
	while (i >= 0) {
		LONG_BORROWSTEP(0,cp,ap);
		LONG_BORROWSTEP(1,cp,ap);
		LONG_BORROWSTEP(2,cp,ap);
		LONG_BORROWSTEP(3,cp,ap);
		ap += 4;
		cp += 4;
		i -= 4;
	}
	
	LENGTH(c) = LENGTH(a);
	
	if (carry != 0)
		handle_exception(CRITICAL, "bigsub: carry is non zero when done");
	
	/*
	 *        Remove leading zero words.  This can only happen in
	 *        bigSubtract because all bignums are positive.
	 */
	trim(c);
}

#define SHIFTSTEP(i) {bp[i] = ap[i];}

void
bigleftshift(BigInt a, BigInt b, int nwords)
{
	BigData ap, bp;
	int i;
	
	GUARANTEE(b, (ulong)(LENGTH(a) + nwords));
	i = LENGTH(a);
	LENGTH(b) = LENGTH(a) + (ulong)nwords;
	ap = NUM(a) + i;
	bp = NUM(b) + i + nwords;
	
	ap -= (i&3);
	bp -= (i&3);
	switch (i & 3) {
	case 3:        SHIFTSTEP(2);
	case 2:         SHIFTSTEP(1);
	case 1:        SHIFTSTEP(0);
	case 0:
		i -= 4;
	}
	while (i >= 0) {
		ap -= 4;
		bp -= 4;
		SHIFTSTEP(3);
		SHIFTSTEP(2);
		SHIFTSTEP(1);
		SHIFTSTEP(0);
		i -= 4;
	}
	
	i = nwords;
	bp -= i&3;
	switch (i&3) {
	case 3:      bp[2] = 0;
	case 2:      bp[1] = 0;
	case 1:      bp[0] = 0;
	case 0:
		i -= 4;
		bp -= 4;
	}
	while (i >= 0) {
		bp[3] = 0;
		bp[2] = 0;
		bp[1] = 0;
		bp[0] = 0;
		i -= 4;
		bp -= 4;
	}
	
	trim(b);
	
}


void
bigrightshift(BigInt a, BigInt b, int nwords)
{
	int i, j, alen;
	BigData ap, oap;
	
	alen = LENGTH(a);
	if (a == b) {
		i = alen - nwords;
		if (i > 0) {
			LENGTH(a) = (ulong)i;
			ap = NUM(a) + nwords;
			oap = NUM(a);
			for (j=0; j<i; j++)
				oap[j]=ap[j];
		}
		else
			reset_big(a, (NumType)0);
		return;
	}
	
	LENGTH(a) -= (ulong)nwords;
	if (LENGTH(a) > 0) {
		NUM(a) += nwords;
		bigCopy(a, b);
		NUM(a) -= nwords;
	}
	else {
		LENGTH(b) = 1;
		NUM(b)[0] = 0;
	}
	LENGTH(a) = (ulong)alen;
	
}



/* Basic n^2 multiplication */
void
lbigmult(BigInt a, BigInt b, BigInt c)
{
	int i;
	NumType m;
	NumType *bp = NUM(b);
	
	if ((c == b) || (c == a))
		handle_exception(CRITICAL, "lbigmult: product pointer cannot be the same as either multiplicand.");
	
	reset_big(c, (NumType)0);
	
	if ((ZERO(a) != 1) && (ZERO(b) != 1)) {
		GUARANTEE(c, LENGTH(a) + LENGTH(b));
		for (i = 0; i < LENGTH(b); i++) {
			m = bp[i];
			numtype_bigmult(a, m, c, i);
		}
		SIGN(c) = SIGN(a) * SIGN(b);
	}
	trim(c);
}

void
bigsquare(BigInt a, BigInt c)
{
	
	if (a == c)
		handle_exception(CRITICAL, "bigsquare: product pointer cannot be the same as multiplicand.");
	
	GUARANTEE(c, 2*LENGTH(a));
	reset_big(c, (NumType)0);
	numtype_bigsquareN(NUM(a), NUM(c), LENGTH(a));
	LENGTH(c) = 2*LENGTH(a);
	trim(c);
	
}
