#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#include "ip.h"

#define DPRINT if(0)print

	Proto	ipextproto;

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

enum
{
	/* filter types */
	Fia=	0,		/* interface address */
	Fsa,			/* source address */
	Fda,			/* destination address */
	Fbody,			/* message body offset */
};

typedef struct Ipfilter
{
	Ipfilter	*next;
	int		offset;		/* negative means interface address */
	ulong		mask;
	ulong		val;
};

typedef struct Ipextproto
{
	Ipextproto	*next;
	Conv		*c;
	Ipfilter	*f;
} Ipextproto;

static Ipextproto *ipext[256];

static char*
ipextprotoconnect(Conv *c, char **argv, int argc)
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

	lock(&ipextproto);
	if(Fsbuiltinproto(&fs, proto) || ipextt2c[proto] != nil){
		unlock(&ipextproto);
		return "handler exists for protocol";
	}
	c->lport = proto;
	ipextt2c[proto] = c;
	unlock(&ipextproto);

	Fsconnected(&fs, c, nil);

	return nil;
}

static char*
ipextprotoconnect(Conv *c, char **argv, int argc)
{
	Ipextproto *p, **l;
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

	p = (Ipextproto*)c->ptcl;

	memset(p, 0, sizeof(Ipextproto));

	lock(&ipextproto);
	for(l = &ipext; *l; l = &(*l)->next)
		;
	*l = p;
	c->lport = proto;
	unlock(&ipextproto);

	Fsconnected(&fs, c, nil);

	return nil;
}

static int
ipextprotostate(char **msg, Conv *c)
{
	USED(c);
	*msg = "protocol";
	return 1;
}

static void
ipextprotocreate(Conv *c)
{
	c->rq = qopen(64*1024, 0, 0, c);
	c->wq = qopen(64*1024, 0, 0, 0);
}

static char*
ipextprotoannounce(Conv*, char**, int)
{
	return "ipextproto does not support announce";
}

/* called with c locked, we must unlock */
static void
ipextprotoclose(Conv *c)
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
ipextprotokick(Conv *c, int l)
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
_ipextprotoiput(Block *bp)
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
ipextprotoinit(Fs *fs)
{
	ipextproto.name = "ipextproto";
	ipextproto.kick = ipextprotokick;
	ipextproto.connect = ipextprotoconnect;
	ipextproto.announce = ipextprotoannounce;
	ipextproto.state = ipextprotostate;
	ipextproto.create = ipextprotocreate;
	ipextproto.close = ipextprotoclose;
	ipextproto.ctl = tcpctl;
	ipextproto.rcv = nil;
	ipextproto.ctl = nil;
	ipextproto.ipproto = -1;
	ipextproto.nc = 64;
	ipextproto.ptclsize = 1;

	ipextprotoiput = _ipextprotoiput;

	Fsproto(fs, &ipextproto);
}
