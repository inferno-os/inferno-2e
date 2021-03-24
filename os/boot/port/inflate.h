/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: inflate.h,v 1.1 1998/01/29 14:01:29 shaggy Exp $
 *
 */
#ifndef _SYS_INFLATE_H_
#define _SYS_INFLATE_H_


#define GZ_WSIZE 0x8000

/*
 * Global variables used by inflate and friends.
 * This structure is used in order to make inflate() reentrant.
 */
struct inflate {
        /* Public part */

        /* This pointer is passed along to gz_getc */
        void           *gz_id;		/* input data */

        /* This pointer is passed along to gz_write */
        void           *gz_od;		/* output data */

        /* Fetch next character to be uncompressed */
        int             (*gz_getc) (void *);

        /* Dispose of uncompressed characters */
        int             (*gz_write) (void *, const void *, int);

	/* Semi-private part */
        uchar          *gz_slide;	/* sliding window buffer */
	ulong		gz_crc;		/* crc running total */
	ulong		gz_len;		/* length of uncompressed data */

        /* Private part (don't peek!) */
        ulong           gz_bb;  	/* bit buffer */
        unsigned        gz_bk;  	/* bits in bit buffer */
        unsigned        gz_hufts;       /* track memory usage */
        struct huft    *gz_fixed_tl;    /* must init to NULL !! */
        struct huft    *gz_fixed_td;
        int             gz_fixed_bl;
        int             gz_fixed_bd;
        unsigned        gz_wp;
};

int inflate(struct inflate *);


#endif  /* ! _SYS_INFLATE_H_ */
