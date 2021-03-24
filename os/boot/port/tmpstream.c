#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "bpi.h"

static int tmpchanopen(BPChan*, int);
static void tmpchanclose(BPChan*);
static int tmpchanread(BPChan*, uchar *buf, long n, long offset);
static int tmpchanwrite(BPChan*, uchar *buf, long n, long offset);

static Queue tmpq;
static BPChan tmpchan = {
	{"tmp", "none", "none" , {0}, 0666},
	 tmpchanopen, tmpchanclose, tmpchanread, tmpchanwrite,
	&tmpq,
};

static int
tmp_read(Istream*, void *buf, int n)
{
	return qread(&tmpq, buf, n, 0);
}

static int
tmp_write(Ostream*, void *buf, int n)
{
	n = qwrite(&tmpq, buf, n);
	tmpchan.d.length += n;
	return n;
}

static Istream*
tmp_sd_openi(const char*)
{
	Istream *s = (Istream*)malloc(sizeof(*s));
	if(s == nil)
		return nil;

	memset(s, 0, sizeof(*s));
	s->size = tmpchan.d.length;
	s->read = tmp_read;

	return s;
}

static Ostream*
tmp_sd_openo(const char*)
{
	Ostream *s = (Istream*)malloc(sizeof(*s));
	if(s == nil)
		return nil;

	memset(s, 0, sizeof(*s));
	s->write = tmp_write;

	return s;
}

static StreamDev tmp_sd =
{
	"T",
	tmp_sd_openi,
	tmp_sd_openo,
};

static int
cmd_freetmp(int, char**, int*)
{
	qflush(&tmpq);
	tmpchan.d.length = 0;
	return 0;
}

static int
tmpchanopen(BPChan *c, int)
{
	c->d.length = qlen(&tmpq);
	return 0;
}

static void
tmpchanclose(BPChan*)
{
	statusclear();
}

static int
tmpchanread(BPChan *c, uchar *buf, long n, long offset)
{
	n = qread((Queue*)c->aux, buf, n, 0);
	status("tmp read", offset, c->d.length);
	return n;
}

static int
tmpchanwrite(BPChan *c, uchar *buf, long n, long offset)
{
	n = qwrite((Queue*)c->aux, buf, n);
	c->d.length += n;
	status("tmp write", offset, 0);
	return n;
}


void
tmpstreamlink(void)
{
	addstreamdevlink(&tmp_sd);
	addcmd('F', cmd_freetmp, 0, 0, "flush tmp");
	if(bpi->file2chan)
		bpi->file2chan(&tmpchan);
}
