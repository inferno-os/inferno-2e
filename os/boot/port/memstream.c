#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "memstream.h"

static int
mem_read(Istream *_s, void *adr, int len)
{
	MemIstream *s = (MemIstream*)_s;
	if(len > s->size-s->pos)
		len = s->size-s->pos;
	if(len > 0) {
		memmove(adr, &s->base[s->pos], len);
		s->pos += len;
	}
	return len;
}

static int
mem_write(Ostream *_s, const void *adr, int len)
{
	MemOstream *s = (MemOstream*)_s;
	if(len > s->size-s->pos)
		len = s->size-s->pos;
	if(len > 0) {
		memmove(&s->base[s->pos], adr, len);
		s->pos += len;
	}
	return len;
}

static uchar*
mem_mmap(Ostream *_s)
{
	MemOstream *s = (MemOstream*)_s;
	return s->base;
}

int
mem_openi(MemIstream *s, const void *base, ulong len)
{
	s->read = mem_read;
	s->close = nil;
	s->mmap = mem_mmap;
	s->base = (const uchar*)base;
	s->pos = 0;
	s->size = len;
	return 0;
}

int
mem_openo(MemOstream *s, void *base, ulong len)
{
	s->write = mem_write;
	s->close = nil;
	s->mmap = mem_mmap;
	s->base = (uchar*)base;
	s->pos = 0;
	s->size = len;
	return 0;
}


static Istream*
mem_sd_openi(const char *args)
{
	MemIstream *s = (MemIstream*)malloc(sizeof(MemIstream));
	ulong adr = strtoul(args, 0, 16);
		
	if(mem_openi(s, (void*)adr, 0xffffffff-adr) < 0) {
		free(s);
		return nil;
	}
	return s;
}

static Ostream*
mem_sd_openo(const char *args)
{
	MemOstream *s = (MemOstream*)malloc(sizeof(MemOstream));
	ulong adr = strtoul(args, 0, 16);

	if(mem_openo(s, (void*)adr, 0xffffffff-adr) < 0) {
		free(s);
		return nil;
	}
	return s;
}

static StreamDev mem_sd = {
	"m",
	mem_sd_openi,
	mem_sd_openo,
};


void
memstreamlink(void)
{
	addstreamdevlink(&mem_sd);
}

