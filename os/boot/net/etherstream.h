typedef struct EtherIstream {	
	Istream;
	int	fd;
	long	lastpos;
	uchar	*buf;
	int	maxlen;
} EtherIstream;


int ether_openi(EtherIstream *, int unit, const char *);
