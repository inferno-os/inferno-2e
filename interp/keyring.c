#include "lib9.h"
#include "kernel.h"
#include <isa.h>
#include "interp.h"
#include "runt.h"
#include "keyring.h"
#include "libcrypt.h"
#include "pool.h"
#include "raise.h"
#include "../keyring/keys.h"

Type	*TSigAlg;
Type	*TCertificate;
Type	*TSK;
Type	*TPK;
Type	*TDigestState;
Type	*TAuthinfo;
Type	*TDESstate;
Type	*TIPint;

enum {
	Maxmsg=	4096
};

uchar IPintmap[] = Keyring_IPint_map;
uchar SigAlgmap[] = Keyring_SigAlg_map;
uchar SKmap[] = Keyring_SK_map;
uchar PKmap[] = Keyring_PK_map;
uchar Certificatemap[] = Keyring_Certificate_map;
uchar DigestStatemap[] = Keyring_DigestState_map;
uchar Authinfomap[] = Keyring_Authinfo_map;
uchar DESstatemap[] = Keyring_DESstate_map;

PK*	checkPK(Keyring_PK *k);

extern void		setid(char*);
extern vlong		osusectime(void);
extern Keyring_IPint*	newIPint(BigInt);
extern void		freeIPint(Heap*, int);

static char exBadSA[]	= "bad signature algorithm";
static char exBadSK[]	= "bad secret key";
static char exBadPK[]	= "bad public key";
static char exBadCert[]	= "bad certificate";

/*
 *  Infinite (actually kind of big) precision integers
 */

/* convert a Big to base64 ascii */
int
bigtobase64(BigInt b, char *buf, int blen)
{
	int n;
	uchar hex[(MaxBigBytes*6 + 7)/8];

	n = LENGTH(b)*sizeof(NumType);
	if(n >= sizeof(hex)){
		if(blen > 0){
			*buf = '*';
			return 1;
		}
		return 0;
	}
	return enc64(buf, blen, hex, bigToBuf(b, sizeof(hex), hex));
}

/* convert a Big to base64 ascii for %U */
int
big64conv(va_list *arg, Fconv *f)
{
	BigInt b;
	char buf[MaxBigBytes];

	b = va_arg(*arg, BigInt);
	bigtobase64(b, buf, MaxBigBytes);
	strconv(buf, f);
	return 0;
}

/* convert a base64 string to a big */
BigInt
base64tobig(char *str, char **strp)
{
	int n;
	char *p;
	BigInt b;
	uchar hex[(MaxBigBytes*6 + 7)/8];

	for(p = str; *p && *p != '\n'; p++)
		;
	b = bigInit(0);
	n = dec64(hex, sizeof(hex), str, p - str);
	bufToBig(hex, n, b);
	if(strp){
		if(*p)
			p++;
		*strp = p;
	}
	trim(b);
	return b;
}

/*
 *  signature algorithms
 */
enum
{
	Maxalg = 8
};
static SigAlgVec	*algs[Maxalg];
static int		nalg;

static SigAlg*
newSigAlg(SigAlgVec *vec)
{
	Heap *h;
	SigAlg *sa;

	h = heap(TSigAlg);
	sa = H2D(SigAlg*, h);
	retstr(vec->name, &sa->x.name);
	sa->vec = vec;
	return sa;
}

static void
freeSigAlg(Heap *h, int swept)
{
	if(!swept)
		freeheap(h, 0);
}

SigAlg*
strtoalg(char *str, char **strp)
{
	int n;
	char *p;
	SigAlgVec **sap;


	p = strchr(str, '\n');
	if(p == 0){
		p = str + strlen(str);
		if(strp)
			*strp = p;
	} else {
		if(strp)
			*strp = p+1;
	}

	n = p - str;
	for(sap = algs; sap < &algs[nalg]; sap++){
		if(strncmp(str, (*sap)->name, n) == 0)
			return newSigAlg(*sap);
	}

	return nil;
}

static SigAlg*
checkSigAlg(Keyring_SigAlg *ksa)
{
	SigAlgVec **sap;
	SigAlg *sa;

	sa = (SigAlg*)ksa;

	for(sap = algs; sap < &algs[Maxalg]; sap++)
		if(sa->vec == *sap)
			return sa;
	print(exBadSA);
	error(exType);
	return nil;
}

/*
 *  parse next new line terminated string into a String
 */
String*
strtostring(char *str, char **strp)
{
	char *p;
	String *s;

	p = strchr(str, '\n');
	if(p == 0)
		p = str + strlen(str);
	s = H;
	retnstr(str, p - str, &s);

	if(strp){
		if(*p)
			p++;
		*strp = p;
	}

	return s;
}

/*
 *  private part of a key
 */
static SK*
newSK(SigAlg *sa, String *owner, int increfsa)
{
	Heap *h;
	SK *k;

	h = heap(TSK);
	k = H2D(SK*, h);
	k->x.sa = (Keyring_SigAlg*)sa;
	if(increfsa) {
		h = D2H(sa);
		h->ref++;
		Setmark(h);
	}
	k->x.owner = owner;
	k->key = 0;
	return k;
}

static void
freeSK(Heap *h, int swept)
{
	SK *k;
	SigAlg *sa;

	k = H2D(SK*, h);
	sa = checkSigAlg(k->x.sa);
	if(k->key)
		(*sa->vec->skfree)(k->key);
	freeheap(h, swept);
}

static SK*
checkSK(Keyring_SK *k)
{
	SK *sk;

	sk = (SK*)k;
	if(sk == H || sk == nil || sk->key == 0 || D2H(sk)->t != TSK){
		print(exBadSK);
		error(exType);
	}
	return sk;
}

void
Keyring_genSK(void *fp)
{
	F_Keyring_genSK *f;
	SK *sk;
	SigAlg *sa;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	sa = strtoalg(string2c(f->algname), 0);
	if(sa == nil)
		return;

	sk = newSK(sa, stringdup(f->owner), 0);
	*f->ret = (Keyring_SK*)sk;
	release();
	sk->key = (*sa->vec->gensk)(f->length);
	acquire();
}

void
Keyring_genSKfromPK(void *fp)
{
	F_Keyring_genSKfromPK *f;
	SigAlg *sa;
	PK *pk;
	SK *sk;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	pk = checkPK(f->pk);
	sa = checkSigAlg(pk->x.sa);
	sk = newSK(sa, stringdup(f->owner), 1);
	*f->ret = (Keyring_SK*)sk;
	release();
	sk->key = (*sa->vec->genskfrompk)(pk->key);
	acquire();
}

static int
sktostr(SK *sk, char *buf, int len)
{
	int n;
	SigAlg *sa;

	sa = checkSigAlg(sk->x.sa);
	n = snprint(buf, len, "%s\n%s\n", string2c(sa->x.name),
			string2c(sk->x.owner));
	return n + (*sa->vec->sk2str)(sk->key, buf+n, len - n);
}

void
Keyring_sktostr(void *fp)
{
	F_Keyring_sktostr *f;
	char *buf;

	f = fp;
	buf = malloc(Maxbuf);

	if(buf)
		sktostr(checkSK(f->sk), buf, Maxbuf);
	retstr(buf, f->ret);

	free(buf);
}

static SK*
strtosk(char *buf)
{
	SK *sk;
	char *p;
	SigAlg *sa;
	String *owner;
	void *key;

	sa = strtoalg(buf, &p);
	if(sa == nil)
		return H;
	owner = strtostring(p, &p);
	if(owner == H){
		destroy(sa);
		return H;
	}

	key = (*sa->vec->str2sk)(p, &p);
	if(key == nil){
		destroy(sa);
		destroy(owner);
		return H;
	}
	sk = newSK(sa, owner, 0);
	sk->key = key;

	return sk;
}

void
Keyring_strtosk(void *fp)
{
	F_Keyring_strtosk *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = (Keyring_SK*)strtosk(string2c(f->s));
}

/*
 *  public part of a key
 */
PK*
newPK(SigAlg *sa, String *owner, int increfsa)
{
	Heap *h;
	PK *k;

	h = heap(TPK);
	k = H2D(PK*, h);
	k->x.sa = (Keyring_SigAlg*)sa;
	if(increfsa) {
		h = D2H(sa);
		h->ref++;
		Setmark(h);
	}
	k->x.owner = owner;
	k->key = 0;
	return k;
}

void
pkimmutable(PK *k)
{
	poolimmutable(D2H(k));
	poolimmutable(D2H(k->x.sa));
	poolimmutable(D2H(k->x.sa->name));
	poolimmutable(D2H(k->x.owner));
}

void
pkmutable(PK *k)
{
	poolmutable(D2H(k));
	poolmutable(D2H(k->x.sa));
	poolmutable(D2H(k->x.sa->name));
	poolmutable(D2H(k->x.owner));
}

void
freePK(Heap *h, int swept)
{
	PK *k;
	SigAlg *sa;

	k = H2D(PK*, h);
	sa = checkSigAlg(k->x.sa);
	if(k->key)
		(*sa->vec->pkfree)(k->key);
	freeheap(h, swept);
}

PK*
checkPK(Keyring_PK *k)
{
	PK *pk;

	pk = (PK*)k;
	if(pk == H || pk == nil || pk->key == 0 || D2H(pk)->t != TPK){
		print(exBadPK);
		error(exType);
	}
	return pk;
}

void
Keyring_sktopk(void *fp)
{
	F_Keyring_sktopk *f;
	PK *pk;
	SigAlg *sa;
	SK *sk;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	sk = checkSK(f->sk);
	sa = checkSigAlg(sk->x.sa);
	pk = newPK(sa, stringdup(sk->x.owner), 1);
	pk->key = (*sa->vec->sk2pk)(sk->key);
	*f->ret = (Keyring_PK*)pk;
}

static int
pktostr(PK *pk, char *buf, int len)
{
	int n;
	SigAlg *sa;

	sa = checkSigAlg(pk->x.sa);
	n = snprint(buf, len, "%s\n%s\n", string2c(sa->x.name),
			string2c(pk->x.owner));
	return n + (*sa->vec->pk2str)(pk->key, buf+n, len - n);
}

void
Keyring_pktostr(void *fp)
{
	F_Keyring_pktostr *f;
	char *buf;

	f = fp;
	buf = malloc(Maxbuf);

	if(buf)
		pktostr(checkPK(f->pk), buf, Maxbuf);
	retstr(buf, f->ret);

	free(buf);
}

static PK*
strtopk(char *buf)
{
	PK *pk;
	char *p;
	SigAlg *sa;
	String *owner;
	void *key;

	sa = strtoalg(buf, &p);
	if(sa == nil)
		return H;
	owner = strtostring(p, &p);
	if(owner == H){
		destroy(sa);
		return H;
	}

	key = (*sa->vec->str2pk)(p, &p);
	if(key == nil){
		destroy(sa);
		destroy(owner);
		return H;
	}
	pk = newPK(sa, owner, 0);
	pk->key = key;

	return pk;
}

void
Keyring_strtopk(void *fp)
{
	F_Keyring_strtopk *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = (Keyring_PK*)strtopk(string2c(f->s));
}

/*
 *  Certificates/signatures
 */

void
certimmutable(Certificate *c)
{
	poolimmutable(D2H(c));
	poolimmutable(D2H(c->x.signer));
	poolimmutable(D2H(c->x.ha));
	poolimmutable(D2H(c->x.sa));
	poolimmutable(D2H(c->x.sa->name));
}

void
certmutable(Certificate *c)
{
	poolmutable(D2H(c));
	poolmutable(D2H(c->x.signer));
	poolmutable(D2H(c->x.ha));
	Setmark(D2H(c->x.sa));
	poolmutable(D2H(c->x.sa));
	Setmark(D2H(c->x.sa->name));
	poolmutable(D2H(c->x.sa->name));
}

Certificate*
newCertificate(SigAlg *sa, String *ha, String *signer, long exp, int increfsa)
{
	Heap *h;
	Certificate *c;

	h = heap(TCertificate);
	c = H2D(Certificate*, h);
	c->x.sa = (Keyring_SigAlg*)sa;
	if(increfsa) {
		h = D2H(sa);
		h->ref++;
		Setmark(h);
	}
	c->x.signer = signer;
	c->x.ha = ha;
	c->x.exp = exp;
	c->signa = 0;

	return c;
}

void
freeCertificate(Heap *h, int swept)
{
	Certificate *c;
	SigAlg *sa;

	c = H2D(Certificate*, h);
	sa = checkSigAlg(c->x.sa);
	if(c->signa)
		(*sa->vec->sigfree)(c->signa);
	freeheap(h, swept);
}

Certificate*
checkCertificate(Keyring_Certificate *c)
{
	Certificate *cert;

	cert = (Certificate*)c;
	if(cert == H || cert == nil || cert->signa == 0 || D2H(cert)->t != TCertificate){
		print(exBadCert);
		error(exType);
	}
	return cert;
}

static int
certtostr(Certificate *c, char *buf, int len)
{
	SigAlg *sa;
	int n;

	sa = checkSigAlg(c->x.sa);
	n = snprint(buf, len, "%s\n%s\n%s\n%d\n", string2c(sa->x.name),
		string2c(c->x.ha), string2c(c->x.signer), c->x.exp);
	return n + (*sa->vec->sig2str)(c->signa, buf+n, len - n);
}

void
Keyring_certtostr(void *fp)
{
	F_Keyring_certtostr *f;
	char *buf;

	f = fp;
	buf = malloc(Maxbuf);

	if(buf)
		certtostr(checkCertificate(f->c), buf, Maxbuf);
	retstr(buf, f->ret);

	free(buf);
}

static Certificate*
strtocert(char *buf)
{
	Certificate *c;
	char *p;
	SigAlg *sa;
	String *signer, *ha;
	long exp;
	void *signa;

	sa = strtoalg(buf, &p);
	if(sa == 0)
		return H;

	ha = strtostring(p, &p);
	if(ha == H){
		destroy(sa);
		return H;
	}

	signer = strtostring(p, &p);
	if(signer == H){
		destroy(sa);
		destroy(ha);
		return H;
	}

	exp = strtoul(p, &p, 10);
	if(*p)
		p++;
	signa = (*sa->vec->str2sig)(p, &p);
	if(signa == nil){
		destroy(sa);
		destroy(ha);
		destroy(signer);
		return H;
	}

	c = newCertificate(sa, ha, signer, exp, 0);
	c->signa = signa;

	return c;
}

void
Keyring_strtocert(void *fp)
{
	F_Keyring_strtocert *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = (Keyring_Certificate*)strtocert(string2c(f->s));
}

static Certificate*
sign(SK *sk, char *ha, ulong exp, uchar *a, int len)
{
	Certificate *c;
	BigInt b;
	int n;
	SigAlg *sa;
	DigestState *ds;
	uchar digest[SHAdlen];
	char *buf;
	String *hastr;

	hastr = H;
	sa = checkSigAlg(sk->x.sa);
	buf = malloc(Maxbuf);
	if(buf == nil)
		return nil;

	/* add signer name and expiration time to hash */
	n = snprint(buf, Maxbuf, "%s %lud", string2c(sk->x.owner), exp);
	if(strcmp(ha, "sha") == 0){
		ds = sha(a, len, 0, 0);
		sha((uchar*)buf, n, digest, ds);
		n = Keyring_SHAdlen;
	} else if(strcmp(ha, "md5") == 0){
		ds = md5(a, len, 0, 0);
		md5((uchar*)buf, n, digest, ds);
		n = Keyring_MD5dlen;
	} else if(strcmp(ha, "md4") == 0){
		ds = md4(a, len, 0, 0);
		md4((uchar*)buf, n, digest, ds);
		n = Keyring_MD5dlen;
	} else {
		free(buf);
		return nil;
	}
	free(buf);

	/* turn message into a big integer */
	b = bigInit(0);
	if(b == nil)
		return nil;
	bufToBig_sm(1, digest, n, b);

	/* sign */
	retstr(ha, &hastr);
	c = newCertificate(sa, hastr, stringdup(sk->x.owner), exp, 1);
	certimmutable(c);		/* hide from the garbage collector */
	c->signa = (*sa->vec->sign)(b, sk->key);
	freeBignum(b);

	return c;
}

void
Keyring_sign(void *fp)
{
	F_Keyring_sign *f;
	Certificate *c;
	BigInt b;
	int n;
	SigAlg *sa;
	SK *sk;
	XDigestState *ds;
	uchar digest[SHAdlen];
	char *buf;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	sk = checkSK(f->sk);
	sa = checkSigAlg(sk->x.sa);

	/* add signer name and expiration time to hash */
	if(f->state == H)
		return;
	buf = malloc(Maxbuf);
	if(buf == nil)
		return;
	ds = (XDigestState*)f->state;
	n = snprint(buf, Maxbuf, "%s %d", string2c(sk->x.owner), f->exp);
	if(strcmp(string2c(f->ha), "sha") == 0){
		sha((uchar*)buf, n, digest, &ds->state);
		n = Keyring_SHAdlen;
	} else if(strcmp(string2c(f->ha), "md5") == 0){
		md5((uchar*)buf, n, digest, &ds->state);
		n = Keyring_MD5dlen;
	} else if(strcmp(string2c(f->ha), "md4") == 0){
		md4((uchar*)buf, n, digest, &ds->state);
		n = Keyring_MD5dlen;
	} else {
		free(buf);
		return;
	}
	free(buf);

	/* turn message into a big integer */
	b = bigInit(0);
	if(b == nil)
		return;
	bufToBig_sm(1, digest, n, b);

	/* sign */
	c = newCertificate(sa, stringdup(f->ha), stringdup(sk->x.owner), f->exp, 1);
	*f->ret = (Keyring_Certificate*)c;
	c->signa = (*sa->vec->sign)(b, sk->key);
	freeBignum(b);
}

static int
verify(PK *pk, Certificate *c, char *a, int len)
{
	BigInt b;
	int n;
	SigAlg *sa, *pksa;
	DigestState *ds;
	uchar digest[SHAdlen];
	char *buf;

	sa = checkSigAlg(c->x.sa);
	pksa = checkSigAlg(pk->x.sa);
	if(sa->vec != pksa->vec)
		return 0;

	/* add signer name and expiration time to hash */
	buf = malloc(Maxbuf);
	if(buf == nil)
		return 0;
	n = snprint(buf, Maxbuf, "%s %d", string2c(c->x.signer), c->x.exp);
	if(strcmp(string2c(c->x.ha), "sha") == 0){
		ds = sha((uchar*)a, len, 0, 0);
		sha((uchar*)buf, n, digest, ds);
		n = Keyring_SHAdlen;
	} else if(strcmp(string2c(c->x.ha), "md5") == 0){
		ds = md5((uchar*)a, len, 0, 0);
		md5((uchar*)buf, n, digest, ds);
		n = Keyring_MD5dlen;
	} else if(strcmp(string2c(c->x.ha), "md4") == 0){
		ds = md4((uchar*)a, len, 0, 0);
		md4((uchar*)buf, n, digest, ds);
		n = Keyring_MD5dlen;
	} else {
		free(buf);
		return 0;
	}
	free(buf);

	/* turn message into a big integer */
	b = bigInit(0);
	if(b == nil)
		return 0;
	bufToBig_sm(1, digest, n, b);

	/* verify */
	n = (*sa->vec->verify)(b, c->signa, pk->key);

	freeBignum(b);
	return n;
}

void
Keyring_verify(void *fp)
{
	F_Keyring_verify *f;
	Certificate *c;
	BigInt b;
	int n;
	SigAlg *sa, *pksa;
	PK *pk;
	XDigestState *ds;
	uchar digest[SHAdlen];
	char *buf;

	f = fp;
	*f->ret = 0;

	c = checkCertificate(f->cert);
	sa = checkSigAlg(c->x.sa);
	pk = checkPK(f->pk);
	pksa = checkSigAlg(pk->x.sa);
	if(sa->vec != pksa->vec)
		return;

	/* add signer name and expiration time to hash */
	if(f->state == H)
		return;
	buf = malloc(Maxbuf);
	if(buf == nil)
		return;
	n = snprint(buf, Maxbuf, "%s %d", string2c(c->x.signer), c->x.exp);
	ds = (XDigestState*)f->state;

	if(strcmp(string2c(c->x.ha), "sha") == 0){
		sha((uchar*)buf, n, digest, &ds->state);
		n = Keyring_SHAdlen;
	} else if(strcmp(string2c(c->x.ha), "md5") == 0){
		md5((uchar*)buf, n, digest, &ds->state);
		n = Keyring_MD5dlen;
	} else if(strcmp(string2c(c->x.ha), "md4") == 0){
		md4((uchar*)buf, n, digest, &ds->state);
		n = Keyring_MD5dlen;
	} else {
		free(buf);
		return;
	}
	free(buf);

	/* turn message into a big integer */
	b = bigInit(0);
	if(b == nil)
		return;
	bufToBig_sm(1, digest, n, b);

	/* verify */
	*f->ret = (*sa->vec->verify)(b, c->signa, pk->key);

	freeBignum(b);
}

/*
 *  digests
 */
void
Keyring_cloneDigestState(void *fp)
{
	F_Keyring_cloneDigestState *f;
	Heap *h;
	XDigestState *ds;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->state != (Keyring_DigestState*)H) {
		h = heap(TDigestState);
		ds = H2D(XDigestState*, h); 	
		memmove(&ds->state, &((XDigestState*)f->state)->state, sizeof(ds->state)); 
		*f->ret = (Keyring_DigestState*)ds;
	}
}

void
Keyring_sha(void *fp)
{
	F_Keyring_sha *f;
	Heap *h;
	XDigestState *ds;
	int n;
	uchar *digest;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->buf == (Array*)H)
		return;

	if(f->digest != (Array*)H){
		if(f->digest->len < SHAdlen)
			return;
		digest = f->digest->data;
	} else
		digest = 0;

	n = f->n;
	if(n > f->buf->len)
		n = f->buf->len;

	if(f->state == H){
		h = heap(TDigestState);
		ds = H2D(XDigestState*, h);
		memset(&ds->state, 0, sizeof(ds->state));
	} else {
		D2H(f->state)->ref++;
		ds = (XDigestState*)f->state;
	}

	sha(f->buf->data, n, digest, &ds->state);

	*f->ret = (Keyring_DigestState*)ds;
}

void
Keyring_md5(void *fp)
{
	F_Keyring_md5 *f;
	Heap *h;
	XDigestState *ds;
	int n;
	uchar *digest;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->buf == (Array*)H)
		return;

	if(f->digest != (Array*)H){
		if(f->digest->len < MD5dlen)
			return;
		digest = f->digest->data;
	} else
		digest = 0;

	n = f->n;
	if(n > f->buf->len)
		n = f->buf->len;

	if(f->state == H){
		h = heap(TDigestState);
		ds = H2D(XDigestState*, h);
		memset(&ds->state, 0, sizeof(ds->state));
	} else {
		D2H(f->state)->ref++;
		ds = (XDigestState*)f->state;
	}

	md5(f->buf->data, n, digest, &ds->state);

	*f->ret = (Keyring_DigestState*)ds;
}

void
Keyring_md4(void *fp)
{
	F_Keyring_md4 *f;
	Heap *h;
	XDigestState *ds;
	int n;
	uchar *digest;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->buf == (Array*)H)
		return;

	if(f->digest != (Array*)H){
		if(f->digest->len < MD4dlen)
			return;
		digest = f->digest->data;
	} else
		digest = 0;

	n = f->n;
	if(n > f->buf->len)
		n = f->buf->len;

	if(f->state == H){
		h = heap(TDigestState);
		ds = H2D(XDigestState*, h);
		memset(&ds->state, 0, sizeof(ds->state));
	} else {
		D2H(f->state)->ref++;
		ds = (XDigestState*)f->state;
	}

	md4(f->buf->data, n, digest, &ds->state);

	*f->ret = (Keyring_DigestState*)ds;
}

void
Keyring_dhparams(void *fp)
{
	F_Keyring_dhparams *f;
	EGParams *egp;

	f = fp;
	destroy(f->ret->t0);
	f->ret->t0 = H;
	destroy(f->ret->t1);
	f->ret->t1 = H;

	release();
	egp = genEGParams(f->nbits, f->nbits/2);
	acquire();
	f->ret->t0 = newIPint(egp->alpha);
	f->ret->t1 = newIPint(egp->p);
	free(egp);
}

static int
sendmsg(int fd, void *buf, int n)
{
	char num[10];

	release();
	snprint(num, sizeof(num), "%4.4d\n", n);
	if(kwrite(fd, num, 5) != 5){
		acquire();
		return -1;
	}
	n = kwrite(fd, buf, n);
	acquire();
	return n;
}

void
Keyring_sendmsg(void *fp)
{
	F_Keyring_sendmsg *f;

	f = fp;
	*f->ret = -1;
	if(f->fd == H || f->buf == H)
		return;
	*f->ret = sendmsg(f->fd->fd, f->buf->data, f->buf->len);
}


static int
senderr(int fd, char *err)
{
	char num[10];
	int n, m;

	release();
	n = strlen(err);
	m = strlen("remote: ");
	snprint(num, sizeof(num), "!%3.3d\n", n+m);
	if(kwrite(fd, num, 5) != 5){
		acquire();
		return -1;
	}
	kwrite(fd, "remote: ", m);
	n = kwrite(fd, err, n);
	acquire();
	return n;
}

static int
nreadn(int fd, void *av, int n)
{

	char *a;
	long m, t;

	a = av;
	t = 0;
	while(t < n){
		m = kread(fd, a+t, n-t);
		if(m <= 0){
			if(t == 0)
				return m;
			break;
		}
		t += m;
	}
	return t;
}

#define MSG "input or format error"

static void
getmsgerr(char *buf, int n)
{
	if(n > sizeof(MSG))
		n = sizeof(MSG)-1;
	memmove(buf, MSG, n-1);
	buf[n-1] = 0;
}

static int
getmsg(int fd, char *buf, int n)
{
	char num[6];
	int len;

	release();
	if(nreadn(fd, num, 5) != 5){
		getmsgerr(buf, n);
		acquire();
		return -1;
	}
	num[5] = 0;

	if(num[0] == '!')
		len = strtoul(num+1, 0, 10);
	else
		len = strtoul(num, 0, 10);

	if(len < 0 || len >= n || nreadn(fd, buf, len) != len){
		getmsgerr(buf, n);
		acquire();
		return -1;
	}

	buf[len] = 0;
	acquire();
	if(num[0] == '!')
		return -len;

	return len;
}

void
Keyring_getmsg(void *fp)
{
	F_Keyring_getmsg *f;
	char *buf;
	int n;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;
	if(f->fd == H)
		return;

	buf = malloc(Maxmsg);
	if(buf == nil)
		return;

	n = getmsg(f->fd->fd, buf, Maxmsg);
	if(n < 0){
		free(buf);
		return;
	}

	*f->ret = mem2array(buf, n);
	free(buf);
}

void
Keyring_auth(void *fp)
{
	F_Keyring_auth *f;
	BigInt r0, r1, p, alpha, alphar0, alphar1, alphar0r1, low;
	SK *mysk;
	PK *mypk, *spk, *hispk;
	Certificate *cert, *hiscert, *alphacert;
	char *buf, *err;
	int n, fd, version;
	long now;

	hispk = H;
	hiscert = H;
	alphacert = H;
	err = nil;

	/* null out the return values */
	f = fp;
	destroy(f->ret->t0);
	f->ret->t0 = H;
	destroy(f->ret->t1);
	f->ret->t1 = H;
	low = r0 = r1 = alphar0 = alphar1 = alphar0r1 = nil;

	/* check args */
	if(f->fd == H || f->fd->fd < 0){
		retstr("bad fd", &(f->ret->t0));
		return;
	}
	fd = f->fd->fd;

	buf = malloc(Maxbuf);
	if(buf == nil)
		return;

	if(f->info == H){
		err = "missing args";
		goto out;
	}
	if(f->info->p == H){
		err = "missing diffie hellman mod";
		goto out;
	}
	if(f->info->alpha == H){
		err = "missing diffie hellman base";
		goto out;
	}
	mysk = checkSK(f->info->mysk);
	if(mysk == H){
		err = "bad sk arg";
		goto out;
	}
	mypk = checkPK(f->info->mypk);
	if(mypk == H){
		err = "bad pk arg";
		goto out;
	}
	cert = checkCertificate(f->info->cert);
	if(cert == H){
		err = "bad certificate arg";
		goto out;
	}
	spk = checkPK(f->info->spk);
	if(spk == H){
		err = "bad signer key arg";
		goto out;
	}

	/* send auth protocol version number */
	if(sendmsg(fd, "1", 1) <= 0){
		err = MSG;
		goto out;
	}

	/* get auth protocol version number */
	n = getmsg(fd, buf, Maxbuf-1);
	if(n < 0){
		err = buf;
		goto out;
	}
	buf[n] = 0;
	version = atoi(buf);
	if(version < 1 || n > 4){
		err = "incompatible authentication protocol";
		goto out;
	}

	/* get alpha and p */
	p = ((IPint*)f->info->p)->b;
	alpha = ((IPint*)f->info->alpha)->b;

	low = bigInit(0);
	r0 = bigInit(0);
	r1 = bigInit(0);
	alphar0 = bigInit(0);
	alphar0r1 = bigInit(0);

	/* generate alpha**r0 */
	release();
	bigRightShift(p, (LENGTH(p)*NumTypeBits)/4, low);
	getRandBetween(low, p, r0, PSEUDO);
	bigPow(alpha, r0, p, alphar0);
	trim(alphar0);
	acquire();

	/* send alpha**r0 mod p, mycert, and mypk */
	n = bigtobase64(alphar0, buf, Maxbuf);
	if(sendmsg(fd, buf, n) <= 0){
		err = MSG;
		goto out;
	}

	n = certtostr(cert, buf, Maxbuf);
	if(sendmsg(fd, buf, n) <= 0){
		err = MSG;
		goto out;
	}

	n = pktostr(mypk, buf, Maxbuf);
	if(sendmsg(fd, buf, n) <= 0){
		err = MSG;
		goto out;
	}

	/* get alpha**r1 mod p, hiscert, hispk */
	n = getmsg(fd, buf, Maxbuf-1);
	if(n < 0){
		err = buf;
		goto out;
	}
	buf[n] = 0;
	alphar1 = base64tobig(buf, 0);
	trim(alphar1);

	/* trying a fast one */
	if(bigCompare(p, alphar1) <= 0){
		err = "possible replay attack";
		goto out;
	}

	/* if alpha**r1 == alpha**r0, someone may be trying a replay */
	if(bigCompare(alphar0, alphar1) == 0){
		err = "possible replay attack";
		goto out;
	}

	n = getmsg(fd, buf, Maxbuf-1);
	if(n < 0){
		err = buf;
		goto out;
	}
	buf[n] = 0;
	hiscert = strtocert(buf);
	if(hiscert == H){
		err = "bad certificate";
		goto out;
	}
	certimmutable(hiscert);		/* hide from the garbage collector */

	n = getmsg(fd, buf, Maxbuf-1);
	if(n < 0){
		err = buf;
		goto out;
	}
	buf[n] = 0;
	hispk = strtopk(buf);
	if(hispk == H){
		err = "bad public key";
		goto out;
	}
	pkimmutable(hispk);		/* hide from the garbage collector */

	/* verify his public key */
	if(verify(spk, hiscert, buf, n) == 0){
		err = "pk doesn't match certificate";
		goto out;
	}

	/* check expiration date - in seconds of epoch */

		now = osusectime()/1000000;
		if(hiscert->x.exp != 0 && hiscert->x.exp <= now){
			err = "certificate expired";
			goto out;
		}


	/* sign alpha**r0 and alpha**r1 and send */
	n = bigtobase64(alphar0, buf, Maxbuf);
	n += bigtobase64(alphar1, buf+n, Maxbuf-n);
	alphacert = sign(mysk, "sha", 0, (uchar*)buf, n);
	n = certtostr(alphacert, buf, Maxbuf);
	if(sendmsg(fd, buf, n) <= 0){
		err = MSG;
		goto out;
	}
	certmutable(alphacert);
	destroy(alphacert);
	alphacert = H;

	/* get signature of alpha**r1 and alpha**r0 and verify */
	n = getmsg(fd, buf, Maxbuf-1);
	if(n < 0){
		err = buf;
		goto out;
	}
	buf[n] = 0;
	alphacert = strtocert(buf);
	if(alphacert == H){
		err = "alpha**r1 doesn't match certificate";
		goto out;
	}
	certimmutable(alphacert);		/* hide from the garbage collector */
	n = bigtobase64(alphar1, buf, Maxbuf);
	n += bigtobase64(alphar0, buf+n, Maxbuf-n);
	if(verify(hispk, alphacert, buf, n) == 0){
		err = "bad certificate";
		goto out;
	}

	/* we are now authenticated and have a common secret, alpha**(r0*r1) */
	f->ret->t0 = stringdup(hispk->x.owner);
	bigPow(alphar1, r0, p, alphar0r1);
	n = bigToBuf(alphar0r1, Maxbuf, (uchar*)buf);
	trim(alphar0r1);
	f->ret->t1 = mem2array(buf, n);

out:
	/* return status */
	if(f->ret->t0 == H)
		senderr(fd, err);
	else
		sendmsg(fd, "OK", 2);

	/* read responses */
	if(err != buf){
		for(;;){
			n = getmsg(fd, buf, Maxbuf-1);
			if(n < 0){
				destroy(f->ret->t0);
				f->ret->t0 = H;
				destroy(f->ret->t1);
				f->ret->t1 = H;
				if(err == nil){
					if(n < -1)
						err = buf;
					else
						err = MSG;
				}
				break;
			}
			if(n == 2 && buf[0] == 'O' && buf[1] == 'K')
				break;
		}
	}

	/* set error and id to nobody */
	if(f->ret->t0 == H){
		if(err == nil)
			err = MSG;
		retstr(err, &(f->ret->t0));
		if(f->setid)
			setid("nobody");
	} else {
		/* change user id */
		if(f->setid)
			setid(string2c(f->ret->t0));
	}
	
	/* free resources */
	if(hispk != H){
		pkmutable(hispk);
		destroy(hispk);
	}
	if(hiscert != H){
		certmutable(hiscert);
		destroy(hiscert);
	}
	if(alphacert != H){
		certmutable(alphacert);
		destroy(alphacert);
	}
	free(buf);
	if(low != nil){
		freeBignum(low);
		freeBignum(r0);
		freeBignum(r1);
		freeBignum(alphar0);
		freeBignum(alphar1);
		freeBignum(alphar0r1);
	}
}

static Keyring_Authinfo*
newAuthinfo(void)
{
	return H2D(Keyring_Authinfo*, heap(TAuthinfo));
}

void
Keyring_writeauthinfo(void *fp)
{
	F_Keyring_writeauthinfo *f;
	int n, fd;
	char *buf;
	PK *spk;
	SK *mysk;
	Certificate *c;

	f = fp;
	*f->ret = -1;

	if(f->filename == H)
		return;
	if(f->info == H)
		return;
	if(f->info->alpha == H || f->info->p == H)
		return;
	if(((IPint*)f->info->alpha)->b == 0 || ((IPint*)f->info->p)->b == H)
		return;
	spk = checkPK(f->info->spk);
	mysk = checkSK(f->info->mysk);
	c = checkCertificate(f->info->cert);

	buf = malloc(Maxbuf);
	if(buf == nil)
		return;

	/*
	 *  The file may already exist or be a file2chan file so first
	 *  try opening with trucation since create will change the
	 *  permissions of the file and create doesn't work with a
	 *  file2chan.
	 */
	release();
	fd = kopen(string2c(f->filename), OTRUNC|OWRITE);
	if(fd < 0)
		fd = kcreate(string2c(f->filename), OWRITE, 0600);
	if(fd < 0)
		fd = kopen(string2c(f->filename), OWRITE);
	acquire();
	if(fd < 0)
		goto out;

	/* signer's public key */
	n = pktostr(spk, buf, Maxmsg);
	if(sendmsg(fd, buf, n) <= 0)
		goto out;

	/* certificate for my public key */
	n = certtostr(c, buf, Maxmsg);
	if(sendmsg(fd, buf, n) <= 0)
		goto out;

	/* my secret/public key */
	n = sktostr(mysk, buf, Maxmsg);
	if(sendmsg(fd, buf, n) <= 0)
		goto out;

	/* diffie hellman base */
	n = bigtobase64(((IPint*)f->info->alpha)->b, buf, Maxbuf);
	if(sendmsg(fd, buf, n) <= 0)
		goto out;

	/* diffie hellman modulus */
	n = bigtobase64(((IPint*)f->info->p)->b, buf, Maxbuf);
	if(sendmsg(fd, buf, n) <= 0)
		goto out;

	*f->ret = 0;
out:
	free(buf);
	release();
	kclose(fd);
	acquire();
}

void
Keyring_readauthinfo(void *fp)
{
	F_Keyring_readauthinfo *f;
	int fd;
	char *buf;
	int n, ok;
	PK *mypk;
	SK *mysk;
	SigAlg *sa;
	Keyring_Authinfo *ai;
	BigInt b;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	ok = 0;

	if(f->filename == H)
		return;

	buf = malloc(Maxbuf);
	if(buf == nil)
		return;

	ai = newAuthinfo();
	*f->ret = ai;

	release();
	fd = kopen(string2c(f->filename), OREAD);
	acquire();
	if(fd < 0)
		goto out;

	/* signer's public key */
	n = getmsg(fd, buf, Maxmsg);
	if(n < 0)
		goto out;

	ai->spk = (Keyring_PK*)strtopk(buf);
	if(ai->spk == H)
		goto out;

	/* certificate for my public key */
	n = getmsg(fd, buf, Maxmsg);
	if(n < 0)
		goto out;
	ai->cert = (Keyring_Certificate*)strtocert(buf);
	if(ai->cert == H)
		goto out;

	/* my secret/public key */
	n = getmsg(fd, buf, Maxmsg);
	if(n < 0)
		goto out;
	mysk = strtosk(buf);
	ai->mysk = (Keyring_SK*)mysk;
	if(mysk == H)
		goto out;
	sa = checkSigAlg(mysk->x.sa);
	mypk = newPK(sa, stringdup(mysk->x.owner), 1);
	mypk->key = (*sa->vec->sk2pk)(mysk->key);
	ai->mypk = (Keyring_PK*)mypk;

	/* diffie hellman base */
	n = getmsg(fd, buf, Maxmsg);
	if(n < 0)
		goto out;
	b = base64tobig(buf, 0);
	ai->alpha = newIPint(b);

	/* diffie hellman modulus */
	n = getmsg(fd, buf, Maxmsg);
	if(n < 0)
		goto out;
	b = base64tobig(buf, 0);
	ai->p = newIPint(b);
	ok = 1;
out:
	if(!ok){
		destroy(*f->ret);
		*f->ret = H;
	}
	free(buf);
	if(fd >= 0){
		release();
		kclose(fd);
		acquire();
	}
}

void
keyringmodinit(void)
{
	extern SigAlgVec* elgamalinit(void);

	TIPint = dtype(freeIPint, sizeof(IPint), IPintmap, sizeof(IPintmap));
	TSigAlg = dtype(freeSigAlg, sizeof(SigAlg), SigAlgmap, sizeof(SigAlgmap));
	TSK = dtype(freeSK, sizeof(SK), SKmap, sizeof(SKmap));
	TPK = dtype(freePK, sizeof(PK), PKmap, sizeof(PKmap));
	TCertificate = dtype(freeCertificate, sizeof(Certificate), Certificatemap,
		sizeof(Certificatemap));
	TDigestState = dtype(freeheap, sizeof(XDigestState), DigestStatemap,
		sizeof(DigestStatemap));
	TDESstate = dtype(freeheap, sizeof(XDESstate), DESstatemap,
		sizeof(DESstatemap));
	TAuthinfo = dtype(freeheap, sizeof(Keyring_Authinfo), Authinfomap, sizeof(Authinfomap));

	algs[nalg++] = elgamalinit();

	fmtinstall('U', big64conv);
	builtinmod("$Keyring", Keyringmodtab);
}

/*
 *  IO on a delimited channel.  A message starting with 0x00 is a normal
 *  message.  One starting with 0xff is an error string.
 *
 *  return negative number for error messages (including hangup)
 */
static int
getbuf(int fd, uchar *buf, int n, char *err, int nerr)
{
	int len;

	release();
	len = kread(fd, buf, n);
	acquire();
	if(len <= 0){
		strncpy(err, "hungup", nerr);
		buf[nerr-1] = 0;
		return -1;
	}
	if(buf[0] == 0)
		return len-1;
	if(buf[0] != 0xff){
		strncpy(err, "failure", nerr);
		err[nerr-1] = 0;
		return -1;
	}

	/* error string */
	len--;
	if(len < 1){
		strncpy(err, "unknown", nerr);
		err[nerr-1] = 0;
	} else {
		if(len >= nerr)
			len = nerr-1;
		memmove(err, buf+1, len);
		err[len] = 0;
	}
	return -1;
}

void
Keyring_getstring(void *fp)
{
	F_Keyring_getstring *f;
	uchar *buf;
	char err[64];
	int n;

	f = fp;
	destroy(f->ret->t0);
	f->ret->t0 = H;
	destroy(f->ret->t1);
	f->ret->t1 = H;

	if(f->fd == H)
		return;

	buf = malloc(Maxmsg);
	if(buf == nil)
		return;

	n = getbuf(f->fd->fd, buf, Maxmsg, err, sizeof(err));
	if(n < 0)
		retnstr(err, strlen(err), &f->ret->t1);
	else
		retnstr(((char*)buf)+1, n, &f->ret->t0);

	free(buf);
}

void
Keyring_getbytearray(void *fp)
{
	F_Keyring_getbytearray *f;
	uchar *buf;
	char err[64];
	int n;

	f = fp;
	destroy(f->ret->t0);
	f->ret->t0 = H;
	destroy(f->ret->t1);
	f->ret->t1 = H;

	if(f->fd == H)
		return;

	buf = malloc(Maxmsg);
	if(buf == nil)
		return;

	n = getbuf(f->fd->fd, buf, Maxmsg, err, sizeof(err));
	if(n < 0)
		retnstr(err, strlen(err), &f->ret->t1);
	else
		f->ret->t0 = mem2array(buf+1, n);

	free(buf);
}

static int
putbuf(int fd, void *p, int n)
{
	char *buf;

	buf = malloc(Maxmsg);
	if(buf == nil)
		return -1;

	release();
	buf[0] = 0;
	if(n < 0){
		buf[0] = 0xff;
		n = -n;
	}
	if(n >= Maxmsg)
		n = Maxmsg - 1;
	memmove(buf+1, p, n);
	n = kwrite(fd, buf, n+1);
	acquire();

	free(buf);
	return n;
}

void
Keyring_putstring(void *fp)
{
	F_Keyring_putstring *f;

	f = fp;
	*f->ret = -1;
	if(f->fd == H || f->s == H)
		return;
	*f->ret = putbuf(f->fd->fd, string2c(f->s), strlen(string2c(f->s)));
}

void
Keyring_puterror(void *fp)
{
	F_Keyring_puterror *f;

	f = fp;
	*f->ret = -1;
	if(f->fd == H || f->s == H)
		return;
	*f->ret = putbuf(f->fd->fd, string2c(f->s), -strlen(string2c(f->s)));
}

void
Keyring_putbytearray(void *fp)
{
	F_Keyring_putbytearray *f;
	int n;

	f = fp;
	*f->ret = -1;
	if(f->fd == H || f->a == H)
		return;
	n = f->n;
	if(n > f->a->len)
		n = f->a->len;
	*f->ret = putbuf(f->fd->fd, f->a->data, n);
}

void
Keyring_dessetup(void *fp)
{
	F_Keyring_dessetup *f;
	Heap *h;
	XDESstate *ds;
	uchar *ivec;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;

	if(f->key == (Array*)H)
		return;
	if(f->ivec == (Array*)H)
		ivec = 0;
	else
		ivec = f->ivec->data;

	h = heap(TDESstate);
	ds = H2D(XDESstate*, h);

#ifndef NOSPOOKS
	if (f->key->len >= 8) {
		f->key->data[0] &= 0x0f;
		f->key->data[2] &= 0x0f;
		f->key->data[4] &= 0x0f;
		f->key->data[6] &= 0x0f;
	}
#endif NOSPOOKS

	setupDESstate(&ds->state, f->key->data, ivec);

	*f->ret = (Keyring_DESstate*)ds;
}

void
Keyring_desecb(void *fp)
{
	F_Keyring_desecb *f;
	XDESstate *ds;
	int i;
	uchar *p;
	uchar tmp[8];

	f = fp;

	if(f->state == (Keyring_DESstate*)H)
		return;
	if(f->buf == (Array*)H)
		return;
	if(f->buf->len < f->n)
		f->n = f->buf->len;

	ds = (XDESstate*)f->state;
	p = f->buf->data;

	for (i = 8; i <= f->n; i += 8, p += 8)
		block_cipher(ds->state.expanded, p, f->direction);
	
	if ((f->n & 7) != 0) {
		for (i=0; i<8; i++)
			tmp[i] = i;
		block_cipher(ds->state.expanded, tmp, 0);
		for (i=0; i<(f->n&7); i++)
			p[i] ^= tmp[i];
	}
}

void
Keyring_descbc(void *fp)
{
	F_Keyring_descbc *f;
	XDESstate *ds;
	uchar *p, *ep, *ip, *p2, *eip;
	uchar tmp[8];

	f = fp;

	if(f->state == (Keyring_DESstate*)H)
		return;
	if(f->buf == (Array*)H)
		return;
	if(f->buf->len < f->n)
		f->n = f->buf->len;

	ds = (XDESstate*)f->state;
	p = f->buf->data;

	if(f->direction == 0){
		for(ep = p + f->n; p < ep; p += 8){
			p2 = p;
			ip = ds->state.ivec;
			for(eip = ip+8; ip < eip; )
				*p2++ ^= *ip++;
			block_cipher(ds->state.expanded, p, 0);
			memmove(ds->state.ivec, p, 8);
		}
	} else {
		for(ep = p + f->n; p < ep; p += 8){
			memmove(tmp, p, 8);
			block_cipher(ds->state.expanded, p, 1);
			p2 = tmp;
			ip = ds->state.ivec;
			for(eip = ip+8; ip < eip; ){
				*p++ ^= *ip;
				*ip++ = *p2++;
			}
		}
	}
}
