/*
 *	Marsaglia's SWC random number generator.
 *	Implemented by D. P. Mitchell
 *	Copyright (c) 1993 Bell Labs
 */
#include "lib9.h"
#include <libcrypt.h>

#define M        (1<<31)
#define R1        48
#define S1        8
#define LOGR1        6
#define R2        48
#define S2        8
#define LOGR2        6

long longRand1(void);
long longRand2(void);

/*
 *        physically-random initial state
 */
static long randomData1[48] = {
1947146662,255872804,611859725,1698768494,1168331706,903266320,
156960988,2108094856,1962943837,190733878,1611490366,1064800627,
750133665,1388252873,1292226713,2024731946,1741633152,1048965367,
1242081949,648944060,2146570054,1918590288,1154878382,1659238901,
1517088874,1343505038,702442230,223319626,2112415214,1198445789,
1667134997,81636584,1477696870,1396665379,296474908,221800638,
285530547,506210295,322166150,242833116,542659480,1371231536,
164404762,580757470,1564914124,2004579430,389459662,1039937051,
};
static long vec1[48];
static int feed1, tap1, borrow1 = 0;
static int rand_seeded = 0;

static long randomData2[48] = {
0x491fcfdd,0xf36ad4bc,0x27adffdd,0x8bc1bfde,0xcc4cd9af,
0x942e7bcf,0x59115271,0x7eb44cc2,0x08224962,0x74bcb091,
0xfd9efedc,0x08e8135c,0xa12e27a5,0x0c06367f,0x67966a76,
0x692c9559,0x629d82b2,0x62363f36,0xbfff3330,0xea1242af,
0x59e1d1b0,0x282ee969,0x0f1399df,0x97aceac7,0x07c44f1e,
0x0afd53ea,0x0bef3079,0xd6662669,0x735be859,0xc591eabf,
0x84ff845f,0x8a894a78,0x34c8f257,0xb85bce47,0x65989b54,
0x87ac1eb4,0x06949770,0x0dd27f9e,0xda749d55,0xa360a339,
0x05829596,0x00da965f,0x5b4166e7,0x4b57d1f5,0x4c20d47c,
0x0aca3e61,0x078f2e19,0x06519a99,/*0x6c244574,0x19472f3a,
0x375c6265,0x1625869b,0x09c0cf3d,0x092556c3,0x26448cfa,*/
};
static long vec2[48];
static int feed2, tap2, borrow2;

long longRand1(void);

void
initRand1(long seed)
{
        int i;

        feed1 = 0;
        tap1 = R1 - S1;
        borrow1 = 0;
        /*
         *        the seed pseudorandomly perturbes a
         *        physically random state.
         */
        seed = 1103515245*seed + 12345;
        seed = 1103515245*seed + 12345;
        seed = 1103515245*seed + 12345;
        for (i = 0; i < R1; i++) {
                seed = 1103515245*seed + 12345;
                vec1[i] = seed ^ randomData1[i];
        }
        for (i = 0; i < R1*LOGR1; i++)
                longRand1();
}

/*
 *      Marsaglia's subtract-with-borrow generator
 *      (The Annals of Applied Probability, 1991, 1(3), 462-480)
 *          from code by J. Reeds 91/11/19.
 */
long
longRand1(void)
{
        long tmp;

        tmp = (vec1[tap1] - vec1[feed1]) - borrow1;
        if (tmp < 0) {
                borrow1 = 1;
                tmp += M;
        } else
                borrow1 = 0;
        vec1[feed1] = tmp;
        if (++feed1 >= R1)
                feed1 = 0;
        if (++tap1 >= R1)
                tap1 = 0;
        return tmp;
}

void
initRand2(long seed)
{
        int i;

        feed2 = 0;
        tap2 = R2 - S2;
        borrow2 = 0;
        /*
         *        the seed pseudorandomly perturbes a
         *        physically random state.
         */
        seed = 1103515245*seed + 12345;
        seed = 1103515245*seed + 12345;
        seed = 1103515245*seed + 12345;
        for (i = 0; i < R2; i++) {
                seed = 1103515245*seed + 12345;
                vec2[i] = seed ^ randomData2[i];
        }
        for (i = 0; i < R2*LOGR2; i++)
                longRand2();
}

long
longRand2(void)
{
        long tmp;

        tmp = (vec2[tap2] - vec2[feed2]) - borrow2;
        if (tmp < 0) {
                borrow2 = 1;
                tmp += M;
        } else
                borrow2 = 0;
        vec2[feed2] = tmp;
        if (++feed2 >= R2)
                feed2 = 0;
        if (++tap2 >= R2)
                tap2 = 0;
        return tmp;
}


void reg_print(long reg[],
		       int length)
{
    int i;

    for (i=0; i<length; i++)
        print("%x\n", reg[i]);
}


static uchar key[8];

static void
init_prand(void)
{
    uchar lbuf[8];
    long seed;
    int i;

    randomBytes((uchar *)key, (ulong)8, REALLY);
    randomBytes((uchar *)lbuf, (ulong)8, REALLY);
    seed = (lbuf[3]<<24)|(lbuf[2]<<16)|(lbuf[1]<<8)|lbuf[0];
    initRand1(seed);
    seed = (lbuf[7]<<24)|(lbuf[6]<<16)|(lbuf[5]<<8)|lbuf[4];
    initRand2(seed);
    for (i=0; i<8; i++)
        lbuf[i] = 0;
    rand_seeded = 1;
}

void
seed_prand(uchar useed[16])
{
    uchar *lbuf;
    long seed;
    int i;

    lbuf = useed;
    seed = (lbuf[3]<<24)|(lbuf[2]<<16)|(lbuf[1]<<8)|lbuf[0];
    initRand1(seed);
    seed = (lbuf[7]<<24)|(lbuf[6]<<16)|(lbuf[5]<<8)|lbuf[4];
    initRand2(seed);

    for (i=0; i<8; i++)
        key[i] = useed[i+8];

    rand_seeded = 1;
}

ulong
prand(void) {
    ulong retval;
    long x1, x2left, x2right, x3;
    char block[8];
    int i;

    if (rand_seeded == 0)
        init_prand();

    x1 = longRand1();
    x2left = longRand2();
    x2right = longRand2();
    x3 = longRand2();

    for (i=0; i<3; i++) {
        block[i] = x2right & 0xff;
        block[i+4] = x2left & 0xff;
        x2right >>= 8;
        x2left >>= 8;
    }
    block[3] = x3 & 0xff;
    x3 >>= 8;
    block[7] = x3 & 0xff; 

    block_cipher((ulong *)key, (uchar *)block, 0);

    retval = block[3];
    for (i=2; i >= 0; i--)
        retval = (retval << 8) + (ulong)(block[i] & 0xFF);

    retval += x1;

    return retval;
}
