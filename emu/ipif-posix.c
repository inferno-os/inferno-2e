#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/socket.h>
#include	<net/if.h>
#include	<net/if_arp.h>
#include	<netinet/in.h>
#include	<netinet/tcp.h>
#include	<netdb.h>
#include        "dat.h"
#include        "fns.h"
#include        "ip.h"
#include        "error.h"
#include	<sys/ioctl.h>

int
so_socket(int type)
{
	int fd, one;
	char err[ERRLEN];

	switch(type) {
	default:
		error("bad protocol type");
	case S_TCP:
		type = SOCK_STREAM;
		break;
	case S_UDP:
		type = SOCK_DGRAM;
		break;
	}

	fd = socket(AF_INET, type, 0);
	if(fd < 0) {
		oserrstr(err);
		error(err);
	}
	else if(type == SOCK_DGRAM){
		one = 1;
		setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char*)&one, sizeof (one));
	}
	return fd;
}

int
so_send(int sock, void *va, int len, void *hdr, int hdrlen)
{
	int r;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	char *h = hdr;

	sin = (struct sockaddr_in*)&sa;

	osenter();
	if(hdr == 0)
		r = write(sock, va, len);
	else {
		sin->sin_family = AF_INET;
		/* tempting, but incorrect...
		memmove(&sin->sin_addr, h+4,  4);
		memmove(&sin->sin_port, h+10, 2);
		bind(sock, &sa, sizeof(sa));	*/
		memmove(&sin->sin_addr, h,  4);
		memmove(&sin->sin_port, h+8, 2);
		r = sendto(sock, va, len, 0, &sa, sizeof(sa));
	}
	osleave();

	/* if nonblocked mode is set, return 0 instead of -1 */
	if ((r == -1) && (errno == EWOULDBLOCK))
		r = 0;

	return r;
}

int
so_recv(int sock, void *va, int len, void *hdr, int hdrlen)
{
	int r, l;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	char h[12];

	sin = (struct sockaddr_in*)&sa;

	osenter();
	if(hdr == 0)
		r = read(sock, va, len);
	else {
		l = sizeof(sa);
		r = recvfrom(sock, va, len, 0, &sa, &l);
		if(r >= 0) {
			memmove(h, &sin->sin_addr, 4);
			memmove(h+8, &sin->sin_port, 2);

			/* alas there's no way to get the local addr/port correctly.  Pretend. */
			getsockname(sock, &sa, &l);
			memmove(h+4, &sin->sin_addr, 4);
			memmove(h+10, &sin->sin_port, 2);
			if(hdrlen > 12)
				hdrlen = 12;
			memmove(hdr, h, hdrlen);
		}
	}
	osleave();

	/* if nonblocked mode is set, return 0 instead of -1 */
	if ((r == -1) && (errno == EWOULDBLOCK || errno == EAGAIN))
		r = 0;

	return r;
}

void
so_close(int sock)
{
	close(sock);
}

void
so_connect(int fd, unsigned long raddr, unsigned short rport)
{
	int r;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	char err[ERRLEN];

	sin = (struct sockaddr_in*)&sa;

	memset(&sa, 0, sizeof(sa));
	sin->sin_family = AF_INET;
	hnputs(&sin->sin_port, rport);
	hnputl(&sin->sin_addr.s_addr, raddr);

	osenter();
	r = connect(fd, &sa, sizeof(sa));
	osleave();
	if(r < 0) {
		oserrstr(err);
		error(err);
	}
}

void
so_getsockname(int fd, unsigned long *laddr, unsigned short *lport)
{
	int len;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	char err[ERRLEN];

	sin = (struct sockaddr_in*)&sa;

	len = sizeof(sa);
	if(getsockname(fd, &sa, &len) < 0) {
		oserrstr(err);
		error(err);
	}

	if(sin->sin_family != AF_INET || len != sizeof(*sin))
		error("not AF_INET");

	*laddr = nhgetl(&sin->sin_addr.s_addr);
	*lport = nhgets(&sin->sin_port);
}

void
so_listen(int fd)
{
	int r;
	char err[ERRLEN];

	osenter();
	r = listen(fd, 5);
	osleave();
	if(r < 0) {
		oserrstr(err);
		error(err);
	}
}

int
so_accept(int fd, unsigned long *raddr, unsigned short *rport)
{
	int nfd, len;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	char err[ERRLEN];

	sin = (struct sockaddr_in*)&sa;

	len = sizeof(sa);
	osenter();
	nfd = accept(fd, &sa, &len);
	osleave();
	if(nfd < 0) {
		oserrstr(err);
		error(err);
	}

	if(sin->sin_family != AF_INET || len != sizeof(*sin))
		error("not AF_INET");

	*raddr = nhgetl(&sin->sin_addr.s_addr);
	*rport = nhgets(&sin->sin_port);
	return nfd;
}

void
so_bind(int fd, int su, unsigned short port)
{
	int i, one;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	char err[ERRLEN];

	sin = (struct sockaddr_in*)&sa;

	one = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) < 0) {
		oserrstr(err);
		print("setsockopt: %s", err);
	}

	if(su) {
		for(i = 600; i < 1024; i++) {
			memset(&sa, 0, sizeof(sa));
			sin->sin_family = AF_INET;
			hnputs(&sin->sin_port, i);

			if(bind(fd, &sa, sizeof(sa)) >= 0)	
				return;
		}
		oserrstr(err);
		error(err);
	}

	memset(&sa, 0, sizeof(sa));
	sin->sin_family = AF_INET;
	hnputs(&sin->sin_port, port);

	if(bind(fd, &sa, sizeof(sa)) < 0) {
		oserrstr(err);
		error(err);
	}
}

void
so_poll(int fd, long timeout)
{
	int n;
	fd_set tbl;
	struct timeval t;
	char err[ERRLEN];

	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;
	FD_ZERO(&tbl);
	FD_SET(fd, &tbl);

	osenter();
	n = select(fd+1, &tbl, NULL, NULL, &t);
	osleave();

	if (n < 0) {
		oserrstr(err);
		error(err);
	}
	if (n == 0) 
		error("timed-out");
}

void
so_setsockopt(int fd, int opt, int value)
{
	int r;
	struct linger l;
	char err[ERRLEN];

	if (opt == SO_LINGER) {
		l.l_onoff = 1;
		l.l_linger = (short) value;
		osenter();
		r = setsockopt(fd, SOL_SOCKET, opt, (char *)&l, sizeof(l));
		osleave();
	}
	else if (opt == TCP_NODELAY) {
		osenter();
		r = setsockopt(fd, IPPROTO_TCP, opt, (char *)&value, sizeof(value));
		osleave();
	}
	else
		error(Ebadctl);
	if (r < 0) {
		oserrstr(err);
		error(err);
	}
}

int
so_gethostbyname(char *host, char**hostv, int n)
{
	int i;
	unsigned char buf[32], *p;
	struct hostent *hp;

	hp = gethostbyname(host);
	if(hp == 0)
		return 0;

	for(i = 0; hp->h_addr_list[i] && i < n; i++) {
		p = hp->h_addr_list[i];
		sprint(buf, "%ud.%ud.%ud.%ud", p[0], p[1], p[2], p[3]);
		hostv[i] = strdup(buf);
		if(hostv[i] == 0)
			break;
	}
	return i;
}

int
so_gethostbyaddr(char *addr, char **hostv, int n)
{
	int i;
	struct hostent *hp;
	unsigned long straddr;

	straddr = inet_addr(addr);
	if ( (int) straddr == -1 )
		return 0;

	hp = gethostbyaddr((char *)&straddr, sizeof (straddr), AF_INET);
	if ( hp == 0 )
		return 0;

	hostv[0] = strdup(hp->h_name);
	if ( hostv[0] == 0 )
		return 0;
	for ( i = 1; hp->h_aliases[i-1] && i < n; i++ ) {
		hostv[i] = strdup(hp->h_aliases[i-1]);
		if ( hostv[i] == 0 )
			break;
	}
	return i;
}

int
so_getservbyname(char *service, char *net, char *port)
{
	ushort p;
	struct servent *s;

	s = getservbyname(service, net);
	if(s == 0)
		return -1;
	p = (ushort) s->s_port;
	sprint(port, "%d", nhgets(&p));	
	return 0;
}

int
so_hangup(int fd, int linger)
{
	int r;
	static struct linger l = {1, 1000};

	osenter();
	if (linger)
		setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l));
	r = shutdown(fd, 2);
	if (r >= 0)
		r = close(fd);
	osleave();
	return r;
}

int
bipipe(int fd[2])
{
	return socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
}

void
hnputl(void *p, unsigned long v)
{
	unsigned char *a;

	a = p;
	a[0] = v>>24;
	a[1] = v>>16;
	a[2] = v>>8;
	a[3] = v;
}

void
hnputs(void *p, unsigned short v)
{
	unsigned char *a;

	a = p;
	a[0] = v>>8;
	a[1] = v;
}

unsigned long
nhgetl(void *p)
{
	unsigned char *a;
	a = p;
	return (a[0]<<24)|(a[1]<<16)|(a[2]<<8)|(a[3]<<0);
}

unsigned short
nhgets(void *p)
{
	unsigned char *a;
	a = p;
	return (a[0]<<8)|(a[1]<<0);
}

#define CLASS(p) ((*(unsigned char*)(p))>>6)

unsigned long
parseip(char *to, char *from)
{
	int i;
	char *p;

	p = from;
	memset(to, 0, 4);
	for(i = 0; i < 4 && *p; i++){
		to[i] = strtoul(p, &p, 0);
		if(*p == '.')
			p++;
	}
	switch(CLASS(to)){
	case 0:	/* class A - 1 byte net */
	case 1:
		if(i == 3){
			to[3] = to[2];
			to[2] = to[1];
			to[1] = 0;
		} else if (i == 2){
			to[3] = to[1];
			to[1] = 0;
		}
		break;
	case 2:	/* class B - 2 byte net */
		if(i == 3){
			to[3] = to[2];
			to[2] = 0;
		}
		break;
	}
	return nhgetl(to);
}

void
so_noblock(int fd, int nb)
{
	int flag;

	flag = fcntl(fd, F_GETFL);
	if (nb)
		fcntl(fd, F_SETFL, flag | O_NONBLOCK);
	else
		fcntl(fd, F_SETFL, flag & ~O_NONBLOCK);
}

void
arpadd(char *ipaddr, char *eaddr, int n)
{
	struct arpreq a;
	static char ebuf[ERRLEN];
	struct sockaddr_in pa;
	int s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&a, 0, sizeof(a));
	memset(&pa, 0, sizeof(pa));
	pa.sin_family = AF_INET;
	pa.sin_port = 0;
	parseip((char*)&pa.sin_addr, ipaddr);
	memmove(&a.arp_pa, &pa, sizeof(pa));
	while(ioctl(s, SIOCGARP, &a) != -1) {
		ioctl(s, SIOCDARP, &a);
		memset(&a.arp_ha, 0, sizeof(a.arp_ha));
	}
	a.arp_ha.sa_family = AF_UNSPEC;
	parseether((uchar*)a.arp_ha.sa_data, eaddr);
	a.arp_flags = ATF_PERM;
	if(ioctl(s, SIOCSARP, &a) == -1) {
		oserrstr(ebuf);
		close(s);
		error(ebuf);
	}
	close(s);
}
