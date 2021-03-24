#include <lib9.h>
#include <kernel.h>
#include <image.h>

Font*
openfont(Display *d, char *name, int ldepth)
{
	Font *fnt;
	int fd, i;
	char *buf;
	Dir dir;

	fd = libopen(name, OREAD);
	if(fd < 0)
		return 0;
	if(libdirfstat(fd, &dir) < 0){
    Err0:
		libclose(fd);
		return 0;
	}
	buf = malloc(dir.length+1);
	if(buf == 0)
		goto Err0;
	buf[dir.length] = 0;
	i = libread(fd, buf, dir.length);
	libclose(fd);
	if(i != dir.length){
		free(buf);
		return 0;
	}
	fnt = buildfont(d, buf, name, ldepth);
	free(buf);
	return fnt;
}
