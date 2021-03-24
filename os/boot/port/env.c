#include <lib9.h>
#include "dat.h"
#include "fns.h"


const char **environ = nil;
static const char **envpbufend = nil;
static char *envsbuf = nil;
static char *envsbufend = nil;
static char *envsbuffree = nil;

extern int radix;

enum VarType { VNUM, VSTR };

typedef struct VarTab VarTab;
struct VarTab {
	const char *name;
	union {
		struct {
			int (*get)(void);
			void (*set)(int);
		} num;
		struct {
			const char* (*get)(void);
			void (*set)(const char *);
		} str;
	};
	enum VarType type;
	VarTab *next;
};

static VarTab *vartab = nil;


static const char*
_getenv(const char *name)
{
	int n = strlen(name);
	const char **ep = environ;
	while(*ep && (strncmp(*ep,  name, n) != 0 || (*ep)[n] != '='))
		ep++;
	if(!*ep)
		return nil;
	return *ep+n+1;
}

const char*
getenv(const char *name)
{
	VarTab *v = vartab;
	while(v) {
		if(strcmp(v->name, name) == 0) {
			static char numbuf[20];
			switch(v->type) {
			case VNUM: 
				sprint(numbuf, (strcmp(name, "r") == 0
					|| radix == 10) ? "%d"
					: (radix == 16) ? "%ux"
					: (radix == 8) ? "%o"
					: "0x%ux",
					v->num.get());
				return numbuf;
			case VSTR:
				return v->str.get();
			}
		}
		v = v->next;
	}
	return _getenv(name);
}

static int
_putenv(const char *name, const char *val)
{
	int n = strlen(name);
	const char **ep = environ;
	char *sp = envsbuf;
	if(!*ep)
		envsbuffree = sp;
	while(sp < envsbuffree) {
		char *np = sp + strlen(sp) + 1;
		if(strncmp(sp, name, n) == 0 && sp[n] == '=') {
			memmove(sp, np, envsbuffree - np);
			envsbuffree -= (np - sp);
		} else {
			*ep++ = sp;
			sp = np;
		}
	}
	if(ep+1 >= envpbufend || sp+n+1+strlen(val)+1 > envsbufend) {
		*ep = nil;
		return -1;
	}
	*ep++ = sp;
	envsbuffree = sp + sprint(sp, "%s=%s", name, val) + 1;
	*ep = nil;
	return 0;
}

int
putenv(const char *name, const char *val)
{
	VarTab *v = vartab;
	while(v) {
		if(strcmp(v->name, name) == 0) {
			switch(v->type) {
			case VNUM:
				v->num.set(strtoul(val, 0, 
					(strcmp(name, "r") == 0) ? 0 : radix));
				return 0;
			case VSTR:
				v->str.set(val);
				return 0;
			}
		}
		v = v->next;
	}
	return _putenv(name, val);
}


int
nbindenv(const char *name, int (*get)(void), void (*set)(int))
{
	VarTab *v = malloc(sizeof(VarTab));
	v->name = name;
	v->num.get = get;
	v->num.set = set;
	v->type = VNUM;
	v->next = vartab;
	vartab = v;
	return 0;
}

int sbindenv(const char *name,
		const char* (*get)(void), void (*set)(const char*))
{
	VarTab *v = malloc(sizeof(VarTab));
	v->name = name;
	v->str.get = get;
	v->str.set = set;
	v->type = VSTR;
	v->next = vartab;
	vartab = v;
	return 0;
}

int envinit(const char **pbuf, int pbufsize, char *sbuf,  int sbufsize);

int
envinit(const char **pbuf, int pbufsize, char *sbuf,  int sbufsize)
{
	environ = pbuf;
	envsbuf = sbuf;
	*environ = nil;
	envpbufend = pbuf+pbufsize/sizeof(*pbuf);
	envsbufend = sbuf+sbufsize/sizeof(*sbuf);
	return 0;	
}


void
printenv(void)
{
	const char **ep = environ;
	VarTab *v = vartab;
	while(v) {
		print("*%s=%s\n", v->name, getenv(v->name));
		v = v->next;
	}
	while(*ep)
		print("%s\n", *ep++);
}

