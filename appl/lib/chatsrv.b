implement ChatSrv;

include "sys.m";
        sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";


HISTLEN:	con 16;

ChatSrv: module
{
	init: fn(nil: ref Draw->Context, nil: list of string);
};

ReadReq: adt
{
	nb:	int;
	rc:	Sys->Rread;
};

ChatConn: adt
{	
	fid:		int;
	name:		string;
	outbuf:		string;
	req:		ref ReadReq;
	session:	cyclic ref ChatSession;

	addmessage:	fn(c: self ref ChatConn, str: string);
	sendbuffer:	fn(c: self ref ChatConn, nb: int): (array of byte, string);
};
cList: list of ref ChatConn;

ChatSession: adt
{
	name:		string;
	history:	list of string;
	connList:	cyclic list of ref ChatConn;

	addmessage:	fn(s: self ref ChatSession, from: string, buf: array of byte): (int, string);
	addhistory:	fn(s: self ref ChatSession, str: string);
};
sList: list of ref ChatSession;

StatConn: adt
{
	fid:	int;
	buf:	array of byte;
};
stList: list of ref StatConn;	


init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	if(sys == nil) {
		sys->print("chatsrv: error: load Sys: %r\n");
		return;
	}

	stderr = sys->fildes(2);

	sys->bind("#s", "/chan", sys->MBEFORE);
	chatio := sys->file2chan("/chan", "chat");
	if(chatio == nil) {
		sys->fprint(stderr, "chatsrv: error: chatio file2chan: %r\n");
		return;
	}
	ctlio := sys->file2chan("/chan", "chatctl");
	if(ctlio == nil) {
		sys->fprint(stderr, "chatsrv: error: ctlio file2chan: %r\n");
		return;
	}

	spawn srv(chatio, ctlio);
}

srv(chatio, ctlio: ref Sys->FileIO)
{
	nb: int;
	rc: Sys->Rread;
	wc: Sys->Rwrite;
	fid: int;
	buf: array of byte;

	sys->pctl(Sys->NEWPGRP, nil);

	for (;;) {
		alt {

		(nil, buf, fid, wc) = <- chatio.write =>
			if(wc != nil)
				wc <-= parsestring(buf, fid);
			else
				closeconn(fid);

		(nil, nb, fid, rc) = <- chatio.read =>
			if(rc != nil)
				queuereadreq(fid, nb, rc);
			else
				closeconn(fid);

		(nil, nb, fid, rc) = <- ctlio.read =>
			if(rc != nil)
				rc <-= sendstatus(fid, nb);
			else
				closestatconn(fid);

		(nil, nil, fid, wc) = <- ctlio.write =>
			if(wc != nil)
				wc <-= (0, "chatsrv: write to ctl file not implemented");
			else
				closestatconn(fid);
		}
	}
}

parsestring(buf: array of byte, fid: int): (int, string)
{
	if(buf[0] != byte ':')
		return parsecommand(buf, fid);

	c := getconn(fid);
	if(c != nil)
		return c.session.addmessage(c.name, buf);

	return (0, "chatsrv: error: fid " + string fid + " not in any session");
}

parsecommand(buf: array of byte, fid: int): (int, string)
{
	ok: int;
	err: string;

	s := string buf;
	(argc, argl) := sys->tokenize(s, " \t\n");
	if(argc < 0)
		return (0, "chatsrv: tokenize error: " + s);
	if(argc == 0)
		return (len buf, nil);

	cmd := hd argl;

	if(cmd == "join" && argc == 3)
		(ok, err) = joinsession(hd tl argl, fid, hd tl tl argl);

	else if(cmd == "drop" && argc == 1)
		(ok, err) = dropconn(fid);

	else
		(ok, err) = (0, "chatsrv: error: bad command: " + s);

	if(ok)
		return (len buf, nil);
	else
		return (0, err);
}

queuereadreq(fid: int, nb: int, rc: Sys->Rread)
{
	c := getconn(fid);
	if(c == nil){
		rc <-= (nil, "chatsrv: error: fid " + string fid + " not connected to any session");
		return;
	}

	if(c.req != nil)
		sys->fprint(stderr, "chansrv: error: previous read request not nil: %s(%d)!\n", c.name, c.fid);

	if(len c.outbuf > 0){
		rc <-= c.sendbuffer(nb);
		c.req = nil;
		return;
	}

	c.req = ref ReadReq(nb, rc);
}

ChatSession.addmessage(
	s: self ref ChatSession, 
	from: string, 
	buf: array of byte
		): (int, string)
{
	str := from + ": " + string buf[1:len buf] + "\n";

	s.addhistory(str);
	for(cl := s.connList; cl != nil; cl = tl cl)
		(hd cl).addmessage(str);
	return (len buf, nil);
}

ChatSession.addhistory(s: self ref ChatSession, str: string)
{
	s.history = str :: s.history ;
	if(len s.history > HISTLEN){
		n: list of string;
		h := s.history;
		for(i := 0; i < HISTLEN; i++){
			n = hd h :: n;
			h = tl h;
		}
		s.history = n;
	}
}

ChatConn.addmessage(c: self ref ChatConn, str: string)
{
	c.outbuf += str;
	if(c.req != nil){
		c.req.rc <-= c.sendbuffer(c.req.nb);
		c.req = nil;
	}
}

ChatConn.sendbuffer(c: self ref ChatConn, nb: int): (array of byte, string)
{
	buf : array of byte;

	if(nb >= len c.outbuf){
		buf = array of byte c.outbuf;
		c.outbuf = "";
	}
	else {
		buf = array of byte c.outbuf[:nb];
		c.outbuf = c.outbuf[nb:];
	}
	return (buf, nil);
}

joinsession(session: string, fid: int, alias: string): (int, string)
{
	#sys->print("chatsrv: fid %d joining session %s as alias %s\n", fid, session, alias);

	if(getconn(fid) != nil)
		closeconn(fid);

	c := ref ChatConn(fid, alias, "", nil, nil);

	s := getsession(session);
	if(s != nil){
		for(cl := s.connList; cl != nil; cl = tl cl){
			tc := hd cl;
			if(tc.name == alias)
				return (0, "alias " + alias + " already in use");
		}
	}
	if(s == nil){
		s = ref ChatSession(session, nil, nil);
		sList = s :: sList;
	}
	else {
		rh := revhist(s.history);
		for(; rh != nil; rh = tl rh)
			c.addmessage(hd rh);
	}

	s.connList = c :: s.connList;
	c.session = s;

	cList = c :: cList;

	s.addmessage("", array of byte (":<" + alias +" joins.>"));
	return (1, nil);
}

revhist(a: list of string): list of string
{
	ret: list of string;

	for(; a != nil; a = tl a)
		ret = hd a :: ret;
	return ret;
}

dropconn(fid: int): (int, string)
{
	#sys->print("chatsrv: dropping fid %d\n", fid);
	if(getconn(fid) != nil)
		closeconn(fid);
	return (1, nil);
}

getconn(fid: int): ref ChatConn
{
	for(cl := cList; cl != nil; cl = tl cl){
		c := hd cl;
		if(c.fid == fid)
			return c;
	}
	return nil;
}

getsession(session: string): ref ChatSession
{
	for(sl := sList; sl != nil; sl = tl sl){
		s := hd sl;
		if(s.name == session)
			return s;
	}
	return nil;
}

closeconn(fid: int)
{
	s : ref ChatSession;
	alias : string;
	ncList : list of ref ChatConn;
	nsList : list of ref ChatSession;

	# remove fid from cList and subsequently from sList
	# if sList has no remaining connections, remove it.

	for(cl := cList; cl != nil; cl = tl cl){
		c := hd cl;
		if(c.fid != fid)
			ncList = c :: ncList;
		else{
			s = c.session;
			alias = c.name;
		}
	}
	if(s == nil)
		return;
	cList = ncList;

	ncList = nil;
	for(cl = s.connList; cl != nil; cl = tl cl){
		c := hd cl;
		if(c.fid != fid)
			ncList = c :: ncList;
	}
	s.connList = ncList;

	if(len ncList != 0){
		s.addmessage("", array of byte (":<" + alias +" departs.>"));
		return;
	}

	nsList = nil;
	for(sl := sList; sl != nil; sl = tl sl){
	 	ts := hd sl;
		if(ts != s)
			nsList = ts :: nsList;
	}
	sList = nsList;
}

sendstatus(fid: int, nb: int): (array of byte, string)
{
	d: array of byte;
	st: ref StatConn;
	nstList: list of ref StatConn;

	found := 1;
	for(stl := stList; stl != nil; stl = tl stl){
		tst := hd stl;
		if(tst.fid == fid)
			st = tst;
		else
			nstList = tst :: nstList;
	}
	if(st == nil){
		found = 0;
		st = ref StatConn(fid, array of byte listsessions());
	}

	if(nb < len st.buf){
		if(!found)
			stList = st :: stList;

		d = st.buf[:nb];
		st.buf = st.buf[nb:];
		return (d, nil);
	}

	if(found && len st.buf == 0)
		stList = nstList;
	else if(!found)
		stList = st :: stList;

	d = st.buf;
	st.buf = array of byte "";
	return (d, nil);
}	

listsessions(): string
{
	str : string;

	for(sl := sList; sl != nil; sl = tl sl){
		s := hd sl;
		str += s.name + ":";
		for(cl := s.connList; cl != nil; cl = tl cl){
			tc := hd cl;
			str += " " + tc.name;
		}
		str += "\n";
	}
	return str;
}

closestatconn(fid: int)
{
	nstList: list of ref StatConn;

	for(stl := stList; stl != nil; stl = tl stl){
		tst := hd stl;
		if(tst.fid != fid)
			nstList = tst :: nstList;
	}
	stList = nstList;
}

