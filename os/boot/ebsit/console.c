#include <u.h>
#include "lib.h"
#include "iostream.h"
#include "fns.h"
#include "console.h"
#include "bootparam.h"
#include "ttystream.h"

static void
demcon_oflush(void)
{
	static int has_dev_tty = -1;
	int fd;
	if(has_dev_tty < 0) {
		has_dev_tty = 0;
		if((fd = bootparam->open("/dev/tty", BP_O_RDONLY))) {
			has_dev_tty = 1;
			bootparam->close(fd);
		}
	}
	if(has_dev_tty) 
		bootparam->write(bootparam->console_fd,
			"\033[A\0337\n\0338\033[B", 11);
}

static int
demcon_read(Istream *s, void *va, int len)
{
	return bootparam->read(bootparam->console_fd, va, len);
};

static int
demcon_write(Ostream *s, void *va, int len)
{
	if(len == 0) {
		demcon_oflush();
		return 0;
	} else {
		const char *c = (const char*)va;
		while(len-- && *c)
			c++;
		return bootparam->write(bootparam->console_fd, va,
							(uint)c-(uint)va);
	}
}


static Istream _demcon_istream = { demcon_read, 0, 0 };
static Ostream _demcon_ostream = { demcon_write, 0, 0 };

Istream *demcon_istream = &_demcon_istream;
Ostream *demcon_ostream = &_demcon_ostream;

vidcon_write(Ostream *, void *va, int len)
{
	int n = screen_write((char*)va, len);
	screen_flush();
	return n;
}


static Ostream _vidcon_ostream = { vidcon_write, 0, 0 };
Ostream *vidcon_ostream = &_vidcon_ostream;

static int
kbd_read(Istream *, void *va, int len)
{
	uchar *a = (uchar*)va;
	int i = len;
	while(i-- > 0) 
		*a++ = kbd_getc();
	return len;
}

static Istream _kbd_istream = { kbd_read, 0, 0 };
static TtyIstream _tty_kbd_istream;

Istream *kbd_istream = &_kbd_istream;

Istream *stdin = &_demcon_istream;
Ostream *stdout = &_demcon_ostream;
Ostream *stderr = &_demcon_ostream;


void
console_init()
{
	if(!(bootparam->flags & BP_FLAG_DEBUG)) {
		stdin = stdout = nil;
		demcon_istream = demcon_ostream = nil;
	}
	kbd_init();
	if(tty_openi(&_tty_kbd_istream, kbd_istream, vidcon_ostream,
			TTYSTREAM_ECHO) >= 0)
		kbd_istream = &_tty_kbd_istream;
}

