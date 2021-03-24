#include <lib9.h>
#include "dat.h"
#include "fns.h"

static StreamDev *sdlinks = 0;

extern int radix;

static StreamDev *
parse_devname(const char *str, char *args, char *adrsize)
{
	StreamDev *sd = sdlinks;
	const char *xp = strchr(str, '!');
	const char *ap = strrchr(str, '@');
	int slen = strlen(str);
	if(ap)
		strcpy(adrsize, ap+1);
	else {
		ap = strrchr(str, ',');
		if(ap && ap > xp)
			strcpy(adrsize, ap);
		else {
			adrsize[0] = '\0';
			ap = str+slen;
		}
	}
	if(xp) {
		strncpy(args, xp+1, ap-xp-1);
		args[ap-xp-1] = '\0';
	} else {
		strncpy(args, str, ap-str);
		args[ap-str] = '\0';
		/*
		xp = str;
		*/
		xp=ap;
	}
	while(sd) {
		if(strncmp(str, sd->name, xp-str) == 0)
			return sd;
		sd = sd->next;
	}
	error("bad device");
	return 0;
}

static void
setadrsize(Istream *s, char *adrsize)
{
	char *cp = strchr(adrsize, ',');
	if(!*adrsize)
		return;
	s->pos = strtoul(adrsize, 0, radix);
	if(cp)
		s->size = s->pos+strtoul(cp+1, 0, radix);
}

Istream*
sd_openi(const char *str)
{
	char args[128];
	char adrsize[40];
	StreamDev *sd;
	Istream *s;
	if(str[0] == '-' && str[1] == '\0') {
		s = malloc(320);	/* enough for most streams */
		memcpy(s, stdin, 320);
	} else {	
		if(!(sd = parse_devname(str, args, adrsize)))
			return nil;
		if((s = sd->openi(args)))
			setadrsize(s, adrsize);
	}
	return s;
}


Ostream*
sd_openo(const char *str)
{
	char args[128];
	char adrsize[40];
	StreamDev *sd;
	Ostream *s;
	if(str[0] == '-' && str[1] == '\0') {
		s = malloc(sizeof(Ostream));
		memcpy(s, stdout, sizeof(Ostream));
	} else {
		if(!(sd = parse_devname(str, args, adrsize)))
			return nil;
		if((s = sd->openo(args)))
			setadrsize(s, adrsize);
	}
	return s;
}

int
sd_closei(Istream *s)
{
	siclose(s);
	free(s);
	return 0;
}

int
sd_closeo(Ostream *s)
{
	soclose(s);
	free(s);
	return 0;
}

static int cmd_ldev(int, char **, int *)
{
	StreamDev *sd = sdlinks;
	while(sd) {
		print("%s! %c%c\n",
			sd->name, sd->openi ? 'r' : ' ',
				sd->openo ? 'w' : ' ');
		sd = sd->next;
	}
	return 0;
}

void
addstreamdevlink(StreamDev *sd)
{
	sd->next = sdlinks;
	sdlinks = sd;
	addcmd('!', cmd_ldev, 0, 0, "dev");
}

