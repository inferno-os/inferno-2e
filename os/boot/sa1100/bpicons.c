#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "bpi.h"
#include "../port/ttystream.h"
#include "../port/screen.h"
#include "../port/vmode.h"
#include "../port/error.h"
#include "kbd.h"

#ifdef ndef
static void
bpicon_oflush(void)
{
	static int has_dev_tty = -1;
	int fd;
	if(has_dev_tty < 0) {
		has_dev_tty = 0;
		if((fd = bpi->open("/dev/tty", BP_O_RDONLY))) {
			has_dev_tty = 1;
			bpi->close(fd);
		}
	}
	if(has_dev_tty) 
		bpi->write(bpi->console_fd, "\033[A\0337\n\0338\033[B", 11);
}
#endif

static int
bpicon_read(Istream *, void *va, int len)
{
	return bpi->read(bpi->console_fd, va, len);
};

static int
bpicon_write(Ostream *, void *va, int len)
{
	if(len == 0) {
		// bpicon_oflush();
		if(bpi->poll)
			bpi->poll(1);
		return 0;
	} else {
		const char *c = (const char*)va;
		while(len-- && *c)
			c++;
		return bpi->write(bpi->console_fd, va, (uint)c-(uint)va);
	}
}


static Istream _bpicon_istream = { bpicon_read, 0, 0 };
static Ostream _bpicon_ostream = { bpicon_write, 0, 0 };

Istream *dbgin = &_bpicon_istream;
Ostream *dbgout = &_bpicon_ostream;

Istream *stdin = &_bpicon_istream;
Ostream *stdout = &_bpicon_ostream;

int localcons;


void
ioloop()
{
	for(;;) {
		if(localcons) {
			stdin = conin;
			stdout = conout;
		} 
		cmdinterp();
	}
}


int
cmd_iredir(int, char **argv, int *)
{
	switch(argv[1][0]) {
	case 'd': stdin = dbgin; break;
	case 'k': stdin = conin; break;
	}
	return 0;
}

int
cmd_oredir(int, char **argv, int *)
{
	switch(argv[1][0]) {
	case 'd': stdout = dbgout; break;
	case 'v': stdout = conout; break;
	}
	return 0;
}

void
bpiconslink()
{
	/*
	if(bpi->bpi_major >= 1)
		print("WARNING: BootParam interface < 1.0 is required\n");
	*/

	print("debug=%d\n", bpi->flags&BP_FLAG_DEBUG);
	if(!(bpi->flags & BP_FLAG_DEBUG)) {
		localcons = 1;
		stdin = stdout = nil;
		dbgin = dbgout = nil;
	}
	addcmd('<', cmd_iredir, 1, 1, "input redir");
	addcmd('>', cmd_oredir, 1, 1, "output redir");
}

