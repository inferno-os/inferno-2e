typedef struct BpiIstream {	
	Istream;
	int	fd;
	long	lastpos;
} BpiIstream;

typedef struct BpiOstream {
	Ostream;
	int	fd;
	long	lastpos;
} BpiOstream;

int bpi_openi(BpiIstream *, const char *);
int bpi_openo(BpiOstream *, const char *);

