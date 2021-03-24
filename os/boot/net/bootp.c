#include <lib9.h>
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "etherif.h"
#include "netboot.h"
#include "udp.h"


extern int bootptimeout;

static void
printipaddr(const char *s, in_addr addr)
{
	char buf[4*4];
	sprintip(buf, addr);
	print("  %s: %s\n", s, buf);
}

static void
decode_rfc1048(uchar *p)
{
	static const char rfc1048_cookie[4] = RFC1048_COOKIE;
	uchar *end = p + BOOTP_VENDOR_LEN, *q;
	if (!memcmp(p, rfc1048_cookie, 4)) { /* RFC 1048 header */
		if(netdebug & NETDBG_BOOTP_INFO)
			print("  vend: rfc1048\n");
		p += 4;
		while (p < end) {
			switch (*p) {
			case RFC1048_PAD:
				p++;
				continue;
			case RFC1048_END:
				p = end;
				continue;
			case RFC1048_GATEWAY:
				arptable[ARP_GATEWAY].ipaddr = nhgetl(p+2);
				if(netdebug & NETDBG_BOOTP_INFO)
					printipaddr("gateway", arptable[ARP_GATEWAY].ipaddr);
				break;
			case RFC1048_NETMASK:
				netmask = nhgetl(p+2);
				if(netdebug & NETDBG_BOOTP_INFO)
					printipaddr("netmask", netmask);
				break;
			case RFC1048_HOSTNAME:
				hostnamelen = TAG_LEN(p);
				memcpy(hostname, p+2, hostnamelen);
				hostname[hostnamelen] = 0;
				if(netdebug & NETDBG_BOOTP_INFO)
					print("  hostname: %s\n", hostname);
				break;
			default:
				if(netdebug & NETDBG_BOOTP_INFO) {
					print("  Unknown RFC1048-tag ");
					for (q=p; q<p+2+TAG_LEN(p); q++)
						print("%x ",*q);
					print("\n");
				}
			}
			p += TAG_LEN(p) + 2;
		}
	}
}

const char*
readip(const char *s, in_addr *addr)
{
	ulong a = 0;
	int i;
	for(i=0; i<4; i++) {
		a = (a<<8)|strtol(s, &s, 0);
		++s;
	}
	*addr = a;
	return s;
}


static void
decode_p9bootp(const char *p)
{
	static const char p9bootp_cookie[4] = P9BOOTP_COOKIE;
	if (!memcmp(p, p9bootp_cookie, 4)) { /* Plan9-style header */
		in_addr auth;
		if(netdebug & NETDBG_BOOTP_INFO)
			print("  vend: plan9\n");
		p += 4;
		p = readip(p, &netmask);
		p = readip(p, &arptable[ARP_SERVER].ipaddr);
		p = readip(p, &auth);
		p = readip(p, &arptable[ARP_GATEWAY].ipaddr);
		printipaddr("netmask", netmask);
		printipaddr("server", arptable[ARP_SERVER].ipaddr);
		printipaddr("auth", auth);
		printipaddr("gateway", arptable[ARP_GATEWAY].ipaddr);
	}
}

static int
bootp_input(void *vn, struct mbuf *m)
{
	struct	bootp_t *bootpreply;
	char	*name = (char*)vn;

	bootpreply = mtod(m, struct bootp_t *);
	if(netdebug & NETDBG_BOOTP_STATUS)
		print("<bootp:recv:%d:0x%ux>", m->m_len, bootpreply->bp_op);
	if ((m->m_len >= sizeof(struct bootp_t)) &&  (bootpreply->bp_op == BOOTP_REPLY)) {
		arptable[ARP_CLIENT].ipaddr = nhgetl(bootpreply->bp_yiaddr);
		netmask = default_netmask(arptable[ARP_CLIENT].ipaddr);
		arptable[ARP_SERVER].ipaddr = nhgetl(bootpreply->bp_siaddr);
		memset(arptable[ARP_SERVER].node, 0, Eaddrlen);  /* Kill arp */
		arptable[ARP_GATEWAY].ipaddr = nhgetl(bootpreply->bp_giaddr);
		memset(arptable[ARP_GATEWAY].node, 0, Eaddrlen);  /* Kill arp */
		strncpy(servname, bootpreply->bp_sname, MAXHOSTNAMELEN);
		strncpy(name, bootpreply->bp_file, *(ushort*)vn);
		if(netdebug & NETDBG_BOOTP_INFO) {
			printipaddr("client", arptable[ARP_CLIENT].ipaddr);
			printipaddr("server", arptable[ARP_SERVER].ipaddr);
			printipaddr("gateway", arptable[ARP_GATEWAY].ipaddr);
			if(name[0])
				print("  bootfile: '%s'\n", name);
		}
		decode_rfc1048((uchar *)bootpreply->bp_vend);
		decode_p9bootp((char *)bootpreply->bp_vend);
		return 1;
	}
	return 0;
}

int
bootp(char *namebuf, int maxlen)
{
	int retry = MAX_BOOTP_RETRIES;
	struct bootp_t *bp;
	ulong  starttime;
	uchar pkt[UDPHDRSIZE+sizeof(struct bootp_t)];

	memset(pkt, 0, sizeof pkt);
	bp = (struct bootp_t*)&pkt[UDPHDRSIZE];
	bp->bp_op = BOOTP_REQUEST;
	bp->bp_htype = 1;
	bp->bp_hlen = Eaddrlen;
	hnputl(bp->bp_xid, (starttime = timer_start()));
	memcpy(bp->bp_hwaddr, arptable[ARP_CLIENT].node, Eaddrlen);
	*(ushort*)namebuf = maxlen;
	while (retry--) {
		status("bootp", retry, MAX_BOOTP_RETRIES);
		if(interrupt()) {
			error("bootp aborted");
			return -1;
		}
		if(netdebug & NETDBG_BOOTP_STATUS)
			print("<bootp:xmit:%d>", sizeof(struct bootp_t));
		udp_transmit(INADDR_BCAST, BOOTP_CLIENT, BOOTP_SERVER, bp, sizeof(struct bootp_t));
		if (udp_receive(BOOTP_CLIENT, bootp_input, namebuf, bootptimeout))
			return 0;
		hnputs(bp->bp_secs, tmr2ms(timer_ticks(starttime))/1000);
		delay(1000);
	}
	error("bootp failed");
	return -1;
}

