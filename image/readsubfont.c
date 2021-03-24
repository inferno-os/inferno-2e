#include <lib9.h>
#include <kernel.h>
#include <image.h>

void
_unpackinfo(Fontchar *fc, uchar *p, int n)
{
	int j;

	for(j=0;  j<=n;  j++){
		fc->x = p[0]|(p[1]<<8);
		fc->top = p[2];
		fc->bottom = p[3];
		fc->left = p[4];
		fc->width = p[5];
		fc++;
		p += 6;
	}
}

Subfont*
readsubfont(Display*d, char *name, int fd, int dolock)
{
	char hdr[3*12+4+1];
	int n, locked;
	uchar *p;
	Fontchar *fc;
	Subfont *f;
	Image *i;

	i = readimage(d, fd, dolock);
	if(i == nil)
		return nil;
	if(libread(fd, hdr, 3*12) != 3*12){
		kwerrstr("rdsubfonfile: header read error: %r");
		return nil;
	}
	n = atoi(hdr);
	p = malloc(6*(n+1));
	if(p == nil)
		return nil;
	if(libread(fd, p, 6*(n+1)) != 6*(n+1)){
		kwerrstr("rdsubfonfile: fontchar read error: %r");
    Err:
		free(p);
		return nil;
	}
	fc = malloc(sizeof(Fontchar)*(n+1));
	if(fc == nil)
		goto Err;
	_unpackinfo(fc, p, n);
	locked = 0;
	if(dolock)
		locked = lockdisplay(d, 0);
	f = allocsubfont(name, n, atoi(hdr+12), atoi(hdr+24), fc, i);
	if(locked)
		unlockdisplay(d);
	if(f == nil){
		free(fc);
		goto Err;
	}
	free(p);
	return f;
}
