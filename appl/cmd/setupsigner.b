implement SetupSigner;


include "sys.m";
	sys: Sys;
	Dir: import Sys;
include "draw.m";
include "sh.m";
include "daytime.m";
	daytime: Daytime;
include "readdir.m";
include "regex.m";
	regex: Regex;
	Re, compile, execute: import regex;

stderr, stdin, stdout:	ref Sys->FD;

signerkey:	con "/keydb/signerkey";
passwordfile:	con "/keydb/password";


SetupSigner: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};


init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdin = sys->fildes(0);
	stderr = sys->fildes(2);

	# Check to see if /keydb/signerkey already exists
	(i, nil) := sys->stat(signerkey);

	# Skip creating signerkey if it exists
	if (i==0) {
		sys->print("\n/keydb/signerkey already exists. A new signerkey file will not be created,\n" +
			"since this could invalidate existing certificates. (See the command \n" +
			"lib/createsignerkey for more information).\n\n");

		# Create passwords
		password();

		# Start lib/srv
		services();

		return;	
	}

	# If signerkey doesn't already exist, create it
	signer();

	# Then create passwords
	password();

	# And start lib/srv
	services();
}

# Routine to create /keydb/signerkey
signer() 
{
	# Before attempting to run lib/createsignerkey make sure
	# you can write to /keydb by creating an empty signerkey
	# file. It will be overwritten by createsignerkey. This is
	# not as elegant as checking username v. uid and gid and
	# permissions but it is simpler

	if(sys->create(signerkey, sys->OWRITE, 8r666) == nil) {
		sys->print("Make sure you have write permission on /keydb before continuing.\n\n");
		exit;
	}

	# Default signer id argument to contents of /dev/sysname
	signerid := sysname();

	sys->print("\nCreating signerkey file with an id of %s and no expiration date (See the\n"+
		"command lib/createsignerkey for more information). It will take a while to\n"+
		"generate these keys.....\n\n", signerid);

	csk := load Command "/dis/lib/createsignerkey.dis";
	if (csk == nil) {
		sys->print("Couldn't load /dis/lib/createsignerkey.dis. Cannot continue.\n");
		exit;
	}
	csk->init(nil, "createsignerkey" :: signerid :: nil);
}

#Routine to add or modify passwords
password()
{
	sys->print("Administer user passwords (See command changelogin).\n");

	# calculate and display password expiration date. This will be 
	# default for all passwords.
	daytime = load Daytime Daytime->PATH;
	if(daytime == nil) {
		sys->fprint(stderr, "Could not load Daytime module\n");
		exit;
	}
	
	now := daytime->now();
	tm := daytime->local(now);
	tm.sec = 59;
	tm.min = 59;
	tm.hour = 23;
	expsecs := now + 365*24*60*60;
	otm := daytime->local(expsecs);
	sys->print("Passwords will expire on %2.2d/%2.2d/%4.4d\n", otm.mon+1, otm.mday, otm.year+1900);

	# addpassword.dis is a modified version of changelogin. 
	# It defaults the expiration date and free form comments. 
	chlogin := load Command "/dis/addpassword.dis";
	if (chlogin == nil) {
		sys->fprint(stderr, "setupsigner: couldn't load /dis/addpassword.dis for passwords: %r\n");
		exit;
	}

	# check to see if a password file exists,
	# if not, create a password entry for the
	# current user

	(passwdf, nil) := sys->stat(passwordfile);
	if (passwdf != 0){
		sys->print("A signer server must have a password file.\n");
		fduser := sys->open("/dev/user", Sys->OREAD);
		if(fduser == nil){
			sys->print("Could not open /dev/user\n\n");
			exit;
		}
		buf := array[128] of byte;
        	n := sys->read(fduser, buf, len buf);
		if(n < 0){
			sys->print("Could not read /dev/user\n\n");
			exit;
		}
        	username := string buf[0:n];
		sys->print("Creating a password for your username: %s\n",username);
		chlogin->init(nil, "addpassword" :: username :: nil);
	}

	# Check to see if the user wants to add passwords for other 
	# users on a network. If yes, prompt for other passwords.
	(ok, ans) := ask(stdin, "Do you wish to create a password for a user?", "n");
	if(!ok) {
		sys->fprint(stderr, "setupsigner: %s\n", ans);
		return;
	}
	if(ans[0] != 'y' && ans[0] != 'Y')
		return;

	# Prompt for other user names
	for(;;) {
		username : string;
		(ok, username) = ask(stdin, "Enter username", nil);
		if(!ok) {
			sys->fprint(stderr, "setupsigner: %s\n", username);
			return;
		}

		# Use modified changelogin module to enter passwords
		chlogin->init(nil, "addpassword" :: username :: nil);
		(ok, ans) = ask(stdin, "Do you wish to create a password for another user?", "n");
		if(!ok) {
			sys->fprint(stderr, "setupsigner: %s\n", ans);
			return;
		}
		if(ans[0] != 'y' && ans[0] != 'Y')
			break;
	}
}

# Routine to run lib/srv after signerkey and password files are created
services()
{
	err := sys->chdir("/");
	if (err != 0) {
		sys->fprint(stderr, "couldn't cd to / to run lib/srv \n");
		return;
	}

	# Check to see if lib/srv is already running
	checksrv();
	libsrv := load Command "/dis/lib/srv.dis";
	if (libsrv == nil) {
		sys->fprint(stderr, "setupsigner: couldn't load /dis/lib/srv.dis: %r\n");
		return;
	}
	sys->print("\nSigner keys and passwords created. After starting services to wait for\n"+
		"client requests, signer setup will be complete (see lib/srv).\n\n");
	libsrv->init(nil, "srv" :: nil);
}

# Routine to read /dev/sysname and remove domain name for signerid
sysname(): string
{
	fd := sys->open("#c/sysname", sys->OREAD);
	if(fd == nil)
		return "anon";

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) 
		return "anon";

	# strip off any domain name suffix
	(i, t) := sys->tokenize(string buf[0:n], ".");
	if (i<=0)
		return "anon";

	return hd t;
}


# Routine to prompt user and supply defaults
ask(io: ref Sys->FD, prompt, def: string): (int, string)
{
	if(def == nil || def == "")
		sys->print("%s: ", prompt);
	else
		sys->print("%s [%s]: ", prompt, def);
		(ok, resp) := readline(io);
	if(ok && resp == "" && def != nil)
		return (ok, def);
	return (ok, resp);
}

# Routine to read console input
readline(io: ref Sys->FD): (int, string)
{
        r : int;
        line : string;
        buf := array[8192] of byte;

        # Read up to the CRLF
        line = "";
        for(;;) 
		{
                r = sys->read(io, buf, len buf);
                if(r <= 0)
                        return (0, sys->sprint("error: read from console mode: %r"));
 
                line += string buf[0:r];
                if ((len line >= 1) && (line[(len line)-1] == '\n'))
			break;
        	}
        return (1, line[0:len line - 1]);
}

# Routine to check /prog to see if lib/srv (Server) is already running. If it is running, 
# this routine exits with a message. If not, it starts lib/srv and returns.
checksrv()
{
 	if (sys->bind("#p", "/prog", Sys->MREPL)<0) { 
		sys->fprint(stderr, "couldn't bind #p to /prog, so will not start lib/srv\n");
		exit;
	}
	
	dr := load Readdir Readdir->PATH;
	if(dr == nil) {
		sys->fprint(stderr, "couldn't load Readdir (%r), so will not start lib/srv\n");
		exit;
	}

	# Get PID directories from /prog
	(d, n) := dr->init("/prog", Readdir->NONE);
	for(i := 0; i < n; i++) {
		# For each PID, read the status file and search for "Server"
		statfd := sys->open("/prog/" + d[i].name + "/status", sys->OREAD);
		regex := load Regex Regex->PATH;
		if(regex == nil) {
			sys->fprint(stderr, "couldn't load Regex (%r), so will not start lib/srv\n");
			exit;
		}
		buf := array[100] of byte;
		nbytes := sys->read(statfd,buf,len buf);
	
		# If lib/srv already started, exit with message
		if (regex->execute(regex->compile("Server",0),string buf[0:nbytes])!= nil) {
			sys->print("\nSigner keys and passwords created. Services already started (see" +
				"lib/srv).\nWaiting for clients to request certificates. "+
				"Signer setup complete!\n\n");
			exit;
		}
	}
	return;
}

