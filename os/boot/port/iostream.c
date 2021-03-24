#include <lib9.h>
#include "dat.h"
#include "fns.h"

int
sread(Istream *s, void *buf, int len)
{
	if(s)
		return s->read(s, buf, len);
	return -1;
}

int
sgetc(Istream *s)
{
	if(s) {
		uchar c;
		if(s->read(s, &c, 1) == 1)
			return c;
	}
	return -1;	
}


char*
sgets(Istream *s, char *buf, int maxlen)
{
	int len = 0;
	int c;
	while(len < maxlen-1 && (c = sgetc(s)) != '\n') {
		if(c <= 0)
			return nil;
		buf[len++] = c;
	}
	buf[len] = 0;
	return buf;
}

int
sputc(Ostream *s, char c)
{
	if(s && s->write(s, &c, 1) == 1)
		return c;
	return -1;
}

int
sputs(Ostream *s, char *sz)
{
	return swrite(s, sz, strlen(sz));
}

int
swrite(Ostream *s, const void *buf, int len)
{
	if(s)
		return s->write(s, buf, len);
	return -1;
}


int
siseek(Istream *s, int ofs, int whence)
{
	if(!s)
		return -1;
	switch(whence) {
	case 0:	s->pos = ofs; break;
	case 1:	s->pos += ofs; break;
	case 2: s->pos = s->size + ofs; break;
	}
	if(s->pos < 0) {
		s->pos = 0;
		return -1;
	}
	if(s->pos > s->size) {
		s->pos = s->size;
		return -1;
	}
	return s->pos;
}

int
soseek(Ostream *s, int ofs, int whence)
{
	/* this may be illegal with some compilers, but for ours
	 * it reduces redundant code
	 */
	return siseek(s, ofs, whence);
}


int
siflush(Istream *s)
{
	return sread(s, nil, 0);
}

int
soflush(Ostream *s)
{
	return swrite(s, nil, 0);
}


int
siclose(Istream *s)
{
	if(!s)
		return -1;
	if(s->close)
		return s->close(s);
	return 0;
}

int
soclose(Istream *s)
{
	if(!s)
		return -1;
	if(s->close)
		return s->close(s);
	return 0;
}

uchar*
simmap(Istream *s)
{
	if(!s || !s->mmap)
		return nil;
	return s->mmap(s);
}

uchar*
sommap(Ostream *s)
{
	if(!s || !s->mmap)
		return nil;
	return s->mmap(s);
}

