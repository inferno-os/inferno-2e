enum {
	MaxEther= 3,
	Ntypes= 8,
	Eaddrlen= 6,
	ETHERMINTU= 60,
	ETHERMAXTU= 1514,
	ETHERHDRSIZE= 14,
};

typedef struct Etherpkt Etherpkt;
struct Etherpkt {
	uchar d[Eaddrlen];
	uchar s[Eaddrlen];
	uchar type[2];
	uchar data[1500];
};

typedef struct Ether Ether;
struct Ether {
	ISAConf;			/* hardware info */
	uchar	ea[Eaddrlen];
	uchar	bcast[Eaddrlen];
	int	(*transmit)(uchar *, uint);
	int	(*poll)(uchar *);
	void	*ctlr;
	int	pcmslot;		/* PCMCIA */
};

extern void addethercard(char*, int(*)(Ether*));

#define NEXT(x, l)	(((x)+1)%(l))
#define PREV(x, l)	(((x) == 0) ? (l)-1: (x)-1)
#define	HOWMANY(x, y)	(((x)+((y)-1))/(y))
#define ROUNDUP(x, y)	(HOWMANY((x), (y))*(y))


int eth_probe(void);
int eth_reset(void);
int eth_transmit(uchar *dest, ushort type, void *data, int len);
struct mbuf* eth_poll(ushort type);
int arp_resolve(int arpentry);

extern Ether eth0;

#define memcpy2(d, s)	{ (d)[0] = (s)[0]; (d)[1] = (s)[1]; }
void memcpy4(uchar *d, uchar *s);


#define hnputs(p,n)	{ ushort v=n; (p)[0]=v>>8; (p)[1]=v; }
#define hnputl(p,n)	{ ulong v=n; (p)[0]=v>>24; (p)[1]=v>>16; (p)[2]=v>>8; (p)[3]=v; }
// void hnputl(void *p, ulong v);
#define nhgets(p)	(((p)[0]<<8)|(p)[1])
// ushort nhgets(void *p);
//#define nhgetl(p)	(((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|((p)[3]))
ulong nhgetl(void *p);

