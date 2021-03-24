#include <lib9.h>
#include <image.h>

/*
 * Default version: convert to file name
 */

char*
subfontname(char *cfname, char *fname, int ldepth)
{
	char *t, *u, tmp1[256], tmp2[256];

	t = cfname;
	if(t[0] != '/'){
		strcpy(tmp2, fname);
		u = utfrrune(tmp2, '/');
		if(u)
			u[0] = 0;
		else
			strcpy(tmp2, ".");
		sprint(tmp1, "%s/%s", tmp2, t);
		t = tmp1;
	}
	sprint(tmp2, "%s.%d", t, ldepth);
	if(access(tmp2, 0) == 0)
		t = tmp2;
	else if(access(t, 0) < 0)
		return 0;
	t = strdup(t);
	return t;
}
