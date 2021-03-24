#include <lib9.h>

void*
memmove(void *a1, void *a2, ulong n)
{
	uchar *s1 = a1;
	uchar *s2 = a2;

	if(s1 == s2)
		return s1;
	if(s1 > s2) {
		s1 += n;
		s2 += n;
		while(n > 0) {
			*--s1 = *--s2;
			--n;
		}
	} else {
		while(n > 0) {
			*s1++ = *s2++;
			--n;
		}
	}
	return a1;
}

void*
memcpy(void *a1, void *a2, ulong n)
{
	return memmove(a1, a2, n);
}
