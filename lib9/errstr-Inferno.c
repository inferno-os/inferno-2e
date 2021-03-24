#include "lib9.h"

void
oserrstr(char *buf)
{
	*buf = 0;
	errstr(buf);
}
