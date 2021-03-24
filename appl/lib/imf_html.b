implement IMF;

# HTML implementation
# Ed Bacher, evb@lucent.com

include "sys.m";
        sys: Sys;
        print, fprint, sprint, stat: import sys;
        stderr:	ref sys->FD;

# declares regex: Regex
include "regexutils.m";
	regexu: RegexUtils;
	match,
	match_mult,
	sub,
	sub_re,
	subg,
	subg_re:	import regexu;
        
include "daytime.m";
	daytime: Daytime;
	
include "imf.m";


TOP = "";
BOTTOM = "";

# for target-specific character processing
specials(text: string): string
{
	return text;
}

init()
{
	if (sys == nil)
		sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	
	daytime = load Daytime Daytime->PATH;
	if (daytime == nil) {
		fprint(stderr, "error loading Daytime from %s: %r\n", Daytime->PATH);
		return;
	}


# HTML header information
TOP += "<html>\n<head>\n";
# NOTE: add this string (or similar) to TOP to force browser to use unicode character set
# (not all browsers will do it)
# "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=unicode-1-1-utf-8\">\n"

# do BOTTOM
	BOTTOM += "\n</body></html>\n";

# get time information
	now := daytime->time();
	mtime := now;
	(ntoks, nowlist) := sys->tokenize(now, " ");

	date_mdy,
	year: string;
	if (ntoks == 6) {
		month := hd tl nowlist;
		day := hd tl tl nowlist;
		yr := hd tl tl tl tl tl nowlist;
		year = yr;
		date_mdy = month + ". " + day + ", " + yr;
	}

        
# special characters
	emdash_tag = " &#151; ";
	space_tag = "&nbsp";
	zero_tag = "";
	lt_tag = "&lt;";
	gt_tag = "&gt;";

# example tag array, ex, exl (left), and ex0 (0 margin)
	extag = array[] of {
		"", # nil to bump up index
		".ex",
		".exl",
		".ex0",
	};

	notag = array[] of {
		"",
		"",
		""
	};

# blocktags (not needed for html)
	blocktags = array[] of {""};



# tag definitions: general form is:
#	".tag", "<opening tag>", "<closing tag>",
#
	tags = array[] of {

# basic paragraph tags
		".p",
		"\n<p>\n",	"\n</p>\n",

		".pin",
		"\n<blockquote><p>\n",	"\n</p></blockquote>\n",
		
		".p0",
		"\n<p>\n",	"\n</p>\n",

# structural font tags
		".v",	# variable
		"<em><code>", "</code></em>",

		".em",	# emphasis (instead of .i)
		"<em>", "</em>",

		".strong",	# strong
		"<strong>", "</strong>",

		".m",	# menu items
		"<strong>", "</strong>",

		".opt",	# command options
		"<code>", "</code>",
		
		".n",	# name (misc. names)
		"<em>", "</em>",
		
		".url",	# URLs
		"<em>", "</em>",

		".ref",	# references (books, etc.)
		"<em>", "</em>",

		".l",	# literal
		"<code>", "</code>",

		".in",	# user input
		"<strong><code>", "</code></strong>",

		".ip",	# user input
		"<strong><code>", "</code></strong>",

		".file",	# filenames
		"<code>", "</code>",

		".fn",	# function names
		"<code>", "</code>",

		".cmd",	# command names
		"<code>", "</code>",

# specific (physical) font changes
		".i",
		"<em>", "</em>",
		
		".b",
		"<strong>", "</strong>",
		
		".c",
		"<code>", "</code>",

		".cb",
		"<strong><code>", "</code></strong>",
		
		".ci",
		"<em><code>", "</code></em>",


# structural headings for man pages
# like .SH (section (or sub) head) and .SS (secondary subhead) of man
		".sh",
		"\n<h3>", "</h3>\n",
		".ss",
		"\n<h4>", "</h4>\n",

# .h for general manpage heading
		".h",
		"\n<h3>", "</h3>\n",
		
# .ht for manpage head at top of page -- manpage(section)
		".ht",
		"\n<h3>", "</h3>\n",

# .hl for literal heading (function prototypes as headings)
		".hl",
		"\n<h4><code>", "</code></h4>\n",
		
# numbered headings (these are kind of physical, since they are basically HTML)
		".h1",
		"\n<h1>", "</h1>\n",
		".h2",
		"\n<h2>", "</h2>\n",
		".h2n",
		"\n<h2>", "</h2>\n",
		".h3",
		"\n<h3>", "</h3>\n",
		".h4",
		"\n<h4>", "</h4>\n",

# indented example
		".ex", "\n<blockquote><pre>\n", "</pre></blockquote>\n",

# left justified example
		".exl", "<pre>\n", "</pre>\n",

# 0 indent example (no difference for HTML)
		".ex0", "<pre>\n", "</pre>\n",

# simplified table
		".tab", "", "",
# table on left margin
		".table", "<p><table border=0 cellpadding=2 cellspacing=5>\n", "\n</table>\n",
# table centered
		".tablecenter", "<p><center><table border=0 cellpadding=2 cellspacing=5>\n", "\n</table></center>\n",
# table row
		".tr", "<tr>\n", "\n</tr>\n",
# table data (regular plus shortcuts for literal, name, variable, menu item)
		".td", "<td valign=\"top\">", "\n</td>\n",
		".tdl", "<td valign=\"top\"><code>\n", "\n</code></td>\n",
		".tdv", "<td valign=\"top\"><code><em>\n", "\n<em></code></td>\n",
		".tdn", "<td valign=\"top\"><em>\n", "\n</em></td>",
		".tdm", "<td valign=\"top\"><strong>\n", "\n</strong></td>\n",

# ordered list
		".ol", "<ol>\n", "\n</ol>\n",
# unordered list
		".ul", "<ul>\n", "\n</ul>\n",
# unordered list item
		".uli", "\n<li>\n", "",
# ordered list (first) item
		".oli1", "\n<li>\n", "",
# ordered list item
		".oli", "\n<li>\n", "",

# punctuation marks
		".", ".\n", "",
		",", ",\n", "",

# included file
		".include", "", "",

# definition list
		".dl", "<p><dl>\n", "\n</dl>\n",
# definition list items (regular, literal, variable, name, menu item) dt, then dd (with <p> after)
		".dt", "<dt>\n", "\n</dt>",
		".dtl", "<dt><code>\n", "\n</code></dt>\n",
		".dtv", "<dt><code><em>\n", "\n</em></code></dt>\n",
		".dtn", "<dt><em>\n", "\n</em></dt>\n",
		".dtm", "<dt><strong>\n", "\n</strong></dt>\n",
# definition list definition
		".dd", "<dd>\n", "</dd><p>\n",

# NOTE
		".note",
		"\n<dl><dt>\n<strong>NOTE:</strong>\n</dt><dd><hr>\n", "\n<hr></dd></dl>\n",

# cross reference (to man page)
		".x",
		"\n<a href=", "</a>\n",

# comment (will appear in doc unless turned off with -C)
		".com",
		"<code><strong>/***BEGIN </strong></code><strong>",
		"</strong><code><strong> END***/</strong></code>",

# hyperlink label
		".label",
		"\n<a name=\"", "\"></a>",
		
# general link
		".link",
		"\n<a href=", "</a>",

# image link
		".img",
		"\n<img src=\"", "\">",

# insert date information
		".date",
		mtime, "",

		".dateshort",
		date_mdy, "",

		".year",
		year, "",

# horizontal rule
		".hr", "\n<hr>\n", "",

# line break
		".br", "\n<br>\n", "",

		".#",
		"", "",
		
# HTML title
		".title",
		"<title>", "\n</title>\n</head><body bgcolor=\"white\">\n",

# manual page name paragraph (under NAME section)
		".name",
		"\n<p>\n",	"\n</p>\n",
		
	};

}

