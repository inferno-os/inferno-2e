CharonUtils: module
{
	PATH: con "/dis/charon/chutils.dis";

	# Modules for everyone to share
	C: Ctype;
	E: Events;
	G: Gui;
	L: Layout;
	I: Img;
	B: Build;
	LX: Lex;
	J: Script;
	CH: Charon;

	# HTTP methods
	HGet, HPost : con iota;
	hmeth: array of string;

	# Media types (must track mnames in chutils.b)
	ApplMsword, ApplOctets, ApplPdf, ApplPostscript, ApplRtf,
	ApplFramemaker, ApplMsexcel, ApplMspowerpoint, UnknownType,

	Audio32kadpcm, AudioBasic,

	ImageCgm, ImageG3fax, ImageGif, ImageIef, ImageJpeg, ImagePng, ImageTiff,
	ImageXBit, ImageXBit2, ImageXBitmulti, ImageXInfernoBit, ImageXXBitmap,

	ModelVrml,

	MultiDigest, MultiMixed,

	TextCss, TextEnriched, TextHtml, TextJavascript, TextPlain, TextRichtext,
	TextSgml, TextTabSeparatedValues, TextXml,

	VideoMpeg, VideoQuicktime : con iota;

	mnames: array of string;

	# Charsets  (must track chsetnames in chutils.b)
	UnknownCharset, US_Ascii, ISO_8859_1, UTF_8, Unicode: con iota;

	chsetnames: array of string;

	# Netconn states
	NCfree, NCidle, NCconnect, NCgethdr, NCgetdata,
	NCdone, NCerr : con iota;

	ncstatenames: array of string;

	# Netcomm synch protocol values
	NGstartreq, NGwaitreq, NGstatechg, NGfreebs : con iota;

	# Progress states
	Punused, Pstart, Pconnected, Psslconnected, Phavehdr, Phavedata, Pdone, Perr, Paborted : con iota;

	# Colors
	White: con 16rFFFFFF;
	Black: con 16r000000;
	Grey: con 16rdddddd;
	DarkGrey: con 16r9d9d9d;
	LightGrey: con 16rededed;
	Blue: con 16r0000CC;
	Navy: con 16r000080;
	Red: con 16rFF0000;
	DarkRed: con 16r9d0000;

	# Header major status values (code/100)
	HSNone, HSInformation, HSOk, HSRedirect, HSError, HSServererr : con iota;
	hsnames: array of string;

	# Individual status code values (HTTP, but use for other transports too)
	HCContinue:		con 100;
	HCSwitchProto:		con 101;
	HCOk:			con 200;
	HCCreated:		con 201;
	HCAccepted:		con 202;
	HCOkNonAuthoritative:	con 203;
	HCNoContent:		con 204;
	HCResetContent:		con 205;
	HCPartialContent:	con 206;
	HCMultipleChoices:	con 300;
	HCMovedPerm:		con 301;
	HCMovedTemp:		con 302;
	HCSeeOther:		con 303;
	HCNotModified:		con 304;
	HCUseProxy:		con 305;
	HCBadRequest:		con 400;
	HCUnauthorized:		con 401;
	HCPaymentRequired:	con 402;
	HCForbidden:		con 403;
	HCNotFound:		con 404;
	HCMethodNotAllowed:	con 405;
	HCNotAcceptable:	con 406;
	HCProxyAuthRequired:	con 407;
	HCRequestTimeout:	con 408;
	HCConflict:		con 409;
	HCGone:			con 410;
	HCLengthRequired:	con 411;
	HCPreconditionFailed:	con 412;
	HCRequestTooLarge:	con 413;
	HCRequestURITooLarge:	con 414;
	HCUnsupportedMedia:	con 415;
	HCRangeInvalid:		con 416;
	HCExpectFailed:		con 419;
	HCServerError:		con 500;
	HCNotImplemented:	con 501;
	HCBadGateway:		con 502;
	HCServiceUnavailable:	con 503;
	HCGatewayTimeout:	con 504;
	HCVersionUnsupported:	con 505;
	HCRedirectionFailed:	con 506;

	# Max number of redirections tolerated
	Maxredir : con 10;

	# Image Level config options
	ImgNone, ImgNoAnim, ImgProgressive, ImgFull: con iota;

 	# SSL connection version
 	NOSSL, SSLV2, SSLV3, SSLV23: con iota << 1;

	# User Configuration Information (Options)
	# Debug option letters:
	# 'd' -> Basic operation info (navigation, etc.)
	# 'e' -> Events (timing of progress through get/layout/image conversion)
	# 'h' -> Build layout items from lex tokens
	# 'i' -> Image conversion
	# 'l' -> Layout
	# 'n' -> transport (Network access)
	# 'o' -> always use old http (http/1.0)
	# 'p' -> synch Protocol between ByteSource/Netconn
	# 'r' -> Resource usage
	# 's' -> Scripts
	# 't' -> Table layout
	# 'u' -> use Uninstalled dis modules
	# 'w' -> Warn about recoverable problems in retrieved pages
	# 'x -> lex Html tokens
	Config: adt
	{
		userdir:	string;		# where to find bookmarks, cache, etc.
		srcdir:		string;		# where to find charon src (for debugging)
		starturl:	ref Url->ParsedUrl;# never nil (could be last of command args)
		change_homeurl:	int;
		homeurl:	ref Url->ParsedUrl;# never nil
		helpurl:	ref Url->ParsedUrl;
 		usessl:		int; # use ssl version 2, 3 or both
 		devssl:		int; # use devssl
		custbkurl:	string; # where are customized bookmarks-never nil
		dualbkurl:	string; # where is the dual bookmark page-never nil
		httpproxy:	ref Url->ParsedUrl;# nil, if no proxy
		noproxydoms:	list of string; # domains that don't require proxy
		buttons:	string;		# customized buttons
		defaultwidth:	int;		# of entire browser
		defaultheight:	int;		# of entire browser
		x:			int;		# initial x position for browser
		y:			int;		# initial y position for browser
		nocache:	int;		# true if shouldn't retrieve from or store to
		maxstale:	int;		# allow cache hit even if exceed expiration by maxstale
		imagelvl:	int;		# ImgNone, etc.
		imagecachenum: int;	# imcache.nlimit
		imagecachemem: int;	# imcache.memlimit
		docookies:	int;		# allow cookie storage/sending?
		doscripts:		int;		# allow scripts to execute?
		saveauthinfo:	int;		# save auth info in file?
		showprogress:	int;		# show progress area?
		usecci:		int;		# allow external (CCI) control
		httpminor:	int;		# use HTTP 1.httpminor
		agentname:	string;	# what to send in HTTP header
		nthreads:		int;		# number of simultaneous gets allowed
		dbgfile:		string;	# file to write debug messages to
		dbg:		array of byte;	# ascii letters for different debugging kinds
	};

	# Information for fulfilling HTTP request
	ReqInfo : adt
	{
		url:	ref Url->ParsedUrl;	# should be absolute
		method:	int;			# HGet or HPost
		body:	array of byte;		# used for HPost
		auth:	string;			# optional auth info
		target:	string;			# target frame name
	};

	MaskedImage: adt {
		im:		ref Draw->Image;		# the image
		mask:	ref Draw->Image;		# if non-nil, a mask for the image
		delay:	int;			# if animated, delay in millisec before next frame
		more:	int;			# true if more frames follow
		bgcolor:	int;			# if not -1, restore to this (RGB) color before next frame
		origin:	Draw->Point;		# origin of im relative to first frame of an animation

		free: fn(mim: self ref MaskedImage);
	};

	# Charon Image info.
	# If this is an animated image then len mims > 1
	CImage: adt
	{
		src:	ref Url->ParsedUrl;	# source of image
		lowsrc:	ref Url->ParsedUrl;	# for low-resolution devices
		actual: ref Url->ParsedUrl;	# what came back as actual source of image
		imhash:	int;			# hash of src, for fast comparison
		width:	int;
		height:	int;
		next:	cyclic ref CImage;	# next (newer) image in cache
		mims: array of ref MaskedImage;
		complete: int;			# JavaScript Image.complete

		new: fn(src: ref Url->ParsedUrl, lowsrc: ref Url->ParsedUrl, width, height: int) : ref CImage;
		match: fn(a: self ref CImage, b: ref CImage) : int;
		bytes: fn(ci: self ref CImage) : int;
	};

	# In-memory cache of CImages
	ImageCache: adt
	{
		imhd:	ref CImage;	# head (LRU) of cache chain (linked through CImage.next)
		imtl:		ref CImage;	# tail MRU) of cache chain
		n:	int;			# size of chain
		memused: int;		# current total of image mem used by cached images
		memlimit: int;		# keep memused less than this
		nlimit: int;			# keep n less than this

		init: fn(ic: self ref ImageCache);
		resetlimits: fn(ic: self ref ImageCache);
		look: fn(ic: self ref ImageCache, ci: ref CImage) : ref CImage;
		add: fn(ic: self ref ImageCache, ci: ref CImage);
		deletelru: fn(ic: self ref ImageCache);
		clear: fn(ic: self ref ImageCache);
		need: fn(ic: self ref ImageCache, nbytes: int) : int;
	};

	# An connection to some host
	Netconn: adt
	{
		id:		 int;			# for debugging
		host:	string;			# host name
		port:	int;			# port number
		scheme: int;		# Url->HTTP, etc.
		conn:	Sys->Connection;	# fds, etc.
 		sslx:	ref SSL3->Context;	# ssl connection
 		vers:	int;			# ssl version
		state:	int;			# NCfree, etc.
		queue:	cyclic array of ref ByteSource;
						# following are indexes into queue
		qlen:		int;		# queue[0:qlen] is queue of requests
		gocur:	int;		# go thread currently processing
		ngcur:	int;		# ng threads currently processing
		reqsent:	int;		# next to send request for
		pipeline:	int;		# are requests being pipelined?
		connected:	int;	# are we connected to host?
		tstate:	int;		# for use by transport
		tbuf: 	array of byte;	# for use by transport
		idlestart:	int;		# timestamp when went Idle

		new: fn(id: int) : ref Netconn;
		makefree: fn(nc: self ref Netconn);
	};

	# Info from an HTTP response header
	Header: adt
	{
		code:	int;			# HC... (detailed response code)
		actual:	ref Url->ParsedUrl;	# actual request url (may be result of redir)
		base:	ref Url->ParsedUrl;	# Content-Base or request url
		location:	ref Url->ParsedUrl;	# Content-Location
		length:	int;			# -1 if unknown
		mtype:	int;			# TextHtml, etc.
		chset:	int;			# for text types: ISO_8859, etc.
		msg:	string;			# possible message explaining status
		refresh:string;			# used for server push
		chal:	string;			# used if code is HSneedauth
		warn:	string;			# should show this to user
		lastModified:	string;		# last-modified field

		new: fn() : ref Header;
		setmediatype: fn(h: self ref Header, name: string, first: array of byte);
		print: fn(h: self ref Header);
	};

	# A source of raw bytes (with HTTP info)
	ByteSource: adt
	{
		id: int;				# for debugging
		req:	ref ReqInfo;
		hdr:	ref Header;		# filled in from headers
		data:	array of byte;		# all the data, maybe partially filled
		edata: int;				# data[0:edata] is valid
		err: string;			# there was an error
		net:	cyclic ref Netconn;	# servicing fd, etc.
		refgo: int;				# go proc is still using
		refnc: int;				# netconn proc is still using

		# consumer changes only these fields:
		lim: int;				# consumer has seen data[0:lim]
		seenhdr: int;			# consumer has seen hdr

		free: fn(bs: self ref ByteSource);
		stringsource: fn(s: string) : ref ByteSource;
	};

        # Single value and its attributes
        Cookie: adt
        {
                value : string;         # name=value data
                comment : string;	# info about cookie's purpose
                domain : string;        # where cookie should be returned to
                expires : string;       # date/time when cookie is discarded
                maxage : string;	# lifetime of the cookie in seconds
                path : string;          # path where cookie is valid
                secure : int;           # positive value means only use ssl
        };

        # All current cookies
        cookielist : list of Cookie;

	Lock: adt
	{
		enter0: byte;
		enter1: byte;
		turn: byte;
		lockval: byte;
		waiting: int;
		waitch:	chan of byte;

		lock:	fn(l: self ref Lock, caller: int);
		unlock:	fn(l: self ref Lock, caller: int);
		new:	fn() : ref Lock;
		free:	fn(l: self ref Lock);
	};

	# Snapshot of current system resources
	ResourceState: adt
	{
		ms: int;		# a millisecond time stamp
		main: int;		# main memory
		mainlim: int;		# max main memory
		heap: int;		# heap memory
		heaplim: int;		# max heap memory
		image: int;		# used image memory
		imagelim: int;		# max image memory

		cur: fn() : ResourceState;
		since: fn(rnew: self ResourceState, rold: ResourceState) : ResourceState;
		print: fn(r: self ResourceState, msg: string);
	};

	# Globals
	config: Config;
	startres: ResourceState;
	imcache: ref ImageCache;
	progresschan: chan of (int, int, int, string);
	gen: int;	# go generation number

	init: fn(ch: Charon, me: CharonUtils, argl: list of string, underwm: int) : string;

	# Dispatcher functions
	startreq: fn(req: ref ReqInfo) : ref ByteSource;
	waitreq: fn() : ref ByteSource;
	freebs: fn(bs: ref ByteSource);
	abortgo: fn(gopgrp: int);
	netget: fn();

	# Miscellaneous utility functions
	kill: fn(pid: int, dogroup: int);
	getline: fn(fd: ref Sys->FD, buf: array of byte, bstart, bend: int) :
		(array of byte, int, int, int);
	saveconfig: fn(fname: string) : int;
	strlookup: fn(a: array of string, s: string) : int;
	realloc: fn(a: array of byte, incr: int) : array of byte;
	hcphrase: fn(code: int) : string;
	hdraction: fn(bs: ref ByteSource, ismain: int, nredirs: int) : (int, string, string, ref Url->ParsedUrl);
	makestrinttab: fn(a: array of string) : array of StringIntTab->StringInt;
	urlequal: fn(a, b: ref Url->ParsedUrl) : int;
	makeabsurl: fn(s: string) : ref Url->ParsedUrl;
	loadpath: fn(s: string) : string;
	event: fn(s: string, data: int);
	color: fn(s: string, dflt: int) : int;
	raise: fn(e: string);
	assert: fn(i: int);
        setcookie: fn(s: string, host: string, path: string);
        getcookies: fn(host: string, path: string) : list of string;
};
