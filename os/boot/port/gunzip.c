#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "inflate.h"
#include "gzip.h"


/* XXX - This is REALLY BAD... order is not defined -- needs to be fixed */
#define GetByte(s)	(sgetc(s))
#define GetWord(s)	((GetByte(s) << 8) | GetByte(s))
#define GetLong(s)	((GetWord(s) << 16) | GetWord(s))


char gzip_name[128];


int gunzip_header(Istream *s)
{
	uchar flags;     /* compression flags */
 	ushort magic; /* magic header */
	ulong stamp;     /* time stamp */
	int method;

    	magic = GetWord(s);
	if(magic == -1)
		return OK;

	if(magic != GZIP_MAGIC && magic != OLD_GZIP_MAGIC)
		return NOTGZIP;
	
	method = GetByte(s);
	if (method != CM_DEFLATED) 
	    return ERROR;
	flags  = GetByte(s);

	if (((flags & ENCRYPTED) != 0) ||
            ((flags & CONTINUATION) != 0) ||
            ((flags & RESERVED) != 0)) {
           return ERROR;
	}

	stamp  = GetLong(s);
	USED(stamp);

	GetByte(s);  /* Ignore extra flags for the moment */
	GetByte(s);  /* Ignore OS type for the moment */

	if ((flags & CONTINUATION) != 0) {
	    unsigned part = GetWord(s);
	    USED(part);
	}
	if ((flags & EXTRA_FIELD) != 0) {
	    unsigned len = GetWord(s);
	    while (len--) GetByte(s);
	}

	/* Get original file name if it was truncated */
	if ((flags & ORIG_NAME) != 0) {
		char *cp = gzip_name;
		while((*cp++ = GetByte(s)))
			continue;
	} else
		gzip_name[0] = 0;

	/* Discard file comment if any */
	if ((flags & COMMENT) != 0) {
	    while (GetByte(s) != 0) /* null */ ;
	}
    	return method; 
}


int
status_sgetc(Istream *s)
{
	if(!(s->pos & 0x3fff))
		status("uncompressing", s->pos, s->size);
	return sgetc(s);
}

int
gunzip(Istream *is, Ostream *os)
{
	int method;
	struct inflate g;

	g.gz_id = is;
	g.gz_od = os;
	g.gz_getc = (int (*)(void*))status_sgetc;
	g.gz_write = (int (*)(void*, const void*, int))swrite;
	g.gz_slide = (uchar*)malloc(GZ_WSIZE);
	g.gz_crc = 0;
	g.gz_len = 0;
	/* we don't support multiple parts, since we don't even know
	   the true total size necessarily... */
	if((method = gunzip_header(is)) > 0) {
		ulong orig_crc;
		ulong orig_len;
		inflate(&g);
		orig_crc = GetLong(is);
		orig_len = GetLong(is);
		if(g.gz_crc != orig_crc || g.gz_len != orig_len) {
			print("CRC/SIZE error: CRC:%x(%x) len:%d(%d)",
				g.gz_crc, orig_crc,
				g.gz_len, orig_len);
			free(g.gz_slide);
			return -1;
		}
	}
	status("uncompressed", is->pos, is->size);
	if(method == ERROR)
		print("Error in GZIP format\n");
	free(g.gz_slide);
	return g.gz_len;
}

