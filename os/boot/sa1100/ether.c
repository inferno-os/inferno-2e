#include <lib9.h>
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "etherif.h"
#include "../net/netboot.h"

extern struct arptable_t arptable[];
extern int post_xmit_delay;


struct Ether eth0;

extern int arptimeout;


static struct {
	char*	type;
	int (*reset)(Ether*);
} cards[MaxEther+1];

void
addethercard(char* t, int (*r)(Ether*))
{
	static int ncard;

	if(ncard == MaxEther)
		panic("too many ether cards");
	cards[ncard].type = t;
	cards[ncard].reset = r;
	ncard++;
}

void
memcpy4(uchar *d, uchar *s)
{	
	d[0] = s[0];
	d[1] = s[1];
	d[2] = s[2];
	d[3] = s[3];
}

int
sprinteth(char *d, uchar *addr)
{
	return sprint(d, "%2.2ux:%2.2ux:%2.2ux:%2.2ux:%2.2ux:%2.2ux", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

const char*
readeth(const char *s, uchar *addr)
{
	int i;
	--s;
	for(i=0; i<Eaddrlen; i++)
		*addr++ = strtol(++s, &s, 16);
	return s;
}

int
eth_probe(void)
{
	int n;
	static uchar broadcast[Eaddrlen] = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	for(n=0; cards[n].type; n++) {
		if(cards[n].reset(&eth0))
			continue;
		memmove(eth0.bcast, broadcast, Eaddrlen);
		memmove(arptable[ARP_CLIENT].node, eth0.ea, Eaddrlen);
		return 1;
	}
	return 0;
}


int
eth_transmit(uchar *dest, ushort t, void *d, int len)
{
	int retval;
	Etherpkt *e;

	e = (Etherpkt*)((uchar*)d - ETHERHDRSIZE);
	len += ETHERHDRSIZE;
	e->type[0] = t>>8;
	e->type[1] = t&0xff;
	memcpy(e->s, eth0.ea, Eaddrlen);
	memcpy(e->d, dest, Eaddrlen);

	if(netdebug & NETDBG_ETH_SENDSHOW)
		print("<es:%d>", len);
	if(netdebug & NETDBG_ETH_SENDDUMP) {
		char cmd[80];
		print("eth xmit: len=%ud type=0x%ux\n", len, t);
		sprint(cmd, "e @%ux,%ux", (ulong)d, len);
		system(cmd);
	}

	retval = eth0.transmit((uchar*)e, len);

	/* Wait a bit so polling has a chance */
	microdelay(post_xmit_delay);

	return retval;
}


struct mbuf*
eth_poll(ushort dt)
{
	int     len;
	ushort type;
	Etherpkt *e;
	static struct mbuf packetbuf;
	struct mbuf *mb;
	uchar *d;

	mb = &packetbuf;	
	mb->m_data = d = mb->m_buffer;
	mb->m_len = len = eth0.poll(d);

	if (len == 0) return 0; /* timeout, no packet */

	e = (Etherpkt*)d;
	mb->m_data += ETHERHDRSIZE;
	mb->m_len -= ETHERHDRSIZE;
	type = (e->type[0] << 8) | e->type[1];

	if(netdebug & NETDBG_ETH_RECVSHOW)
		print("<er:%d>", len);
	if(netdebug & NETDBG_ETH_RECVDUMP) {
		char cmd[80];
		sprinteth(cmd, e->d);
		print("eth recv: len=%d type=0x%ux src=%s\n", len, type, cmd);
		sprint(cmd, "e @%ux,%ux", (ulong)d, (ulong)len);
		system(cmd);
	}

	if (type == ETHERTYPE_ARP) {
		struct ether_arp *arpreq;
		in_addr reqip;

		arpreq = mtod(mb, struct ether_arp*);

		reqip = nhgetl(arpreq->arp_tpa);
		if ((nhgets(arpreq->arp_op) == ARPOP_REQUEST) && (reqip == arptable[ARP_CLIENT].ipaddr)) {
			hnputs(arpreq->arp_op, ARPOP_REPLY);
			memcpy4(arpreq->arp_tpa, arpreq->arp_spa);
			memcpy(arpreq->arp_tha, arpreq->arp_sha, Eaddrlen);
			memcpy(arpreq->arp_sha, arptable[ARP_CLIENT].node, Eaddrlen);
			hnputl(arpreq->arp_spa, reqip);
			eth_transmit(arpreq->arp_tha, ETHERTYPE_ARP,
						mb->m_data, mb->m_len);
			return 0;
		}
	}

	if(type != dt)
		return 0;

	return mb;
}


int arp_resolve(int arpentry)
{
	struct ether_arp *arpreq;
	in_addr	tpa;
	int 	retry = MAX_ARP_RETRIES;
	uchar	pkt[ETHERHDRSIZE + sizeof(struct ether_arp)];

	tpa = arptable[arpentry].ipaddr;
	arpreq = (struct ether_arp*)&pkt[ETHERHDRSIZE];
	hnputs(arpreq->arp_hrd, ARPHRD_ETHER);
	hnputs(arpreq->arp_pro, ETHERTYPE_IP);
	arpreq->arp_hln = Eaddrlen;
	arpreq->arp_pln = 4;
	hnputs(arpreq->arp_op, ARPOP_REQUEST);
	memcpy(arpreq->arp_sha, arptable[ARP_CLIENT].node, Eaddrlen);
	hnputl(arpreq->arp_spa, arptable[ARP_CLIENT].ipaddr);
	memset(arpreq->arp_tha, 0, Eaddrlen);
	hnputl(arpreq->arp_tpa, tpa);

	while (retry--) {
		ulong	time;
		struct ether_arp	*arpreply;

		eth_transmit(eth0.bcast, ETHERTYPE_ARP, arpreq, sizeof(struct ether_arp));

		for (time = 0; time < arptimeout*2; ++time) {
			struct mbuf *mb;
			if ((mb = eth_poll(ETHERTYPE_ARP))) {
				if (mb->m_len >= sizeof(struct ether_arp)) {
					arpreply = mtod(mb, struct ether_arp *);
					if ((nhgets(arpreply->arp_op) == ARPOP_REPLY) && nhgetl(arpreply->arp_spa) == tpa) {
						memcpy(arptable[arpentry].node, arpreply->arp_sha,  Eaddrlen);
						return 1;
					}
					continue;
				}

			}
			microdelay(500);
		}
	}
	return 0;
}


/*
#define hnputs(p,n)	{ ushort v=n; (p)[0]=v>>8; (p)[1]=v; }
#define hnputl(p,n)	{ ulong v=n; (p)[0]=v>>24; (p)[1]=v>>16; (p)[2]=v>>8; (p)[3]=v; }
#define nhgets(p)	(((p)[0]<<8)|(p)[1])
#define nhgetl(p)	(((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|((p)[3]))
*/


/*
ushort
nhgets(void *p)
{
	uchar *a = p;
	return (a[0]<<8)|a[1];
}
*/

ulong
nhgetl(void *p)
{
	uchar *a = p;
	return (a[0]<<24)|(a[1]<<16)|(a[2]<<8)|a[3];
}

/*
void
hnputl(void *p, ulong v)
{
	uchar *a = p;
	a[0] = v>>24;
	a[1] = v>>16;
	a[2] = v>>8;
	a[3] = v;
}
*/
