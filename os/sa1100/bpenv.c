#include <u.h>
#include "../port/lib.h"
#include <dat.h>
#include <fns.h> 
#include "bootparam.h"


char*
bpgetenv(char *name)
{
	int n;
	char **ep;

	if (bootparam == nil)
		return nil;
	n = strlen(name);
	ep = bootparam->envp;
	while(*ep && (strncmp(*ep,  name, n) != 0 || (*ep)[n] != '='))
		ep++;
	if(!*ep)
		return nil;
	return *ep+n+1;
}

int
bpoverride(char *s, int *p)
{
	char *v = bpgetenv(s);
	if(v) {
		*p = strtol(v, 0, 0);
		return 1;
	} else
		return 0;
}

int
bpoverride_uchar(char *s, uchar *p)
{
	char *v = bpgetenv(s);
	if(v) {
		*p = strtol(v, 0, 0);
		return 1;
	} else
		return 0;
}

char*
bpenumenv(int n)
{
	static int initialized = 0;
	static char e_cpuspeed[9+10+UTFmax+1];
	static char e_bootname[9+40+UTFmax+1];
	static char e_bootver[8+9+UTFmax+1];

	if (bootparam == nil)
		return nil;
	if(!initialized) {
		snprint(e_bootname, sizeof e_bootname, "bootname=%s", bootparam->bootname);
		snprint(e_bootver, sizeof e_bootver, "bootver=%d.%d", bootparam->bootver_major, bootparam->bootver_minor);
		snprint(e_cpuspeed, sizeof e_cpuspeed, "cpuspeed=%lud", bootparam->cpuspeed);
		initialized = 1;
	}
	switch(n) {
	case 0: return e_bootname;
	case 1: return e_bootver;
	case 2: return e_cpuspeed;
	default: return bootparam->envp[n-3];
	}
	return bootparam->envp[n];
}

