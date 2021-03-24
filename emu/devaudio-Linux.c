/*
 * stubs for Linux; some day I'll rewrite this around the portable part of devaudio-Hp.c
 */
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "svp.h"
#include "devaudio.h"

/* junk to make this nightmare link */

#define 	Audio_8Bit_Val		8
#define 	Audio_16Bit_Val		16

#define 	Audio_Mono_Val		1
#define 	Audio_Stereo_Val	2

#define 	Audio_Mic_Val		1
#define 	Audio_Linein_Val	2

#define		Audio_Speaker_Val	1
#define		Audio_Headphone_Val	2
#define		Audio_Lineout_Val	3

#define 	Audio_Pcm_Val		0
#define 	Audio_Ulaw_Val		1
#define 	Audio_Alaw_Val		2

#define 	Audio_8K_Val		8000
#define 	Audio_11K_Val		11025
#define 	Audio_22K_Val		22050
#define 	Audio_44K_Val		44100

#include "devaudio-tbls.c"

#define DEBUG 0
static int debug = DEBUG;

void
audio_file_init(void)
{
	USED(debug);

	return;
}

int
audio_file_open(Chan *c, int omode)
{
	USED(c); USED(omode);

	error(Eperm);
	return 0;
}

long
audio_file_read(Chan *c, void *va, long count, long offset)
{
	USED(c); USED(va); USED(count); USED(offset);

	error(Eperm);
	return -1;
}

long
audio_file_write(Chan *c, void *va, long count, long offset)
{
	USED(c); USED(va); USED(count); USED(offset);

	error(Eperm);
	return -1;
}

void
audio_file_close(Chan *c)
{
	USED(c);

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
	USED(c); USED(omode);

	error(Eperm);
	return 0;
}

long
audio_ctl_read(Chan *c, void *va, long count, long offset)
{
	USED(c); USED(va); USED(count); USED(offset);

	error(Eperm);
	return -1;
}

long
audio_ctl_write(Chan *c, void *va, long count, long offset)
{
	USED(c); USED(va); USED(count); USED(offset);

	error(Eperm);
	return -1;
}

void
audio_ctl_close(Chan *c)
{
	USED(c);

	error(Eperm);
	return;
}
