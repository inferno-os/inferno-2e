implement CSplugin;

#
#	Module:		ispservice
#	Purpose:	Connection Server Plugin For Simple PPP Dial-on-Demand
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#

include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
include "lock.m";
	lock: Lock;
	Semaphore: import lock;

include "cfgfile.m";
	cfg:	CfgFile;
	ConfigFile: import cfg;

include "../ppp/modem.m";
include "../ppp/script.m";
include "../ppp/pppclient.m";
	ppp: PPPClient;
include "../ppp/pppgui.m";

include "cs.m";

include "qidcmp.m";
qi : Qidcmp;
Cdir : import qi;

#
# Globals
# 

	context:		ref Draw->Context;
	modeminfo:		ref Modem->ModemInfo;
	pppinfo:		ref PPPClient->PPPInfo;
	scriptinfo:		ref Script->ScriptInfo;
	isp_number:		string;						# should be part of pppinfo
	isp_lock:		ref Lock->Semaphore;
	lastload:		int;
	lastCdir:		ref Qidcmp->Cdir;

#
# Constants (for now)
#

DEFAULT_ISP_DB_PATH:	con "/services/ppp/isp.cfg";	# contains pppinfo & scriptinfo
MODEM_DB_PATH:	con	"/services/ppp/modem.cfg";			# contains modeminfo
ISP_DB_PATH:	con "/usr/inferno/config/isp.cfg";		# contains pppinfo & scriptinfo
ISP_RETRIES:	con 5;

getcfgstring(c: ref ConfigFile, key: string) :string
{
	ret : string;

	retlist := c.getcfg(key);
	if (len retlist == 0)
		return nil;

	for (;len retlist; retlist = tl retlist)
		ret+= (hd retlist) + " ";
	
	return ret[:(len ret-1)];		# trim the trailing space
}

configinit()
{
	mi:	Modem->ModemInfo;
	pppi: PPPClient->PPPInfo;
	info: list of string;

	cfg = load CfgFile CfgFile->PATH;
	if (cfg == nil) {
		sys->raise("fail: Couldn't load cfgfile module");
		return;
	}

	# Modem Configuration
	
	modemcfg := cfg->init(MODEM_DB_PATH);
	if (modemcfg == nil) {
		sys->raise("fail: Couldn't load modem configuration file: "+MODEM_DB_PATH);
		return;
	}
	modeminfo = ref mi;

	modeminfo.path = getcfgstring(modemcfg, "PATH");
	modeminfo.init = getcfgstring(modemcfg, "INIT");
	modeminfo.errorcorrection = getcfgstring(modemcfg,"CORRECT");
	modeminfo.compression = getcfgstring(modemcfg,"COMPRESS");
	modeminfo.flowctl = getcfgstring(modemcfg,"FLOWCTL");
	modeminfo.rateadjust = getcfgstring(modemcfg,"RATEADJ");
	modeminfo.mnponly = getcfgstring(modemcfg,"MNPONLY");

	cfg->verify(DEFAULT_ISP_DB_PATH, ISP_DB_PATH);
	(ok, stat) := sys->stat(ISP_DB_PATH);

	if (qi != nil && lastCdir == nil)
	  lastCdir = ref Cdir(ref stat, 0);
	sys->print("cfg->init(%s)\n", ISP_DB_PATH);

	lastload = stat.mtime;
	# ISP Configuration
	pppcfg := cfg->init(ISP_DB_PATH);
	if (pppcfg == nil) {
		sys->raise("fail: Couldn't load ISP configuration file: "+ISP_DB_PATH);
		return;
	}
	pppinfo = ref pppi;
	isp_number = getcfgstring(pppcfg, "NUMBER");
	pppinfo.ipaddr = getcfgstring(pppcfg,"IPADDR");
	pppinfo.ipmask = getcfgstring(pppcfg,"IPMASK");
	pppinfo.peeraddr = getcfgstring(pppcfg,"PEERADDR");
	pppinfo.maxmtu = getcfgstring(pppcfg,"MAXMTU");
	pppinfo.username = getcfgstring(pppcfg,"USERNAME");
	pppinfo.password = getcfgstring(pppcfg,"PASSWORD");

	info = pppcfg.getcfg("SCRIPT");
	if (info != nil) {
		scriptinfo = ref Script->ScriptInfo;
		scriptinfo.path = hd info;
		scriptinfo.username = pppinfo.username;
		scriptinfo.password = pppinfo.password;
	} else
		scriptinfo = nil;

	info = pppcfg.getcfg("TIMEOUT");
	if (info != nil) {
		scriptinfo.timeout = int (hd info);
	}
}

#
# Parts of the following two functions could be generalized
#

ipaddrp(a: string): int
{
	i, c, ac, np : int = 0;
 
	for(i = 0; i < len a; i++) {
		c = a[i];
		if(c >= '0' && c <= '9') {
			np = 10*np + c - '0';
			continue;
		}
		if (c == '.' && np) {
			ac++;
	 		if (np > 255)
				return 0;
			np = 0;
			continue;
		}
		return 0;
	}
	return np && np < 256 && ac == 3;
}

# check if there is an existing PPP connection
connected(): int
{
	ifd := sys->open("/net/ipifc", Sys->OREAD);
	if(ifd == nil) {
		sys->raise("fail: can't open /net/ipifc");
		return 0;
	}

	d := array[10] of Sys->Dir;
	buf := array[1024] of byte;

	for (;;) {
		n := sys->dirread(ifd, d);
		if (n <= 0)
			return 0;
		for(i := 0; i < n; i++)
			if(d[i].name[0] <= '9') {
				sfd := sys->open("/net/ipifc/"+d[i].name+"/status", Sys->OREAD);
				if (sfd == nil)
					continue;
				ns := sys->read(sfd, buf, len buf);
				if (ns <= 0)
					continue;
				(nflds, flds) := sys->tokenize(string buf[0:ns], " \t\r\n");
				if(nflds < 4)
					continue;
				if (ipaddrp(hd tl tl flds))
					return 1;
			}
	}
}

init(ctxt: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;

	# use a strategy that works with namespace
	qi = load Qidcmp Qidcmp->PATH;
	if (qi == nil) sys->print("load %s %r\n", Qidcmp->PATH);
	else qi->init(ctxt, nil);

	sys->print("Initializing ISP Dial-On-Demand service module\n");

	ppp = load PPPClient PPPClient->PATH;
	if (ppp == nil) {
		sys->raise("fail: Couldn't load ppp module");
		return;
	}

	lock = load Lock Lock->PATH;
	if (lock == nil) {
		sys->raise("fail: Couldn't load lock module");
		return;
	}

	# Contruct Config Tables During Init - may want to change later
	#	for multiple configs (Software Download Server versus ISP)
	configinit();	
	context = ctxt;

	# set us up a lock
	isp_lock = lock->init();
}

cfg_changep(dir : ref Sys->Dir) : int
{
	if (lastCdir != nil) return lastCdir.cmp(dir);
	# fall back to insufficient check (namespace!)
	else return (dir.mtime > lastload);
}
 
dialup_cancelled: int;      
xlate(data: string):(list of string)
{
	e := ref Sys->Exception;
	if (sys->rescue("*",e) == Sys->EXCEPTION) {
		sys->rescued(Sys->ONCE, nil);
		isp_lock.release();
		sys->raise(e.name);
	} 
	dialup_cancelled = 0;
	isp_lock.obtain();
	# "dialup_cancelled" may be set by the previous call to xlate().
	# any client waiting for isp_lock will be told "dialup cancelled",
	# i.e., "dialup cancelled" is propagated to outstanding xlate() calls;
	# any client calling xlate() later will proceed as usual. 
	if (dialup_cancelled == 1)
		sys->raise("fail: dialup cancelled");		
	(ok, stat) := sys->stat(ISP_DB_PATH);
	if (ok < 0 || cfg_changep(ref stat))
		configinit();
	if (!connected()) {
		resp_chan : chan of int;
		logger := chan of int;
		pppgui := load PPPGUI PPPGUI->PATH;
		for (count :=0; count < ISP_RETRIES; count++) {
			resp_chan = pppgui->init(context, logger, ppp, nil);
			spawn ppp->connect(modeminfo, isp_number, scriptinfo, pppinfo, logger);
			x := <-resp_chan;
			if (x > 0) {
				if (x == 1) {
					isp_lock.release();
					return data :: nil;
				} else	{		# user canceled dial-in
					dialup_cancelled = 1;
					sys->raise("fail: dialup cancelled");
				}
			}
			# else connect failed, go around loop to try again
		}
		sys->raise("fail: dialup failed");
	}
	isp_lock.release();
	return data :: nil;	
}
