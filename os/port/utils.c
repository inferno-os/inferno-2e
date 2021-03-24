#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

/*
 *	These are in parse.c in Brazil...
 */
int
parsefields(char *lp, char **fields, int n, char *sep)
{
	int i;

	for(i=0; lp && *lp && i<n; i++){
		while(*lp && strchr(sep, *lp) != 0)
			*lp++=0;
		if(*lp == 0)
			break;
		fields[i]=lp;
		while(*lp && strchr(sep, *lp) == 0)
			lp++;
	}
	return i;
}

/*
 *  parse a command written to a device
 */
Cmdbuf*
parsecmd(char *p, int n)
{
	Cmdbuf *cb;

	cb = smalloc(sizeof(*cb));
	
	if(n > sizeof(cb->buf)-1)
		n = sizeof(cb->buf)-1;
	memmove(cb->buf, p, n);
	if(n > 0 && cb->buf[n-1] == '\n')
		n--;
	cb->buf[n] = '\0';
	cb->nf = parsefields(cb->buf, cb->f, nelem(cb->f), " ");
	return cb;
}

/*
 *	Should be in libkern???
 */
vlong
strtoll(char *nptr, char **endptr, int base)
{
	char *p;
	vlong n;
	int c, v, neg, ndig;

	p = nptr;
	neg = 0;
	n = 0;
	ndig = 0;

	/*
	 * White space
	 */
	for(;;p++){
		switch(*p){
		case ' ':
		case '\t':
		case '\n':
		case '\f':
		case '\r':
		case '\v':
			continue;
		}
		break;
	}

	/*
	 * Sign
	 */
	if(*p=='-' || *p=='+')
		if(*p++ == '-')
			neg = 1;

	/*
	 * Base
	 */
	if(base==0){
		if(*p != '0')
			base = 10;
		else{
			base = 8;
			if(p[1]=='x' || p[1]=='X'){
				p += 2;
				base = 16;
			}
		}
	}else if(base==16 && *p=='0'){
		if(p[1]=='x' || p[1]=='X')
			p += 2;
	}else if(base<0 || 36<base)
		goto Return;

	/*
	 * Non-empty sequence of digits
	 */
	for(;; p++,ndig++){
		c = *p;
		v = base;
		if('0'<=c && c<='9')
			v = c - '0';
		else if('a'<=c && c<='z')
			v = c - 'a' + 10;
		else if('A'<=c && c<='Z')
			v = c - 'A' + 10;
		if(v >= base)
			break;
		n = n*base + v;
	}
    Return:
	if(ndig == 0)
		p = nptr;
	if(endptr)
		*endptr = p;
	if(neg)
		return -n;
	return n;

}
