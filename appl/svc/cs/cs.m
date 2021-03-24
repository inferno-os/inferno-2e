#
#	Module:		CSplugin
#	Purpose:	Connection Server plug-in for processing requests
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#

CSplugin: module
{
	# init should load configuration information
	init:	fn(ctxt: ref Draw->Context, nil: list of string);

	# xlate actually performs the translation - raising exceptions on errors
	xlate:	fn(data: string): (list of string);
};

#
# Error Codes (Raisable from inside plugin)
#

Ebadargs:	con "bad format request";
Eunknown:	con "unknown host";
Eservice:	con "bad service name";
Econfig:	con "couldn't load config file";
