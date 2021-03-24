implement Cs;

#
#	Module:		CS
#	Purpose:	Connection Server
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#	History:	Based on original CS from 1127 Research Tree
#

include "sys.m";
	sys: Sys;
	FD, FileIO: import Sys;
include "draw.m";
include "cs.m";
include "bufio.m";
include "string.m";
include "regex.m";
	regex:	Regex;

Cs: module
{
	init:	fn(ctxt: ref Draw->Context, nil: list of string);
};

# Contants
CONFIG_PATH:	con "/services/cs/config";

# Globals
stderr: ref FD;

# ADT Definitions
Reply: adt
{
	fid   : int;
	addrs : list of string;
	rchan : chan of int;
	wait  : int;
	err   : string;
};

rlist: list of ref Reply;

Plugin: adt
{
	mod:	CSplugin;	# module to implement the plug-in
	exp:	regex->Re;	# regular expression which selects the module
};

plugins:	list of Plugin;

init(ctxt: ref Draw->Context, argv: list of string)
{
	if((sys = load Sys Sys->PATH) == nil)
		return;
	if((regex = load Regex Regex->PATH) == nil) {
		sys->raise("fail: couldn't load regex module");
		return;
	}
	
	stderr = sys->fildes(2);

	force := 0;

	if (argv !=nil)
		argv = tl argv;
	while(argv != nil) {
		s := hd argv;
		if(s[0] != '-')
			break;
		for(i := 1; i < len s; i++) case s[i] {
		'f' =>
			force = 1;
		* =>
			sys->fprint(stderr, "usage: svc/cs/cs [-f]\n");
			return;
		}
		argv = tl argv;
	}

	if(!force){
		(ok, nil) := sys->stat("/net/cs");
		if(ok >= 0) {
			sys->raise("fail: cs already started");
			return;
		}
	}

	sys->bind("#s", "/net", Sys->MBEFORE );
	file := sys->file2chan("/net", "cs");
	if(file == nil) {
		sys->raise("fail: failed to make file: /net/cs");
		return;
	}
	
	config(ctxt);

	spawn cs(file);
}

config(context: ref Draw->Context)
{
	lines:	list of string;

	# load bufio
	bufio := load Bufio Bufio->PATH;
	str := load String String->PATH;

	if (bufio == nil) {
		sys->raise("fail:Couldn't load BufIO module");
		exit;
	}
	if (str == nil) {
		sys->raise("fail:Couldn't load String module");
		exit;
	}

	Iobuf: import bufio;
	# open the config file
	f := bufio->open(CONFIG_PATH,Sys->OREAD);

	if(f == nil) {
		sys->raise("fail:Couldn't load config file");
		exit;
	}

	# read config file line by line and place into list;
	while((l := f.gett("\r\n")) != nil) {
		if ((l == "\r") || (l == "\n") || (l[0] == '#'))
			continue;
		if ((l[(len l)-1] == '\r')||(l[(len l)-1] == '\n'))
			l = l[:(len l)-1];
		lines = l::lines;
	}
	
	# parse lines and process regular expressions
	while (lines != nil) {
		newplugin: Plugin;
		(modpath, modex) := str->splitl(hd lines, " ");
		lines = tl lines;
		if (modex == nil) continue;
		modex = modex[1:];
		newplugin.mod = load CSplugin modpath;
		if (newplugin.mod == nil) {
			sys->print("stderr: Couldn't load plugin %s: %r\n",modpath);
			continue;
		}
		newplugin.mod->init(context, nil);
		newplugin.exp = regex->compile(modex, 1);
		plugins = newplugin::plugins;
	}
}

cs(file: ref FileIO)
{
	off, nbytes, fid : int;
	rc : Sys->Rread;
	wc : Sys->Rwrite;
	buf : array of byte;

	# Monitor for rlist queue
	reqch = chan of int;
	freech = chan of int;	
	spawn cs_mon();

	rlist = nil;
	spawn watcher();

	for (;;) {
  		alt {
			(off, buf, fid, wc) = <-file.write =>
				cleanfid(fid);
				if(wc == nil)
					break;
				spawn process(buf, fid, wc);
				buf = nil;

			(nil, nbytes, fid, rc) = <-file.read =>
				if (rc != nil) {
					spawn result(off, fid, nbytes, rc);
	  				continue;
				}
 		   }
	}
}

reqch, freech : chan of int;

cs_mon()
{
	while(1) {

		# Wait for request
		<- reqch;

		# Wait for free
		<- freech;
	}
}

cleanfid(fid: int)
{
	reqch <- = 1;
	new : list of ref Reply;

	for(s := rlist; s != nil; s = tl s) {
		r := hd s;
		if(r.fid != fid)
			new = r :: new;
		else
			watchtime(r, 0);
	}
	rlist = new;
	freech <- = 1;
}

watcher()
{
	for(;;) {
		sys->sleep(Timeout);
		if (rlist == nil)
			continue;

		new : list of ref Reply = nil;
		reqch <- = 1;
		ct := sys->millisec();
		for(s := rlist; s != nil; s = tl s) {
			r := hd s;
			if (!watchtime(r, ct))
				new = r :: new;
		}
		rlist = new;
		freech <- = 1;
	}
}

# time left to read your reply after DNS responds
Timeout : con 10000;

watchtime(r : ref Reply, ct : int) : int
{
	if ((ct == 0 && r.wait != 0) || (1 < r.wait && r.wait < ct)) {
		# too late - free send process
		<- r.rchan;
		r.wait = 0;
		r.err = "timeout";
		return 1;
	}
	return 0;
}

watch(r : ref Reply)
{ 
	reqch <- = 1;
	if (r.wait == 1)
		r.wait = sys->millisec() + Timeout;
	freech <- = 1;
}

unwatch(r : ref Reply) : int
{
	ready := 0;
	reqch <- = 1;
	if (r.wait) {
		ready = 1;
		r.wait = 0;
	}
	freech <- = 1;
	return ready;
}

reads(str: string, off, nbytes: int): (array of byte, string)
{
	if(len str < 1 || nbytes < 1)
		return (nil, "nothing to do");
	bstr := array of byte str;
	if(off >= len bstr)
		return (nil, "offset exceeds string length");
	if(off + nbytes > len bstr)
		nbytes = len bstr - off;
	buf := bstr[off:off+nbytes];
	return (buf, nil);
}

result(off, fid,nbytes: int, rc : chan of (array of byte, string))
{
	r: ref Reply;
	err, ipaddr: string;
	s: list of ref Reply;

	for(s = rlist; s != nil; s = tl s) {
		r = hd s;
		if(r.fid == fid) {
	    		if (unwatch(r))
				<- r.rchan;
	    		if (r.addrs != nil) {
	      			ipaddr = hd r.addrs;
	      			r.addrs = tl r.addrs;
	    		} else
	      			err = r.err;
	    		if (r.addrs == nil)
	      			cleanfid(fid);
	    		break;
	  	}
	}

	off = 0; # offset from file2chan is inconsistent (mr)

	# make sure this is the last thing done since it frees the fid.
	if (ipaddr == nil) {
		if (err == nil)
			err = "can't translate address";
		rc <-= (nil, err);
	} else 
		rc <-= reads(ipaddr, off, nbytes);
}

#
# Process the write request. Create a channel to synchronize the read
# request.  Place the reply adt on the reply list with a valid fid and
# sync channel.  Then release the users write request.
#

process(data : array of byte, fid :int, wc : chan of (int, string))
{
	# Channel to sync on result completion
  	rchan := chan of int;

  	r := ref Reply;
  	r.rchan = rchan;
  	r.wait = 1;
  	r.fid = fid;
  
  	reqch <- = 1;
  	rlist = r :: rlist;
  	freech <- = 1;

  	# Must create an entry before returning to the user
  	wc <-= (len data, nil);

  	(addrs, err) := xlate(data);

  	r.addrs = addrs;
  	r.err = err;
  	if (err != nil)
    		sys->fprint(stderr, "cs: %s\n", err);

  	# Set read result expiration time
  	watch(r);

  	# Let the reader know the information is ready
  	r.rchan <-= 1;
}

xlate1(cpi: Plugin, addr: string): (list of string, string)
{
	e := ref Sys->Exception;
	if (sys->rescue("fail: *", e) == Sys->HANDLER) {
		resp := cpi.mod->xlate(addr);
		return (resp, nil);
	} else {
		err := e.name[6:];
		sys->rescued(Sys->ACTIVE, nil);
		return (nil, err);
	}			
}

xlate(data: array of byte): (list of string, string)
{
	cpi: Plugin;
	cca: string;
	resp: list of string;
	newresp: list of string;

	err: string;
	if ((data[(len data)-1] == byte '\n') || (data[(len data)-1] == byte '\r'))
		resp = (string data[:( len data )-1 ]::nil);
	else
		resp = (string data::nil);

	# for all configured plug-ins
	for (pi := plugins; len pi; pi = tl pi) {
		cpi = hd pi;
		# for all current addresses
		
		resp = reverselos(resp);
		for (ca := resp; len ca; ca = tl ca) {
			cca = hd ca;
			# if ca matches the expression
			if  (regex->execute(cpi.exp, cca) != nil) {
				(newaddrs, newerr) := xlate1(cpi, cca);
				# prepend results in reverse order
				for (a := newaddrs; len a; a = tl a)
					newresp = (hd a) :: newresp;
				if (newerr != nil)
					err = newerr;
			} else 
				newresp = cca :: newresp;
		}
		resp = newresp;
		newresp = nil;
	}

	# for all current addresses
	for (foo := resp; len foo; foo = tl foo) {
		(n, l) := sys->tokenize(hd foo, "!\n");
		case (n) {
			2 => 
				newresp = ("/net/"+(hd l)+"/clone "+hd (tl l))::newresp;
				break;
			3 => 
				newresp = ("/net/"+(hd l)+"/clone "+hd (tl l)+"!"+hd (tl tl l))::newresp;
				break;
			* =>
				sys->fprint(stderr,"Malformed address (%s) returned from translation - ignored\n", hd foo);
				continue;
		}
		
	}

	return (newresp, err);
}

reverselos(l: list of string) : list of string
{
	t : list of string;
	for(; l != nil; l = tl l)
		t = hd l :: t;
	return t;
}

