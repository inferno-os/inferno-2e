#include "lib9.h"

ulong
umult(ulong a, ulong b, ulong *hi)
{
	vlong product;

	product = (unsigned long long) a * (unsigned long long) b;
	*hi = product >> 32;
	return product & ~0;
}
