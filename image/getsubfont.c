#include <lib9.h>
#include <kernel.h>
#include <image.h>

/*
 * Default version: treat as file name
 */

Subfont*
getsubfont(Display *d, char *name)
{
	int fd;
	Subfont *f;

	fd = libopen(name, OREAD);
		
	if(fd < 0){
		fprint(2, "getsubfont: can't open %s: %r\n", name);
		return 0;
	}
	/*
	 * unlock display so i/o happens with display released.
	 * getsubfont is called only from string.c and stringwidth.c,
	 * which are known to be safe to have this done - they are
	 * called directly from limbo, not the garbage collector.
	 */
	if(d->local == 0)
		unlockdisplay(d);
	f = readsubfont(d, name, fd, 1);
	if(d->local == 0)
		lockdisplay(d, 0);
	if(f == 0)
		fprint(2, "getsubfont: can't read %s: %r\n", name);
	libclose(fd);
	return f;
}
