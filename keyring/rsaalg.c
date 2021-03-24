#include <lib9.h>
#include <kernel.h>
#include <isa.h>
#include "interp.h"
#include "../interp/runt.h"
#include "libcrypt.h"
#include "keys.h"

static RSAPrivateKey*
rsa_str2sk(char *str, char **strp)
{
	RSAPrivateKey *rsa;
	char *p;

	rsa = crypt_malloc(sizeof(*rsa));
	rsa->modulus = base64tobig(str, &p);
	rsa->publicExponent = base64tobig(p, &p);
	rsa->privateExponent = base64tobig(p, &p);
	rsa->crt = crypt_malloc(sizeof(*rsa->crt));
	rsa->crt->p = base64tobig(p, &p);
	rsa->crt->q = base64tobig(p, &p);
	rsa->crt->dp = base64tobig(p, &p);
	rsa->crt->dq = base64tobig(p, &p);
	rsa->crt->c12 = base64tobig(p, &p);
	if(strp)
		*strp = p;

	return rsa;
}

static RSAPublicKey*
rsa_str2pk(char *str, char **strp)
{
	RSAPublicKey *rsa;
	char *p;

	rsa = crypt_malloc(sizeof(*rsa));
	rsa->modulus = base64tobig(str, &p);
	rsa->publicExponent = base64tobig(p, &p);
	if(strp)
		*strp = p;

	return rsa;
}

static RSASignature*
rsa_str2sig(char *str, char **strp)
{
	RSASignature *rsa;
	char *p;

	rsa = base64tobig(str, &p);
	if(strp)
		*strp = p;
	return rsa;
}

static int
rsa_sk2str(RSAPrivateKey *rsa, char *buf, int len)
{
	char *cp, *ep;

	ep = buf + len - 1;
	cp = buf;

	cp += snprint(cp, ep - cp, "%U\n", rsa->modulus);
	cp += snprint(cp, ep - cp, "%U\n", rsa->publicExponent);
	cp += snprint(cp, ep - cp, "%U\n", rsa->privateExponent);
	cp += snprint(cp, ep - cp, "%U\n", rsa->crt->p);
	cp += snprint(cp, ep - cp, "%U\n", rsa->crt->q);
	cp += snprint(cp, ep - cp, "%U\n", rsa->crt->dp);
	cp += snprint(cp, ep - cp, "%U\n", rsa->crt->dq);
	cp += snprint(cp, ep - cp, "%U\n", rsa->crt->c12);
	*cp = 0;

	return cp - buf;
}

static int
rsa_pk2str(RSAPrivateKey *rsa, char *buf, int len)
{
	char *cp, *ep;

	ep = buf + len - 1;
	cp = buf;

	cp += snprint(cp, ep - cp, "%U\n", rsa->modulus);
	cp += snprint(cp, ep - cp, "%U\n", rsa->publicExponent);
	*cp = 0;

	return cp - buf;
}

static int
rsa_sig2str(RSASignature *rsa, char *buf, int len)
{
	char *cp, *ep;

	ep = buf + len - 1;
	cp = buf;

	cp += snprint(cp, ep - cp, "%U\n", rsa);
	*cp = 0;

	return cp - buf;
}

static RSAPublicKey*
rsa_sk2pk(RSAPrivateKey *s)
{
	RSAPublicKey *p;

	p = crypt_malloc(sizeof(*p));

	p->publicExponent = bigInit(0);
	bigCopy(s->publicExponent, p->publicExponent);
	p->modulus = bigInit(0);
	bigCopy(s->modulus, p->modulus);

	return p;
}

/* generate an rsa secret key */
static void*
rsa_gen(int len)
{
	RSAKeySet *ks;
	RSAPrivateKey *key;

	ks = genRSAKeySet(len, 8);
	key = ks->privateKey;
	freeRSAPublicKey(ks->publicKey);
	free(ks);
	return key;
}

/* generate an rsa secret key with same params as a public key */
static void*
rsa_genfrompk(RSAPublicKey *pub)
{
	int nbits;
	RSAKeySet *ks;
	RSAPrivateKey *key;

	nbits = BIT2BYTE*LENGTH(pub->modulus);
	
	ks = genRSAKeySet(nbits, nbits/2);
	key = ks->privateKey;
	freeRSAPublicKey(ks->publicKey);
	free(ks);
	return key;
}

SigAlgVec*
rsainit(void)
{
	SigAlgVec *vec;

	vec = malloc(sizeof(SigAlgVec));
	if(vec == nil)
		panic("rsainit: no memory");

	vec->name = "rsa";
	vec->str2sk = rsa_str2sk;
	vec->str2pk = rsa_str2pk;
	vec->str2sig = rsa_str2sig;

	vec->sk2str = rsa_sk2str;
	vec->pk2str = rsa_pk2str;
	vec->sig2str = rsa_sig2str;

	vec->sk2pk = rsa_sk2pk;

	vec->gensk = rsa_gen;
	vec->genskfrompk = rsa_genfrompk;
	vec->sign = RSASign;
	vec->verify = RSAVerify;

	vec->skfree = freeRSAPrivateKey;
	vec->pkfree = freeRSAPublicKey;
	vec->sigfree = freeRSASig;

	return vec;
}

void
sktorsa(SK *sk, Keyring_RSAKeys *rsa)
{
	RSAPrivateKey *s;
	BigInt b;

	s = sk->key;
	b = bigInit(0);
	bigCopy(s->publicExponent, b);
	rsa->pubexp = newIPint(b);
	b = bigInit(0);
	bigCopy(s->privateExponent, b);
	rsa->privexp = newIPint(b);
	b = bigInit(0);
	bigCopy(s->modulus, b);
	rsa->modulus = newIPint(b);
}
