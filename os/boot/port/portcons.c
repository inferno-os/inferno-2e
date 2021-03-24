#include <lib9.h>
#include "dat.h"
#include "fns.h"

#define PRINTSIZE	1024

int
sprint(char *s, char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = doprint(s, s+PRINTSIZE, fmt, arg) - s;
	va_end(arg);

	return n;
}

int
print(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	va_start(arg, fmt);
	n = doprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	swrite(stdout, buf, n);
	return n;
}

void
panic(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	swrite(stdout, "ACK! ", 14);
	va_start(arg, fmt);
	n = doprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	swrite(stdout, buf, n);
	halt();
}


int
putc(char c)
{ return sputc(stdout, c); }

int
puts(char *s)
{ return sputs(stdout, s); }

char *
gets(char *s, int n)
{ soflush(stdout); return sgets(stdin, s, n); }

void
error(const char *s)
{
	strcpy(lasterr, s);
}

