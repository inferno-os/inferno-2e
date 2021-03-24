implement I18N;

include "sys.m";
	sys : Sys;
include "i18n.m";
	locale : I18N_Locale;

	day, abday, mon, abmon,
	d_t_fmt, d_fmt, s_d_fmt, t_fmt, t12_fmt, s_t_fmt, s_t12_fmt,
	twelveHour, am_pm, firstday,
	decimal_point, thousands_sep, grouping,
	currency_symbol, int_curr_symbol,
	mon_decimal_point, mon_thousands_sep, mon_grouping,
	positive_sign, negative_sign, frac_digits, int_frac_digits,
	upper, lower, space, cntrl, graph, print, punct, digit, xdigit, blank,
	equiv, dontcare, phonepattern : import locale;
	
include "daytime.m";
	daytime : Daytime;
include "string.m";
	str : String;


inited := 0;

init() : int
{
	if ( inited == -1 )
		return -1;
	if ( inited == 1 )
		return 0;
	sys = load Sys Sys->PATH;
	daytime = load Daytime Daytime->PATH;
	str = load String String->PATH;
	locale = load I18N_Locale I18N_Locale->PATH;
	if ( sys == nil || daytime == nil || str == nil || locale == nil )
		return (inited = -1);
	inited = 1;
	return 0;
}
###
# LC_TYPE
###
#	Make use of fact that in Latin1 lower case letters are always
#	CASEDIFF bigger than the corresponding upper case letter.
#	If we include latin2, greek, cyrillic, etc., this needs to be
#	revisited.

to_upper(s : string) : string
{
	if ( inited != 1 )
		return s;
	ans := s;
	l := len s;
	for ( i := 0; i < l; i++ )
		if ( str->in(s[i], lower) )
			ans[i] -= CASEDIFF;
	return ans;
}

to_lower(s : string) : string
{
	if ( inited != 1 )
		return s;
	ans := s;
	l := len s;
	for ( i := 0; i < l; i++ )
		if ( str->in(s[i], upper) )
			ans[i] += CASEDIFF;
	return ans;
}

isupper(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, upper);
}

islower(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, lower);
}

iscontrol(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, cntrl);
}

isspace(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, space);
}

isblank(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, blank);
}

isdigit(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, digit);
}

ishexdigit(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, xdigit);
}

isalpha(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return islower(c) | isupper(c);
}

isalphanum(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return isalpha(c) || isdigit(c);
}

isprint(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, print);
}

isgraph(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, graph);
}

ispunct(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, punct);
}
	
fold(bool : int)
{
	ignorecase := (bool != 0);
}

isDontCare(c : int) : int
{
	if ( inited != 1 )
		return 0;
	return str->in(c, dontcare);
}

###
# LC_COLLATE
###
#	Determine collating sequence based on multi-level comparisons.
#	1. Compare at letter level.  Letters differing by diacritical
#	   or case are considered the same, and don't-cares are ignored.
#	2. Same letter with differing diacritical are different.  Case
#	   is ignored, also don't-cares.
#	3. Distinguish case, but ignore don't-cares.
#	4. Distinguish don't-cares.

strcoll(s1, s2 : string) : int
{
	if ( inited != 1 )
		return 0;
	l1 := len s1;
	l2 := len s2;
	i1 := i2 := 0;
	c1, c2 : int;
	cdiff := ddiff := 0;
	while ( 1 ) {
		# skip any don't-care characters
		while ( i1 < l1 && ! isalphanum(c1 = s1[i1]) )
			i1++;
		while ( i2 < l2 && ! isalphanum(c2 = s2[i2]) )
			i2++;
		if ( i1 >= l1 ) {
			if ( i2 >= l2 )	# first-level equality
				break;
			return -1;	# first string shorter
		}
		if ( i2 >= l2 ) {
			return 1;	# second string shorter
		}
		i1++;
		i2++;
		
		# If you wanted to be able to treat 'ch' or 'll' as single
		# characters for the purpose of sorting, or to treat ß (German
		# s-tset) like 2 s's, you would have to do something about here.
		
		if ( c1 == c2 )
			continue;
		ct1 := c1;
		ct2 := c2;
		if ( isupper(c1) )
			c1 += CASEDIFF;
		if ( isupper(c2) )
			c2 += CASEDIFF;
		if ( c1 == c2 ) {		# same except for case
			if ( cdiff == 0 ) {	# first case difference
				if ( ct1 > ct2 )	cdiff = 1;
				else			cdiff = -1;
			}
			continue;
		}
		(ce1, cp1) := equivalence(c1);
		(ce2, cp2) := equivalence(c2);
		if ( ce1 == ce2 ) {		# same up to diacritical
			if ( ddiff == 0 ) {	# first diacritical difference
				if ( cp1 > cp2 )	ddiff = 1;
				else			ddiff = -1;
			}
			continue;
		}
		# If get this far, chars belong to different classes.
		# Following depends on having character classes (either
		# first char of class or the singleton value) occur in sort
		# order in the encoding.
		if ( ce1 < ce2 )
			return -1;
		if ( ce1 > ce2 )
			return 1;
	}
	# At top level, strings match.
	# Now look for differences in diacriticals
	if ( ddiff )		# there was a diacritical difference
		return ddiff;

	# How about a case difference?
	if ( cdiff )
		return cdiff;

	# Strings are equal except for don't-cares
	if ( s1 == s2 )
		return 0;
	if ( s1 < s2 )
		return -1;
	return 1;
}

equivalence(c : int) : (int, int)
{
	l := len equiv;
	for ( i := 0; i < l; i++ ) {
		m := len equiv[i];
		for ( j := 0; j < m; j++ )
			if ( c == equiv[i][j] )
				return (equiv[i][0], j);
	}
	return (c, 0);
}

###
# LC_TIME
###
dayname(wday : int) : (string, string)
{
	if ( inited != 1 )
		return (nil, nil);
	da := wday % 7;
	if ( da < 0 )
		da += 7;
	return (day[da], abday[da]);
}

monthname(mo : int) : (string, string)
{
	if ( inited != 1 )
		return (nil, nil);
	month := (mo - 1) % 12;
	if ( month < 0 )
		month += 12;
	return (mon[month], abmon[month]);
}

weekbegin() : int
{
	if ( inited != 1 )
		return 0;
	return firstday;
}

wday(tim : int) : int
{
	if ( inited != 1 )
		return -1;
	tm := daytime->local(tim);
	return tm.wday;
}

yday(tim : int) : int
{
	if ( inited != 1 )
		return -1;
	tm := daytime->local(tim);
	return tm.yday + 1;
}

timestr(hr, min, sec : int) : string
{
	if ( inited != 1 )
		return nil;
	tm := ref daytime->Tm;
	tm.hour = hr;
	tm.min = min;
	tm.sec = sec;
	if ( twelveHour )
		return datetm(t12_fmt, tm);
	else
		return datetm(t_fmt, tm);
}

shorttime(hr, min, sec : int) : string
{
	if ( inited != 1 )
		return nil;
	tm := ref daytime->Tm;
	tm.hour = hr;
	tm.min = min;
	tm.sec = sec;
	if ( twelveHour )
		return datetm(s_t12_fmt, tm);
	else
		return datetm(s_t_fmt, tm);
}

gettm(yr, mo, da : int) : ref Daytime->Tm
{
	if ( yr >= 1900 )
		yr -= 1900;
	while ( mo < 1 )
		mo += 12;
	while ( mo > 12)
		mo -= 12;
	while ( da > 31 )
		da -= 31;
	if ( inited != 1 )
		return nil;
	tm := daytime->local(daytime->now());
	tm.mday = da;
	tm.mon = mo - 1;
	tm.year = yr;
	#
	#	Can't just return this Tm.  Need to get wday, yday, etc.,
	#	properly calculated.
	#
	timet := daytime->tm2epoch(tm);
	return daytime->local(timet);
}

shortdate(yr, mo, da : int) : string
{
	tm := gettm(yr, mo, da);
	if ( tm == nil )
		return nil;
	return datetm(s_d_fmt, tm);
}

datestr(yr, mo, da : int) : string
{
	tm := gettm(yr, mo, da);
	if ( tm == nil )
		return nil;
	return datetm(d_fmt, tm);
}

text(tim : int) : string
{
	if ( inited != 1 )
		return nil;
	return date(d_t_fmt, tim);
}

now() : int
{
	if ( inited != 1 )
		return 0;
	return daytime->now();
}

then(yr, mo, da, hr, min, sec : int) : int
{
	if ( inited != 1 )
		return 0;
	tm := daytime->local(daytime->now());
	if ( yr > 1900 )
		yr -= 1900;
	tm.year = yr;
	tm.mon = (mo - 1) % 12;
	tm.mday = (da - 1) % 31 + 1;
	tm.hour = hr % 24;
	tm.min = min % 60;
	tm.sec = sec % 60;
	return daytime->tm2epoch(tm);
}

unthen(tim : int) : (int, int, int, int, int, int)
{
	if ( inited != 1 )
		return (0, 0, 0, 0, 0, 0);
	tm := daytime->local(tim);
	return (tm.year, tm.mon + 1, tm.mday, tm.hour, tm.min, tm.sec);
}

time() : string
{
	return text(now());
}

date(fmt : string, tim : int) : string
{
	if ( inited != 1 )
		return nil;
	tm := daytime->local(tim);
	return datetm(fmt, tm);
}

datetm(fmt : string, tm : ref Daytime->Tm) : string
{
	if ( inited != 1 )
		return nil;
	ans : string;
	j := 0;
	for ( i := 0; i < len fmt; i++ ) {
		if ( fmt[i] == '%' ) {
			if ( ++i >= len fmt )
				return "";
			intval := -1;
			case fmt[i] {
			    '%' =>
				ans[j++] = '%';
			    'A' =>
				ans += day[tm.wday];
			    'a' =>
				ans += abday[tm.wday];
			    'B' =>
				ans += mon[tm.mon];
			    'b' or 'h' =>
				ans += abmon[tm.mon];
			    'c' =>
				ans += datetm(d_t_fmt, tm);
			    'C' =>
				intval = tm.year / 100 + 19;
			    'd' =>
				intval = tm.mday;
			    'D' =>
				ans += datestr(tm.year, tm.mon, tm.mday);
			    'e' =>
				ans += sys->sprint("%2.d", tm.mday);
			    'H' =>
				intval = tm.hour;
			    'I' =>
				intval = tm.hour % 12;
				if ( intval == 0 )
					intval += 12;
			    'j' =>
				intval = tm.yday + 1;
			    'k' =>
				ans += sys->sprint("%2.d", tm.hour);
			    'l' =>
				hr := tm.hour %12;
				if ( hr == 0 )
					hr += 12;
				ans += sys->sprint("%2.d", hr);
			    'm' =>
				intval = tm.mon + 1;
			    'M' =>
				intval = tm.min;
			    'n' =>
				ans[len ans] = '\n';
			    'p' =>
				ans += sys->sprint("%s", am_pm[tm.hour / 12]);
			    'r' =>
				ans += datetm(t_fmt + " %p", tm);
			    'R' =>
				ans += datetm("%H:%M", tm);
			    'S' =>
				intval = tm.sec;
			    't' =>
				ans[len ans] = '\t';
			    'T' =>
				ans += datetm("%H:%M:%S", tm);
			    'u' =>
				intval = tm.wday + 1;
			    'U' =>
				nextsun := tm.yday + 1 - tm.wday + 7;
				intval = (nextsun - nextsun % 7) / 7;
			    'W' =>
				nextsun := tm.yday + 1 - tm.wday + 7;
				intval = (nextsun - nextsun % 7) / 7;
				if ( tm.wday == 0 )
					intval--;
			    'V' =>
				nextsun := tm.yday + 1 - tm.wday + 7;
				intval = (nextsun - nextsun % 7) / 7;
				if ( tm.wday == 0 )
					intval--;
				if ( nextsun % 7 > 4)
					intval++;
				else if ( intval == 0 )
					intval = 53;
			    'w' =>
				intval = tm.wday;
			    'x' =>
				ans += datestr(tm.year, tm.mon, tm.mday);
			    'X' =>
				ans += timestr(tm.hour, tm.min, tm.sec);
			    'y' =>
				intval = tm.year % 100;
			    'Y' =>
				ans += sys->sprint("%d", tm.year + 1900);
			    'Z' =>
				ans += tm.zone;
			}
			if ( intval >= 0 )
				ans += sys->sprint("%.2d", intval);
		}
		else
			ans[len ans] = fmt[i];
	}
	return ans;
}

###
# LC_MONETARY
###
money(amt : real) : string
{
	if ( inited != 1 )
		return string amt;
	return currency_symbol + print_number(amt, mon_grouping,
			frac_digits, mon_thousands_sep, mon_decimal_point);
}

intmoney(amt : real) : string
{
	if ( inited != 1 )
		return string amt;
	return int_curr_symbol + print_number(amt, mon_grouping,
			frac_digits, mon_thousands_sep, mon_decimal_point);
}

###
# LC_NUMERIC
###
print_number(num : real, group, fdig : int, sep, point : string) : string
{
	if ( inited != 1 )
		return nil;
	s : string;
	ans := "";
	
	if ( real int num == num )	# no fractional part
		s = sys->sprint("%d", int num);
	else if ( fdig >= 0 )
		s = sys->sprint("%.*f", fdig, num);
	else
		s = sys->sprint("%f", num);
	if ( s[0] == '-' ) {
		ans[0] = '-';
		s = s[1:];
	}
	(i,f) := str->splitl(s, ".");
	l := len i;
	o := l % group;
	if ( o == 0 )
		o += group;
	ans += i[:o];
	for ( ; o < l; o += group )
		ans += sep + i[o:o+group];
	if ( f != "" ) {
		last := len f -1;
		while ( f[last] == '0' && last > fdig )
			last--;
		ans += point + f[1:last+1];
	}
	return ans;
}

numstr(num : real) : string
{
	if ( inited != 1 )
		return string num;
	return print_number(num, grouping, -1, thousands_sep, decimal_point);
	
}

###
# TELEPHONE NUMBERS
#
#	There doesn't seem to be an applicable standard for this.
###
telformat(num : string, pattern : string) : string
{
	if ( inited != 1 )
		return num;
	if ( pattern == nil )
		pattern = phonepattern;
	j := len num;
	lastsub := len pattern;
	for ( i := lastsub - 1; i >= 0; i-- ) {
		if ( pattern[i] != '#' )	# non-# stay as is
			continue;
		while ( j-- > 0 && (! isdigit(num[j])) )   # back up to number
			;
		if ( j < 0 )				# no more in num
			if ( lastsub < len pattern )	# made substitution(s)
				return pattern[lastsub:];
			else
				return "";
		pattern[i] = num[j];
		lastsub = i;
	}
	while ( j-- > 0 )		# prepend any extra digits in num
		if ( isdigit(num[j]) )
			pattern = num[j:j+1] + pattern;

	return pattern;
}
