#ifndef _SVP_H_
#define _SVP_H_
#include <stdlib.h>

typedef struct _svp_t {
	char*		s;	/* string */
	unsigned long	v;	/* value */
} svp_t ;

#define SVP_SZ(a) (sizeof(a)/sizeof(svp_t))

extern int sval(char*, unsigned long*, unsigned long, unsigned long);
extern int svpmatchs(svp_t*, int, char*, unsigned long*);
extern int svpmatchv(svp_t*, int, char**, unsigned long);
extern int svpindexv(svp_t*, int, unsigned long);
extern int svpindexs(svp_t*, int, char*);

#endif
