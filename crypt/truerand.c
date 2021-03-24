#include "lib9.h"
#include "kernel.h"
#include <libcrypt.h>

ulong
truerand(void)
{
	ulong x;
	int randfd;

	randfd = kopen("/dev/random", OREAD);
	if(randfd < 0)
		handle_exception(CRITICAL, "can't open /dev/random");
	if(kread(randfd, &x, sizeof(x)) != sizeof(x))
		handle_exception(CRITICAL, "can't read /dev/random");
	kclose(randfd);
	return x;
}

int
n_truerand(int n)
{
	int slop, v;
	
	slop = 0x7FFFFFFF % n;
	do {
		v = truerand() >> 1;
	} while (v <= slop);
	return v % n;
}
