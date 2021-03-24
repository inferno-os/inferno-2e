#include "../port/portbpi.h"

/*
 *	Unless absolutely necessary, all changes
 *	should be made in a manner that does not rearrange any
 *	attributes or change their sizes.
 */

struct Bpi {
	PortBpi;
	ulong	flashbase;	/* base address of flash memory */
	ulong	cpuspeed;	/* CPU speed, in HZ */
	ulong	pagetable;	/* page table virtual address */
	ulong	_[5];

	/* for interfacing to serial port, etc for raw connection: */
	int	(*send)(void *buf, int len);
	int	(*recv)(void *buf, int len, int ms);
	/* for taking over the connection from StyxMon: */
	int	bps;
	int	rootfid;
	ulong	_[4];
};


extern Bpi *bpi;

void bpidump(void);

