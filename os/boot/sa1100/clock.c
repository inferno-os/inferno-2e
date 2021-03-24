#include <u.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "io.h"

void (*delayf)(void) = 0;

// macros for fixed-point math

ulong _mularsv(ulong m0, ulong m1, ulong a, ulong s);

/* truncated: */
#define FXDPTDIV(a,b,n) ((ulong)(((uvlong)(a) << (n)) / (b)))
#define MAXMUL(a,n)     ((ulong)((((uvlong)1<<(n))-1)/(a)))
#define MULDIV(x,a,b,n) (((x)*FXDPTDIV(a,b,n)) >> (n)) 
#define MULDIV64(x,a,b,n) ((ulong)_mularsv(x, FXDPTDIV(a,b,n), 0, (n)))

/* rounded: */
#define FXDPTDIVR(a,b,n) ((ulong)((((uvlong)(a) << (n))+((b)/2)) / (b)))
#define MAXMULR(a,n)     ((ulong)((((uvlong)1<<(n))-1)/(a)))
#define MULDIVR(x,a,b,n) (((x)*FXDPTDIVR(a,b,n)+(1<<((n)-1))) >> (n)) 
#define MULDIVR64(x,a,b,n) ((ulong)_mularsv(x, FXDPTDIVR(a,b,n), 1<<((n)-1), (n)))


// these routines are all limited to a maximum of 1165 seconds,
// due to the wrap-around of the OSTIMER

ulong
timer_start(void)
{
	return OSTMRREG->oscr;
}

ulong
timer_ticks(ulong t0)
{
	return OSTMRREG->oscr - t0;
}

int
timer_devwait(ulong *adr, ulong mask, ulong val, int ost)
{
	int i;
	ulong t0 = timer_start();
	while((*adr & mask) != val) 
		if(timer_ticks(t0) > ost)
			return ((*adr & mask) == val) ? 0 : -1;
		else
			for (i = 0; i < 10; i++);	/* don't pound OSCR too hard! */
	return 0;
}

void
timer_setwatchdog(int ost)
{
	OstmrReg *ostmr = OSTMRREG;
	ostmr->osmr[3] = ostmr->oscr + ost;
	if(ost) {
		ostmr->ossr = (1<<3);
		ostmr->oier |= (1<<3);
		ostmr->ower = 1;
	} else 
		ostmr->oier &= ~(1<<3);
}

void
timer_delay(int ost)
{	
	ulong t0 = timer_start();
	while(timer_ticks(t0) < ost)
		;
}


ulong
us2tmr(int us)
{
	return MULDIV64(us, TIMER_HZ, 1000000, 24);
}

int
tmr2us(ulong t)
{
	return MULDIV64(t, 1000000, TIMER_HZ, 24);
}

void
microdelay(int us)
{
	ulong t0 = timer_start();
	ulong ost = us2tmr(us);
	while(timer_ticks(t0) <= ost)
		;
}


ulong
ms2tmr(int ms)
{
	return MULDIV64(ms, TIMER_HZ, 1000, 20);
}

int
tmr2ms(ulong t)
{
	return MULDIV64(t, 1000, TIMER_HZ, 32);
}

void
delay(int ms)
{
	ulong t0 = timer_start();
	ulong ost;
	if(delayf)
		delayf();
	if(ms == 0) 
		return;
	ost = ms2tmr(ms);
	while(timer_ticks(t0) <= ost)
		if(delayf)
			delayf();
}
