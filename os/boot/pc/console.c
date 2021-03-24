#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

static IOQ consiq;
static IOQ consoq;

static int useuart;

void
consinit(void)
{
	char *p;
	int baud, port;

	qinit(&consiq);
	kbdinit();

	if((p = getconf("console")) == 0 || strcmp(p, "cga") == 0)
		return;

	qinit(&consoq);

	port = strtoul(p, 0, 0);
	baud = 0;
	if(p = getconf("baud"))
		baud = strtoul(p, 0, 0);
	if(baud == 0)
		baud = 9600;
	uartspecial(port, kbdchar, conschar, baud);
	useuart = 1;
}

void
kbdchar(int c)
{
	c &= 0x7F;
	if(c == 0x10)
		panic("^p");
	consiq.putc(&consiq, c);
}

int
conschar(void)
{
	return consoq.getc(&consoq);
}

static void
consputs(char* s, int n)
{
	cgascreenputs(s, n);
	if(useuart)
		uartputs(&consoq, s, n);
}

static int
getline(char *buf, int size, int dotimeout)
{
	int c, i=0;
	ulong start;
	char echo;

	for (;;) {
		start = m->ticks;
		do{
			if(dotimeout && ((m->ticks - start) > 5*HZ))
				return -2;
			c = consiq.getc(&consiq);
		}while(c == -1);
		if(c == '\r')
			c = '\n'; 		/* turn carriage return into newline */
		if(c == '\177')
			c = '\010';		/* turn delete into backspace */
		if(c == '\025')
			echo = '\n';		/* echo ^U as a newline */
		else
			echo = c;
		consputs(&echo, 1);

		if(c == '\010'){
			if(i > 0)
				i--; /* bs deletes last character */
			continue;
		}
		/* a newline ends a line */
		if (c == '\n')
			break;
		/* ^U wipes out the line */
		if (c =='\025')
			return -1;
		if(i == size)
			return size;
		buf[i++] = c;
	}
	buf[i] = 0;
	return i;
}

int
getstr(char *prompt, char *buf, int size, char *def, int dotimeout)
{
	int len, isdefault;

	buf[0] = 0;
	isdefault = (def && *def);
	if(isdefault == 0)
		dotimeout = 0;
	for (;;) {
		if(isdefault)
			print("%s[default==%s]: ", prompt, def);
		else
			print("%s: ", prompt);
		len = getline(buf, size, dotimeout);
		switch(len){
		case -1:
			/* ^U typed */
			continue;
		case -2:
			/* timeout, use default */
			consputs("\n", 1);
			len = 0;
			break;
		default:
			break;
		}
		if(len >= size){
			print("line too long\n");
			continue;
		}
		break;
	}
	if(len == 0 && isdefault)
		strcpy(buf, def);
	return 0;
}

int
sprint(char *s, char *fmt, ...)
{
	return donprint(s, s+PRINTSIZE, fmt, (&fmt+1)) - s;
}

int
print(char *fmt, ...)
{
	char buf[PRINTSIZE];
	int n;

	n = donprint(buf, buf+sizeof(buf), fmt, (&fmt+1)) - buf;
	consputs(buf, n);
	return n;
}

void
panic(char *fmt, ...)
{
	char buf[PRINTSIZE];
	int n;

	consputs("panic: ", 7);
	n = donprint(buf, buf+sizeof(buf), fmt, (&fmt+1)) - buf;
	consputs(buf, n);
	consputs("\n", 1);

	spllo();
	i8042reset();
	aphalt();
}
