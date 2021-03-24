#include "lib9.h"
#include "kernel.h"
#include <isa.h>
#include "interp.h"
#include "runt.h"
#include "libcrypt.h"
#include "pool.h"
#include "../keyring/keys.h"

extern Type	*TIPint;

Keyring_IPint*
newIPint(BigInt b)
{
	Heap *h;
	IPint *ip;

	h = heap(TIPint);
	ip = H2D(IPint*, h);
	ip->b = b;
	return (Keyring_IPint*)ip;
}

void
freeIPint(Heap *h)
{
	IPint *ip;

	ip = H2D(IPint*, h);
	if(ip->b)
		freeBignum(ip->b);
	freeheap(h, 0);
}

void
IPint_iptob64(void *fp)
{
	IPint *ip;
	F_IPint_iptob64 *f;
	char buf[MaxBigBytes];

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	ip = (IPint*)(f->i);
	if(ip == H || ip->b == 0)
		return;

	bigtobase64(ip->b, buf, MaxBigBytes);
	retstr(buf, f->ret);
}

void
IPint_iptobytes(void *fp)
{
	IPint *ip;
	F_IPint_iptobytes *f;
	uchar buf[MaxBigBytes];

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	ip = (IPint*)(f->i);
	if(ip == H || ip->b == 0)
		return;

	*f->ret = mem2array(buf, bigToBuf(ip->b, MaxBigBytes, buf));
}

void
IPint_iptostr(void *fp)
{
	IPint *ip;
	F_IPint_iptostr *f;
	char buf[MaxBigBytes];

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	ip = (IPint*)(f->i);
	if(ip == H || ip->b == 0)
		return;

	bigtostr(ip->b, buf, MaxBigBytes, f->base);
	retstr(buf, f->ret);
}

void
IPint_b64toip(void *fp)
{
	F_IPint_b64toip *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->str == H)
		return;

	*f->ret = newIPint(base64tobig(string2c(f->str), 0));
}

void
IPint_bytestoip(void *fp)
{
	F_IPint_bytestoip *f;
	BigInt b;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->buf == H)
		return;

	b = bigInit(0);
	bufToBig(f->buf->data, f->buf->len, b);
	*f->ret = newIPint(b);
}

void
IPint_bytestoip_sm(void *fp)
{
	F_IPint_bytestoip_sm *f;
	BigInt b;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->mag == H)
		return;
	if(f->sign == 0)
		error("bytestoip_sm: bad sign");

	b = bigInit(0);
	bufToBig_sm(f->sign, f->mag->data, f->mag->len, b);
	*f->ret = newIPint(b);
}

void
IPint_strtoip(void *fp)
{
	F_IPint_strtoip *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->str == H)
		return;

	*f->ret = newIPint(strtobig(string2c(f->str), 0, f->base));
}

/* create a random integer */
void
IPint_random(void *fp)
{
	F_IPint_random *f;
	BigInt b, min, max;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	b = bigInit(1);
	min = bigInit(0);
	max = bigInit(0);
	bigLeftShift(b, f->minbits, min);
	bigLeftShift(b, f->maxbits, max);
	
	release();
	getRandBetween(min, max, b, PSEUDO);
	acquire();

	trim(b);
	freeBignum(min);
	freeBignum(max);
	*f->ret = newIPint(b);
}

/* number of bits in number */
void
IPint_bits(void *fp)
{
	F_IPint_bits *f;
	BigInt b;
	int i, n;
	NumType num;

	f = fp;
	if(f->i == H)
		return;

	b = ((IPint*)f->i)->b;
	trim(b);

	n = (LENGTH(b)-1)*NumTypeBits;
	num = *(NUM(b) + LENGTH(b) - 1);
	for(i = NumTypeBits-1; i > 0; i--)
		if((1<<i) & num)
			break;

	*f->ret = n + i + 1;
}

/* create a new IP from an int */
void
IPint_inttoip(void *fp)
{
	F_IPint_inttoip *f;
	BigInt ret;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->i < 0){
		ret = bigInit(-f->i);
		SIGN(ret) = NEG;
	} else
		ret = bigInit(f->i);

	*f->ret = newIPint(ret);
}

void
IPint_iptoint(void *fp)
{
	F_IPint_iptoint *f;
	BigInt ip;
	int i;

	f = fp;
	*f->ret = 0;
	if(f->i == H)
		return;

	ip = ((IPint*)f->i)->b;
	trim(ip);
	i = *NUM(ip);
	if(LENGTH(ip) > 1 || (i & (1 << (8*sizeof(NumType)-1))))
		i = 0x7fffffff;
	if(SIGN(ip) == NEG)
		i = -i;
	*f->ret = i;
}

/* modular exponentiation */
void
IPint_expmod(void *fp)
{
	F_IPint_expmod *f;
	BigInt exp, mod, base, ret;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->base == H || f->exp == H || f->mod == H)
		return;

	base = ((IPint*)f->base)->b;
	exp = ((IPint*)f->exp)->b;
	mod = ((IPint*)f->mod)->b;
	ret = bigInit(0);
	bigPow(base, exp, mod, ret);

	*f->ret = newIPint(ret);
}

/* basic math */
void
IPint_add(void *fp)
{
	F_IPint_add *f;
	BigInt i1, i2, ret;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->i1 == H || f->i2 == H)
		return;

	i1 = ((IPint*)f->i1)->b;
	i2 = ((IPint*)f->i2)->b;
	ret = bigInit(0);
	bigAdd(i1, i2, ret);

	*f->ret = newIPint(ret);
}
void
IPint_sub(void *fp)
{
	F_IPint_sub *f;
	BigInt i1, i2, ret;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->i1 == H || f->i2 == H)
		return;

	i1 = ((IPint*)f->i1)->b;
	i2 = ((IPint*)f->i2)->b;
	ret = bigInit(0);
	bigSubtract(i1, i2, ret);

	*f->ret = newIPint(ret);
}
void
IPint_mul(void *fp)
{
	F_IPint_mul *f;
	BigInt i1, i2, ret;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->i1 == H || f->i2 == H)
		return;

	i1 = ((IPint*)f->i1)->b;
	i2 = ((IPint*)f->i2)->b;
	ret = bigInit(0);
	bigMultiply(i1, i2, ret);

	*f->ret = newIPint(ret);
}
void
IPint_div(void *fp)
{
	F_IPint_div *f;
	BigInt i1, i2, quo, rem;

	f = fp;
	destroy(f->ret->t0);
	f->ret->t0 = H;
	destroy(f->ret->t1);
	f->ret->t1 = H;

	if(f->i1 == H || f->i2 == H)
		return;

	i1 = ((IPint*)f->i1)->b;
	i2 = ((IPint*)f->i2)->b;
	quo = bigInit(0);
	rem = bigInit(0);
	bigDivide(i1, i2, quo, rem);

	f->ret->t0 = newIPint(quo);
	f->ret->t1 = newIPint(rem);
}
void
IPint_neg(void *fp)
{
	F_IPint_neg *f;
	BigInt i, ret;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->i == H)
		return;

	i = ((IPint*)f->i)->b;
	ret = bigInit(0);
	bigCopy(i, ret);
	SIGN(ret) = -SIGN(ret);

	*f->ret = newIPint(ret);
}

/* equality */
void
IPint_eq(void *fp)
{
	F_IPint_eq *f;
	BigInt b1, b2;

	f = fp;
	*f->ret = 0;

	if(f->i1 == H || f->i2 == H)
		return;

	b1 = ((IPint*)f->i1)->b;
	b2 = ((IPint*)f->i2)->b;
	if(bigCompare(b1, b2) == 0){
		if(SIGN(b1) == SIGN(b2))
			*f->ret = 1;
	}
}

/* compare */
void
IPint_cmp(void *fp)
{
	F_IPint_eq *f;
	BigInt b1, b2;

	f = fp;
	*f->ret = 0;

	if(f->i1 == H || f->i2 == H)
		error("nil reference exception");

	b1 = ((IPint*)f->i1)->b;
	b2 = ((IPint*)f->i2)->b;
	*f->ret = bigCompare(b1, b2);
	if(SIGN(b1) == SIGN(b2)){
		if(SIGN(b1) == NEG)
			*f->ret = -*f->ret;
	} else if(SIGN(b1) == NEG){
		*f->ret = -1;
	}
}
