
enum {
	MAXHOSTNAMELEN=		80,

};


enum {
	MAX_TFTP_RETRIES=	100,
	MAX_BOOTP_RETRIES=	100,
	MAX_ARP_RETRIES=	50,
	MAX_RPC_RETRIES=	50,

	/* defaults in msec: */
	BOOTP_TIMEOUT=		500,
	ARP_TIMEOUT=		100,
	TFTP_TIMEOUT=		250,

};


/* use BSD nomenclature for TCP/IP stack */


enum {
	ETHERTYPE_IP=   	0x0800,
	ETHERTYPE_ARP=   	0x0806,

};


/* ARP packet structure (cf arp.h) specialized for IP and Ethernet (ether.h) */

struct ether_arp {
	uchar  arp_hrd[2];           /* hardware address type (ARPHRD_ETHER) */
	uchar  arp_pro[2];           /* protocol address type (ETHERTYPE_IP) */
	uchar  arp_hln;              /* hardware address length (6) */
	uchar  arp_pln;              /* protocol address length (4) */
	uchar  arp_op[2];
	uchar  arp_sha[6];           /* sender hardware address */
	uchar  arp_spa[4];           /* sender protocol address */
	uchar  arp_tha[6];           /* target hardware address */
	uchar  arp_tpa[4];           /* target protocol address */
};

/* operations */

#define ARPOP_REQUEST  1
#define ARPOP_REPLY    2

/* hardware types */

#define ARPHRD_ETHER   1
#define ARPHRD_802     6


/* IP header structure (ip.h) */

/* N.B. - this is a bit unconventional, but this address is 
 * stored in local host order, so the lowest 8 bits represents
 * the last byte of the network address
 */
typedef ulong in_addr;

#define INADDR_BCAST 0xffffffff  /* limited broadcast */

struct ip {
	uchar    ip_vhl;            /* version and header length */
	uchar    ip_tos;            /* type of service */
	uchar    ip_len[2];         /* total length */
	uchar    ip_id[2];          /* identification */
	uchar    ip_off[2];         /* fragment offset field */
	uchar    ip_ttl;            /* time to live */
	uchar    ip_p;              /* protocol */
	uchar    ip_sum[2];         /* checksum */
	uchar    ip_src[4], ip_dst[4];    /* source and dest address */
};

enum {
	IPPROTO_UDP=    17,

};

/* UDP header structure (udp.h) */

struct udphdr {
	uchar uh_sport[2];               /* source port */
	uchar uh_dport[2];               /* destination port */
	uchar uh_ulen[2];                /* udp message length (with header) */
	uchar uh_sum[2];                 /* check sum */
};



/* BOOTP messages (RFC 951) */

enum {
	BOOTP_SERVER=  67,
	BOOTP_CLIENT=  68,

};

struct bootp_t {
	uchar  bp_op;
	uchar  bp_htype;
	uchar  bp_hlen;
	uchar  bp_hops;
	uchar  bp_xid[4];
	uchar  bp_secs[2];
	uchar  bp_flags[2];
	uchar  bp_ciaddr[4];
	uchar  bp_yiaddr[4];
	uchar  bp_siaddr[4];
	uchar  bp_giaddr[4];
	uchar  bp_hwaddr[16];
	char   bp_sname[64];
	char   bp_file[128];
	uchar  bp_vend[128];
};

enum {
	BOOTP_REQUEST=	1,
	BOOTP_REPLY=	2,

	BOOTP_VENDOR_LEN=	64,

	RFC1048_PAD=		0,
	RFC1048_NETMASK=	1,
	RFC1048_GATEWAY=	3,
	RFC1048_HOSTNAME=	12,
	RFC1048_END=		255,

};

#define TAG_LEN(p)		(*((p)+1))
#define	RFC1048_COOKIE		{ 99, 130, 83, 99 }
#define P9BOOTP_COOKIE		{ 'p', '9', 32, 32 }

/* TFTP messages (RFC 1350) */

enum {
	TFTP_PORT=	69,

};


struct tftp_t {
	union {
		struct {
			uchar opcode[2];
			char rrq[514];
		};
		struct {
			uchar _opcode[2];
			uchar block[2];
			char download[512];
		} data;
		struct {
			uchar _opcode[2];
			uchar block[2];
		} ack;
		struct {
			uchar _opcode[2];
			uchar errcode[2];
			char errmsg[512];
		} err;
	};
};

enum {
	TFTP_MIN_MSG_SIZE=  4,
	TFTP_ACK_MSG_SIZE=  4,

	TFTP_RRQ=	1,
	TFTP_WRQ=	2,
	TFTP_DATA=	3,
	TFTP_ACK=	4,
	TFTP_ERROR=	5,

};


/* ---------------- local data structures ---------------- */

/* simple mbuf structure, suitable for full Ethernet packets */

struct mbuf {
	ulong   m_len;                             /* length of data */
	uchar  *m_data;                            /* location of data */
	uchar   m_buffer[ETHERMAXTU];   	   /* data area appended */
};

#define mtod(m,t) ((t)((m)->m_data))


#define UDPHDRSIZE	(ETHERHDRSIZE+sizeof(struct ip)+sizeof(struct udphdr))


/* bootstrap ARP table format */

struct arptable_t {
	in_addr ipaddr;
	uchar  node[6];
};

/* well-known entries for the bootstrap ARP table */
enum {
	ARP_CLIENT=	0,
	ARP_SERVER=	1,
	ARP_GATEWAY=	2,
	ARP_NS=		3,
	ARP_ROOTSERVER=	4,
	ARP_SWAPSERVER=	5,
	MAX_ARP=	ARP_SWAPSERVER+1,

};


/* NOTE: not all of these flags are currently implemented
 * (some are just placeholders for possible debugging)
 * check the source before using any of them
 */
enum {
	NETDBG_ETH_RECVSHOW = 0x00000001,
	NETDBG_ETH_SENDSHOW = 0x00000002,
	NETDBG_ETH_RECVDUMP = 0x00000010,
	NETDBG_ETH_SENDDUMP = 0x00000020,
	NETDBG_ETH_INIT	    = 0x00000080,
	NETDBG_UDP_RECVSHOW = 0x00000100,
	NETDBG_UDP_SENDSHOW = 0x00000200,
	NETDBG_UDP_RECVDUMP = 0x00001000,
	NETDBG_UDP_SENDDUMP = 0x00002000,
	NETDBG_BOOTP_STATUS = 0x00010000,
	NETDBG_BOOTP_INFO   = 0x00020000,
	NETDBG_TFTP_STATUS  = 0x00100000,
	NETDBG_TFTP_INFO    = 0x00200000,
	NETDBG_PCMCIA_INIT  = 0x08000000,
	NETDBG_PCMCIA_STATUS= 0x01000000,

};

extern int netdebug;

