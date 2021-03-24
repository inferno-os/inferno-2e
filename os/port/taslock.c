#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

static char *lockloop_fmt =
	"lock loop on 0x%lux key 0x%lux pc 0x%lux held by pc 0x%lux\n";

static void
lockloop(ulong key, Lock *l, ulong pc)
{
	setpanic();
	print(lockloop_fmt, l, key, pc, l->pc);
	dumplongs("lock area", (ulong*)(l-4), 5*sizeof *l/sizeof (ulong));
	panic("lockloop");
}

void
lock(Lock *l)
{
	int pri, i;
	ulong pc;
	ulong k;

	pc = getcallerpc(&l);
	if(up == 0) {
		if ((k = tas(&l->key)) != 0) {
			for(i=0; ; i++) {
				if(tas(&l->key) == 0)
					break;
				if (i >= 1000000) {
					lockloop(k, l, pc);
					break;
				}
			}
		}
		l->pc = pc;
		return;
	}

	pri = up->pri;
	up->pri = PriLock;
	if ((k = tas(&l->key)) != 0) {
		for(i=0; ; i++) {
			if(tas(&l->key) == 0)
				break;
			if (i >= 1000) {
				lockloop(k, l, pc);
				break;
			}
			if(conf.nmach == 1 && up->state == Running && islo())
			{
				up->pc = pc;
				sched();
			}
		}
	}
	l->pri = pri;
	l->pc = pc;
}

void
ilock(Lock *l)
{
	ulong x, pc;
	ulong k;
	int i;

	pc = getcallerpc(&l);
	x = splhi();
	for(;;) {
		if((k = tas(&l->key)) == 0) {
			l->sr = x;
			l->pc = pc;
			return;
		}
		for(i=0; ; i++) {
			if(l->key == 0)
				break;
			clockcheck();
			if (i > 100000) {
				lockloop(k, l, pc);
				break;
			}
		}
	}
}

int
canlock(Lock *l)
{
	int pri;

	if (up == 0) {
		if(tas(&l->key))
			return 0;
	}
	else {
		pri = up->pri;
		up->pri = PriLock;
		if (tas(&l->key)) {
			up->pri = pri;
			return 0;
		}
		l->pri = pri;
	}
	l->pc = getcallerpc(&l);
	return 1;
}

void
unlock(Lock *l)
{
	int p;

	p = l->pri;
	l->pc = 0;
	l->key = 0;
	if(up)
		up->pri = p;
}

void
iunlock(Lock *l)
{
	ulong sr;

	sr = l->sr;
	l->pc = 0;
	l->key = 0;
	splx(sr);
}
