#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "memstream.h"

extern int cmdlock;

int rows = 24;	// number of lines on stdout (currently used to pause)


int
loadfile(Istream *s, void *adr)
{
	int r = 0;
	int t = s->size - s->pos;
	uchar *bp = (uchar*)adr;
	do {
		bp += r;
		status("reading", bp-(uchar*)adr, t);
	} while((r = sread(s, bp, 8192)) > 0);
	return r < 0 ? r : bp-(uchar*)adr;
}


// can be optimized for memory read/writes,
// could also be generalized into a "stream to stream" copy command
// such as:  scopy(os, is, len);

static int
cmd_copy(int, char **argv, int *)
{
	Istream *is;
	Ostream *os;
	uchar *bp;
	int n,w,r = 0;

	if(!(is = sd_openi(argv[1])))
		return -1;
	if(!(os = sd_openo(argv[2]))) {
		sd_closei(is);
		return -1;
	}

	n = is->size-is->pos;
	w = os->size-os->pos;
	if(w && n > w) {
		char estr[ERRLEN];
		sprint(estr, "%x > %x", n, w);
		error(estr);
		r = -1;
	} else {
		uchar *fbp;
		bp = (uchar*)malloc(is->size ? n : 0x10000);
		n = loadfile(is, bp);
		if(strstr(argv[0], "/u")) {
			MemIstream mis;
			MemOstream mos;
			fbp = (uchar*)malloc(0x400000);
			if(fbp == nil) {
				error(Enomem);
				r = -1;
				goto err;
			}
			mem_openi(&mis, bp, n);
			mem_openo(&mos, fbp, 0x400000);
			if((n = gunzip(&mis, &mos)) < 0)
				r = -1;
			print("%d -> %d\n", mis.pos, n);
		} else 
			fbp = bp;
		swrite(os, fbp, n);
		if(fbp != bp)
			free(fbp);
		free(bp);
	}
err:
	sd_closei(is);
	sd_closeo(os);
	return r;
}

static int
cmd_boot(int argc, char **argv, int*)
{
	Istream *is;
	uchar *bp;
	int r;
	int sysmode = 1;

	if(argc == 1) {
		char b[256];
		if(!getenv("bootfile")) {
			error("no bootfile");
			return -1;
		}
		sprint(b, "%s $bootfile $bootargs", argv[0]);
		return system(b);
	}

	if(strstr(argv[0], "/u"))
		sysmode = 0;
	if(strstr(argv[0], "/t"))
		sysmode = -1;

	if(!(is = sd_openi(argv[1])))
		return -1;

	if((bp = simmap(is)))
		bp += (r = is->pos);
	else {
		bp = (uchar*)malloc(is->size ? is->size : 0x10000);
		r = loadfile(is, bp);
	}
	if(r >= 0)
		r = execm(bp, is->size, argc-1, &argv[1], sysmode);
	sd_closei(is);
	if(!simmap(is))
		free(bp);
	return r;
}


static int
cmd_examine(int argc, char **argv, int *)
{
	Istream *s;
	int ws=1;
	int line=0;
	static char lastname[NAMELEN];
	static int lastsize;
	static int lastpos = 0;

	if(argc == 2) {
		s = sd_openi(argv[1]);
		if(!s)
			return -1;
		lastpos = 0;
		lastsize = ws;
		strcpy(lastname, argv[1]);
	} else {
		if(lastname[0] == 0)
			return -1;
		s = sd_openi(lastname);
		if(!s)
			return -1;
		siseek(s, lastpos, 0);
		ws = lastsize;
	}
	if(argv[0][1] == '/')
		ws = strtoul(&argv[0][2], 0, 0);
	if(ws != 1 && ws != 2 && ws != 4 && ws != 8)
		ws = 1;
	lastsize = ws;
	while(s->pos < s->size) {
		uchar buf[16];
		char str[80];
		char *sp = str;
		int i, n;
		sp += sprint(sp, "%8.8ux: ", s->pos);
		n = sread(s, buf, sizeof(buf));
		lastpos = s->pos;
		for(i=0; i<sizeof(buf); i++) {
			if(i < n) {
				if(!(i & (ws-1))) switch(ws) {
				case 1:
					sp += sprint(sp, "%2.2ux ", buf[i]);
					break;
				case 2:
					sp += sprint(sp, "%4.4ux  ",
							*(ushort*)&buf[i]);
					break;
				case 4:
					sp += sprint(sp, "%8.8ux    ",
							*(ulong*)&buf[i]);
					break;
				case 8:
					sp += sprint(sp, "%8.8ux%8.8ux        ",
						*(ulong*)&buf[i+4],
						*(ulong*)&buf[i]);
					break;
				}
				if(buf[i] < 32 || buf[i] > 126)
					buf[i] = '.';
			} else {
				sp += sprint(sp, "   ");
				buf[i] = ' ';
			}
		}
		sprint(sp, " %16.16s\n", buf);
		puts(str);
		if(++line >= rows-1)
			break; 
	}
	if(lastpos >= s->size)
		lastname[0] = 0;	// EOF
	sd_closei(s);
	return 0;
}

static int
cmd_deposit(int argc, char **argv, int *nargv)
{
	int i;
	Ostream *s = sd_openo(argv[1]);
	if(!s) 
		return -1;
	for(i=2; i<argc; i++) {
		ulong n = nargv[i];
		swrite(s, &n, sizeof(n));
	}
	sd_closeo(s);
	return 0;
}


static int
cmd_stat(int, char **argv, int *)
{
	Istream *s = sd_openi(argv[1]);
	if(!s)
		return -1;
	print("pos=%ux (%d) size=%ux (%d)\n", s->pos, s->pos,
						s->size, s->size);
	sd_closei(s);
	return 0;
}


int
cmd_start(int argc, char **argv, int *nargv)
{
	int r;

	if(argc != 2 || argv[0][1] != 0) {
		error("usage: s addr");
		return -1;
	}
	r = ((int (*)(void))nargv[1])();
	print("exit %d\n", r);
	return 0;
}

int
cmd_Deposit(int argc, char **, int *nargv)
{
	int i;
	for(i=2; i<argc; i++)
		*(ulong*)nargv[1] = nargv[i];
	return 0;
}

int
cmd_Examine(int, char **, int *nargv)
{
	ulong adr = nargv[1];
	int n = nargv[2];
	do {
		print("%8.8ux: %8.8ux\n", adr,
			*(ulong*)adr);
		adr += sizeof(ulong);
	} while(n-- > 1);
	return 0;
}


int
cmd_delay(int, char **, int *nargv)
{
	delay(nargv[1]);
	return 0;
}


int
cmd_title(int, char **, int *nargv)
{
	do {
		title();
	} while(nargv[1]);
	return 0;
}


int
cmd_print(int argc, char **argv, int *)
{
	while(--argc > 0)
		print("%s ", *++argv);
	print("\n");
	return 0;
}

int
cmd_source(int, char **argv, int *)
{
	Istream *is;
	int r;
	char cmd[128];

	if(!(is = sd_openi(argv[1])))
		return -1;
	while(sgets(is, cmd, sizeof(cmd))) {
		print("%s\n", cmd);
		if((r = _system(cmd, cmdlock)) < 0) {
			sd_closei(is);
			return r;
		}
	}
	return sd_closei(is);
}

int
getrows(void) { return rows; }

void
setrows(int n) { rows = n; }


void
cmdportlink()
{
	addcmd('p', cmd_print, 0, 0xff, "print");
	addcmd('T', cmd_title, 0, 1, "title");
	addcmd('b', cmd_boot, 0, 0xff, "boot");
	addcmd('s', cmd_start, 1, 1, "start @");
	addcmd('D', cmd_Deposit, 2, 0xff, "deposit");
	addcmd('E', cmd_Examine, 1, 2, "examine");
	addcmd('d', cmd_deposit, 2, 0xff, "deposit");
	addcmd('e', cmd_examine, 0, 1, "examine");
	addcmd('S', cmd_stat, 1, 1, "stat");
	addcmd('c', cmd_copy, 2, 2, "copy");
	addcmd('m', cmd_delay, 1, 1, "msec");
	addcmd('.', cmd_source, 1, 1, "source");
	nbindenv("rows", getrows, setrows);
}

