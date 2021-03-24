typedef struct TtyIstream {
	Istream;
	Istream *in;
	Ostream *out;
	uchar lbuf[256];
	int lpos;
	int rpos;
	int flags;
} TtyIstream;

int tty_openi(TtyIstream *, Istream *, Ostream *, int);

enum {
	TTYSTREAM_ECHO_NORM 	= 0x0001,
	TTYSTREAM_ECHO_NL	= 0x0002,
	TTYSTREAM_ECHO_BS	= 0x0004,
	TTYSTREAM_COPY_BS	= 0x0008,
	TTYSTREAM_ECHO	= 	 TTYSTREAM_ECHO_NORM
				|TTYSTREAM_ECHO_NL	
				|TTYSTREAM_ECHO_BS,
};
