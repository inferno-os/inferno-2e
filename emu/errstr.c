#include	"dat.h"

/*
 * General OS interface to errors
 */
void
kwerrstr(char *fmt, ...)
{
	va_list arg;
	char buf[ERRLEN];

	va_start(arg, fmt);
	doprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	strncpy(up->env->error, buf, ERRLEN);
}

void
kerrstr(char *err)
{
	char tmp[ERRLEN];

	strncpy(tmp, up->env->error, ERRLEN);
	strncpy(up->env->error, err, ERRLEN);
	strncpy(err, tmp, ERRLEN);
}

void
kgerrstr(char *err)
{
	char *s;

	s = "<no-up>";
	if(up != nil)
		s = up->env->error;
	strncpy(err, s, ERRLEN);
}
