/*
 *        CryptoLib Bignum Utilities
 *        coded by Jack Lacy December, 1991
 *
 *        Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

static void ctox(uchar *, int, uchar *);

extern int msb_table8[];

int bigNumsAllocated = 0;


NumType zero_data[1] = {0};
NumType one_data[1] = {1};
NumType two_data[1] = {2};
Bignum bigzero = {POS, 1, 1, zero_data};
Bignum bigone = {POS, 1, 1, one_data};
Bignum bigtwo = {POS, 1, 1, two_data};
BigInt zero = &bigzero;
BigInt one = &bigone;
BigInt two = &bigtwo;

BigInt
itobig(NumType i)
{
	BigInt big;
	
	big = (BigInt)crypt_malloc(sizeof(Bignum));
	
	NUM(big) = (BigData)crypt_malloc(sizeof(NumType));
	NUM(big)[0] = (NumType)i;
	SPACE(big) = 1;
	LENGTH(big) = 1;
	SIGN(big) = POS;
	
	bigNumsAllocated++;
	return big;
}


void
freeBignum(BigInt a)
{
	int i;

	if(a == 0)
		return;
	i = (int)SPACE(a);
	while (--i >= 0)
		NUM(a)[i] = 0;
	crypt_free((char *)NUM(a));
	crypt_free((char *)a);
	bigNumsAllocated--;
}

#define NBITS(a) (((LENGTH(a) - 1) * NumTypeBits) + msb((NumType)NUM(a)[LENGTH(a)-1]))
/* return number of bits in BigInt */
int
bigBits(BigInt a)
{
	return (int)NBITS(a);
}

int
bigBytes(BigInt a)
{
	return (int)(LENGTH(a)*sizeof(NumType));
}

Sign
bigTest(BigInt a)
{
	return SIGN(a);
}

NumType
msb(NumType a)
{
	ushort ahi, alo;
	
	if (a & (ulong)0x80000000)
		return 32;
	
	ahi = (ushort)(a >> 16);
	alo = (ushort)(a & 0xFFFF);
	
	if (ahi) {
		alo = ahi & (ushort)0xFF;
		ahi = (ushort)(ahi >> 8);
		if (ahi)
			return (NumType)(24 + msb_table8[ahi]);
		else
			return (NumType)(16 + msb_table8[alo]);
	}
	else {
		ahi = (ushort)(alo >> 8);
		alo = (ushort)(alo & 0xFF);
		if (ahi)
			return (NumType)(8 + msb_table8[ahi]);
		else
			return (NumType)(msb_table8[alo]);
	}
}

Boolean
even(BigInt b)
{
	return EVEN(b);
}

Boolean
odd(BigInt b)
{
	return ODD(b);
}

/*
 *  buf is high order byte first
 *
 * Added to convert from 1 or 2's complement representation by David Rubin, March 1999.
 * Sign < 0 => convert buf from 2's representation, else convert buf from 1's representation.
 */
void
bufToBig_sm(int sign, uchar *buf, int len, BigInt big)
{
	uchar *cp, *ep;
	int s;
	BigData bp;
	NumType m;
	NumType newlen;
	
	newlen = (len + sizeof(NumType) - 1)/sizeof(NumType);
	GUARANTEE(big, (ulong)newlen);
	LENGTH(big) = (ulong)newlen;

	bp = NUM(big);

	memset(bp, 0, newlen*sizeof(NumType));

	ep = buf;

	// convert from ones complement to internal representation
	for(cp = ep + len - 1; cp >= ep;){
		m = 0;
		for(s = 0; s < NumTypeBits && cp >= ep; s += 8)
			m |= (*cp--)<<s;
		*bp++ = m;
	}
	trim(big);
	if (sign == 0)
		handle_exception(CRITICAL, "bufToBig_sm: bad sign");
	if (sign < 0)
		SIGN(big) = NEG;
}

/*
 *  buf is high order byte first
 *
 * Modified to convert from 2's complement representation
 * by David Rubin, March 1999.
 */
void
bufToBig(uchar *buf, int len, BigInt big)
{
	uchar *cp, *ep;
	int s;
	BigData bp;
	NumType m;
	NumType newlen;
	
	newlen = (len + sizeof(NumType) - 1)/sizeof(NumType);
	GUARANTEE(big, (ulong)newlen);
	LENGTH(big) = (ulong)newlen;

	bp = NUM(big);

	memset(bp, 0, newlen*sizeof(NumType));

	ep = buf;

	// convert from twos complement to internal representation
	if (*buf & 0x80) {
		uchar *fnz, b;

		// point fnz to the first non-zero byte, the last byte which can change by adding 1
		for (fnz=ep+len-1; fnz>buf; fnz--)
			if (*fnz != 0)
				break;

		// invert and add 1
		for(cp = ep + len - 1; cp >= ep;){
			m = 0;
			for(s = 0; s < NumTypeBits && cp >= ep; s += 8) {
				b = cp>=fnz ? -*cp : ~*cp;
				m |= b<<s;
				cp--;
			}
			*bp++ = m;
		}

		SIGN(big) = NEG;
	}
	else {
		// convert from ones complement to internal representation
		for(cp = ep + len - 1; cp >= ep;){
			m = 0;
			for(s = 0; s < NumTypeBits && cp >= ep; s += 8)
				m |= (*cp--)<<s;
			*bp++ = m;
		}
	}

	trim(big);
}

/*
 *  buf is high order byte first
 *
 * Modified to convert buf to the 'smallest' 2's complement representation
 * by David Rubin, March 1999. Note: illegal encodings are not checked.
 */
int
bigToBuf(BigInt big, int bufsize, uchar *buf)
{
	BigData bp, ep;
	NumType ss;
	int s;
	uchar *cp, *p, k;
	
	if (LENGTH(big)*sizeof(NumType) > bufsize)
		handle_exception(CRITICAL, "BigToBuf: Buffer is too small.");

	memset(buf, 0, bufsize);
	
	ep = NUM(big);
	bp = ep + LENGTH(big) - 1;
	cp = buf;

	// if the most significant bit is 1, we need to allocate more space in buf
	if (*bp & (0x01 << NumTypeBits-1)) {
		if (1+LENGTH(big)*sizeof(NumType) > bufsize)
			handle_exception(CRITICAL, "BigToBuf: Buffer is too small.");
		*cp++ = 0;
	}

	// store big as a big-endian ones complement byte array
	for(;bp >= ep; bp--){
		ss = *bp;
		for(s = NumTypeBits - 8; s >= 0; s -= 8)
			*cp++ = ss >> s;
	}

	// change buf to a big-endian twos complement byte array
	if (SIGN(big) == NEG) {
		uchar *fnz;

		// point fnz to the first non-zero byte, the last byte which can change by adding 1
		for (fnz=cp-1; fnz>buf; fnz--)
			if (*fnz != 0)
				break;

		// invert and add 1
		for (p=cp-1; p>=buf; p--)
			*p = (p>=fnz ? -*p : ~*p);
	}

	// pullup buffer: remove leading duplicate bytes (0x00 or 0xff) to attain smallest byte representation
	p = cp;
	k = *buf;
	if (k==0x00 || k==0xff) {
		// find first byte different from k
		for (p=buf+1; p<cp; p++)
			if (*p != k)
				break;

		if (k==0xff && !(*p & 0x80) || k==0x00 && (*p & 0x80) || p==cp)
			p--;

		k=p-buf;
		for (p=buf; p<cp-k; p++)
			*p = *(p+k);
	}

	return p - buf;
}

static int
trans_c2x(char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return 0;
}

BigInt
atobig(char *a)
{
	BigInt big;
	uchar *buf, *p;
	int i;
	int size;
	
	big = bigInit(0);

	/* convert hex to byte string */
	i = strlen(a);
	size = (i+1)/2;
	p = buf = crypt_malloc(size);
	if(i&1)
		*p++ = trans_c2x(*a++);
	while(*a){
		i = trans_c2x(*a++)<<4;
		*p++ = i | trans_c2x(*a++);
	}

	/* convert byte string to big */
	bufToBig(buf, size, big);
	SIGN(big) = POS;

	crypt_free(buf);
	return big;
}

/*
 *  convert a big to an ascii string in some base
 */
static int
__bigtoc(BigInt b)
{
	int i;

	i = *(NUM(b) + LENGTH(b) - 1);
	if(i < 10)
		return '0' + i;
	return 'A' + i - 10;
}
int
bigtostr(BigInt big, char *buf, int len, int base)
{
	BigInt b, d, q, r, div, one;
	char *cp, *ecp;
	int digits, neg;

	if(base == 0)
		base = 10;
	if(base < 0 || base > 36)
		return -1;
	if(len <= 1)
		return -1;

	cp = buf;
	if(SIGN(big) == NEG){
		*cp++ = '-';
		len--;
		neg = 1;
	} else
		neg = 0;

	b = bigInit(0);
	d = bigInit(base);
	div = bigInit(base);
	q = bigInit(0);
	r = bigInit(0);
	one = bigInit(1);
	bigCopy(big, b);
	for(digits = 1; bigCompare(b, d) >= 0; digits++){
		bigMultiply(d, div, q);
		bigCopy(q, d);
	}
	if(digits > len - 1)
		return -1;

	for(ecp = cp + digits; cp < ecp; cp++){
		bigDivide(d, div, q, r);
		bigCopy(q, d);
		bigDivide(b, d, q, b);
		*cp = __bigtoc(q);
	}
	*cp = 0;

	freeBignum(b);
	freeBignum(d);
	freeBignum(div);
	freeBignum(q);
	freeBignum(r);
	freeBignum(one);

	return digits + neg;
}

/*
 *  convert an ascii string in a specified base to a big
 */
static int
__ctoi(int x, int base)
{
	if(base <= 10){
		if(x >= '0' && x < '0' + base)
			return x - '0';
		return -1;
	}

	base -= 10;
	if(x >= '0' && x <= '9')
		return x - '0';
	if(x >= 'A' && x < 'A' + base)
		return x - 'A' + 10;
	if(x >= 'a' && x < 'a' + base)
		return x - 'a' + 10;
	return -1;
}
BigInt
strtobig(char *p, char **pp, int base)
{
	BigInt big, dig, tmp;
	int c;
	int sign;
	NumType b;

	b = base;
	big = bigInit(0);
	tmp = bigInit(0);
	dig = bigInit(0);

	sign = POS;
	if(*p == '+')
		p++;
	else if(*p == '-'){
		p++;
		sign = NEG;
	}

	while((c = __ctoi(*p, base)) >= 0){
		numtype_bigmult(big, b, tmp, 0);
		NUM(dig)[0] = c;
		bigAdd(tmp, dig, big);
		reset_big(tmp, 0);
		p++;
	}
	SIGN(big) = sign;
	freeBignum(tmp);
	freeBignum(dig);

	if(pp)
		*pp = p;

	return big;
}

int
bigconv(va_list *arg, void *f)
{
	char* buf;
	BigInt big;
	BigData bp, ep;
	int i;
	char *cp;
	
	big = va_arg(*arg, BigInt);

	i = LENGTH(big);
	cp = buf = crypt_malloc(i*2*sizeof(NumType) + 1);

	bp = NUM(big) + i - 1;
	for(ep = NUM(big); bp >= ep; bp--) {
		sprint(cp, "%*.*lux", 2*sizeof(NumType), 2*sizeof(NumType), *bp);
		cp += 2*sizeof(NumType);
	}
	*cp = '\0';

	strconv(buf, f);
	crypt_free(buf);
	return 0;
}

void
trim(BigInt big)
{
	while ((NUM(big)[LENGTH(big)-1] == 0) && LENGTH(big) > 1)
		LENGTH(big)--;

	/* eliminate negative 0 */
	if(LENGTH(big) == 1 && NUM(big)[0] == 0)
		SIGN(big) = POS;
}

void
reset_big(BigInt a, NumType u)
{
	BigData ap;
	
	ap = NUM(a);
	SIGN(a) = POS;
	LENGTH(a) = 1;
	ap[0] = u;
}
