#
# 	Module:		Sword
#	Authors:	evh
#	Purpose:	Initial DIS thread
#			Setup the namespace and initial configuration
#

implement Sword;

#
# Included Modules
#

include "sys.m";
	sys:	Sys;
	FD:	import sys;

include "draw.m";
	draw:	Draw;
	Context: import Draw;

include "timedio.m";
	timedio : TimedIO;

#
# Globals
#
KFS_READONLY:	con "ronly";
KFS_READWRITE:	con "";
stderr:	ref FD;						# standard error FD
cfd:	ref FD;						# serial port control FD
dfd:	ref FD;						# serial port data FD

#
# Constants
#

#
# The following are for the purposes of remote debug setup
#
debug:	con 9;						# debug level
logfile:con "#c/null";				# debug log file
host:	con "sword";				# devices hostname	 	
start: con "/dis/wm/tblogon.dis";			
startpt:con "/usr/inferno";			# directory to start initial DIS code in


#
# My module definition
#

Sword: module
{
	init:	fn();
};

#
# A module definition to use when loading others
#

Sh: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

echoto(fname, str: string): int
{
	fd := sys->open(fname, Sys->OWRITE);
	if(fd == nil) {
		sys->print("%s: %r\n", fname);
		return -1;
	}
	x := array of byte str;
	if(sys->write(fd, x, len x) == -1) {
		sys->print("write: %r\n");
		return -1;
	}
	return 0;
}

srv()
{
	sys->print("remote debug srv...");
	if(echoto("#t/eia0ctl", "b38400") < 0)
		return;

	fd := sys->open("/dev/eia0", Sys->ORDWR);
	if (fd == nil) {
		sys->print("eia data open: %r\n");
		return;
	}
	if (sys->export(fd, Sys->EXPASYNC) < 0) {
		sys->print("export: %r\n");
		return;
	}
	sys->print("ok\n");
}

# 
# Main Routine
#

init()
{
	# 
	# Load the necessary modules
	#

	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	timedio = load TimedIO TimedIO->PATH;
	if (debug > 0)
		stderr = sys->fildes(2);
	else 
		stderr = sys->open(logfile,Sys->OWRITE);

	namespace_init();		# setup initial namespace
	setsysname();			# setup system name

	mountkfs("#W/flash0fs", "fs", KFS_READONLY);

	if (!failsafe()) {
	  sys->print("Found a file system...");
	  if (sys->open("/n/local", Sys->OREAD) != nil)
	    dobind("#Kfs", "/n/local", sys->MREPL);
	  dobind("#Kfs", "/", sys->MBEFORE);
	}

	namespace_local();	# bind back the local namespace

	# Would the real cs.dis please stand up
#	dobind("/dis/svc/cs.dis","/dis/lib/cs.dis",sys->MREPL);

	mountsfs();
	bindsfs();

	phonestuff();
	
	# bind SSL directory back on /n
	dobind("#/n", "/n", sys->MAFTER);

	# bind the image converters
#	dobind("/dis/wm/browser/readimg.dis", "/dis/wm/browser/readgif.dis", Sys->MREPL);
#	dobind("/dis/wm/browser/readimg.dis", "/dis/wm/browser/readjpg.dis", Sys->MREPL);

	remotedebug := sysenv("#c/sysenv", "remotedebug");
	if(remotedebug == "1") {
		sys->print("Exporting file system for remote debug\n");
		srv();
	}

	sys->print("Starting shell...\n");
	shell();		# start initial application
}

plumb()
{
	plumber := load Sh "/dis/lib/plumb.dis";
	spawn plumber->init(nil, nil);
}

phonestuff()
{
	dobind("#P/tel", "/tel", sys->MREPL);
#	dobind("#P/msgflash", "/cvt/msgflash", sys->MREPL);	
	dobind("#P/cons", "/cvt/cons", sys->MREPL);
	dobind("#P/tgen", "/cvt/tgen", sys->MREPL);
}

#
# Setup initial namespace
#

namespace_init()
{
	dobind("#t", "/net", sys->MREPL);	# serial line
	dobind("#I", "/net", sys->MAFTER);	# IP
	dobind("#c", "/dev", sys->MAFTER);	# console
	(k, nil) := sys->stat("#k");
	if (k >= 0)
	  dobind("#k", "/dev", sys->MBEFORE); 	# keyboard device
	dobind("#p", "/prog", sys->MREPL);	# prog device
}

consprint( x: int )
{
	ccfd := sys->open("#c/sysctl",Sys->OWRITE);
	cmd := array of byte "console restore";
	if (x)
		sys->fprint(ccfd, "console print");
	else
		sys->fprint(ccfd, "console restore");

}

#
# Setup local namespace after network mounts
#

namespace_local()
{
	dobind("#t", "/net", sys->MREPL);	# serial line
	dobind("#I", "/net", sys->MAFTER);	# IP
#	dobind("#J", "/net", sys->MAFTER);	# i2c
	dobind("#p", "/prog", sys->MREPL);	# prog device
	dobind("#d", "/dev", sys->MREPL); 	# draw device
	dobind("#t", "/dev", sys->MAFTER);	# serial line
	dobind("#c", "/dev", sys->MAFTER); 	# console device
	(k, nil) := sys->stat("#k");
	(m, nil) := sys->stat("#m");
	(o, nil) := sys->stat("#O");
	if (k >= 0)
	  dobind("#k", "/dev", sys->MBEFORE);	# keyboard device
	if (m >= 0)
	  dobind("#m", "/dev", sys->MAFTER);	# Modem
	else if (o >= 0)
	  dobind("#O", "/dev", sys->MAFTER);	# Modem
	else sys->fprint(stderr, "fail: modem not found\n");
	dobind("#T","/dev",sys->MAFTER);	# Touchscreen
	dobind("#//dis", "/dis", sys->MAFTER);
	dobind("#//dis/wm", "/dis/wm", sys->MAFTER);
	dobind("#//dis/lib", "/dis/lib", sys->MAFTER);
#	dobind("#//dis/wm", "/dis/wm", sys->MBEFORE);
#	dobind("#//dis/lib", "/dis/lib", sys->MBEFORE);
	
#	readimg := sysenv("#c/sysenv", "readimg");
#	if((readimg == nil) || (readimg == "kernel")) {
#		sys->print("binding kernel readimg library...\n");
#		dobind("/dis/lib/readimg.dis", "/dis/lib/readgif.dis",
#				sys->MREPL); 
#		dobind("/dis/lib/readimg.dis", "/dis/lib/readjpg.dis",
#				sys->MREPL);
#	}

}

#
# Bind wrapper which reports errors & spins
#

dobind(f, t: string, flags: int)
{
	if (sys->bind(f, t, flags) < 0) {
		sys->fprint(stderr, "bind(%s, %s, %d) failed: %r\n", f, t, flags);
		# hang();
	}
}

#
# Set our system name
#

setsysname()
{
	if (debug)
		sys->fprint(stderr, "host = %s\n", host);
	f := sys->open("/dev/sysname", sys->OWRITE);
	if (f == nil) {
		sys->fprint(stderr, "open /dev/sysname failed: %r\n");
		hang();
	}
	b := array of byte host;
	if (sys->write(f, b, len b) < 0) {
		sys->fprint(stderr, "write /dev/sysname failed: %r\n");
		hang();
	}
}


#
# Mount KFS filesystem
#

# Perform command on kfscons.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_cons(cmd : string) : int
{
	return kfs_cmd("cons", cmd);
}

# Perform command on kfsctl.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_ctl(cmd : string) : int
{
	return kfs_cmd("ctl", cmd);
}

# Send command to kfs.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_cmd(file : string, cmd : string) : int
{
	fd := sys->open("#Kcons/kfs" + file, sys->OWRITE);
	if (fd == nil) {
		sys->fprint(stderr, "could not open #Kcons/kfs%s: %r\n", file);
		return 0;
	}
	b := array of byte cmd;
	if (sys->write(fd, b, len b) < 0) {
		sys->fprint(stderr, "#Kcons/kfs%s: %r\n", file);
		return 0;
	}
	return 1;
}

# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

reamkfs(devname, fsname: string) :int
{
	return kfs_ctl("ream " + fsname + " " + devname);
}


# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

mountkfs(devname, fsname, ronly : string) :int
{
	return kfs_ctl("filsys " + fsname + " " + devname + " " + ronly);
}


bindsfs()
{
    ### data flash
    dobind("#Ksfs", "/data", (sys->MREPL | sys->MCREATE));
    dobind("/data", "/usr/inferno", (sys->MBEFORE | sys->MCREATE));
 	dobind("/data", "/usr/inferno/config", sys->MREPL);
}

mountsfs()
{
  sys->print("Mounting serial data flash file system -readwrite\n");
  if (!mountkfs("#W/flash0datafs", "sfs", KFS_READWRITE)) {
    if (!reamkfs("#W/flash0datafs", "sfs")) {
   		sys->print("Data Flash: cannot initialize");
		return;
    }
  }
  if (kfs_cons("cfs sfs"))
    kfs_cons("flashwrite");
}


#
# Bimodal kernel failsafe management
#

Fail: module
{
	PATH : con "/dis/util/fail.dis";

        init:	fn(ctxt: ref Draw->Context, argv: list of string);
	attempt : fn(app : string, max : int);
};
fs : Fail;

ldfail()
{
	if (fs == nil) {
	  fs = load Fail Fail->PATH;
	  if (fs == nil)
	    sys->fprint(sys->fildes(2), "load %s %r\n", Fail->PATH);
	  else
	    fs->init(nil, nil);
	}
}

failsafe() : int
{
	fsp := sysenv("#c/sysenv", "failsafe");
	if (fsp == "0") fsp = nil;
	if (fsp == nil)
	  sys->print("Bimodal kernel in Kfs mode...\n");
	if (fsp == nil && sys->open(start, Sys->OREAD) != nil && sys->open("#Kfs/"+start, Sys->OREAD) == nil) {
	  sys->print("Incomplete file system - Using failsafe mode\n");
	  ldfail();
	  if (fs != nil) fs->attempt(nil, 0);	# Force failsafe mode
	  fsp = "1";
	}
	return fsp != nil;
}

#
# Start the initial application
#

shell()
{
	shenv := sysenv("#c/sysenv", "shell");
	if (shenv == nil) {
		shenv = start;
		ldfail();
		if (fs != nil) fs->attempt(start, 3);	# Count start failures
	}

	psh := load Sh "/dis/config/touchcal.dis";
	if (psh == nil)
		psh = load Sh "/dis/touchcal.dis";
	if (psh == nil) {
		sys->print("could not load touchcal:%r\n");
	} else {
		spawn psh->init(nil, nil);
	}

	sh := load Sh shenv;
	if ( sh == nil ) {
		sys->fprint(stderr, "could not load %s: %r\n", shenv);
		sys->fprint(stderr, "trying shell instead...\n");
		sh = load Sh "/dis/sh.dis";
		if ( sh == nil ) {
			sys->fprint(stderr, "could not load /dis/sh.dis: %r\n");
			hang();
		}
	}
	sys->chdir(startpt);
	sh->init(nil, "wm" :: nil);
}



hang()
{
	c := chan of int;
	<- c;
}



sysenv(filename,paramname : string) : string {
    buf : array of byte;

    fd       := sys->open(filename, sys->OREAD);
    if (fd == nil)
	return(nil);
    buf       = array[4096] of byte;
    nb       := sys->read(fd,buf,4096);
    sbuf     := string buf;
    (nfl,fl) := sys->tokenize(sbuf,"\n");
    while (fl != nil) {
	pair     := hd fl;
	(npl,pl) := sys->tokenize(pair,"=");
	if (npl > 1) {
	    if ((hd pl) == paramname)
		return((hd (tl pl)));
	}
	fl = tl fl;
    }
    return(nil);
}

