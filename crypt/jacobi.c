#include "lib9.h"
#include <libcrypt.h>

int tab2[] = {0, 1, 0, -1, 0, -1, 0, 1};

int
bigJacobi(BigInt a, BigInt b)
{
	static BigInt aa, bb, r;
	static int first_time = 1;
	int v, k;
	
	if (first_time) {
		aa = bigInit(0);
		bb = bigInit(0);
		r = bigInit(0);
		first_time = 0;
	}
	
	if (ZERO(b)) {
		if ((bigCompare(a, one) == 0))
			return 1;
		else
			return 0;
	}
	
	if (EVEN(a) && EVEN(b))
		return 0;
	
	bigCopy(a, aa);
	bigCopy(b, bb);
	
	v = 0;
	while (EVEN(bb)) {
		v++;
		bigRightShift(bb, 1, bb);
	}
	if ((v & 1) == 0)
		k = 1;
	else
		k = tab2[NUM(aa)[0]&7];
	
	if (SIGN(bb) == NEG) {
		SIGN(bb) = POS;
		if (SIGN(aa) == NEG)
			k = -k;
	}
	
	for(;;) {
		if (ZERO(aa)) {
			v = bigCompare(bb, one);
			if (v>0)
				return 0;
			else if (v == 0) {
				return k;
			}
		}
		v = 0;
		while (EVEN(aa)) {
			v++;
			bigRightShift(aa, 1, aa);
		}
		if (v & 1)
			k = k*tab2[NUM(bb)[0]&7];
		
		if ((NUM(aa)[0])&(NUM(bb)[0])&2)
			k = -k;
		bigCopy(aa, r);
		bigMod(bb, r, aa);
		bigCopy(r, bb);
	}

	/* not reached */
	return -1;
}

int
jacobi(int a, int b)
{
	int v;
	int k;
	int r;
	
	if( b==0 ) {
		if( a==1 || a==-1 )
			return(1);
		else
			return(0);
	}
	
	if( ((a & 1) == 0) && ((b & 1) == 0) )
		return(0);
	v = 0;
	while( (b & 1) == 0 ) {
		v++;
		b /= 2;
	}
	if( (v & 1) == 0 ) 
		k = 1;
	else
		k = tab2[a&7];
	
	if( b<0 ) {
		b = -b;
		if( a<0 )
		        k = -k;
        }
	
	for(;;) {
		if( a==0 ) {
			if( b>1 ) return(0);
			if( b==1 ) return(k);
		}
		v = 0;
		while( (a & 1) == 0 ) {
			v++;
			a /= 2;
		}
		if( v&1 )
			k = k*tab2[b&7];
		
		if(a&b&2) k = -k;
		r = abs(a);
		a = b % r;
		b = r;
	}

	/* not reached */
	return -1;
}

int	
isQR(int a, int p, int q)
{
	if( jacobi(a, p)==1 && jacobi(a, q)==1 )
		return(1);
	else return(0);
}

int
bigIsQR(BigInt a, BigInt p, BigInt q)
{
	int retval;
	
	retval = bigJacobi(a, p);
	if (retval != 1)
		return 0;
	retval = bigJacobi(a, q);
	if (retval != 1)
		return 0;
}

