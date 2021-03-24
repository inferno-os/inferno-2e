#
# Phone -
#
# A simple command line program to make outgoing calls using the 
# tel driver.  Implemented to test the Sword/UMEC drivers.
#
implement Phone;

include "sys.m";
	sys: Sys;
include "draw.m";

Phone: module
{
	init:	fn(nil: ref Draw->Context, argv: list of string);
};

usage()
{
	sys->print("Usage: phone [-h] <phone number>\n");
	exit;
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdin := sys->fildes(0);

	if (argv == nil)
		usage();
	argv = tl argv;
	if (argv == nil)
		usage();
	ph_no := hd argv;
	cons := "speaker";

	if (ph_no[0] == '-') {
		cons = "handset";
		argv = tl argv;
		if (argv == nil)
			usage();
		ph_no = hd argv;
	}

	telfd := sys->open("/tel/clone", Sys->ORDWR);
	if (telfd == nil) {
		sys->print("tel: %r\n");
		exit;
	}

	consfd := sys->open("/cvt/cons/clone", Sys->ORDWR);
	if (consfd == nil) {
		sys->print("cons: %r\n");
		exit;
	}

	# take the speakerphone off hook and dial the number
	sys->fprint(consfd, "volume %s 100", cons);
	sys->fprint(consfd, "off-hook %s", cons);
	sys->fprint(telfd, "connect %s", ph_no);

	# wait for the user to finish jabbering ...
	sys->print("Press <ENTER> to hang up ...");
	buf := array[64] of byte;
	sys->read(stdin, buf, len buf);

	# place the speakerphone on hook and hangup the line
	sys->fprint(telfd, "hangup");
	sys->fprint(consfd, "on-hook %s", cons);
}
