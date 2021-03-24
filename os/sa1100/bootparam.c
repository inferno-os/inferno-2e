#include <u.h>
#include "lib.h"
#include "mem.h"
#include "bootparam.h"

void
print_bootparam(void)
{
	int i;
	
	print("bootparam v%d.%d at %ux (sp=%ux)\n", 
			bootparam->bootparam_major,
			bootparam->bootparam_minor,
			bootparam, &i);
	print("bootname:%s v%d.%d\n", bootparam->bootname,
			bootparam->bootver_major,
			bootparam->bootver_minor);
	print("argc=%d argv=%ux envp=%ux flags=%ux\n",
			bootparam->argc,
			bootparam->argv,
			bootparam->envp,	
			bootparam->flags);
	if(bootparam->argv)
		for(i=0; i<bootparam->argc; i++) 
			print(" argv[%d]='%s'\n", i, bootparam->argv[i]);
	if(bootparam->envp)
		for(i=0; bootparam->envp[i]; i++) 
			print(" envp[%d]='%s'\n", i, bootparam->envp[i]);
	print("entry=%ux lomem=%ux himem=%ux\n",
			bootparam->entry,
			bootparam->lomem,
			bootparam->himem);
	print("flashbase=%ux cpuspeed=%d\n",
			bootparam->flashbase,
			bootparam->cpuspeed);
}


