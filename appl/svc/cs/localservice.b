implement CSplugin;

#
#	Module:		localservice
#	Purpose:	Connection Server Plugin For Local Name Resolution
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#

include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
include "cfgfile.m";
	cfg:	CfgFile;
	ConfigFile: import cfg;

include "cs.m";

csdb:	ref ConfigFile;

DB_PATH:	con "/services/cs/db";

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	cfg = load CfgFile CfgFile->PATH;
	if (cfg == nil) {
		sys->raise("fail: Couldn't load cfgfile module");
		return;
	}
	sys->print("Initializing local service module\n");
	csdb = cfg->init(DB_PATH);	
}

xlate(data: string):(list of string)
{
	return csdb.getcfg(data);
}
