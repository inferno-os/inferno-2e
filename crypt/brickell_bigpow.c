/*
 *  Modular exponentiation using Montgomery reduction and
 *  Addition chaining.
 *  coded by Jack Lacy 11/91.
 *  Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

#define NIBBLE(B,N) (((NUM(B)[(N) >> 3] >> (((N) & 7) << 2)) & 15))
#define NIBSPERCHUNK 8

Table *
g16_bigpow(BigInt a, BigInt modulus, NumType explength)
{
	Table *g16_table;
	Mont_set *ms;
	BigInt c, d;
	int i;
	
	ms = mont_set(modulus);
	g16_table = (Table *)crypt_malloc(sizeof(Table)+((unsigned)(sizeof(Bignum)*(explength-2))));
	g16_table->length = (int)explength;
	
	c = res_form(a, ms);
	g16_table->t[0] = bigInit(0);
	bigCopy(c, g16_table->t[0]);
	
	d = bigInit(0);
	i = (int)explength-1;
	for (;; --i) {
		
		if (i == 0)
			break;
		
		bigMultiply(c, c, d);
		REDC(d, ms);
		
		bigMultiply(d, d, c);
		REDC(c, ms);
		
		bigMultiply(c, c, d);
		REDC(d, ms);
		
		bigMultiply(d, d, c);
		REDC(c, ms);
		
		g16_table->t[(int)explength-i] = bigInit(0);
		bigCopy(c, g16_table->t[(int)explength-i]);
	}
	
	freeMs(ms);
	freeBignum(c);
	freeBignum(d);
	
	return g16_table;
}

static Table *
buildCoeffTable(BigInt exp, Mont_set *ms, Table *g16_table)
{
	BigInt tmp1, tmp2, ms_one;
	BigInt one;
	Table *C;
	int i, j;
	int numnibs;
	
	tmp1 = bigInit(0);
	tmp2 = bigInit(0);
	one = bigInit(1);
	ms_one = res_form(one, ms);
	freeBignum(one);
	
	C = (Table *)crypt_malloc(sizeof(Table)+(sizeof(Bignum)*(16-2)));
	C->length = 16;
	C->t[0] = bigInit(1);
	
	numnibs = (int)g16_table->length;
	
	for (j=1; j<16; j++) {
		bigCopy(ms_one, tmp1);
		
		for (i=numnibs-1; i>=0; i--) {
			if (NIBBLE(exp, i) == j) {
				bigMultiply(g16_table->t[i], tmp1, tmp2);
				REDC(tmp2, ms);
				bigCopy(tmp2, tmp1);
			}
		}
		C->t[j] = bigInit(0);
		bigCopy(tmp1, C->t[j]);
	}
	freeBignum(ms_one);
	freeBignum(tmp1);
	freeBignum(tmp2);
	
	return C;
}

void
brickell_bigpow(Table *g16_table, BigInt exp, BigInt modulus, BigInt result)
{
	Table *C;
	BigInt d, tmp;
	Mont_set *ms;
	int i;
	
	ms = mont_set(modulus);
	C = buildCoeffTable(exp, ms, g16_table);
	
	tmp = bigInit(0);
	d = bigInit(0);
	bigCopy(C->t[15], d);
	bigCopy(d, result);
	
	for (i=14; i>0; i--) {
		bigMultiply(C->t[i], d, tmp);
		REDC(tmp, ms);
		bigCopy(tmp, d);
		
		bigMultiply(result, d, tmp);
		REDC(tmp, ms);
		bigCopy(tmp, result);
	}
	REDC(result, ms);
	
	freeTable(C);
	freeMs(ms);
	freeBignum(d);
	freeBignum(tmp);
}
