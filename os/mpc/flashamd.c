#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"../port/error.h"

#include	"flashif.h"

enum {
	DQ7 = 0x80808080,
	DQ6 = 0x40404040,
	DQ5 = 0x20202020,
	DQ3 = 0x08080808,
	DQ2 = 0x04040404,
};

#define	DPRINT	if(0)print
#define	EPRINT	if(1)print

static int
amdwait(ulong *p, ulong ticks)
{
	ulong v0, v1;

	ticks += m->ticks+1;
	v0 = *p;
	for(;;){
		sched();
		v1 = *p;
		if((v1 & DQ6) == (v0 & DQ6))
			break;
		if((v1 & DQ5) == DQ5){
			v0 = *p;
			v1 = *p;
			if((v1 & DQ6) == (v0 & DQ6))
				break;
			EPRINT("flash: DQ5 error: %8.8lux %8.8lux\n", v0, v1);
			return 0;
		}
		if(m->ticks >= ticks){
			EPRINT("flash: timed out: %8.8lux\n", *p);
			return -1;
		}
		v0 = v1;
	}
	return 1;
}

static int
eraseall(Flash *f)
{
	ulong *p;
	int s;

	DPRINT("flash: erase all\n");
	p = (ulong*)f->addr;
	s = splhi();
	*(p+0x555) = 0xAAAAAAAA;
	*(p+0x2AA) = 0x55555555;
	*(p+0x555) = 0x80808080;
	*(p+0x555) = 0xAAAAAAAA;
	*(p+0x2AA) = 0x55555555;
	*(p+0x555) = 0x10101010;	/* chip erase */
	splx(s);
	if(amdwait(p, MS2TK(64*1000)) <= 0){
		*p = 0xF0F0F0F0;	/* reset */
		f->unusable = ~0;
		return -1;
	}
	return 0;
}

static int
erasezone(Flash *f, int zone)
{
	ulong *p;
	int s;

	DPRINT("flash: erase zone %d\n", zone);
	if(zone & ~7)
		return -1;	/* bad zone */
	p = (ulong*)f->addr;
	s = splhi();
	*(p+0x555) = 0xAAAAAAAA;
	*(p+0x2AA) = 0x55555555;
	*(p+0x555) = 0x80808080;
	*(p+0x555) = 0xAAAAAAAA;
	*(p+0x2AA) = 0x55555555;
	*(p+=(zone<<16)) = 0x30303030;	/* sector erase */
	splx(s);
	if(amdwait(p, MS2TK(8*1000)) <= 0){
		*p = 0xF0F0F0F0;	/* reset */
		f->unusable |= 1<<zone;
		return -1;
	}
	return 0;
}

static int
write4(Flash *f, ulong offset, void *buf, long n)
{
	ulong *p, *a, *v;
	int s;

	p = (ulong*)f->addr;
	if(((ulong)p|offset|n)&3)
		return -1;
	n >>= 2;
	a = p + (offset>>2);
	v = buf;
	for(; --n >= 0; v++, a++){
		DPRINT("flash: write %lux %lux -> %lux\n", (ulong)a, *a, *v);
		if(~*a & *v){
			EPRINT("flash: bad write: %lux %lux -> %lux\n", (ulong)a, *a, *v);
			return -1;
		}
		s = splhi();
		*(p+0x555) = 0xAAAAAAAA;
		*(p+0x2AA) = 0x55555555;
		*(p+0x555) = 0xA0A0A0A0;	/* program */
		*a = *v;
		splx(s);
		microdelay(8);
		if(*a != *v){
			microdelay(8);
			while(*a != *v){
				if(amdwait(a, 1) <= 0)
					return -1;
			}
		}
	}
	return 0;
}

static int
reset(Flash *f)
{
	f->id = 0x01;	/* can't use autoselect: might be running in flash */
	f->devid = 0;
	f->write = write4;
	f->eraseall = eraseall;
	f->erasezone = erasezone;
	f->suspend = nil;
	f->resume = nil;
	f->width = 4;
	f->erasesize = 64*1024*f->width;
	*(ulong*)f->addr = 0xF0F0F0F0;	/* reset (just in case) */
	return 0;
}

void
flashamdlink(void)
{
	addflashcard("AMD29F0x0", reset);
}
