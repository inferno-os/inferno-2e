
extern struct arptable_t arptable[MAX_ARP];
extern char hostname[MAXHOSTNAMELEN];
extern char servname[MAXHOSTNAMELEN];
extern int hostnamelen;
extern in_addr netmask;

extern in_addr default_netmask(in_addr);
extern int udp_transmit(in_addr destip, ushort srcport, ushort destport,
			void *data, int len);
extern int udp_receive(ushort port, int (*input)(void*, struct mbuf *),
			void *inf, int timeout);

