#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "zerostream.h"

static int
zero_read(Istream *s, void *adr, int len)
{
	if(len > s->size-s->pos)
		len = s->size-s->pos;
	if(len > 0) {
		memset(adr, 0, len);
		s->pos += len;
	}
	return len;
}


int
zero_openi(Istream *s, const char *pn)
{
	if(pn[0])
		s->size = strtol(pn, 0, 16);
	else
		s->size = (ulong)-1;
	s->read = zero_read;
	s->close = 0;
	s->mmap = 0;
	s->pos = 0;
	return 0;
}

static Istream*
zero_sd_openi(const char *args)
{
	Istream *s = (Istream*)malloc(sizeof(Istream));
	
	if(!*args || zero_openi(s, args) < 0) {
		free(s);
		return nil;
	}
	return s;
}


static StreamDev zero_sd = {
	"Z",
	zero_sd_openi,
	nil,
};


void
zerostreamlink(void)
{
	addstreamdevlink(&zero_sd);
}

