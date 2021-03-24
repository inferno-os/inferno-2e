implement Kfscmd;

#
#	Module:		kfscmd
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#	Purpose:	Easy interface to #Kcons/kfscons
#		
#	Usage:		kfscmd [-n<fsname>] <cmd>
#

include "sys.m";
	sys:	Sys;
	stderr:	ref Sys->FD;

include "draw.m";
include "arg.m";
	arg: Arg;

Context: import Draw;

Kfscmd: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

usage()
{
	sys->fprint(stderr,"\tkfscmd: usage:\n");
	sys->fprint(stderr,"\t\tkfscmd [-n<fsname>] <cmd>\n");
	exit;
}

init(nil: ref Context, argv: list of string)
{
	cfs: string;

	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	arg = load Arg Arg->PATH;
	if (arg == nil)
		sys->fprint(stderr, "can't load %s: %r",Arg->PATH);



	arg->init(argv);
	while((c := arg->opt()) != 0)
	case c {
	'n' =>
		cfs = arg->arg();
		if(cfs == nil)
			usage();
	* =>
		usage();
	}
	argv= arg->argv();
	if (len(argv) != 1) 
		usage();

	dfd := sys->open("#Kcons/kfscons",Sys->OWRITE);
	if (dfd != nil) {
		if (cfs != nil)
			sys->fprint(dfd,"cfs %s\n",cfs);
		sys->fprint(dfd,"%s\n",hd (tl argv));
	} else
		sys->fprint(stderr,"[kfscmd]: couldn't open KFS console\n");
}
