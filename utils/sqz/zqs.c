#include <u.h>
#include <libc.h>
#include <a.out.h>
#include "squeeze.h"

#ifndef nil
#define	nil	((void*)0)
#endif

typedef struct Squeeze Squeeze;
struct Squeeze {
	int	n;
	ulong	tab[7*256];
};

enum {
	CHECK = 1	/* check precise bounds in Squeeze array */
};

#define	GET4(p)	(((((((p)[0]<<8)|(p)[1])<<8)|(p)[2])<<8)|(p)[3])

static	uchar	out[3*1024*1024];
static	uchar	bigb[1024*1024];
static	ulong	top;
static	int	qflag = 1;
static	ulong	chksum, oldsum;
static	int	rdtab(int, Squeeze*, int);
static	long	unsqueezefd(int, void*);
static	uchar*	unsqueeze(uchar*, uchar*, uchar*, Squeeze*, Squeeze*, ulong);
static	uchar*	unsqzseg(int, uchar*, long, ulong);

void
main(int argc, char **argv)
{
	int fd;
	long n;

	if(argc < 2)
		exits("args");
	fd = open(argv[1], OREAD);
	if(fd < 0)
		exits("open");
	n = unsqueezefd(fd, out);
	if(n < 0){
		fprint(2, "zqs: can't unsqueeze\n");
		exits("err");
	}
	if(write(1, out, n) != n){
		fprint(2, "zqs: write error: %r\n");
		exits("err");
	}
	fprint(2, "%ld bytes, %8.8lux csum\n", n, chksum);
	exits(0);
}

static long
unsqueezefd(int fd, uchar *out)
{
	uchar *wp;
	ulong toptxt, topdat;
	long asis, nst, nsd;
	Sqhdr sqh;
	Exec ex;

	if(read(fd, &sqh, SQHDRLEN) != SQHDRLEN)
		return -1;
	if(GET4(sqh.magic) != SQMAGIC)
		return -1;
	if(read(fd, &ex, sizeof(Exec)) != sizeof(Exec))
		return -1;
	toptxt = GET4(sqh.toptxt);
	topdat = GET4(sqh.topdat);
	oldsum = GET4(sqh.sum);
	asis = GET4(sqh.asis);
	if(asis < 0)
		asis = 0;
	nst = GET4(sqh.text);
	nsd = GET4(sqh.data);
	qflag = GET4((uchar*)&ex.magic) == Q_MAGIC;
	if(qflag)
		fprint(2, "PowerPC mode\n");
	wp = unsqzseg(fd, out, nst, toptxt);
	if(wp == nil)
		return -1;
	wp = unsqzseg(fd, wp, nsd, topdat);
	if(wp == nil)
		return -1;
	if(asis){
		if(read(fd, wp, asis) != asis)
			return -1;
		wp += asis;
	}
	return wp-out;
}

static uchar*
unsqzseg(int fd, uchar *wp, long ns, ulong top)
{
	Squeeze sq3, sq4;

	if(ns == 0)
		return wp;
	if(rdtab(fd, &sq3, 0) < 0)
		return nil;
	if(rdtab(fd, &sq4, 8) < 0)
		return nil;
	fprint(2, "tables: %d %d\n", sq3.n, sq4.n);
	if(read(fd, bigb, ns) != ns)
		return nil;
	return unsqueeze(wp, bigb, bigb+ns, &sq3, &sq4, top);
}

static uchar*
unsqueeze(uchar *wp, uchar *rp, uchar *ep, Squeeze *sq3, Squeeze *sq4, ulong top)
{
	ulong nx;
	int code, n;

	if(qflag){
		QREMAP(top);	/* adjust top just once, outside the loop */
	}
	while(rp < ep){
		code = *rp++;
		n = 0;
		nx = code>>4;
		do{
			if(nx == 0){
				nx = top;
			}else{
				if(nx==1){
					if(rp+3 >= ep)
						return nil;
					nx = (((((rp[3]<<8)|rp[2])<<8)|rp[1])<<8)|rp[0];
					rp += 4;
				}else if(nx <= 8){	/* 2 to 8 */
					if(rp+1 >= ep)
						return nil;
					nx = ((nx-2)<<8) | rp[0];
					if(CHECK && nx >= sq4->n)
						return nil;	/* corrupted file */
					nx = sq4->tab[nx] | rp[1];
					rp += 2;
				}else{	/* 9 to 15 */
					if(rp >= ep)
						return nil;	/* corrupted file */
					nx = ((nx-9)<<8) | rp[0];
					if(CHECK && nx >= sq3->n)
						return nil;	/* corrupted file */
					nx = sq3->tab[nx];
					rp++;
				}
				if(rp > ep)
					return nil;	/* corrupted file */
				if(qflag){
					QREMAP(nx);
				}
			}
			wp[0] = nx>>24;
			wp[1] = nx>>16;
			wp[2] = nx>>8;
			wp[3] = nx;
			wp += 4;
			chksum += nx;
			nx = code & 0xF;
		}while(++n == 1);
	}
	return wp;
}

static int
rdtab(int fd, Squeeze *sq, int shift)
{
	uchar b[7*256*5], *p, *ep;
	ulong v, w;
	int i;

	if(read(fd, b, 2) != 2)
		return -1;
	i = (b[0]<<8) | b[1];
	if(1)
		fprint(2, "table: %d\n", i);
	if((i -= 2) > 0){
		if(read(fd, b, i) != i)
			return -1;
	}
	sq->n = 0;
	p = b;
	ep = b+i;
	v = 0;
	while(p < ep){
		w = 0;
		do{
			if(p >= ep)
				return -1;
			w = (w<<7) | (*p & 0x7F);
		}while(*p++ & 0x80);
		v += w;
		if(0)
			fprint(2, "%d %8.8lux %8.8lux\n", sq->n, v, w);
		sq->tab[sq->n++] = v << shift;
	}
	return 0;
}
