#include	"lib9.h"
#include	<bio.h>

int
Bbuffered(Biobuf *bp)
{
	switch(bp->state) {
	case Bracteof:
	case Bractive:
		return -bp->icount;

	case Bwactive:
		return bp->bsize + bp->ocount;
	}
	fprint(2, "Bbuffered: unknown state %d\n", bp->state);
	return 0;
}
