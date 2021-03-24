#include "../port/portfns.h"

void	links(void);
ulong	mmuctlregr(void);
void	mmuctlregw(ulong);
ulong	mmuregr(int);
void	mmuregw(int, ulong);
ulong	va2pa(void*);
ulong	va2ucva(void*);
ulong	va2ubva(void*);
ulong	va2ucubva(void*);
void	writeBackBDC(void);
void	writeBackDC(void);
void	reboot(int);
void	archconfinit(void);
void	bootconfinit(void);
void	archreset(void);
int	archflashwp(int);
int	archflash12v(int);

// #define timer_start()	(*OSCR)
// #define timer_ticks(t)	(*OSCR - (ulong)(t))
#define DELAY(ms)	timer_delay(MS2TMR(ms))
#define MICRODELAY(us)	timer_delay(US2TMR(us))
ulong	timer_start(void);
ulong	timer_ticks(ulong);
int 	timer_devwait(ulong *adr, ulong mask, ulong val, int ost);
void 	timer_setwatchdog(int ost);
void 	timer_delay(int ost);
ulong	ms2tmr(int ms);
int	tmr2ms(ulong t);
void	delay(int ms);
ulong	us2tmr(int us);
int	tmr2us(ulong t);
void 	microdelay(int us);

char* bpgetenv(char *var);
char* bpenumenv(int n);
int bpoverride(char *name, int *p);

void	lcd_setbrightness(ushort);
void	lcd_setcontrast(ushort);
void	lcd_setbacklight(int);

ushort ntohs(ushort s);
ushort htons(ushort s);
ulong ntohl(ulong s);
ulong htonl(ulong s);

int pcmspecial(char *, ISAConf *);

