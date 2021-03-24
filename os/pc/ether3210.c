/*
 * EAGLE Technology Model NE3210
 * 32-Bit EISA BUS Ethernet LAN Adapter.
 * Programmer's Reference Guide kindly supplied
 * by Artisoft Inc/Eagle Technology.
 *
 * BUGS:
 *	no setting of values from config file;
 *	should we worry about doubleword memmove restrictions?
 *	no way to use mem addresses > 0xD8000 at present.
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"
#include "../port/netif.h"

#include "etherif.h"

enum {					/* EISA slot space */
	NVLreset	= 0xC84,	/* 0 == reset, 1 == enable */
	NVLconfig	= 0xC90,

	DP83902off	= 0x000,	/* offset of DP83902 registers */
	Eaddroff	= 0x016,	/* offset of Ethernet address */
};

static struct {
	ulong	port;
	ulong	config;
} slotinfo[MaxEISA];

static ulong mem[8] = {
	0x00FF0000, 0x00FE0000, 0x000D8000, 0x0FFF0000,
	0x0FFE0000, 0x0FFC0000, 0x000D0000, 0x00000000,
};

static ulong irq[8] = {
	15, 12, 11, 10, 9, 7, 5, 3,
};

static struct {
	char	*type;
	uchar	val;
} media[] = {
	{ "10BaseT",	0x00, },
	{ "RJ-45",	0x00, },
	{ "10Base5",	0x80, },
	{ "AUI",	0x80, },
	{ "10Base2",	0xC0, },
	{ "BNC",	0xC0, },
	{ 0, },
};

static int
reset(Ether *ether)
{
	static int already;
	int i;
	ulong p;
	Dp8390 *dp8390;

	/*
	 * First time through, check if this is an EISA machine.
	 * If not, nothing to do. If it is, run through the slots
	 * looking for appropriate cards and saving the
	 * configuration info.
	 */
	if(already == 0){
		already = 1;
		if(strncmp((char*)(KZERO|0xFFFD9), "EISA", 4))
			return 0;

		for(i = 1; i < MaxEISA; i++){
			p = i*0x1000;
			if(inl(p+EISAconfig) != 0x0118CC3A)
				continue;

			slotinfo[i].port = p;
			slotinfo[i].config = inb(p+NVLconfig);
		}
	}

	/*
	 * Look through the found adapters for one that matches
	 * the given port address (if any). The possibilties are:
	 * 1) 0;
	 * 2) a slot address.
	 */
	i = 0;
	if(ether->port == 0){
		for(i = 1; i < MaxEISA; i++){
			if(slotinfo[i].port)
				break;
		}
	}
	else if(ether->port >= 0x1000){
		if((i = (ether->port>>16)) < MaxEISA){
			if((ether->port & 0xFFF) || slotinfo[i].port == 0)
				i = 0;
		}
	}
	if(i >= MaxEISA || slotinfo[i].port == 0)
		return 0;

	/*
	 * Set the software configuration using the values obtained.
	 * For now, ignore any values from the config file.
	 */
	ether->port = slotinfo[i].port;
	ether->mem = KZERO|mem[slotinfo[i].config & 0x07];
	ether->irq = irq[(slotinfo[i].config>>3) & 0x07];
	ether->size = 32*1024;
	if((ether->ea[0]|ether->ea[1]|ether->ea[2]|ether->ea[3]|ether->ea[4]|ether->ea[5]) == 0){
		for(i = 0; i < sizeof(ether->ea); i++)
			ether->ea[i] = inb(ether->port+Eaddroff+i);
	}

	/*
	 * For now, can't map anything other than mem addresses
	 * 0xD0000 and 0xD8000.
	 */
	if(getisa(ether->mem, ether->size, 0) == 0)
		panic("ether3210: 0x%lux reused or invalid", ether->mem);

	/*
	 * Set up the stupid DP8390 configuration.
	 */
	ether->private = malloc(sizeof(Dp8390));
	dp8390 = ether->private;
	dp8390->bit16 = 1;
	dp8390->ram = 1;
	dp8390->dp8390 = ether->port+DP83902off;
	dp8390->tstart = 0;
	dp8390->pstart = HOWMANY(sizeof(Etherpkt), Dp8390BufSz);
	dp8390->pstop = HOWMANY(ether->size, Dp8390BufSz);

	/*
	 * Reset the board, then
	 * initialise the DP83902,
	 * set the ether address.
	 */
	outb(ether->port+NVLreset, 0x00);
	delay(2);
	outb(ether->port+NVLreset, 0x01);

	dp8390reset(ether);
	dp8390setea(ether);

	return 0;
}

void
ether3210link(void)
{
	addethercard("NE3210",  reset);
}
