#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "bpi.h"

void
portbpidump(void)
{
	int i;
	
	print("bpi v%d.%d at %ux (sp=%ux)\n", 
			bpi->bpi_major,
			bpi->bpi_minor,
			bpi, &i);
	print("bootname:%s v%d.%d\n", bpi->bootname,
			bpi->bootver_major,
			bpi->bootver_minor);
	print("argc=%d argv=%ux envp=%ux flags=%ux\n",
			bpi->argc,
			bpi->argv,
			bpi->envp,	
			bpi->flags);
	if(bpi->argv)
		for(i=0; i<bpi->argc; i++) 
			print(" argv[%d]='%s'\n", i, bpi->argv[i]);
	if(bpi->envp)
		for(i=0; bpi->envp[i]; i++) 
			print(" envp[%d]='%s'\n", i, bpi->envp[i]);
	print("entry=%ux lomem=%ux himem=%ux\n",
			bpi->entry,
			bpi->lomem,
			bpi->himem);
}


