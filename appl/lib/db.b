implement DB;

BLKSIZ :		con 8192;

include "sys.m";
    sys :	Sys;

include "draw.m";

include "keyring.m";
	kr: Keyring;

include "security.m";
	au: Auth;

include "db.m";

RES_HEADER_SIZE : con 22;

open(addr, username, password, dbname: string) : (ref DB_Handle, list of string)
{
    (fd, err) := connect(addr, Auth->NOSSL);
    if ( nil == fd ) {
	return (nil, err :: nil);
    }
    return dbopen(fd, username, password, dbname);
}

connect(addr : string, alg : string) : (ref Sys->FD, string)
{
    if (nil == sys) {
		sys = load Sys "$Sys";
    }
#
#	Should check to see if addr == /dev/sysname.  If so, probably should
#	change addr to localhost, since some underlying platform code might
#	choke if handed its own address.
#
    (ok, c) := sys->dial(addr, nil);
    if ( ok < 0 ){
		return (nil, sys->sprint("DB init: dial %s: %r\n", addr));
    }
    (n, addrparts) := sys->tokenize(addr, "!");
    if ( n >= 2 )
	addr = hd addrparts + "!" + hd tl addrparts;

	kr := load Keyring Keyring->PATH;

	user := user();
	kd := "/usr/" + user + "/keyring/";
	cert := kd + addr;
	(ok, nil) = sys->stat(cert);
	if(ok < 0){
		cert = kd + "default";
		# (ok, nil) = sys->stat(cert);
		# if(ok<0)
		#	sys->fprint(stderr, "Warning: no certificate found in %s; use getauthinfo\n", kd);
	}

	ai := kr->readauthinfo(cert);

	#
	# let auth->client handle nil ai
	# if(ai == nil){
	#	return (nil, sys->sprint("DB init: certificate for %s not found, use getauthinfo first", addr));
	# }
	#

	au := load Auth Auth->PATH;
	if(au == nil){
		return (nil, sys->sprint("DB init: can't load module Auth %r"));
	}

	err := au->init();
	if(err != nil){
		return (nil, sys->sprint("DB init: can't initialize module Auth: %s", err));
	}

	fd := ref Sys->FD;

	if ( alg != "noauth" && alg != "nossl" )
	    if ( sys->bind("#D", "/n/ssl", Sys->MREPL) < 0)
		return (nil, sys->sprint("can't bind #D: %r"));

	(fd, err) = au->client(alg, ai, c.dfd);
	if(fd == nil){
		return (nil, sys->sprint("DB init: authentication failed: %s", err));
	}

    return (fd, nil);
}

dbopen(fd : ref Sys->FD, username, password, dbname : string) :
						(ref DB_Handle, list of string)
{
    dbh : DB_Handle;

    dbh.datafd = fd;
    dbh.lock = makelock();
    dbh.sqlstream = -1;
    logon := array of byte (username +"/"+ password +"/"+ dbname);
    (mtype, strm, rc, data) := sendReq(ref dbh, 'I', logon);
    if ( mtype == 'h' ) {
	dbh.datafd = nil;
	return (nil, (sys->sprint("DB: couldn't initialize %s for %s", dbname,
					username) :: string data :: nil));
    }
    dbh.sqlconn = int string data;

    (mtype, strm, rc, data) = sendReq(ref dbh, 'O', array of byte string dbh.sqlconn);
    if ( mtype == 'h' ) {
 	dbh.datafd = nil;
	return (nil, (sys->sprint("DB: couldn't open SQL connection") :: string data :: nil));
    }
    dbh.sqlstream = int string data;
    return (ref dbh, nil);
}

status(s : string)
{
    #	Supposed to do something with an error message
    sys->fprint(sys->fildes(2), "%s\n", s);
    return;
}

DB_Handle.SQLOpen(oldh : self ref DB_Handle) : (int, ref DB_Handle)
{
    dbh : DB_Handle = *oldh;
    (mtype, strm, rc, data) := sendReq(ref dbh, 'O', array of byte string dbh.sqlconn);
    if ( mtype == 'h' ) {
	return (-1, nil);
    }
    dbh.sqlstream = int string data;
    return (0, ref dbh);
}

DB_Handle.SQLClose(handle: self ref DB_Handle) : int
{
    (mtype, strm, rc, data) := sendReq(handle, 'K', array[0] of byte);
    if ( mtype == 'h' ) {
	return -1;
    }
    handle.sqlstream = -1;
    return 0;    
}

DB_Handle.SQL(handle: self ref DB_Handle, command: string) : (int, list of string)
{
    (mtype, strm, rc, data) := sendReq(handle, 'W', array of byte command);
    if ( mtype == 'h' ) {
	return (-1, "Probable SQL format error" :: string data :: nil);
    }
    return (0, nil);
}

DB_Handle.columns(handle: self ref DB_Handle) : int
{
    (mtype, strm, rc, data) := sendReq(handle, 'C', array[0] of byte);
    if ( mtype == 'h' ) {
	return 0;
    }
    return int string data;
}

DB_Handle.nextRow(handle: self ref DB_Handle) : int
{
    (mtype, strm, rc, data) := sendReq(handle, 'N', array[0] of byte);
    if ( mtype == 'h' ) {
	return 0;
    }
    return int string data;
}

DB_Handle.read(handle: self ref DB_Handle, columnI: int) : (int, array of byte)
{
    (mtype, strm, rc, data) := sendReq(handle, 'R',
						array of byte string columnI);
    if ( mtype == 'h' ) {
	return (-1, data);
    }
    return (len data, data);
}

DB_Handle.write(handle : self ref DB_Handle, paramI : int, val : array of byte)
								: int
{
    outbuf := array[len val + 4] of byte;
    param := array of byte sys->sprint("%3d ", paramI);

    for ( i := 0; i < 4; i++ ) {
	outbuf[i] = param[i];
    }
    outbuf[4:] = val;
    (mtype, strm, rc, data) := sendReq(handle, 'P', outbuf);
    if ( mtype == 'h' ) {
	return -1;
    }
    return len val;
}
 
DB_Handle.columnTitle(handle: self ref DB_Handle, columnI: int) : string
{
    (mtype, strm, rc, data) := sendReq(handle, 'T',
						array of byte string columnI);
    if ( mtype == 'h' ) {
	return nil;
    }
    return string data;
}

DB_Handle.errmsg(handle: self ref DB_Handle) : string
{
    (mtype, strm, rc, data) := sendReq(handle, 'H', array[0] of byte);
    return string data;
}

sendReq(handle : ref DB_Handle, mtype : int, data : array of byte) :
					(int, int, int, array of byte)
{
    lock(handle);
    header := sys->sprint("%c1%11d %3d ", mtype, len data, handle.sqlstream);
    if ( sys->write(handle.datafd, array of byte header, 18) != 18 ) {
	unlock(handle);
	return ('h', handle.sqlstream, 0, array of byte "header write failure");
    }
    if ( sys->write(handle.datafd, data, len data) != len data ) {
	unlock(handle);
	return ('h', handle.sqlstream, 0, array of byte "data write failure");
    }
    if ( sys->write(handle.datafd, array of byte "\n", 1) != 1 ) {
	unlock(handle);
	return ('h', handle.sqlstream, 0, array of byte "header write failure");
    }
    hbuf := array[RES_HEADER_SIZE+3] of byte;
    if ( (n := sys->read(handle.datafd, hbuf, RES_HEADER_SIZE)) !=
							RES_HEADER_SIZE ) {
	#  Is this an error?  Try another read.
	if ( n < 0 ||
		(m := sys->read(handle.datafd, hbuf[n:], RES_HEADER_SIZE-n)) !=
							RES_HEADER_SIZE-n ) {
	    unlock(handle);
	    return ('h', handle.sqlstream, 0, array of byte "read error");
	}
    }
    rheader := string hbuf[0:22];
    rtype := rheader[0];
    #	Probably should check version in header[1]
    datalen := int rheader[2:13];
    rstrm := int rheader[14:17];
    retcode := int rheader[18:21];
    
    databuf := array[datalen] of byte;
    # read in loop until get amount of data we want.  If there is a mismatch
    # here, we may hang with a lock on!

    nbytes : int;

    for ( length := 0; length < datalen; length += nbytes ) {
	nbytes = sys->read(handle.datafd, databuf[length:], datalen-length);
	if ( nbytes <= 0 ) {
	    break;
	}
    }
    nbytes = sys->read(handle.datafd, hbuf, 1);	#  The final \n
    unlock(handle);
    return (rtype, rstrm, retcode, databuf);
}

makelock() : chan of int
{
    ch := chan of int;
    spawn holdlock(ch);
    return ch;
}

holdlock(c : chan of int)
{
    for ( ; ; ) {
	i := <- c;
	c <- = i;
    }
}

lock(h : ref DB_Handle)
{
    h.lock <- = h.sqlstream;
}

unlock(h : ref DB_Handle)
{
    <- h.lock;
}

user(): string
{
	sys = load Sys Sys->PATH;

	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0)
		return "";

	return string buf[0:n];	
}
