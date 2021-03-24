#include "lib9.h"


int
runestrlen(Rune *s)
{
	int i;

	i = 0;
	while(*s++)
		i++;
	return i;
}
