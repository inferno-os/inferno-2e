void	accessdir(Iobuf*, Dentry*, int);
void	addfree(Device, long, Superb*);
long	balloc(Device, int, long);
void	bfree(Device, long, int);
int	Cconv(void*, Fconv*);
int	checkname(char*);
int	checktag(Iobuf*, int, long);
void 	cmd_user(void);
char*	cname(char*);
int	con_attach(int, char*, char*);
int	con_clone(int, int);
int	con_create(int, char*, int, int, long, int);
int	con_open(int, int);
int	con_path(int, char*);
int	con_read(int, char*, long, int);
int	con_remove(int);
int	con_stat(int, char*);
int	con_clri(int);
int	con_session(void);
int	con_walk(int, char*);
int	con_write(int, char*, long, int);
int	con_wstat(int, char*);
int	fsconvD2M(Dentry*, char*);
int	fsconvM2D(char*, Dentry*);
int	convM2S(char*, Fcall*, int);
int	convS2M(Fcall*, char*);
void	cprint(char*, ...);
void	dbufread(Iobuf*, Dentry*, long);
int	Zconv(void*, Fconv*);
int	devcmp(Device, Device);
Iobuf*	dnodebuf(Iobuf*, Dentry*, long, int);
int	doremove(File *, int);
void	dtrunc(Iobuf*, Dentry*);
int	Filconv(void*, Fconv*);
ulong	fakeqid(Dentry*);
Float	famd(Float, int, int, int);
ulong	fdf(Float, int);
void	fileinit(FChan*);
File*	filep(FChan*, int, int);
void	formatinit(void);
void	freefp(File*);
void	freewp(Wpath*);
void*	fsalloc(long);
void	fsctl(char*);
Filsys*	fsstr(char*);
void	f_attach(FChan*, Fcall*, Fcall*);
void	f_auth(FChan*, Fcall*, Fcall*);
void	f_clone(FChan*, Fcall*, Fcall*);
void	f_clunk(FChan*, Fcall*, Fcall*);
void	f_create(FChan*, Fcall*, Fcall*);
void	f_fserrstr(FChan*, Fcall*, Fcall*);
void	f_nop(FChan*, Fcall*, Fcall*);
void	f_flush(FChan*, Fcall*, Fcall*);
void	f_open(FChan*, Fcall*, Fcall*);
void	f_read(FChan*, Fcall*, Fcall*);
void	f_remove(FChan*, Fcall*, Fcall*);
void	f_session(FChan*, Fcall*, Fcall*);
void	f_stat(FChan*, Fcall*, Fcall*);
void	f_userstr(FChan*, Fcall*, Fcall*);
void	f_walk(FChan*, Fcall*, Fcall*);
void	f_clwalk(FChan*, Fcall*, Fcall*);
void	f_write(FChan*, Fcall*, Fcall*);
void	f_wstat(FChan*, Fcall*, Fcall*);
int	Tconv(void*, Fconv*);
Iobuf*	getbuf(Device, long, int);
Dentry*	getdir(Iobuf*, int);
long	getraddr(Device);
void	hexdump(void*, int);
int	iaccess(File*, Dentry*, int);
long	indfetch(Iobuf*, Dentry*, long, long , int, int);
int	ingroup(int, int);
void	iobufinit(void);
int	leadgroup(int, int);
File*	newfp(FChan*);
Qid	newqid(Device);
void	newstart(void);
Wpath*	newwp(void);
void	p9fcall(FChan*, Fcall*, Fcall*);
void	panic(char*, ...);
int	prime(long);
void	putbuf(Iobuf*);
void	rootream(Device, long);
void	settag(Iobuf*, int, long);
int	strtouid(char*);
int	strtouid1(char*);
int	superok(Device, long, int);
void	superream(Device, long);
void	sync(char*);
int	syncblock(void);
int	Tconv(void*, Fconv*);
long	time(void);
Tlock*	tlocked(Iobuf*, Dentry*);
long	toytime(void);
void	uidtostr(char*,int);
void	uidtostr1(char*,int);

long	belong(char *);
void	check(Filsys *, long);
int 	cmd_exec(char*);
void	consserve(void);
void	confinit(void);
void	fsconfinit(void);
int	fsinit(int, int);
void	kfsmain(int, char**);
int	fnextelem(void);
long	number(int, int);
int	skipbl(int);
void	startproc(char*, void (*)(void*), void*);
void	syncproc(void*);
void	syncall(void);
ulong	statlen(char*);

char*	wreninit(Device, char*, int);
char*	wrencheck(Device);
void	wrenream(Device);
long	wrensize(Device);
long	wrensuper(Device);
long	wrenroot(Device);
int	wrenread(Device, long, void *);
int	wrenwrite(Device, long, void *);

/*
 * macros for compat with bootes
 */
#define toytime()	seconds()
#define	localfs			1

#define devgrow(d, s)	0
#define nofree(d, a)	0
#ifndef KFS_ISRO	// temporary hack...
#define KFS_ISRO	0
#endif
#define isro(d)		(filsys[d.ctrl].flags&FRONLY)
#define isflash(d)	(filsys[d.ctrl].flags&FFLASH)

#define	superaddr(d)		((*devcall[d.type].super)(d))
#define	getraddr(d)		((*devcall[d.type].root)(d))
#define devsize(d)		((*devcall[d.type].size)(d))
#define	devwrite(d, a, v)	((*devcall[d.type].write)(d, a, v))
#define	devread(d, a, v)	((*devcall[d.type].read)(d, a, v))


/*
 * extra library functions
 */
extern	void	qsort(void*, long, long, int (*)(void*, void*));
