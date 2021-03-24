#include <u.h>
#include "mmap.h"
#include "monconf.h"

#define MONVER_MAJOR	1
#define MONVER_MINOR	14
#define MONVER_PATCH	'c'	/* this should come from the .mcf file... */

DramConf dramconf0 = {0,0,0,0,0,0,0,0,0};
DramConf dramconf1 = {0,0,0,0,0,0,0,0,0};
DramConf dramconf2 = {0,0,0,0,0,0,0,0,0};
DramConf dramconf3 = {0,0,0,0,0,0,0,0,0};
DramConf dramconf4 = {0,0,0,0,0,0,0,0,0};
DramConf dramconf5 = {0,0,0,0,0,0,0,0,0};
DramConf dramconf6 = {0,0,0,0,0,0,0,0,0};
DramConf dramconf7 = {0,0,0,0,0,0,0,0,0};

MmapConf mmapconf[16] = {{1,0,0,0}};
// MmapConf mmapconf7 = {0,0,0,0};

char monname[16] = "styxmon";

MonMisc monmisc = {
	5,		/* styxmon */
	0,0,0,
	(ulong)monname,
	sizeof monname,
	MONVER_MAJOR,
	MONVER_MINOR,
	MONVER_PATCH,
	// .flashbase Static0Base,
	// .cpuspeed_ppcr 5,
	// .cpuspeed_hz 3686400*(5*4+16)
	.gafr 0,
	.gpdr 0,
	.gpsr 0,
};

