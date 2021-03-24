#include <u.h>
#include "dat.h"
#include "fns.h"

/* note: returns 0 for invalid addresses */

ulong
va2pa(void *v)
{
	int idx = (ulong)v >> 20;
	ulong pte = ((ulong*)conf.pagetable)[idx];
	if((pte & 3) != 2)
		return 0;
	return (pte&0xfff00000)|((ulong)v & 0x000fffff);
}

