#include	<lib9.h>
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"etherstream.h"
#include	"etherif.h"
#include	"netboot.h"

struct arptable_t arptable[MAX_ARP];
int post_xmit_delay = 500;

char 	hostname[MAXHOSTNAMELEN];
char	servname[MAXHOSTNAMELEN];
int	hostnamelen;
in_addr netmask;

int arptimeout = ARP_TIMEOUT;
int tftptimeout = TFTP_TIMEOUT;
int bootptimeout = BOOTP_TIMEOUT;

int	pcmcia_initialized = 0;
int	ether_initialized = 0;


int	netdebug;


static const char*
gethostname(void) { return hostname; }

static void
sethostname(const char *s) { strncpy(hostname, s, sizeof(hostname)); }

static const char*
getservname(void) { return servname; }

static void
setservname(const char *s) { strncpy(servname, s, sizeof(servname)); }

static int getnetdebug(void) { return netdebug; }
void setnetdebug(int n) { netdebug = n; }
static int getarptimeout(void) { return arptimeout; }
void setarptimeout(int n) { arptimeout = n; }
static int gettftptimeout(void) { return tftptimeout; }
void settftptimeout(int n) { tftptimeout = n; }
static int getbootptimeout(void) { return bootptimeout; }
void setbootptimeout(int n) { bootptimeout = n; }

#define GETSETIPADDR(get, set, ip) \
static const char* \
get(void) \
{ \
	static char buf[16]; \
	sprintip(buf, ip); \
	return buf; \
} \
static void \
set(const char *s) \
{ \
	readip(s, &ip); \
}

GETSETIPADDR(getnetmask, setnetmask, netmask)
GETSETIPADDR(gethostaddr, sethostaddr, arptable[ARP_CLIENT].ipaddr)
GETSETIPADDR(getservaddr, setservaddr, arptable[ARP_SERVER].ipaddr)
GETSETIPADDR(getgateway, setgateway, arptable[ARP_GATEWAY].ipaddr)

static const char* 
gethwaddr(void) 
{ 
	static char buf[20]; 
	sprinteth(buf, eth0.ea); 
	return buf; 
}

static void 
sethwaddr(const char *s) 
{ 
	readeth(s, eth0.ea); 
}


static int
start_pcmcia(void)
{
	pcmcia_initialized = 0;
	if(!pcmprobe()) {
		error("PCMCIA controller not found");
		return -1;
	} else if(!pcmattach()) {
		error("PCMCIA slots didn't initialize");
		return -1;
	}
	pcmcia_initialized = 1;
	return 0;
}

static int
start_ether(void)
{
	const char *e_hostname = getenv("hostname");
	const char *e_hostaddr = getenv("hostaddr");
	const char *e_servname = getenv("servname");
	const char *e_servaddr = getenv("servaddr");
	const char *e_gateway = getenv("gateway");
	const char *e_netmask = getenv("netmask");
	ether_initialized = 0;
	if(!eth_probe()) {
		error("ether device not found");
		return -1;
	}
	sbindenv("hostaddr", gethostaddr, sethostaddr);
	sbindenv("hostname", gethostname, sethostname);
	sbindenv("netmask", getnetmask, setnetmask);
	sbindenv("servname", getservname, setservname);
	sbindenv("servaddr", getservaddr, setservaddr);
	sbindenv("gateway", getgateway, setgateway);
	sbindenv("eth0addr", gethwaddr, sethwaddr);

	putenv("hostname", e_hostname ? e_hostname : "");
	putenv("hostaddr", e_hostaddr ? e_hostaddr : "0.0.0.0");
	putenv("servname", e_servname ? e_servname : "");
	putenv("servaddr", e_servaddr ? e_servaddr : "0.0.0.0");
	putenv("netmask", e_netmask ? e_netmask : "255.255.255.0");
	putenv("gateway", e_gateway ? e_gateway : "0.0.0.0");

	ether_initialized = 1;
	return 0;
}


static int
eth_read(Istream *_s, void *adr, int len)
{
	EtherIstream *s = (EtherIstream*)_s;
	if(len > s->size-s->pos)
		len = s->size-s->pos;
	if(len > 0) {
		memmove(adr, &s->buf[s->pos], len);
		s->pos += len;
	}
	return len;
}


static int
eth_close(Istream *_s)
{
	EtherIstream *s = (EtherIstream*)_s;
	free(s->buf);
	return 0;
}

static uchar*
eth_mmap(Istream *_s)
{
	EtherIstream *s = (EtherIstream*)_s;
	return s->buf;
}

int
ether_openi(EtherIstream *s, int unit, const char *fname)
{
	char bootpfile[256];
	char *c;
	int len;

	if(unit != 0) {
		error("invalid device");
		return -1;
	}
	if(!pcmcia_initialized && start_pcmcia() < 0)
		return -1; 
	if(!ether_initialized && start_ether() < 0)
		return -1;
	if(arptable[ARP_CLIENT].ipaddr == 0 || arptable[ARP_SERVER].ipaddr == 0
	   		|| !fname[0]) {
		if(bootp(bootpfile, sizeof(bootpfile)) < 0)
			return -1;
		if(!fname[0])
			putenv("bootpfile", (fname = bootpfile));
	}
	fname = strcpy(bootpfile, fname);
	if((c = strchr(fname, '#'))) {
		s->maxlen = strtol(&c[1], 0, 0);
		*c = '\0';
	} else	
		s->maxlen = 2*_M_;
	s->buf = (uchar*)malloc(s->maxlen);
	
	if((len = tftp(fname, s->buf, s->maxlen)) < 0) {
		free(s->buf);
		return -1;
	}
	s->size = len;
	s->read = eth_read;
	s->close = eth_close;
	s->mmap = eth_mmap;
	s->pos = 0;
	return 0;
}


static Istream*
ether_sd_openi(const char *args)
{
	EtherIstream *s = (EtherIstream*)malloc(sizeof(EtherIstream));
	const char *c = strchr(args, '!');
	const char *fname = args;
	int unit = 0;

	if(!c && args[0] >= '0' && args[0] <= '9')
		c = "!";
	if(c) {
		fname = &c[1];
		unit = strtol(args, 0, 0);
	}		
	if(!*args || ether_openi(s, unit, fname) < 0) {
		free(s);
		return nil;
	}
	return s;
}

static StreamDev ether_sd = {
	"e",
	ether_sd_openi,
	0,
};



int
cmd_netctl(int, char **argv, int *)
{
	if(strstr(argv[0], "/P") && start_pcmcia() < 0) 
		return -1;
	if(strstr(argv[0], "/e") && start_ether() < 0) 
		return -1;
	if(strstr(argv[0], "/b")) {
		char buf[256];
		int r = bootp(buf, sizeof(buf));
		print("%d: bootpfile='%s'\n", r, buf);
	}
	return 0;
}

void
etherstreamlink(void)
{
	addstreamdevlink(&ether_sd);
	addcmd('N', cmd_netctl, 0, 0, "netctl");
	nbindenv("netdebug", getnetdebug, setnetdebug);
	nbindenv("arptimeout", getarptimeout, setarptimeout);
	nbindenv("bootptimeout", getbootptimeout, setbootptimeout);
	nbindenv("tftptimeout", gettftptimeout, settftptimeout);
}

