#include <lib9.h>
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "etherif.h"
#include "netboot.h"
#include "udp.h"


int
sprintip(char *d, in_addr a)
{
	return sprint(d, "%3.3d.%3.3d.%3.3d.%3.3d",
		(uchar)(a>>24), (uchar)(a>>16), (uchar)(a>>8), (uchar)a);
}

in_addr
default_netmask(in_addr addr)
{
	int net = addr >> 24;
	if (net <= 127)
		return 0xff000000;
	if (net < 192)
		return 0xffff0000;
	return 0xffffff00;
}


static ushort
ip_chksum(const uchar *data, int len)
{
	ulong sum = 0;
	while(len > 0) {
		sum += data[0]<<8 | data[1];
		len -= 2;
		data += 2;
	}
	sum = (sum&0xffff)+(sum>>16);
	sum += (sum>>16);
	return ~sum;
}


static ushort ip_id = 0;

int
udp_transmit(in_addr destip, ushort srcport, ushort destport, void *d, int len)
{
	struct ip     *ip;
	struct udphdr *udp;
	int arpentry, i;

	udp = (struct udphdr*)((uchar*)d - sizeof(struct udphdr));
	len += sizeof(struct udphdr);
	hnputs(udp->uh_sport, srcport);
	hnputs(udp->uh_dport, destport);
	hnputs(udp->uh_ulen, len);
	
	ip = (struct ip*)((uchar*)udp - sizeof(struct ip));
	len += sizeof(struct ip);
	ip->ip_vhl = 0x45;
	ip->ip_tos = 0;
	hnputs(ip->ip_len, len);
	hnputs(ip->ip_id, ip_id);
	hnputs(ip->ip_off, 0);
	ip->ip_ttl = 60;
	ip->ip_p = IPPROTO_UDP;
	hnputl(ip->ip_src, arptable[ARP_CLIENT].ipaddr);
	hnputl(ip->ip_dst, destip);
	hnputs(ip->ip_sum, 0);
	hnputs(ip->ip_sum, ip_chksum((uchar*)ip, sizeof(struct ip)));
	ip_id++;

	hnputs(udp->uh_sum, 0);

	if (destip == INADDR_BCAST) {
		eth_transmit(eth0.bcast, ETHERTYPE_IP, ip, len);
	} else {
		/* Check to see if we need gateway */
		if (((destip & netmask) != (arptable[ARP_CLIENT].ipaddr & netmask)) &&  arptable[ARP_GATEWAY].ipaddr)
			destip = arptable[ARP_GATEWAY].ipaddr;

		for (arpentry = 0; arpentry<MAX_ARP; arpentry++)
			if (arptable[arpentry].ipaddr == destip) break;
		if (arpentry == MAX_ARP) {
			print("%I not found\n", destip);
			return 0;
		}
		for (i = 0; i<Eaddrlen; i++)
			if (arptable[arpentry].node[i]) break;
		if ((i == Eaddrlen)	/* Need to do arp request */ &&  !arp_resolve(arpentry))
			return 0;
		eth_transmit(arptable[arpentry].node, ETHERTYPE_IP, ip, len);
	}
	return 1;
}


int
udp_receive(ushort port, int (*input)(void*, struct mbuf *), void *inf, int timeout)
{
	ulong time;
	struct	ip *ip;
	struct	udphdr *udp;
	ushort	dport;
	int		ret;
	struct mbuf *mb;

	for (time = 0; time < timeout*2; ++time) {
		if ((mb = eth_poll(ETHERTYPE_IP))) {  /* We have something! */
			ip = mtod(mb, struct ip *);
			mb->m_data += sizeof(struct ip);
			mb->m_len -= sizeof(struct ip);
			if ((ip->ip_vhl != 0x45) || (ip_chksum((uchar*)ip, sizeof(struct ip)) != 0) || (ip->ip_p != IPPROTO_UDP))
				continue;

			udp = mtod(mb, struct udphdr *);
			mb->m_data += sizeof(struct udphdr);
			mb->m_len -= sizeof(struct udphdr);

			dport = nhgets(udp->uh_dport);

			if(dport == port) {
				ret = (*input)(inf, mb);
				if(ret < 0)
					return 0;
				if(ret != 0)
					return 1;
			}
		}
		microdelay(500);
	}
	return 0;
}

