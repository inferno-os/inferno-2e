#include	"lib9.h"

int
logopen(char *logname)
{
	USED(logname);
	return 0;
}

void
logmsg(int fd, char *s)
{
	(void) write(fd, s, strlen(s));
}

void
logclose(void)
{
}
