#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "bpi.h"
#include "memstream.h"

extern char **argvbase;
extern char *argbuf;

static void
argcopy(int argc, char **argv)
{
	char **p = argvbase;
	char *bp = argbuf;
	bpi->argc = argc;
	bpi->argv = p;
	while(argc--) {
		*p++ = bp;
		strcpy(bp, *argv++);
		bp += strlen(bp)+1;
	}
	*p = nil;
	bpi->envp = (char**)environ;
}


int
execm(void *va, int size, int argc, char **argv, int sysmode)
{
	ulong *buf = (ulong*)va;
	MemIstream is;
	int i;

	mem_openi(&is, va, size);

	if(gunzip_header((Istream*)&is) > 0) {
		MemOstream os;
		buf = malloc(0x200000);
		if(buf == nil) {
			error("no memory");
			return -1;
		}
		mem_openo(&os, buf, 0x200000);
		siseek((Istream*)&is, 0, SEEK_SET);
		print("Inflating %s\n", gzip_name);
		soflush(stdout);
		size = gunzip((Istream*)&is, (Ostream*)&os);
		if(size < 0)
			return size;
		print("%d -> %d    \n", is.pos, size);
	}
	argcopy(argc, argv);
	bpi->himem = (ulong)argbuf;
	for(i=0; i<16; i++)
		print("%8.8ux%c", buf[i], ~i&7 ? ' ' : '\n');
	if(sysmode >= 0)
		return bpi->exec(buf, bpi, sysmode ? BP_EXEC_FLAG_SYSMODE : 0);
	return 0;
}

