# Webgrab -- for getting html pages and the subordinate files (images, frame children)
# they refer to (using "src=..." in a tag) into the local file space.
# Assume http: scheme if none specified.
# Usage:
#	webgrab [-r] [-v] [-o stem] url
#  If stem is specified, file will be saved in stem.html and images will
#  go in stem_1.jpg (or .gif, ...), stem_2.jpg, etc.
#  If stem is not specified, derive it from url (see getstem comment, below).
# If -r is specified, get "raw", i.e., no image fetching/html munging.
# If -v is specified (verbose), print some progress information,
# with more if -vv is given.

implement Webgrab;

include "sys.m";
include "draw.m";
include "string.m";
include "url.m";
include "daytime.m";
include "bufio.m";

sys: Sys;
	FD: import sys;
S: String;
U: Url;
	ParsedUrl: import U;
DT: Daytime;
B: Bufio;

Webgrab: module
{
	init: fn(ctxt: ref Draw->Context, argl: list of string);
};

stderr: ref FD;
verbose := 0;

httpproxy: ref Url->ParsedUrl;
noproxydoms: list of string;	# domains that don't require proxy

init(nil: ref Draw->Context, argl: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	S = load String String->PATH;
	U = load Url Url->PATH;
	DT = load Daytime Daytime->PATH;
	B = load Bufio Bufio->PATH;
	if(S == nil || U == nil || DT == nil || B == nil)
		error_exit("can't load a module");
	stem := "";
	rawflag := 0;
	argl = tl argl;
	if(argl == nil)
		usage_exit();
	url := "";
	while(argl != nil) {
		arg := hd argl;
		argl = tl argl;
		if(arg == "-o") {
			if(argl == nil)
				usage_exit();
			stem = hd argl;
			argl = tl argl;
		}
		else if(arg == "-r")
			rawflag = 1;
		else if(arg == "-v")
			verbose = 1;
		else if(arg == "-vv")
			verbose = 2;
		else {
			url = arg;
			break;
		}
	}
	if(url == "" || argl != nil)
		usage_exit();
	(nil,xr) := S->splitstrl(url,"//");
	(nil,yr) := S->splitl(url,":");
	if(xr == "" && yr == "")
		url = "http://" + url;
	u := U->makeurl(url);
	if(stem == "")
		stem = getstem(u);
	readconfig();
	grab(u, stem, rawflag);
}

readconfig()
{
	cfgio := B->open("/services/webget/config", sys->OREAD);
	if(cfgio != nil) {
		for(;;) {
			line := B->cfgio.gets('\n');
			if(line == "") {
				B->cfgio.close();
				break;
			}
			if(line[0]=='#')
				continue;
			(key, val) := S->splitl(line, " \t=");
			val = S->take(S->drop(val, " \t="), "^\r\n");
			if(val == "")
				continue;
			case key {
			"httpproxy" =>
				if(val == "none")
					continue;
				# val should be host or host:port
				httpproxy = U->makeurl("http://" + val);
				if(verbose)
					sys->fprint(stderr, "Using http proxy %s\n", httpproxy.tostring());
			"noproxy" or
			"noproxydoms" =>
				(nil, noproxydoms) = sys->tokenize(val, ";, \t");
			}
		}
	}
}

usage_exit()
{
	sys->fprint(stderr, "Usage: webgrab [-r] [-v] [-o stem] url\n");
	exit;
}

# Make up a stem for forming save-file-names, based on url u.
# Use the last non-nil component of u.path, without a final extension,
# else use the host.  Then, if the stem still contains a '.' (e.g., www.lucent)
# use the part after the final '.'.
# Finally, if all else fails, use use "grabout".
getstem(u: ref ParsedUrl) : string
{
	stem := "";
	if(u.path != "") {
		(l, r) := S->splitr(u.path, "/");
		if(r == "") {
			# path ended with '/'; try next to last component
			if(l != "")
				(l, r) = S->splitr(l[0:len l - 1], "/");
		}
		if(r != "")
			stem = r;
	}
	if(stem == "")
		stem = u.host;
	if(stem != "") {
		ext: string;
		(stem, ext) = S->splitr(stem, ".");
		if(stem == "")
			stem = ext;
		else
			stem = stem[0:len stem - 1];
		(nil, stem) = S->splitr(stem, ".");
	}
	if(stem == "")
		stem = "grabout";
	return stem;
}

grab(u: ref ParsedUrl, stem: string, rawflag: int)
{
	(err, contents, actual) := httpget(u);
	if(err != "")
		error_exit(err);
	ish := is_html(contents);
	if(ish)
		contents = addfetchcomment(contents, u, actual);
	if(rawflag || !ish) {
		writebytes(stem, contents);
		return;
	}
	# get subordinates, modify contents
	subs : list of (string, string);
	(contents, subs)  = subfix(contents, stem);
	writebytes(stem + ".html", contents);
	for(l := subs; l != nil; l = tl l) {
		(fname, suburl) := hd l;
		subu := U->makeurl(suburl);
		subu.makeabsolute(actual);
		(suberr, subcontents, subactual) := httpget(subu);
		if(suberr != "") {
			sys->fprint(stderr, "webgrab: can't fetch subordinate %s from %s: %s\n", fname, subu.tostring(), suberr);
			continue;
		}
		writebytes(fname, subcontents);
	}
}

# Fix the html in array a so that referenced subordinate files (SRC= or BACKGROUND= fields of tags)
# are replaced with local names (stem_1.xxx, stem_2.xxx, etc.),
# and return the fixed array along with a list of (local name, subordinate url)
# of images to be fetched.
subfix(a: array of byte, stem: string) : (array of byte, list of (string, string))
{
	alen := len a;
	if(alen == 0)
		return (a, nil);
	nsubs := 0;
	newa := array[alen + 1000] of byte;
	newai := 0;
	j := 0;
	intag := 0;
	incom := 0;
	quote := 0;
	subs : list of (string, string) = nil;
	for(i := 0; i < alen; i++) {
		c := int a[i];
		if(incom) {
			if(amatch(a, i, alen, "-->")) {
				incom = 0;
				i = i+2;
			}
		}
		else if(intag) {
			if(quote==0 && (amatch(a, i, alen, "src") || amatch(a, i, alen, "background"))) {
				v := "";
				eqi := 0;
				if(amatch(a, i, alen, "src"))
					k := i+3;
				else
					k = i+10;
				for(; k < alen; k++)
					if(!iswhite(int a[k]))
						break;
				if(k < alen && int a[k] == '=') {
					eqi = k;
					k++;
					while(k<alen && iswhite(int a[k]))
						k++;
					if(k<alen) {
						kstart := k;
						c = int a[k];
						if(c == '\'' || c== '"') {
							quote = int a[k++];
							while(k<alen && (int a[k])!=quote)
								k++;
							v = string a[kstart+1:k];
							k++;
						}
						else {
							while(k<alen && !iswhite(int a[k]) && int a[k] != '>')
								k++;
							v = string a[kstart:k];
						}
					}
				}
				if(v != "") {
					f := "";
					for(l := subs; l != nil; l = tl l) {
						(ff,uu) := hd l;
						if(v == uu) {
							f = ff;
							break;
						}
					}
					if(f == "") {
						nsubs++;
						f = stem + "_" + string nsubs + getsuff(v);
						subs = (f, v) :: subs;
					}
					# should check for newa too small
					newa[newai:] = a[j:eqi+1];
					newai += eqi+1-j;
					xa := array of byte f;
					newa[newai:] = xa;
					newai += len xa;
					j = k;
				}
				i = k-1;
			}
			if(c == '>' && quote == 0)
				intag = 0;
			if(quote) {
				if(quote == c)
					quote = 0;
			else if(c == '"' || c == '\'')
				quote = c;
			}
		}
		else if(c == '<')
			intag = 1;
	}
	if(nsubs == 0)
		return (a, nil);
	if(i > j) {
		newa[newai:] = a[j:i];
		newai += i-j;
	}
	ans := array[newai] of byte;
	ans[0:] = newa[0:newai];
	anssubs : list of (string, string) = nil;
	for(ll := subs; ll != nil; ll = tl ll)
		anssubs = hd ll :: anssubs;
	return (ans, anssubs);
}

# add c after all f's in a
fixnames(a: array of byte, f: string, c: byte)
{
	alen := len a;
	n := alen - len f;
	for(i := 0; i < n; i++) {
		if(amatch(a, i, alen, f)) {
			a[i+len f] = c;
		}
	}
}

amatch(a: array of byte, i, alen: int, s: string) : int
{
	slen := len s;
	for(k := 0; i+k < alen && k < slen; k++) {
		c := int a[i+k];
		if(c >= 'A' && c <= 'Z')
			c = c + (int 'a' - int 'A');
		if(c != s[k])
			break;
	}
	if(k == slen) {
		return 1;
	}
	return 0;
}

getsuff(ustr: string) : string
{
	u := U->makeurl(ustr);
	if(u.path != "") {
		for(i := len u.path - 1; i >= 0; i--) {
			c := u.path[i];
			if(c == '.')
				return u.path[i:];
			if(c == '/')
				break;
		}
	}
	return "";
}

iswhite(c: int) : int
{
	return (c==' ' || c=='\t' || c=='\n' || c=='\r');
}

# Add a comment to end of a giving date and source of fetch
addfetchcomment(a: array of byte, u, actu: ref ParsedUrl) : array of byte
{
	now := DT->text(DT->local(DT->now()));
	ustr := u.tostring();
	actustr := actu.tostring();
	comment := "\n<!-- Fetched " + now + " from " + ustr;
	if(ustr != actustr)
		comment += ", redirected to " + actustr;
	comment += " -->\n";
	acom := array of byte comment;
	newa := array[len a + len acom] of byte;
	newa[0:] = a;
	newa[len a:] = acom;
	return newa;
}

# Get u, return (error string, body, actual url of source, after redirection)
httpget(u: ref ParsedUrl) : (string, array of byte, ref ParsedUrl)
{
	body : array of byte;
	for(redir := 0; redir < 10; redir++) {
		if(u.port == "")
			u.port = "80";	# default IP port for HTTP
		if(verbose)
			sys->fprint(stderr, "connecting to %s\n", u.host);
		dialhost, port: string;
		if(httpproxy != nil && need_proxy(u.host)) {
			dialhost = httpproxy.host;
			port = httpproxy.port;
		}
		else {
			dialhost = u.host;
			port = u.port;
		}
		(ok, net) := sys->dial("tcp!" + dialhost + "!" + port, nil);
		if(ok < 0)
			return (sys->sprint("can't dial %s: %r", dialhost), nil, nil);
		req := "GET http://" + u.host + "/" + u.path;
		if(u.query != "")
			req += "?" + u.query;
		req += " HTTP/1.0\r\n\r\n";
		if(verbose)
			sys->fprint(stderr, "writing request: %s\n", req);
		areq := array of byte req;
		n := sys->write(net.dfd, areq, len areq);
		if(n != len areq)
			return (sys->sprint("write problem: %r"), nil, nil);
		ans := readbytes(net.dfd);
		(status, rest) := stripline(ans);
		if(verbose)
			sys->fprint(stderr, "response: %s\n", status);
		(vers, statusrest) := S->splitl(status, " ");
		if(!S->prefix("HTTP/", vers))
			return ("bad reply status: " + status, rest, nil);
		code := int statusrest;
		location := "";
		body = rest;
		for(;;) {
			hline: string;
			(hline, body) = stripline(body);
			if(hline == "")
				break;
			if(verbose > 1)
				sys->fprint(stderr, "%s\n", hline);
			if(!iswhite(hline[0])) {
				(hname, hrest) := S->splitl(hline, ":");
				if(hrest != "") {
					hname = S->tolower(hname);
					hval := S->drop(hrest, ": \t");
					hval = S->take(hval, "^ \t");
					if(hname == "location")
						location = hval;
				}
			}
		}
		if(code != 200) {
			if((code == 300 || code == 301 || code == 302) && location != "") {
				# MultipleChoices, MovedPerm, or MovedTemp
				if(verbose)
					sys->fprint(stderr, "redirect to %s\n", location);
				u = U->makeurl(location);
				continue; 
			}
			return ("status not ok: " + status, rest, u);
		}
		break;
	}
	return ("", body, u);
}

need_proxy(h: string) : int
{
	doml := noproxydoms;
	if(doml == nil)
		return 1;		# all domains need proxy

	lh := len h;
	for(dom := hd doml; doml != nil; doml = tl doml) {
		ld := len dom;
		if(lh >= ld && h[lh-ld:] == dom)
			return 0;	# domain is on the noproxy list
	}

	return 1;
}

# Simple guess test for HTML: first non-white byte is '<'
is_html(a: array of byte) : int
{
	for(i := 0; i < len a; i++)
		if(!iswhite(int a[i]))
			break;
	if(i < len a && a[i] == byte '<')
		return 1;
	return 0;
}

readbytes(fd: ref Sys->FD) : array of byte
{
	buf := array[Sys->ATOMICIO] of byte;
	i := 0;
	avail := len buf;
	for(;;) {
		n := sys->read(fd, buf[i:], avail);
		if(n <= 0)
			break;
		i += n;
		avail -= n;
		if(avail < Sys->ATOMICIO) {
			newbuf := array[2*(len buf)] of byte;
			newbuf[0:] = buf;
			buf = newbuf;
			avail = len newbuf - i;
		}
	}
	return buf[0:i];
}

writebytes(f: string, a: array of byte)
{
	ofd := sys->create(f, Sys->OWRITE, 8r664);
	if(ofd == nil) {
		sys->fprint(stderr, "webgrab: can't create %s\n", f);
		return;
	}
	i := 0;
	clen := len a;
	while(i < clen) {
		n := sys->write(ofd, a[i:], clen-i);
		if(n < 0) {
			sys->fprint(stderr, "webgrab: write error: %r\n");
			return;
		}
		i += n;
	}
	sys->fprint(stderr, "created %s, %d bytes\n", f, clen);
}

stripline(b: array of byte) : (string, array of byte)
{
	n := len b - 1;
	for(i := 0; i < n; i++)
		if(b[i] == byte '\r' && b[i+1] == byte '\n')
			return (string b[0:i], b[i+2:]);
	return ("", b);
}

error_exit(msg: string)
{
	sys->fprint(sys->fildes(2), "%s\n", msg);
	exit;
}
