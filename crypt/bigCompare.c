/*
 *        Big Arithmetic routines
 *        coded by D. P. Mitchell and Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

int
bigCompare(BigInt a, BigInt b)
{
	BigData ap, bp;
	int i;
	
	trim(a);
	trim(b);
	
	if (LENGTH(a) != LENGTH(b))
		return (LENGTH(a) - LENGTH(b));
	i = LENGTH(a);
	ap = NUM(a) + i;
	bp = NUM(b) + i;
	while (--i >= 0 && *--ap == *--bp)
		;
	if (i < 0)
		return 0;
	if (*ap < *bp)
		return -1;
	else
		return 1;
}

