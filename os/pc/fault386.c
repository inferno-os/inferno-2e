#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"ureg.h"
#include	"io.h"

void
fault386(Ureg *ur, void *arg)
{
	int read;
	ulong addr;
	char buf[ERRLEN];

	USED(arg);

	up->dbgreg = ur;		/* For remote ACID */

	addr = getcr2();
	read = !(ur->ecode & 2);
	spllo();
	sprint(buf, "trap: fault %s pc=0x%lux addr=0x%lux",
			read ? "read" : "write", ur->pc, addr);
	if(up->type == Interp)
		disfault(ur, buf);
	dumpregs(ur);
	panic("fault: %s\n", buf);
}
