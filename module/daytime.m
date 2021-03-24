Daytime: module
{
	PATH:	con "/dis/lib/daytime.dis";

	Tm: adt {
		sec:	int;
		min:	int;
		hour:	int;
		mday:	int;
		mon:	int;
		year:	int;
		wday:	int;
		yday:	int;
		zone:	string;
		tzoff:	int;
	};

	# now:
	# return the time in seconds since the epoch
	#
	# time:
	# return the current local time as string
	#
	# text:
	# convert a time structure from local or gmt
	# into a text string
	#
	# filet:
	# return a string containing the file time
	# prints mon day hh:mm if the file is < 6 months old
	# 	 mon day year  if > 6 months old
	#
	# local:
	# uses /locale/timezone to convert an epoch time in seconds into
	# a local time structure
	#
	# gmt:
	# return a time structure for GMT
	#
	# tm2epoch:
	# convert a time structure to an epoch time in seconds
	#
	# string2tm:
	# parse a string representing a date into a time structure
	now:		fn(): int;
	time:		fn(): string;
	text:		fn(tm: ref Tm): string;
	filet:		fn(now, file: int): string;
	local:		fn(tim: int): ref Tm;
	gmt:		fn(tim: int): ref Tm;
	tm2epoch:	fn(tm: ref Tm): int;
	string2tm:	fn(date: string): ref Tm;
};
