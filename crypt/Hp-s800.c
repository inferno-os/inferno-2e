#include "lib9.h"

/*
 * It is unclear that there is any point in hand-tuning PA-RISC assembly
 * language for a function that can be compiled.  The instruction set is
 * arcane, and the compiler sophisticated.  Also, standard optimizations
 * seem to work as well as any others the compiler has, on this short
 * function.
 */

ulong
umult(ulong m1, ulong m2, ulong *dhi)
{
	vlong product;

	/* HP compiler type promotion rules dictage cast. */
	product= (unsigned long long)m1 * (unsigned long long)m2;
	*dhi = product >> 32;
	return product & ~0;
}
