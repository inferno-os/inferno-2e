#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <kernel.h>

int
verifysigner(uchar *sign, int len, uchar *data)
{
	/*
	 * Module signing is part of the commercial
	 * Inferno product
	 */

	USED(sign);
	USED(len);
	USED(data);

	return 1;
}
