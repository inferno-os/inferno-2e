implement InfernoDoc;

# a simple interpreter for a simple markup language
# Uses Bufio for both input and output (output buffer
# speeds it up considerably)
#
# produces readable HTML and MIF (maker interchange format)
# with potential to create modules for other output formats
# like XML, man, etc. (see imf.m, imf_html.b)
#
# it would be nice sometime to put all the tag-specific stuff
# in a separate module, so that this can be a generic tag
# processor for any implementation of IMF tags

# Ed Bacher, evb@lucent.com

include "sys.m";
	sys:	Sys;
	print, fprint, sprint, stat: import sys;
	stdout, stderr:	ref sys->FD;
include "draw.m";


# declares regex: Regex
include "regexutils.m";
	regexu: RegexUtils;
	match,
	match_mult,
	sub,
	sub_re,
	subg,
	subg_re:	import regexu;

	Re:	import regex;
	
include "bufio.m";
	bufio:	Bufio;
	Iobuf: import bufio;
	inbuf, outbuf: ref Iobuf;

include "daytime.m";
	daytime: Daytime;

include "workdir.m";
	workdir: Workdir;

include "string.m";
	str: String;

include "imf.m";
	imf: IMF;
	emdash_tag,
	space_tag,
	zero_tag,
	lt_tag,
	gt_tag,
	extag,
	notag,
	blocktags,
	tags,
	TOP,
	BOTTOM:	import imf;

Line: adt
{
	number: int;
	text: string;
	next: cyclic ref Line;
	getnext: fn(this: self ref Line);
};

Line.getnext(this: self ref Line)
{
	this = this.next;
	this.next.text = inbuf.gets('\n');
	this.number++;
}

#
# globals
#
maxtag : int;
debug := 1;

# flags initially 1
addspace,
commentson	: int = 1;

# flags initially 0
exflag,
ulistflag,
olistflag,
newulist,
newolist,
isfirstitem,
litflag,
inlineflag,
xrefflag, 
commentflag,
allcapsflag,
tabflag,
titleflag,			# check for <title> tag
linkflag,
tabfirst,  			# mark first line of .tab table
tabcols		: int = 0;

pname,
target,
readfile,
lastfile,
nextline,
link,
image,
tab_sep		: string;

lastline: ref Line;

# stack of (readfile, inbuf) used for .include lines
readstack:	list of (string, ref Iobuf);

# regular expressions
tag_re,
white_re,
section_re,
member_re,
chan_re,
minus_minus_re,
leadspace_re,
twospaces_re,
tab_re,
emdash_re,
space_re,
zero_re,
lt_re,
gt_re,
TWO_PIPES_re,
pipe_holder_re,
inline_angle_re,
inline_percent_re,
open_angle_re,
close_angle_re,
percent_re	: regex->Re;

# regular expression pattern constants
# note \\ needed to escape \ and pass it to regex

# a period by itself is a tag.
TAG_PATTERN: 	con "^[\\.,][/]*[\\/a-zA-Z0-9_(#)]*";

# note that inline tags can be like .l<text>, .l%text%, or .l$text$
# but the text cannot include >, %, or $
INLINE_angle:	con "(\\.[/]*[\\/a-zA-Z0-9_(#)]+)<([^>]*)>";
INLINE_percent:	con "(\\.[/]*[\\/a-zA-Z0-9_(#)]+)%([^%]*)%";

WHITESPACE: 	con "[ \t\r\n]*";
LEADSPACE: 	con "^[ \t]";
TAB:		con "[\t]";
SECTION:	con "^(.*)\\((.*)\\)";  # e.g., man(1)

EMDASH:		con "\\|[mM]";
SPACE_PAT:	con "\\|[sS]";		# non-breaking space character
ZERO:		con "\\|[zZ]";		# zero-width character
LT:		con "\\|<";
GT:		con "\\|>";
TWO_PIPES:	con "\\|\\|";
pipe_holder:	con "this_represents_a_pipe";

# checklist gets passed to checkline
checklist: list of (Re, string);

# miscellaneous constants
DEFAULT_TAB_SEP:	con ";";
PIPE:		con "|";
PRINTLEN:	con 128;	# print(2) limitation (actually 256)

InfernoDoc: module  
{
	init:   fn(ctxt: ref Draw->Context, argv: list of string);
};
 
usage()
{
	fprint(stderr, "Usage: %s [-f imf*.dis] [-h[C]] file\n", pname);
	return;
}

loaderror(mod, path: string)
{
	fprint(stderr, "error loading %s from %s: %r\n", mod, path);
	exit;
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdout = sys->fildes(1);
	stderr = sys->fildes(2);
	
	bufio = load Bufio Bufio->PATH;
	if (bufio == nil) loaderror("Bufio", Bufio->PATH);
	
	regex = load Regex Regex->PATH;
	if (regex == nil) loaderror("Regex", Regex->PATH);

	regexu = load RegexUtils RegexUtils->PATH;
	if (regexu == nil) loaderror("RegexUtils", RegexUtils->PATH);
	regexu->init();

	daytime = load Daytime Daytime->PATH;
	if (daytime == nil) loaderror("Daytime", Daytime->PATH);

	workdir = load Workdir Workdir->PATH;
	if (daytime == nil) loaderror("Workdir", Workdir->PATH);

	str = load String String->PATH;
	if (str == nil) loaderror("String", String->PATH);
	
	(pname, argv) = (hd argv, tl argv);

	get_imfPATH := 0;
	target = "html";		# default output type
	imfPATH := IMF->htmlPATH;	# default IMF implementation

# get options, files
	for (; argv != nil; argv = tl argv) {

		arg := hd argv;
		case arg {
			
			"-C"	=>
				commentson = 0;

			"-h"	=>
				imfPATH = IMF->htmlPATH;
				target = "html";

			"-hC"	=>
				imfPATH = IMF->htmlPATH;
				target = "html";
				commentson = 0;
				
# mif is currently an undocumented option!
			"-m"	=>
				imfPATH = IMF->mifPATH;
				target = "mif";

			"?" or "-?"	=>
				usage();
				return;

			"-f"	=>
				get_imfPATH = 1;

			*	=>	# should be a file, either IMF implementation
					# or an IMF document
			
				if (get_imfPATH) {
					imfPATH = arg;
					get_imfPATH = 0;
				}
				else
					readfile = arg;
		}
	}

	imf = load IMF imfPATH;
	if (imf == nil) loaderror("IMF", imfPATH);
	imf->init();
	

	path := workdir->init();
	maxtag = len tags - 2;

	outbuf = bufio->fopen(stdout, bufio->OWRITE);
	if (outbuf == nil) {
		fprint(stderr, "outbuf: error opening stdout: %r\n");
		return;
	}		

	tab_sep = DEFAULT_TAB_SEP;
	tag_re = regex->compile(TAG_PATTERN, 0);
	white_re = regex->compile(WHITESPACE, 0);
	leadspace_re = regex->compile(LEADSPACE, 0);
	tab_re = regex->compile(TAB, 0);
	emdash_re = regex->compile(EMDASH, 0);
	space_re = regex->compile(SPACE_PAT, 0);
	twospaces_re = regex->compile("  ", 0);
	zero_re = regex->compile(ZERO, 0);
	lt_re = regex->compile(LT, 0);
	gt_re = regex->compile(GT, 0);
	section_re = regex->compile(SECTION, 1);
	TWO_PIPES_re = regex->compile(TWO_PIPES, 0);
	pipe_holder_re = regex->compile(pipe_holder, 0);
	open_angle_re = regex->compile("<", 0);
	close_angle_re = regex->compile(">", 0);
	percent_re = regex->compile("%", 0);

	inline_angle_re = regex->compile(INLINE_angle, 1);
	inline_percent_re = regex->compile(INLINE_percent, 1);

	checklist = list of {
		(open_angle_re, "<"),
		(close_angle_re, ">"),
		(percent_re, "%")
	};

	outbuf.puts(sprint("%s", TOP));
	outbuf.puts(sprint("<!-- generated by %s from %s/%s -->\n", pname, path, readfile));
	outbuf.puts(sprint("<!-- %s -->\n\n", daytime->time()));
	idoc(readfile);
}

#
# idoc() returns -1 on error (to catch included file errors)
#
idoc(file: string): int
{
	lastfile = readfile;
	readfile = file;
	inbuf = bufio->open(readfile, bufio->OREAD);
	if (inbuf == nil) {
		if (lastfile != readfile) {
			fprint(stderr, "%s:%.4d : error opening %s: %r\n",
			lastfile, lastline.number, readfile);
		} else {
			fprint(stderr, "error opening %s: %r\n", readfile);
		}
		
		return -1;
	}
# push current input file info. on stack
	readstack = (readfile, inbuf) :: readstack;
	
# get first line, then examine lines one by one
	line := ref Line(0, "", nil);
	line.next = line;
	line.getnext();
	
	count: int;
	opentags: list of (string, int);
	opentags = nil;
	while (line.next.text != nil) {
		tagtest := match(tag_re, line.text);
		tagtest = str->tolower(tagtest);
		iscloser := 0;
		gotit := 0;

# ignore com blocks if -C option, but catch /com tag
# this will not catch nested com blocks
		if (commentflag && !commentson) {
			case tagtest {
				"./com"	=>
					;
				*	=>
					line.getnext();
					continue;
			}
		}

		if (tagtest != "") {
			case tagtest {
				".ex"   => exflag = 1;
				"./ex"  => exflag = 0;
				
				".exl"  => exflag = 2;
				"./exl" => exflag = 0;
				
				".ex0"  => exflag = 3;
				"./ex0" => exflag = 0;

				".l" or ".c" or ".cb" or ".ci" or ".ip"	=> litflag = 1;
				"./l" or "./c" or "./cb" or "./ci" or "./ip"	=> litflag = 0;

				".x"    => xrefflag = 1;
				"./x"   => xrefflag = 0;

				# list tag handling here
				# note that the .li tag does not need to
				# appear in the implementation of imf.m
				# but .oli1, .oli, and .uli do
				".ul"	=>
					ulistflag = 1;
					newulist = 1;
				"./ul"	=> ulistflag = 0;
				".ol"	=>
					olistflag = 1;
					newolist = 1;
				"./ol"	=> olistflag = 0;
				".li"	=>
					if (newolist) {
						isfirstitem = 1;
						tagtest = ".oli1";
					} else if (olistflag) {
						isfirstitem = 0;
						tagtest = ".oli";
					} else
						tagtest = ".uli";
				"./li"	=>
					if (newolist) {
						tagtest = "./oli1";
						newolist = 0;
					} else if (olistflag)
						tagtest = "./oli";
					else
						tagtest = "./uli";

				".com"	=> commentflag = 1;
				"./com"	=> commentflag = 0;

				".tab"	=>
					tabflag = 1;
					opentag(".table", line);
					(n, toks) := sys->tokenize(line.text, " \t");
					if (n > 1)
						tab_sep = hd tl toks;
					tabfirst = 1;
					
				"./tab"	=>
					tabflag = 0;
					tab_sep = DEFAULT_TAB_SEP;
					closetag(".table", line);

				".title"	=> titleflag = 1;

				".include"	=>
					(n, toks) := sys->tokenize(line.text, " \t");
					
					if (n <= 1) break;
					include_file := chomp(hd tl toks);
					
					if (cycle(include_file)) {
						fprint(stderr, "%s:%.4d : cyclic .include of %s\n",
							readfile, line.number, include_file);
						return -1;
					}
					
					lastfile = readfile;
					lastline = line;
					if ( idoc(include_file) < 0 ) {
						return -1;
					}

				".sh"	=>
					allcapsflag = 1;

				"./sh"	=>
					allcapsflag = 0;

				".link"	=>
					linkflag = 1;
					(n, toks) := sys->tokenize(line.text, " \t");
					if (n > 1)
						link = hd tl toks;

				"./link" => linkflag = 0;
		
			}


# check for block comments
			if ( (commentflag || tagtest == "./com") && !commentson ) {
				line.getnext();
				continue;
			}

# detect closing tags and check for unclosed and unopened tags
			if (len tagtest > 1 && tagtest[1] == '/') {
				realtag := tagtest;
				tagtest = sub(tagtest, "^\\./", ".");
				iscloser = 1;
				if (opentags == nil) {
					fprint(stderr,"%s:%.4d : unopened tag: %s\n",
						readfile, line.number, realtag);
				} else {

					# if all tags are closed, then opentags is nil, but
					# if you don't have an opening tag, there could be
					# trouble.  How can we detect that error?
					# need to detect non-blank lines and check to see
					# if a tag is open.
				
					(lastopen, linenum) := hd opentags;
					if (tagtest == lastopen && opentags != nil) {
						opentags = tl opentags;
					} else {

						fprint(stderr, "%s:%.4d : unclosed tag: %s (detected on line %d)\n",
							readfile, linenum, lastopen, line.number);
						if (opentags != nil) {
							opentags = tl opentags;
						}
					}
				}
				
			} else {
# don't add .# or . tags to opentags list (since they don't require closes)
				case tagtest {
					".#" or "." or "," or ".hr" or ".br" or ".date"
					or ".include" or ".dateshort" or ".year"     =>
						;
					*           =>
						opentags = (tagtest, line.number) :: opentags;
				}
			}

# check block tags
			for (i := 0; i < len blocktags; i += 3) {
				if (tagtest == blocktags[i]) {
					gotit = 1;
					outbuf.puts(sprint("%s", blocktags[i + 1 + iscloser]));
					break;
				}
			}

# check line tags 
			if (!gotit) {
				count = 0;
				for (count = 0; count < maxtag ; count += 3) {
					if (tagtest == tags[count]) {
						outbuf.puts(sprint("%s", tags[count+1+iscloser]));
						break;
					}
				
				}
				if (count >= maxtag)
					fprint(stderr, "%s:%.4d : unknown tag: %s\n", readfile, line.number, tagtest);
			}


		} else {
			if (commentflag && !commentson) {
				;
			} else {
				donotags(line);
			}
		}


# add space to all lines except those beginning paragraphs
		case tagtest {
			"." or "," or ".label" or ".p" or ".pin" or ".pc" or "pc0"
			or ".h1" or ".h2" or ".h2n" or ".h3" or ".h4"
			or ".sh" or ".ss" or ".h" or ".name"
				=>
				addspace = 0;
			*	=>
				addspace = 1;
		}
		line.getnext();
	}
	
# do BOTTOM stuff only for the main file (not for .include files)
	if (tl readstack == nil) outbuf.puts(sprint("%s", BOTTOM));
	outbuf.flush();
#	if (debug) fprint(stderr,"flushed outbuf\n");

# report on any remaining unclosed tags
	while (opentags != nil) {
		(tag, linenum) := hd opentags;
		fprint(stderr, "%s:%.4d : unclosed tag: %s\n", readfile, linenum, tag);
		opentags = tl opentags;
	}

# check that .title tag was used (only for topmost file, not .include files)
	if ((len readstack == 1) && !titleflag) {
		fprint(stderr, "%s:%.4d : warning: no .title tag (needs to be first tag)\n",
			readfile, 1);
	}

# if working on stack, pop it now
	if (tl readstack != nil) {
		(readfile, inbuf) = hd tl readstack;
		readstack = tl readstack;
	}
	return 0;
}

donotags(line: ref Line)
{
# non-tagged lines processing begins here

	if (xrefflag && target == "html") {
		htmlxref(line);

	} else if (exflag) {
		addspace = 0;
		if (target == "mif") {
			opentag(extag[exflag], line);
			parseline(line);
			closetag(extag[exflag], line);
		} else {
			line.text = get_cr(line.text);
			parseline(line);
		}

	} else if (tabflag) {
		parsetabline(line, tabfirst);

	} else if (linkflag) {
		parselink(line);

	# print non-tag lines if not whitespace
	} else if (match(white_re, line.text) != line.text) {
		line.text = chomp(line.text);
		parseline(line);
	}
}

getmanpage(line: ref Line): (string, string)
{
	pos := match_mult(section_re, line.text);
	if (pos == nil)
		return (nil, nil);

	(beg, end) := pos[1];
	page := line.text[beg:end];
	(beg, end) = pos[2];
	section := line.text[beg:end];

	return (page, section);
}



# not currently used (may be necessary for MIF)
clean_nextline()
{
	if (exflag) {
		case target {
			"mif"  =>
				nextline = subg_re(nextline, tab_re, "\\t");
			"html" =>
				;
		}
	} else {
		nextline = sub_re(nextline, leadspace_re, " ");
	}
}

cleanexline(text: string): string
{
	if ( (target == "mif") && exflag ) {
		text = subg_re(text, tab_re, "\\t");
	}
	
	text = subg_re(text, close_angle_re, "&gt");
	text = subg_re(text, open_angle_re, "&lt");
	
# do markup-specific special characters (may be a no-op)
	text = imf->specials(text);
	text = specials(text);
	return text;
}


cleanline(text: string): string
{
	text = subg(text, "\\", "");
	text = sub_re(text, leadspace_re, " ");
	text = subg_re(text, twospaces_re, " ");

#	text = subg_re(text, close_angle_re, "&gt");
#	text = subg_re(text, open_angle_re, "&lt");
	
	if (allcapsflag)
		text = str->toupper(text);
			
# special characters
	text = specials(text);

# do markup-specific special characters (may be a no-op)
	text = imf->specials(text);

	return text;
}


specials(text: string): string
{
# the |* escapes are deprecated, but should be
# supported for older documents
# for HTML documents, we just pass through the &*; character
# sequences.  For other targets, we will have to substitute
# appropriately. This coding ( &#[0-9]+; | &[a-z]+ )
# should be consistent with XML.
	
# protect || (double-pipe) escapes
	text = subg_re(text, TWO_PIPES_re, pipe_holder);

	text = subg_re(text, emdash_re, emdash_tag);
	text = subg_re(text, space_re, space_tag);
	text = subg_re(text, lt_re, lt_tag);
	text = subg_re(text, gt_re, gt_tag);
	text = subg_re(text, zero_re, zero_tag);
	
# put pipe back
	text = subg_re(text, pipe_holder_re, PIPE);
	
	return text;
}

# trim \n then \r from end of line
chomp(text: string): string
{
	length := len text;
	if ( (length >= 1) && (text[length - 1] == '\n') ) {
		text = text[: length - 1 ];
		
# get line length again
		length = len text;
		if ( (length >= 1) && (text[length - 1] == '\r') ) {
			text = text[: length - 1 ];
		}
	}
	return text;
}

# clean \r character from end of line -- confuses some browsers
get_cr(text: string): string
{
	length := len text;
	if ( (length >= 2) && (text[length - 2] == '\r') ) {
		text = text[: length - 2] + "\n";
	}
	return text;
}

# creates an HTML cross-reference and calls parseline()
htmlxref(line: ref Line)
{
	xrefflag = 1;
	line.text = chomp(line.text);
	(page, section) := getmanpage(line);
	line.text = "\"../" + section + "/" + page + ".html" + "\">" +
		"<em>" + page + " </em>" + "(" + section + ")" ;
	parseline(line);
	xrefflag = 0;
}

parselink(line: ref Line)
{
	line.text = chomp(line.text);
	line.text = "\"" + link + "\">" + line.text;
	parseline(line);
}

parseline(line: ref Line)
{
	# make sure you get the right inline tags:
	pos := match_mult(inline_angle_re, line.text);
	pos_percent := match_mult(inline_percent_re, line.text);
	
	if ( (pos_percent != nil) && (pos != nil) ) {
		(percent_begin, nil) := pos_percent[1];
		(angle_begin, nil) := pos[1];
		if (percent_begin < angle_begin) {
			pos = pos_percent;
		}
	}

	if (pos == nil) {
		pos = pos_percent;
	}
	
	if (pos == nil) {
		inlineflag = 0;
		if (!exflag && !xrefflag && !linkflag && !litflag) {
			checkline(line, checklist);
		}
		printpiece(line.text);
	} else {
		(tagbeg, tagend) := pos[1];
		printpiece(line.text[:tagbeg]);
		inlineflag = 1;
		addspace = 0;
		
		(beg, end) := pos[2];
		tag := line.text[tagbeg:tagend];
		word := line.text[beg:end];
		
		opentag(tag, line);
		if (tag == ".x") {
			xref := ref Line(line.number, word, nil);
			htmlxref(xref);
		} else {
			printpiece(word);
		}
		closetag(tag, line);

		rest := ref Line(line.number, line.text[end+1:], line.next);
		parseline(rest);
	}
}

parsetabline(line: ref Line, start: int)
{
	(n, toks) := sys->tokenize(line.text, tab_sep);
	if (toks == nil) {
		fprint(stderr, "%s:%.4d : error parsing table line\n", readfile, line.number);
		return;
	}

	if (start) {
		tabcols = n;
		tabfirst = 0;
	} else if (n != tabcols) {
		fprint(stderr, "%s:%.4d : inconsistent number of table columns\n", readfile, line.number);
	}
	
	opentag(".tr", line);
	while (toks != nil) {
		opentag(".td", line);
		parseline(ref Line(line.number, hd toks, line.next));
		closetag(".td", line);
		toks = tl toks;
	}
	closetag(".tr", line);
}


closetag(tag: string, line: ref Line)
{
	(nil, closer) := findtag(tag, line);
	outbuf.puts(sprint("%s", closer));
}

opentag(tag: string, line: ref Line)
{
	(opener, nil) := findtag(tag, line);
	outbuf.puts(sprint("%s", opener));
}


findtag(tagtest: string, line: ref Line): (string, string)
{
	count := 0;
	for (count = 0; count < maxtag ; count += 3) {
		if (tagtest == tags[count]) {
			return (tags[count+1], tags[count+2]);
		}
	}
	if (count >= maxtag) {
		fprint(stderr, "%s:%.4d : unknown embedded tag: %s\n", readfile, line.number, tagtest);
		return (nil, nil);
	}
	return(nil, nil);
}

checkline(line: ref Line, checklist: list of (Re, string))
{
	for (; checklist != nil; checklist = tl checklist) {
		(pattern_re, pattern) := hd checklist;
		
		if (match(pattern_re, line.text) != "") {
			fprint(stderr, "%s:%.4d : warning: bare %s in text\n", readfile, line.number, pattern);
			fprint(stderr, "%s\n\n", line.text);
		}
	}
}

printpiece(piece: string)
{
	#if (addspace && (target == "html")) piece = " " + piece;
	if (addspace) piece = " " + piece;
		
	if (!exflag && !inlineflag) piece = cleanline(piece);
	else piece = cleanexline(piece);

	if (len piece <= PRINTLEN) {
		outbuf.puts(sprint("%s%s%s", notag[1], piece, notag[2]));
	} else { 
		printlongpiece(piece);
	}
}

# deal with print(2) limitation - 256 bytes now
printlongpiece(piece: string)
{
	outbuf.puts(sprint("%s", notag[1]));
	for (i :=0; i < len piece; i += PRINTLEN) {
		start := i;
		end := i + PRINTLEN;
		if (end > len piece) end = len piece;
		chunk := piece[start:end];
		if ( n := outbuf.puts(sprint("%s", chunk)) < len chunk) {
			fprint(stderr, "error (%r): n = %d, len chunk = %d\n", n, len chunk);
		}
	}
	outbuf.puts(sprint("%s", notag[2]));
}

getlist(alist: list of string): string
{
	newstring : string;
	while (alist != nil) {
		newstring += hd alist;
		alist = tl alist;
	}
	return newstring;
}

cycle(include_file: string): int
{
	for (stack := readstack; stack != nil; stack = tl stack) {
		(file, nil) := hd stack;
		if (file == include_file) return -1;
	}
	return 0;
}
	
