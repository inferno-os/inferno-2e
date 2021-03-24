implement PPPTest;

include "sys.m";
	sys:	Sys;
include "draw.m";

include "lock.m";
include "modem.m";
include "script.m";
include "pppclient.m";
include "pppgui.m";

PPPTest: module {
	init:	fn(nil: ref Draw->Context, args: list of string);
};

init( ctxt: ref Draw->Context, nil: list of string )
{
	sys = load Sys Sys->PATH;
	
	mi:	Modem->ModemInfo;
	pi:	PPPClient->PPPInfo;
#	si:	Script->ScriptInfo;

	mi.path = "/dev/modem";
	mi.init = "AT&F0SS6=10&D2\\N3X4";

	#si.path = "rdid.script";
	#si.username = "ericvh";
	#si.password = "foobar";
	#si.timeout = 60;

	pi.username = "tester";
	pi.password = "foobar";

	ppp := load PPPClient PPPClient->PATH;

	logger := chan of int;

	spawn ppp->connect( ref mi, "8747", nil, ref pi, logger );
	
	pppgui := load PPPGUI PPPGUI->PATH;
	respchan := pppgui->init( ctxt, logger,ppp, nil);

	event := 0;
	while (1) {
		event =<- respchan;
		sys->print("GUI event received: %d\n",event);
		if (event) {
			sys->print("success");
			exit;
		} else {
			sys->raise("fail: Couldn't connect to ISP");
		}
	}	
}
