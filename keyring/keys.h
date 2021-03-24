typedef struct IPint IPint;
typedef struct SigAlg SigAlg;
typedef struct SigAlgVec SigAlgVec;
typedef struct SK SK;
typedef struct PK PK;
typedef struct Certificate Certificate;
typedef struct XDigestState XDigestState;
typedef struct XDESstate XDESstate;

enum
{
	AlgDSS,
	AlgEG,

	Maxbuf=	4096,
	MaxBigBytes = 1024
};

/* infininite precision integer */
struct IPint
{
	Keyring_IPint x;
	BigInt	b;
};

/* generic certificate */
struct Certificate
{
	Keyring_Certificate x;
	void		*signa;	/* actual signature */
};

/* generic public key */
struct PK
{
	Keyring_PK	x;
	void		*key;	/* key and system parameters */
};

/* digest state */
struct XDigestState
{
	Keyring_DigestState	x;
	DigestState	state;
};

/* DES state */
struct XDESstate
{
	Keyring_DESstate	x;
	DESstate	state;
};

/* generic secret key */
struct SK
{
	Keyring_SK	x;
	void		*key;	/* key and system parameters */
};

struct SigAlgVec {
	char	*name;

	void*	(*str2sk)(char*, char**);
	void*	(*str2pk)(char*, char**);
	void*	(*str2sig)(char*, char**);

	int	(*sk2str)(void*, char*, int);
	int	(*pk2str)(void*, char*, int);
	int	(*sig2str)(void*, char*, int);

	void*	(*sk2pk)(void*);

	void*	(*gensk)(int);
	void*	(*genskfrompk)(void*);
	void*	(*sign)(BigInt, void*);
	int	(*verify)(BigInt, void*, void*);

	void	(*skfree)(void*);
	void	(*pkfree)(void*);
	void	(*sigfree)(void*);
};

struct SigAlg
{
	Keyring_SigAlg	x;
	SigAlgVec	*vec;
};

int	bigtobase64(BigInt b, char *buf, int blen);
BigInt	base64tobig(char *str, char **strp);
SigAlg*	strtoalg(char *str, char **strp);
String*	strtostring(char *str, char **strp);
Keyring_IPint*	newIPint(BigInt);
