#include	<lib9.h>
#include	"dat.h"
#include	"fns.h"
#include	"bpi.h"
#include	"bpistream.h"


static int
dem_read(Istream *_s, void *adr, int len)
{
	BpiIstream *s = (BpiIstream*)_s;
	int n;
	if(s->pos != s->lastpos) 
		bpi->seek(s->fd, s->pos);

	n = bpi->read(s->fd, adr, len);
	s->lastpos = (s->pos += n);
	return n;
}

static int
dem_write(Ostream *_s, const void *adr, int len)
{
	BpiOstream *s = (BpiOstream*)_s;
	int n;
	if(s->pos != s->lastpos)
		bpi->seek(s->fd, s->pos);

	n = bpi->write(s->fd, adr, len);
	s->lastpos = (s->pos += n);
	return n;
}


static int
dem_close(Istream *_s)
{
	BpiIstream *s = (BpiIstream*)_s;
	if(bpi->close(s->fd) != 0) {
		char estr[ERRLEN];
		sprint(estr, "%d: close failed", s->fd);
		error(estr);
		return -1;
	} else
		return 0;
}

int
bpi_openi(BpiIstream *s, const char *fname)
{
	BpStat stbuf;

	if(!(s->fd = bpi->open(fname, BP_O_RDONLY))) {
		char estr[ERRLEN];
		sprint(estr, "%s: cannot open", fname);
		error(estr);
		return -1;
	}

	s->read = dem_read;
	s->close = dem_close;
	s->mmap = nil;
	if(bpi->fstat(s->fd, &stbuf) < 0)
		s->size = 0;
	else
		s->size = stbuf.size;
	s->pos = 0;
	return 0;
}

int
bpi_openo(BpiIstream *s, const char *fname)
{
	if(!(s->fd = bpi->open(fname, BP_O_WRONLY))) {
		char estr[ERRLEN];
		sprint(estr, "%s: cannot create", fname);
		error(estr);
		return -1;
	}

	s->read = dem_write;
	s->close = dem_close;
	s->mmap = nil;
	s->size = (1<<31)-1;
	s->pos = 0;
	return 0;
}

static Istream*
dem_sd_openi(const char *args)
{
	BpiIstream *s = (BpiIstream*)malloc(sizeof(BpiIstream));
	
	if(!*args || bpi_openi(s, args) < 0) {
		free(s);
		return nil;
	}
	return s;
}

static Ostream*
dem_sd_openo(const char *args)
{
	BpiIstream *s = (BpiIstream*)malloc(sizeof(BpiIstream));
	
	if(!*args || bpi_openo(s, args) < 0) {
		free(s);
		return nil;
	}
	return s;
}


static StreamDev dem_sd = {
	"D",		// Demon/Debug
	dem_sd_openi,
	dem_sd_openo,
};


void
bpistreamlink(void)
{
	addstreamdevlink(&dem_sd);
}

