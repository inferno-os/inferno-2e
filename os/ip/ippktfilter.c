#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#include "ip.h"

Proto	ippktfilter;

typedef struct Iphdr
{
	uchar	vihl;		/* Version and header length */
	uchar	tos;		/* Type of service */
	uchar	length[2];	/* packet length */
	uchar	id[2];		/* Identification */
	uchar	frag[2];	/* Fragment information */
	uchar	ttl;		/* Time to live */
	uchar	proto;		/* Protocol */
	uchar	cksum[2];	/* Header checksum */
	uchar	src[4];		/* Ip source */
	uchar	dst[4];		/* Ip destination */
} Iphdr;

/*
 *  filter spec looks like "expr && expr && expr ..."
 *  where expr can be one of
 *	proto==val
 *	src&address-mask==val
 *	dst&address-mask==val
 *	iaddr&address-mask==val
 *	data[ulong-offset] & mask ==val
 */
enum {
	Fmask=		1<<3,
	Fset=		1<<4,
	Frange=		1<<5,

	Fproto=		0,
	Fsrc,
	Fdst,
	Fiaddr,
	Fdata,
};

typedef struct Ipfilter
{
	Ipfilter	*next;
	int		offset;
	ulong		mask;
	ulong		val;
};

typedef struct Ippktfilter
{
	Ippktfilter	*next;
	Conv		*c;
	Ipfilter	*f;
} Ippktfilter;

static Ippktfilter *proto[256];		/* first level choosed protocol */

static char*
ippktfilterconnect(Conv *c, char **argv, int argc)
{
	int proto;

	switch(argc){
	case 2:
		proto = atoi(argv[1]);
		if(proto < 0 || proto > 255)
			return "bad args to connect";
		break;
	default:
		return "bad args to connect";
		break;
	}

	Fsconnected(&fs, c, nil);

	return nil;
}

static int
ippktfilterstate(char **msg, Conv *c)
{
	USED(c);
	*msg = "protocol";
	return 1;
}

static void
ippktfiltercreate(Conv *c)
{
	c->rq = qopen(64*1024, 0, 0, c);
	c->wq = qopen(64*1024, 0, 0, 0);
}

static char*
ippktfilterannounce(Conv*, char**, int)
{
	return "ippktfilter does not support announce";
}

/* called with c locked, we must unlock */
static void
ippktfilterclose(Conv *c)
{
	ipextt2c[c->lport] = nil;

	qclose(c->rq);
	qclose(c->wq);
	qclose(c->eq);
	c->laddr = 0;
	c->raddr = 0;
	c->lport = 0;
	c->rport = 0;

	unlock(c);
}

static void
ippktfilterkick(Conv *c, int l)
{
	Block *bp;
	Iphdr *ih;

	USED(l);

	bp = qget(c->wq);
	if(bp == nil)
		return;
	ih = (Iphdr*)(bp->rp);

	/* sanity clause */
	if(blocklen(bp) < sizeof(*ih)){
		freeblist(bp);
		return;
	}

	ih->proto = c->lport;	/* lest someone spoof another proto */
	ipiput(nil, bp);
}

static void
_ippktfilteriput(Block *bp)
{
	Iphdr *ih;
	Conv *c;

	ih = (Iphdr*)(bp->rp);
	c = ipextt2c[ih->proto];
	if(c == nil){
		freeblist(bp);
		return;
	}

	/* make sure it reaches external proc in one piece */
	if(bp->next)
		bp = concatblock(bp);

	qpass(c->rq, bp);
	/*c->rq.randomdrop(128*1024);*/
}

void
ippktfilterinit(Fs *fs)
{
	ippktfilter.name = "ippktfilter";
	ippktfilter.kick = ippktfilterkick;
	ippktfilter.connect = ippktfilterconnect;
	ippktfilter.announce = ippktfilterannounce;
	ippktfilter.state = ippktfilterstate;
	ippktfilter.create = ippktfiltercreate;
	ippktfilter.close = ippktfilterclose;
	ippktfilter.ctl = tcpctl;
	ippktfilter.rcv = nil;
	ippktfilter.ctl = nil;
	ippktfilter.ipproto = -1;
	ippktfilter.nc = 64;
	ippktfilter.ptclsize = 1;

	ippktfilteriput = _ippktfilteriput;

	Fsproto(fs, &ippktfilter);
}
