/*
 *	Core multiplication routines.
 *	Implemented by Jack Lacy.
 *	Copyright Bell Labs 1994.
 *
 */
#include "lib9.h"
#include <libcrypt.h>

/*
   These utilities must run as efficiently as possible.
   They all make use of inner core multiplication utilities
   in longmult.c.
   numtype_bigmultN()        : utility used by recursiveMultiply() (fastmult.c)
   Assumes a and b have the same length, N.
   numtype_bigsquareN()        : utility used by bigsquare() (bigmath.c)
   and recursiveSquare() (fastmult.c)
   numtype_bigmult()        : utility used by lbigmult() (bigmath.c)
   REDC()                : Modular reduction workhorse used in bigPow() (bigpow.c)
   */

void
numtype_bigmultN(ulong *a, ulong *b, ulong *c, int N)
{
	ulong *ap, *bp, *cp;
	ulong carry, m;
	int i;
	
	ap = a;
	bp = b;
	cp = c;
	
	for (i=0; i<N; i++)
		cp[i] = (NumType)0;
	
	i = 0;
	do {
		m = ap[i];
		carry = LMULT(cp, m, bp, N);
		cp[N] = carry;
		cp++;
		i++;
	} while (i<N);
}

void
numtype_bigsquareN(ulong *a, ulong *c, int N)
{
	ulong *ap, *cp, m;
	int i, j;

	ap = a;
	cp = c;

	BUILDDIAG(cp, ap, N);

	if (N == 1) return;
	
	ap = a;
	cp = c-1;
	i = 0;
	j = 1;
	do {
		cp += 2;
		m = ap[i];
		SQUAREINNERLOOP(cp, m, ap, j, N);
		i++; j++;
	} while (i<N-1);
}

void
numtype_bigmult(BigInt a, ulong sb, BigInt c, int offset)
{
	ulong m, carry;
	ulong *ap, *cp;
	int gap, i;

	i = LENGTH(a) + offset;
	GUARANTEE(c, (ulong)(i + 2));
	gap = LENGTH(c) - i;

	if (gap < 0) {
		i = -gap;
		cp = NUM(c) + LENGTH(c);
		do {
			*cp++ = (ulong)0;
		} while (--i >= 0);
	}

	ap = NUM(a);
	m = sb;
	cp = NUM(c) + offset;
	carry = LMULT(cp, m, ap, (int)LENGTH(a));
	cp += LENGTH(a);

	if ((i=gap) > 0) {
		do {
			cp[0] = cp[0] + carry;
			carry = (cp[0] < carry);
			cp++;
		} while ((carry != 0) && (--i > 0));
	}
	else
		LENGTH(c) = (ulong)(offset + LENGTH(a));

	if (carry) {
		*cp = carry;
		LENGTH(c)++;
	}
	trim(c);

	
}


