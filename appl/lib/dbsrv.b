implement DBserver;

include "sys.m";
	sys     : Sys;
	stdin, stderr  : ref Sys->FD;

include "draw.m";
	Context : import Draw;

include "keyring.m";
	kr      : Keyring;

include "security.m";
	auth    : Auth;

include "db.m";              # For now.

DBserver : module
{
	init:   fn(ctxt: ref Context, argv: list of string);
};

# argv is a list of Inferno supported algorithms such as
#
#		Auth->NOAUTH ::
#               Auth->NOSSL :: 
#               Auth->CLEAR :: 
#               Auth->SHA :: 
#               Auth->MD5 :: 
#               Auth->RC4 ::
#               Auth->SHA_RC4 ::
#               Auth->MD5_RC4 ::
#               nil;
#
init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	if(sys == nil){
		sys->print("dbsrv: load Sys: %r\n");
		return;
	}
	stdin = sys->fildes(0);
	stderr = sys->open("/dev/cons", Sys->OWRITE);

	kr = load Keyring Keyring->PATH;
	if(nil == kr){
		sys->fprint(stderr, "load: keyring: %r\n");
	}

#       if(errorlogging){
#               #       make stderr the appropriate thing
#       }

	auth = load Auth Auth->PATH;
	if(auth == nil){
		sys->fprint(stderr, "Error: DBserver: can't load module Auth\n");
		exit;
	}

	error := auth->init();
	if(error != nil){
		sys->fprint(stderr, "Error: DBserver: %s\n", error);
		exit;
	}

	user := user();

	kr = load Keyring Keyring->PATH;
	ai := kr->readauthinfo("/usr/"+user+"/keyring/default");
	#
	# let auth->server handle nil ai
	# if(ai == nil){
	#	sys->fprint(stderr, "Error: DBserver: readauthinfo failed: %r\n");
	#	exit;
	# }
	#

	if(argv == nil){
		sys->fprint(stderr, "Error: styxd: no algorithm list\n");
		exit;
	}
	#
	# We might need this.  Since we can't tell yet, bind it here.
	#
	if ( sys->bind("#D", "/n/ssl", Sys->MREPL) < 0) {
		sys->fprint(stderr, "Error: DBserver: can't bind ssl: %r\n");
		exit;
	}

	(client_fd, info_or_err) := auth->server(argv, ai, stdin);
	if(client_fd == nil){
		sys->fprint(stderr, "Error: DBserver: %s\n", info_or_err);
		exit;
	}

    sys->pctl(Sys->FORKNS, nil);

    ############################################################################ 
    #
    # Execute an OS command to run an instance of the "infdb" database driver.
    #
    ############################################################################

    #
    # First, we must open the clone file.
    # 
    ctl_fd := sys->open("/cmd/clone", sys->ORDWR);
    if (ctl_fd == nil)
    {
	sys->fprint(stderr, "Could not open /cmd/clone\n");
    }

    #
    # Now, read the ctl file to get <n>, to generate other pathnames.
    #
    n_buf := array [5] of byte;
    num_read := sys->read(ctl_fd, n_buf, len n_buf);
    if (num_read == 0)
    {
	sys->fprint(stderr, "Read failed on /cmd/<n>/ctl\n");
    }
    n_string := string n_buf[0:num_read];

    #
    # Write the command into the control file.  This starts infdb!
    #
    command := "exec infdb"; 
    num_written := sys->write(ctl_fd, array of byte command, len command);
    if (num_written != len command)
    {
	sys->fprint(stderr, "Write failed on /cmd/%s/ctl\n", n_string);
	}

    #
    # Open the data file.
    #
    infdb_fd := sys->open("/cmd/" + n_string + "/data", sys->ORDWR);
    if (infdb_fd == nil)
    {
	sys->fprint(stderr, "Open failed on /cmd/%s/data\n", n_string);
    }

    spawn dbxfer(infdb_fd, client_fd);

    dbxfer(client_fd, infdb_fd);
    sys->write(infdb_fd, array of byte "X1          0   0 \n", 19);
}


dbxfer (source_fd, sink_fd : ref Sys->FD)
{
    #############################################################################
    #
    # The following loop reads from the source and writes to the sink.
    #
    #############################################################################

    buf := array [8192] of byte;
    for ( ; ; )
    {
	# Just try large block read for now.
	num_read := sys->read(source_fd, buf, 8192);
	if (num_read == 0)
	{
	    # Assume normal EOF and return OK.
	    return;
	}
	else if (num_read < 0)
	{
#            sys->fprint(stderr, "Read failed on source_fd: %s\n", string source_fd);
	     return;
	}

	num_written := sys->write(sink_fd, buf, num_read);
	if (num_written != num_read)
	{
#            sys->fprint(stderr, "Write failed on sink_fd: %s\n", string sink_fd);
	     return;
	}
    }
}


user(): string
{
	sys = load Sys Sys->PATH;

	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0)
		return "";

	return string buf[0:n]; 
}
