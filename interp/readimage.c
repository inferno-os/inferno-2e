#include <lib9.h>
#include <kernel.h>
#include <pool.h>
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "raise.h"
#include "image.h"
#include "readimage.h"
#include "readimagemod.h"

#define DEB (1)
#define DPRINT if(DEB) print
void dump_buf(uchar *buf, int n, int x);

uchar iobufmap[] = Readimage_Iobuf_map;
uchar rawimagemap[] = Readimage_Rawimage_map;

Type*	TIobuf;
Type*	TRawimage;

void
readimagemodinit(void)
{
	TIobuf = dtype(freeheap, sizeof(Readimage_Iobuf), iobufmap, sizeof(iobufmap));
	TRawimage = dtype(freeheap, sizeof(Readimage_Rawimage), rawimagemap, sizeof(rawimagemap));
	builtinmod("$Readimage", Readimagemodtab);
}

#define ERRSTR(err, msg) if((err) != nil) { *(err) = c2string((msg), strlen(msg)); USED(err); };

enum {
	RFT_ERROR = -1,
	RFT_UNKNOWN = 0,
	RFT_GIF,
	RFT_JFIF,
	RFT_CBIT,
	RFT_BIT,
	RFT_PM,
	RFT_PBM,
	RFT_XBM,
	RFT_SUNRAS,
	RFT_BMP,
	RFT_UTAHRLE,
	RFT_IRIS,
	RFT_PCX,
	RFT_TIFF,
	RFT_PDSVICAR,
	RFT_PS,
	RFT_IFF,
	RFT_TARGA,
	RFT_XPM,
	RFT_XWD,
	RFT_FITS,
};

/* adapted from xv.c of the xv-3.10a distribution */
/* may want to reimplement this using nested switches to avoid so many */
/* calls to strncmp(), or at least include only a subset of these formats */
int imgformat(uchar magicno[30])
{
	if(DEB)dump_buf(magicno, 30, 1);
	if (strncmp((char*)magicno,"GIF87a", 6)==0 || strncmp((char*)magicno,"GIF89a", 6)==0)
		return RFT_GIF;

	else if(magicno[0]==0xff && magicno[1]==0xd8 && magicno[2]==0xff)
		return RFT_JFIF;

	else if (strncmp((char*)magicno,"compressed\n", 11)==0)
		return RFT_CBIT;

	else if (strncmp((char*)magicno,"          ", 10)==0)
		return RFT_BIT;

	else if((magicno[0]=='M' && magicno[1]=='M') ||
			(magicno[0]=='I' && magicno[1]=='I'))
		return RFT_TIFF;

	else if(magicno[0] == 'B' && magicno[1] == 'M')
		return RFT_BMP;

	/* note: have to check XPM before XBM, as first 2 chars are the same */
	else if(strncmp((char*)magicno, "/* XPM */", 9)==0)
		return RFT_XPM;

	else if(strncmp((char*)magicno,"#define", 7)==0 ||
			(magicno[0] == '/' && magicno[1] == '*'))
		return RFT_XBM;

	else if(strncmp((char*)magicno, "%!", 2)==0 ||
			strncmp((char*)magicno, "\004%!", 3)==0)
		return RFT_PS;

	else if(magicno[0] == 'P' && magicno[1]>='1' && magicno[1]<='6')
		return RFT_PBM;

	else if(magicno[0]==0x59 && (magicno[1]&0x7f)==0x26 &&
			magicno[2]==0x6a && (magicno[3]&0x7f)==0x15)
		return RFT_SUNRAS;

	else if(magicno[0]==0x52 && magicno[1]==0xcc)
		return RFT_UTAHRLE;

	else if((magicno[0]==0x01 && magicno[1]==0xda) ||
			(magicno[0]==0xda && magicno[1]==0x01))
		return RFT_IRIS;

	else if(magicno[0]==0x0a && magicno[1] <= 5)
		return RFT_PCX;

	else if(strncmp((char*)magicno,"VIEW", 4)==0 || strncmp((char*)magicno,"WEIV", 4)==0)
		return RFT_PM;

	else if(strncmp((char*)magicno, "FORM", 4)==0 && 
			strncmp((char*)magicno+8, "ILBM", 4)==0)
		return RFT_IFF;

	else if(magicno[0]==0 && magicno[1]==0 &&
			magicno[2]==2 && magicno[3]==0 &&
			magicno[4]==0 && magicno[5]==0 &&
			magicno[6]==0 && magicno[7]==0)
		return RFT_TARGA;

	else if(magicno[4]==0x00 && magicno[5]==0x00 &&
			magicno[6]==0x00 && magicno[7]==0x07)
		return RFT_XWD;

	else if(strncmp((char*)magicno,"SIMPLE  ", 8)==0 && magicno[29] == 'T')
		return RFT_FITS;
	
	else if(strncmp((char*)magicno,  "NJPL1I00", 8)==0 ||
			strncmp((char*)magicno+2,"NJPL1I", 6)==0 ||
			strncmp((char*)magicno,  "CCSD3ZF", 7)==0 ||
			strncmp((char*)magicno+2,"CCSD3Z", 6)==0 ||
			strncmp((char*)magicno,  "LBLSIZE=", 8)==0)
		return RFT_PDSVICAR;

	return RFT_UNKNOWN;
}

int
sane_iobuf(Readimage_Iobuf *fd, String **ret_str)
{
	/* Iobuf sanity check */
	if(fd == H) {
		DPRINT("readimagedata: nil input Iobuf\n");
		ERRSTR(ret_str, "nil input Iobuf");
		return 0;
	}
	if(fd->fd == H) {
		DPRINT("readimagedata: nil Iobuf fd\n");
		ERRSTR(ret_str, "nil input Iobuf");
		return 0;
	}
	if(fd->fd->fd < 0) { /* Iobuf -> Sys_FD -> fd */
		DPRINT("readimagedata: invalid Iobuf fd\n");
		ERRSTR(ret_str, "invalid Iobuf fd");
		return 0;
	}
	if(fd->buffer == H) {
		DPRINT("readimagedata: nil Iobuf buffer\n");
		ERRSTR(ret_str, "nil Iobuf buffer");
		return 0;
	}
	if(fd->buffer->len < 30) {
		DPRINT("readimagedata: buffer too small: %d\n", fd->buffer->len);
		ERRSTR(ret_str, "Iobuf buffer too small");
		return 0;
	}
	if(fd->size < 0) {
		DPRINT("readimagedata: negative size %d\n", fd->size);
		ERRSTR(ret_str, "negative Iobuf size");
		return 0;
	}
	if(fd->index < 0) {
		DPRINT("readimagedata: negative index %d\n", fd->index);
		ERRSTR(ret_str, "negative Iobuf index");
		return 0;
	}
	if(fd->index != fd->size) {
		if(fd->mode != Readimage_OREAD && fd->mode != Readimage_ORDWR) {
			DPRINT("readimagedata: Iobuf not open for reading\n");
			ERRSTR(ret_str, "Iobuf not open for reading");
			return 0;
		}
		if(fd->lastop != Readimage_OREAD) {
/* I should flush the data and do a write2read, but that's for another day */
			DPRINT("readimagedata: Iobuf lastop is not OREAD; call flush() first\n");
			ERRSTR(ret_str, "Iobuf lastop is not OREAD; call flush() first");
			return 0;
		}
		if(fd->size > fd->buffer->len) {
			DPRINT("readimagedata: size %d too large\n", fd->size);
			ERRSTR(ret_str, "Iobuf size too large");
			return 0;
		}
		if(fd->index > fd->size) {
			DPRINT("readimagedata: index %d too large\n", fd->index);
			ERRSTR(ret_str, "Iobuf index too large");
			return 0;
		}
	}
	return 1;
}

void
Readimage_readimagedata(void *fp)
{
	F_Readimage_readimagedata *f;
	Readimage_Iobuf *fd;
	int multi;
	Array **ret_arr;
	String **ret_str;
	int retn;

	f = fp;
	fd = f->fd;
	multi = f->multi;
	USED(multi);

	destroy(f->ret->t0);
	destroy(f->ret->t1);
	f->ret->t0 = H;
	f->ret->t1 = H;

	ret_arr = &(f->ret->t0);
	ret_str = &(f->ret->t1);

	DPRINT("readimagedata: checking iobuf\n");
	if(!sane_iobuf(fd, ret_str))
		return;

	DPRINT("readimagedata: getting image magic\n");
	/* make sure the magic 30 bytes will fit in the buffer */
	if(fd->index + 30 > fd->buffer->len) {
		fd->size -= fd->index;
		if(fd->size > 0)
			/* move data to buf[0] */
			memmove(fd->buffer->data, fd->buffer->data + fd->index, fd->size);
		fd->bufpos += fd->index;
		fd->index = 0;
		DPRINT("readimagedata: moved %d bytes to buffer[0]\n", fd->size);
	}
	/* make sure we have 30 bytes to examine */
	if(fd->size - fd->index < 30) {
		int filln = 30 - (fd->size - fd->index);
		if((retn = libreadn(fd->fd->fd, fd->buffer->data + fd->size, filln)) != filln) {
			DPRINT("readimagedata: kread(%d, buf, %d) returned %d reading magic\n", fd->fd->fd, filln, retn);
			ERRSTR(ret_str, "error reading image header");
			return;
		}
		fd->filpos += retn;
		fd->size += retn;
		DPRINT("readimagedata: read %d bytes for total buffer of %d\n", retn, fd->size - fd->index);
	}

	DPRINT("readimagedata: determining image type\n");
	switch (imgformat(fd->buffer->data + fd->index)) {
	case RFT_ERROR:
		DPRINT("readimagedata: error detecting image format\n");
		ERRSTR(ret_str, "error detecting image format");
		return;
	case RFT_GIF:
		DPRINT("readimagedata: detected a GIF\n");
		retn = readgifdata(fd, ret_arr, ret_str, multi);
		DPRINT("readimagedata: readgifdata returns\n");
		if(retn < 0) {
			DPRINT("readimagedata: error %d reading GIF file\n", retn);
			if(ret_str == H)
				ERRSTR(ret_str, "error reading GIF file");
			return;
		}
		break;
	case RFT_JFIF:
		DPRINT("readimagedata: detected a JPEG\n");
		retn = readjpegdata(fd, ret_arr, ret_str, multi);
		DPRINT("readimagedata: readjpegdata returns\n");
		if(retn < 0) {
			DPRINT("readimagedata: error %d reading JFIF file\n", retn);
			if(ret_str == H)
				ERRSTR(ret_str, "error reading JFIF file");
			return;
		}
		break;
	case RFT_CBIT:
		DPRINT("readimagedata: cannot decode compressed Inferno BIT files\n");
		ERRSTR(ret_str, "cannot decode compressed Inferno BIT files");
		return;
	case RFT_BIT:
		DPRINT("readimagedata: cannot decode Inferno BIT files\n");
		ERRSTR(ret_str, "cannot decode Inferno BIT files");
		return;
	case RFT_PM:
	case RFT_PBM:
	case RFT_XBM:
	case RFT_SUNRAS:
	case RFT_BMP:
	case RFT_UTAHRLE:
	case RFT_IRIS:
	case RFT_PCX:
	case RFT_TIFF:
	case RFT_PDSVICAR:
	case RFT_PS:
	case RFT_IFF:
	case RFT_TARGA:
	case RFT_XPM:
	case RFT_XWD:
	case RFT_FITS:
		DPRINT("readimagedata: unsupported image format\n");
		ERRSTR(ret_str, "unsupported image format");
		return;
	case RFT_UNKNOWN:
	default:
		DPRINT("readimagedata: unrecognized image format\n");
		ERRSTR(ret_str, "unrecognized image format");
		return;
	}

	DPRINT("readimagedata: returning to Dis code\n");
}

void
Readimage_remap(void *fp)
{
	F_Readimage_remap *f;
	Readimage_Rawimage *i;
	Draw_Display *d;
	int errdiff;
	Draw_Image **ret_img;
	String **ret_str;

	f = fp;
	i = f->i;
	d = f->d;
	errdiff = f->errdiff;

	destroy(f->ret->t0);
	destroy(f->ret->t1);
	f->ret->t0 = H;
	f->ret->t1 = H;

	ret_img = &(f->ret->t0);
	ret_str = &(f->ret->t1);

	remap(i, d, errdiff, ret_img, ret_str);
}

Array *
allocimgarray(Heap *cmapheap, Heap *chanheap)
{
	Array *a;
	Heap *h;
	Array *cmap, *chan0;
	Array *images;
	Readimage_Rawimage **rawimage;
	Array **channel;

	if(cmapheap != nil) {
		DPRINT("allocimgarray: making Heap* @ %lux (cmapheap) mutable\n", cmapheap);
		Setmark(cmapheap);
		poolmutable(cmapheap);
		cmap = H2D(Array*, cmapheap);
	}
	else cmap = H;
	if(chanheap != nil) {
		DPRINT("allocimgarray: making Heap* @ %lux (chanheap) mutable\n", chanheap);
		Setmark(chanheap);
		poolmutable(chanheap);
		chan0 = H2D(Array*, chanheap);
	}
	else chan0 = H;

	DPRINT("allocimagearray: allocating array[1] of ref\n");
	/* array[1] of ref Rawimage */
	h = nheap(sizeof(Array) + 1*sizeof(Readimage_Rawimage*));
	h->t = &Tarray;
	h->t->ref++;
	images = H2D(Array*, h);
	a = images;
	a->t = &Tptr;
	a->len = 1;
	a->root = H;
	a->data = (uchar*)a + sizeof(Array);
	initarray(&Tptr, a);

	DPRINT("allocimagearray: allocating Rawimage\n");
	/* Rawimage */
	h = heapz(TRawimage);
	rawimage = (Readimage_Rawimage**)(a->data);
	*rawimage = H2D(Readimage_Rawimage*, h);
	(*rawimage)->cmap = cmap;
	(*rawimage)->nchans = 1;

	DPRINT("allocimagearray: allocating array of array of byte\n");
	/* array of array of byte */
	h = nheap(sizeof(Array) + 1*sizeof(Array*));
	h->t = &Tarray;
	h->t->ref++;
	(*rawimage)->chans = H2D(Array*, h);
	a = (*rawimage)->chans;
	a->t = &Tptr;
	a->len = 1;
	a->root = H;
	a->data = (uchar*)a + sizeof(Array);
	initarray(&Tptr, a);
	channel = (Array**)(a->data);
	*channel = chan0;

	return images;
}

void
dump_buf(uchar *buf, int n, int x)
{
	int i, l=n;
	uchar *bp=buf, *p;
	int step=16;

	while(l > 0) {
		p = bp;
		fprint(2, "%ux: ", bp);
		if(x) {
			for(i=0; i<step; i++) {
				if(i<l) {
					fprint(2, "%.2ux ", *bp);
					bp++;
				}
				else
					fprint(2, "   ");
			}
			bp=p;
		}
		fprint(2, " |");
		for(i=0; i<step; i++) {
			if(i<l) {
				if((*bp)>=32&&(*bp)<=126)
					fprint(2, "%c", *bp);
				else
					fprint(2, ".");
				bp++;
			}
			else
				fprint(2, " ");
		}
		fprint(2, "|\n");
		l -= step;
	}
}
