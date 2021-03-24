implement XfsSrv;

include "draw.m";

include "sys.m";
	sys : Sys;

include "styx.m";

include "iotrack.m";

include "dossubs.m";

include "dosfs.m";
	dosfs : Dosfs;

XfsSrv: module
{
        init:   fn(ctxt: ref Draw->Context, argv: list of string);
};

usage(argv0: string)
{
	sys->print("usage: %s [-v] [-S] [-F] [-c] [-f devicefile] [-m mountpoint]\n", argv0);
	exit;
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	dosfs = load Dosfs Dosfs->PATH;
	if(dosfs == nil)
		sys->print("failed to load %s: %r\n", Dosfs->PATH);

	pipefd := array[2] of ref Sys->FD;
	argv0 := hd argv;
	argv = tl argv;
	chatty := 0;

	logfile := "";
	srvfile	:= "/n/dos"; 
	deffile := "/dev/hd0disk";

	while (argv!=nil) {
		case (hd argv) {
		"-v" =>
			chatty |= DosSubs->VERBOSE;
		"-S" =>
			chatty |= DosSubs->STYX_MESS;
		"-F" =>
			chatty |= DosSubs->FAT_INFO;
		"-c" =>
			chatty |= DosSubs->CLUSTER_INFO;
		"-l" =>
			if(tl argv != nil)
				logfile = hd tl argv;
			else
				usage(argv0);
		"-f" =>
			if(tl argv !=nil) {
				deffile = hd tl argv;
				argv = tl argv;
			}
			else
				usage(argv0);
		"-m" =>
			if(tl argv != nil) {
				srvfile= hd tl argv;
				argv = tl argv;
			}
			else
				usage(argv0);	
		* =>
			usage(argv0);
		}
		argv = tl argv;
	}

	dosfs->init(deffile, logfile, chatty);

	if(deffile == "" || srvfile == "")
		usage(argv0);

	if(sys->pipe(pipefd) < 0) {
		sys->fprint(sys->fildes(2),"xfssrv: pipe %r\n");
		exit;
	}

	dosfs->setup();

	spawn dosfs->dossrv(pipefd[1]);

	n := sys->mount(pipefd[0], srvfile, sys->MREPL|sys->MCREATE, deffile);
	if(n == -1) {
		sys->fprint(sys->fildes(2), "xfssrv: mount %s: %r\n", srvfile);
		exit;
	}

	sys->fprint(sys->fildes(2), "%s : mounted %s at %s\n",
			argv0, deffile, srvfile);
}

