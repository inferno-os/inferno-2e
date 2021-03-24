implement Logind;

include "sys.m";
	sys: Sys;
    sprint, fprint, print, millisec : import sys;

include "draw.m";

include "keyring.m";
	kr: Keyring;

include "security.m";
	ssl: SSL;
	login: Login;

Netboot: module
{
  PATH : con "/dis/lib/netboot.dis";
  init : fn(nil: ref Draw->Context, nil: list of string);
};

Logind: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

stderr, stdin: ref Sys->FD;
 
init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdin = sys->fildes(0);
	stderr = sys->open("/dev/cons", sys->OWRITE);

	kr = load Keyring Keyring->PATH;

	login = load Login Login->PATH;
	if(login == nil){
		sys->fprint(stderr, "Error: logind: can't load module Login %r\n");
		exit;
	}

	error := login->init();
	if(error != nil){
		sys->fprint(stderr, "Error: logind: %s\n", error);
		exit;
	}

	ssl = load SSL SSL->PATH;
	if(ssl == nil){
		sys->fprint(stderr, "Error: logind: can't load module SSL %r\n");
		exit;
	}

	# push ssl, leave in clear mode
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0){
		sys->fprint(stderr, "Error: logind: can't bind #D (SSL) %r");
		exit;
	}

    ## seperate
    (err, conn) := ssl->connect(stdin);     
	if(conn == nil){
		sys->fprint(stderr, "Error: logind: can't push ssl: %s\n", err);
		exit;
	}

    # Here the road forks, we want logind to timeout so that server 
    # sockets can be GCed even if the client disappers (power down,
    # crash, reset) when the login protocol is blocked reading a
    # half-open socket.
    #

    lpid, tpid, fpid : int;
    cpidchan := chan of int;

	# kill entire group in case children spawn procs of their own 
    mypid:=sys->pctl(Sys->NEWPGRP,nil); 

    spawn dologind(cpidchan, argv, conn);
    lpid =<-cpidchan;

    spawn tmon(cpidchan);
    tpid =<- cpidchan;

    # Rendezvous with first thread to finish
    fpid = <- cpidchan;
    if (fpid == tpid) { # time out
        ton := "login";
        if (Netbootp) {
            ton = "netboot";
        }
        fprint(stderr, "logind: %s timed out!\n",ton);
    }
    
    # Kill the entire group to remove any dangling threads.
    fd := sys->open("#p/" + string (sys->pctl(0,nil)) + "/ctl", Sys->OWRITE);
    if (fd == nil) {
        fprint(stderr, "Error: logind can't open '#p' ctl file - %r\n");
    } else {
        kmsg := array of byte "killgrp";
        sys->write(fd, kmsg, len kmsg); # ignore failure
    }
}

LTO : con 300000; # five minutes

Netbootp := 0; 
# Netboot retries calling-back the client for up to two minutes.
# If the retry parmaters are embedded in appl/cmd/rcb.b change this
# this timeout may not be appropriate.
NBTO : con 300000; # five minutes

tmon(pidchan : chan of int)
{
    mypid := sys->pctl(0,nil);
    pidchan <-= mypid;
    sys->sleep(LTO); 
    if (Netbootp) { # netboot needs extra time...
        sys->sleep(NBTO); # additive
    }
    pidchan <-= mypid; # tell parent we're done
}

#
# logind, the real part
#
dologind(pidchan : chan of int, argv : list of string, conn : ref Sys->Connection)
{
    mypid := sys->pctl(0,nil);
    pidchan <-= mypid;

	# server hello
	pw : ref Password->PW;
	info : ref Keyring->Authinfo;
    err : string;

    ## ms = millisec();
	(pw, err) = login->shello("/licensedb/Agreement", conn);
	if(pw == nil){
		sys->fprint(stderr, "Error: logind: %s\n", err);
        pidchan <-= mypid;
        return;
	} 
    ## ms = mst(ms,"login->shello()");
	# get default signer key
	(info, err) = login->signerkey("/keydb/signerkey");
	if(info == nil){
		sys->fprint(stderr, "Error: logind: %s\n", err);
        pidchan <-= mypid;
        return;
	}
    ##ms = mst(ms, "login->signerkey()");
	# start key exchange
	(info, err) = login->skeyx(pw, info, conn);
	if(info == nil){
		sys->fprint(stderr, "Error: logind: %s\n", err);
        pidchan <-= mypid;
		return;
	}
    ##ms = mst(ms, "login->skeyx()");

	# save a copy of certificate
	#file := "/usr/" + pw.id + "/keyring/certificate";
	#if(kr->writeauthinfo(file, info) < 0)
	#	sys->fprint(stderr, "Error: logind: writeauthinfo to %s failed %r\n", file);

	netboot := load Netboot Netboot->PATH;
    ##ms = mst(ms, "load Netboot");
	if (netboot != nil) {
        # Netboot eventually results in a call of Keyring->auth()
        # which reads from the far end,  so it's susceptible to blocking
        # on a vanished client. 
        Netbootp = 1; # Just bump up the timer.
        netboot->init(nil, argv);
        ##ms = mst(ms, "netboot->init()");
    }
    pidchan <-= mypid;     # tell parent we're done
}

##ms : int; # debug timing traces

##mst(bt : int, msg : string) : int
##{
##    fprint(stderr,"logind: %s took %d mS\n", msg, millisec() - bt);
##    return millisec();
##}
