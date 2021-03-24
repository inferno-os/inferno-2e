#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "bpi.h"

int radix = 16;
int cmdlock = 0;

typedef struct CmdTab {
	int (*f)(int, char**, int*);
	int minargs;
	int maxargs;
	const char *desc;
} CmdTab;

CmdTab cmdtab[96];


void
addcmd(char c, int (*f)(int, char**, int*),
		int minargs, int maxargs, const char *desc)
{
	CmdTab *ct = &cmdtab[c-32];
	ct->f = f;
	ct->minargs = minargs;
	ct->maxargs = maxargs;
	ct->desc = desc;
}


int
islabel(char c)
{
	return 	   (c >= '0' && c <= '9')
		|| (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z')
		|| (c == '_');
}

int
envsubst(char *dest, int destsize, const char *src)
{
	char *np = nil;
	char *dend = dest + destsize - 1;
	while(np || (*src && dest < dend)) {
		if(np && !islabel(*src)) {
			const char *v;
			int n;
			*dest = '\0';
			v = getenv(np);
			if(!v)
				v = "";			// string not found
			n = strlen(v);
			if(np+n+1 >= dend)
				return -1;              // buffer overflow
			strcpy(np, v);
			dest = np + n;
			np = nil;
		} else if(*src == '$') {
			np = dest;
			src++;
		} else
			*dest++ = *src++;
	}
	*dest = '\0';
	return *src ? -1 : 0;			// check for buffer overflow
}


int
_system(const char *cmd, int cmdlock)
{
	char buf[256];
	char *argv[128];
	int nargv[128];
	int argc;
	char *cp;
	char ch;
	int i;
	int r = 0;

	if(envsubst(buf, sizeof(buf), cmd) < 0)
		return -1; 
	cp = buf;
	ch = *cp;
	while(ch) {
		argc = 0;
		for(i=0; i<nelem(argv); i++)
			argv[i] = "";
		memset(nargv, 0, sizeof(nargv));
		do {
			while((ch = *cp) && ch <= 32)
				cp++;
			if(!ch)
				break;
			if(ch == ';') {
				cp++;
				break;
			}
			argv[argc] = cp;
			while((ch = *cp) && ch > 32 && ch != ';') {
				if(!argc && ch == '=' && cp != argv[argc]) {
					*cp++ = '\0';
					return putenv(argv[argc], cp);
				}
				cp++;
			}
			*cp++ = '\0';
			nargv[argc] = strtoul(argv[argc], 0, radix);
			argc++;
		} while(ch && ch != ';');

		if(argv[0]) {
			CmdTab *ct;
			if(argv[0][0] <= 32 || argv[0][0] >= 127) {
				error("line noise");
				return -1;
			}
			if(argv[0][0] == '#')
				return 0;
			if(cmdlock && argv[0][0] != '.') {
				error("cmdlock on");
				return -1;
			}
			ct = &cmdtab[argv[0][0]-32];

			r = -1;
			if(!ct->f)
				error("invalid command");
			else if(argc-1 < ct->minargs)
				error("missing args");
			else if(argc-1 > ct->maxargs)
				error("too many args");
			else 
				r = ct->f(argc, argv, nargv);
			if(r < 0)
				return r;
		}
	}
	return r;
}

int
system(const char *cmd)
{
	return _system(cmd, 0);
}

void
cmdinterp(void)
{
	char cmd[128];

	do {
		const char *prompt = getenv("PS1");
		print(prompt ? prompt : ">>> ");
		soflush(stdout);
		if(!gets(cmd, sizeof(cmd)))
			return;
	} while(!cmd[0]);
	
	lasterr[0] = '\0';
	if(system(cmd) < 0)
		print("?%r\n");
}


int
cmd_env(int, char **argv, int *)
{
	if(argv[0][1])
		print("%s\n", getenv(&argv[0][1]));
	else
		printenv(); 
	return 0;
}

int
cmd_help(int argc, char **argv, int *)
{
	int first = 32;
	int last = 127;
	int i;
	if(argc > 1) 
		first = last = argv[1][0];
	for(i=first; i <= last; i++) {
		CmdTab *ct = &cmdtab[i-32];
		if(ct->f)
			print("%c (%3d-%3d) %s\n", i,
				ct->minargs, ct->maxargs, ct->desc);
	}
	return 0;
}

void
setradix(int r) { radix = r; }

int
getradix(void) { return radix; }

void
setcmdlock(int n) { cmdlock = n; }

int
getcmdlock(void) { return cmdlock; }

void
cmdlink()
{
	addcmd('?', cmd_help, 0, 1, "help");
	addcmd('=', cmd_env, 0, 0, "env");
	nbindenv("r", getradix, setradix);
	nbindenv("cmdlock", getcmdlock, setcmdlock);
}

