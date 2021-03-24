#
#	Localedef source for fr_FR (French in France) locale
#
#	To convert the i18n library to another locale, you need to fill in
#	the values in this file with the appropriate strings and numbers.
#	Suggested procedure is to put this information in a file called
#	i18n.locale.b, in directory /locale/<localized>/appl/lib, where
#	<localized> is appropriate to the locale (e.g., fr_FR).  Before use
#	the module (i18n.locale.dis) should be copied to (or bound over)
#	the file /dis/lib/i18n.locale.dis.
#	
implement I18N_Locale;

include "i18n.m";

###
# LC_TIME
###
#	Abbreviations for days of the week, in order, starting with Sunday
###
abday = array[] of {
    "dim.",
    "lundi",
    "mardi",
    "mercr.",
    "jeudi",
    "vendr.",
    "sam."
};
#
#	Days of the week, in order, starting with Sunday
#
day = array[] of {
    "dimanche",
    "lundi",
    "mardi",
    "mercredi",
    "jeudi",
    "vendredi",
    "samedi"
};
#
#	Abbreviations for months, in order, starting with January
#
abmon = array[] of {
    "janv.",
    "fevr.",
    "mars",
    "avr.",
    "mai",
    "juin",
    "juil.",
    "août",
    "sept.",
    "oct.",
    "nov.",
    "déc."
};
#
#	Months, in order, starting with January
#
mon = array[] of {
    "janvier",
    "février",
    "mars",
    "avril",
    "mai",
    "juin",
    "juillet",
    "août",
    "septembre",
    "octobre",
    "novembre",
    "décembre"
};
#
#	Formats for various date and time strings.  In forming these formats,
#	the following substitutions are available:
#
#	%%	Gives a single %
#	%a	Abbreviated day name
#	%A	Full day name
#	%b	Abbreviated month name
#	%B	Full month name
#	%c	Date and time representation specified by d_t_fmt
#	%C	Century number (2 digits)
#	%d	Day of month, range 1-31 (2 digits)
#	%D	Date as %m/%d/%y
#	%e	Day of month, range 1-31 (single digits preceded by space)
#	%h	Same as %b
#	%H	Hour, range 0-23 (2 digits)
#	%I	Hour, range 1-12 (2 digits)
#	%j	Day of year, range 1-366 (2 or 3 digits)
#	%k	Hour, range 0-23 (single digits preceded by space)
#	$l	Hour, range 1-12 (single digits preceded by space)
#	%m	Month number, range 1-12 (2 digits)
#	%M	Minute, range 0-59 (2 digits)
#	%n	Newline is inserted
#	%p	Locale equivalent of a.m. or p.m.
#	%r	Time in 12-hour format with %p
#	%R	Time as %H:%M
#	%s	Seconds, range 0-59 (2 digits)
#	%t	Tab is inserted
#	%T	Time as %H:%M:%S
#	%u	Weekday as a number, range 1-7 (1 is Sunday)
#	%U	Week number of year, range 0-53 (Sunday is first day of week 1)
#	%V	Week number of year, range 1-53 (Monday is first day of week,
#		week 1 has >= 4 days in new year, else week 53
#	%w	Weekday number, range 0-6 (Sunday is 0)
#	%W	Week number of year, range 0-53 (Monday is first day of week 1)
#	%x	Date using the format d_fmt
#	%X	Time using the format t_tmt (24-hour) or t12_fmt (12-hour)
#	%y	Year within century, range 0-99 (2 digit)
#	%Y	Year (4 digits)
#	%Z	Time zone name or abbreviation, if any
#

#
#	Format of string returned by text()
#
d_t_fmt = "%a, %e %b %Y %Hh%Mm%Ss";
#
#	Format of string returned by datestr()  (Long date format)
#
d_fmt = "%A, %e %B %Y";
#
#	Format of string returned by shortdate() (Short date format)
#
s_d_fmt = "%d/%m/%y";
#
#	Format of string returned by timestr() (Long 24-hour format)
#
t_fmt = "%Hh%Mm%Ss";
#
#	Format of string returned by timestr() (Long 12-hour format)
#
t12_fmt = "%lh%Mm%Ss %p";
#
#	Format of string returned by shorttime() (Short 24-hour format)
#
s_t_fmt = "%Hh%M";
#
#	Format of string returned by shorttime() (Short 12-hour format)
#
s_t12_fmt = "%lh%M %p";

twelveHour = 1;		# 12-hour/24-hour value (1 => 12, 0 => 24)

am_pm = array[] of {		# Locale representation of a.m. and p.m. strings
    "am",			#  (a.m. first)
    "pm"
};

firstday = 1;			# First day of week in this locale (0 => Sunday,
				# 1 => Monday, etc.)
###
# LC_NUMERIC
###

decimal_point = ",";		# String that separates integer and fractional
				# parts of a number

thousands_sep = ".";		# String that separates digit groups of a number

grouping = 3;			# Number of digits in a digit group in printed
				# numbers
###
# LC_MONETARY
###

int_curr_symbol = "FF ";	# String used to represent locale currency
				# units outside of the locale

currency_symbol = "F";		# String used to represent locale currency
				# units within the locale

mon_decimal_point = ",";	# String that separates whole currency units
				# from fractional units

mon_thousands_sep = ".";	# String that separates groups of digits of
				# currency amounts

mon_grouping = 3;		# Number of digits in a digit group in
				# currency amounts

positive_sign = "";		# Symbol used for positive currency amounts

negative_sign = "-";		# Symbol used for negative currency amounts

int_frac_digits = 2;		# Number of fractional digits to display
				# for currency amounts (outside of this locale)

frac_digits = 2;		# Number of fractional digits to display for
				# currency amounts (within this locale)

# Ref ISO 4217
###
# LC_TYPE
###
#	A sequence a-z is a memory saving notation meaning all the letters
#	from a to z, and may be used where a sequence of letters has adjacent
#	codes.  'a' is the smallest code in the sequence, 'z' the largest.

upper = "A-ZÀ-ÖØ-Þ";			# Uppercase letters.
lower = "a-zà-öø-þ";			# Lowercase letters.
space = "\t\n\v\u000c\r ";		# Codes representing horizontal or
					# vertical space.
cntrl = "\u0000-\u001f\u007f\u0080-\u009f";		# Control characters.
graph = "!-~¡-ÿ";			# Codes which lay down ink.
print = " -~ -ÿ";			# Codes representing characters with
					# specified widths
punct = "!-/:-@[-`{-~¡-»¿×÷";		# graphics except alphanumerics
digit = "0-9";
xdigit = "0-9a-fA-F";			# legal hexadecimal digits
blank = " \t";				# horizontal motion within line (no ink)
###
# LC_COLLATE
###
#	A table of equivalents, i.e. characters that differ only by diacriticals
#	The comparison is done on lower case versions, so you don't to put the
#	upper case versions here unless there is no lower case.  However, it
#	won't hurt to include both cases.  Characters should appear in the
#	classes in the order in which they should be sorted.

equiv = array[] of {
	"aáàâãäå",
	"cç",
	"eéèêë",
	"iìíîï",
	"nñ",
	"oóòôõö",
	"uúùûü",
	"yýÿ"
};

order_start := array[] of {
    1, -1, 1
};

dontcare = "\u0000-/:-@[-`{-~";


