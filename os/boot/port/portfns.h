int		qread(Queue*, void*, long n, long offset);
int		qwrite(Queue*, void*, long n);
int		qempty(Queue*);
int		qlen(Queue*);
void		qflush(Queue*);

Block		*newblock(void *buf, int n);

int		putc(char c);
int		puts(char *s);
char *		gets(char *s, int n);
void		panic(char *fmt, ...);

void		addcmd(char c, int (*f)(int, char**, int*), int minargs, int maxargs, const char *desc);
int		_system(const char *cmd, int cmdlock);
int		system(const char *cmd);
void		cmdinterp(void);
void		ioloop(void);

void		mallocinit(ulong lowaddr, ulong highaddr);
void		print_bpi(void);
void		print_title(void);
void		title(void);
void		switchcons(void);
void		boot(void);
void		autoboot(void);
int		execm(void*, int size, int argc, char **argv, int sysmode);
void		halt(void);
void		error(const char*);
void		status(const char*, int, int);
void		statusclear(void);
int		interrupt(void);
void		exit(int);

void		printbootinfo(void);

const char*	getenv(const char *name);
int		putenv(const char *name, const char *val);
int		nbindenv(const char *name, int (*)(void), void (*)(int));
int		sbindenv(const char *name, const char* (*)(void), void (*)(const char*));
int		envinit(const char **pbuf, int pbufsize, char *sbuf,  int sbufsize);
void		printenv(void);

Istream*	sd_openi(const char*);
Ostream*	sd_openo(const char*);
int		sd_closei(Istream*);
int		sd_closeo(Ostream*);
void		addstreamdevlink(StreamDev *);

int		sread(Istream *, void *, int);
int		swrite(Ostream *, const void *, int);
int		sgetc(Istream *);
char*		sgets(Istream *, char *, int);
int		sputc(Ostream *, char);
int		sputs(Ostream *, char *);
int		siseek(Istream *, int, int);
int		soseek(Ostream *, int, int);
int		siflush(Istream *);
int		soflush(Ostream *);
int		soclose(Istream *);
int		siclose(Ostream *);
uchar*		simmap(Istream *);
uchar*		sommap(Ostream *);

#ifndef SEEK_SET
#define SEEK_SET	0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR	1
#endif
#ifndef SEEK_END
#define SEEK_END	2
#endif

int		gunzip_header(Istream *s);
int		gunzip(Istream *is, Ostream *os);

