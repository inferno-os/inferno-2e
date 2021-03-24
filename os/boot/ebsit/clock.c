#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "bpi.h"
#include "io.h"

void (*delayf)(void) = 0;
int lpms, lpus;

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


/*
void
delay(int msec)
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
*/

// 80 ticks per second
#define OSCR	((ulong*)0x524)

ulong
timer_start(void)
{
	return *OSCR;
}

ulong
timer_ticks(ulong t0)
{
	return *OSCR - t0;
}

int
timer_devwait(ulong *adr, ulong mask, ulong val, int ost)
{
	ulong t0 = timer_start();
	ost++;	/* to correctly handle borderline cases */
	while((*adr & mask) != val) 
		if(timer_ticks(t0) > ost)
			return ((*adr & mask) == val) ? 0 : -1;
	return 0;
}

void
timer_setwatchdog(int ost)
{
	// can't do this
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
	// ulong t0 = timer_start();
	// ulong ost = us2tmr(us);
	// ost++;
	// while(timer_ticks(t0) <= ost)
	//	;
	busyloop(lpus, us);
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
	// ulong t0 = timer_start();
	// ulong ost = ms2tmr(ms);
	// while(timer_ticks(t0) <= ost)
	//	;
	busyloop(lpms, ms);
}

void
clocklink(void)
{
	int n = *(int*)0x520;
	lpus = (3*n) >> 6;
	lpms = (3000*n) >> 6;
}

