#include "lib9.h"
#include <libcrypt.h>

/* m-bit Cipher Feedback Mode support functions */

uchar cfm_mask[8] = {
	0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff,
};

#define RBITSHIFT(i, ap, bp) {\
	el = ap[i];\
	s = el >> n;\
	bp[i] = s + c;\
	c = el << j;\
}

static void
rightshift(uchar in[8], int m, uchar out[8])
{
	uchar el, s, c;
	int i, j;
	ulong n;
	
	for (i=(m/8), j=0; i<8; i++, j++) 
		out[j] = in[i];
	
	n = m%8;
	j = 8-n;
	c = 0;
	for (i=7; i>=0; i--) {
		RBITSHIFT(i, out, out);
	}
	
}

#define LBITSHIFT(i, ap, bp) {\
	el = ap[i];\
	s = (el << n);\
	bp[i] = s + c;\
	c = (el >> j);\
}

static void
leftshift(uchar in[8], int m, uchar out[8])
{
	uchar el, s, c;
	uchar tmp[8];
	int i, j, n;
	
	memset(tmp, 0, 8);
	for (i=(m/8), j=0; i<8; i++, j++) 
		tmp[i] = in[j];
	
	n = m%8;
	j = 8-n;
	c = 0;
	for (i=0; i<8; i++) {
		LBITSHIFT(i, tmp, out);
	}
	
}

static void
leftmost_mbits_xor(uchar in[8], uchar sreg[8], int m)
{
	uchar tmp[8];
	int i;
	
	rightshift(sreg, 64-m, tmp);
	for (i=0; i<(m/8); i++)
		in[i] ^= tmp[i];
	if (m%8)
		in[i] = (in[i] & cfm_mask[(m%8)-1]) ^ tmp[i];
	
	memset(tmp, 0, 8);
}


static void
mbit_loadreg(uchar in[8], uchar reg[8], int m)
{
	int i;
	
	for (i=0; i<(m/8); i++)
		reg[i] = in[i];
	reg[i] |= (in[i] & cfm_mask[(m%8)-1]);
	
}


void
mbitCFMEncrypt(uchar in[8], uchar sreg[8], ulong key[32], int m)
{
	
	block_cipher(key, sreg, 0);
	leftmost_mbits_xor(in, sreg, m);
	leftshift(sreg, m, sreg);
	
}

void
mbitCFMDecrypt(uchar in[8], uchar sreg[8], ulong key[32], int m)
{
	
	leftshift(sreg, m, sreg);
	mbit_loadreg(in, sreg, m);
	block_cipher(key, sreg, 0);
	leftmost_mbits_xor(in, sreg, m);
	
}
