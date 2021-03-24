implement PPPClient;


include "sys.m";
	sys : Sys;
include "draw.m";

include "kill.m";
include "modem.m";
include "script.m";
include "lock.m";

include "pppclient.m";

#
# Globals (these will have to be removed if we are going multithreaded)
#

pid := -1;
modeminfo:	ref	Modem->ModemInfo;

ppplog(log: chan of int, errfile: string) 
{
	pid = sys->pctl(0, nil);				# set reset pid to our pid 
	src := sys->open(errfile, Sys->OREAD);
	if (src == nil)
		sys->raise(sys->sprint("fail: Couldn't open %s: %r", errfile));

	LOGBUFMAX:	con 1024;
	buf := array[LOGBUFMAX] of byte;

    	while ((count := sys->read(src, buf, LOGBUFMAX)) > 0) {
	    	(n, toklist) := sys->tokenize(string buf[:count],"\n");
	    	for (;len toklist;toklist = tl toklist) {
			case hd toklist {
				"no error" =>
					log <-= s_SuccessPPP;
					break;
				"permission denied" =>
					lasterror = "Username or Password Verification Failed";
					log <-= s_Error;
					break;
				"write to hungup channel" =>
					lasterror = "Remote Host Hung Up";
					log <-= s_Error;
					break;
				* =>
					lasterror = hd toklist;
					log <-= s_Error;
					break;
			};
		}
	}
}

startppp(logchan: chan of int, pppinfo: ref PPPInfo)
{
	ifd := sys->open("/net/ipifc/clone", Sys->ORDWR);
	if (ifd == nil)
		sys->raise("fail: Couldn't open /net/ipifc/clone");

	buf := array[32] of byte;
	n := sys->read(ifd, buf, len buf);
	if(n <= 0)
		sys->raise("fail: can't read from /net/ipifc/clone");

	spawn ppplog(logchan, "/net/ipifc/" + string buf[0:n] + "/err");
	logchan <-= s_StartPPP;

	if (pppinfo.ipaddr == nil)
		pppinfo.ipaddr = "-";
#	if (pppinfo.ipmask == nil)
#		pppinfo.ipmask = "255.255.255.255";
	if (pppinfo.peeraddr == nil)
		pppinfo.peeraddr = "-";
	if (pppinfo.maxmtu == nil)
		pppinfo.maxmtu = "512";
	if (pppinfo.username == nil)
		pppinfo.username = "-";
	if (pppinfo.password == nil)
		pppinfo.password = "-";
	framing := "1";

	ifc := "bind ppp "+modeminfo.path+" "+ pppinfo.ipaddr+" "+pppinfo.peeraddr+" "+pppinfo.maxmtu
			+" "+framing+" "+pppinfo.username+" "+pppinfo.password;

	# send the add command
	if (sys->fprint(ifd, "%s", ifc) < 0) {
		sys->raise("fail: Couldn't write /net/ipifc");
		return;
	};
}

connect(  mi: ref Modem->ModemInfo, number: string, 
		scriptinfo: ref Script->ScriptInfo, pppinfo: ref PPPInfo, logchan: chan of int)
{
	sys = load Sys Sys->PATH;

	if (pid != -1)			# yikes we are already running
		reset();

	# create a new process group
	pid = sys->pctl( Sys->NEWPGRP, nil);
	
	e := ref Sys->Exception;

	if (sys->rescue("fail*", e) == Sys->EXCEPTION) {
		lasterror = e.name;
		logchan <-= s_Error;
		sys->rescued(Sys->EXIT, nil);	# nuke the process group?
		exit;
	}

	logchan <-= s_Initialized;
	
	# open & init the modem
	modeminfo = mi;
	modem := load Modem Modem->PATH;
	if (modem == nil) {
		sys->raise("fail: Couldn't load modem module");
		return;
	}
	
	modemdev := modem->init(modeminfo);
	logchan <-= s_StartModem;
	modem->dial(modemdev, number);
	logchan <-= s_SuccessModem;

	# if script
	if (scriptinfo != nil) {
		script := load Script Script->PATH;
		if (script == nil) {
			sys->raise("fail: Couldn't load script module");
			return;
		}
		logchan <-= s_StartScript;
		script->execute( modem, modemdev, scriptinfo );
		logchan <-= s_SuccessScript;
	}
	
	#
	# Kill The Modem Monitor
	#

	killmod := load Kill Kill->PATH;
	if (killmod == nil) {
		sys->raise("fail: Couldn't load kill module");
		return;
	}
	killmod->killpid( string modemdev.pid, array of byte "kill");	
	modem = nil;	# unload modem module

	# if ppp
	if ( pppinfo != nil) 
		startppp( logchan, pppinfo );
	else
		logchan <-= s_Done;
}

reset()
{
	sys->print("reset...");
	killmod := load Kill Kill->PATH;
	if (killmod == nil) {
		sys->raise("fail: Couldn't load kill module");
		return;
	}

	if (pid != -1) {
		killmod->killpid( string pid, array of byte "killgrp");
		pid = -1;
	}

	modem := load Modem Modem->PATH;
	if (modem == nil) {
		sys->raise("fail: Couldn't load modem module");
		return;
	}
	modemdev := modem->init( modeminfo );
	modem->onhook( modemdev );
	if (modemdev.pid !=0) {
		killmod->killpid( string modemdev.pid, array of byte "kill");
		# flush the modem so we don't zombie the monitor process
		modem->send(modemdev,"AT\r");
	}
	modem = nil;

	# clear error buffer
	lasterror = nil;
}

