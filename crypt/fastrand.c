#include	"lib9.h"
#include	<libcrypt.h>

/*
 *	algorithm by
 *	D. P. Mitchell & J. A. Reeds
 *
 *	adapted to stretch the rate of a true random
 *	number generator by D. L. Presotto
 */
#define	LEN	607
#define	TAP	273
#define	MASK	0x7fffffffL
#define	A	48271
#define	M	2147483647
#define	Q	44488
#define	R	3399
#define FREQ	256

static	ulong	rng_vec[LEN];
static	int	rng_feed;
static	int	started;
static	ulong	rounds;

static void
seedrand(void)
{
	long lo, hi, x, seed;
	int i;

	seed = truerand()%M;
	if(seed < 0)
		seed += M;
	if(seed == 0)
		seed = 89482311;
	x = seed;

	/*
	 *	Initialize by x[n+1] = 48271 * x[n] mod (2**31 - 1)
	 */
	for(i = -20; i < LEN; i++) {
		hi = x / Q;
		lo = x % Q;
		x = A*lo - R*hi;
		if(x < 0)
			x += M;
		if(i >= 0)
			rng_vec[i] = x;
		if(((rounds++)%FREQ) == 0)
			x ^= truerand();
	}
}

/*
 *  given that there isn't a right answer, this routine is reentrant
 *  despite the globals
 */
ulong
fastrand(void)
{
	ulong x;
	int p, tap;

	if(!started){
		seedrand();
		started = 1;
	}
	p = rng_feed;
	if(p < 0)
		p = 0;

	if(p >= LEN-1)
		p = 0;
	else
		p = p + 1;
	rng_feed = p;

	tap = p + TAP;
	if(tap >= LEN)
		tap -= LEN;

	x = (rng_vec[p] + rng_vec[tap]) & MASK;
	if(((rounds++)%FREQ) == 0)
		rng_vec[x%LEN] ^= truerand();
	rng_vec[p] = x;
        return x;
}
