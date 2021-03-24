implement Transport;

include "common.m";
include "transport.m";
include "date.m";
include "sslhs.m";

#D: Date;
# sslhs: SSLHS;
ssl3: SSL3;
Context: import ssl3;
# Inferno supported cipher suites: RSA_EXPORT_RC4_40_MD5, RSA_RC4_128_MD5
ssl_suites := array [] of {byte 0, byte 16r03, byte 0, byte 16r04};
ssl_comprs := array [] of {byte 0};

# local copies from CU
sys: Sys;
U: Url;
	ParsedUrl: import U;
S: String;
C: Ctype;
T: StringIntTab;
CU: CharonUtils;
	Netconn, ByteSource, Header, config : import CU;

ctype: array of byte;	# local copy of C->ctype

HTTPD:		con 80;		# Default IP port
HTTPSD:		con 443;	# Default IP port for HTTPS

# For Inferno, won't be able to read more than this at one go anyway
BLOCKSIZE:	con 1460;

# HTTP/1.1 Spec says 5, but we've seen more than that in non-looping redirs
# MAXREDIR:	con 10;

# tstate bits
THTTP_1_0, TPersist, TProxy, TSSL: con (1<<iota);

# Header fields (in order: general, request, response, entity)
HCacheControl, HConnection, HDate, HPragma, HTransferEncoding,
	HUpgrade, HVia,
	HKeepAlive, # extension
HAccept, HAcceptCharset, HAcceptEncoding, HAcceptLanguage,
	HAuthorization, HExpect, HFrom, HHost, HIfModifiedSince,
	HIfMatch, HIfNoneMatch, HIfRange, HIfUnmodifiedSince,
	HMaxForwards, HProxyAuthorization, HRange, HReferer,
	HUserAgent,
	HCookie, # extension
HAcceptRanges, HAge, HLocation, HProxyAuthenticate, HPublic,
	HRetryAfter, HServer, HSetProxy, HVary, HWarning,
	HWWWAuthenticate,
	HContentDisposition, HSetCookie, HRefresh, # extensions
	HWindowTarget, HPICSLabel, # more extensions
HAllow, HContentBase, HContentEncoding, HContentLanguage,
	HContentLength, HContentLocation, HContentMD5,
	HContentRange, HContentType, HETag, HExpires,
	HLastModified,
	HXReqTime, HXRespTime, HXUrl, # our extensions, for cached entities
	NumHfields: con iota;

# (track above enumeration)
hdrnames := array[] of {
	"Cache-Control",
	"Connection",
	"Date",
	"Pragma",
	"Transfer-Encoding",
	"Upgrade",
	"Via",
	"Keep-Alive",
	"Accept",
	"Accept-Charset",
	"Accept-Encoding",
	"Accept-Language",
	"Authorization",
	"Expect",
	"From",
	"Host", 
	"If-Modified-Since",
	"If-Match",
	"If-None-Match",
	"If-Range",
	"If-Unmodified-Since",
	"Max-Forwards",
	"Proxy-Authorization",
	"Range",
	"Refererer",
	"User-Agent",
	"Cookie",
	"Accept-Ranges",
	"Age", 
	"Location",
	"Proxy-Authenticate",
	"Public",
	"Retry-After",
	"Server",
	"Set-Proxy",
	"Vary",
	"Warning",
	"WWW-Authenticate",
	"Content-Disposition",
	"Set-Cookie",
	"Refresh",
	"Window-Target",
	"PICS-Label",
	"Allow", 
	"Content-Base", 
	"Content-Encoding",
	"Content-Language",
	"Content-Length",
	"Content-Location",
	"Content-MD5",
	"Content-Range",
	"Content-Type",
	"ETag",
	"Expires",
	"Last-Modified",
	"X-Req-Time",
	"X-Resp-Time",
	"X-Url"
};

# For fast lookup; track above, and keep sorted and lowercase
hdrtable := array[] of { T->StringInt
	("accept", HAccept),
	("accept-charset", HAcceptCharset),
	("accept-encoding", HAcceptEncoding),
	("accept-language", HAcceptLanguage),
	("accept-ranges", HAcceptRanges),
	("age", HAge),
	("allow", HAllow),
	("authorization", HAuthorization),
	("cache-control", HCacheControl),
	("connection", HConnection),
	("content-base", HContentBase),
	("content-disposition", HContentDisposition),
	("content-encoding", HContentEncoding),
	("content-language", HContentLanguage),
	("content-length", HContentLength),
	("content-location", HContentLocation),
	("content-md5", HContentMD5),
	("content-range", HContentRange),
	("content-type", HContentType),
	("cookie", HCookie),
	("date", HDate),
	("etag", HETag),
	("expect", HExpect),
	("expires", HExpires),
	("from", HFrom),
	("host", HHost),
	("if-modified-since", HIfModifiedSince),
	("if-match", HIfMatch),
	("if-none-match", HIfNoneMatch),
	("if-range", HIfRange),
	("if-unmodified-since", HIfUnmodifiedSince),
	("keep-alive", HKeepAlive),
	("last-modified", HLastModified),
	("location", HLocation),
	("max-forwards", HMaxForwards),
	("pics-label", HPICSLabel),
	("pragma", HPragma),
	("proxy-authenticate", HProxyAuthenticate),
	("proxy-authorization", HProxyAuthorization),
	("public", HPublic),
	("range", HRange),
	("referer", HReferer),
	("refresh", HRefresh),
	("retry-after", HRetryAfter),
	("server", HServer),
	("set-cookie", HSetCookie),
	("set-proxy", HSetProxy),
	("transfer-encoding", HTransferEncoding),
	("upgrade", HUpgrade),
	("user-agent", HUserAgent),
	("vary", HVary),
	("via", HVia),
	("warning", HWarning),
	("window-target", HWindowTarget),
	("www-authenticate", HWWWAuthenticate),
	("x-req-time", HXReqTime),
	("x-resp-time", HXRespTime),
	("x-url", HXUrl)
};

HTTP_Header: adt {
	startline: string;

	# following four fields only filled in if this is a response header
	protomajor: int;
	protominor: int;
	code: int;
	reason: string;
	iossl: int; # true for ssl 

	vals: array of string;

	new: fn() : ref HTTP_Header;
 	read: fn(h: self ref HTTP_Header, fd: ref sys->FD, sslx: ref SSL3->Context, buf: array of byte) : (string, int, int);
 	write: fn(h: self ref HTTP_Header, fd: ref sys->FD, sslx: ref SSL3->Context) : int;
	usessl: fn(h: self ref HTTP_Header);
	addval: fn(h: self ref HTTP_Header, key: int, val: string);
	getval: fn(h: self ref HTTP_Header, key: int) : string;
};

Nameval: adt {
	key: string;
	val: string;

	namevals: fn(s: string, sep: int) : list of Nameval;
	find: fn(l: list of Nameval, key: string) : (int, string);
};

mediatable: array of T->StringInt;

agent : string;
dbg := 0;
warn := 0;
sptab : con " \t";

init(cu: CharonUtils)
{
	CU = cu;
	sys = load Sys Sys->PATH;
	S = load String String->PATH;
	U = load Url Url->PATH; 
	C = cu->C;
	T = load StringIntTab StringIntTab->PATH;
#	D = load Date CU->loadpath(Date->PATH);
#	if (D == nil)
#		CU->raise(sys->sprint("EXInternal: can't load Date: %r"));
#	D->init(cu);
	ctype = C->ctype;
	# sslhs = nil;	# load on demand
	ssl3 = nil; # load on demand
	mediatable = CU->makestrinttab(CU->mnames);
	agent = (CU->config).agentname;
	dbg = int (CU->config).dbg['n'];
	warn = dbg || int (CU->config).dbg['w'];
}

connect(nc: ref Netconn, bs: ref ByteSource)
{
	if(nc.scheme == U->HTTPS)
		nc.tstate |= TSSL;
	if(config.httpminor == 0)
		nc.tstate |= THTTP_1_0;
	dialhost : string;
	dialport : string;
	if(config.httpproxy != nil && need_proxy(nc.host)) {
		nc.tstate |= TProxy;
		dialhost = config.httpproxy.host;
		if(config.httpproxy.port != "")
			nc.port = int config.httpproxy.port;
		dialport = config.httpproxy.port;
	}
	else {
		dialhost = nc.host;
		dialport = string nc.port;
	}
	addr := "tcp!" + dialhost + "!" + dialport;
	err := "";
	if(dbg)
		sys->print("http %d: dialing %s\n", nc.id, addr);
	rv: int;
	(rv, nc.conn) = sys->dial(addr, nil);
	if(rv < 0) {
		syserr := sys->sprint("%r");
		if(S->prefix("cs: dialup", syserr))
			err = syserr[4:];
		else if(S->prefix("cs: dns: no translation found", syserr))
			err = "unknown host";
		else
			err = "couldn't connect";
	}
	else {
		if(dbg)
			sys->print("http %d: connected\n", nc.id);
		if(nc.tstate&TSSL) {
			#if(sslhs == nil) {
			#	sslhs = load SSLHS SSLHS->PATH;
			#	if(sslhs == nil)
			#		err = sys->sprint("can't load SSLHS: %r");
			#	else
			#		sslhs->init(2);
			#}
			#if(err == "")
			#	(err, nc.conn) = sslhs->client(nc.conn.dfd, addr);
			if(nc.tstate&TProxy) # tunelling SSL through proxy
				err = tunnel_ssl(nc);
	 		vers := 0;
 			if(err == "") {
				if(ssl3 == nil) {
	 				ssl3 = load SSL3 SSL3->PATH;
 					if(ssl3 == nil)
 						err = "can't load SSL3 module";
					else
						err = ssl3->init();
				}
 				if(config.usessl == CU->NOSSL)
 					err = "ssl is configured off";
 				else if(config.usessl == CU->SSLV2)
 					vers = 2;
	 			else if(config.usessl == CU->SSLV3)
 					vers = 3;
 				else
 					vers = 23;
			}
 			if(err == "") {
 				nc.sslx = ssl3->Context.new();
 				if(config.devssl)
 					nc.sslx.use_devssl();
 				info := ref SSL3->Authinfo(ssl_suites, ssl_comprs, nil, 
 						0, nil, nil, nil);
 				(err, nc.vers) =  nc.sslx.client(nc.conn.dfd, addr, vers, info);
 			}
		}
	}
	if(err == "") {
		nc.connected = 1;
		nc.state = CU->NCgethdr;
	}
	else {
		if(dbg)
			sys->print("http %d: connection failed: %s\n", nc.id, err);
		bs.err = err;
		closeconn(nc);
	}
}

tunnel_ssl(nc: ref Netconn) : string
{
	httpvers: string;
	if(nc.state&THTTP_1_0)
		httpvers = "1.0";
	else
		httpvers = "1.1";
	req := "CONNECT " + nc.host + ":" + string nc.port + " HTTP/" + httpvers;
 	n := sys->fprint(nc.conn.dfd, "%s\r\n\r\n", req);
	if(n < 0)
		return sys->sprint("proxy: %r");
	buf := array [Sys->ATOMICIO] of byte;
	n = sys->read(nc.conn.dfd, buf, Sys->ATOMICIO);
	if(n < 0)
		return sys->sprint("proxy: %r");;
	resp := string buf[0:n];
	(m, s) := sys->tokenize(resp, " ");
	if(m < 2 || hd tl s != "200")
		return "proxy: " + resp;
	return "";
}

need_proxy(h: string) : int
{
	doml := config.noproxydoms;
	if(doml == nil)
		return 1; # all domains need proxy

	lh := len h;
	for(dom := hd doml; doml != nil; doml = tl doml) {
		ld := len dom;
		if(lh >= ld && h[lh-ld:] == dom)
			return 0; # domain is on the noproxy list
	}

	return 1;
}

writereq(nc: ref Netconn, bs: ref ByteSource)
{
	#
	# Prepare the request
	#
	req := bs.req;
	u := req.url;
	requ, httpvers: string;
	#if(nc.tstate&TProxy)
	if((nc.tstate&TProxy) && !(nc.tstate&TSSL))
		requ = u.tostring();
	else {
		requ = u.pstart + u.path;
		if(u.query != "")
			requ += "?" + u.query;
	}
	if(nc.tstate&THTTP_1_0)
		httpvers = "1.0";
	else
		httpvers = "1.1";
	reqhdr := HTTP_Header.new();
 	if(nc.tstate&TSSL)
 		reqhdr.usessl();
	reqhdr.startline = CU->hmeth[req.method] + " " +  requ + " HTTP/" + httpvers;
	if(u.port != "")
		reqhdr.addval(HHost, u.host+ ":" + u.port);
	else
		reqhdr.addval(HHost, u.host);
	reqhdr.addval(HUserAgent, agent);
#	if(cr != nil && (cr.status == CRRevalidate || cr.status == CRMustRevalidate)) {
#		if(cr.etag != "")
#			reqhdr.addval(HIfNoneMatch, cr.etag);
#		else
#			reqhdr.addval(HIfModifiedSince, D->dateconv(cr.notafter));
#	}
	if(req.auth != "")
		reqhdr.addval(HAuthorization, "Basic " + req.auth);
	if(req.method == CU->HPost) {
		reqhdr.addval(HContentLength, string (len req.body));
		reqhdr.addval(HContentType, "application/x-www-form-urlencoded");
	}
        if((CU->config).docookies > 0) {
                pagecookies := CU->getcookies(u.host, "/" + u.path);
                for (; pagecookies != nil; pagecookies = tl pagecookies) {
                        reqhdr.addval(HCookie, hd pagecookies);
                }
        }
	#
	# Issue the request
	#
	err := "";
	if(dbg > 1) {
		sys->print("http %d: writing request:\n", nc.id);
		reqhdr.write(sys->fildes(1), nil);
	}
	rv := reqhdr.write(nc.conn.dfd, nc.sslx);
	if(rv >= 0 && req.method == CU->HPost) {
		if(dbg > 1)
			sys->print("http %d: writing body:\n%s\n", nc.id, string req.body);
 		if((nc.tstate&TSSL) && nc.sslx != nil)
 			rv = nc.sslx.write(req.body, len req.body);
 		else
 			rv = sys->write(nc.conn.dfd, req.body, len req.body);
	}
	if(rv < 0) {
		err = "error writing to host";
	}
	if(err != "") {
		if(dbg)
			sys->print("http %d: error: %s", nc.id, err);
		bs.err = err;
		closeconn(nc);
	}
}


gethdr(nc: ref Netconn, bs: ref ByteSource)
{
	resph := HTTP_Header.new();
 	if(nc.tstate&TSSL)
 		resph.usessl();
	hbuf := array[8000] of byte;
 	(err, i, j) := resph.read(nc.conn.dfd, nc.sslx, hbuf);
	if(err != "") {
		if(!(nc.tstate&THTTP_1_0)) {
			# try switching to http 1.0
			if(dbg)
				sys->print("http %d: switching to HTTP/1.0\n", nc.id);
			nc.tstate |= THTTP_1_0;
		}
	}
	else {
		if(dbg) {
			sys->print("http %d: got response header:\n", nc.id);
			resph.write(sys->fildes(1), nil);
			sys->print("http %d: %d bytes remaining from read\n", nc.id, j-i);
		}
		if(resph.protomajor == 1) {
			if(!(nc.tstate&THTTP_1_0) && resph.protominor == 0) {
				nc.tstate |= THTTP_1_0;
				if(dbg)
					sys->print("http %d: switching to HTTP/1.0\n", nc.id);
			}
		}
		else if(warn)
			sys->print("warning: unimplemented major protocol %d.%d\n",
				resph.protomajor, resph.protominor);
		if(j > i)
			nc.tbuf = hbuf[i:j];
		else
			nc.tbuf = nil;
		bs.hdr = hdrconv(resph, bs.req.url, nc.tbuf);
		if(bs.hdr.length == 0 && (nc.tstate&THTTP_1_0))
			closeconn(nc);
	}
	if(err != "") {
		if(dbg)
			sys->print("http %d: error %s\n", nc.id, err);
		bs.err = err;
		closeconn(nc);
	}
}

getdata(nc: ref Netconn, bs: ref ByteSource)
{
	buf := bs.data;
	n := 0;
	if(nc.tbuf != nil) {
		# initial data from overread of header
		n = len nc.tbuf;
		if(len buf <= n) {
			if(warn && len buf < n)
				sys->print("more initial data than specified length\n");
			bs.data = nc.tbuf;
			nc.tbuf = nil;
		}
		else {
			buf[0:] = nc.tbuf;
			nc.tbuf = nil;
		}
	}
	if(n == 0) {
 		if((nc.tstate&TSSL) && nc.sslx != nil) 
 			n = nc.sslx.read(buf[bs.edata:], len buf - bs.edata);
 		else
 			n = sys->read(nc.conn.dfd, buf[bs.edata:], len buf - bs.edata);
	}
	if(dbg > 1)
		sys->print("http %d: read %d bytes\n", nc.id, n);
	if(n <= 0) {
		nc.conn.dfd = nil;
		nc.conn.cfd = nil;
		nc.conn.dir = "";
		nc.connected = 0;
		nc.sslx = nil;
		if(n < 0)
			bs.err = sys->sprint("%r");
	}
	else {
		bs.edata += n;
		if(bs.edata == len buf && bs.hdr.length != 100000000) {
			if(nc.tstate&THTTP_1_0) {
				closeconn(nc);
			}
		}
	}
	if(bs.err != "") {
		if(dbg)
			sys->print("http %d: error %s\n", nc.id, bs.err);
		closeconn(nc);
	}
}

hdrconv(hh: ref HTTP_Header, u: ref ParsedUrl, initcontent: array of byte) : ref Header
{
	hdr := Header.new();
	hdr.code = hh.code;
	hdr.actual = u;
	s := hh.getval(HContentBase);
	if(s != "")
		hdr.base = U->makeurl(s);
	else
		hdr.base = hdr.actual;
	s = hh.getval(HLocation);
	if(s != "")
		hdr.location = U->makeurl(s);
	s = hh.getval(HContentLength);
	if(s != "")
		hdr.length = int s;
	else
		hdr.length = -1;
	s = hh.getval(HContentType);
	if(s != "")
		setmtype(hdr, s);
	if(hdr.mtype == CU->UnknownType)
		hdr.setmediatype(u.path, initcontent);
	hdr.msg = hh.reason;
	hdr.refresh = hh.getval(HRefresh);
	hdr.chal = hh.getval(HWWWAuthenticate);
	s = hh.getval(HContentEncoding);
	if(s != "") {
		if(warn)
			sys->print("warning: unhandled content encoding: %s\n", s);
		# force "save as" dialog
		hdr.mtype = CU->UnknownType;
	}
	hdr.warn = hh.getval(HWarning);
	hdr.lastModified = hh.getval(HLastModified);
        if(dbg)
                sys->print("Got Cookie: %s\n", hh.getval(HSetCookie));
        if((CU->config).docookies > 0)
                CU->setcookie(hh.getval(HSetCookie), u.host, "/" + u.path);
	return hdr;
}

# Set hdr's media type and chset (if a text type).
# If can't set media type, leave it alone (caller will guess).
setmtype(hdr: ref CU->Header, s: string)
{
	(ty, parms) := S->splitl(S->tolower(s), ";");
	(fnd, val) := T->lookup(mediatable, trim(ty));
	if(fnd) {
		hdr.mtype = val;
		hdr.chset = CU->ISO_8859_1;
		if(len parms > 0 && val >= CU->TextPlain && val <= CU->TextSgml) {
			nvs := Nameval.namevals(parms[1:], ';');
			s: string;
			(fnd, s) = Nameval.find(nvs, "chset");
			if(fnd) {
				cty := CU->strlookup(CU->chsetnames, S->tolower(s));
				if(cty >= 0)
					hdr.chset = cty;
				else if(warn)
					sys->print("warning: unknown character set in %s\n", s);
			}
		}
	}
	else {
		if(warn)
			sys->print("warning: unknown media type in %s\n", s);
	}
}

# Remove leading and trailing whitespace from s.
trim(s: string) : string
{
	is := 0;
	ie := len s;
	while(is < ie) {
		if(ctype[s[is]] != C->W)
			break;
		is++;
	}
	if(is == ie)
		return "";
	while(ie > is) {
		if(ctype[s[ie-1]] != C->W)
			break;
		ie--;
	}
	if(is >= ie)
		return "";
	if(is == 0 && ie == len s)
		return s;
	return s[is:ie];
}

# If s is in double quotes, remove them
remquotes(s: string) : string
{
	n := len s;
	if(n >= 2 && s[0] == '"' && s[n-1] == '"')
		return s[1:n-1];
	return s;
}

HTTP_Header.new() : ref HTTP_Header
{
	return ref HTTP_Header("", 0, 0, 0, "", 0, array[NumHfields] of { * => "" });
}

HTTP_Header.usessl(h: self ref HTTP_Header)
{
 	h.iossl = 1;
}

HTTP_Header.addval(h: self ref HTTP_Header, key: int, val: string)
{
	oldv := h.vals[key];
	if(oldv != "") {
		# check that hdr type allows list of things
		case key {
			HAccept or HAcceptCharset or HAcceptEncoding
			or HAcceptLanguage or HAcceptRanges
			or HCacheControl or HConnection or HContentEncoding
			or HContentLanguage or HIfMatch or HIfNoneMatch
			or HPragma or HPublic or HUpgrade or HVia
			or HWarning or HWWWAuthenticate or HExpect =>
				val = oldv + ", " + val;
                        HCookie =>
                                val = oldv + "; " + val;
                        HSetCookie =>
                                if (oldv[len oldv - 1] == ';')
                                        oldv = oldv[0: len oldv - 1];
                                val = oldv + ", " + val;
			* =>
				if(warn)
					sys->print("warning: multiple %s headers not allowed\n",
						hdrnames[key]);
		}
	}
	h.vals[key] = val;
}

HTTP_Header.getval(h: self ref HTTP_Header, key: int) : string
{
	return h.vals[key];
}

# Read into supplied buf.
# Returns (ok, start of non-header bytes, end of non-header bytes)
# If bytes > 127 appear, assume Latin-1
#
# Header values added will always be trimmed (see trim() above).
HTTP_Header.read(h: self ref HTTP_Header, fd: ref sys->FD, sslx: ref SSL3->Context, buf: array of byte) : (string, int, int)
{
	i := 0;
	j := 0;
	aline : array of byte = nil;
	eof := 0;
 	if(h.iossl && sslx != nil) {
 		(aline, eof, i, j) = ssl_getline(sslx, buf, i, j);
 	}
 	else {
 		(aline, eof, i, j) = CU->getline(fd, buf, i, j);
 	}
	if(eof) {
		return ("header read got immediate eof", 0, 0);
	}
	h.startline = latin1tostring(aline);
	if(dbg > 1)
		sys->print("header read, startline=%s\n", h.startline);
	(vers, srest) := S->splitl(h.startline, " ");
	if(len srest > 0)
		srest = srest[1:];
	(scode, reason) := S->splitl(srest, " ");
	ok := 1;
	if(len vers >= 8 && vers[0:5] == "HTTP/") {
		(smaj, vrest) := S->splitl(vers[5:], ".");
		if(smaj == "" || len vrest <= 1)
			ok = 0;
		else {
			h.protomajor = int smaj;
			if(h.protomajor < 1)
				ok = 0;
			else
				h.protominor = int vrest[1:];
		}
		if(len scode != 3)
			ok = 0;
		else {
			h.code = int scode;
			if(h.code < 100)
				ok = 0;
		}
		if(len reason > 0)
			reason = reason[1:];
		h.reason = reason;
	}
	else
		ok = 0;
	if(!ok)
		return (sys->sprint("header read failed to parse start line '%s'\n", string aline), 0, 0);
	
	prevkey := -1;
	while(len aline > 0) {
 		if(h.iossl && sslx != nil) {
 			(aline, eof, i, j) = ssl_getline(sslx, buf, i, j);
 		}
 		else {
 			(aline, eof, i, j) = CU->getline(fd, buf, i, j);
 		}
		if(eof)
			return ("header doesn't end with blank line", 0, 0);
		if(len aline == 0)
			break;
		line := latin1tostring(aline);
		if(dbg > 1)
			sys->print("%s\n", line);
		if(ctype[line[0]] == C->W) {
			if(prevkey < 0) {
				if(warn)
					sys->print("warning: header continuation line at beginning: %s\n", line);
			}
			else
				h.vals[prevkey] = h.vals[prevkey] + " " + trim(line);
		}
		else {
			(nam, val) := S->splitl(line, ":");
			if(val == nil) {
				if(warn)
					sys->print("warning: header line has no colon: %s\n", line);
			}
			else {
				(fnd, key) := T->lookup(hdrtable, S->tolower(nam));
				if(!fnd) {
					if(warn)
						sys->print("warning: unknown header field: %s\n", line);
				}
				else {
					h.addval(key, trim(val[1:]));
					prevkey = key;
				}
			}
		}
	}
	return ("", i, j);
}

# Write in big hunks.  Convert to Latin1.
# Return last sys->write return value.
HTTP_Header.write(h: self ref HTTP_Header, fd: ref sys->FD, sslx: ref SSL3->Context) : int
{
	# Expect almost all responses will fit in this sized buf
	buf := array[sys->ATOMICIO] of byte;
	i := 0;
	buflen := len buf;
	need := len h.startline + 2 + 2;
	if(need > buflen) {
		buf = CU->realloc(buf, need-buflen);
		buflen = len buf;
	}
	i = copyaslatin1(buf, h.startline, i, 1);
	for(key := 0; key < NumHfields; key++) {
		val := h.vals[key];
		if(val != "") {
			# 4 extra for this line, 2 for final cr/lf
			need = len val + len hdrnames[key] + 4 + 2;
			if(i + need > buflen) {
				buf = CU->realloc(buf, i+need-buflen);
				buflen = len buf;
			}
			i = copyaslatin1(buf, hdrnames[key], i, 0);
			buf[i++] = byte ':';
			buf[i++] = byte ' ';
			# perhaps should break up really long lines,
			# but we aren't expecting any
			i = copyaslatin1(buf, val, i, 1);
		}
	}
	buf[i++] = byte '\r';
	buf[i++] = byte '\n';
	n := 0;
	for(k := 0; k < i; ) {
 		if(h.iossl && sslx != nil) {
 			n = sslx.write(buf[k:], i-k);
 		}
 		else {
 			n = sys->write(fd, buf[k:], i-k);
 		}
		if(n <= 0)
			break;
		k += n;
	}
	return n;
}

# For latin1tostring, so don't have to keep allocating it
lbuf := array[300] of byte;

# Assume we call this on 'lines', so they won't be too long
latin1tostring(a: array of byte) : string
{
	imax := len lbuf - 1;
	i := 0;
	n := len a;
	for(k := 0; k < n; k++) {
		b := a[k];
		if(b < byte 128)
			lbuf[i++] = b;
		else
			i += sys->char2byte(int b, lbuf, i);
		if(i >= imax) {
			if(imax > 1000) {
				if(warn)
					sys->print("warning: header line too long\n");
				break;
			}
			lbuf = CU->realloc(lbuf, 100);
			imax = len lbuf - 1;
		}
	}
	ans := string lbuf[0:i];
	return ans;
}

# Copy s into a[i:], converting to Latin1.
# Add cr/lf if addcrlf is true.
# Assume caller has checked that a has enough room.
copyaslatin1(a: array of byte, s: string, i: int, addcrlf: int) : int
{
	ns := len s;
	for(k := 0; k < ns; k++) {
		c := s[k];
		if(c < 256)
			a[i++] = byte c;
		else {
			if(warn)
				sys->print("warning: non-latin1 char in header ignored: '%c'\n", c);
		}
	}
	if(addcrlf) {
		a[i++] = byte '\r';
		a[i++] = byte '\n';
	}
	return i;
}

# Split a value (guaranteed trimmed) into sep-separated list of one of
# 	token
#	token = token
#	token = "quoted string"
# and put into list of Namevals (lowercase the first token)
Nameval.namevals(s: string, sep: int) : list of Nameval
{
	ans : list of Nameval = nil;
	n := len s;
	i := 0;
	while(i < n) {
		tok : string;
		(tok, i) = gettok(s, i, n);
		if(tok == "")
			break;
		tok = S->tolower(tok);
		val := "";
		while(i < n && ctype[s[i]] == C->W)
			i++;
		if(i == n || s[i] == sep)
			i++;
		else if(s[i] == '=') {
			while(i < n && ctype[s[i]] == C->W)
				i++;
			if(s[i] == '"')
				(val, i) = getqstring(s, i, n);
			else
				(val, i) = gettok(s, i, n);
		}
		else
			break;
		ans = Nameval(tok, val) :: ans;
	}
	if(warn && i < n)
		sys->print("warning: failed to parse namevals: '%s'\n", s);
	return ans;
}

gettok(s: string, i,n: int) : (string, int)
{
	while(i < n && ctype[s[i]] == C->W)
		i++;
	if(i == n)
		return ("", i);
	is := i;
	for(; i < n; i++) {
		c := s[i];
		ct := ctype[c];
		if(!(int (ct&(C->D|C->L|C->U|C->N|C->S))))
			if(int (ct&(C->W|C->C)) || S->in(c, "()<>@,;:\\\"/[]?={}"))
				break;
	}
	return (s[is:i], i);
}

# get quoted string; return it without quotes, and index after it
getqstring(s: string, i,n: int) : (string, int)
{
	while(i < n && ctype[s[i]] == C->W)
		i++;
	if(i == n || s[i] != '"')
		return ("", i);
	is := ++i;
	for(; i < n; i++) {
		c := s[i];
		if(c == '\\')
			i++;
		else if(c == '"')
			return (s[is:i], i+1);
	}
	if(warn)
		sys->print("warning: quoted string not closed: %s\n", s);
	return (s[is:i], i);
}

# Find value corresponding to key (should be lowercase)
# and return (1, value) if found or (0, "")
Nameval.find(l: list of Nameval, key: string) : (int, string)
{
	for(; l != nil; l = tl l)
		if((hd l).key == key)
			return (1, (hd l).val);
	return (0, "");
}

defaultport(scheme: int) : int
{
	if(scheme == U->HTTPS)
		return HTTPSD;
	else
		return HTTPD;
}

closeconn(nc: ref Netconn)
{
	nc.conn.dfd = nil;
	nc.conn.cfd = nil;
	nc.conn.dir = "";
	nc.connected = 0;
	nc.sslx = nil;
}

ssl_getline(sslx: ref SSL3->Context, buf: array of byte, bstart, bend: int)
	:(array of byte, int, int, int)
{
 	ans : array of byte = nil;
 	last : array of byte = nil;
 	eof := 0;
mainloop:
 	for(;;) {
 		for(i := bstart; i < bend; i++) {
 			if(buf[i] == byte '\n') {
 				k := i;
 				if(k > bstart && buf[k-1] == byte '\r')
 					k--;
 				last = buf[bstart:k];
 				bstart = i+1;
 				break mainloop;
 			}
 		}
 		if(bend > bstart)
 			ans = append(ans, buf[bstart:bend]);
 		last = nil;
 		bstart = 0;
 		bend = sslx.read(buf, len buf);
 		if(bend <= 0) {
 			eof = 1;
 			bend = 0;
 			break mainloop;
 		}
 	}
 	return (append(ans, last), eof, bstart, bend);
}
 
# Append copy of second array to first, return (possibly new)
# address of the concatenation.
append(a: array of byte, b: array of byte) : array of byte
{
 	if(b == nil)
 		return a;
 	na := len a;
 	nb := len b;
 	ans := realloc(a, nb);
 	ans[na:] = b;
 	return ans;
}
 
# Return copy of a, but incr bytes bigger
realloc(a: array of byte, incr: int) : array of byte
{
 	n := len a;
 	newa := array[n + incr] of byte;
 	if(a != nil)
 		newa[0:] = a;
 	return newa;
}

