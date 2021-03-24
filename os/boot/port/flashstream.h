
typedef struct FlashInfo FlashInfo;
struct FlashInfo {
	FlashMap *m;
	ulong *secbuf;
	ulong last_secbase;
	ulong last_secsize;
};

typedef struct FlashIstream FlashIstream;
struct FlashIstream {	
	Istream;
	FlashInfo *flashinfo;
	ulong	pbase;
};

typedef struct FlashOstream FlashOstream;
struct FlashOstream {
	Ostream;
	FlashInfo *flashinfo;
	ulong	pbase;
};

int flash_openi(FlashIstream *, const char *);
int flash_openo(FlashOstream *, const char *);

