#
# 	Module:		Shannon
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#	Purpose:	Initial DIS thread
#				Setup the namespace and initial configuration
#

implement Shannon;

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

stderr:	ref FD;						# standard error FD
cfd:	ref FD;						# serial port control FD
dfd:	ref FD;						# serial port data FD
dirs:=	array[] of { "dis", "fonts", "icons", "usr", "services" };
									# directories to mount from the server
#
# Constants
#

#
# The following are for the purposes of remote debug setup
#
debug:	con 9;						# debug level
use_local: con 1;
use_remote: con 0;
logfile:con "#c/null";				# debug log file
host:	con "shannon";				# device's hostname	 	
login:	con "shannon";				# device's log-in name 
passwd:	con "infernodemo";			# device's password		
port:	con "eia0";					# Serial Port to use for PPP 
baud:	con "b38400";				# Baud Rate to connect at
lip:	con "135.3.67.3";			# device's ip address
netmask:con "255.255.255.255";		# device's netmask
rip:	con "135.3.60.46";			# router IP address
sip:	con "135.3.60.46";			# file server's IP address
rmount:	con "/n/server";			# file server mount point
lmount: con "/n/local";
start:	con "/dis/wm/wm.dis";		# initial application (usually a wm)
imount:	con "/dis/imount.dis";		# path to mount (insecure for debug)
startpt:con "/usr/inferno";			# directory to start initial DIS code in
MAX_RETRIES: con 10;				# maximum attempts on a match before fail
TIMEOUT1: con 5000;					# initial match timeout
TIMEOUT2: con 10000;				# secondary match timeout

#
# My module definition
#

Shannon: module
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



# 
# Main Routine
#

init()
{
	# 
	# Load the necessary modules
	#

	sys = load Sys Sys->PATH;
	timedio = load TimedIO TimedIO->PATH;
	if (debug > 0)
		stderr = sys->fildes(2);
	else 
		stderr = sys->open(logfile,Sys->OWRITE);

	namespace_init();		# setup initial namespace
	setsysname();			# setup system name
	if(use_local) {
		mountkfs("#W/flash0fs", "fs");
		dobind("#Kfs", "/n/local", sys->MBEFORE);
		dobind("#Kfs", "/", sys->MBEFORE);
	}
	if(use_remote) {
		logon();		# logon to PPP debug host
		addinterface();		# initialize PPP
		addroute();		# setup router as default route
		dialstyx();		# dial the file server
		rootfs();		# bind the root file system
	}
	namespace_local();	# bind back the local namespace
	sys->print("Starting shell...\n");
	shell();		# start initial application
}

#
# Bind root file systtem from server
#

rootfs()
{
	l := rmount + "/";

	dobind( l, "/", sys->MBEFORE );
}

#
# Setup initial namespace
#

namespace_init()
{
	dobind("#t", "/net", sys->MREPL);	# serial line
	dobind("#I", "/net", sys->MAFTER);	# IP
	dobind("#c", "/dev", sys->MAFTER);	# console
	dobind("#p", "/prog", sys->MREPL);	# prog device
}

#
# Setup local namespace after network mounts
#

namespace_local()
{
	dobind("#t", "/net", sys->MREPL);	# serial line
	dobind("#I", "/net", sys->MAFTER);	# IP
	dobind("#p", "/prog", sys->MREPL);	# prog device
	dobind("#d", "/dev", sys->MBEFORE); # draw device
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
# Log into the PPP debug server
#

logon()
{
	d := "/net/" + port;
	c := d + "ctl";

	if (debug)
		sys->fprint(stderr, "data = %s, ctl = %s\n", d, c);

	#
	# Open up the inferface & initialize to baud rate
	#

	dfd = sys->open(d, sys->ORDWR);
	if (dfd == nil) {
		sys->fprint(stderr, "could not open %s: %r", d);
		hang();
	}

	cfd = sys->open(c, sys->OWRITE);
	if (cfd == nil) {
		sys->fprint(stderr, "could not open %s: %r", c);
		hang();
	}

	b := array of byte baud;
	if (sys->write(cfd, b, len b) < 0) {
		sys->fprint(stderr, "could not write %s: %r", c);
		hang();
	}

	# 
	# Send a break to wake up the debug server & then log in
	#

	sys->fprint(cfd, "break\n");	
	
	while (1) {
		sys->fprint(stderr, "\nWaiting for login prompt...\n");
		if (match(dfd, cfd, "in:")) {
			sys->fprint(dfd, "%s\r", login);
			sys->fprint(stderr, "\nWating for password prompt...\n");
			if (match(dfd, cfd, "rd:")) {
				sys->fprint(dfd, "%s\r", passwd);
				sys->fprint(stderr,"\nWaiting for PPP signature...\n");
				if (match(dfd,cfd, "~"))
					break;
			}
		}
	}
	if (debug)
		sys->fprint(stderr, "\nLogged in\n");
}


#
# Mount KFS filesystem
#

mountkfs(devname: string, fsname: string)
{
	fd := sys->open("#Kcons/kfsctl", sys->OWRITE);
	if (fd == nil) {
		sys->fprint(stderr, "could not open #Kcons/kfsctl: %r");
		hang();
	}
	b := array of byte ("filsys " + fsname + " " + devname);
	if (sys->write(fd, b, len b) < 0) {
		sys->fprint(stderr, "could not write #Kcons/kfsctl: %r");
		hang();
	}
}

#
# Setup PPP interface
#

addinterface()
{
	fd := sys->open("/net/ipifc", sys->OWRITE);
	if (fd == nil) {
		sys->fprint(stderr, "could not open /net/ipifc: %r");
		hang();
	}
	b := array of byte ("add serial /net/" + port + " " + lip + " "+netmask+" " + rip);
	if (debug)
		sys->fprint(stderr, "%s\n", string b);
	if (sys->write(fd, b, len b) < 0) {
		sys->fprint(stderr, "could not write /net/ipifc: %r");
		hang();
	}
}

#
# Setup default route
#

addroute()
{
	fd := sys->open("/net/iproute", sys->OWRITE);
	if (fd == nil) {
		sys->fprint(stderr, "could not open /net/iproute: %r");
		hang();
	}
	b := array of byte ("add 0.0.0.0 0.0.0.0 " + rip);
	if (debug)
		sys->fprint(stderr, "%s\n", string b);
	if (sys->write(fd, b, len b) < 0) {
		sys->fprint(stderr, "could not write /net/iproute: %r");
		hang();
	}
}

#
# Dial the file server
#

dialstyx()
{
	m := load Sh imount;
	if (m == nil) {
		sys->fprint(stderr, "could not load /dis/imount.dis: %r\n");
		hang();
	}
	if (debug)
		sys->fprint(stderr, "import filesystem from %s\n", sip);
	m->init(nil, "imount" :: (("tcp!" + sip + "!6666") :: (rmount :: nil)));
}

#
# Start the initial application
#

shell()
{
	sh := load Sh start;
	if ( sh == nil ) {
		sys->fprint(stderr, "could not load " + start + ": %r\n");
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

#
# Expect mechanism
#

match(f: ref FD, c: ref FD, s: string) :int
{

	b := array[2] of byte;

	for (tries := 0; tries < MAX_RETRIES; ) {
	
		case timedio->read(f, b, 1,TIMEOUT1) {
			timedio->ERROR => {
				if (debug)
					sys->fprint(stderr, "read error: %r\n");
				hang();
			}
			timedio->TIMEOUT => {
				if (debug)
					sys->fprint(stderr,"[timeout]");
				sys->fprint(c, "break\n");
				sys->fprint(f, "\r");
			}
			* => {
				if (debug)
					sys->fprint(stderr,"%c", int b[0]);
			}
		}
		
head:	
		while (int b[0] == s[0]) {	# match first character
			i := 1;
			while (i < len s) {		# match the rest
				case timedio->read(f, b, 1, TIMEOUT2) {
					timedio->ERROR => {
						if (debug)
							sys->fprint(stderr, "read error: %r\n");
						hang();
					}
					timedio->TIMEOUT => {
						if (debug)
							sys->fprint(stderr, "[timeout 2]");
						sys->fprint(c, "break\n");
						sys->fprint(f, "\r");
						continue head;
					}
					* => {
						if (debug)
							sys->fprint(stderr,"%c", int b[0]);
					
						if (int b[0] != s[i]) 
							continue head;
						i++;
					}
				}
			}
			return 1; # Success
		}
		if (int b[0] == 10)			# increment tries if we receive
			tries++; 				# a newline
		
	}
	if (debug)
		sys->fprint(stderr,"Retries Exceeded\n");
	return 0; # Failure
}

hang()
{
	c := chan of int;
	<- c;
}
