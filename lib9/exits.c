#include "lib9.h"

void
exits(const char *s)
{
	if(s == 0 || *s == 0)
		exit(0);
	exit(1);
}

void
_exits(const char *s)
{
	if(s == 0 || *s == 0)
		_exit(0);
	_exit(1);
}
