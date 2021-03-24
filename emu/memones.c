#include "dat.h"
#include "image.h"
#include "memimage.h"

static ulong	onesbits = ~0;

static Memdata onesdata = {
	nil,
	&onesbits
};

static Memimage	xones =
{
	{ 0, 0, 1, 1 },
	{ -100000, -100000, 100000, 100000 },
	3,
	1,
	&onesdata,
	0,
	1
};
Memimage *memones = &xones;
