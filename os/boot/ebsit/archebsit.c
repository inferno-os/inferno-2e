#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "io.h"
#include "bpi.h"

void
archreset(void)
{
}

void
archconfinit(void)
{	
}

void
dbgled(int n, ulong v)
{
        int x;
        for(x=0; x<32; x++, v>>=1) {
		int y;
        	ulong *s = (ulong*)0x03100000+(31-x)+n*640;
		for(y=0; y<3; y++, s+=160) 
			*s = ((x&7) ? 0x85000000 : 0x87000000)
				| ((v&1) ? 0 : 0xffffff);
		*s = 0x85858585;
        }
}

int
archflash12v(int on)
{
        return 1;
}

int
archflashwp(int /*wp*/)
{
	return 1;
}

/* Multi-word output to I/O address */
void
outss(int ioaddr, const void *data, int count)
{
    int i;
    const ushort *p = (const ushort *)data;
    for (i = 0; i < count; i++) {
        outs(ioaddr, *p++);
 
    }
}
 
/* Multi-word input from an I/O address */
void
inss(int ioaddr, void *buffer, int count)
{
    int i;
    ushort *p = (ushort *)buffer;
    for (i = 0; i < count; i++) {
        *p++ = ins(ioaddr);
 
    }
}


ushort
ins(int addr)
{
	uchar *p = (uchar*)(PIObase | addr);
	return p[0]|(p[1]<<8);
}

void
outs(int addr, ushort v)
{
	uchar *p = (uchar*)(PIObase | addr);
	p[0] = v&0xff;
	p[1] = v>>8;
}

