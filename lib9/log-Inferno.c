#include	"lib9.h"

extern int _logging;	/* print.c */
static char	_logname[64];

int
logopen(char *logname)
{
	strncpy(_logname, logname, sizeof(_logname)-1);
	_logging = 1;
	return 1;
}

void
logmsg(int fd, char *s)
{
	USED(fd);
	syslog(0, _logname, "%s", s);
}

void
logclose(void)
{
	_logging = 0;
}
