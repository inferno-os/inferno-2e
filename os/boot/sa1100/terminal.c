#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "io.h"
#include "kbd.h"
#include "../port/screen.h"
#include "keyboard.h"

#include "bpi.h"


static int
serwrite(uchar *data, int len)
{
	int n = 0;
	UartReg *u = UARTREG(3);
	if(bpi->send)
		return bpi->send(data, len);
	while(len--) {
		delay(0);
		while(!(u->utsr1 & UTSR1_TNF))
			;
		delay(0);
		u->utdr = *data++;
		n++;
	}
	return n;
}

static int
serread(uchar *data)
{
	int n = 0;
	UartReg *u = UARTREG(3);
	if(bpi->recv)
		return bpi->recv(data, 1, 1);
	if(u->utsr1 & UTSR1_RNE) {
		*data++ = u->utdr;
		++n;
	}
	return n;
}

extern void (*delayf)();

enum {
	SBUFSIZE= 8192
};

uchar _sbuf[SBUFSIZE+8192+16+512];	/* extra padding for StyxMon's DMA */
uchar *sbuf = _sbuf;
int sbin = 0;
int sbout = 0;
int sbfull = SBUFSIZE-1;

static void
dsread(void)
{
	while(sbin != sbfull && serread(&sbuf[sbin]) > 0) {
		sbin++;
		if(sbin >= SBUFSIZE)
			sbin = 0;
	}
}

int
terminal(int, char **, int *)
{
	ulong srows = text_rows;
	uchar buf[32];
	UartReg *u = UARTREG(3);
	ulong sutcr3;

	sutcr3 = u->utcr3;
	u->utcr3 = UTCR3_TXE|UTCR3_RXE;
	if(bpi->recv)
		sbuf = (uchar*)va2pa(sbuf);
	else
		delayf = dsread;
	text_rows = vd_hgt/fonthgt;
	for(;;) {
		int n;
		while((n = sread(rconin, buf, -sizeof buf)) > 0) {
			if(buf[0] == 0x1c) {
				sread(rconin, buf, 1);
				switch(buf[0]) {
				case 'q':
					goto tdone;
				case 'i':
					serwrite((uchar*)"\034", 1);
				case '.':
					continue;
				default:
					print("<q/i/.>");
				}
				break;
			}
			serwrite(buf, n);
		}
		dsread();
		if(sbin != sbout) {
			if(sbin < sbout) {
				swrite(conout, &sbuf[sbout], SBUFSIZE-sbout);
				sbout = 0;
			}
			if(sbin > sbout) {
				int in = sbin;
				swrite(conout, &sbuf[sbout], in-sbout);
				sbout = in;
			}
			soflush(stdout);
			sbfull = sbout-1;
			if(sbfull < 0)
				sbfull = SBUFSIZE-1;
		}
		dsread();
	}
tdone:
	delayf = 0;
	u->utcr3 = sutcr3;
	text_rows = srows;
	return 0;
}


int
getbps(void) {
	UartReg *u = UARTREG(3);
	int c = (u->utcr2)|((u->utcr1&0xff)<<8);
	return 3686400/(16*(c+1));
}

void
setbps(int b)
{
	int c = (3686400/b)/16-1;
	UartReg *u = UARTREG(3);
	u->utcr3 &= ~(UTCR3_TXE|UTCR3_RXE);
	u->utcr1 = c>>8; 
	u->utcr2 = c&0xff; 
	u->utcr3 |= (UTCR3_TXE|UTCR3_RXE);
}

void
terminallink()
{
	addcmd('X', terminal, 0, 1, "terminal");
	nbindenv("bps", getbps, setbps); 
}

