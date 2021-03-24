# Implement a subset of Mosaic Common Client protocol
#
# Currently implemented:
#	GET URL <url> OUTPUT CURRENT
#		(but repsonse is always an immediate OK)
#	SEND BROWSERVIEW
#	SEND BROWSERVIEW STOP
#	DISCONNECT
# and extensions:
#	HOME
#	FORWARD
#	BACK
#	PAGEUP
#	PAGEDOWN
#	RELOAD
#	ABORT
#	BOOKMARKS

implement CCI;

include "sys.m";
	sys: Sys;
	FD, Connection: import sys;

include "draw.m";

include "string.m";
	S: String;

include "bufio.m";
	B : Bufio;
	Iobuf: import Bufio;

include "event.m";
	E: Events;
	Event: import E;

include "url.m";
	U: Url;
	ParsedUrl: import U;

include "cci.m";

DBG: con 0;

R_OK: con 200;
R_GET_OK: con 210;
R_DISCONNECT_OK: con 212;
R_BROWSERVIEW_OK: con 219;
R_BROWSERVIEW_STOP_OK: con 220;
R_BACK_OK: con 221;
R_FORWARD_OK: con 222;
R_HOME_OK: con 223;
R_RELOAD_OK: con 224;
R_PAGEUP_OK: con 225;
R_PAGEDOWN_OK: con 226;
R_ABORT_OK: con 227;
R_BOOKMARKS_OK: con 228;
R_ADDBOOKMARK_OK: con 229;
R_PAGELEFT_OK: con 230;
R_PAGERIGHT_OK: con 231;

R_SEND_DATA_OUTPUT: con 302;	# output from Send Output protocol
R_SEND_BROWSERVIEW: con 303;	# output from Send Browserview proto

R_UNRECOGNIZED: con 401;	# what's this?
R_ERROR: con 402;	# does not follow protocol

R_DONE: con 600;	# we're finished

ech: chan of ref Event;
cctl: chan of (int, string, string, array of byte);
connected := 0;
sending_view := 0;
lastdata: array of byte = nil;
lasturl := "";
lastctype := "";

init(smod: String, emod: Events, umod: Url)
{
	sys = load Sys Sys->PATH;
	B = load Bufio Bufio->PATH;
	S = smod;
	E = emod;
	U = umod;
	if(B == nil || S == nil || E == nil || U == nil) {
		sys->print("cci: null module(s)\n");
		return;
	}
	ech = E->evchan;
	cctl = chan of (int, string, string, array of byte);
	(aok, c) := sys->announce("tcp!*!6674");
	if(aok < 0) {
		sys->print("cci: can't announce cci service: %r\n");
		return;
	}
	spawn cci_listen(c);
}

cci_listen(c: Connection)
{
	(lok, nc) := sys->listen(c);
	if(lok < 0) {
		sys->print("cci: listen: %r\n");
		return;
	}
	buf := array[64] of byte;
	l := sys->open(nc.dir+"/remote", sys->OREAD);
	n := sys->read(l, buf, len buf);
	if(n >= 0 && DBG)
		sys->print("cci: client: %s %s", nc.dir, string buf[0:n]);

	nc.dfd = sys->open(nc.dir+"/data", sys->ORDWR);
	if(nc.dfd == nil) {
		sys->print("cci: open: %s: %r\n", nc.dir);
		return;
	}
	ior := B->fopen(nc.dfd, sys->OREAD);
	iow := B->fopen(nc.dfd, sys->OWRITE);
	if(ior == nil || iow == nil) {
		sys->print("cci: bufio open: %s: %r\n", nc.dir);
		return;
	}
	csync := chan of int;
	spawn cci_from_client(ior, csync);
	<- csync;
	spawn cci_to_client(iow, csync);
	<- csync;
	B->iow.puts(sys->sprint("%d Hello, this is Charon\r\n", R_OK));
	B->iow.flush();
	if(DBG)
		sys->print("wrote: %d Hello, this is Charon\r\n", R_OK);
	connected = 1;
}

cci_from_client(io: ref Iobuf, csync: chan of int)
{
	csync <-= 1;
    loop:
	for(;;) {
		if(DBG)
			sys->print("\tcci_from_client reading from client\n");
		line := B->io.gets('\n');
		if(DBG)
			sys->print("\tgot '%s' from client\n", line);
		if(line == "") {
			break;
		}
		(n, l) := sys->tokenize(line, " \t\r\n");
		if(n < 1) {
			cctl <-= (R_ERROR, "", "", nil);
			continue;
		}
		case hd l {
		"ABORT" =>
			ech <-= ref Event.Estop(0);
			cctl <-= (R_ABORT_OK, "", "", nil);
		"ADD-BOOKMARK" =>
			ech <-= ref Event.Estop(0);	# TODO: implement add bookmark
			cctl <-= (R_ADDBOOKMARK_OK, "", "", nil);
		"BACK" =>
			ech <-= ref Event.Ego("", "_top", 0, E->EGback);
			cctl <-= (R_BACK_OK, "", "", nil);
		"BOOKMARKS" =>
			ech <-= ref Event.Ego("", "_top", 0, E->EGbookmarks);
			cctl <-= (R_BOOKMARKS_OK, "", "", nil);
		"DISCONNECT" =>
			cctl <-= (R_DISCONNECT_OK, "", "", nil);
			break loop;
		"FORWARD" =>
			ech <-= ref Event.Ego("", "_top", 0, E->EGforward);
			cctl <-= (R_FORWARD_OK, "", "", nil);
		"GET" =>
			if(n >=3 && nth(l, 1) == "URL") {
				url := nth(l, 2);
				url = url[1:len url -1];
				pu := U->makeurl(url);
				if(pu.scheme == U->NOSCHEME) {
					# relative to current page
					bu := U->makeurl(lasturl);
					pu.makeabsolute(bu);
					url = pu.tostring();
				}
				ech <-= ref Event.Ego(url, "_top", 0, E->EGnormal);
				cctl <-= (R_GET_OK, "", "", nil);
			}
			else
				cctl <-= (R_UNRECOGNIZED, "", "", nil);
		"HOME" =>
			ech <-= ref Event.Ego("", "_top", 0, E->EGhome);
			cctl <-= (R_HOME_OK, "", "", nil);
		"PAGEUP" =>
			ech <-= ref Event.Ekey(E->Kup);
			cctl <-= (R_PAGEUP_OK, "", "", nil);
		"PAGEDOWN" =>
			ech <-= ref Event.Ekey(E->Kdown);
			cctl <-= (R_PAGEDOWN_OK, "", "", nil);
		"PAGELEFT" =>
			ech <-= ref Event.Ekey(E->Kleft);
			cctl <-= (R_PAGELEFT_OK, "", "", nil);
		"PAGERIGHT" =>
			ech <-= ref Event.Ekey(E->Kright);
			cctl <-= (R_PAGERIGHT_OK, "", "", nil);
		"RELOAD" =>
			ech <-= ref Event.Ego("", "_top", 0, E->EGreload);
			cctl <-= (R_RELOAD_OK, "", "", nil);
		"SEND" =>
			what := nth(l, 1);
			opt := nth(l, 2);
			if(what == "BROWSERVIEW" && opt == "") {
				sending_view = 1;
				cctl <-= (R_BROWSERVIEW_OK, "", "", nil);
				if(lastdata != nil)
					cctl <-=  (R_SEND_BROWSERVIEW, lasturl, lastctype, lastdata);
			}
			else if(what == "BROWSERVIEW" && opt == "STOP") {
				sending_view = 0;
				cctl <-= (R_BROWSERVIEW_STOP_OK, "", "", nil);
			}
			else
				cctl <-= (R_UNRECOGNIZED, "", "", nil);
		* =>
			cctl <-= (R_UNRECOGNIZED, "", "", nil);
		}
	}
	cctl <-= (R_DONE, "", "", nil);
	connected = 0;
	sending_view = 0;
	lastdata = nil;
	if(DBG)
		sys->print("\tclient listener exiting\n");
}

cci_to_client(io: ref Iobuf, csync: chan of int)
{
	csync <-= 1;
	for(;;) {
		if(DBG)
			sys->print("cci_to_client waiting for something to send\n");
		(code, msg, ctype, data) := <- cctl;
		if(DBG)
			sys->print("got something; writing to client\n");
		if(code == R_DONE)
			return;
		B->io.puts(sys->sprint("%d %s\r\n", code, msg));
		if(ctype != "") {
			n := len data;
			B->io.puts(sys->sprint("Content-Type:%s\r\nContent-Length:%d\r\n", ctype, n));
			nw := B->io.write(data, n);
			if(nw != n)
				sys->print("cci: tried to write %d, wrote %d\n", n, nw);
		}
		B->io.flush();
		if(DBG) {
			sys->print("wrote: %d %s\r\n", code, msg);
			if(ctype != "")
				sys->print("wrote: Content-Type:%s\r\nContent-Length:%d\r\n<data>\n", ctype, len data);
		}
	}
}

view(url, ctype: string, data: array of byte)
{
	if(DBG)
		sys->print("\t\tview called for url %s, connected=%d, sending_view=%d\n",
				url, connected, sending_view);
	if(!sending_view) {
		lastdata = data;
		lasturl = url;
		lastctype = ctype;
	}
	else
		cctl <-= (R_SEND_BROWSERVIEW, url, ctype, data);
}

# 0-origin indexing of string list
nth(l: list of string, n: int) : string
{
	while(l != nil && n > 0) {
		l = tl l;
		n--;
	}
	if(l == nil)
		return "";
	else
		return hd l;
}
