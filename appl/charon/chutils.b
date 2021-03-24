implement CharonUtils;

include "common.m";
include "transport.m";
#####################international
include "charon_gui.m";

	me: CharonUtils;
	sys: Sys;
	D: Draw;
	S: String;
	U: Url;
	T: StringIntTab;

Font : import D;
charon_gui: Charon_gui;
ParsedUrl: import U;

NCTimeout : con 100000;		# free NC slot after 100 seconds
UBufsize : con 40*1024;		# initial buffer size for unknown lengths
UEBufsize : con 1024;		# initial buffer size for unknown lengths, error responses

botchexception := "EXInternal: ByteSource protocol botch";
bytesourceid := 0;
crlf : con "\r\n";
ctype : array of byte;	# local ref to C->ctype[]
dbgproto : int;
dbg: int;
netconnid := 0;
netconns := array[10] of ref Netconn;
sptab : con " \t";
transports := array[Transport->Tmax] of Transport;
ngchan : chan of (int, ref ByteSource, ref Netconn, chan of ref ByteSource);
reqanschan : chan of ref ByteSource;

# must track HTTP methods in chutils.m
# (upper-case, since that's required in HTTP requests)
hmeth = array[] of { "GET", "POST" };

# following array must track media type def in chutils.m
# keep in alphabetical order
mnames = array[] of {
	"application/msword",
	"application/octet-stream",
	"application/pdf",
	"application/postscript",
	"application/rtf",
	"application/vnd.framemaker",
	"application/vnd.ms-excel",
	"application/vnd.ms-powerpoint",
	"application/x-unknown",
	"audio/32kadpcm",
	"audio/basic",
	"image/cgm",
	"image/g3fax",
	"image/gif",
	"image/ief",
	"image/jpeg",
	"image/png",
	"image/tiff",
	"image/x-bit",
	"image/x-bit2",
	"image/x-bitmulti",
	"image/x-inferno-bit",
	"image/x-xbitmap",
	"model/vrml",
	"multipart/digest",
	"multipart/mixed",
	"text/css",
	"text/enriched",
	"text/html",
	"text/javascript",
	"text/plain",
	"text/richtext",
	"text/sgml",
	"text/tab-separated-values",
	"text/xml",
	"video/mpeg",
	"video/quicktime"
};

# track Charsets in chutils.b
#(cf rfc1945 for other names, those we don't bother handling)
chsetnames = array[] of {
	"unknown", "us-ascii", "iso-8859-1", "unicode-1-1-utf-8", "unicode-1-1"
};

ncstatenames = array[] of {
	"free", "idle", "connect", "gethdr", "getdata",
	"done", "err"
};

hsnames = array[] of {
	"none", "information", "ok", "redirect", "request error", "server error"
};

hcphrase(code: int) : string
{
	ans : string;
	case code {
#####################international
        HCContinue =>           ans = charon_gui->iContinue;
        HCSwitchProto =>        ans = charon_gui->iSwProtocol;
        HCOk =>                 ans = charon_gui->iOk;
        HCCreated =>            ans = charon_gui->iCreated;
        HCAccepted =>           ans = charon_gui->iAccepted;
        HCOkNonAuthoritative => ans = charon_gui->iNonAuth;
        HCNoContent =>          ans = charon_gui->iNoContent;
        HCResetContent =>       ans = charon_gui->iResetContent;
        HCPartialContent =>     ans = charon_gui->iPartContent;
        HCMultipleChoices =>    ans = charon_gui->iMultiChoice;
        HCMovedPerm =>          ans = charon_gui->iMovedPerm;
        HCMovedTemp =>          ans = charon_gui->iMovedTemp;
        HCSeeOther =>           ans = charon_gui->iSeeOther;
        HCNotModified =>        ans = charon_gui->iNotModify;
        HCUseProxy =>           ans = charon_gui->iUseProxy;
        HCBadRequest =>         ans = charon_gui->iBadReq;
        HCUnauthorized =>       ans = charon_gui->iUnauth;
        HCPaymentRequired =>    ans = charon_gui->iPayRequired;
        HCForbidden =>          ans = charon_gui->iForbidden;
        HCNotFound =>           ans = charon_gui->iNotFound;
        HCMethodNotAllowed =>   ans = charon_gui->iNotAllowed;
        HCNotAcceptable =>      ans = charon_gui->iNotAccpt;
        HCProxyAuthRequired =>  ans = charon_gui->iProxyAuth;
        HCRequestTimeout =>     ans = charon_gui->iReqTimeout;
        HCConflict =>           ans = charon_gui->iConflict;
        HCGone =>               ans = charon_gui->iGone;
        HCLengthRequired =>     ans = charon_gui->iLenRequired;
        HCPreconditionFailed => ans = charon_gui->iPrecondFailed;
        HCRequestTooLarge =>    ans = charon_gui->iReqTooLarge;
        HCRequestURITooLarge => ans = charon_gui->iUriTooLarge;
        HCUnsupportedMedia =>   ans = charon_gui->iUnsuppMedia;
        HCRangeInvalid =>       ans = charon_gui->iRangeInvalid;
        HCExpectFailed =>       ans = charon_gui->iExpectFailed;
        HCServerError =>        ans = charon_gui->iServerError;
        HCNotImplemented =>     ans = charon_gui->iNotImplement;
        HCBadGateway =>         ans = charon_gui->iBadGateway;
        HCServiceUnavailable => ans = charon_gui->iServUnavail;
        HCGatewayTimeout =>     ans = charon_gui->iGatewayTimeout;
        HCVersionUnsupported => ans = charon_gui->iVerUnsupp;
        HCRedirectionFailed =>  ans = charon_gui->iRedirFailed;
        * =>                    ans = charon_gui->iUnknownCode;
	}
	return ans;
}

# This array should be kept sorted
fileexttable := array[] of { T->StringInt
	("ai", ApplPostscript),
	("au", AudioBasic),
# ("bit", ImageXBit),
	("bit", ImageXInfernoBit),
	("bit2", ImageXBit2),
	("bitm", ImageXBitmulti),
	("eps", ApplPostscript),
	("gif", ImageGif),
	("htm", TextHtml),
	("html", TextHtml),
	("jpe", ImageJpeg),
	("jpeg", ImageJpeg),
	("jpg", ImageJpeg),
	("pdf", ApplPdf),
	("ps", ApplPostscript),
	("shtml", TextHtml),
	("text", TextPlain),
	("tif", ImageTiff),
	("tiff", ImageTiff),
	("txt", TextPlain)
};

# argl is command line
# underwm is true if running under the window manager.
# Return string that is empty if all ok, else path of module
# that failed to load.
init(ch: Charon, c: CharonUtils, argl: list of string, underwm: int) : string
{
	me = c;
	sys = load Sys Sys->PATH;
	startres = ResourceState.cur();
	D = load Draw Draw->PATH;
	CH = ch;

	S = load String String->PATH;
	if(S == nil)
		return String->PATH;

	U = load Url Url->PATH;
	if(U == nil)
		return Url->PATH;

	T = load StringIntTab StringIntTab->PATH;
	if(T == nil)
		return StringIntTab->PATH;

	#####################international
	charon_gui = load Charon_gui Charon_gui->PATH;
	if(charon_gui==nil)
		raise(sys->sprint("EXinternal:couldn't load Charon_gui:%r"));
	charon_gui->init();

	# Now have all the modules needed to process command line
	# (hereafter can use our loadpath() function to substitute the
	# build directory version if dbg['u'] is set)

	setconfig(argl, underwm);

	if(underwm) {
		G = load Gui loadpath(Gui->GUIWMPATH);
		if(G == nil)
			return loadpath(Gui->GUIWMPATH);
	}
	else {
		G = load Gui loadpath(Gui->PATH);
		if(G == nil)
			return loadpath(Gui->PATH);
	}

	C = load Ctype loadpath(Ctype->PATH);
	if(C == nil)
		return loadpath(Ctype->PATH);

	E = load Events Events->PATH;
	if(E == nil)
		return loadpath(Events->PATH);

	if(config.doscripts) {
		J = load Script loadpath(Script->JSCRIPTPATH);
		if(J == nil)
			return loadpath(Script->JSCRIPTPATH);
	}

	LX = load Lex loadpath(Lex->PATH);
	if(LX == nil)
		return loadpath(Lex->PATH);

	B = load Build loadpath(Build->PATH);
	if(B == nil)
		return loadpath(Build->PATH);

	I = load Img loadpath(Img->PATH);
	if(I == nil)
		return loadpath(Img->PATH);

	L = load Layout loadpath(Layout->PATH);
	if(L == nil)
		return loadpath(Layout->PATH);


	# Intialize all modules after loading all, so that each
	# may cache pointers to the other modules
	# (G will be initialized in main charon routine, and L has to
	# be inited after that, because it needs G's display to allocate fonts)

	E->init();
	I->init(me);
	B->init(me);
	LX->init(me);
	if(J != nil)
		J->init(me);

	# preload some transports
	gettransport(Url->HTTP);
	gettransport(Url->FILE);

	if(config.showprogress)
		progresschan = chan of (int, int, int, string);
	imcache = ref ImageCache;
	ctype = C->ctype;
	dbgproto = int config.dbg['p'];
	dbg = int config.dbg['d'];
	ngchan = chan of (int, ref ByteSource, ref Netconn, chan of ref ByteSource);
	reqanschan = chan of ref ByteSource;
	return "";
}


# Make a ByteSource for given request, and make sure
# that it is on the queue of some Netconn.
# If don't have a transport for the request's scheme,
# the returned bs will have err set.
startreq(req: ref ReqInfo) : ref ByteSource
{
	bs := ref ByteSource(
			bytesourceid++,
			req,		# req
			nil,		# hdr
			nil,		# data
			0,		# edata
			"",		# err
			nil,		# net
			1,		# refgo
			1,		# refnc
			0,		# lim
			0		# seenhdr
		);

	if(config.showprogress)
		progresschan <-= (bs.id, Pstart, 0, req.url.tostring());
	ngchan <-= (NGstartreq, bs, nil, reqanschan);
	<-reqanschan;
	return bs;
}

# Wait for some ByteSource in current go generation to
# have a state change that goproc hasn't seen yet.
waitreq() : ref ByteSource
{
	ngchan <-= (NGwaitreq, nil, nil, reqanschan);
	bs := <-reqanschan;
	return bs;
}

# Notify netget that goproc is finished with bs.
freebs(bs: ref ByteSource)
{
	ngchan <-= (NGfreebs, bs, nil, reqanschan);
	<-reqanschan;
}

abortgo(gopgrp: int)
{
	if(int config.dbg['d'])
		sys->print("abort go\n");
	kill(gopgrp, 1);
	freegoresources();

	# renew the channels so that receives/sends by killed threads don't
	# muck things up
	ngchan = chan of (int, ref ByteSource, ref Netconn, chan of ref ByteSource);
	reqanschan = chan of ref ByteSource;
}

freegoresources()
{
	for(i := 0; i < len netconns; i++) {
		nc := netconns[i];
		nc.makefree();
	}
}

# This runs as a separate thread.
# It acts as a monitor to synchronize access to the Netconn data
# structures, as a dispatcher to start runnetconn's as needed to
# process work on Netconn queues, and as a notifier to let goproc
# know when any ByteSources have advanced their state.
netget()
{
	msg, n, i: int;
	bs : ref ByteSource;
	nc: ref Netconn;
	c, pendingc : chan of ref ByteSource;
	waitpending := 0;
	maxconn := config.nthreads;
	gncs := array[maxconn] of int;

	for(n = 0; n < len netconns; n++)
		netconns[n] = Netconn.new(n);
mainloop:
	for(;;) {
		(msg,bs,nc,c) = <- ngchan;
		case msg {
		NGstartreq =>
			# bs has req filled in, and is otherwise in its initial state.
			# Find a suitable Netconn and add bs to its queue of work,
			# then send nil along c to let goproc continue.

			if(dbgproto)
				sys->print("Startreq BS=%d for %s\n", bs.id, bs.req.url.tostring());
			scheme := bs.req.url.scheme;
			host := bs.req.url.host;
			(transport, err) := gettransport(scheme);
			if(err != "")
				bs.err = err;
			else {
				sport :=bs.req.url.port;
				if(sport == "")
					port := transport->defaultport(scheme);
				else
					port = int sport;
				i = 0;
				freen := -1;
				for(n = 0; n < len netconns && (i < maxconn || freen == -1); n++) {
					nc = netconns[n];
					if(nc.state == NCfree) {
						if(freen == -1)
							freen = n;
					}
					else if(nc.host == host
					   && nc.port == port && nc.scheme == scheme && i < maxconn) {
						gncs[i++] = n;
					}
				}
				if(i < maxconn) {
					# use a new netconn for this bs
					if(freen == -1) {
						freen = len netconns;
						newncs := array[freen+10] of ref Netconn;
						newncs[0:] = netconns;
						for(n = freen; n < freen+10; n++)
							newncs[n] = Netconn.new(n);
						netconns = newncs;
					}
					nc = netconns[freen];
					nc.host = host;
					nc.port = port;
					nc.scheme = scheme;
					nc.qlen = 0;
					nc.ngcur = 0;
					nc.gocur = 0;
					nc.reqsent = 0;
					nc.pipeline = 0;
					nc.connected = 0;
				}
				else {
					# use existing netconn with fewest outstanding requests
					nc = netconns[gncs[0]];
					if(maxconn > 1) {
						minqlen := nc.qlen - nc.gocur;
						for(i = 1; i < maxconn; i++) {
							x := netconns[gncs[i]];
							if(x.qlen-x.gocur < minqlen) {
								nc = x;
								minqlen = x.qlen-x.gocur;
							}
						}
					}
				}
				if(nc.qlen == len nc.queue) {
					nq := array[nc.qlen+10] of ref ByteSource;
					nq[0:] = nc.queue;
					nc.queue = nq;
				}
				nc.queue[nc.qlen++] = bs;
				bs.net = nc;
				if(dbgproto)
					sys->print("Chose NC=%d for BS %d, qlen=%d\n", nc.id, bs.id, nc.qlen);
				if(nc.state == NCfree || nc.state == NCidle) {
					if(nc.connected) {
						nc.state = NCgethdr;
						if(dbgproto)
							sys->print("NC %d: starting runnetconn in gethdr state\n", nc.id);
					}
					else {
						nc.state = NCconnect;
						if(dbgproto)
							sys->print("NC %d: starting runnetconn in connect state\n", nc.id);
					}
					spawn runnetconn(nc, transport);
				}
			}
			c <-= nil;

		NGwaitreq =>
			# goproc wants to be notified when some ByteSource
			# changes to a state that the goproc hasn't seen yet.
			# Send such a ByteSource along return channel c.

			if(dbgproto)
				sys->print("Waitreq\n");
			assert(!waitpending);
			for(n = 0; n < len netconns; n++) {
				nc = netconns[n];
				if(nc.gocur < nc.qlen) {
					bs = nc.queue[nc.gocur];
					assert(bs.refgo != 0);
					if(bs.err != "" || 
					   (bs.hdr != nil && !bs.seenhdr) ||
					   (bs.edata > bs.lim) || 
					   (nc.gocur < nc.ngcur)) {
						c <-= bs;
						continue mainloop;
					}
				}
			}
			if(dbgproto)
				sys->print("Waitpending\n");
			waitpending = 1;
			pendingc = c;
			
		NGfreebs =>
			# goproc is finished with bs.

			if(dbgproto)
				sys->print("Freebs BS=%d\n", bs.id);
			nc = bs.net;
			bs.refgo = 0;
			if(bs.refnc == 0) {
				bs.free();
				if(nc != nil)
					nc.queue[nc.gocur] = nil;
			}
			if(nc != nil) {
				# can be nil if no transport was found
				nc.gocur++;
				if(dbgproto)
					sys->print("NC %d: gocur=%d, ngcur=%d, qlen=%d\n", nc.id, nc.gocur, nc.ngcur, nc.qlen);
				if(nc.gocur == nc.qlen && nc.ngcur == nc.qlen) {
					if(!nc.connected)
						nc.makefree();
				}
			}
			c <-= nil;

		NGstatechg =>
			# Some runnetconn is telling us tht it changed the
			# state of nc.  Send a nil along c to let it continue.

			if(dbgproto)
				sys->print("Statechg NC=%d, state=%s\n",
					nc.id, ncstatenames[nc.state]);
			sendtopending : ref ByteSource = nil;
			if(waitpending && nc.gocur < nc.qlen) {
				bs = nc.queue[nc.gocur];
				if(dbgproto) {
					totlen := 0;
					if(bs.hdr != nil)
						totlen = bs.hdr.length;
					sys->print("BS %d: havehdr=%d seenhdr=%d edata=%d lim=%d, length=%d\n",
						bs.id, bs.hdr != nil, bs.seenhdr, bs.edata, bs.lim, totlen);
					if(bs.err != "")
						sys->print ("   err=%s\n", bs.err);
				}
				if(bs.refgo &&
				   (bs.err != "" ||
				    (bs.hdr != nil && !bs.seenhdr) ||
				    (bs.edata > bs.lim)))
					sendtopending = bs;
					# don't send here, prefer to let c get answer first */
			}
			if(nc.state == NCdone || nc.state == NCerr) {
				if(dbgproto)
					sys->print("NC %d: runnetconn finishing\n", nc.id);
				assert(nc.ngcur < nc.qlen);
				bs = nc.queue[nc.ngcur];
				bs.refnc = 0;
				if(bs.refgo == 0) {
					bs.free();
					nc.queue[nc.ngcur] = nil;
				}
				nc.ngcur++;
				if(dbgproto)
					sys->print("NC %d: ngcur=%d\n", nc.id, nc.ngcur);
				nc.state = NCidle;
				if(dbgproto)
					sys->print("NC %d: idle\n", nc.id);
				if(nc.ngcur < nc.qlen) {
					if(nc.connected) {
						nc.state = NCgethdr;
						if(dbgproto)
							sys->print("NC %d: starting runnetconn in gethdr state\n", nc.id);
					}
					else {
						nc.state = NCconnect;
						if(dbgproto)
							sys->print("NC %d: starting runnetconn in connect state\n", nc.id);
					}
					spawn runnetconn(nc, transports[nc.scheme]);
				}
				else if(nc.gocur == nc.qlen && !nc.connected)
					nc.makefree();
			}
			c <-= nil;
			if(sendtopending != nil) {
				if(dbgproto)
					sys->print("Send BS %d to pending waitreq\n", bs.id);
				pendingc <-= sendtopending;
				waitpending = 0;
			}

		}
	}
}

# A separate thread, to handle ngcur request of transport.
# If nc.gen ever goes < gen, we have aborted this go.
runnetconn(nc: ref Netconn, t: Transport)
{
	ach := chan of ref ByteSource;
	err := "";

	assert(nc.ngcur < nc.qlen);
	bs := nc.queue[nc.ngcur];

	# dummy loop, just for breaking out of in error cases
eloop:
	for(;;) {
		# Make the connection, if necessary
		if(nc.state == NCconnect) {
			t->connect(nc, bs);
			if(bs.err != "")
				break eloop;
			nc.state = NCgethdr;
		}
		assert(nc.state == NCgethdr && nc.connected);
		if(config.showprogress) {
			if(nc.scheme == Url->HTTPS)
				progresschan <-= (bs.id, Psslconnected, 0, "");
			else
				progresschan <-= (bs.id, Pconnected, 0, "");
		}

		# Write enough requests so that nc.ngcur header
		# will be next to be retrieved
		while(nc.reqsent < nc.qlen) {
			t->writereq(nc, nc.queue[nc.reqsent]);
			if(nc.queue[nc.reqsent].err != "") {
				if(nc.reqsent > nc.ngcur) {
					nc.queue[nc.reqsent].err = "";
					break;
				}
				else
					break eloop;
			}
			nc.reqsent++;
			if(nc.reqsent >= nc.ngcur+1 || !nc.pipeline)
				break;
		}
		assert(nc.reqsent > nc.ngcur);

		# Get the header
		t->gethdr(nc, bs);
		if(bs.err != "")
			break eloop;
		assert(bs.hdr != nil);
		if(config.showprogress)
			progresschan <-= (bs.id, Phavehdr, 0, "");

		totlen := bs.hdr.length;
		if(totlen > 0) {
			nc.state = NCgetdata;
			ngchan <-= (NGstatechg,nil,nc,ach);
			<- ach;
			bs.data = array[totlen] of byte;
			while(bs.edata < totlen) {
				t->getdata(nc, bs);
				if(bs.err != "")
					break eloop;
				if(config.showprogress)
					progresschan <-= (bs.id, Phavedata, 100*bs.edata/totlen, "");
				ngchan <-= (NGstatechg, nil, nc, ach);
				<- ach;
			}
		}
		else if(totlen == -1) {
			# Unknown length.
			# To simplify consumer semantics, we want bs.data to
			# not change underfoot, so for now, simply accumlate
			# everything before telling consumer about a state change.
			#
			# Report progress percentage based on current totlen (wrong
			# of course, but at least shows trend)
			if(bs.hdr.code == HCOk || bs.hdr.code ==HCOkNonAuthoritative)
				totlen = UBufsize;
			else
				totlen = UEBufsize;
			nc.state = NCgetdata;
			bs.hdr.length = 100000000;	# avoid BS free during following loop
			ngchan <-= (NGstatechg,nil,nc,ach);
			<- ach;
			bs.data = array[totlen] of byte;
			for(;;) {
				t->getdata(nc, bs);
				if(config.showprogress)
					progresschan <-= (bs.id, Phavedata, 100*bs.edata/totlen, "");
				if(bs.err != "") {
					# assume EOF
					bs.data = bs.data[0:bs.edata];
					bs.err = "";
					bs.hdr.length = bs.edata;
					nc.connected = 0;
					break;
				}
				if(bs.edata == totlen) {
					newbuf := array[totlen+totlen] of byte;
					newbuf[0:] = bs.data;
					bs.data = newbuf;
					totlen += totlen;
				}
			}
		}
		nc.state = NCdone;
		if(config.showprogress)
			progresschan <-= (bs.id, Phavedata, 100, "");
		break;
	}
	if(bs.err != "") {
		nc.state = NCerr;
		nc.connected = 0;
		if(config.showprogress)
			progresschan <-= (bs.id, Perr, 0, bs.err);
	}
	ngchan <-= (NGstatechg, nil, nc, ach);
	<- ach;
}

Netconn.new(id: int) : ref Netconn
{
	return ref Netconn(
			id,		# id
			"",		# host
			0,		# port
			0,		# scheme
			sys->Connection(nil, nil, ""),	# conn
			nil,		# ssl context
			0,		# undetermined ssl version
			NCfree,	# state
			array[10] of ref ByteSource,	# queue
			0,		# qlen
			0,0,0,	# gocur, ngcur, reqsent
			0,		# pipeline
			0,		# connected
			0,		# tstate
			nil,		# tbuf
			0		# idlestart
			);
}

Netconn.makefree(nc: self ref Netconn)
{
	if(dbgproto)
		sys->print("NC %d: free\n", nc.id);
	nc.state = NCfree;
	nc.host = "";
	nc.conn.dfd = nil;
	nc.conn.cfd = nil;
	nc.conn.dir = "";
	nc.qlen = 0;
	nc.gocur = 0;
	nc.ngcur = 0;
	nc.reqsent = 0;
	nc.pipeline = 0;
	nc.connected = 0;
	nc.tbuf = nil;
	for(i := 0; i < len nc.queue; i++)
		nc.queue[i] = nil;
}

ByteSource.free(bs: self ref ByteSource)
{
	if(dbgproto)
		sys->print("BS %d freed\n", bs.id);
	if(config.showprogress)
		if(bs.err == "")
			progresschan <-= (bs.id, Pdone, 100, "");
		else
			progresschan <-= (bs.id, Perr, 0, bs.err);
	bs.req = nil;
	bs.hdr = nil;
	bs.data = nil;
	bs.err = "";
	bs.net = nil;
}

# Return an ByteSource that is completely filled, from string s
ByteSource.stringsource(s: string) : ref ByteSource
{
	a := array of byte s;
	n := len a;
	hdr := ref Header(
			HCOk,		# code
			nil,			# actual
			nil,			# base
			nil,			# location
			n,			# length
			TextHtml, 	# mtype
			UTF_8,		# chset
			"",			# msg
			"",			# refresh
			"",			# chal
			"",			# warn
			""			# last-modified
		);
	bs := ref ByteSource(
			bytesourceid++,
			nil,		# req
			hdr,		# hdr
			a,		# data
			n,		# edata
			"",		# err
			nil,		# net
			1,		# refgo
			0,		# refnc
			0,		# lim
			1		# seenhdr
		);
	return bs;
}

MaskedImage.free(mim: self ref MaskedImage)
{
	mim.im = nil;
	mim.mask = nil;
}

CImage.new(src: ref U->ParsedUrl, lowsrc: ref U->ParsedUrl, width, height: int) : ref CImage
{
	return ref CImage(src, lowsrc, nil, strhash(src.host + "/" + src.path), width, height, nil, nil, 0);
}

# Return true if Cimages a and b represent the same image.
# As well as matching the src urls, the specified widths and heights must match too.
# (Widths and heights are specified if at least one of those is not zero.)
#
# BUG: the width/height matching code isn't right.  If one has width and height
# specified, and the other doesn't, should say "don't match", because the unspecified
# one should come in at its natural size.  But we overwrite the width and height fields
# when the actual size comes in, so we can't tell whether width and height are nonzero
# because they were specified or because they're their natural size.
CImage.match(a: self ref CImage, b: ref CImage) : int
{
	if(a.imhash == b.imhash) {
		if(urlequal(a.src, b.src)) {
			return (a.width == 0 || b.width == 0 || a.width == b.width) &&
				(a.height == 0 || b.height == 0 || a.height == b.height);
			# (above is not quite enough: should also check that don't have
			# situation where one has width set, not height, and the other has reverse,
			# but it is unusual for an image to have a spec in only one dimension anyway)
		}
	}
	return 0;
}

# Return approximate number of bytes in image memory used
# by ci.
CImage.bytes(ci: self ref CImage) : int
{
	tot := 0;
	for(i := 0; i < len ci.mims; i++) {
		mim := ci.mims[i];
		dim := mim.im;
		if(dim != nil)
			tot += ((dim.r.max.x-dim.r.min.x)>>(3-dim.ldepth)) *
					(dim.r.max.y-dim.r.min.y);
		dim = mim.mask;
		if(dim != nil)
			tot += ((dim.r.max.x-dim.r.min.x)>>(3-dim.ldepth)) *
					(dim.r.max.y-dim.r.min.y);
	}
	return tot;
}

# Call this after initial windows have been made,
# so that resetlimits() will exclude the images for those
# windows from the available memory.
ImageCache.init(ic: self ref ImageCache)
{
	ic.imhd = nil;
	ic.imtl = nil;
	ic.n = 0;
	ic.memused = 0;
	ic.resetlimits();
}

# Call resetlimits when amount of non-image-cache image
# memory might have changed significantly (e.g., on main window resize).
ImageCache.resetlimits(ic: self ref ImageCache)
{
	res := ResourceState.cur();
	avail := res.imagelim - (res.image-ic.memused);
		# (res.image-ic.memused) is used memory not in image cache
	avail = 8*avail/10;	# allow 20% slop for other applications, etc.
	ic.memlimit = config.imagecachemem;
	if(ic.memlimit > avail)
		ic.memlimit = avail;
	ic.nlimit = config.imagecachenum;
	ic.need(0);	# if resized, perhaps need to shed some images
}

# Look for a CImage matching ci, and if found, move it
# to the tail position (i.e., MRU)
ImageCache.look(ic: self ref ImageCache, ci: ref CImage) : ref CImage
{
	ans : ref CImage = nil;
	prev : ref CImage = nil;
	for(i := ic.imhd; i != nil; i = i.next) {
		if(i.match(ci)) {
			if(ic.imtl != i) {
				# remove from current place in cache chain
				# and put at tail
				if(prev != nil)
					prev.next = i.next;
				else
					ic.imhd = i.next;
				i.next = nil;
				ic.imtl.next = i;
				ic.imtl = i;
			}
			ans = i;
			break;
		}
		prev = i;
	}
	return ans;
}

# Call this to add ci as MRU of cache chain (should only call if
# it is known that a ci with same image isn't already there).
# Update ic.memused.
# Assume ic.need has been called to ensure that neither
# memlimit nor nlimit will be exceeded.
ImageCache.add(ic: self ref ImageCache, ci: ref CImage)
{
	ci.next = nil;
	if(ic.imhd == nil)
		ic.imhd = ci;
	else
		ic.imtl.next = ci;
	ic.imtl = ci;
	ic.memused += ci.bytes();
	ic.n++;
}

# Delete least-recently-used image in image cache
# and update memused and n.
ImageCache.deletelru(ic: self ref ImageCache)
{
	ci := ic.imhd;
	if(ci != nil) {
		ic.imhd = ci.next;
		if(ic.imhd == nil) {
			ic.imtl = nil;
			ic.memused = 0;
		}
		else
			ic.memused -= ci.bytes();
		for(i := 0; i < len ci.mims; i++)
			ci.mims[i].free();
		ci.mims = nil;
		ic.n--;
	}
}

ImageCache.clear(ic: self ref ImageCache)
{
	while(ic.imhd != nil)
		ic.deletelru();
}

# Call this just before allocating an Image that will used nbytes
# of image memory, to ensure that if the image were to be
# added to the image cache then memlimit and nlimit will be ok.
# LRU images will be shed if necessary.
# Return 0 if it will be impossible to make enough memory.
ImageCache.need(ic: self ref ImageCache, nbytes: int) : int
{
	while(ic.n >= ic.nlimit || ic.memused+nbytes > ic.memlimit) {
		if(ic.imhd == nil)
			return 0;
		ic.deletelru();
	}
	return 1;
}

strhash(s: string) : int
{
	prime: con 8388617;
	hash := 0;
	n := len s;
	for(i := 0; i < n; i++) {
		hash = hash % prime;
		hash = (hash << 7) + s[i];
	}
	return hash;
}

gettransport(scheme: int) : (Transport, string)
{
	err := "";
	transport : Transport = nil;
	if(scheme < 0 || scheme >= Transport->Tmax)
		err = "Unknown scheme";
	else {
		transport = transports[scheme];
		if(transport == nil) {
			tpath := "";
			case scheme {
				Url->HTTP => tpath = Transport->HTTPPATH;
				Url->HTTPS => tpath = Transport->HTTPPATH;
				Url->FTP => tpath = Transport->FTPPATH;
				Url->FILE => tpath = Transport->FILEPATH;
			}
			if(tpath == "")
				err = "Unsupported scheme";
			else {
				transport = load Transport loadpath(tpath);
				if(transport == nil)
					err = "Can't load transport";
				else {
					transport->init(me);
					transports[scheme] = transport;
				}
			}
		}
	}
	return (transport, err);
}

# Return new Header with default values for fields
Header.new() : ref Header
{
	return ref Header(
		HCOk,		# code
		nil,		# actual
		nil,		# base
		nil,		# location
		-1,		# length
		UnknownType,	# mtype
		UnknownCharset,	# chset
		"",		# msg
		"",		# refresh
		"",		# chal
		"",		# warn
		""		# last-modified
	);
}

jpmagic := array[] of {byte 16rFF, byte 16rD8, byte 16rFF, byte 16rE0,
		byte 0, byte 0, byte 'J', byte 'F', byte 'I', byte 'F', byte 0};

# Set the mtype (and possibly chset) fields of h based on (in order):
#	first bytes of file, if unambigous
#	file name extension
#	first bytes of file, even if unambigous (guess)
#	if all else fails, then leave as UnknownType.
# If it's a text type, also set the chset.
# (HTTP Transport will try to use Content-Type first, and call this if that
# doesn't work; other Transports will have to rely on this "guessing" function.)
Header.setmediatype(h: self ref Header, name: string, first: array of byte)
{
	# Look for key signatures at beginning of file (perhaps after whitespace)
	n := len first;
	mt := UnknownType;
	for(i := 0; i < n; i++)
		if(ctype[int first[i]] != C->W)
			break;
	if(n - i >= 6) {
		s := string first[i:i+6];
		case S->tolower(s) {
		"<html>" or "<head>" or "<title" =>
			h.mtype = TextHtml;
		"<!doct" =>
			if(n - i >= 14 && string first[i+6:i+14] == "ype html")
				mt = TextHtml;
		"gif87a" or "gif89a" =>
			if(i == 0)
				mt = ImageGif;
		"#defin" =>
			# perhaps should check more definitively...
			h.mtype = ImageXXBitmap;
		}
#		if(h.mtype == UnknownType) {
			if(i == 0 && n >= len jpmagic) {
				for(; i<len jpmagic; i++)
					if(jpmagic[i]>byte 0 && first[i]!=jpmagic[i])
						break;
				if(i == len jpmagic)
					mt = ImageJpeg;
			}
#		}
	}

	if(mt == UnknownType) {
		# Try file name extension
		(nil, file) := S->splitr(name, "/");
		if(file != "") {
			(f, ext) := S->splitr(file, ".");
			if(f != "" && ext != "") {
				(fnd, val) := T->lookup(fileexttable, S->tolower(ext));
				if(fnd)
					mt = val;
			}
		}
	}

	if(mt == UnknownType || (mt >= TextHtml && mt <= TextSgml)) {
		# Try file statistics
		nctl := 0;
		ntophalf := 0;
		if(n > 1024)
			n = 1024;
		for(i = 0; i < n; i++) {
			b := first[i];
			if(ctype[int b]==C->C)
				nctl++;
			if(b >= byte 16r80)
				ntophalf++;
		}
		if(nctl < n/100 && ntophalf < n/10) {
			if(mt == UnknownType)
				mt = TextPlain;
			if(ntophalf == 0)
				h.chset = US_Ascii;
			else
				h.chset = ISO_8859_1;
		}
		else if(mt == UnknownType) {
			if(n == 0) {
				mt = TextHtml;	# just guess most likely...
				h.chset = ISO_8859_1;
			}
			else
				mt = ApplOctets;
		}
		else
			h.chset = ISO_8859_1;
	}
	h.mtype = mt;
}

Header.print(h: self ref Header)
{
	mtype := "?";
	if(h.mtype >= 0 && h.mtype < len mnames)
		mtype = mnames[h.mtype];
	chset := "?";
	if(h.chset >= 0 && h.chset < len chsetnames)
		chset = chsetnames[h.chset];
	sys->print("code=%d (%s) length=%d mtype=%s chset=%s\n",
		h.code, hcphrase(h.code), h.length, mtype, chset);
	if(h.base != nil)
		sys->print("  base=%s\n", h.base.tostring());
	if(h.location != nil)
		sys->print("  location=%s\n", h.location.tostring());
	if(h.refresh != "")
		sys->print("  refresh=%s\n", h.refresh);
	if(h.chal != "")
		sys->print("  chal=%s\n", h.chal);
	if(h.warn != "")
		sys->print("  warn=%s\n", h.warn);
}


# Locks where only two threads compete for lock.
# Use Dekker's alg to spin lock on real lock, then, if lock
# is busy, wait on a channel.
Lock.new() : ref Lock
{
	l := ref Lock(byte 0, byte 0, byte 0, byte 0, 0, chan of byte);
	return l;
}

# Caller should be 0 or 1, according to thread doing locking.
Lock.lock(l: self ref Lock, caller: int)
{
	case caller {
	0 =>
		l.enter0 = byte 1;
		while(l.enter1 != byte 0) {
			if(l.turn == byte 1) {
				l.enter0 = byte 0;
				while(l.turn == byte 1)
					sys->sleep(0);
				l.enter0 = byte 1;
			}
		}
	1 =>
		l.enter1 = byte 1;
		while(l.enter0 != byte 0) {
			if(l.turn == byte 0) {
				l.enter1 = byte 0;
				while(l.turn == byte 0)
					sys->sleep(0);
				l.enter1 = byte 1;
			}
		}
	}
	if(l.lockval == byte 0)
		l.lockval = byte 1;
	else
		l.waiting = 1;
	case caller {
	0 =>
		l.enter0 = byte 0;
		l.turn = byte 1;
	1 =>
		l.enter1 = byte 0;
		l.turn = byte 0;
	}
	if(l.waiting)
		<- l.waitch;
}

Lock.unlock(l: self ref Lock, caller: int)
{
	wake := 0;
	case caller {
	0 =>
		l.enter0 = byte 1;
		while(l.enter1 != byte 0) {
			if(l.turn == byte 1) {
				l.enter0 = byte 0;
				while(l.turn == byte 1)
					sys->sleep(0);
				l.enter0 = byte 1;
			}
		}
	1 =>
		l.enter1 = byte 1;
		while(l.enter0 != byte 0) {
			if(l.turn == byte 0) {
				l.enter1 = byte 0;
				while(l.turn == byte 0)
					sys->sleep(0);
				l.enter1 = byte 1;
			}
		}
	}
	if(l.waiting) {
		l.waiting = 0;
		wake = 1;
	}
	else
		l.lockval = byte 0;
	case caller {
	0 =>
		l.enter0 = byte 0;
		l.turn = byte 1;
	1 =>
		l.enter1 = byte 0;
		l.turn = byte 0;
	}
	if(wake)
		l.waitch <-= byte 0;
}

Lock.free(l: self ref Lock)
{
	l.waitch = nil;
}

mfd : ref sys->FD = nil;
ResourceState.cur() : ResourceState
{
	ms := sys->millisec();
	main := 0;
	mainlim := 0;
	heap := 0;
	heaplim := 0;
	image := 0;
	imagelim := 0;
	if(mfd == nil)
		mfd = sys->open("/dev/memory", sys->OREAD);
	if (mfd == nil)
		raise(sys->sprint("can't open /dev/memory: %r"));

	sys->seek(mfd, 0, Sys->SEEKSTART);

	buf := array[400] of byte;
	n := sys->read(mfd, buf, len buf);
	if (n <= 0)
		raise(sys->sprint("can't read /dev/memory: %r"));

	(nil, l) := sys->tokenize(string buf[0:n], "\n");
	# p->cursize, p->maxsize, p->hw, p->nalloc, p->nfree, p->nbrk, poolmax(p), p->name)
	while(l != nil) {
		s := hd l;
		cur_size := int s[0:12];				
		max_size := int s[12:24];
		case s[7*12:] {
		"main" =>
			main = cur_size;
			mainlim = max_size;
		"heap" =>
			heap = cur_size;
			heaplim = max_size;
		"image" =>
			image = cur_size;
			imagelim = max_size;
		}
		l = tl l;
	}

	return ResourceState(ms, main, mainlim, heap, heaplim, image, imagelim);
}

ResourceState.since(rnew: self ResourceState, rold: ResourceState) : ResourceState
{
	return (rnew.ms - rold.ms, 
		rnew.main - rold.main, 
		rnew.heaplim,
		rnew.heap - rold.heap,
		rnew.heaplim, 
		rnew.image - rold.image, 
		rnew.imagelim);
}

ResourceState.print(r: self ResourceState, msg: string)
{
	sys->print("%s:\n\ttime: %d.%#.3ds; memory: main %dk, mainlim %dk, heap %dk, heaplim %dk, image %dk, imagelim %dk\n",
				msg, r.ms/1000, r.ms % 1000, r.main / 1024, r.mainlim / 1024,
				r.heap / 1024, r.heaplim / 1024, r.image / 1024, r.imagelim / 1024);
}

# Decide what to do based on Header and whether this is
# for the main entity or not, and the number of redirections-so-far.
# Return tuple contains:
#	(use, error, challenge, redir)
# and action to do is:
#	If use==1, use the entity else drain its byte source.
#	If error != nil, mesg was put in progress bar
#	If challenge != nil, get auth info and make new request with auth
#	Else if redir != nil, make a new request with redir for url
#
# (if challenge or redir is non-nil, use will be 0)
hdraction(bs: ref ByteSource, ismain: int, nredirs: int) : (int, string, string, ref U->ParsedUrl)
{
	use := 1;
	error := "";
	challenge := "";
	redir : ref U->ParsedUrl = nil;

	h := bs.hdr;
	assert(h != nil);
	bs.seenhdr = 1;
	code := h.code;
	case code/100 {
	HSOk =>
		if(code != HCOk)
			error = "unexpected code: " + hcphrase(code);
	HSRedirect =>
		if(h.location != nil) {
			redir = h.location;
			# spec says url should be absolute, but some
			# sites give relative ones
			if(redir.scheme == U->NOSCHEME)
				redir.makeabsolute(h.base);
			if(dbg)
				sys->print("redirect %s to %s\n", h.actual.tostring(), redir.tostring());
			if(nredirs >= Maxredir) {
				redir = nil;
				error = "probable redirect loop";
			}
			else
				use = 0;
		}
	HSError =>
		if(code == HCUnauthorized && h.chal != "") {
			challenge = h.chal;
			use = 0;
		}
		else {
			error = hcphrase(code);
			use = ismain;
		}
	HSServererr =>
		error = hcphrase(code);
		use = ismain;
	* =>
		error = "unexpected code: " + string code;
		use = 0;

	}
	if(error != "" && config.showprogress)
		progresschan <-= (bs.id, Perr, 0, error);
	return (use, error, challenge, redir);
}

# Use event when only care about time stamps on events
event(s: string, data: int)
{
	sys->print("%s: %d %d\n", s, sys->millisec()-startres.ms, data);
}

kill(pid: int, dogroup: int)
{
	msg : array of byte;
	if(dogroup)
		msg = array of byte "killgrp";
	else
		msg = array of byte "kill";
	ctl := sys->open("#p/" + string pid + "/ctl", sys->OWRITE);
	if(ctl != nil)
		sys->write(ctl, msg, len msg);
}

# Read a line up to and including cr/lf (be tolerant and allowing missing cr).
# Look first in buf[bstart:bend], and if that isn't sufficient to get whole line,
# refill buf from fd as needed.
# Return values:
#	array of byte: the line, not including cr/lf
#	eof, true if there was no line to get or a read error
#	bstart', bend': new valid portion of buf (after cr/lf).
getline(fd: ref sys->FD, buf: array of byte, bstart, bend: int) :
		(array of byte, int, int, int)
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
		bend = sys->read(fd, buf, len buf);
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

# Look (linearly) through a for s; return its index if found, else -1.
strlookup(a: array of string, s: string) : int
{
	n := len a;
	for(i := 0; i < n; i++)
		if(s == a[i])
			return i;
	return -1;
}

# Set up config global to defaults, then try to read user-specifiic
# config data from /usr/<username>/charon/config, then try to
# override from command line arguments.
# Use underwm to decide default sizes.
setconfig(argl: list of string, underwm: int)
{
	# Defaults, in absence of any other information
	config.userdir = "";
	config.srcdir = "/appl/cmd/charon";
	config.starturl = U->makeurl("file:/services/webget/start.html");
	config.homeurl = config.starturl;
	config.change_homeurl = 1;
	config.helpurl = U->makeurl("file:/services/webget/help.html");
	config.custbkurl = "/services/config/bookmarks.html";
	config.dualbkurl = "/services/config/dualdisplay.html";
	config.httpproxy = nil;
	config.noproxydoms = nil;
	config.buttons = "help,resize,hide,exit";
	if(underwm) {
		config.defaultwidth = 630;
		config.defaultheight = 450;
	}
	else {
		config.defaultwidth = 800;
		config.defaultheight = 800;
	}
	config.x = 0;
	config.y = 0;
	config.nocache = 0;
	config.maxstale = 0;
	config.imagelvl = ImgFull;
	config.imagecachenum = 60;
	config.imagecachemem = 100000000;	# 100Meg, will get lowered later
	config.docookies = 0;
	config.doscripts = 0;
	config.saveauthinfo = 0;
	config.showprogress = 1;
	config.usecci = 0;
	config.httpminor = 0;
	config.agentname = "Charon/2.0 (Inferno)";
	config.nthreads = 1;
	config.dbgfile = "";
	config.dbg = array[128] of { * => byte 0 };
	
	# Reading default config file
	readconf("/services/config/charon");

	# Try reading user config file
	user := "";
	fd := sys->open("/dev/user", sys->OREAD);
	if(fd != nil) {
		b := array[40] of byte;
		n := sys->read(fd, b, len b);
		if(n > 0)
			user = string b[0:n];
	}
	if(user != "") {
		config.userdir = "/usr/" + user + "/charon";
		readconf(config.userdir + "/config");
	}

	if(argl == nil)
		return;
	# Try command line arguments
	# All should be 'key=val' or '-key' or '-key val', except last which can be url to start
	for(l := tl argl; l != nil; l = tl l) {
		s := hd l;
		if(s == "")
			continue;
		if(S->prefix("-x ", s)) {
				# hack to handle tk args "-x val -y val", that all come in one arg from wm
				(m, ll) := sys->tokenize(s, " ");
				ll = tl ll;
				if(ll != nil) {
					setopt("x", hd ll);
					ll = tl ll;
					if(ll != nil && hd ll == "-y" && hd tl ll != nil)
						setopt("y", hd tl ll);
				}
		}
		else if(S->prefix("-u ", s)) {
				# handle redirect which may contain '=' such as from cgi-bin
				(m, ll) := sys->tokenize(s, " "); 
				if(m != 2)
					sys->print("couldn't use more than 2 string in redirect format\n");
				ll = tl ll;
				if(ll != nil)
					setopt("starturl", hd ll);
		}
		else if(s[0] == '-') {
			a := s[1:];
			b := "";
			if(tl l != nil) {
				b = hd tl l;
				if(S->prefix("-", b))
					b = "";
				else
					l = tl l;
			}
			if(!setopt(a, b))
				sys->print("couldn't set option from arg '%s'\n", s);
		}
		else {
			(a, b) := S->splitl(s, "=");
			if(b != "") {
				b = b[1:];
				if(!setopt(a, b))
					sys->print("couldn't set option from arg '%s'\n", s);
			}
			else if(tl l == nil)
				if(!setopt("starturl", s))
					sys->print("couldn't set starturl from arg '%s'\n", s);
		}
	}
}

readconf(fname: string)
{
	cfgio := sys->open(fname, sys->OREAD);
	if(cfgio != nil) {
		buf := array[sys->ATOMICIO] of byte;
		i := 0;
		j := 0;
		aline : array of byte;
		eof := 0;
		for(;;) {
			(aline, eof, i, j) = getline(cfgio, buf, i, j);
			if(eof)
				break;
			line := string aline;
			if(len line == 0 || line[0]=='#')
				continue;
			(key, val) := S->splitl(line, " \t=");
			if(key != "") {
				val = S->take(S->drop(val, " \t="), "^#\r\n");
				if(!setopt(key, val))
					sys->print("couldn't set option from line '%s'\n", line);
			}
		}
	}
}

# Set config option named 'key' to val, returning 1 if OK
setopt(key: string, val: string) : int
{
	ok := 1;
	if(val == "none")
		val = "";
	v := int val;
	case key {
	"userdir" =>
		config.userdir = val;
	"srcdir" =>
		config.srcdir = val;
	"starturl" =>
		if(val != "")
			config.starturl = makeabsurl(val);
		else
			ok = 0;
	"homeurl" =>
		if(val != "")
			if(config.change_homeurl) {
				config.homeurl = makeabsurl(val);
				# order dependent
				config.starturl = config.homeurl;
			}
		else
			ok = 0;
	"change_homeurl" =>
		config.change_homeurl = v;
	"helpurl" =>
		if(val != "")
			config.helpurl = makeabsurl(val);
		else
			ok = 0;
	"httpproxy" =>
		if(val != "")
			config.httpproxy = makeabsurl(val);
		else
			config.httpproxy = nil;
	"noproxy" or "noproxydoms" =>
		(nil, config.noproxydoms) = sys->tokenize(val, ";, \t");
 	"usessl" =>
 		if(val == "v2")
 			config.usessl |= SSLV2;
 		if(val == "v3")
 			config.usessl |= SSLV3;
 	"devssl" =>
 		if(v == 0)
 			config.devssl = 0;
 		else
 			config.devssl = 1;
	"buttons" =>
		config.buttons = S->tolower(val);
	"defaultwidth" or "width" =>
		if(v > 200)
			config.defaultwidth = v;
		else
			ok = 0;
	"defaultheight" or "height" =>
		if(v > 100)
			config.defaultheight = v;
		else
			ok = 0;
	"x" =>
		config.x = v;
	"y" =>
		config.y = v;
	"nocache" =>
		config.nocache = v;
	"maxstale" =>
		config.maxstale = v;
	"imagelvl" =>
		config.imagelvl = v;
	"imagecachenum" =>
		config.imagecachenum = v;
	"imagecachemem" =>
		config.imagecachemem = v;
	"docookies" =>
		config.docookies = v;
	"doscripts" =>
		config.doscripts = v;
	"saveauthinfo" =>
		config.saveauthinfo = v;
	"showprogress" =>
		config.showprogress = v;
	"usecci" =>
		config.usecci = v;
	"http" =>
		if(val == "1.1")
			config.httpminor = 1;
		else
			config.httpminor = 0;
	"agentname" =>
		config.agentname = val;
	"nthreads" =>
		config.nthreads = v;
	"dbgfile" =>
		config.dbgfile = val;
	"dbg" =>
		for(i := 0; i < len val; i++) {
			c := val[i];
			if(c < len config.dbg)
				config.dbg[c]++;
			else {
				ok = 0;
				break;
			}
		}
	* =>
		ok = 0;
	}
	return ok;
}

saveconfig(fname: string): int
{
	buf := array [Sys->ATOMICIO] of byte;
	fd := sys->create(fname, Sys->OWRITE, 8r600);
	if(fd == nil)
		return -1;

	nbyte := savealine(fd, buf, "# Charon user configuration\n", 0);
	nbyte = savealine(fd, buf, "userdir=" + config.userdir + "\n", nbyte);
	nbyte = savealine(fd, buf, "srcdir=" + config.srcdir +"\n", nbyte);
	if(config.change_homeurl){ 
		nbyte = savealine(fd, buf, "starturl=" + config.starturl.tostring() + "\n", nbyte);
 		nbyte = savealine(fd, buf, "homeurl=" + config.homeurl.tostring() + "\n", nbyte); 	
	}
	if(config.httpproxy != nil)
		nbyte = savealine(fd, buf, "httpproxy=" + config.httpproxy.tostring() + "\n", nbyte); 	
 	if(config.usessl == SSLV2)
 		nbyte = savealine(fd, buf, "usessl=v2\n", nbyte);
 	else if(config.usessl == SSLV3)
 		nbyte = savealine(fd, buf, "usessl=v3\n", nbyte);
 	else if(config.usessl == SSLV23)
 		nbyte = savealine(fd, buf, "usessl=v2\n" + "usessl=v3\n", nbyte);
	if(config.devssl == 0)
		nbyte = savealine(fd, buf, "devssl=0\n", nbyte);
	else
		nbyte = savealine(fd, buf, "devssl=1\n", nbyte);
	if(config.noproxydoms != nil) {
		doms := "";
		doml := config.noproxydoms;
		while(doml != nil) {
			doms += hd doml + ",";
			doml = tl doml;
		}
		nbyte = savealine(fd, buf, "noproxy=" + doms + "\n", nbyte);
	}
	nbyte = savealine(fd, buf, "defaultwidth=" + string config.defaultwidth + "\n", nbyte); 	 	
	nbyte = savealine(fd, buf, "defaultheight=" + string config.defaultheight + "\n", nbyte); 	
	nbyte = savealine(fd, buf, "x=" + string config.x + "\n", nbyte);
	nbyte = savealine(fd, buf, "y=" + string config.y + "\n", nbyte);
	nbyte = savealine(fd, buf, "nocache=" + string config.nocache + "\n", nbyte);
	nbyte = savealine(fd, buf, "maxstale=" + string config.maxstale + "\n", nbyte);
	nbyte = savealine(fd, buf, "imagelvl=" + string config.imagelvl + "\n", nbyte);
	nbyte = savealine(fd, buf, "imagecachenum=" + string config.imagecachenum + "\n", nbyte);
	nbyte = savealine(fd, buf, "imagecachemem=" + string config.imagecachemem + "\n", nbyte);
	nbyte = savealine(fd, buf, "docookies=" + string config.docookies + "\n", nbyte);
	nbyte = savealine(fd, buf, "saveauthinfo=" + string config.saveauthinfo + "\n", nbyte);
	nbyte = savealine(fd, buf, "showprogress=" + string config.showprogress + "\n", nbyte);
	nbyte = savealine(fd, buf, "usecci=" + string config.usecci + "\n", nbyte);
	nbyte = savealine(fd, buf, "http=" + "1." + string config.httpminor + "\n", nbyte);
	nbyte = savealine(fd, buf, "agentname=" + string config.agentname + "\n", nbyte);
	nbyte = savealine(fd, buf, "nthreads=" + string config.nthreads + "\n", nbyte);
	#for(i := 0; i < len config.dbg; i++)
		#nbyte = savealine(fd, buf, "dbg=" + string config.dbg[i] + "\n", nbyte);

	if(nbyte > 0)
		sys->write(fd, buf, nbyte);

	return 0; 
}

savealine(fd: ref Sys->FD, buf: array of byte, s: string, n: int): int
{
	if(Sys->ATOMICIO < n + len s) {
		sys->write(fd, buf, n);
		buf[0:] = array of byte s;
		return len s;
	}
	buf[n:] = array of byte s;
	return n + len s;
}

# Make a StringInt table out of a, mapping each string
# to its index.  Check that entries are in alphabetical order.
makestrinttab(a: array of string) : array of T->StringInt
{
	n := len a;
	ans := array[n] of T->StringInt;
	for(i := 0; i < n; i++) {
		ans[i].key = a[i];
		ans[i].val = i;
		if(i > 0 && a[i] < a[i-1])
			raise("EXInternal: table out of alphabetical order");
	}
	return ans;
}

# Should really move into Url module.
# Don't include fragment in test, since we are testing if the
# pointed to docs are the same, not places within docs.
urlequal(a, b: ref U->ParsedUrl) : int
{
	return a.scheme == b.scheme
		&& a.host == b.host
		&& a.port == b.port
		&& a.user == b.user
		&& a.passwd == b.passwd
		&& a.pstart == b.pstart
		&& a.path == b.path
		&& a.query == b.query;
}

# U->makeurl, but add http:// if not an absolute path already
makeabsurl(s: string) : ref ParsedUrl
{
	if(s != "") {
		(nil,xr) := S->splitstrl(s, ":/");
		if(xr == "")
			s = "http://" + s;
		u := U->makeurl(s);
		return u;
	}
	return nil;
}

# Return place to load from, given installed-path name.
# (If config.dbg['u'] is set, change directory to config.srcdir.)
loadpath(s: string) : string
{
	if(config.dbg['u'] == byte 0)
		return s;
	(nil, f) := S->splitr(s, "/");
	return config.srcdir + "/" + f;
}

color_tab := array[] of { T->StringInt
	("aqua",	16r00FFFF),
	("black",	Black),
	("blue",	Blue),
	("fuchsia",	16rFF00FF),
	("gray",	16r808080),
	("green",	16r008000),
	("lime",	16r00FF00),
	("maroon",	16r800000),
	("navy",	Navy),
	("olive",	16r808000),
	("purple",	16r800080),
	("red",	Red),
	("silver",	16rC0C0C0),
	("teal",	16r008080),
	("white",	White),
	("yellow",	16rFFFF00)
};
# Convert HTML color spec to RGB value, returning dflt if can't.
# Argument is supposed to be a valid HTML color, or "".
# Return the RGB value of the color, using dflt if s
# is "" or an invalid color.
color(s: string, dflt: int) : int
{
	if(s == "")
		return dflt;
	s = S->tolower(s);
	c := s[0];
	if(c < C->NCTYPE && ctype[c] == C->L) {
		(fnd, v) := T->lookup(color_tab, s);
		if(fnd)
			return v;
	}
	if(s[0] == '#')
		s = s[1:];
	(v, rest) := S->toint(s, 16);
	if(rest == "")
		return v;
	# s was invalid, so choose a valid one
	return dflt;
}

raise(e: string)
{
	if(len e > Sys->ERRLEN)
		e = e[:Sys->ERRLEN];
	sys->raise(e);
}

assert(i: int)
{
	if(!i) {
		raise("EXInternal: assertion failed");
#		sys->print("assertion failed\n");
#		s := hmeth[-1];
	}
}

setcookie(s: string, host: string, path: string)
{
	left, right, next, newnext : string;
	newcookie : Cookie;
	templist : list of Cookie;
	isnew := 1;
	(defpath, nil) := S->splitr(path, "/");

	if (s != nil) {
		(nil, toklist) := sys->tokenize(s, ";");

		for (; toklist != nil; toklist = tl toklist) {
			next = "";
			newnext = "";
			if (isnew) {
				newcookie = Cookie (nil, nil, host, nil, nil, defpath, 0);
				isnew = 0;
			}
			if (dbg) sys->print("TOKEN=%sEND\n", hd toklist);
			(left, right) = S->splitl(hd toklist, "=");
			left = B->trim_white(left);
			left = S->tolower(left);
			#(nil, right) = S->splitr(right, "=");
			if (len right > 1)
				right = right[1:];
			case left {
				"comment" =>
					newcookie.comment = right;
				"domain" =>
					#(newcookie.domain, next) = S->splitl(right, ",");
					#sys->print("<next=%s!\n", next);
					#sys->print("domain=%s!\n", newcookie.domain);
					#if (len next > 2) {
					#	next = next[1:];
					#	sys->print(">next=%s!\n", next);
					#	toklist = next :: toklist;
					#	isnew = 1;
					#}

                                       newcookie.domain = right;
                                        (right, next) = S->splitl(right, ",");
                                        if (next != "") {
                                        	(next, newnext) = S->splitl(next, ",");
                                        	newcookie.domain = right;
					}

                                        if (len newnext > 2) {
                                                newnext = newnext[2:];
                                                toklist = tl toklist;
                                                toklist = nil :: newnext :: toklist;
                                                isnew = 1;
                                        }


				"expires" =>
			#		newcookie.expires = right;
					(right, next) = S->splitl(right, ",");
					next = next[1:];
					(next, newnext) = S->splitl(next, ",");
					newcookie.expires = right + "," + next;

                                        if (len newnext > 2) {
						newnext = newnext[2:];
						toklist = tl toklist;
                                                toklist = nil :: newnext :: toklist;
                                                isnew = 1;
					}
				"max-age" =>
					newcookie.maxage = right;
				"path" =>
					newcookie.path = B->trim_white(right);
                                        (right, next) = S->splitl(right, ",");
                                        if (next != "") {
                                                (next, newnext) = S->splitl(next, ",");
                                                newcookie.path = right;
                                        }

                                        if (len newnext > 2) {
                                                newnext = newnext[2:];
                                                toklist = tl toklist;
                                                toklist = nil :: newnext :: toklist;
                                                isnew = 1;
                                        }


				"secure" =>
					newcookie.secure = 1;
				* =>
					if (newcookie.value != nil && dbg)
						sys->print("warning: cookie already has a value\n");
					newcookie.value = hd toklist;
			}

		if (isnew) {
                for (; cookielist != nil; cookielist = tl cookielist) {
                        if (len (hd cookielist).path >= len newcookie.path)
                                if (((hd cookielist).path == newcookie.path) && ((hd cookielist).domain == newcookie.
domain)) {
                                        (tempval, nil) := S->splitl((hd cookielist).value, "=");
                                        (cookval, nil) := S->splitl(newcookie.value, "=");
                                        if (tempval == cookval) {
                                                # trash the old one
                                                cookielist = tl cookielist;
                                                cookielist = newcookie :: cookielist;
						if (dbg)
                                                	sys->print("AN OLD COOKIE IS BEING REPLACED BY A NEW ONE! -- ISNEW\n");
                                                break;
                                        }
                                }
                        templist = hd cookielist :: templist;
                }

                if (cookielist == nil) {
                        cookielist = newcookie :: cookielist;
			if (dbg) {
				sys->print("<<<<<<<<<<< ISNEW >>>>>>>>>>>>>\n");
                        	sys->print("******* NEW COOKIE ADDED ******\n");
                        	sys->print("value = %s\n", newcookie.value);
                        	sys->print("comment = %s\n", newcookie.comment);
                        	sys->print("domain = %s\n", newcookie.domain);
                        	sys->print("expires = %s\n", newcookie.expires);
                        	sys->print("max-age = %s\n", newcookie.maxage);
                        	sys->print("path = %s\n", newcookie.path);
                        	sys->print("secure = %d\n", newcookie.secure);
                        	if (newcookie.secure)
                               		sys->print("cookie is secure!\n");
                        	sys->print("*******************************\n");
			} #close if dbg
		} #close if cookielist
                # Reverse the temp list and add it back to the original
                for (; templist != nil; templist = tl templist) {
                        cookielist = hd templist :: cookielist;
    		}

		}
		}

		for (; cookielist != nil; cookielist = tl cookielist) {
			if (len (hd cookielist).path >= len newcookie.path)
				if (((hd cookielist).path == newcookie.path) && ((hd cookielist).domain == newcookie.domain)) {
					(tempval, nil) := S->splitl((hd cookielist).value, "=");
					(cookval, nil) := S->splitl(newcookie.value, "=");
					if (tempval == cookval) {
						# trash the old one
						cookielist = tl cookielist;
						cookielist = newcookie :: cookielist;
						if (dbg)
							sys->print("AN OLD COOKIE IS BEING REPLACED BY A NEW ONE! -- REGULAR\n");
						break;
					}
				}
			templist = hd cookielist :: templist;
		}
		
		if (cookielist == nil) {
			cookielist = newcookie :: cookielist;	
			if (dbg) {
				sys->print(">>>>>>>>>> REGULAR <<<<<<<<<<<\n");
				sys->print("******* NEW COOKIE ADDED ******\n");
				sys->print("value = %s\n", newcookie.value);
				sys->print("comment = %s\n", newcookie.comment);
				sys->print("domain = %s\n", newcookie.domain);
				sys->print("expires = %s\n", newcookie.expires);
				sys->print("max-age = %s\n", newcookie.maxage);
				sys->print("path = %s\n", newcookie.path);
				sys->print("secure = %d\n", newcookie.secure);
				if (newcookie.secure)
					sys->print("cookie is secure!\n");
				sys->print("*******************************\n");
			} #close if dbg
		} #close if (cookielist)
		# Reverse the temp list and add it back to the original
		for (; templist != nil; templist = tl templist) {
			cookielist = hd templist :: cookielist;
		}
	}

}


getcookies(host: string, path: string) : list of string
{
	ans, newans : list of string;
	# Should this be passed as an argument (the for loop is destructive)
	localcookies := cookielist;

	for (; localcookies != nil; localcookies = tl localcookies) {
		domain := (hd localcookies).domain;
		if (host == domain) {
			if (S->prefix((hd localcookies).path, path)) {
				ans = (hd localcookies).value :: ans;
			}
		}
		else if (len host > len domain && host[len host - len domain:] == domain) {
                        if (S->prefix((hd localcookies).path, path)) {
                                ans = (hd localcookies).value :: ans;
                        }
		}
	}

	if (dbg)
		sys->print("Sending %d cookies !\n", len ans);

	# This list was reversed during creation, put it back in order
	for (; ans != nil; ans = tl ans) {
		newans = hd ans :: newans;
	}

	return newans;

}

