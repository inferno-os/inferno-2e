implement Tmonitor;

include "sys.m";
	sys: Sys;
	FD, Connection: import sys;
	stderr: ref FD;
	stdlog: ref FD;

include "draw.m";
	Context: import Draw;

include "bufio.m";
	Iobuf: import Bufio;
	bufio: Bufio;

include "tmonitor.m";

include "sh.m";
include "eventCodes.m";
events: Events;

pidChan: chan of int;
buf := array[128] of byte;
errMsg : string;

tmonitorInit(chatty: int, srvfile: string, logFd: ref FD): (list of ref Service, string)
{
	if(sys == nil)
		sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	if(logFd == nil) {
		logFile := "/services/logs/tmonLog";

		stdlog = sys->create(logFile, sys->ORDWR, 8r777);
		if (stdlog == nil) {
			sys->fprint(stderr, "Error: Srv: failed to create %s: %r -- redirect to console\n",
				logFile);
			stdlog = sys->fildes(1);
		}
		else
			sys->fprint(stdlog, "Logging to %s\n", logFile);
	}
	else
		stdlog = logFd;

	srvlist : list of ref Service;
	monFile := "#p/" + string sys->pctl(0, nil) + "/wait";
	if (chatty)
		sys->fprint(stderr, "Open %s\n", monFile);
	mon := sys->open(monFile, sys->OREAD);
	if (mon == nil) {
		errMsg = sys->sprint("Fatal Error: failed to open %s  %r", monFile);
		sys->fprint(stdlog, "%s\n", errMsg);
		return(nil, errMsg);
	}

	pidChan = chan of int;

	(srvlist, errMsg) = opensrv(srvfile);
	if (srvlist == nil) {
		errMsg = sys->sprint("No service list: %s", errMsg);
		sys->fprint(stderr, "%s\n", errMsg);
		return(nil, errMsg);
	}

	TmonDispatch(chatty, srvlist, mon);
	return(srvlist, nil);
}

TmonDispatch(chatty: int, srvlist: list of ref Service, monitor: ref FD)
{
	#
	# start all services in the services file
	#
	red: int;
	current: ref Service;
	slist: list of ref Service;
	slist = srvlist;
	while(slist != nil) {
		current = hd slist;
		if ((*current).ptype == "M")
			spawn gensrvMonolith(*current);
		else
			spawn gensrvSpawner(*current);

		red = <- pidChan;
		sys->fprint(stdlog, "%s is pid %d\n", (*current).port, red);
		(*current).pid = red;

		slist = tl slist;
	}
	if (chatty) {
		sys->sleep(1000);
		dumpTbl(srvlist);
	}

	for(;;) {
		got := sys->read(monitor, buf, len buf);
		sys->fprint(stdlog, "%d %s\n", got, string buf);
	}
}

gensrvMonolith(sv: Service)
{
	pid := sys->pctl(0,nil);
	pidChan <-= pid;

	cmd := load Command hd sv.cmd;
	if(cmd == nil){
		sys->fprint(stdlog, "command %s not found\n", hd sv.cmd);
		exit;
	}

	cmd->init(nil, tl sv.cmd);
}

gensrvSpawner(sv: Service)
{
	pid := sys->pctl(0,nil);
	pidChan <-= pid;
	(ok, c) := sys->announce(sv.net+"!*!"+sv.port);
	if(ok < 0){
		sys->fprint(stdlog, "Error: Srv: can't announce %s: %r\n", hd sv.cmd);
		exit;
	}

	if(sv.net == "udp")
		startsrv(sv.cmd, stdlog, nil);
	else
		for(;;)
			gendoer(sv.cmd, c);
}

gendoer(cmdname: list of string, c: Connection)
{
	(ok, nc) := sys->listen(c);
	if(ok < 0) {
		sys->fprint(stdlog, "listen: %r\n");
		return;
	}

	lbuf := array[64] of byte;
	l := sys->open(nc.dir+"/remote", sys->OREAD);
	if (l == nil) {
		sys->fprint(stdlog, "%d\ttmonitor: gendoer\tfault\t1\t cannot open %s: %r\n",
			events->eventFault, nc.dir+"/remote");
		return;
	}

	n := sys->read(l, lbuf, len lbuf);
	if (n <= 0) {
		sys->fprint(stdlog, "%d\ttmonitor: gendoer\tfault\t1\t read failed on %s: %r\n",
			events->eventFault, nc.dir+"/remote");
		return;
	}
	remote := string lbuf[0:n];
	if(n >= 0)
		sys->fprint(stdlog, "New client (%s): %s %s", hd cmdname, nc.dir, string lbuf[0:n]);
	else
		sys->fprint(stdlog, "New unknown client of (%s)", hd cmdname);
	l = nil;

	nc.dfd = sys->open(nc.dir+"/data", sys->ORDWR);
	if(nc.dfd == nil) {
		sys->fprint(stdlog, "Error: open %s: %r\n", nc.dir);
		return;
	}

	spawn startsrv(cmdname, nc.dfd, remote);
}

startsrv(cmdname: list of string, fd: ref Sys->FD, remote : string)
{
	cmd := load Command hd cmdname;
	if(cmd == nil){
		sys->fprint(stdlog, "command %s not found\n", hd cmdname);
		exit;
	}

	sys->pctl(sys->NEWFD, fd.fd :: nil);
	sys->dup(fd.fd, 0);
	sys->dup(fd.fd, 1);

	if (hd cmdname == "/dis/lib/logind.dis")
		cmd->init(nil, remote :: nil);
	else
		cmd->init(nil, tl cmdname);
}

opensrv(srvfile: string) : (list of ref Service, string)
{
	if(sys == nil)
		sys = load Sys Sys->PATH;

	if(bufio == nil)
		bufio = load Bufio Bufio->PATH;

	srvfd: ref FD;
	if ((srvfd = sys->open(srvfile, sys->OREAD)) == nil)
		return (nil, sys->sprint("opensrv: can't open %s %r", srvfile));

	srvbuf: ref Iobuf;
	if ((srvbuf = bufio->fopen(srvfd, bufio->OREAD)) == nil)
		return (nil, sys->sprint("opensrv: can't fopen %s %r", srvfile));

	srvstr: string;
	srvlist: list of ref Service;
	srvlist = nil;
	line := 0;
	test : int;
	while((srvstr = bufio->srvbuf.gets('\n')) != nil){
		if(srvstr[0] == '#'){   # comment
			line++;
			continue;
		}
		(n, slist) := sys->tokenize(srvstr, " \t\r\n");
		if (n == 0){    # blank line
			line++;
			continue;
		}

		if(n < 3){
			line++;
			return (nil, sys->sprint("Config file %s at line %d record with %d fields", 
				srvfile, line, n));
		}

		# Force enhanced format of config file
		test = int (hd slist);
		if (test == 0)
			return (nil, sys->sprint("Config file %s Unrecognized Format - FATAL", 
				srvfile));

		srvlist = ref Service(0, int (hd slist), hd tl slist, hd tl tl slist, -1, hd tl tl tl slist, tl tl tl tl slist) :: srvlist;
		line++;
	}
	return (srvlist, nil);
}

dumpTbl(slist: list of ref Service)
{
	sys->fprint(stderr, "%5s %5s %6s %14s   %4s  %5s  %s\n", "HITS", "ALLOW", "TYPE", "PORT", "PID", "Net",  "Dis");
	sys->fprint(stderr, "__________________________________________________________\n");

	current: ref Service;
	while(slist != nil) {
		current = hd slist;
		sys->fprint(stderr, "%5d %5d %6s %14s   %4d  %5s  %s\n",
			(*current).deaths, (*current).recover, (*current).ptype,
			(*current).port, (*current).pid,
			(*current).net, hd (*current).cmd);
		slist = tl slist;
	}
	sys->fprint(stderr, "\n\n");
}

