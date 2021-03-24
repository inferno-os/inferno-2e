implement CSplugin;

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;
include "draw.m";
include "cs.m";
include "cfgfile.m";
include "srv.m";
	srv: Srv;

include "ipsrv.m";
	is : Ipsrv;

#
#	Module:		ipservice
#	Purpose:	Connection Server Plugin For IP (DNS Name & Service Name Resolution)
#	Author:		Inferno Business Unit Members
#

#
# Front-End For CSplug-in
#

init( nil: ref Draw->Context, nil: list of string )
{
	sys = load Sys Sys->PATH;
	# First let's try loading the built-in
	srv = load Srv Srv->BUILTINPATH;
	if (srv != nil) {
		sys->print("Initializing IP service module - Using built-in srv\n");
		return;
	}

	# Merged both versions: lib/ipsrv now shared with lib/service -- obc
	# XXX can't use lib/service because of how dial-on-demand currently works.
	# resolving this issue is part of my current work plan -- dhog

	stderr = sys->fildes(2);

	sys->print("Initializing IP service module\n");

	cfg := load CfgFile CfgFile->PATH;
	if (cfg == nil) {
		sys->raise("fail: load "+CfgFile->PATH);
		return;
	}
	if (cfg->verify(defaultdnsfile, dnsfile) == nil)
		sys->raise(Econfig);
	  
	is = load Ipsrv Ipsrv->PATH;
	if (is == nil) {
		sys->raise("fail: load "+Ipsrv->PATH);
		return;
	}
	# provide compatible plug-in:
	# dial dns port, use dnsfile and iterations, no caching
	is->init(nil, Ipsrv->PATH :: "-m" :: "dnsport" :: "-s" :: srvfile :: "-d" :: dnsfile :: "-i" :: string iterations :: nil);
}

#
# IP Service->Port Mapping
#
srvfile: con "/services.txt";			# location of service->port mapping

defaultdnsfile: con "/services/dns/db";
dnsfile: con "/usr/inferno/config/dns.cfg";
iterations: con 5;

isdigit(c: int): int
{
	return (c >= '0' && c <= '9');
}

isnum(num: string): int
{
	for(i:=0; i<len num; i++)
		if(!isdigit(num[i]))
			return 0;
	return 1;
}

isipaddr(addr: string): int
{
	(nil, fields) := sys->tokenize(addr, ".");
	if(fields == nil)
		return 0;
	for(; fields != nil; fields = tl fields) {
		if(!isnum(hd fields) ||
			int hd fields < 0 ||
			int hd fields > 255)
			return 0;
	}
	return 1;
}

numeric(a: string): int
{
	i, c: int;

	for(i = 0; i < len a; i++) {
		c = a[i];
		if(c < '0' || c > '9')
			return 0;
	}
	return 1;
}

xlate(data: string):(list of string)
{
	n: int;
	l, rl : list of string;
	netw, mach, service: string;

#	if (srv == nil) {
#		(ok, stat) := sys->stat(dnsfile);
#		if (ok < 0 || stat.mtime > lastload)
#			init(nil, nil);
#	}

	(n, l) = sys->tokenize(string data, "!\n");
	if(n != 3) {
		sys->raise("fail: "+Ebadargs);
		return nil;
	}

	netw = hd l;
	mach = hd tl l;
	service = hd tl tl l;

	if(netw == "net")
		netw = "tcp";

	if(mach == "*")
		l = "" :: nil;
	else
		if(isipaddr(mach) == 0) {
			if (srv != nil)
				l = srv->iph2a(mach);
			else
				l = is->iph2a(mach);
			if(l == nil) {
				sys->raise("fail: "+Eunknown);
				return nil;
			}
		}
		else
			l = mach :: nil;		

	if(numeric(service) == 0) {
		if (srv != nil)
			service = srv->ipn2p(netw, service);
		else
			service = is->ipn2p(netw, service);
		if(service == nil) {
			sys->raise("fail: "+Eservice);
			return nil;
		}
	}	

	# Construct a return list based on translated values
 	for(; len l; l = tl l)
		rl = netw+"!"+(hd l)+"!"+service::rl;
    	
	# Reverse the list
	for(; len rl; rl = tl rl) 
		l = (hd rl)::l;	

  	return l;	
}
