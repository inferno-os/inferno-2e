#include <u.h>
#ifndef EMU
#include <libc.h>
#endif
typedef unsigned int size_t;

typedef jmp_buf osjmpbuf;
#define	ossetjmp(buf)	setjmp(buf)

extern	int	msize(void*);
extern	void*	mallocz(int, int);

#define DBL_DIG		15
#define DBL_MAX_10_EXP	308
#define DBL_MAX_EXP	1024
#define FLT_RADIX	2

#define FLT_ROUNDS 1
#define Storeinc(a,b,c) (*a++ = b << 16 | c & 0xffff)
#define	setbinmode()
#define __LITTLE_ENDIAN

enum
{
	ICOSSCALE = 1024
};

extern	vlong	strtoll(char*, char**, int);
extern	int		runestrlen(Rune*);
extern	void		icossin(int, int*, int*);
extern	void		icossin2(int, int, int*, int*);

extern	ulong		getcallerpc(void*);
