/*
 * I/O
 */
#define PIObase		0x02000000		/* port I/O space */
#define MIObase		0x03000000		/* memory I/O space */
#define IOPIO(a)	(PIObase+(a))		/* ?? */
#define IOMIO(a)	(MIObase+(a))

#define inb(addr)	(*((uchar*)(PIObase|(addr))))
//#define inw(addr)	(*((ushort*)(PIObase|(addr))))
ushort ins(int addr);
#define inl(addr)	(*((ulong*)(PIObase|(addr))))
void inss(int addr, void *, int);
//#define ins(addr)	(*((ushort*)(PIObase|(addr))))
#define outb(addr, val)	( *((uchar*)(PIObase|(addr))) = (val) )
void outs(int addr, ushort val);
//#define outw(addr, val) ( *((ushort*)(PIObase|(addr))) = (val) )
//#define outs(addr, val) ( *((ushort*)(PIObase|(addr))) = (val) )
#define outl(addr, val)	( *((ulong*)(PIObase|(addr))) = (val) )
void outss(int addr, const void *, int);

/* CSR Read Bits */

#define CSRR_EOF		0x0000001
#define CSRR_PCMCIA_IRQ0	0x0000002
#define CSRR_PCMCIA_IRQ1 	0x0000004
#define CSRR_PCMCIA_IRQ2 	0x0000008
#define CSRR_DAA_Ring		0x0000010
#define CSRR_Mouse_Data		0x0000020
#define CSRR_Keybd_Data		0x0000040
#define CSRR_Cause_Int0		0x0000080
#define CSRR_ComDataRdy		0x0001000
#define CSRR_ISA_Slot_0  	0x0002000
#define CSRR_ISA_Slot_1		0x0004000
#define CSRR_Cause_Int1		0x0008000

/* CSR Write Bits */

#define CSRW_IRQ0	0x1
#define CSRW_IRQ1	0x2
#define CSRW_IRQ2	0x4
#define CSRW_IRQ3	0x8
#define CSRW_IRQ4	0x10
#define CSRW_IRQ5	0x20
#define CSRW_IRQ6	0x40
#define CSRW_RamEnable	0x80
#define CSRW_Codec_On	0x100
#define CSRW_comDataRdy	0x1000
#define CSRW_ISA0_On	0x2000
#define CSRW_ISA1_On	0x4000
#define CSRW_CauseInt1	0x8000

#define BUSUNKNOWN	0
#define MaxEISA		0

/*
 * LED macros
 */
#define POWER	lightGreen()
#define MESSAGE	lightRed()
#define LOWBAT	lightYellow()

#define GPLR	((ulong*)0)	/* because of machine-dependent code in flash.c */
