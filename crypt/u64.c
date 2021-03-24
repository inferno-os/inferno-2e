#include "lib9.h"

static uchar t64d[256];
static char t64e[64];

void	fatal(char *fmt, ...);

static void
init64(void)
{
	int c, i;

	/* Equipotent initialization makes races harmless. */
	memset(t64d, 255, '0');
	memset(t64d + '9' + 1, 255, 'A' - '9' - 1);
	memset(t64d + 'Z' + 1, 255, 'a' - 'Z' - 1);
	memset(t64d + 'z' + 1, 255, sizeof(t64d) - 'z' - 1);
	i = 0;
	for(c = 'A'; c <= 'Z'; c++){
		t64e[i] = c;
		t64d[c] = i++;
	}
	for(c = 'a'; c <= 'z'; c++){
		t64e[i] = c;
		t64d[c] = i++;
	}
	for(c = '0'; c <= '9'; c++){
		t64e[i] = c;
		t64d[c] = i++;
	}
	t64e[i] = '+';
	t64d['+'] = i++;
	t64e[i] = '/';
	t64d['/'] = i;
}

int
dec64(uchar *out, int lim, char *in, int n)
{
	ulong b24;
	uchar *start = out;
	uchar *e = out + lim;
	int i, c;

	if(t64e[0] == 0)
		init64();

	b24 = 0;
	i = 0;
	while(n-- > 0){
 
		c = t64d[*in++];
		if(c == 255)
			continue;
		switch(i){
		case 0:
			b24 = c<<18;
			break;
		case 1:
			b24 |= c<<12;
			break;
		case 2:
			b24 |= c<<6;
			break;
		case 3:
			if(out + 3 > e)
				goto exhausted;

			b24 |= c;
			*out++ = b24>>16;
			*out++ = b24>>8;
			*out++ = b24;
			i = -1;
			break;
		}
		i++;
	}
	switch(i){
	case 2:
		if(out + 1 > e)
			goto exhausted;
		*out++ = b24>>16;
		break;
	case 3:
		if(out + 2 > e)
			goto exhausted;
		*out++ = b24>>16;
		*out++ = b24>>8;
		break;
	}
exhausted:
	return out - start;
}

int
enc64(char *out, int lim, uchar *in, int n)
{
	int i;
	ulong b24;
	char *start = out;
	char *e = out + lim;

	if(t64e[0] == 0)
		init64();
	for(i = n/3; i > 0; i--){
		b24 = (*in++)<<16;
		b24 |= (*in++)<<8;
		b24 |= *in++;
		if(out + 5 >= e)
			goto exhausted;
		*out++ = t64e[(b24>>18)];
		*out++ = t64e[(b24>>12)&0x3f];
		*out++ = t64e[(b24>>6)&0x3f];
		*out++ = t64e[(b24)&0x3f];
	}

	switch(n%3){
	case 2:
		b24 = (*in++)<<16;
		b24 |= (*in)<<8;
		if(out + 4 >= e)
			goto exhausted;
		*out++ = t64e[(b24>>18)];
		*out++ = t64e[(b24>>12)&0x3f];
		*out++ = t64e[(b24>>6)&0x3f];
		break;
	case 1:
		b24 = (*in)<<16;
		if(out + 4 >= e)
			goto exhausted;
		*out++ = t64e[(b24>>18)];
		*out++ = t64e[(b24>>12)&0x3f];
		*out++ = '=';
		break;
	}
exhausted:
	*out++ = '=';
	*out = 0;
	return out - start;
}
