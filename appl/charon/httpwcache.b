implement Transport;

include "common.m";
include "transport.m";
include "date.m";
include "sslhs.m";

D: Date;
sslhs: SSLHS;

# local copies from CU
sys: Sys;
U: Url;
	ParsedUrl: import U;
S: String;
C: Ctype;
T: StringIntTab;
CU: CharonUtils;
	Netconn, ByteSource, Header, config: import CU;

ctype: array of byte;	# local copy of C->ctype
stdout: ref sys->FD;

HTTPD:		con 80;		# Default IP port
HTTPSD:		con 443;	# Default IP port for HTTPS
Version:	con "2.0";	# Client ID

# HTTP/1.1 Spec says 5, but we've seen more than that in non-looping redirs
# MAXREDIR:	con 10;

# tstate bits
THTTP_1_0, TPersist, TProxy, TSSL, THaveCur, THaveHdr, TSrvClose, TRetry, TBad: con (1<<iota);

# Cache response status values
CROk, CRRevalidate, CRMustRevalidate, CRRefetch: con iota;

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

	vals: array of string;

	new: fn() : ref HTTP_Header;
	read: fn(h: self ref HTTP_Header, fd: ref sys->FD) : (int, array of byte);
	write: fn(h: self ref HTTP_Header, fd: ref sys->FD) : int;
	addval: fn(h: self ref HTTP_Header, key: int, val: string);
	getval: fn(h: self ref HTTP_Header, key: int) : string;
};

Nameval: adt {
	key: string;
	val: string;

	namevals: fn(s: string, sep: int) : list of Nameval;
	find: fn(l: list of Nameval, key: string) : (int, string);
};

CacheResponse: adt {
	status:	int;				# CROk, etc
	hh: 		ref HTTP_Header;	# details of cache entry
	etag:		string;			# revalidate if-none-match this
	notafter: 	int;			# revalidate not-modified-after this time
	msgprefix:	array of byte;		# first bytes of entity (after header)
	fd: 		ref sys->FD;		# where to read rest of msg
};

mediatable: array of T->StringInt;

agent := "Inferno/" + Version;
dbg := 0;
warn := 0;
sptab : con " \t";
oldhttp := 0;

init(cu: CharonUtils)
{
	CU = cu;
	sys = load Sys Sys->PATH;
	S = load String String->PATH;
	U = load Url Url->PATH;
	C = cu->C;
	T = load StringIntTab StringIntTab->PATH;
	D = load Date CU->loadpath(Date->PATH);
	if (D == nil)
		cu->raise(sys->sprint("EXInternal: can't load Date: %r"));
	D->init(cu);
	ctype = C->ctype;
	# sslhs = nil;	# load on demand
	stdout = sys->fildes(1);
	mediatable = CU->makestrinttab(CU->mnames);
}

getter(nc: ref Netconn)
{
	port := string nc.port;
	if(nc.port == 0) {
		if(nc.scheme == U->HTTPS) {
			port = string HTTPSD;
			nc.tstate |= TSSL;
		}
		else
			port = string HTTPD;
	}
	for(;;) {
		nc.tstate &= TSSL;
		dbg = int config.dbg['n'];
		warn = (int config.dbg['w']) || dbg;
		oldhttp = int config.dbg['o'];
		dialhost := nc.host;
		proxy := config.httpproxy;
		if(proxy != nil) {
			nc.tstate |= TProxy;
			dialhost = proxy.host;
		}
		trysome(nc, "tcp!" + dialhost + "!" + port);
		if(nc.tstate&TBad)
			break;
	}
}

# Make a fresh call to server (when first need it, which
# might be never if requests can come out of cache)
# and process requests until server closes its end.
trysome(nc: ref Netconn, addr: string)
{
	if(dbg > 1)
		sys->print("http %d: trysome (re)start, tstate=%ux\n", nc.id, nc.tstate);
	for(;;) {
		if(!(nc.tstate&THaveCur)) {
			nc.getcur();
#			nc.tstate |= THaveCur;
			if(dbg)
				sys->print("http %d: got request for %s\n", nc.id, nc.cur.req.url.tostring());
		}
		if(oldhttp)
			nc.tstate |= THTTP_1_0;
		getone(nc, addr);
nc.tstate |= TBad;
nc.status = CU->Saborted;
		if(nc.tstate&(TBad|TRetry))
			break;
	}
	nc.conn.dfd = nil;
	nc.conn.cfd = nil;
	nc.conn.dir = "";
}

getone(nc: ref Netconn, addr: string)
{
	if(dbg > 1)
		sys->print("http %d: getone %s\n", nc.id, addr);
	cur := nc.cur;
	req := cur.req;
	u := req.url;
	sendreq := 1;
	reqtime := 0;
	resptime := 0;
	# can cache queries, but rarely useful
	# could also try to screen out long "computer-generated" names
	cacheable := (req.method==CU->HGet) && u.query == "" && !config.nocache
			&& !(nc.tstate&TSSL) && req.auth == "";
	usecache := cacheable;
	cr : ref CacheResponse = nil;
	resph : ref HTTP_Header = nil;
	rprefix : array of byte;
	if(usecache) {
		cr = cacheread(nc);
		if(cr == nil)
			usecache = 0;
		else {
			if(cr.status == CROk)
				sendreq = 0;
		}
	}
	#
	# now sendreq will be true unless cache response is unconditionally ok.
	# cr will be non-nil if request was in cacheable and in cache (but it
	# may be past its freshness lifetime, and thus, require sendreq for
	# revalidation or replacement).
	#
	connected := (nc.conn.dfd != nil);
	if(sendreq) {
		if(!connected)
			connected = doconnect(nc, addr);
		if(connected) {
			reqtime = D->now();
			connected = dosendreq(nc, req, cr);
			if(connected) {
				#
				# Get initial part of response
				#
				resph = HTTP_Header.new();
				ok : int;
				(ok, rprefix) = resph.read(nc.conn.dfd);
				if(ok) {
					resptime = D->now();
					if(dbg) {
						sys->print("http %d: got response header:\n", nc.id);
						resph.write(stdout);
					}
					if(resph.protomajor == 1) {
						if(resph.protominor == 0)
							nc.tstate |= THTTP_1_0;
					}
					else if(warn)
						sys->print("warning: unimplemented major protocol %d.%d\n",
							resph.protomajor, resph.protominor);
				}
				else {
					connected = 0;
					if(nc.tstate&THTTP_1_0)
						nc.tstate |= TBad;
					else
						nc.tstate |= (TRetry|THTTP_1_0);
					resph = nil;
				}
			}
		}
	}
	#
	# Now resph will be non-nil if got response to request.
	# What to do with response depends on state of cr.
	#
	usecache = 0;
	if(cr != nil) {
		usestale := 0;
		case cr.status {
		CROk =>
			usecache = 1;
		CRRevalidate or CRMustRevalidate =>
			if(resph != nil) {
				if(resph.code == CU->HCNotModified) {
					if(dbg)
						sys->print("http %d: revalidation succeeded\n", nc.id);
					# TODO: cache writeback with new info
				}
				if(resph.code != CU->HCNotModified) {
					usecache = 1;
					if(dbg)
						sys->print("http %d: revalidation failed (code %d)\n", nc.id, resph.code);
				}
			}
			else
				usestale = (cr.status != CRMustRevalidate);
		CRRefetch =>
			usestale = (resph == nil);
		}
		if(usestale) {
			if(dbg)
				sys->print("http %d: using stale cache anyway: can't get current response\n", nc.id);
			usecache = 1;
			# TODO: get right syntax for this
			cr.hh.addval(HWarning, "Stale cache entry, but host unreachable");
		}
	}
	#
	# Now usecache is true if we're to fulfil request from cr,
	# otherwise we're to use resph.
	#
	firstbytes : array of byte = nil;	# message prefix
	fd : ref sys->FD = nil;			# where to read rest of response from
	hh : ref HTTP_Header = nil;		# resph or cr.hh
	cacheit := 0;				# should we write back answer to cache?
	cachefd: ref sys->FD = nil;		# fd to write to, if so
	cachename := "";			# name of cache file, if so
	if(usecache) {
		firstbytes = cr.msgprefix;
		fd = cr.fd;
		hh = cr.hh;
	}
	else if(resph != nil) {
		firstbytes = rprefix;
		fd = nc.conn.dfd;
		hh = resph;
		case hh.code {
		CU->HCOk or CU->HCMovedPerm =>
			# TODO check other cacheable responses
			cacheit = 1;
		}
	}
	else {
		if(dbg)
			sys->print("http %d: failure: can't use cache and can't talk to host\n", nc.id);
		nc.haveerr("can't connect to host");
		return;
	}
	s := hh.getval(HTransferEncoding);
	if(s != "") {
		if(dbg)
			sys->print("http %d: transfer encoding: %s\n", nc.id, s);
		# TODO: implement s == "chunked"
		sys->print("unimplemented chunked transfer encoding\n");
		connected = 0;
		nc.tstate |= TBad;
		nc.haveerr("unimplemented chunked transfer encoding\n");
		return;
	}
	#
	# Fill in hdr
	#
	hdr := Header.new();
	nc.hdr = hdr;
	hdr.code = hh.code;
	s = hh.getval(HContentBase);
	if(s != "")
		hdr.base = U->makeurl(s);
	else
		hdr.base = u;
	s = hh.getval(HContentLocation);
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
		hdr.setmediatype(u.path, firstbytes);
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
	#
	# Tell ByteSource about header
	#
	if(hdr.length == 0)
		nc.havehdrnodata();
	else
		nc.havehdr();
	if(cacheit)
		(cachefd, cachename) = cachewritehdr(nc, hh, u, reqtime, resptime);
	if(hdr.length != 0) {
		#
		# Data loop
		#
		tot := 0;
		need := hdr.length;
		if(need == -1)
			need = 100000000;
		if(firstbytes != nil) {
			nc.havedata(firstbytes);
			if(cachefd != nil)
				cachefd = cachewritedata(nc, cachefd, cachename, firstbytes);
			tot += len firstbytes;
		}
		while(tot < need) {
			buf := array[sys->ATOMICIO] of byte;
			k := need - tot;
			if(k > sys->ATOMICIO)
				k = sys->ATOMICIO;
			n := sys->read(fd, buf, k);
			if(dbg > 1)
				sys->print("http %d: read %d bytes\n", nc.id, n);
			if(n < 0) {
				if(cachename != "")
					cacheremove(cachename);
				nc.haveerr(sys->sprint("read error: %r\n"));
				return;
			}
			if(n == 0)
				break;
			d := buf[0:n];
			nc.havedata(d);
			if(cachefd != nil)
				cachefd = cachewritedata(nc, cachefd, cachename, d);
			tot += n;
		}
		if(hdr.length > 0 && tot != need) {
			if(warn)
				sys->print("warning: content length (%d) didn't match read length (%d)\n",
						hdr.length, tot);
		}
		nc.done();
	}
	if(dbg)
		sys->print("http %d: done %s\n", nc.id, u.tostring());
}

# Return 1 if connection succeeds, else return 0 and set state to TBad
doconnect(nc: ref Netconn, addr: string) : int
{
	connected := 0;
	if(dbg)
		sys->print("http %d: dialing %s\n", nc.id, addr);
	rv: int;
	(rv, nc.conn) = sys->dial(addr, nil);
	if(rv < 0) {
		nc.tstate |= TBad;
		if(dbg)
			sys->print("http %d: dial error %r\n", nc.id);
	}
	else {
		connected = 1;
		if(dbg)
			sys->print("http %d: connected\n", nc.id);
		if(nc.tstate&TSSL) {
			if(sslhs == nil) {
				sslhs = load SSLHS SSLHS->PATH;
				if(sslhs == nil)
					CU->raise(sys->sprint("EXInternal: can't load SSLHS: %r"));
				sslhs->init(2);
			}
			e: string;
			(e, nc.conn) = sslhs->client(nc.conn.dfd, addr);
			if(e != "") {
				if(dbg)
					sys->print("http %d: ssl handshake failed: %s\n", nc.id, e);
				connected = 0;
				nc.conn.dfd = nil;
				nc.conn.cfd = nil;
				nc.tstate |= TBad;
			}
			else {
				if(dbg)
					sys->print("http %d: ssl handshake complete\n", nc.id);
			}
		}
	}
	return connected;
}

# Return 1 if write succeeds, else return 0 and set state to TBad
dosendreq(nc: ref Netconn, req: ref CU->ReqInfo, cr: ref CacheResponse) : int
{
	#
	# Prepare the request
	#
	u := req.url;
	requ, httpvers: string;
	if(nc.tstate&TProxy)
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
	reqhdr.startline = CU->hmeth[req.method] + " " +  requ + " HTTP/" + httpvers;
	reqhdr.addval(HHost, u.host);
	reqhdr.addval(HUserAgent, agent);
	if(cr != nil && (cr.status == CRRevalidate || cr.status == CRMustRevalidate)) {
		if(cr.etag != "")
			reqhdr.addval(HIfNoneMatch, cr.etag);
		else
			reqhdr.addval(HIfModifiedSince, D->dateconv(cr.notafter));
	}
	if(req.auth != "")
		reqhdr.addval(HAuthorization, req.auth);
	if(req.method == CU->HPost)
		reqhdr.addval(HContentLength, string (len req.body));
	#
	# Issue the request
	#
	connected := 1;
	if(dbg > 1) {
		sys->print("http %d: writing request:\n", nc.id);
		reqhdr.write(stdout);
	}
	rv := reqhdr.write(nc.conn.dfd);
	if(rv >= 0 && req.method == CU->HPost)
		rv = sys->write(nc.conn.dfd, req.body, len req.body);
	if(rv < 0) {
		connected = 0;
		nc.tstate |= TBad;
		if(dbg)
			sys->print("http %d: write error %r\n", nc.id);
	}
	return connected;
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

# See if current request is in cache.
# If so, return a CacheResponse with conditions as to
# when it can be used (unconditionally, conditionally,
# or only if server can't be reached).
#
# Definitions:
#   age: time since response was sent or successfully validated
#   freshness lifetime: time between generation of response and expiration time
#   fresh: age <= freshness lifetime
#   stale: not fresh
#
# We are implementing a private (as opposed to shared) cache.
cacheread(nc: ref Netconn) : ref CacheResponse
{
	cur := nc.cur;
	u := cur.req.url;
	uname := u.tostring();
	hname := hashname(uname);
	fd := sys->open(hname, sys->OREAD);
	if(fd == nil)
		return nil;

	if(dbg)
		sys->print("http %d: found cache file %s for %s\n", nc.id, hname, uname);
	hh := HTTP_Header.new();
	(ok, msgprefix) := hh.read(fd);
	if(!ok) {
		cacheremove(hname);
		return nil;
	}

	ans := ref CacheResponse(CROk, hh, "", 0, msgprefix, fd);

	# see if cache validation is necessary
	validatetime := 0;

	# we wrote x-req-time, x-resp-time and x-url, so they should be there
	# (the times are in seconds-past-epoch)
	sreqtime := hh.getval(HXReqTime);
	sresptime := hh.getval(HXRespTime);
	url := hh.getval(HXUrl);
	if(sreqtime == "" || sresptime == "" || url == "") {
		# cache entry seems bad; perhaps error or interrupt during write
		cacheremove(hname);
		return nil;
	}
	if(url != uname)
		return nil;		# cache collision; should be rare

	sexpires := hh.getval(HExpires);
	sdate := hh.getval(HDate);
	slastmod := hh.getval(HLastModified);
	sage := hh.getval(HAge);
	scachectl := hh.getval(HCacheControl);
	cachel := Nameval.namevals(scachectl, ',');

	# calculate current_age, according to spec
	response_time := int sresptime;
	request_time := int sreqtime;
	now := D->now();
	age_value := 0;
	if(sage != "")
		age_value = int sage;
	date_value := D->date2sec(sdate);	# if 0, date parse failed
	if(date_value == 0) {
		# HTTP/1.1 requires date (unless no reliable clock at server)
		# but HTTP/1.0 says "assign one" if it is going into cache.
		# So use request_time as approx to date if necessary.
		if(warn && sdate != "")
			sys->print("warning: failed to parse date: %s\n", sdate);
		date_value = request_time;
	}

	apparent_age := response_time - date_value;
	if(apparent_age < 0)
		apparent_age = 0;
	corrected_received_age := apparent_age;
	if(corrected_received_age < age_value)
		corrected_received_age = age_value;
	response_delay := response_time - request_time;
	corrected_initial_age := corrected_received_age + response_delay;
	resident_time := now - response_time;
	current_age := corrected_initial_age + resident_time;

	# now calculate freshness lifetime
	freshness_lifetime := 0;
	heuristic_freshness := 0;
	expires_value := 0;
	if(sexpires != "") {
		expires_value = D->date2sec(sexpires);
		if(expires_value == 0 && warn)
			sys->print("warning: faild to parse date: %s\n", sexpires);
	}
	lastmod := 0;
	if(slastmod != "") {
		lastmod = D->date2sec(slastmod);
		if(warn && lastmod == 0)
			sys->print("warning: failed to parse date: %s\n", slastmod);
	}
	max_age_value := 0;
	(fndma, sma) := Nameval.find(cachel, "max-age");
	if(fndma) {
		max_age_value = int sma;
		freshness_lifetime = max_age_value;
	}
	else if(sexpires != "") {
		expires_value = D->date2sec(sexpires);
		freshness_lifetime = expires_value - date_value;
	}
	else if(lastmod > 0) {
		# guess 10% of time since last modified
		heuristic_freshness = 1;
		freshness_lifetime = (now - lastmod) / 10;
	}
	else {
		# just guess 1 hr
		heuristic_freshness = 1;
		freshness_lifetime = 3600;
	}
	staleness := current_age - freshness_lifetime;
	response_is_fresh := (staleness <= 0);
	status := CROk;
	(fndmr, nil) := Nameval.find(cachel, "must-revalidate");
	if(fndmr) {
		status = CRMustRevalidate;	# not allowed to provide this entry if server is unreachable
		if(dbg)
			sys->print("http %d: cache-control says must revalidate\n", nc.id);
	}
	if(status != CRMustRevalidate) {
		if(!response_is_fresh) {
			# only revalidate if staleness exceeds user tolerance
			if(staleness >= config.maxstale) {
				status = CRRevalidate;
				if(dbg)
					sys->print("http %d: must revalidate: stale by %d-%d=%d\n",
						nc.id, current_age, freshness_lifetime, staleness);
			}
		}
	}

	etag := "";
	notafter := 0;
	if(status == CROk) {
		if(dbg)
			sys->print("http %d: cache page still fresh\n", nc.id);
	}
	else {
		# choose validator
		etag = hh.getval(HETag);
		if(etag != "") {
			if(dbg)
				sys->print("http %d: revalidate etag %s\n", nc.id, etag);
		}
		else if(lastmod > 0) {
			notafter = lastmod;
			if(dbg)
				sys->print("http %d: revalidate lastmod no later than %s\n", nc.id, D->dateconv(lastmod));
		}
		else {
			# no validator; must refetch (but use cache if server down)
			status = CRRefetch;
			if(dbg)
				sys->print("http %d: must refetch (no validator)\n", nc.id);
		}
	}
	return ref CacheResponse(status, hh, etag, notafter, msgprefix, fd);
}

cachewritehdr(nc: ref Netconn, hh: ref HTTP_Header, u: ref Url->ParsedUrl, reqt, respt: int) : (ref sys->FD, string)
{
	s := hh.getval(HPragma);
	nvs : list of Nameval;
	fnd : int;
	if(s != "") {
		nvs = Nameval.namevals(s, ',');
		(fnd, nil) = Nameval.find(nvs, "no-cache");
		if(fnd)
			return (nil, "");
	}
	s = hh.getval(HCacheControl);
	if(s != "") {
		nvs = Nameval.namevals(s, ',');
		(fnd, nil) = Nameval.find(nvs, "no-cache");
		if(fnd)
			return (nil, "");
		(fnd, nil) = Nameval.find(nvs, "no-store");
		if(fnd)
			return (nil, "");
	}
	uname := u.tostring();
	hname := hashname(uname);
	hh.vals[HXReqTime] = string reqt;
	hh.vals[HXRespTime] = string respt;
	hh.vals[HXUrl] = uname;
	fd := sys->create(hname, sys->OWRITE, 8r600);
	if(fd != nil) {
		if(dbg)
			sys->print("http %d: cache writeback to %s\n", nc.id, hname);
		if(hh.write(fd) < 0) {
			cacheremove(hname);
			fd = nil;
		}
	}
	return (fd, hname);
}

cachewritedata(nc: ref Netconn, cachefd: ref sys->FD, hname: string, d: array of byte) : ref sys->FD
{
	if(sys->write(cachefd, d, len d) != len d) {
		if(dbg)
			sys->print("http %d: error writing cache block: %r\n",nc.id);
		cacheremove(hname);
	}
	return cachefd;
}

cacheremove(hname: string)
{
	if(dbg)
		sys->print("http: removing cache entry %s\n", hname);
	sys->remove(hname);
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

hashname(uname: string) : string
{
	hash := 0;
	prime: con 8388617;
	# start after "http:"
	for(i := 5; i < len uname; i++) {
		hash = hash % prime;
		hash = (hash << 7) + uname[i];
	}
	return sys->sprint("%s/cache/%.8ux", config.userdir, hash); 
}

HTTP_Header.new() : ref HTTP_Header
{
	return ref HTTP_Header("", 0, 0, 0, "", array[NumHfields] of { * => "" });
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

# Returns (ok, possible start of message-body)
# If bytes > 127 appear, assume Latin-1
#
# Header values added will always be trimmed (see trim() above).
HTTP_Header.read(h: self ref HTTP_Header, fd: ref sys->FD) : (int, array of byte)
{
	buf := array[sys->ATOMICIO] of byte;
	i := 0;
	j := 0;
	aline, rest : array of byte = nil;
	eof := 0;
	(aline, eof, i, j) = CU->getline(fd, buf, i, j);
	if(eof) {
		if(dbg)
			sys->print("http: header read got immediate eof\n");
		return (0, rest);
	}
	h.startline = latin1tostring(aline);
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
	if(!ok) {
		if(dbg)
			sys->print("http: header read failed to parse start line '%s'\n", string aline);
		return (0, rest);
	}
	
	prevkey := -1;
	while(len aline > 0) {
		(aline, eof, i, j) = CU->getline(fd, buf, i, j);
		if(eof)
			return (0, rest);
		if(len aline == 0)
			break;
		line := latin1tostring(aline);
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
	if(j > i)
		rest = buf[i:j];
	return (1, rest);
}

# Write in big hunks.  Convert to Latin1.
# Return last sys->write return value.
HTTP_Header.write(h: self ref HTTP_Header, fd: ref sys->FD) : int
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
		n = sys->write(fd, buf[k:], i-k);
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
