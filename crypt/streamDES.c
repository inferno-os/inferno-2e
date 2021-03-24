#include "lib9.h"
#include <libcrypt.h>

void
streamEncrypt(uchar *buf, uchar mask[8], int *len, uchar *key, int *flag)
{
	uchar *block, tmp[8];
	int i, j, length;
	
	length = *len;
	
	if (*flag) {
		for (i=0; i<8; i++) {
			mask[i] = 0;
		}
		*flag = 0;
	}
	i = 8;
	
	block = buf;
	while (i <= length) {
		blockOFMEncrypt(block, mask, key, (int)8);
		i += 8;
		block += 8;
		
	}
	if ((length & 7) != 0) {
		for (j=0; j<8; j++)
			tmp[j] = j;
		blockOFMEncrypt(tmp, mask, key, (int)8);
		for (j=0; j<(length&7); j++)
			block[j] ^= tmp[j];
		*flag = 1;
	}
	
}

void
streamDecrypt(uchar *buf, uchar *mask, int *len, uchar *key, int *flag)
{
	streamEncrypt(buf, mask, len, key, flag);
}
