typedef struct MemIstream {	
	Istream;
	const uchar *base;
} MemIstream;

typedef struct MemOstream {
	Ostream;
	uchar	*base;
} MemOstream;

int mem_openi(MemIstream *, const void *, ulong);
int mem_openo(MemOstream *, void *, ulong);

