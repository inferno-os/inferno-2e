#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "ttystream.h"

static int
tty_read(Istream *_s, void *va, int len)
{
	TtyIstream *s = (TtyIstream*)_s;
	uchar *a = (uchar*)va;
	int c;
	if(s->rpos >= s->lpos) {
		s->rpos = 0;
		s->lpos = 0;
		do {
			switch((c = sgetc(s->in))) {
			case '\b':
			case 127:	/* delete */
				if(s->lpos > 0) {
					s->lpos--;
					if(s->flags & TTYSTREAM_COPY_BS) {
						sputc(s->out, '\\');
						sputc(s->out, s->lbuf[s->lpos]);
						sputc(s->out, '\\');
					} else if(s->flags & TTYSTREAM_ECHO_BS)
						sputs(s->out, "\b \b");
				}
				break;
			case 21:	/* ctrl-u */
				if(s->flags & TTYSTREAM_COPY_BS) {
					sputs(s->out, "\\");
					while(s->lpos-- > 0)
						sputc(s->out, s->lbuf[s->lpos]);
					sputs(s->out, "\\");
				}
				if(s->flags & TTYSTREAM_ECHO_BS)
					while(s->lpos-- > 0) 
						sputs(s->out, "\b \b");
				s->lpos = 0;
				break;
			case '\r':	
			case '\n':
				if(s->flags & TTYSTREAM_ECHO_NL)
					sputc(s->out, '\n');
				s->lbuf[s->lpos++] = '\n';
			case -1:
			case 4:	/* ctrl-d */
			case 0:
				c = 0;
				break;
			default:
				if(s->flags & TTYSTREAM_ECHO_NORM)
					sputc(s->out, c);
				if(s->lpos < sizeof(s->lbuf)-1)
					s->lbuf[s->lpos++] = c;
			}
			soflush(stdout);
		} while(c);
	}
	c = s->lpos - s->rpos;
	c = c < len ? c : len;
	memcpy(a, &s->lbuf[s->rpos], c);
	s->rpos += c;
	return c;
}


int
tty_openi(TtyIstream *s, Istream *in, Ostream *out, int flags)
{
	s->in = in;
	s->out = out;
	s->read = tty_read;
	s->close = nil;
	s->lpos = 0;
	s->rpos = 0;
	s->flags = flags;
	return 0;
}

void
ttystreamlink(void)
{
}

