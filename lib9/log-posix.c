#include	"lib9.h"
#include <syslog.h>

extern int _logging;	/* print.c */

int
logopen(char *logname)
{
	openlog(logname, LOG_PID, LOG_DAEMON);
	_logging = 1;
	return 1;
}

void
logmsg(int fd, char *s)
{
	int pri;

	pri = LOG_INFO;
	if (fd == 2)
		pri = LOG_ERR;
	syslog(pri, "%s", s);
}

void
logclose(void)
{
	_logging = 0;
	closelog();
}
