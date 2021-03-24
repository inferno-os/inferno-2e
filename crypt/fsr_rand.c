#ifdef NOSPOOKS
/*
 *	Feedback shift pseudorandom number
 *	generator by D.P. Mitchell and Jack Lacy.
 *	Copyright (c) 1991 Bell Laboratories
 */
#include "lib9.h"
#include <libcrypt.h>

/* Feedback Shift Register and DES based pseudoRand */

#define FSR_MAX 7
#define UPPER_INDEX 6
#define LOWER_INDEX 2

static ulong RANDKEY[32];
static ulong fsr[FSR_MAX];
static int rand_key_set = 0;
static int fsr_loaded = 0;
static void init_fsrrand(void);

static ulong init_fsr[55] = {
	0x491fcfddUL,0xf36ad4bcUL,0x27adffddUL,0x8bc1bfdeUL,0xcc4cd9afUL,
	0x942e7bcfUL,0x59115271UL,0x7eb44cc2UL,0x08224962UL,0x74bcb091UL,
	0xfd9efedcUL,0x08e8135cUL,0xa12e27a5UL,0x0c06367fUL,0x67966a76UL,
	0x692c9559UL,0x629d82b2UL,0x62363f36UL,0xbfff3330UL,0xea1242afUL,
	0x59e1d1b0UL,0x282ee969UL,0x0f1399dfUL,0x97aceac7UL,0x07c44f1eUL,
	0x0afd53eaUL,0x0bef3079UL,0xd6662669UL,0x735be859UL,0xc591eabfUL,
	0x84ff845fUL,0x8a894a78UL,0x34c8f257UL,0xb85bce47UL,0x65989b54UL,
	0x87ac1eb4UL,0x06949770UL,0x0dd27f9eUL,0xda749d55UL,0xa360a339UL,
	0x05829596UL,0x00da965fUL,0x5b4166e7UL,0x4b57d1f5UL,0x4c20d47cUL,
	0x0aca3e61UL,0x078f2e19UL,0x06519a99UL,0x6c244574UL,0x19472f3aUL,
	0x375c6265UL,0x1625869bUL,0x09c0cf3dUL,0x092556c3UL,0x26448cfaUL,
};

/*
   static void
   fsr_print() {
   long i;
   Biobuf *fp = stdout;
   
   if(stdout == 0)
	stdout = Bopen("/fd/1", OWRITE);
   for (i=0; i<FSR_MAX; i++) {
   Bprint(fp, "%08lx\n",fsr[i]);
   }
   }
   */

void
seed_fsr(uchar *seed, int seedlen)
{
	int i, j;
	int log_fsrmax = 3; /* log 7 */
	uchar rk[8];
	
	if (rand_key_set == 0) {
		key_crunch((uchar *)seed, seedlen, rk);
		key_setup(rk, RANDKEY);
		for (i=0; i<8; i++)
			rk[i] = 0;
		rand_key_set = 1;
	}
	
	for (i=0; i<FSR_MAX; i++) {
		fsr[i] = init_fsr[i];
	}
	
	for (i=0; i<seedlen; i++) {
		fsr[i%FSR_MAX] ^= ((ulong)(seed[i]&0xff) << ((i%4)*8));
	}
	
	fsr_loaded = 1;
	for (i=0; i<log_fsrmax; i++)
		for (j=0; j<FSR_MAX; j++)
			fsrRandom();
}

static void
init_fsrrand(void)
{
	uchar *load_vector, *rk;
	int i;

	rk = (uchar *)crypt_malloc(8);
	load_vector = (uchar *)crypt_malloc(28);
	randomBytes(rk, 8, REALLY);
	key_setup((uchar *)rk, RANDKEY);
	for (i=0; i<8; i++)
		rk[i] = 0;
	rand_key_set = 1;
	
	randomBytes(load_vector, 28, REALLY);
	seed_fsr((uchar *)load_vector, 28);
	
	for (i=0; i<28; i++)
		load_vector[i] = 0;
	crypt_free(rk);
	crypt_free(load_vector);
	fsr_loaded = 1;
}

static int li = LOWER_INDEX;
static int ui = UPPER_INDEX;

ulong
fsrRandom(void)
{
	ulong uip, lip;
	static uchar block[8];
	ulong retval;
	int i;
	
	if (fsr_loaded == 0) {
		init_fsrrand();
	}
	uip = fsr[ui];
	lip = fsr[li];
	for (i=0; i<4; i++) {
		block[i] = (uchar)((uip >> (i*8)) & 0xff);
		block[i+4] = (uchar)((lip >> (i*8)) & 0xff);
	}
	
	block_cipher(RANDKEY, block, 0);
	
	retval = block[3];
	for (i=2; i >= 0; i--)
		retval = (retval << 8) + (ulong)(block[i] & 0xFF);
	
	uip = block[7];
	for (i=6; i>=4; i--)
		uip = (uip << 8) + (ulong)(block[i] & 0xff);
	
	fsr[ui] ^= uip;
	
	li--;
	ui--;
	if (li == -1)
		li = UPPER_INDEX;
	else if (ui == -1)
		ui = UPPER_INDEX;
	
	
	return retval;
}
#endif NOSPOOKS
