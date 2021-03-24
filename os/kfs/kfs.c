#include	"all.h"
#include "../port/error.h"

/*
 * until the BUFSIZE values are moved to a Filsys/Device-specific
 * location, IBUFSIZE is the value for all devices.
 * file systems that use something else can't be accessed.
 * it's a small block size for Inferno to help out small flash devices.
 */

enum {
	IBUFSIZE = 1024,
};

int	writeallow;	/* never on; for compatibility with fs */
int	wstatallow;
int	kfschat;

static	FChan	*chaninit(int);
static	void	bsizeinit(int);
static	void	fsconsinit(void);

extern	int	qclosed(Queue*);

void
kfsmain(int argc, char **argv)
{
	USED(argc, argv);
	
	fsconfinit();
	formatinit();

	chan = chaninit(-1);
	fsconsinit();
	files = fsalloc(fsconf.nfile * sizeof(*files));
	tlocks = fsalloc(NTLOCK * sizeof *tlocks);
	wpaths = fsalloc(fsconf.nwpath * sizeof(*wpaths));
	uid = fsalloc(fsconf.nuid * sizeof(*uid));
	uidspace = fsalloc(fsconf.uidspace * sizeof(*uidspace));
	gidspace = fsalloc(fsconf.gidspace * sizeof(*gidspace));

	/*
	 * init global locks
	 */
	wlock(&mainlock); wunlock(&mainlock);
	lock(&wpathlock); unlock(&wpathlock);

	/*
	 * set the block size and allocate some buffers
	 */
	bsizeinit(IBUFSIZE);
	iobufinit();

	kproc("Fs.sync", syncproc, nil);
}

static char *
estrdup(char *n)
{
	char *p;

	p = malloc(strlen(n)+1);
	if(p == nil)
		error("no memory");
	strcpy(p, n);
	return p;
}

/*
 * respond to requests on the #Kcons/kfsctl device
 *	ream name devname
 *		initialise a new file system on devname
 *	filsys name devname [ro]
 *		check and activate an existing file system on devname
 * in both cases the file system is referenced as #Kname
 */
void
fsctl(char *s)
{
	char *fields[5], *err, ebuf[ERRLEN];
	int nf, ream;
	Filsys *fs;
	Device dev;

	if(strncmp(s, "cons", 4) == 0) {
		s += 4;
		while ((*s == ' ') || (*s == '\t'))
			s++;
		if (!cmd_exec(s))
			error("kfs: invalid command");
		return;
	}
	nf = parsefields(s, fields, 5, " \t\n");
	if(nf < 3)
		error(Eio);
	ream = 0;
	if(strcmp(fields[0], "filsys") == 0)
		;
	else if(strcmp(fields[0], "ream") == 0)
		ream = 1;
	else
		error("unknown ctl request");
	if(strcmp(fields[1], "cons") == 0)
		error(Ebadarg);
	wlock(&mainlock);
	if(waserror()){
		wunlock(&mainlock);
		nexterror();
	}
	for(fs = filsys;; fs++){
		if(fs == &filsys[MAXFILSYS])
			error("kfs: too many active file systems");
		if(fs->name == nil)
			break;
		if(strcmp(fields[1], fs->name) == 0 || strcmp(fields[2], fs->devname) == 0)
			error(Einuse);
	}
	dev = (Device){Devwren, 0, 0, 0};	/* only type for now */
	dev.ctrl = fs-filsys;
	fs->dev = dev;
	err = nil;
	if (KFS_ISRO)
	{
		fs->flags |= FRONLY;
		if (ream)
			err = fserrstr[Eronly];
	}
	if (nf > 3)
	{
		if (!ream && (strncmp(fields[3], "ro", 2) == 0))
			fs->flags |= FRONLY;
		else
			err = "invalid 4th arg";
	}
	if (err == nil)
		err = (*devcall[dev.type].init)(dev, fields[2], ream);
	if(err != nil){
		snprint(ebuf, sizeof(ebuf), "kfs: init: %s", err);
		error(ebuf);
	}
	if(ream){
		(*devcall[dev.type].ream)(dev);
		rootream(dev, getraddr(dev));
		superream(dev, superaddr(dev));
	}else{
		err = (*devcall[dev.type].check)(dev);
		if(err != nil){
			snprint(ebuf, sizeof(ebuf), "kfs: dcheck: %s", err);
			error(ebuf);
		}
	}
	fs->dev = dev;
	fs->name = estrdup(fields[1]);
	fs->devname = estrdup(fields[2]);
	wunlock(&mainlock);
	poperror();
	if(!ream && !superok(dev, superaddr(dev), 0))
		check(fs, Cfree|Cquiet);
	if(fs == &filsys[0]){
		/* first one activates console and determines /adm/users */
		consserve();
		cmd_exec("cfs");
		cmd_exec("user");
	}
}

/*
 * process to synch blocks
 * it puts out a block/line every second
 * it waits 10 seconds if catches up.
 * in both cases, it takes about 10 seconds
 * to get up-to-date.
 */
void
syncproc(void*)
{
	int i;

	for(;;){
		i = syncblock();
		tsleep(&up->sleep, return0, 0, i?1000:10000);
	}
}

void
fsconfinit(void)
{
	int cpuserver;

	cpuserver = 0;
	
	fsconf.nuid = 600;
	fsconf.uidspace = fsconf.nuid*6;
	fsconf.gidspace = fsconf.nuid*3;

	if(cpuserver)
		fsconf.nfile = 1000;
	else
		fsconf.nfile = 250;
	fsconf.nwpath = fsconf.nfile*8;

	fscons.flags = 0;
}

static FChan*
chaninit(int rfd)
{
	FChan *cp;

	cp = fsalloc(sizeof *cp);
	cp->chan = rfd;
	strncpy(cp->whoname, "<none>", sizeof(cp->whoname));
	fileinit(cp);
	wlock(&cp->reflock);
	wunlock(&cp->reflock);
	lock(&cp->flock);
	unlock(&cp->flock);
	return cp;
}

static void
fsconsinit(void)
{
	fscons.chan = fsalloc(sizeof(FChan));
	wlock(&fscons.chan->reflock);
	wunlock(&fscons.chan->reflock);
	lock(&fscons.chan->flock);
	unlock(&fscons.chan->flock);
}

/*
 * always called with mainlock locked
 */
void
syncall(void)
{
	for(;;)
		if(!syncblock())
			return;
}

ulong
memsize(void)
{
	return 4*1024*1024;	/* squeeze it */
}

static void
bsizeinit(int bsize)
{
	RBUFSIZE = bsize;

	/*
	 * set up the block size dependant variables
	 */
	BUFSIZE = RBUFSIZE - sizeof(Tag);
	DIRPERBUF = BUFSIZE / sizeof(Dentry);
	INDPERBUF = BUFSIZE / sizeof(long);
	INDPERBUF2 = INDPERBUF * INDPERBUF;
	FEPERBUF = (BUFSIZE - sizeof(Super1) - sizeof(long)) / sizeof(long);

	if(fsconf.niobuf == 0) {
		fsconf.niobuf = memsize()/10;
		if(fsconf.niobuf > 2*1024*1024)
			fsconf.niobuf = 2*1024*1024;
		fsconf.niobuf /= bsize;
		if(fsconf.niobuf < 30)
			fsconf.niobuf = 30;
		if(fsconf.niobuf > 100)
			fsconf.niobuf = 100;
	}
}

/*
 * allocate rest of mem
 * for io buffers.
 */
#define	HWIDTH	5	/* buffers per hash */
void
iobufinit(void)
{
	long i;
	Iobuf *p, *q;
	Hiob *hp;

	i = fsconf.niobuf*RBUFSIZE;
	niob = i / (sizeof(Iobuf) + RBUFSIZE + sizeof(Hiob)/HWIDTH);
	nhiob = niob / HWIDTH;
	while(!prime(nhiob))
		nhiob++;
	if(1)
		print("kfs: %ld buffers; %ld hashes\n", niob, nhiob);
	hiob = fsalloc(nhiob * sizeof(Hiob));
	hp = hiob;
	for(i=0; i<nhiob; i++) {
		lock(hp);
		unlock(hp);
		hp++;
	}
	p = fsalloc(niob * sizeof(Iobuf));
	hp = hiob;
	for(i=0; i<niob; i++) {
		qlock(p);
		qunlock(p);
		if(hp == hiob)
			hp = hiob + nhiob;
		hp--;
		q = hp->link;
		if(q) {
			p->fore = q;
			p->back = q->back;
			q->back = p;
			p->back->fore = p;
		} else {
			hp->link = p;
			p->fore = p;
			p->back = p;
		}
		p->dev = devnone;
		p->addr = -1;
		p->xiobuf = fsalloc(RBUFSIZE);
		p->iobuf = (char*)-1;
		p++;
	}
}

void
cprint(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE/2];	/* watch the size of this one */

	va_start(arg, fmt);
	n = doprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	while(waserror())
		;	/* not clear it's always safe to interrupt cprint's caller */
	if(1||fscons.out == nil || qclosed(fscons.out) || qfull(fscons.out))
		putstrn(buf, n);	/* force it out: might be important */
	else
		qwrite(fscons.out, buf, n);
	poperror();
}
