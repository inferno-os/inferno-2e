void	aamloop(int);
void addboottype(Type *);
void	alarminit(void);
Block*	allocb(int);
int	bootp(Medium *, char*);
void	cancel(Alarm*);
void	cgascreenputs(char*, int);
void	checkalarms(void);
int	cistrcmp(char*, char*);
int	cistrncmp(char*, char*, int);
void	clockinit(void);
void	consinit(void);
int	conschar(void);
int	cpuid(char*, int*, int*);
void	delay(int);
uchar*	etheraddr(int);
int	etherinit(void);
int	etherrxpkt(int, Etherpkt*, int);
int	ethertxpkt(int, Etherpkt*, int, int);
#define	evenaddr(x)		/* 386 doesn't care */
int	floppyinit(void);
long	floppyread(int, void*, long);
long	floppyseek(int, long);
void	freeb(Block*);
char*	getconf(char*);
ulong	getcr0(void);
ulong	getcr2(void);
ulong	getcr3(void);
/* int	getfields(char*, char**, int, char); */
int	getstr(char*, char*, int, char*, int);
int	hardinit(void);
long	hardread(int, void*, long);
long	hardseek(int, long);
long	hardwrite(int, void*, long);
void	i8042a20(void);
void	i8042reset(void);
void*	ialloc(ulong, int);
int	inb(int);
ushort	ins(int);
ulong	inl(int);
void	insb(int, void*, int);
void	inss(int, void*, int);
void	insl(int, void*, int);
int	isaconfig(char*, int, ISAConf*);
void	kbdinit(void);
void	kbdchar(int);
void	lgdt(ushort[3]);
void	lidt(ushort[3]);
void	ltr(ulong);
void	machinit(void);
void	meminit(void);
void	microdelay(int);
void	mmuinit(void);
uchar	nvramread(int);
void	outb(int, int);
void	outs(int, ushort);
void	outl(int, ulong);
void	outsb(int, void*, int);
void	outss(int, void*, int);
void	outsl(int, void*, int);
void	panic(char*, ...);
int	pcicfgr8(Pcidev*, int);
int	pcicfgr16(Pcidev*, int);
int	pcicfgr32(Pcidev*, int);
void	pcicfgw8(Pcidev*, int, int);
void	pcicfgw16(Pcidev*, int, int);
void	pcicfgw32(Pcidev*, int, int);
Pcidev* pcimatch(Pcidev*, int, int);
void	putcr3(ulong);
void	putidt(Segdesc*, int);
void	qinit(IOQ*);
Partition* sethardpart(int, char*);
Partition* setscsipart(int, char*);
void	setvec(int, void (*)(Ureg*, void*), void*);
int	splhi(void);
int	spllo(void);
void	splx(int);
void	trapinit(void);
void	uartspecial(int, void (*)(int), int (*)(void), int);
void	uartputs(IOQ*, char*, int);
void	vectortable(void);

#define malloc(n)	ialloc(n, 0)
#define free(v)

#define	GSHORT(p)	(((p)[1]<<8)|(p)[0])
#define	GLONG(p)	((GSHORT(p+2)<<16)|GSHORT(p))
#define	GLSHORT(p)	(((p)[0]<<8)|(p)[1])
#define	GLLONG(p)	((GLSHORT(p)<<16)|GLSHORT(p+2))

#define KADDR(a)	((void*)((ulong)(a)|KZERO))
#define PADDR(a)	((ulong)(a)&~KZERO)

#define	HOWMANY(x, y)	(((x)+((y)-1))/(y))
#define ROUNDUP(x, y)	(HOWMANY((x), (y))*(y))

#define xalloc(n)	ialloc(n, 0)
#define xfree(v)
typedef int Lock;
typedef struct {int dummy;} QLock;
typedef int Rendez;
#define lock(l)
#define unlock(l)
#define ilock(l)
#define iunlock(l)
#define intrenable(a, b, c, d)	setvec(a, b, c)

int	dmacount(int);
int	dmadone(int);
void	dmaend(int);
void	dmainit(int);
long	dmasetup(int, void*, long, int);

