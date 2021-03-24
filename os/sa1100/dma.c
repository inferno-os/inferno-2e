//
// Each DMA channel is double buffered.
// so one buffer can be in use while the other is being filled or emptied.
//
// Each DMA channel can be set in one of two directions, DmaIN or DmaOUT
// (device to memory, and memory to device, respectively)
//
// The endianess of the DMA transfer can be set.
//
// 
//
// The usual course of action is the following, where
//	channel		= the dma channel to use, 0..5 (0 is highest priority)
//			  (some versions of the StrongArm have bugs in some
//			   channels)
//	isr		= interrupt service routine called when the dma is done
//	arg		= argument to pass to the ISR
//	device		= The dma device (from the list, DmaUDC .. DmaSSP)
//	direction	= DmaIN (device -> memory) or DmaOUT (memory -> device)
//	endianess	= Endian of the transfer (DmaLittle or DmaBig)
//	b1		= src or dst buffer for the dma xfer.
//	b1siz		= length in bytes of the dma buffer
//	b2		= src or dst buffer for the dma xfer.
//	b2siz		= length in bytes of the dma buffer
//
//	init()
//	{
//		 // configure the dma channel
//		dmasetup(channel, device, direction, endianess);
//
//		 // enable the dma interrupt
//		 // do this after dmasetup because this channel might already
//		 // have been in use by someone else and you wouldn't want
//		 // to get interrupted about a different xfer before you
//		 // call dmasetup()
//		intrenable(DmaBIT(channel), isr, arg, BusCPU);
//
//		 // begin the dma xfer
//		dmastart(channel, void *b1, int b1siz, void *b2, int b2siz);
//		...
//	}
//
//	isr()
//	{
//		if(dmaerror(channel)) {
//			... handle the error ...
//			return;
//		}
//
//		//
//		// the first buffer finished will be "b1" from dmastart(),
//		// then "b2", followed thereafter in the order you pass them
//		// to dmacontinue()
//		//

//		dmacontinue(channel, some_buf, sizeof(some_buf));
//	}
//

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"

#define DMABASE	0xB0000000

enum {
	 // DMA CSR operation select
	CSRSET =	4,			// write 1's to set
	CSRCLEAR =	8,			// write 1's to clear
	CSRGET =	4,			// read

	 /* DMA CSR bits */
	CSRrun=		1 << 0,
	CSRie=		1 << 1,
	CSRerror=	1 << 2,
	CSRdonea=	1 << 3,
	CSRstrta=	1 << 4,
	CSRdoneb=	1 << 5,
	CSRstrtb=	1 << 6,
	CSRbiu=		1 << 7,
};

#define DDAR(channel) ((ulong*)(DMABASE + (0x20*(channel))))
#define DCSR(channel, rw) ((ulong*)(DMABASE + (rw) + (0x20*(channel))))

#define DBS(channel, bs) \
	((ulong*)(DMABASE + 0x10 + ((bs >> 5) * 4) + (0x20*(channel))))
#define DBT(channel, bs) \
	((ulong*)(DMABASE + 0x14 + ((bs >> 5) * 4) + (0x20*(channel))))

 /* DMA device addresses */
#define DMA_UDC_ADDR		0x80000008
#define DMA_SDLC_ADDR		0x80020078
#define DMA_UART0_ADDR		0x80010014
#define DMA_HSSP_ADDR		0x8004006C
#define DMA_UART1_ADDR		0x80030014
#define DMA_UART2_ADDR		0x80050014
#define DMA_MCPAUDIO_ADDR	0x80060008
#define DMA_MCPTELECOM_ADDR	0x8006000C
#define DMA_SSP_ADDR		0x80070008

void
dmasetup(int channel, int device, int direction, int endianess)
{
	ulong devaddr;
	uchar width, burst;

	 // disable any pending dma on this channel
	dmastop(channel);

	switch(device) {
		case DmaUDC:
			devaddr = DMA_UDC_ADDR;
			width = 0;
			burst = 1;
			break;
		case DmaSDLC:
			devaddr = DMA_SDLC_ADDR;
			width = 0;
			burst = 0;
			break;
		case DmaUART0:
			devaddr = DMA_UART0_ADDR;
			width = 0;
			burst = 0;
			break;
		case DmaHSSP:
			devaddr = DMA_HSSP_ADDR;
			width = 0;
			burst = 1;
			break;
		case DmaUART1:
			devaddr = DMA_UART1_ADDR;
			device = DmaHSSP;	/* same DS[3:0] as DmaHSSP */
			width = 0;
			burst = 0;
			break;
		case DmaUART2:
			devaddr = DMA_UART2_ADDR;
			width = 0;
			burst = 0;
			break;
		case DmaMCPaudio:
			devaddr = DMA_MCPAUDIO_ADDR;
			width = 1;
			burst = 1;
			break;
		case DmaMCPtelecom:
			devaddr = DMA_MCPTELECOM_ADDR;
			width = 1;
			burst = 0;
			break;
		case DmaSSP:
			devaddr = DMA_SSP_ADDR;
			width = 0;
			burst = 1;
			break;
		default:
			panic("setupdma: Unknown device %d", device);
			return;
	}

	 //
	 // disable run, no interrupts, default to buffer A
	 //
	*DCSR(channel, CSRCLEAR) = CSRrun | CSRie | CSRbiu;

	 //
	 // setup the dma controller
	 //
	*DDAR(channel) =
		 // set DA[31:8] (ddar[31:8])
		devaddr & 0xF0000000 | ((((devaddr & 0x0001FFFFC) >> 2)) << 8) |
		((device | direction) << 4) |	// DS[3:0] (ddar[7:4])
		(width << 3) | 			// DW (ddar[3])
		(burst << 2) |			// BS (ddar[2])
		(endianess << 1) |		// E (ddar[1])
		direction; 			// RW [0]
}

//
// Note: The caller must insure the buffer obeys all the 
//	 rules of the dma controller (i.e. it doesn't span
//	 non-contig blocks of physical memory, and on some versions
//	 of the StrongArm it doesn't span 256 byte boundaries.)
void
dmastart(int channel, void *buf1, int buf1size, void *buf2, int buf2size)
{
	ulong dmabuf;
	ulong v;
	ulong csr = CSRrun | CSRie;

	 // stop running, clear all bits that generate interrupts,
	 // and anything that might give us a "false start"
	*DCSR(channel, CSRCLEAR) = CSRrun | CSRie 
				| CSRdonea | CSRdoneb | CSRerror
				| CSRstrta | CSRstrtb;

	v = *DCSR(channel, CSRGET);

//	print("dmastart(%d,%ux:%ux,%ux:%ux,%ux)\n", channel, v,
//			buf1, buf1size, buf2, buf2size);

	 // start first xfer with buffer B or A?
	if(v & CSRbiu)
		dmabuf = CSRstrtb;
	else
		dmabuf = CSRstrta;
	csr |= dmabuf;

	 // setup first src/dst and size
	*DBS(channel, dmabuf) = (ulong)buf1;
	*DBT(channel, dmabuf) = buf1size;

	 // if there is another src/dst, set it up too.
	if(buf2 != nil) {
		 // next to buffer B?
		if(dmabuf == CSRstrta)
			dmabuf = CSRstrtb;
		else
			dmabuf = CSRstrta;

		*DBS(channel, dmabuf) = (ulong)buf2;
		*DBT(channel, dmabuf) = buf2size;
		csr |= dmabuf;
	}

	*DCSR(channel, CSRSET) = csr;
}

int
dmacontinue(int channel, void *buf, int bufsize)
{
	ulong startbuf;
	ulong clr;
	ulong v = *DCSR(channel, CSRGET);

	 //
	 // states & actions
	 //	1) A done and B done
	 //		clear donea and doneb
	 //		biu:	pick B
	 //		!biu:	pick A
	 //	2) A done and B in use
	 //		clear donea
	 //		Put it in A (CSRbiu == B, but B is already ready to go)
	 //	3) A in use and B done
	 //		clear doneb
	 //		Put it in B (CSRbiu == A, but A is already ready to go)
	 //	4) A in use and B in use
	 //		panic!
	 //
	switch(v & (CSRstrta|CSRstrtb)) {
	case 0:		// state 1
		if(v & CSRbiu) {
			clr = CSRdoneb;
			startbuf = CSRstrtb;
		} else {
			clr = CSRdonea;
			startbuf = CSRstrta;
		}
		break;
	case CSRstrtb:	// state 2
		clr = CSRdonea;
		startbuf = CSRstrta;
		break;
	case CSRstrta:	// state 3
		clr = CSRdoneb;
		startbuf = CSRstrtb;
		break;
	default: 	// state 4
		print("dmacontinue: unreachable state (DCSR%d=%lux) %lux,%lux %lux,%lux\n",
							channel, v,
		*DBS(channel, CSRstrta), *DBT(channel, CSRstrta),
		*DBS(channel, CSRstrtb), *DBT(channel, CSRstrtb));
		return -1;
	}


	*DBS(channel, startbuf) = (ulong)buf;
	*DBT(channel, startbuf) = bufsize;

	*DCSR(channel, CSRCLEAR) = clr;
	*DCSR(channel, CSRSET) = startbuf;
	return 0;
}


//
// clear run.
// clear interrupt enable.
// clear error.
// clear donea, strta.
// clear doneb, strtb.
//
void
dmastop(int channel)
{
	// print("dmastop (was %ux)\n", *DCSR(channel, CSRGET));

	*DCSR(channel, CSRCLEAR) =	CSRrun |
					CSRie |
					CSRerror |
					CSRdonea |
					CSRstrta |
					CSRdoneb |
					CSRstrtb;
}

//
// return nonzero if there was an error
//
// NOTE: clears the error state
//
int
dmaerror(int channel)
{
	ulong e = *DCSR(channel, CSRGET) & CSRerror;

	if(e)
		*DCSR(channel, CSRCLEAR) = CSRerror;

	return e;
}


void
dmareset(void)
{
	int i;
	// turn off each DMA channel, but don't clear the
	// reset of its state, since StyxMon needs this state
	// info to recover the connection
	for(i=0; i<6; i++)
		*DCSR(i, CSRCLEAR) = CSRrun;
}

