#include <lib9.h>

void*
memset(void *ap, int c, ulong n)
{
	char *p = ap;
	char *e = p+n;
	while(p < e)
		*p++ = c;
	return ap;
}
