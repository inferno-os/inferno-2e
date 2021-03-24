#
# make a basic Plan9/Inferno partition table
#

implement Diskpart;

include "sys.m";
	sys: Sys;

include "draw.m";

include "arg.m";
	arg: Arg;

Sector: con 512;
Bootlen: con 900000/Sector;

# Plan9/Inferno partitions

Partition: adt
{
	name: string;
	start:	int;
	end:	int;
};

Diskpart: module
{
	init:	fn(nil: ref Draw->Context, nil: list of string);
};

stdout: ref Sys->FD;
stderr: ref Sys->FD;

init(nil: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	arg = load Arg Arg->PATH;

	sys->pctl(Sys->NEWPGRP|Sys->FORKFD, nil);

	stdout = sys->fildes(1);
	stderr = sys->fildes(2);
	if(arg == nil)
		error(sys->sprint("can't load %s: %r", Arg->PATH));

	wflag := 0;
	arg->init(args);
	while((c := arg->opt()) != 0)
		case c {
		'w' =>
			wflag = 1;
		* =>
			usage();
		}
	args = arg->argv();
	if(len args != 1)
		usage();
	dev := hd args;
	(i, d) := sys->stat(dev+"disk");
	if(i < 0)
		error(sys->sprint("can't stat %s: %r", dev+"disk"));
	nsec := d.length/512;
	if(nsec <= 0)
		error(sys->sprint("%s: too small (%d bytes)", dev+"disk", d.length));
	fd := sys->open(dev+"partition", Sys->ORDWR);
	if(fd == nil)
		error(sys->sprint("%s: can't open partition table: %r", dev+"partition"));
	pl := readpart(fd, nsec);
	if(pl != nil && !wflag){
		s := maketable(pl);
		sys->print("existing ");
		sys->write(stdout, array of byte s, len s);
		exit;
	}
	disk := Partition("disk", 0, nsec);
	avail := Partition("avail", 16, nsec-1);
	(pl, avail) = alloc(nil, avail, "dos", 128*1024/Sector);
	(pl, avail) = alloc(pl, avail, "boot", Bootlen);
	(pl, avail) = alloc(pl, avail, "nvram", 4);
	(pl, avail) = alloc(pl, avail, "fs", avail.end-avail.start);
	partblock := array [Sector] of { * => byte 0 };
	s := maketable(pl);
	if(wflag){
		partblock[0:] = array of byte s;
		sys->seek(fd, 0, 0);
		if(sys->write(fd, partblock, len partblock) != len partblock)
			error(sys->sprint("%s: error writing partition table: %r", dev));
	}
	sys->write(stdout, array of byte s, len s);
}

alloc(pl: list of Partition, avail: Partition, name: string, length: int): (list of Partition, Partition)
{
	p := Partition(name, avail.start, avail.start+length);
	avail.start += length;
	if(avail.start > avail.end)
		error(sys->sprint("not enough space for %s partition", name));
	return (app(pl,p), avail);
}

readpart(fd: ref Sys->FD, limit: int): list of Partition
{
	pl: list of Partition;
	p: Partition;

	buf := array [Sector] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < len buf)
		return nil;
	buf[n-1] = byte 0;
	(nf, f) := sys->tokenize(string buf, "\n");
	if(nf < 1 || hd f != "plan9 partitions")
		return nil;
	while((f = tl f) != nil){
		(nil, pf) := sys->tokenize(hd f, " \t");
		if(len pf != 3)
			break;
		p.name = hd pf;
		pf = tl pf;
		p.start = int hd pf;
		pf = tl pf;
		p.end = int hd pf;
		if(p.start > p.end || p.start >= limit)
			break;
		pl = app(pl, p);
	}
	return pl;
}

maketable(pl: list of Partition): string
{
	s := "plan9 partitions\n";
	for(; pl != nil; pl = tl pl){
		pp := hd pl;
		s += sys->sprint("%s %d %d\n", pp.name, pp.start, pp.end);
	}
	return s;
}

app(pl: list of Partition, p: Partition): list of Partition
{
	if(pl == nil)
		return p :: nil;
	return hd pl :: app(tl pl, p);
}

usage()
{
	sys->fprint(stderr, "Usage: disk/part device\n");
	exit;
}

error(s: string)
{
	sys->fprint(stderr, "part: %s\n", s);
	exit;
}
