#include "dat.h"
#include "fns.h"
#include "error.h"
#include "svp.h"
#include "devaudio.h"
#include "devaudio-Unixware.h"
#include "devaudio-tbls.c"

#define DEBUG 0
static int debug = DEBUG;

void
audio_file_init(void)
{
	return;
}

int
audio_file_open(Chan *c, int omode)
{
	error(Eperm);
	return 0;
}

long
audio_file_read(Chan *c, void *va, long count, long offset)
{
	error(Eperm);
	return -1;
}

long
audio_file_write(Chan *c, void *va, long count, long offset)
{
	error(Eperm);
	return -1;
}

void
audio_file_close(Chan *c)
{
	error(Eperm);
	return ;
}

void
audio_ctl_init(void)
{
	return;
}

int
audio_ctl_open(Chan *c, int omode)
{
	error(Eperm);
	return 0;
}

long
audio_ctl_read(Chan *c, void *va, long count, long offset)
{
	error(Eperm);
	return -1;
}

long
audio_ctl_write(Chan *c, void *va, long count, long offset)
{
	error(Eperm);
	return -1;
}

void
audio_ctl_close(Chan *c)
{
	error(Eperm);
	return;
}
