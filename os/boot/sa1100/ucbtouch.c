#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mcp.h"
#include "ucbtouch.h"

TouchCal newtouchcal;
int	touch_raw_count = 0;
int	touch_valid_count = 0;
extern int	touch_read_delay;
extern int	touch_l2nreadings;
extern int	touch_minpdelta;
extern int	touch_filterlevel;

static TouchPnt vp[1];	// valid points
static TouchPnt tp[2];	// test points
static int nvp;		// number of valid points
static int ntp;		// number of test points

#define ABS(n)		((n) > 0 ? (n) : -(n))
#define VDOT(a,b)	((a).x*(b).x + (a).y*(b).y)
#define VSUB(a,b)	((TouchPnt){(a).x-(b).x, (a).y-(b).y})
#define VADD(a,b)	((TouchPnt){(a).x+(b).x, (a).y+(b).y})
#define VNEG(a)		((TouchPnt){-(a).x, -(a).y})


static int
touchreadrawx(int n)
{
	int i = 1<<touch_l2nreadings;
	ulong t = 0;
	n += TOUCH_READ_X1;
	mcptouchsetup(n);
	microdelay(touch_read_delay);
	while(i--) 
		t += mcpadcread(n);
	return t >> touch_l2nreadings;
}

static int
touchreadrawy(int n)
{
	int i = 1<<touch_l2nreadings;
	ulong t = 0;
	n += TOUCH_READ_Y1;
	mcptouchsetup(n);
	microdelay(touch_read_delay);
	while(i--) 
		t += mcpadcread(n);
	return t >> touch_l2nreadings;
}

static int
touchreadrawp(void)
{
	mcptouchsetup(TOUCH_READ_P1);
	// microdelay(touch_read_delay);
	return mcpadcread(TOUCH_READ_P1);
}

int
touchpressed(void)
{
	if(touchreadrawp() > touchcal.ptp)
		return 1;
	else {
		nvp = ntp = 0;
		return 0;
	}
}

int
touchreleased(void)
{
	if(touchreadrawp() < touchcal.ptr) {
		nvp = ntp = 0;
		return 1;
	} else
		return 0;
}

void
touchsetrawcal(int q, int n, int v)
{
	switch(n) {
	case 0:	newtouchcal.p[q].x = v; break;
	case 1: newtouchcal.p[q].y = v; break;
	default:
		n -= 2;
		if(n&1)
			newtouchcal.r[n>>1][q].y = v;
		else
			newtouchcal.r[n>>1][q].x = v;
	}
	newtouchcal.ptp = touchcal.ptp;
	newtouchcal.ptr = touchcal.ptr;
}

int
touchgetrawcal(int q, int n)
{
	switch(n) {
	case 0:	return touchcal.p[q].x; 
	case 1: return touchcal.p[q].y;
	default:
		n -= 2;
		return (n&1) ? touchcal.r[n>>1][q].y : touchcal.r[n>>1][q].x;
	}
}


void
touchrawcal(int q, int px, int py)
{
	TouchCal *tc = &newtouchcal;
	TouchPnt *p = &tc->p[q];
	TouchPnt *r;
	int i;
	int n = 0;
	int pv;
	static int svp = 0;
	static int srp = 0;
	static int ptp = 1023;
	static int ptr = 0;

	*p = (TouchPnt){px, py};
	for(i=0; i<4; i++) 
		tc->r[i][q] = (TouchPnt){0, 0};

	if(svp == 0) {
		for(i=0; i<1000; i++) {
			pv = touchreadrawp();
			srp = pv > srp ? pv : srp;
			pv = srp-pv;
			svp = pv > svp ? pv : svp;
		}
		ptp = srp+touch_minpdelta+svp;
		// print("srp=%d svp=%d ptp=%d\n", srp, svp, ptp);
	}
	while(pv = touchreadrawp(), pv <= ptp)
		;
	do {
		for(i=0; i<4; i++) {
			r = &tc->r[i][q];
			r->x += touchreadrawx(i);
			r->y += touchreadrawy(i);
		}
		pv = touchreadrawp();
		ptp = (ptp*7+pv)>>3;
		ptr = (srp+ptp)>>1;
		n++;
	} while(pv > ptr);
	while(pv > srp) 
		pv = touchreadrawp();
	tc->ptp = (ptp+ptr)>>1;
	tc->ptr = ptr;
	// print(" p: %d/%d/%d %d/%d\n", srp, ptp, ptr, tc->ptp, tc->ptr);
	for(i=0; i<4; i++) {
		r = &tc->r[i][q];
		// print("%d/%d %d: %d,%d -> %d,%d\n", q, i, n,
		//		r->x, r->y,
		//		r->x/n, r->y/n);
		r->x /= n; 
		r->y /= n;
	}
}

int
touchcalibrate(void)
{
	int i;
	TouchCal *tc = &newtouchcal;
	TouchTrans *t = tc->t;
	TouchPnt *p = tc->p;
	TouchPnt p1 = VSUB(p[1], p[0]);
	TouchPnt p2 = VSUB(p[2], p[0]);
	tc->err = (TouchPnt){0,0};
	for(i=0; i<4; i++, t++) {
		int xe, ye;
		TouchPnt *r = tc->r[i];
		TouchPnt r1 = VSUB(r[1], r[0]);
		TouchPnt r2 = VSUB(r[2], r[0]);
		int d = (r1.y*r2.x-r1.x*r2.y) >> 6;
		if(d == 0)
			return 0;
		// print("p0=%d,%d p1=%d,%d p2=%d,%d\nr0=%d,%d r1=%d,%d r2=%d,%d d=%d\n",
		//	p[0].x, p[0].y, p1.x, p1.y, p2.x, p2.y,
		//	r[0].x, r[0].y, r1.x, r1.y, r2.x, r2.y, d);
		t->xxm = ((p2.x*r1.y-p1.x*r2.y)<<10)/d;
		t->xym = ((p1.x*r2.x-p2.x*r1.x)<<10)/d;
		t->xa = (p[0].x<<16)-t->xxm*r[0].x-t->xym*r[0].y;
		t->yxm = ((p2.y*r1.y-p1.y*r2.y)<<10)/d;
		t->yym = ((p1.y*r2.x-p2.y*r1.x)<<10)/d;
		t->ya = (p[0].y<<16)-t->yxm*r[0].x-t->yym*r[0].y;
		// print("xxm=%d xym=%d xa=%d  yxm=%d yym=%d ya=%d\n", 
		//	t->xxm, t->xym, t->xa,
		//	t->yxm, t->yym, t->ya);
		xe = r[3].x*t->xxm + r[3].y*t->xym + t->xa - (p[3].x << 16);
		ye = r[3].x*t->yxm + r[3].y*t->yym + t->ya - (p[3].y << 16);
		// print(" %d: err: %d,%d\n", i, xe>>16, ye>>16);
		xe >>= 1;
		ye >>= 1;
		t->xa -= xe;
		t->ya -= ye;
		tc->err.x += ABS(xe);
		tc->err.y += ABS(ye);
	}
	tc->err = (TouchPnt){(tc->err.x + (1<<18) - 1) >> 18,
   			     (tc->err.y + (1<<18) - 1) >> 18};
	// print(" total err: %d,%d\n", tc->err.x, tc->err.y);

	i = VDOT(p1, p1)/(20*20);
	if(tc->err.x*tc->err.x > i || tc->err.y*tc->err.y > i)
		return 0;
	newtouchcal.var = touchcal.var;	   // until we can compute it...
	touchcal = newtouchcal;
	return 1;
}

static int correlate(TouchPnt p1, TouchPnt p2, TouchPnt p3)
{
	TouchPnt v1, v2;
	int d;
	int n;
	v1 = VSUB(p2, p1); 
	v2 = VSUB(p3, p2);
	while(ABS(v1.x) > 31 || ABS(v1.y) > 31
			|| ABS(v2.x) > 31 || ABS(v2.y) > 31) {
		v1.x >>= 1;
		v1.y >>= 1;
		v2.x >>= 1;
		v2.y >>= 1;
	}
	d = VDOT(v1, v1)*VDOT(v2, v2);
	if(d == 0)
		return 1<<10;
	if((p1.x >> 4) == (p3.x >> 4) && (p1.y >> 4) == (p3.y >> 4))
		return 0;
	n = VDOT(v1, v2);
	return ((n*ABS(n)) << 10)/d;
}


int
touchreadxy(int *fx, int *fy)
{
	int i;
	TouchTrans *t = touchcal.t;
	TouchPnt p = (TouchPnt){0,0};

	for(i=0; i<4; i++, t++) {
		int rx = touchreadrawx(i);
		int ry = touchreadrawy(i);
		p.x += (rx*t->xxm + ry*t->xym + t->xa)>>14;
		p.y += (rx*t->yxm + ry*t->yym + t->ya)>>14;
	}
	if(touchreleased())
		return 0;
	++touch_raw_count;

	/* (uncomment to disable filtering)
		*fx = p.x >> 4;
		*fy = p.y >> 4;
		return 1;
	*/

	// screen_putpixel(p.x>>4, p.y>>4,	0);

	if(ntp > 1 || (ntp > 0 && nvp > 0)) {
		TouchPnt *p0 = nvp > 0 ? &vp[nvp-1] : &tp[ntp-2];
		TouchPnt *p1 = &tp[ntp-1];
		int c1 = correlate(*p0, *p1, p);
		//print("A %d: (%d,%d) %d,%d %d,%d %d,%d [%d %d]\n", c1,
		//		vp[0].x>>4, vp[0].y>>4,
		//		p0->x>>4, p0->y>>4,
		//		p1->x>>4, p1->y>>4,
		//		p.x>>4, p.y>>4, nvp, ntp);
		if(ntp > 1) {
			int c2 = correlate(*p0, tp[ntp-2], p);
			//print("B %d: (%d,%d) %d,%d %d,%d %d,%d [%d %d]\n", c2,
			//	vp[0].x>>4, vp[0].y>>4,
			//	p0->x>>4, p0->y>>4,
			//	tp[ntp-2].x>>4, tp[ntp-2].y>>4,
			//	p.x>>4, p.y>>4, nvp, ntp);
			if(c2 > c1) {
				c1 = c2;
				p1 = &tp[ntp-2];
			}
		}
		if(c1 <= touch_filterlevel && ntp > 1) {
			int c2 = correlate(tp[ntp-2], tp[ntp-1], p);
			//print("C %d: (%d,%d) %d,%d %d,%d %d,%d [%d %d]\n", c2,
			//	vp[0].x>>4, vp[0].y>>4,
			//	tp[ntp-2].x>>4, tp[ntp-2].y>>4,
			//	tp[ntp-1].x>>4, tp[ntp-1].y>>4,
			//	p.x>>4, p.y>>4, nvp, ntp);
			if(c2 > c1) {
				c1 = c2;
				// p0 = &tp[ntp-2];
				p1 = &tp[ntp-1];
			}
		}
		if(c1 > touch_filterlevel) {
			if(nvp > 0) {
				TouchPnt v = VSUB(*p1, vp[0]);
				if((ABS(v.x) >> 4) <= touchcal.var.x
	    				 && (ABS(v.y) >> 4) <= touchcal.var.y) {
					tp[0] = p;
					ntp = 1;
					return 0;
				}
			}
			vp[0] = *p1;
			nvp = 1;
			touch_valid_count++;
			*fx = p1->x>>4;
			*fy = p1->y>>4;
			// print("%d: %d,%d\n", c1, p1->x>>4, p1->y>>4);
			tp[0] = p;
			ntp = 1;
			return 1;
		} 
	}
	if(ntp == 2) {
		/*
		if(nvp > 0)
			vp[0] = (TouchPnt){(vp[0].x+tp[0].x)>>1,
						(vp[0].y+tp[0].y)>>1};
		tp[0] = (TouchPnt){(tp[0].x+tp[1].x)>>1, (tp[0].y+tp[1].y)>>1};
		*/
		tp[0] = tp[1];
		ntp--;
	}
	tp[ntp++] = p;
	return 0;
}

