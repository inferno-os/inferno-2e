#include <u.h>
#include "dat.h"
#include "fns.h"

static ulong top;
static ulong bottom;

void*
malloc(int size)
{
	top = (top-size-sizeof(int))&~3;
	if(top < bottom)
		return nil;
	*(int*)top = size;
	return (void*)(top+sizeof(int));
}


void
free(void* ptr)
{
	if((ulong)ptr == top+sizeof(int))
		top += sizeof(int)+*(int*)top;
}


void
mallocinit(ulong newbottom, ulong newtop)
{
	top = newtop;
	bottom = newbottom;
}

