#include "lib9.h"

ulong
umult(ulong m1, ulong m2, ulong *dhi)
{
	vlong product;

	product= (unsigned long long) m1 * (unsigned long long) m2;
	*dhi = product >> 32;
	return product & ~0;
}
