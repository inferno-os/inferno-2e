#include "svp.h"

int
svpmatchs(svp_t* t, int n, char* s, unsigned long *v)
{
int i;

	if(! t || ! s)
		return 0;

	for(i = 0; i< n; i++, t++) {
		if(! t->s)
			return 0;

		if(! strncmp(t->s, s, strlen(t->s))) {
			*v = t->v;
			return 1;
		}
	}
	return 0;
}

int
svpmatchv(svp_t* t, int n, char** s, unsigned long v)
{
int i;

	if(! t)
		return 0;

	for(i = 0; i < n; i++, t++) {
		if(t->v == v) {
			if(! t->s)
				return 0;
			*s = t->s;
			return 1;
		}
	}
	return 0;
}

int
svpindexv(svp_t* t, int n, unsigned long v)
{
int i = 0;

	if(! t)
		return -1;

	for(i = 0; i < n; i++, t++) {
		if(t->v == v) 
			return i;
	}
	return -1;
}

int
svpindexs(svp_t* t, int n, char* s)
{
int i= 0;

	if(! t || ! s)
		return -1;

	for(i = 0; i < n; i++, t++) {
		if(! t->s)
			return -1;
		if(! strncmp(t->s, s, strlen(t->s))) 
			return i;
	}
	return -1;
}

int 
sval(char* buf, unsigned long* v, unsigned long max, unsigned long min)
{
unsigned long val = strtoul(buf, 0, 10);

	if((val > max) || (val < min))
		return 0;
	*v = val;
	return 1;
}
