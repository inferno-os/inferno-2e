#include <lib9.h>

/* this avoids pulling in the large atol() function 
 * when code is already using the very similar strtol() function
 */

int
atoi(char *s)
{
	return strtol(s, 0, 0);
}
