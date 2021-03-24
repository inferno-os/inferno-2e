#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "putimage.h"
#include "screen.h"
#include "custom.h"

int
showimage(const char *fname)
{
	uchar *image;
	Istream *is;
	Ostream *os;

	os = stdout;
	stdout = nil;
	if(!(is = sd_openi(fname))) {
		stdout = os;
		return -1;
	}
	if(!(image = simmap(is)) || !*image || imagehgt(image) < 10) {
		char estr[ERRLEN];
		sprint(estr, "error: can't read %s", fname);
		error(estr);
		sd_closei(is);
		stdout = os;
		return -1;
	} else
		putimage((vd_wid >> 1)-(imagewid(image) >> 1),
			((vd_hgt-fonthgt*2) >> 1)-(imagehgt(image) >> 1),
			image, 1);
	sd_closei(is);
	stdout = os;
	return 0;
}

