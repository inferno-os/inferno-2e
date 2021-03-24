#include "bpi.h"
#include "../styxmon/styxmon.h"

struct Global					// because we're in ROM
{
	int		cmd;
	ulong		val;
	ulong		baddr;			// reboot address
	int		bps;
	int		bootsize;
	int		msgpos;
	int		inpin;
	int		inpout;

	PortGlobal;

	BPChan		rootdir;
	BPChan		conschan;
	BPChan		ctlchan;
	BPChan		memchan;
	BPChan		bootchan;

	char		inpbuf[1024];
	char		msgbuf[4096];	// note: this *must* be last
};

#define	G	((Global*)GLOBAL_OFS)

