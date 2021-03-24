#include <lib9.h>

Tm*
p9gmtime(ulong t)
{
	struct tm *now;
	static Tm tnow;
	char *tz;

	now = gmtime(&t);
	tnow.sec = now->tm_sec;
	tnow.min = now->tm_min;
	tnow.hour = now->tm_hour;
	tnow.mday = now->tm_mday;
	tnow.mon = now->tm_mon;
	tnow.year = now->tm_year;
	tnow.wday = now->tm_wday;
	tnow.yday = now->tm_yday;
	tz = getenv("TZ");
	if(tz == 0)
		tz = "EST5EDT";
	if(now->tm_isdst)
		memmove(tnow.zone, tz, 3);
	else
		memmove(tnow.zone, tz + strlen(tz) - 3, 3);
	tnow.zone[3] = 0;

	return &tnow;
}
