/*
 *  PC specific code for the ns16552.
 */
enum
{
	UartFREQ= 1843200,
};

#define uartwrreg(u,r,v)	outb((u)->port + r, (u)->sticky[r] | (v))
#define uartrdreg(u,r)		inb((u)->port + r)

extern void	ns16552setup(ulong, ulong, char*);
extern void	uartclock(void);

static void
uartpower(int, int)
{
}

/*
 *  handle an interrupt to a single uart
 */
static void
ns16552intrx(Ureg* ureg, void* arg)
{
	USED(ureg);

	ns16552intr((ulong)arg);
}

/*
 *  install the uarts (called by reset)
 */
void
ns16552install(void)
{
	int port;
	char *p, *q;
	static int already;

	if(already)
		return;
	already = 1;

	/* first two ports are always there and always the normal frequency */
	ns16552setup(0x3F8, UartFREQ, "eia0");
	intrenable(VectorUART0, ns16552intrx, (void*)0, BUSUNKNOWN);
	ns16552setup(0x2F8, UartFREQ, "eia1");
	intrenable(VectorUART1, ns16552intrx, (void*)1, BUSUNKNOWN);
	addclock0link(uartclock);

	/* set up a serial console */
	if(p = getconf("console")){
		port = strtol(p, &q, 0);
		if(p != q && (port == 0 || port == 1))
			ns16552special(port, 9600, &kbdq, &printq, kbdcr2nl);
	}
}

/*
 * If the UART's receiver can be connected to a DMA channel,
 * this function does what is necessary to create the
 * connection and returns the DMA channel number.
 * If the UART's receiver cannot be connected to a DMA channel,
 * a -1 is returned.
 */
char
ns16552dmarcv(int dev)
{
 
	USED(dev);
        return -1;
}
