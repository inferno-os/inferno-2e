#include "lib9.h"
#include "mathi.h"
extern char	*dtoa(double, int, int, int *, int *, char **);

enum
{
	NONE	= -1000,
	FPLUS	= 1<<0,
	FMINUS	= 1<<1,
	FSHARP	= 1<<2
};  /* extracted from lib9/doprint.c */

int
gfltconv(va_list *arg, Fconv *f)
{
	int flags = f->f3;
	int width = f->f1;
	int precision = f->f2;
	char afmt = f->chr;
	double d;
	int echr, exponent, sign, ndig, nout, i;
	char *digits, *edigits, fmt, ebuf[512], *eptr;
	static char out[1024], *pout;
	int SPACE = 0, ZPAD = 0;  /* for possible future use */

	d = va_arg(*arg, double);
	echr = 'e';
	fmt = afmt;
	if(precision < 0) precision = 6;
	else if(precision > 20 ) precision = 20;
	if(width >= (int)sizeof(out)) width = sizeof(out)-1;
	switch(fmt){
	case 'f':
		digits = dtoa(d, 3, precision, &exponent, &sign, &edigits);
		break;
	case 'E':
		echr = 'E';
		fmt = 'e';
		/* fall through */
	case 'e':
		digits = dtoa(d, 2, 1+precision, &exponent, &sign, &edigits);
		break;
	case 'G':
		echr = 'E';
		/* fall through */
	default:
	case 'g':
		if(f->f2<=0 && width<=0){
			g_fmt(out, d, echr);
			strconv(out, f);
			return 0;
		}
		if (precision > 0)
			digits = dtoa(d, 2, precision, &exponent, &sign, &edigits);
		else {
			digits = dtoa(d, 0, precision, &exponent, &sign, &edigits);
			precision = edigits - digits;
			if (exponent > precision && exponent <= precision + 4)
				precision = exponent;
			}
		if(exponent >= -3 && exponent <= precision){
			fmt = 'f';
			precision -= exponent;
		}else{
			fmt = 'e';
			--precision;
		}
		break;
	}
	if (exponent == 9999) {
		/* Infinity or Nan */
		precision = 0;
		exponent = edigits - digits;
		fmt = 'f';
	}
	ndig = edigits-digits;
	if((afmt=='g' || afmt=='G') && !(flags&FSHARP)){ /* knock off trailing zeros */
		if(fmt == 'f'){
			if(precision+exponent > ndig) {
				precision = ndig - exponent;
				if(precision < 0)
					precision = 0;
			}
		}
		else{
			if(precision > ndig-1) precision = ndig-1;
		}
	}
	nout = precision;				/* digits after decimal point */
	if(precision!=0 || flags&FSHARP) nout++;		/* decimal point */
	if(fmt=='f' && exponent>0) nout += exponent;	/* digits before decimal point */
	else nout++;					/* there's always at least one */
	if(sign || flags&(SPACE|FPLUS)) nout++;		/* sign */
	eptr = ebuf;
	if(fmt != 'f'){					/* exponent */
		for(i=exponent<=0?1-exponent:exponent-1; i; i/=10)
			*eptr++ = '0' + i%10;
		while(eptr<ebuf+2) *eptr++ = '0';
		nout += eptr-ebuf+2;			/* e+99 */
	}
	pout = out;
	if(!(flags&ZPAD) && !(flags&FMINUS))
		while(nout < width){
			*pout++ = ' ';
			nout++;
		}
	if(sign) *pout++ = '-';
	else if(flags&FPLUS) *pout++ = '+';
	else if(flags&SPACE) *pout++ = ' ';
	if(flags&ZPAD)
		while(nout < width){
			*pout++ = '0';
			nout++;
		}
	if(fmt == 'f'){
		for(i=0; i<exponent; i++) *pout++ = i<ndig?digits[i]:'0';
		if(i == 0) *pout++ = '0';
		if(precision>0 || flags&FSHARP) *pout++ = '.';
		for(i=0; i!=precision; i++)
			*pout++ = 0<=i+exponent && i+exponent<ndig?digits[i+exponent]:'0';
	}
	else{
		*pout++ = digits[0];
		if(precision>0 || flags&FSHARP) *pout++ = '.';
		for(i=0; i!=precision; i++) *pout++ = i<ndig-1?digits[i+1]:'0';
	}
	if(fmt != 'f'){
		*pout++ = echr;
		*pout++ = exponent<=0?'-':'+';
		while(eptr>ebuf) *pout++ = *--eptr;
	}
	while(nout < width){
		*pout++ = ' ';
		nout++;
	}
	*pout = 0;
	f->f2 = NONE;
	strconv(out, f);
	return 0;
}
