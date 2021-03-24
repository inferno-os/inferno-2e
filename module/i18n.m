I18N : module {
	
	PATH : con "/dis/lib/i18n.dis";
	
	CASEDIFF : con 'a' - 'A';

	init : fn() : int;			# load the needed locale data
						#  and other modules

	strcoll : fn(s1, s2 : string) : int;	# return as in C's strcmp

	money : fn(amt : real) : string;	# with local currency symbol
	intmoney : fn(amt : real) : string;	# with int'l currency name

	numstr : fn(num : real) : string;	# using appropriate
						#     decimal pts, separators

	timestr : fn(hr, min, sec : int) : string;
	shorttime : fn(hr, min, sec : int) : string;

	datestr : fn(yr, mo, da : int) : string;
	shortdate : fn(yr, mo, da : int) : string;
	
	date : fn(fmt : string, tim : int) : string;	# User specified
							#   time/date format

	now : fn() : int;			# return seconds since epoch
	then : fn(yr, mo, da, hr, min, sec : int) : int; # get secs since epoch
	unthen : fn(tim : int) : (int, int, int, int, int, int); # inverse then
	
	wday : fn(tim : int) : int;		# return day of week (0-6)
	yday : fn(tim : int) : int;		# return day of year (1-366)

	text : fn(tim : int) : string;		# convert seconds since epoch
						#     to date-time string
	time : fn() : string;			# current date-time
	
	dayname : fn(day : int) : (string, string);    # (full, abbrev)
	monthname : fn(mon : int) : (string, string);  # (full, abbrev)
	weekbegin : fn() : int;		# 1st day of week.  Sun == 0, etc.

	to_upper : fn(s : string) : string;
	to_lower : fn(s : string) : string;
	
	isupper		: fn(c : int) : int;
	islower		: fn(c : int) : int;
	iscontrol	: fn(c : int) : int;
	isspace		: fn(c : int) : int;	# whitespace
	isblank		: fn(c : int) : int;
	isdigit		: fn(c : int) : int;
	ishexdigit	: fn(c : int) : int;
	isalpha		: fn(c : int) : int;
	isalphanum	: fn(c : int) : int;	# isalpha() || isdigit()
	isprint		: fn(c : int) : int;	# isgraph() || ' '
	isgraph		: fn(c : int) : int;
	ispunct		: fn(c : int) : int;

	telformat	: fn(num : string, pattern : string) : string;
};

I18N_Locale : module {

	PATH : con "/dis/lib/i18n.locale.dis";
	
	day, abday, mon, abmon : array of string;
	d_t_fmt, d_fmt, s_d_fmt, t_fmt, t12_fmt, s_t_fmt, s_t12_fmt : string;
	twelveHour :int;
	am_pm : array of string;
	firstday : int;
	decimal_point, thousands_sep : string;
	grouping : int;
	currency_symbol, int_curr_symbol : string;
	mon_decimal_point, mon_thousands_sep : string;
	mon_grouping : int;
	positive_sign, negative_sign : string;
	frac_digits, int_frac_digits : int;
	upper, lower, space, cntrl, graph, print : string;
	punct, digit, xdigit, blank : string;
	equiv : array of string;
	dontcare : string;
	phonepattern : string;
};
