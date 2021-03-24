#include "lib9.h"
#include <sys/types.h>
#include <fcntl.h>

long
seek(int fd, long where, int from)
{
	return lseek(fd, where, from);
}
