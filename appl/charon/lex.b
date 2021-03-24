implement Lex;

include "common.m";
include "charon_code.m";
charon_code:Charon_code;

# local copies from CU
sys: Sys;
CU: CharonUtils;
S: String;
T: StringIntTab;
C: Ctype;
ctype: array of byte;

EOF : con -2;
EOB : con -1;

tagnames = array[] of {
	" ",
	"!",
	"a", 
	"abbr",
	"acronym",
	"address",
	"applet", 
	"area",
	"b",
	"base",
	"basefont",
	"bdo",
	"big",
	"blink",
	"blockquote",
	"body",
	"bq",
	"br",
	"button",
	"caption",
	"center",
	"cite",
	"code",
	"col",
	"colgroup",
	"dd",
	"del",
	"dfn",
	"dir",
	"div",
	"dl",
	"dt",
	"em",
	"fieldset",
	"font",
	"form",
	"frame",
	"frameset",
	"h1",
	"h2",
	"h3",
	"h4",
	"h5",
	"h6",
	"head",
	"hr",
	"html",
	"i",
	"iframe",
	"img",
	"input",
	"ins",
	"isindex",
	"kbd",
	"label",
	"legend",
	"li",
	"link",
	"map",
	"menu",
	"meta",
	"nobr",
	"noframes",
	"noscript",
	"object",
	"ol",
	"optgroup",
	"option",
	"p",
	"param",
	"pre",
	"q",
	"s",
	"samp",
	"script",
	"select",
	"small",
	"span",
	"strike",
	"strong",
	"style",
	"sub",
	"sup",
	"table",
	"tbody",
	"td",
	"textarea",
	"tfoot",
	"th",
	"thead",
	"title",
	"tr",
	"tt",
	"u",
	"ul",
	"var"
};

tagtable : array of T->StringInt;	# initialized from tagnames

attrnames = array[] of {
	"abbr",
	"accept-charset",
	"access-key",
	"action",
	"align",
	"alink",
	"alt",
	"archive",
	"axis",
	"background",
	"bgcolor",
	"border",
	"cellpadding",
	"cellspacing",
	"char",
	"charoff",
	"charset",
	"checked",
	"cite",
	"class",
	"classid",
	"clear",
	"code",
	"codebase",
	"codetype",
	"color",
	"cols",
	"colspan",
	"compact",
	"content",
	"coords",
	"data",
	"datetime",
	"declare",
	"defer",
	"dir",
	"disabled",
	"enctype",
	"face",
	"for",
	"frame",
	"frameborder",
	"headers",
	"height",
	"href",
	"hreflang",
	"hspace",
	"http-equiv",
	"id",
	"ismap",
	"label",
	"lang",
	"link",
	"longdesc",
	"lowsrc",
	"marginheight",
	"marginwidth",
	"maxlength",
	"media",
	"method",
	"multiple",
	"name",
	"nohref",
	"noresize",
	"noshade",
	"nowrap",
	"object",
	"onabort",
	"onblur",
	"onchange",
	"onclick",
	"ondblclick",
	"onerror",
	"onfocus",
	"onkeypress",
	"onkeyup",
	"onload",
	"onmousedown",
	"onmousemove",
	"onmouseout",
	"onmouseover",
	"onmouseup",
	"onreset",
	"onselect",
	"onsubmit",
	"onunload",
	"profile",
	"prompt",
	"readonly",
	"rel",
	"rev",
	"rows",
	"rowspan",
	"rules",
	"scheme",
	"scope",
	"scrolling",
	"selected",
	"shape",
	"size",
	"span",
	"src",
	"standby",
	"start",
	"style",
	"summary",
	"tabindex",
	"target",
	"text",
	"title",
	"type",
	"usemap",
	"valign",
	"value",
	"valuetype",
	"version",
	"vlink",
	"vspace",
	"width"
};

attrtable : array of T->StringInt;	# initialized from attrnames

chartab:= array[] of { T->StringInt
	("AElig", 'Æ'),
	("Aacute", 'Á'),
	("Acirc", 'Â'),
	("Agrave", 'À'),
	("Aring", 'Å'),
	("Atilde", 'Ã'),
	("Auml", 'Ä'),
	("Ccedil", 'Ç'),
	("ETH", 'Ð'),
	("Eacute", 'É'),
	("Ecirc", 'Ê'),
	("Egrave", 'È'),
	("Euml", 'Ë'),
	("Iacute", 'Í'),
	("Icirc", 'Î'),
	("Igrave", 'Ì'),
	("Iuml", 'Ï'),
	("Ntilde", 'Ñ'),
	("Oacute", 'Ó'),
	("Ocirc", 'Ô'),
	("Ograve", 'Ò'),
	("Oslash", 'Ø'),
	("Otilde", 'Õ'),
	("Ouml", 'Ö'),
	("THORN", 'Þ'),
	("Uacute", 'Ú'),
	("Ucirc", 'Û'),
	("Ugrave", 'Ù'),
	("Uuml", 'Ü'),
	("Yacute", 'Ý'),
	("aacute", 'á'),
	("acirc", 'â'),
	("acute", '´'),
	("aelig", 'æ'),
	("agrave", 'à'),
	("alpha", 'α'),
	("amp", '&'),
	("aring", 'å'),
	("atilde", 'ã'),
	("auml", 'ä'),
	("beta", 'β'),
	("brvbar", '¦'),
	("ccedil", 'ç'),
	("cdots", '⋯'),
	("cedil", '¸'),
	("cent", '¢'),
	("chi", 'χ'),
	("copy", '©'),
	("curren", '¤'),
	("ddots", '⋱'),
	("deg", '°'),
	("delta", 'δ'),
	("divide", '÷'),
	("eacute", 'é'),
	("ecirc", 'ê'),
	("egrave", 'è'),
	("emdash", '—'),
	("emsp", ' '),
	("endash", '–'),
	("ensp", ' '),
	("epsilon", 'ε'),
	("eta", 'η'),
	("eth", 'ð'),
	("euml", 'ë'),
	("frac12", '½'),
	("frac14", '¼'),
	("frac34", '¾'),
	("gamma", 'γ'),
	("gt", '>'),
	("iacute", 'í'),
	("icirc", 'î'),
	("iexcl", '¡'),
	("igrave", 'ì'),
	("iota", 'ι'),
	("iquest", '¿'),
	("iuml", 'ï'),
	("kappa", 'κ'),
	("lambda", 'λ'),
	("laquo", '«'),
	("ldots", '…'),
	("lt", '<'),
	("macr", '¯'),
	("micro", 'µ'),
	("middot", '·'),
	("mu", 'μ'),
	("nbsp", ' '),
	("not", '¬'),
	("ntilde", 'ñ'),
	("nu", 'ν'),
	("oacute", 'ó'),
	("ocirc", 'ô'),
	("ograve", 'ò'),
	("omega", 'ω'),
	("omicron", 'ο'),
	("ordf", 'ª'),
	("ordm", 'º'),
	("oslash", 'ø'),
	("otilde", 'õ'),
	("ouml", 'ö'),
	("para", '¶'),
	("phi", 'φ'),
	("pi", 'π'),
	("plusmn", '±'),
	("pound", '£'),
	("psi", 'ψ'),
	("quad", ' '),
	("quot", '"'),
	("raquo", '»'),
	("reg", '®'),
	("rho", 'ρ'),
	("sect", '§'),
	("shy", '­'),
	("sigma", 'σ'),
	("sp", ' '),
	("sup1", '¹'),
	("sup2", '²'),
	("sup3", '³'),
	("szlig", 'ß'),
	("tau", 'τ'),
	("theta", 'θ'),
	("thinsp", ' '),
	("thorn", 'þ'),
	("times", '×'),
	("trade", '™'),
	("uacute", 'ú'),
	("ucirc", 'û'),
	("ugrave", 'ù'),
	("uml", '¨'),
	("upsilon", 'υ'),
	("uuml", 'ü'),
	("varepsilon", '∈'),
	("varphi", 'ϕ'),
	("varpi", 'ϖ'),
	("varrho", 'ϱ'),
	("vdots", '⋮'),
	("vsigma", 'ς'),
	("vtheta", 'ϑ'), 
	("xi", 'ξ'),
	("yacute", 'ý'),
	("yen", '¥'),
	("yuml", 'ÿ'),
	("zeta", 'ζ'),
};

# Characters Winstart..Winend are those that Windows
# uses interpolated into the Latin1 set.
# They aren't supposed to appear in HTML, but they do....
Winstart : con 16r7f;
Winend: con 16r9f;
winchars := array[] of { '•',
	'•', '•', '‚', 'ƒ', '„', '…', '†', '‡',
	'ˆ', '‰', 'Š', '‹', 'Œ', '•', '•', '•',
	'•', '‘', '’', '“', '”', '•', '–', '—',
	'˜', '™', 'š', '›', 'œ', '•', '•', 'Ÿ' 
};

NAMCHAR : con (C->L|C->U|C->D|C->N);
LETTER : con (C->L|C->U);

dbg := 0;
warn := 0;

init(cu: CharonUtils)
{
	CU = cu;
	sys = load Sys Sys->PATH;
	S = load String String->PATH;
	C = cu->C;
	T = load StringIntTab StringIntTab->PATH;
	tagtable = CU->makestrinttab(tagnames);
	attrtable = CU->makestrinttab(attrnames);
	ctype = C->ctype;
########### for i18n 
        charon_code=load Charon_code Charon_code->PATH;
        if(charon_code==nil)
                CU->raise(sys->sprint("EXinternal:couldn't load Charon_code:%r"));
        charon_code->init();

}

TokenSource.new(b: ref CU->ByteSource, chset, mtype: int) : ref TokenSource
{
	if(!(chset==CU->US_Ascii || chset==CU->ISO_8859_1 ||
			chset==CU->UTF_8 || chset==CU->Unicode))
		CU->raise("EXInternal: bad character set");
	ans := ref TokenSource(
		0,		# i
		b,		# b
		chset,	# chset
		mtype,	# mtype
		0		# ihigh
		);
	dbg = int (CU->config).dbg['x'];
	warn = (int (CU->config).dbg['w']) || dbg;
	return ans;
}

TokenSource.gettoks(ts: self ref TokenSource): array of ref Token
{
	ToksMax : con 500;		# max chunk of tokens returned
	a := array[ToksMax] of ref Token;
	ai := 0;
	if(dbg) {
		sys->print("gettoks starts, ts.i=%d, ts.b.edata=%d\n", ts.i, ts.b.edata);
		if(dbg > 1 && ts.i < ts.b.edata) {
			sys->print("SOURCE from ts.i on...\n");
			sys->write(sys->fildes(1), ts.b.data[ts.i:ts.b.edata], ts.b.edata-ts.i);
			sys->print("\nSOURCE END\n\n");
		}
	}
	if(ts.mtype == CU->TextHtml) {
		while(ai < ToksMax) {
			starti := ts.i;
			c := getchar(ts);
			if(c < 0)
				break;
			tok : ref Token;
			if(c == '<') {
				tok = gettag(ts, starti);
				if(tok != nil && tok.tag != Comment) {
					a[ai++] = tok;
					if(tok.tag == Tscript) {
						# special rules for getting Data after...
						starti = ts.i;
						c = getchar(ts);
						tok = getscriptdata(ts, c, starti);
						if(tok != nil)
							a[ai++] = tok;
					}
				}
			}
			else {
				tok = getdata(ts, c, starti);
				if(tok != nil)
					a[ai++] = tok;
			}
			if(tok == nil)
				break;
			else
				if(dbg > 1)
					sys->print("lex: got token %s\n", tok.tostring());
		}
		if(ai > 0) {
			# Several tags expect PCDATA after them,
			# which means that build needs to see another tag or eof
			# after any data in order to know that PCDATA is ended.
			# Backup if we haven't got to the following tag yet.
			for(i := ai-1; i >= 0 && a[i].tag == Data; )
				i--;
			if(i >= 0) {
				t := a[i].tag;
				if(t == Toption || t == Tselect || t == Ttextarea || t == Ttitle || t == Tscript) {
					if(ts.i != len ts.b.data) {
						# not eof, so backup
						ts.i = a[i].starti;
						ai = i;
					}
				}
			}
		}
	}
	else {
		# plain text (non-html) tokens
		while(ai < ToksMax) {
			tok := getplaindata(ts);
			if(tok == nil)
				break;
			else
				a[ai++] = tok;
			if(dbg > 1)
				sys->print("lex: got token %s\n", tok.tostring());
		}
	}
	if(dbg)
		sys->print("lex: returning %d tokens\n", ai);
	ts.b.lim = ts.ihigh;
	if(ai == 0)
		return nil;
	return a[0:ai];
}

# For case where source isn't HTML.
# Just make data tokens, one per line (or partial line,
# at end of buffer), ignoring non-whitespace control
# characters and dumping \r's
getplaindata(ts: ref TokenSource): ref Token
{
	s := "";
	j := 0;
	starti := ts.i;

	for(c := getchar(ts); c >= 0; c = getchar(ts)) {
		if(c < ' ') {
			if(ctype[c] == C->W) {
				if(c == '\r') {
					# ignore it unless no following '\n',
					# in which case treat it like '\n'
					c = getchar(ts);
					if(c != '\n') {
						if(c >= 0)
							ungetchar(ts, c);
						c = '\n';
					}
				}
			}
			else
				c = 0;	# ignore
		}
		if(c != 0)
			s[j++] = c;
		if(c == '\n')
			break;
	}
	if(s == "")
		return nil;
	return ref Token(Data, s, nil, starti);
}

# Gather data up to next start-of-tag or end-of-buffer.
# Translate entity references (&amp;).
# Ignore non-whitespace control characters and get rid of \r's.
getdata(ts: ref TokenSource, firstc, starti: int): ref Token
{
	s := "";
	j := 0;
	c := firstc;

	while(c >= 0) {
		if(c == '&') {
			ok : int;
			(c, ok) = ampersand(ts);
			if(!ok)
				break;	# incomplete entity reference (ts backed up by ampersand)
		}
		else if(c < ' ') {
			if(ctype[c] == C->W) {
				if(c == '\r') {
					# ignore it unless no following '\n',
					# in which case treat it like '\n'
					c = getchar(ts);
					if(c != '\n') {
						if(c >= 0)
							ungetchar(ts, c);
						c = '\n';
					}
				}
			}
			else {
				if(warn)
					sys->print("warning: non-whitespace control character %d ignored\n", c);
				c = 0;	# ignore
			}
		}
		else if(c == '<') {
			ungetchar(ts, c);
			break;
		}
		if(c != 0)
			s[j++] = c;
		c = getchar(ts);
	}
	if(s == "")
		return nil;
	return ref Token(Data, s, nil, starti);
}

# The rules for lexing scripts are different (ugh).
# Gather up everything until see a </SCRIPT>.
getscriptdata(ts: ref TokenSource, firstc, starti: int): ref Token
{
	s := "";
	j := 0;
	tstarti := starti;
	c := firstc;

	while(c >= 0) {
		while(c == '<') {
			# Other browsers ignore stuff to end of line after <!
			savei := ts.i;
			c = getchar(ts);
			if(c == '!') {
				while(c >= 0 && c != '\n')
					c = getchar(ts);
				if(c == '\n')
					c = getchar(ts);
			}
			else if(c >= 0) {
				backup(ts, savei);
				tok := gettag(ts, tstarti);
				if(tok == nil)
					break;
				backup(ts, tstarti);
				if(tok.tag == Tscript+RBRA)
					return ref Token(Data, s, nil, starti);
			
				# here tag was not </SCRIPT>, so take as regular data
				c = getchar(ts);	# should be '<' again
				break;
			}
		}
		if(c < 0)
			break;
		if(c != 0)
			s[j++] = c;
		tstarti = ts.i;
		c = getchar(ts);
	}
	if(ts.i == ts.b.edata)
		return ref Token(Data, s, nil, starti);
	backup(ts, starti);
	return nil;
}

# We've just seen a '<'.  Gather up stuff to closing '>' (if buffer
# ends before then, return nil).
# If it's a tag, look up the name, gather the attributes, and return
# the appropriate token.
# Else it's either just plain data or some kind of ignorable stuff:
# return a Data or Comment token as appropriate.
gettag(ts: ref TokenSource, starti: int): ref Token
{
	rbra := 0;
	ans : ref Token = nil;
	al: list of Attr;
	nexti := ts.i;
	c := getchar(ts);

	# dummy loop: break out of this when hit end of buffer
 eob:
	for(;;) {
		if(c == '/') {
			rbra = RBRA;
			c = getchar(ts);
		}
		if(c < 0)
			break eob;
		if(c>=C->NCTYPE || !int (ctype[c]&LETTER)) {
			# not a tag
			if(c == '!') {
				ans = comment(ts, starti);
				if(ans != nil)
					return ans;
				break eob;
			}
			else {
				backup(ts, nexti);
				return ref Token(Data, "<", nil, starti);
			}
		}
		# c starts a tagname
		ans = ref Token(Notfound, nil, nil, starti);
		name := "";
		name[0] = lowerc(c);
		i := 1;
		for(;;) {
			c = getchar(ts);
			if(c < 0)
				break eob;
			if(c>=C->NCTYPE || !int (ctype[c]&NAMCHAR))
				break;
			name[i++] = lowerc(c);
		}
		(fnd, tag) := T->lookup(tagtable, name);
		if(fnd)
			ans.tag = tag+rbra;
		else
			ans.text = name;	# for warning print, in build
attrloop:
		for(;;) {
			# look for "ws name" or "ws name ws = ws val"  (ws=whitespace)
			# skip whitespace
			while(c < C->NCTYPE && ctype[c] == C->W) {
				c = getchar(ts);
				if(c < 0)
					break eob;
			}
			if(c == '>')
				break attrloop;
			if(c == '<') {
				if(warn)
					sys->print("warning: unclosed tag; last name=%s\n", name);
				ungetchar(ts, '<');
				break attrloop;
			}
			if(c >= C->NCTYPE || !int (ctype[c]&LETTER)) {
				if(warn)
					sys->print("warning: expected attribute name; last name=%s\n", name);
				# skip to next attribute name
				for(;;) {
					c = getchar(ts);
					if(c < 0)
						break eob;
					if(c < C->NCTYPE && int (ctype[c]&LETTER))
						continue attrloop;
					if(c == '<') {
						if(warn)
							sys->print("warning: unclosed tag; last name=%s\n", name);
						ungetchar(ts, '<');
						break attrloop;
					}
					if(c == '>')
						break attrloop;
				}
			}
			# gather attribute name
			name = "";
			name[0] = lowerc(c);
			i = 1;
			for(;;) {
				c = getchar(ts);
				if(c < 0)
					break eob;
				if(c >= C->NCTYPE || !int (ctype[c]&NAMCHAR))
					break;
				name[i++] = lowerc(c);
			}
			(afnd, attid) := T->lookup(attrtable, name);
			if(warn && !afnd)
				sys->print("warning: unknown attribute name %s\n", name);
			# skip whitespace
			while(c < C->NCTYPE && ctype[c] == C->W) {
				c = getchar(ts);
				if(c < 0)
					break eob;
			}
			if(c != '=') {
				# no value for this attr
				if(afnd)
					al = (attid, "") :: al;
				continue attrloop;
			}
			# c is '=' here;  skip whitespace
			for(;;) {
				c = getchar(ts);
				if(c < 0)
					break eob;
				if(c >= C->NCTYPE || ctype[c] != C->W)
					break;
			}
			# gather value
			quote := 0;
			if(c == '\'' || c == '"') {
				quote = c;
				c = getchar(ts);
				if(c < 0)
					break eob;
			}
			val := "";
			nv := 0;
		valloop:
			for(;;) {
				if(c < 0)
					break eob;
				if(c == '>') {
					if(quote) {
						# c might be part of string (though not good style)
						# but if line ends before close quote, assume
						# there was an unmatched quote
						ti := ts.i;
						for(;;) {
							c = getchar(ts);
							if(c < 0)
								break eob;
							if(c == quote) {
								backup(ts, ti);
								val[nv++] = '>';
								c = getchar(ts);
								continue valloop;
							}
							if(c == '\n') {
								if(warn)
									sys->print("warning: apparent unmatched quote\n");
								backup(ts, ti);
								quote = 0;
								c = '>';
								break valloop;
							}
						}
					}
					else
						break valloop;
				}
				if(quote) {
					if(c == quote) {
						c = getchar(ts);
						if(c < 0)
							break eob;
						break valloop;
					}
					if(c == '\r') {
						c = getchar(ts);
						continue valloop;
					}
					if(c == '\t' || c == '\n')
						c = ' ';
				}
				else {
					if(c < C->NCTYPE && ctype[c]==C->W)
						break valloop;
				}
				if(c == '&') {
					ok : int;
					(c, ok) = ampersand(ts);
					if(!ok)
						break eob;
				}
				val[nv++] = c;
				c = getchar(ts);
			}
			if(afnd)
				al = (attid, val) :: al;
		}
		ans.attr = al;
		return ans;
	}
	if(ts.i == len ts.b.data) {
		if(warn)
			sys->print("warning: incomplete tag at end of page\n");
		backup(ts, nexti);
		return ref Token(Data, "<", nil, starti);
	}
	else
		backup(ts, starti);
	return nil;
}

# We've just read a '<!' at position starti,
# so this may be a comment or other ignored section, or it may
# be just a literal string if there is no close before end of file
# (other browsers do that).
# The accepted practice seems to be (note: contrary to SGML spec!):
# If see <!--, look for --> to close, or if none, > to close.
# If see <!(not --), look for > to close.
# If no close before end of file, leave original characters in as literal data.
#
# If we see ignorable stuff, return Comment token.
# Else return nil (caller should back up and try again when more data arrives,
# unless at end of file, in which case caller should just make '<' a data token).
comment(ts: ref TokenSource, starti: int) : ref Token
{
	nexti := ts.i;
	havecomment := 0;
	commentstart := 0;
	c := getchar(ts);
	if(c == '-') {
		c = getchar(ts);
		if(c == '-') {
			commentstart = 1;
			if(findstr(ts, "-->"))
				havecomment = 1;
			else
				backup(ts, nexti);
		}
	}
	if(!havecomment) {
		if(c == '>')
			havecomment = 1;
		else if(c >= 0) {
			if(!commentstart || ts.b.edata == len ts.b.data) {
				if(findstr(ts, ">"))
					havecomment = 1;
			}
		}
	}
	if(havecomment)
		return ref Token(Comment, nil, nil, starti);
	return nil;
}

# Look for string s in token source.
# If found, return 1, with buffer at next char after s,
# else return 0 (caller should back up).
findstr(ts: ref TokenSource, s: string) : int
{
	c0 := s[0];
	n := len s;

 mainloop:
	for(;;) {
		c := getchar(ts);
		if(c < 0)
			break;
		if(c == c0) {
			if(n == 1)
				return 1;
			nexti := ts.i;
			for(i := 1; i < n; i++) {
				c = getchar(ts);
				if(c < 0)
					break mainloop;
				if(c != s[i])
					break;
			}
			if(i == n)
				return 1;
			# got to deal with self overlap in s
			backup(ts, nexti);
		}
	}
	return 0;
}

# We've just read an '&'; look for an entity reference
# name, and if found, return (translated char, 1).
# if there is a complete entity name but it isn't known,
# try prefixes (gets around some buggy HTML out there),
# and if that fails, back up to just past the '&' and return ('&', 1).
# If the entity can't be completed in the current buffer, back up
# to the '&' and return (0, 0).
ampersand(ts: ref TokenSource): (int, int)
{
	savei := ts.i;
	c := getchar(ts);
	fnd := 0;
	ans := 0;
	if(c == '#') {
		c = getchar(ts);
		v := 0;
		while(c >= 0) {
			if(ctype[c] != C->D)
				break;
			v = v*10 + c-'0';
			c = getchar(ts);
		}
		if(c >= 0) {
			if(!(c == ';' || c == '\n' || c == '\r'))
				ungetchar(ts, c);
			c = v;
			if(c==160)
				c = ' ';   # non-breaking space
			if(c >= Winstart && c <= Winend)
				c = winchars[c-Winstart];
			ans = c;
			fnd = 1;
		}
	}
	else if(c >= 0 && c < C->NCTYPE && int (ctype[c]&(C->L|C->U))) {
		s := "";
		s[0] = c;
		k := 1;
		for(;;) {
			c = getchar(ts);
			if(c < 0)
				break;
			if(c < C->NCTYPE && int (ctype[c]&NAMCHAR))
				s[k++] = c;
			else {
				if(!(c == ';' || c == '\n' || c == '\r'))
					ungetchar(ts, c);
				break;
			}
		}
		if(c >= 0) {
			(fnd, ans) = T->lookup(chartab, s);
			if(!fnd) {
				# Try prefixes of s
				if(c == ';' || c == '\n' || c == '\r')
					ungetchar(ts, c);
				while(--k > 0) {
					(fnd, ans) = T->lookup(chartab, s[0:k]);
					if(fnd) {
						i := len s;
						while(i > k) {
							i--;
							ungetchar(ts, s[i]);
						}
						break;
					}
				}
			}
		}
	}
	if(!fnd) {
		backup(ts, savei);
		if(c < 0 && ts.i < len ts.b.data) {
			# was incomplete
			ungetchar(ts, '&');
			return (0, 0);
		}
		else
			return ('&', 1);
	}
	return (ans, 1);
}

# If c is an uppercase letter, return its lowercase version,
# otherwise return c.
# Assume c is a NAMCHAR, so don't need range check on ctype[]
lowerc(c: int) : int
{
	if(ctype[c] == C->U) {
		# this works for accented characters in Latin1, too
		return c + 16r20;
	}
	return c;
}

Token.aval(t: self ref Token, attid: int): (int, string)
{
	attr := t.attr;
	while(attr != nil) {
		a := hd attr;
		if(a.attid == attid)
			return (1, a.value); 
		attr = tl attr;
	}
	return (0, "");
}


# for debugging
Token.tostring(t: self ref Token) : string
{
	ans := "";
	if(dbg > 1)
		ans = sys->sprint("[%d]", t.starti);
	tag := t.tag;
	if(tag == Data)
		ans = ans + "'" + t.text + "'";
	else {
		ans = ans + "<";
		if(tag >= RBRA) {
			tag -= RBRA;
			ans = ans + "/";
		}
		tname := tagnames[tag];
		if(tag == Notfound)
			tname = "?";
		ans = ans + S->toupper(tname);
		for(al := t.attr; al != nil; al = tl al) {
			a := hd al;
			aname := attrnames[a.attid];
			ans = ans + " " + aname;
			if(a.value != "")
				ans = ans + "='" + a.value + "'";
		}
		ans = ans + ">";
	}
	return ans;
}

# Get next char, obeying ts.chset.
# Returns -1 if no complete character left before current end of data.
getchar(ts: ref TokenSource) : int
{
	bs := ts.b;
	if(ts.i >= bs.edata)
		return -1;
	buf := bs.data;
	# for i18n
	(unicodechar,index):=charon_code->convCode(buf,ts.i);
        if(unicodechar!=-1) {
                        ts.i=index;
                        return unicodechar;
        }

	c := int buf[ts.i];
	case ts.chset {
	CU->ISO_8859_1 =>
		if(c >= Winstart && c <= Winend)
			c = winchars[c-Winstart];
		ts.i++;
	CU->US_Ascii =>
		if(c > 127) {
			if(warn)
				sys->print("non-ascii char (%x) when US-ASCII specified\n", c);
			# Could mask it off, but more likely the Latin1 character was intended
			# and the content-type label was wrong, so just leave c as is.
		}
		ts.i++;
	CU->UTF_8 =>
		n, ok: int;
		(c, n, ok) = sys->byte2char(buf, ts.i);
		if(ok && ts.i+n <= bs.edata)
			ts.i += n;
		else if(n == 0 || ts.i+n > bs.edata) {
			# not enough bytes in ts.buf to complete utf-8 char
			if(bs.edata == len buf)
				ts.i = bs.edata;	# mark "all used"
			c = -1;
		}
		else {
			# invalid UTF sequence, c is now error char
			if(warn)
				sys->print("warning: invalid utf-8 sequence (starts with %x)\n", int ts.b.data[ts.i]);
			ts.i += n;
		}
	CU->Unicode =>
		if(ts.i < bs.edata - 1) {
			# standards say most-significant byte first
			c = (c<<8) | (int buf[ts.i+1]);
			ts.i += 2;
		}
		else {
			if(bs.edata == len buf)
				ts.i = bs.edata;	# mark "all used"
			c = -1;
		}
	}
	if(ts.i > ts.ihigh)
		ts.ihigh = ts.i;
	return c;
}

# Assuming c was the last character returned by getchar, set
# things up so that next getchar will get that same character
# followed by the current 'next character', etc.
ungetchar(ts: ref TokenSource, c: int)
{
	n := 1;
	case ts.chset {
	CU->UTF_8 =>
		if(c >= 128) {
			a := array[sys->UTFmax] of byte;
			n = sys->char2byte(c, a, 0);
		}
	CU->Unicode =>
		n = 2;
	}
	ts.i -= n;
}

# Restore ts so that it is at the state where buf was savebuf
# and the index was savei.
backup(ts: ref TokenSource, savei: int)
{
	if(dbg)
		sys->print("lex: backup; i=%d, savei=%d\n",
				ts.i, savei);
	ts.i = savei;
}
