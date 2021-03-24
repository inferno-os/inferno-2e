/* gzip.h -- common declarations for all gzip modules
 * Copyright (C) 1992-1993 Jean-loup Gailly.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */


/* Return codes from gzip */
#define OK      0
#define ERROR   -1
#define WARNING -2
#define NOTGZIP	-3

/* Compression methods (see algorithm.doc) */
#define CM_STORED   0
#define CM_COMPRESSED  1
#define CM_PACKED      2
#define CM_LZHED       3
/* methods 4 to 7 reserved */
#define CM_DEFLATED    8
#define CM_MAX_METHODS 9


#define	PACK_MAGIC     0x1e1f /* Magic header for packed files \037\036 */
#define	GZIP_MAGIC     0x8b1f /* Magic header for gzip files, \037\213 */
#define	OLD_GZIP_MAGIC 0x9e1f /* Mgc hdr for gzip 0.5 = freeze 1.x \037\236 */
#define	LZH_MAGIC      0xa01f /* Magic header for SCO LZH Compress files*/
#define PKZIP_MAGIC    0x04034b50 /* pkzip files \120\113\003\004 */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02 /* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

/* internal file attribute */
#define UNKNOWN 0xffff
#define BINARY  0
#define ASCII   1

