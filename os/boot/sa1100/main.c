#include	<lib9.h>
#include	"dat.h"
#include	"fns.h"
#include	"mem.h"
#include	"../port/error.h"
#include	"bpi.h"

uchar SBOOT_VER_MAJ = 2;
uchar SBOOT_VER_MIN = 3;

// extern uchar confvermaj, confvermin;
extern const char *confverid;
extern const char *conffile;
extern int maxnarg;
extern int maxnenv;
extern int argbufsize;
extern int envbufsize;

char **argvbase;
char *argbuf;

extern int doautoboot;

extern char end[];
extern char edata[];

Conf conf;

void
print_title(void)
{
	print("\nsboot %d.%d-%s (%s)\n - Inferno BootStrap Loader\n\n",
		SBOOT_VER_MAJ, SBOOT_VER_MIN, confverid, conffile);
}

void
confinit(void)
{
	archconfinit();
	bootconfinit();
}

void
main(void)
{
	extern int cmdlock;
	ulong envpbase;
	ulong envdbase;
	int debug = (bpi->flags & BP_FLAG_DEBUG != 0);

	memset(edata, 0, end-edata);
	mallocinit((ulong)end, bpi->himem-64*_K_);
	if(!debug)
		stdout = nil;

	archreset();
	confinit();
	links();

	if(debug)
		printbootinfo();

	envpbase = bpi->himem - maxnenv*sizeof(char*);
	envdbase = bpi->himem - envbufsize;
	envinit((char**)envpbase, bpi->himem-envpbase,
		(char*)envdbase, envpbase-envdbase);
	argvbase = (char**)(envdbase - maxnarg*sizeof(char*));
	argbuf = (char*)(envdbase - argbufsize);

	mmuctlregw(mmuctlregr() | CpCDcache | CpCwb | CpCIcache);

	if(debug) {
		print("debug mode\n");
		soflush(stdout);
		stdout = dbgout;
		stdin = dbgin;
		print_title();
		cmdlock=1;
		system(". F!plan9.ini");
		cmdlock=0;
		ioloop();
	} else {
		system("A");		/* autoboot */
		doautoboot=0;
		autoboot();
	}
	print("<exit>\n");
}

void
halt(void)
{
	while(1)
		;
}

