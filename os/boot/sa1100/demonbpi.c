/* Demon/BootParam interface 
 * (for old demons without BPI)
 */

#include <lib9.h>
#include "bpi.h"

extern void demon_exit(ulong);
extern void demon_writec(uchar);
extern uchar demon_readc(void);
extern void demon_write0(char*);
extern int demon_seek(int, ulong);
extern ulong demon_flen(int);
extern int demon_istty(int);
extern int demon_open(const char *, int);
extern int demon_close(int);
extern int demon_write(int, const void*, ulong);
extern int demon_read(int, void*, ulong);

extern Bpi *bpi;
static char *bpi_argv[] = {"sboot"};

#define BP_CONSOLE_FD	0

long
demonbpi_write(int fd, const void *va, ulong len)
{
	int n = 0;
	uchar *a = (uchar*)va;
	if(fd == BP_CONSOLE_FD) {
		while(len > 0) {
			char buf[512];
			int x = len > sizeof(buf)-1 ? sizeof(buf)-1 : len;
			memcpy(buf, a, x);
			buf[x] = '\0';
			demon_write0(buf);
			a += x; 
			n += x;
			len -= x;
		}
	} else
		n = len - demon_write(fd, va, len);
	return n;
}

long
demonbpi_read(int fd, void *va, ulong len)
{
	int n = 0;
	uchar *a = (uchar*)va;
	if(fd == BP_CONSOLE_FD) {
		while(len-- > 0) {
			*a = demon_readc();
			if(*a == 0 || *a == 255 || *a == 4)
				return n;
			a++, n++;
		}
	} else
		n = len - demon_read(fd, va, len);
	return n;
}

void
demonbpi_reboot(int  code)
{
	demon_exit(code+1);
}

int
demonbpi_fstat(int fd, BpStat *stbuf)
{
	stbuf->size = demon_flen(fd);
	stbuf->flags = 0;
	if(demon_istty(fd))
		stbuf->flags |= BPSTAT_TTY;
	return 0;
}

int
demonbpi_exec(void *adr, Bpi *bp, int flags)
{
	ulong *hdr;
	ulong base;
	hdr = (ulong*)adr;
	base = hdr[10];
	demon_write0("Moving to exec\n");
	memmove((void*)base, adr, hdr[5]+hdr[6]);
	demon_write0("Ready to exec\n");
	// demon_sysmode();
	// return ((int (*)(Bpi*))base)(bpi);
	demon_exec(base);
	return 0;
}

void
demonbpi_init(void)
{
	bpi = (Bpi*)(0x0001e000);
	bpi->bpi_major = Bpi_ver_major;
	bpi->bpi_minor = Bpi_ver_minor;
	bpi->bootver_major = 0;
	bpi->bootver_minor = 1;
	bpi->bootname = "DemonBPI";
	bpi->argv = bpi_argv;
	bpi->argc = nelem(bpi->argv);
	bpi->envp = nil;
	bpi->flags = BP_FLAG_DEBUG;
	bpi->entry = 0x8080;	/* unknown */
	bpi->lomem = 0x00100000;	/* assumed high */
	bpi->himem = 0x00400000;	/* just to be safe.. 4MB */
	bpi->console_fd = BP_CONSOLE_FD;
	bpi->exit = demon_exit;
	bpi->reboot = demonbpi_reboot;
	bpi->open = demon_open;
	bpi->close = demon_close;
	bpi->read = demonbpi_read;
	bpi->write = demonbpi_write;
	bpi->fstat = demonbpi_fstat;
	bpi->seek = demon_seek;
	bpi->exec = demonbpi_exec;
	bpi->flashbase = 0x08000000;
	bpi->cpuspeed = 200000000;
	bpi->pagetable = 0x01008000;
}

