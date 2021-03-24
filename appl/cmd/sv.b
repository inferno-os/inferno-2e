implement StyxView;

# view Styx messages from client
#
# Author: Ed Bacher, evb@lucent.com
# - adapted from Tad Hunt's original rdbgsrv program for
# sending commands to the SWoRD monitor.
# - run this from the shell (not as a background thread)
# or use sh -n if running from a script
#
# debug options (implemented for user as -v[v] for 1, 2)
# 1: trace, 2: dump, 4: write one byte at a time.

include "sys.m";
        sys: Sys;
        print: import sys;
        stderr, stdout: ref sys->FD;
include "draw.m";
include "bufio.m";
	bufmod: Bufio;
	Iobuf: import bufmod;

outbuf: ref Iobuf;

#
# globals
#

# debug, verbose stuff:
debug := 0;
TRACE:		con 1;
DUMP:		con 2;
WRITESLOW:	con 4;
shift := 1;

# default device directory (ip device)
dev := "/net/tcp/0/data";
mtpt: string;

# message types (from styx.m)
Tnop,		#  0 
Rnop,		#  1 
Terror,		#  2	# illegal 
Rerror,		#  3 
Tflush,		#  4 
Rflush,		#  5 
Tclone,		#  6 
Rclone,		#  7 
Twalk,		#  8 
Rwalk,		#  9 
Topen,		# 10 
Ropen,		# 11 
Tcreate,	# 12 
Rcreate,	# 13 
Tread,		# 14 
Rread,		# 15 
Twrite,		# 16 
Rwrite,		# 17 
Tclunk,		# 18 
Rclunk,		# 19 
Tremove,	# 20 
Rremove,	# 21 
Tstat,		# 22 
Rstat,		# 23 
Twstat,		# 24 
Rwstat,		# 25 
Tsession,	# 26	# not used
Rsession,	# 27	# not used
Tattach,	# 28 
Rattach,	# 29
Tmax		: con iota;


message_size := array[] of {
	3,	# Tnop
	3,	# Rnop
	0,	# Terror	(illegal)
	67,	# Rerror
	5,	# Tflush
	3,	# Rflush
	7,	# Tclone
	5,	# Rclone
	33,	# Twalk
	13,	# Rwalk
	6,	# Topen
	13,	# Ropen
	38,	# Tcreate
	13,	# Rcreate
	15,	# Tread
	0,	# Rread		(variable size)
	0,	# Twrite	(variable size)
	7,	# Rwrite
	5,	# Tclunk
	5,	# Rclunk
	5,	# Tremove
	5,	# Rremove
	5,	# Tstat
	121,	# Rstat
	121,	# Twstat
	5,	# Rwstat
	0,	# Tsession	(not used)
	0,	# Rsession	(not used)
	61,	# Tattach
	13	# Rattach
};

StyxView: module
{
        init: fn(nil: ref Draw->Context, argv: list of string);
};

usage()
{
        sys->fprint(stderr, "usage: %s [-v[v]] [-f dev] [-h] mountpoint\n", progname);
        exit;
}


PassProto: adt
{
        rfd: ref Sys->FD;
        wfd: ref Sys->FD;

        start: fn(name: string): ref PassProto;
        read:  fn(this: self ref PassProto, buf: array of byte, nbytes: int): int;
        write: fn(this: self ref PassProto, buf: array of byte, nbytes: int): int;

        get2: fn(this: self ref PassProto): int;
        put2: fn(this: self ref PassProto, n: int): int;
        
        get1: fn(this: self ref PassProto): int;
};

init(nil: ref Draw->Context, av: list of string)
{
    sys = load Sys Sys->PATH;
    if (sys == nil)
            return;

	stdout = sys->fildes(1);
	stderr = sys->fildes(2);

    bufmod = load Bufio Bufio->PATH;
    if (bufmod == nil)
    	sys->raise(sys->sprint("fail: could not load Bufio from %s: %r\n", Bufio->PATH));

    outbuf = bufmod->fopen(stdout, bufmod->OWRITE);
    	
    arginit(av);
    while (o := opt())
		case o {
		'd' =>
		        d := arg();
		        if (d == nil)
		                usage();
		        debug = int d;
		'v' =>
			if (shift & WRITESLOW) usage();
			debug += shift;
			shift = shift<<1;
		'f' =>
		        s := arg();
		        if (s == nil)
		                usage();
		        dev = s;
		'h' or '?' =>
		        usage();
		* =>
			usage();
		}

    mtpt = arg();
    if (mtpt == nil)
            usage();

# connect to communication data files
    proto := PassProto.start(dev);
    if (proto == nil)
            sys->raise("fail: proto start");

    fds := array[2] of ref Sys->FD;

    if (sys->pipe(fds) == -1)
            sys->raise(sys->sprint("fail: pipe: %r"));

    if (debug)
            sys->print("%s: starting server\n", progname);

# give one end of pipe to transceiver function to connect to byte source
    rc := chan of int;
    spawn tranceiver(fds[1], proto, rc);
    rpid := <- rc;

# mount other end of pipe on mountpoint in local namespace
    if (sys->mount(fds[0], mtpt, Sys->MREPL, nil) == -1) {
            killpid(rpid);
            sys->raise(sys->sprint("fail: mount: %r"));
    }
}


killpid(pid: int)
{
        if (pid == 0)
                return;
        fd := sys->open("/prog/"+string pid+"/ctl", sys->OWRITE);
        if (fd == nil)
                return;

        sys->write(fd, array of byte "kill", 4);
}

tranceiver(fd: ref Sys->FD, proto: ref PassProto, pidchan: chan of int)
{
        pidchan <-= sys->pctl(0, nil);

        buf := array[Sys->ATOMICIO+64] of byte;
        e := ref Sys->Exception;
        if (sys->rescue("*", e)) 
        {
                sys->print("%s: server: %s: exiting\nunmounting %s\n", progname, e.name, mtpt);
                sys->unmount(nil, mtpt);
                return;
        }

        for (;;) {
# T message interception
                n := sys->read(fd, buf, len buf);
                if (n == -1)
                        sys->raise(sys->sprint("server: read: %r"));
                if (proto.write(buf, n) != n)
                        sys->raise("server: proto.write failed");

# R message interception
                n = proto.read(buf, len buf);
                if (n == -1)
                        sys->raise("server: proto.read failed");
                if (sys->write(fd, buf, n) != n)
                        sys->raise(sys->sprint("server: write: %r"));
        }
}


# open read and write file descriptors for communication device
#
PassProto.start(device:string): ref PassProto
{
        p: PassProto;

        if (sys == nil)
                        return nil;

        p.rfd = sys->open(device, Sys->OREAD);
        p.wfd = sys->open(device, Sys->OWRITE);
        if (p.rfd == nil || p.wfd == nil)
                        return nil;

        return ref p;
}

PassProto.read(this: self ref PassProto, buf: array of byte, nbytes: int): int
{
# get message type and store in buf
        mtype := this.get1();
        buf[0] = byte mtype;

        r := 0;
        start := 1;
        b := array[Sys->ATOMICIO+16] of byte;
# set number of bytes expected: see Reference Manual for message sizes
# note that type is first byte
        case mtype {
                
                # Rread and Twrite require special handling
                # because the messages are not fixed length
                
                Rread   => 
                # put tag, fid into buf, then get count
                        r = sys->read(this.rfd, b, 4);
                        buf[start:] = b[:r];
                        start += r;
                        count := this.get2();
                        buf[start:] = int_to_abyte(count);
                        start += 2;     
                        nbytes = count + 8;
                        
                Twrite  => ;
                # put tag, fid, offset into buf, then get count
                        r = sys->read(this.rfd, b, 12);
                        buf[start:] = b[:r];
                        start += r;
                        count := this.get2();
                        buf[start:] = int_to_abyte(count);
                        start += 2;
                        nbytes = count + 16;
                        

                *       =>
                	nbytes = message_size[mtype];
       }
        

# read the rest of the message and store
        for (i := start; i < nbytes; i += r) {
                r = sys->read(this.rfd, b, nbytes);
                if (r == -1)
                        return -1;
                buf[i:] = b[:r];
        }

        if (debug & TRACE)
                trace("read ", buf);
        if (debug & DUMP)
                dump("proto.read", buf, nbytes);

        return nbytes;
}

PassProto.write(this: self ref PassProto, buf: array of byte, nbytes: int): int
{
        if (debug & WRITESLOW) {
                i := 0;
                while (i < nbytes) {
                        if (sys->write(this.wfd, buf[i++:], 1) != 1)
                                return -1;
                        sys->sleep(1);
                }
        } else {
                if (sys->write(this.wfd, buf, nbytes) != nbytes)
                        return -1;
        }

        if (debug & TRACE)
                trace("\nwrite", buf);
        if (debug & DUMP)
                dump("proto.write", buf, nbytes);
        return nbytes;
}

PassProto.get1(this: self ref PassProto): int
{
        buf := array[1] of byte;
        n := sys->read(this.rfd, buf, 1);
        if (n != 1)
                sys->raise(sys->sprint("fail: PassProto.get1: read %d: %r", n));
        val := int buf[0];
        return val;
}

PassProto.get2(this: self ref PassProto): int
{

        buf := array[1] of byte;

        n := sys->read(this.rfd, buf, 1);
        if (n  != 1)
                sys->raise(sys->sprint("fail: PassProto.get2: read %d: %r",n));
        val := int buf[0];

        n = sys->read(this.rfd, buf, 1);
        if (n  != 1)
                sys->raise(sys->sprint("fail: PassProto.get2: read %d: %r",n));
        val |= int buf[0] << 8;

        return val;
}

PassProto.put2(this: self ref PassProto, n: int): int
{
        buf := array[2] of byte;

        buf[0] = byte n;
        buf[1] = byte (n>>8);

        if (sys->write(this.wfd, buf, 2) != 2)
                return -1;
        return 2;
}

int_to_abyte(n: int): array of byte
{
        buf := array[2] of byte;
        buf[0] = byte n;
        buf[1] = byte (n>>8);
        
        return buf;
}

trace2(buf: array of byte): int
{
# do little-endian switch
        return int buf[0] | (int buf[1] << 8);
}



trace(sourcept: string,  op: array of byte ) 
{
        case int op[0] {
         Tnop =>
                sys->print("%s: Tnop(%d)    tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Tflush =>
                sys->print("%s: Tflush(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Tclone =>
                sys->print("%s: Tclone(%d)  tag(%d) fid(%d) newfid(%d)\n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]), trace2(op[5:]));
         Twalk =>
                sys->print("%s: Twalk(%d)   tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Topen =>
                sys->print("%s: Topen(%d)   tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Tcreate =>
                sys->print("%s: Tcreate(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]));
         Tread =>
                sys->print("%s: Tread(%d)   tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Twrite =>
                sys->print("%s: Twrite(%d)  tag(%d) fid(%d) count(%d)\n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]), trace2(op[13:]) );
         Tclunk =>
                sys->print("%s: Tclunk(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Tremove =>
                sys->print("%s: Tremove(%d) tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]));
         Tstat =>
                sys->print("%s: Tstat(%d)   tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Twstat =>
                sys->print("%s: Twstat(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Tattach =>
                sys->print("%s: Tattach(%d) tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]));
         Rnop =>
                sys->print("%s: Rnop(%d)    tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rflush =>
                sys->print("%s: Rflush(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rclone =>
                sys->print("%s: Rclone(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rwalk =>
                sys->print("%s: Rwalk(%d)   tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Ropen =>
                sys->print("%s: Ropen(%d)   tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rcreate =>
                sys->print("%s: Rcreate(%d) tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]));
         Rread =>
                sys->print("%s: Rread(%d)   tag(%d) fid(%d) count(%d)\n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]), trace2(op[5:]) );
         Rwrite =>
                sys->print("%s: Rwrite(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rclunk =>
                sys->print("%s: Rclunk(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rremove =>
                sys->print("%s: Rremove(%d) tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]));
         Rstat =>
                sys->print("%s: Rstat(%d)   tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rwstat =>
                sys->print("%s: Rwstat(%d)  tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]) );
         Rattach =>
                sys->print("%s: Rattach(%d) tag(%d) fid(%d) \n",
                sourcept, int op[0], trace2(op[1:]), trace2(op[3:]));
         Rerror =>
                s := "";
                for (i:=0;i<64;i++) {
                        if (op[3+i] != byte 0)
                                s[len s] = int op[3+i];
                        else
                                break;
                }
                sys->print("%s: Rerror(%d)  tag(%d) %s\n",
                sourcept, int op[0], trace2(op[1:]), s);
        }
}

dump(msg: string, buf: array of byte, n: int)
{
# message type and no. of bytes
        sys->print("%s: [%d bytes]: ", msg, n);
        
# the bytes (in one-byte chunks)
        s := " ";
        line := "";
        for (i:=0;i<n;i++) {
                if ((i % 20) == 0) {
                        line += sys->sprint("%s\n", s);
                        s = " ";
                }
                line += sys->sprint("%2.2x", int buf[i]);
                if (int buf[i] >= 32 && int buf[i] < 127)
                        s[len s] = int buf[i];
                else
                        s += ".";
                        
                if ( ((i + 1) % 20) != 0)
                        line += sys->sprint(" ");
        }
# do the last line
        for (i %= 20; (i < 20) && (i != 0); i++){
                line += sys->sprint("  ");
                if ( ((i + 1) % 20) != 0)
                        line += sys->sprint(" ");
        }
        line += sys->sprint("%s\n\n", s);

        outbuf.puts(line);
        outbuf.flush();
}


#
# Arg parsing from Roger Peppe <rog@ohm.york.ac.uk>
#
progname := "";
args: list of string;
curropt: string;

arginit(argv: list of string)
{
        if (argv == nil)
                return;
        (progname, args) = (hd argv, tl argv);
}

# don't allow any more options after this function is invoked
argv() : list of string
{
        ret := args;
        args = nil;
        return ret;
}

# get next option argument
arg() : string
{
        if (curropt != "") {
                ret := curropt;
                curropt = nil;
                return ret;
        }

        if (args == nil)
                return nil;

        ret := hd args;
        if (ret[0] == '-')
                ret = nil;
        else
                args = tl args;
        return ret;
}

# get next option letter
# return 0 at end of options
opt() : int
{
        if (curropt != "") {
                opt := curropt[0];
                curropt = curropt[1:];
                return opt;
        }

        if (args == nil)
                return 0;

        nextarg := hd args;
        if (nextarg[0] != '-' || len nextarg < 2)
                return 0;

        if (nextarg == "--") {
                args = tl args;
                return 0;
        }

        opt := nextarg[1];
        if (len nextarg > 2)
                curropt = nextarg[2:];
        args = tl args;
        return opt;
}
