implement SetupUser;


include "sys.m";
	sys: Sys;
	stderr, stdin, stdout: ref Sys->FD;
include "draw.m";
include "sh.m";
include "readdir.m";
include "regex.m";
        regex: Regex;
        Re, compile, execute: import regex;
include "keyring.m";
	kr: Keyring;
include "security.m";
	login: Login;
	ssl: SSL;
include "string.m";


SetupUser: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};


init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
        stderr = sys->fildes(2);
        stdout = sys->fildes(1);
        stdin = sys->fildes(0);

        sys->print("\n\n" +
	"This utility is intended to help you set up Inferno for evaluation purposes.\n" +
	"It will take you through the basic steps to set up your system as either an\n" +
	"Inferno client or server. It uses default settings in order to simplify the\n" +
	"procedure. Related commands used for an actual network configuration appear\n" +
	"in parentheses and are described in your User's Guide.\n\n\n");

	# Copy wm setup files from /usr/inferno
	wmsetup();

	# Get a default certificate
	getcertificate();	
        sys->print("\nSetup is complete!\n\n");
}

# Routine to run lib/srv (unless it is already running)
services()
{
	err := sys->chdir("/");
	if (err != 0) {
		sys->fprint(stderr, "couldn't cd to / to run lib/srv \n");
		return;
	}

	# Check to see if lib/srv is already running
	# If return is 0, start it. Otherwise just give a message.
        if (checksrv() == 0) {
		libsrv := load Command "/dis/lib/srv.dis";
		if (libsrv == nil) {
                        sys->fprint(stderr, "setupuser: couldn't load /dis/lib/srv.dis: %r\n");
			exit;
                }

		sys->print(
		"Will now bind network devices (bind '#I' /net) and start the connection\n"+
		"service (lib/cs) to establish a network connection. Also, starting\n" +
		"services so that your system can be either a client or file server\n" +
		"(See lib/srv)\n\n");

		libsrv->init(nil, "srv" :: nil);
		sys->sleep(1000);
	}
	# If it is already running, we just display a message and continue
	else
		sys->print("\n" +
		"Network devices have been previously set up (bind '#I' /net);\n" +
		"the connection service (lib/cs) is already started; and client services\n"+
		"have been previously started (See lib/srv). Your system is set up to be\n"+
		"either a client or file server.\n\n");      

	return;
}

# Routine to check /prog to see if lib/srv (Server) is already running. 
checksrv() : int
{
 	if (sys->bind("#p", "/prog", Sys->MREPL)<0) { 
		sys->fprint(stderr, "couldn't bind #p to /prog (%r), so will not start lib/srv \n");
		exit;
	}
	
	dr := load Readdir Readdir->PATH;
	if(dr == nil) {
		sys->fprint(stderr, "couldn't load Readdir (%r), so will not start lib/srv \n");
		exit;
	}
	regex := load Regex Regex->PATH;
	if(regex == nil) {
		sys->fprint(stderr, "couldn't load RegEx (%r), so will not start lib/srv\n");
		exit;
	}

	# Get PID directories from /prog
	(d, n) := dr->init("/prog", Readdir->NONE);

	# For each PID, read the status file and search for "Server"
	for(i := 0; i < n; i++) {
		statfd := sys->open("/prog/" + d[i].name + "/status", sys->OREAD);
		buf := array[100] of byte;
		nbytes := sys->read(statfd,buf,len buf);
	
		# If lib/srv already started, return with a 1
                if (regex->execute(regex->compile("Server",0),string buf[0:nbytes]) != nil)
			return 1;
	}
	return 0;
}

wmsetup()
{
	copy := load Command "/dis/cp.dis";
	if (copy == nil) {
		sys->print("Could not load /dis/cp.dis in order to copy Window Manager setup files\n");
		return;
	}
	user := username();
	if(user == "") {
		sys->fprint(stderr, "setupuser: unable to determine username from /dev/user: %r\n");
		return;
	}
	todir := "/usr/" + user; 
	(status,nil) := sys->stat(todir);
	if (status != 0) {
		if(sys->create(todir, sys->OREAD, sys->CHDIR + 8r770) == nil) {
			sys->print("Cannot create %s\n", todir);
			exit;
		}
		sys->print("Created your user directory %s and copying\n"+
			"Window Manager setup files from /usr/inferno to %s\n\n",
			todir, todir);
	}
	else
		sys->print("\nCopying Window Manager setup files from " +
			" /usr/inferno to %s\n\n", todir);

	copy->init(nil, "cp"::"/usr/inferno/namespace"::"/usr/inferno/wmsetup"::"/usr/inferno/plumbing"::todir::nil);
	return;
}

#checkcsdb()
#{
#        signer := login->defaultsigner();
#        if(signer == nil)
#               sys->fprint(stderr, "warning: can't get default signer server name\n");
#}

getcertificate()
{
	#Determine username and signername
	user := username();
	if(user == "") {
		sys->fprint(stderr, "setupuser: unable to determine username from /dev/user: %r\n");
		return;
	}
	# checkcsdb();		# should check /services/cs/db for $SIGNER setup
        (code, signername) := ask(stdin, "Enter name of signer server or press Enter for default", sysname());
        if(!code) {
		sys->fprint(stderr, "setupsigner: %s\n", signername);
		return;
	}
	if(signername == "")
		signername = sysname();
	signer := "tcp!" + signername;
        dir := "/usr/" + user + "/keyring";
        file := dir + "/default";
	(i, nil) := sys->stat(file);
	if (i == 0) {
                sys->print("\nYou already have a certificate file named 'default' in %s.\n" +
			"However, it may not be from the same signer that you entered.",dir);
		(ok, ans) := ask(stdin, "Do you want to overwrite it?", "y");
		sys->print("\n");
		if(!ok){
			sys->print("Did not get valid response. Restart setupuser\n");
			exit;
		}
		if(ans[0] != 'y' && ans[0] != 'Y'){
                        sys->print("Previously created certificate in /usr/%s/keyring/default will be used for authentication.\n\n",user);
                        services();
			return;
		}	
	}
        else {
                (i, nil) := sys->stat(dir);
                if (i != 0) {
			if(sys->create(dir,sys->OREAD,sys->CHDIR + 8r770) == nil) {
				sys->print("Cannot create /usr/%s/keyring\n",user);
				exit;
			}
			sys->print("Created %s to store your default certificate\n\n",dir);
                 }
        }
  	ssl = load SSL SSL->PATH;
	if(ssl == nil){
                sys->fprint(stderr, "Error: can't load module ssl");
		exit;
	}
	# push ssl, leave in clear mode for now
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0){
                sys->fprint(stderr, "Error: cannot bind #D: %r");
		exit;
	}
 	kr = load Keyring Keyring->PATH;
	str := load String String->PATH;
	if(str == nil){
                sys->fprint(stderr, " can't load module String\n");
		exit;
	}
 	login = load Login Login->PATH;
	if(login == nil){
                sys->fprint(stderr, " can't load module Login\n");
		exit;
	}

	# Start lib/srv
	services();
 	error := login->init();
	if(error != nil){
                sys->fprint(stderr, "login->init: %s\n", error);
		exit;
	}

	save := "yes";
	redo := "yes";
	view := "no";	
	accept := "yes";

	# flag to determine what part of the client/server exchange should be
	# repeated if the user makes a mistake (e.g. wrong password)
        redial := 0;

	# After making the connection, get the password and start the login exchange
	# with the server. Permit the user to continue trying to get a certificate.
	for(;;) {
		#Connect to signer server
		if(redial == 0)
			sys->print("\nSetting up a network connection to the signer server\n");
		else
			sys->print("Resetting the network connection\n");
		(ok,lc) := sys->dial(signer+"!inflogin", nil);
		if(ok < 0){
			sys->fprint(stderr, "Cannot contact signer: dial login daemon failed %r\n\n");
			exit;
		}
		(err, c) := ssl->connect(lc.dfd);
		if(c == nil){
			sys->fprint(stderr, "Error: can't push ssl: %s\n", err);
			exit;
		}
		lc.dfd = nil;
		lc.cfd = nil;
 
		# get license agreement  
		agf := "/licensedb/agreement.sig";
		(c, err) = login->chello(user, agf, c);
		if(c == nil){
			sys->fprint(stderr, "Error: %s\n", err);
			exit;
		}

		# accept agreement (but display message only first time through the loop
		if(redial == 0)
			sys->print("\nThe signer has sent a license agreement,\n"+
				"which you can view later in %s\n\n", agf);
		if(kr->putstring(c.dfd, "agree") < 0) {
			sys->fprint(stderr, "Error: can't send string: %r");
			exit;
		}

		# Get your password
		fdctl := sys->open("/dev/consctl", sys->OWRITE);
		ctlbuf := array of byte "rawon";
		if(fdctl == nil || sys->write(fdctl,ctlbuf,len ctlbuf) != 5) {
			sys->fprint(stderr, "Unable to change console mode to enter password");
			exit; 
		}
		sys->print("Enter your password to get a certificate:");
		mode := "rawon";
		(readcode,password) := readline(stdin,mode);
		if(sys->write(fdctl,array of byte "rawoff",6) != 6) {
			sys->fprint(stderr, "Unable to change console mode back to 'normal'");
			exit; 
		}

		# request certification
		info : ref Keyring->Authinfo;
                (info, err) = login->ckeyx(user,password, c);

 		# save the certificate in default file
		if(info != nil) {
                        if(kr->writeauthinfo(file,info) < 0) {        
                                sys->print("Could not write certificate. Writeauthinfo to %s failed\n", file);
                                exit;
                        }
                        sys->print("Certificate written to %s\n",file);
                        break;
                }
                else {
		# debug message
		sys->print("Could not get a certificate, possibly because the password is wrong.\n");
		(ok, ans) := ask(stdin, "Do you want to try again?", "y");
		sys->print("\n");
		if(!ok) {        
			sys->fprint(stderr, "setupsigner: %s\n", ans);
			return;
		}
		if(ans[0] != 'y' && ans[0] != 'Y')
			exit;
                 }
		# try again but don't redo the license agreement
		user = username();
		redial = 1;
	}
}

username(): string
{
	fd := sys->open("/dev/user", Sys->OREAD);
	if(fd == nil)
		return "";
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) 
		return "";
	return string buf[0:n];
}

#Routine to read /dev/sysname and remove domain name
sysname(): string
{
	fd := sys->open("#c/sysname", sys->OREAD);
	if(fd == nil)
                return "anon";
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) 
                return "anon";

	# strip off domain name suffix
        (i,t) := sys->tokenize(string buf[0:n], ".");
        if(i<=0)
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
	(ok, resp) := readline(io, "rawoff");
	if(ok && resp == "" && def != nil)
		return (ok, def);
	return (ok, resp);
}

# Routine to read console input
readline(io: ref Sys->FD,mode: string): (int, string)
{
        r : int;
        line : string;
        buf := array[8192] of byte;

        # Read up to the CRLF
        line = "";
        for(;;) {
                r = sys->read(io, buf, len buf);
                if(r <= 0) {
			sys->print("Error: Could not read from console. Start again.");
			exit;
		} 
                line += string buf[0:r];
                if ((len line >= 1) && (line[(len line)-1] == '\n')) {
                        if (mode == "rawon") {
				r = sys->write(stdout,array of byte "\n",1);
				if(r <= 0)
					return (0,nil);
                        }
			break;
        	}
                else {
                        if (mode == "rawon") {
                                r = sys->write(stdout,array of byte "*",1);
				if(r <= 0)
					return (0,nil);
                        }
                        
        	}
        }
        return (1, line[0:len line - 1]);
}

