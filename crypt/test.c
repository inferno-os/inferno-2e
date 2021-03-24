#include "lib9.h"
#include <libcrypt.h>

enum
{
	Modsize=	1024,
	Reducesize=	160,
	Bits2byte=	8,
};

BigInt
str2num(char *s)
{
	int n;
	BigInt b;

	n = strlen(s);
	if(n > Modsize/Bits2byte){
		fprint(2, "message too big\n");
		exits(0);
	}

	b = bigInit(0);
	GUARANTEE(b, n/sizeof(NumType));
	memmove(b->num, s, n);
	return b;
}

void
main(void)
{
	EGParams *params;
	EGKeySet *keyset;
	DSSSignature *sig;
	BigInt msg;

	fmtinstall('B', bigconv);

	params = genEGParams(Modsize, Reducesize);
	print("p = %B\nq = %B\nalpha = %B\n", params->p, params->q, params->alpha);

	keyset = genEGKeySet(params);
	print("pub = %B\n", keyset->publicKey->publicKey);
	print("priv = %B\n", keyset->privateKey->secret);

	msg = str2num("the rain in spain");
	sig = DSSSign(msg, keyset->privateKey);
	print("r = %B\n", sig->r);
	print("s = %B\n", sig->s);
	sig->r->num[0] ^= 1;
	if(DSSVerify(msg, sig, keyset->publicKey))
		print("OK\n");
	else
		print("not OK\n");
	exits(0);
}
