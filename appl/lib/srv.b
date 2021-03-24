implement Server;

include "sys.m";
	sys: Sys;
	FD, Connection: import sys;
	stderr: ref FD;
	stdlog: ref FD;

include "bufio.m";
	Iobuf: import Bufio;
	bufio: Bufio;

include "draw.m";
	Context: import Draw;

include "srv.m";
	srv: Srv;

include "string.m";
        str: String;

include "newns.m";

include "sh.m";

include "tmonitor.m";
tmonitor: Tmonitor;

include "eventCodes.m";

Server: module
{
	init: fn(ctxt: ref Context, args: list of string);
};

# Set defaults and potentially override them with getCmdArgs()
logFile := Events->logdirName+"/"+Events->logName;
namespaceFile := "/services/namespace";
configFile := "/services/server/config";
spawnit := 1;
startLogSrv := 1;
chatty := 1;
logargs : list of string;

init(nil: ref Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	srv = load Srv Srv->PATH;
	str = load String String->PATH;

	# bind #I, if not bound already
	if (sys->open("/net/tcp", Sys->OREAD) == nil && sys->bind("#I", "/net", Sys->MREPL) < 0) {
		sys->fprint(stderr, "Error: srv: can't bind #I: %r\n");
		exit;
	}

	getCmdArgs(args);
	if (spawnit == 0)
		init2(nil, args);
	else
		spawn init2(nil, args);
}

init2(nil: ref Context, args: list of string)
{
	(ok, nil) := sys->stat(namespaceFile);
	if(ok >= 0) {
		ns := load Newns Newns->PATH;
		if(ns == nil) {
			sys->fprint(stderr, "Error: srv: failed to load namespace builder: %s: %r\n",
				Newns->PATH);
			exit;
		}
		else {
			nserr := ns->newns(nil, namespaceFile);
			if(nserr != nil) {
				sys->fprint(stderr, "Error: srv: user namespace file: %s %s\n",
					namespaceFile, nserr);
				exit;
			}
		}
	}

	#
	# cs must be started before listen
	#
	cs := load Command "/dis/lib/cs.dis";
	if(cs == nil) {
		sys->fprint(stderr, "srv: cs module load failed: /dis/lib/cs.dis : %r\n");
		exit;
	}
	else
		cs->init(nil, nil);

	tmonitor = load Tmonitor Tmonitor->PATH;
	if(tmonitor == nil) {
		sys->fprint(stderr, "tmonitor didn't load: %s :%r\n", Tmonitor->PATH);
		exit;
	}

	GrpPid := sys->pctl(sys->NEWPGRP,nil);
	if (chatty)
		sys->print("srv: initialize srvs Pgrp=%d\n", GrpPid);

	if(startLogSrv == 1) {
		if (chatty)
			sys->print("RUN LOGSRV\n");
		#
		# logsrv must start early
		#
		logsrv := load Command "/dis/lib/logsrv.dis";
		if(logsrv == nil) {
			sys->fprint(stderr, "srv: logsrv module load failed: /dis/lib/logsrv.dis : %r\n");
			exit;
		}

		logsrv->init(nil, "srv" :: "log" :: logFile :: logargs);
		stdlog = sys->open(logFile, sys->OWRITE);
		if (stdlog == nil) {
			sys->fprint(stderr, "Error: Srv: failed to open Service %s: %r FATAL\n",
				logFile);
			exit;
		}
		if (chatty) {
			sys->fprint(stderr, "Srv: Logging to %s\n", logFile);
			sys->fprint(stdlog, "Srv: configFile = %s\n", configFile);
			sys->fprint(stdlog, "Srv: namespaceFile = %s\n", namespaceFile);
		}
	}
	else {
		if (chatty)
			sys->print("NOLOGSRV\n");
		if (logFile == nil) {
			stdlog = sys->fildes(1);
			if (chatty)
				sys->fprint(stderr, "Logging to Console\n");
		}
		else {
			(ok, nil) = sys->stat(logFile);
			if (ok < 0) {
				stdlog = sys->create(logFile, sys->ORDWR, 8r640);
				if (stdlog == nil) {
					sys->fprint(stderr, "Error: Srv: failed to create RW %s: %r FATAL\n",
						logFile);
					exit;
				}
			}
			else {
				stdlog = sys->open(logFile, sys->ORDWR);
				if (stdlog == nil) {
					sys->fprint(stderr, "Error: Srv: failed to open RW %s: %r FATAL\n",
						logFile);
					exit;
				}
			}
			if (chatty)
				sys->fprint(stderr, "Logging to %s\n", logFile);
		}
	}

	if (chatty) {
		sys->fprint(stderr, "Srv: configFile = %s\n", configFile);
		sys->fprint(stderr, "Srv: namespaceFile = %s\n\n\n", namespaceFile);
	}

	#
	# open and parse service file and dispatch and monitor threads
	#
	(srvlist, err) := tmonitor->tmonitorInit(chatty, configFile, stdlog);
	if(srvlist == nil){
		sys->fprint(stdlog, "Error: Srv: %s\n", err);
		exit;
	}
}

keyVal(tok: string): (string, string)
{
	(key, val) := str->splitl(tok, "=");
	if (val != nil)
		(nil, val) = str->splitr(val, "=");
	return(key,val);
}

getCmdArgs(args: list of string)
{
	tmpl := args;
	if (tmpl != nil)
		tmpl = tl tmpl;
	while(tmpl != nil) {
		(key, val) := keyVal(hd tmpl);
		case key {
		"nospawn" => spawnit = 0;
		"nologsrv" => startLogSrv = 0;
		"ns" => namespaceFile = val;
		"log" => logFile = val;
		"config" => configFile = val;
		"journal" => logargs = key :: logargs;
		"measure" => logargs = key :: logargs;
		"quiet" => chatty = 0;
		* => ;
		}
		tmpl = tl tmpl;
	}
}

