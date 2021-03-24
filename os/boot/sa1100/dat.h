typedef struct TouchPnt TouchPnt;
typedef struct TouchTrans TouchTrans;
typedef struct TouchCal TouchCal;
typedef struct Vmode Vmode;
typedef struct Conf Conf;
typedef struct ISAConf ISAConf;

struct Conf
{
	ulong	pagetable;
	// ulong	flashbase;
	ulong	cpuspeed;
};

enum {
	ISAOPTLEN=16,
	NISAOPT=8,
};

struct ISAConf {
	char	type[28];
	ulong	port;
	ulong	irq;
	ulong	sairq;
	ulong	dma;
	ulong	mem;
	ulong	size;
	ulong	freq;
	
	int	nopt;
	char	opt[NISAOPT][ISAOPTLEN];
};


#include "../port/portdat.h"

enum {
	/* argument to pcmpin() */
	PCMready,
	PCMeject,
	PCMstschng,
};

