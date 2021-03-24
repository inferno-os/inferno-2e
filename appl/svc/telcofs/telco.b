implement Telco;

include "sys.m";
	sys: Sys;
	FD:		import Sys;
include "draw.m";
	Context:		import Draw;
include "styx.m";
	telcofs:	StyxServer;
	styx:		Styx;
include "telco.m";

stderr:	ref FD;

init(nil: ref Context, argl: list of string)
{
	sys = load Sys Sys->PATH;
	sys->pctl(Sys->FORKFD|Sys->NEWPGRP, nil);
	argl = tl argl;
	e := initr(argl);
	if (e != nil)
		err(e);
}

initr(argl: list of string): string
{
	if (sys == nil)
		sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	if(argl == nil)
		return nil;
	styx = load Styx Styx->PATH;
	if (styx == nil)
		return "could not load " + Styx->PATH;
	telcofs = load StyxServer StyxServer->TELCOFSPATH;
	if (telcofs == nil)
		return "could not load " + StyxServer->TELCOFSPATH;
	spawn telco(argl);
	sys->sleep(2000);		# allow thread time to set namespace up
	return nil;
}

shutdown()
{
	if (telcofs == nil)
		return;
	telcofs->shutdown();
}

err(msg: string)
{
	sys->fprint(stderr, "telco: %s: %r\n", msg);
	exit;
}

telco(devs: list of string)
{
	e := styx->serve(devs, telcofs);
	if (telcofs != nil)
		telcofs->reply->shutdown();
}