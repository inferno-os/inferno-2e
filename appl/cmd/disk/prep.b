#
# make partition table
#

implement Diskprep;

include "sys.m";
	sys: Sys;
	Dir, sprint, fprint: import sys;

include "draw.m";

include "string.m";
	str: String;

include "arg.m";
	arg: Arg;

Diskprep: module
{
	init:	fn(nil: ref Draw->Context, nil: list of string);
};

BOOTSECS: con 2048;	# 512 byte sectors

ropt, aopt := 0;
changed := 0;
size: int;
secsize: int;
secs: int;

Maxpath: con 4*Sys->NAMELEN;
Maxpart: con 8;


# Plan9/Inferno partitions

Partition: adt
{
	name: string;
	start:	int;
	end:	int;
};
npart := 0;
ptab := array [Maxpart+2] of Partition;

char *secbuf;
dosend:	int;	# end of any active dos partition 
stderr: ref Sys->FD;
stdin: ref Sys->FD;

init(nil: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	str = load String String->PATH;
	arg = load Arg Arg->PATH;

	sys->pctl(Sys->NEWPGRP|Sys->FORKNS|Sys->FORKFD, nil);

	stdin = sys->fildes(0);
	stderr = sys->fildes(2);
	if(arg == nil)
		error(sys->sprint("can't load %s: %r", Arg->PATH));

	arg->init(args);
	while((c := arg->opt()) != 0)
		case c {
		'r' =>
			ropt++;
		'a' =>
			aopt++;
		'f' =>
			force = 1;
		* =>
			usage();
		}
	args = arg->argv();
	if(len args != 1){
		fprint(stderr, "prep [-raf] special\n");
		exit;
	}

	prepare(hd args);

	if(changed) 
		print("\n *** PARTITION TABLE HAS BEEN MODIFIED ***\n");
}

usage()
{
	fprint(stderr, "prep [-raf] special\n");
	exit;
}

error(s: string)
{
	fprint(stderr, "prep: %s\n", s);
	exit;
}

readline(): string
{
	buf := array [128] of byte;
	n := sys->read(stdin, buf, len buf);
	if(n <= 0)
		return nil;
	return string buf[0:n];
}

rddiskinfo(special: string): int
{
	name: string;

	n := len special;
	if(n >= 4 && special[n-4:] == "disk")
		special = special[0:n-4];
	name := special+"disk";
	(i, d) := sys->stat(name);
	if(i < 0)
		error("stating %s: %r", name);
	size = d.length;

	name = special+"partition";
	(i, d) = sys->stat(name);
	if(i < 0)
		error("stating %s: %r", name);
	secsize = d.length;
	secs = size/secsize;
	secbuf = array [secsize+1] of byte;

	npart = 2;

	ptab[0].name = "disk";
	ptab[0].start = 0;
	ptab[0].end = secs;

	ptab[1].name = "partition";
	ptab[1].start = secs - 1;
	ptab[1].end = secs;

	fd := sys->open(name, Sys->ORDWR);
	if(fd == nil)
		error("can't open %s: %r", name);

	print("sector = %d bytes, disk = %d sectors, all numbers are in sectors\n", secsize, size/secsize);
	return fd;
}

prepare(special: string)
{
	pp: array of Partition;

	fd := rddiskinfo(special);	
	rddosinfo(special);

	delete := 0;
	automatic := aopt;
	if(aopt == 0)
		case read_table(fd) {
		-1 =>
			automatic++;
		* =>
			print("Plan 9 / Inferno partition table exists\n");
			print_table(special);
			print("(a)utopartition, (d)elete, (e)dit or (q)uit ?");
			line := readline();
			if(line == nil)
				quit();
			if(ropt)
				return;
			switch (*line) {
				case 'a':
					delete++;
					automatic++;
					break;
				case 'd':
					delete++;
					break;
				case 'e':
					break;
				default:
					return;
			}
		}

	if(delete){
		for(i = 2; i < Maxpart+2; i++){
			pp = ptab[i:];
			pp[0].name = nil;
			pp[0].start = 0;
			pp[0].end = 0;
			pp++;
		}
		npart = 2;
	}

	if(npart == 2 && dosend && dosend < ptab[0].end){
		ptab[2].name = "dos";
		ptab[2].start = 0;
		ptab[2].end = dosend;
	}

	if(automatic)
		print("Setting up default Inferno partitions\n");
	else
		print_table(special);

	npart = 2;
	for(;;) {
		if(automatic){
			auto_table();
			automatic = 0;
		} else
			edit_table();
		if(aopt)
			break;

		print_table(special);
		print("\nOk [y/n]:");
		line := readline();
		if(line == 0)
			return;
		if(line[0] == '\n' || line[0] == 'y')
			break;
	}
	write_table(fd);
}


# make a boot area, nvram, and file system (leave DOS partition)

auto_table()
{
	total := ptab[0].end - ptab[0].start - 1;
	next := 0;
	npart = 2;
	pp := ptab[2:];
	if(ptab[2].name == "dos"){
		next += ptab[2].end - ptab[2].start;
		npart++;
	}

	if(total-next < BOOTSECS)
		error("not enough room for boot area");

	ptab[npart].name = "boot";
	ptab[npart].start = next;
	next += BOOTSECS;
	ptab[npart].end = next;
	npart++;

	#  two blocks for nvram

	ptab[npart].name = "nvram";
	ptab[npart].start = next;
	next += 2;
	ptab[npart].end = next;
	npart++;

	# the rest is file system

	ptab[npart].name = "fs";
	ptab[npart].start = next;
	next = total;
	ptab[npart].end = next;
	npart++;
}

edit_table()
{
	sofar := 0;

	for(i := 2; i < Maxpart+2; i++){
		pp := ptab[i:];
		for(;;){
			print(" %d name (%s [- to delete, * to quit]):", i, pp[0].name);
			line := readline();
			if(pp[0].name == nil){
				pp[0].start = sofar;
				pp[0].end = secs - 1;
			}
			if(line == nil || line[0] == '*')
				return;
			if(line[0] == '-'){
				pp[0].name = nil;
				for(j := i; j < Maxpart+1; j++)
					ptab[j] = ptab[j+1];
			} else if(line[0] != '\n'){
				line = line[0:len len-1];
				pp[0].name = line;
				break;
			} else if(pp[0].name != nil)
				break;
		}

		do {
			print(" %d start (%d):", i, pp[0].start);
			line = readline();
			if(line == nil)
				return;
			if(line[0] >= '0' && line[0] <= '9')  
				pp[0].start = int line;
			if(pp[0].end == 0)
				pp[0].end = pp[0].start;
		}while(pp[0].start < 0 || pp[0].start >= ptab[0].end);

		do {
			print(" %d length (%d):", i, pp[0].end-pp[0].start);
			line = readline();
			if(line == nil)
				return;
			if(line[0] >= '0' && line[0] <= '9')  
				pp[0].end = pp[0].start + int line;
		}while(pp[0].end < 0 || pp[0].end > ptab[0].end ||
			pp[0].end < pp[0].start);
		sofar = pp[0].end;

		npart = i+1;
	}
}
	
read_table(fd: ref Sys->FD): int
{
	int lines;
	char *line[Maxpart+1];
	char *field[3];
	Partition *pp;

	if((i := read(fd, secbuf, secsize)) < 0)
		error("reading partition table", 0);
	secbuf[i] = 0;

	setfields("\n");
	lines = getfields(secbuf, line, Maxpart+1);
	setfields(" \t");

	if(strcmp(line[0], "plan9 partitions") != 0)
		return -1;

	for(i = 1; i < lines; i++){
		pp = ptab[i:];
		if(getfields(line[i], field, 3) != 3)
			break;
		if(strlen(field[0]) > NAMELEN)
			break;
		strcpy(pp[0].name, field[0]);
		pp[0].start = strtoul(field[1], 0, 0);
		pp[0].end = strtoul(field[2], 0, 0);
		if(pp[0].start > pp[0].end || pp[0].start >= ptab[0].end)
			break;
		npart++;
	}

	return npart;
}

print_table(special: string)
{
	(nil, basename) := str->splitr(special, "/");
	if(basename == nil)
		basename = special;

	print("\nNr Name                 Overlap       Start      End        %%     Size\n");
	for(i := 0; i < npart; i++) {
		pp := ptab[i:];
		buf := basename+pp[0].name;
		s1 := pp[0].start;
		e1 := pp[0].end;
		op := "";
		for(j := 0; j < npart; j++){
			s2 = ptab[j].start;
			e2 = ptab[j].end;
			if(s1 < e2 && e1 > s2)
				op[len op] = '0' + j;
			else
				op[len op] = '-';
		}
		for(; j < Maxpart+2; j++)
			op[len op] = ' ';
		ps = pp[0].end - pp[0].start;
		print("%2d %-20.20s %s %8d %8d %8d %8d\n", i, buf, overlap,
			pp[0].start, pp[0].end, (ps*100)/secs, ps);
	}
}

write_table(fd: ref Sys->FD)
{
	off: int;

	if(ropt)
		return;

	if(sys->seek(fd, 0, 0) < 0)
		error("seeking table: %r");

	secbuf := array [secsize] of {* => byte 0};
	s := "plan9 partitions\n";
	for(i := 2; i < npart; i++){
		pp := ptab[i:];
		s += sys->sprint("%s %d %d\n", pp->name, pp->start, pp->end);
	}
	secbuf[0:] = array of byte s;
	if(write(fd, secbuf, secsize) != secsize)
		error("writing table: %r");

	changed = 1;
}


#  DOS boot sector and partition table

DOSpart: adt
{
	uchar flag;		# active flag 
	uchar shead;		# starting head 
	uchar scs[2];		# starting cylinder/sector 
	uchar type;		# partition type 
	uchar ehead;		# ending head 
	uchar ecs[2];		# ending cylinder/sector 
	uchar start[4];		# starting sector 
	uchar len[4];		# length in sectors 
};
DOSptab: adt
{
	DOSpart p[4];
	uchar magic[2];		# 0x55 0xAA 
};
enum
{
	DOSactive=	0x80,	# active partition 

	DOS12=		1,	# 12-bit FAT partition 
	DOS16=		4,	# 16-bit FAT partition 
	DOSext=		5,	# extended partition 
	DOShuge=	6,	# "huge" partition 

	OS2hpfs=	7,
	OS2boot=	0xa,

	Minix=		0x81,	# Minux, and `old' Linux 
	Linuxswap=	0x82,	# Linux swap partition 
	Linuxfs=	0x83,	# Linux native (ext2fs) 

	BSD386=		0xa5,

	BSDIfs=		0xb7,
	BSDIswap=	0xb8,

	DOSmagic0=	0x55,
	DOSmagic1=	0xAA,
};

GL(p: array of byte): int
{
	return ((int p[3]<<24)|(int p[2]<<16)|(int p[1]<<8)| int p[0]);
}

rddosinfo(special: string)
{
	char name[Maxpath];
	int fd;
	char *t;
	DOSptab b;
	DOSpart *dp;
	ulong end;

	sprint(name, "%sdisk", special);
	fd = sys->open(name, Sys->OREAD);
	if(fd < 0)
		error("opening %s", name);
	sys->seek(fd, 446, 0);
	if(sys->read(fd, &b, sizeof(b)) != sizeof(b))
		return;
	close(fd);
	if(b.magic[0] != DOSmagic0 || b.magic[1] != DOSmagic1)
		return;

	print("\nDOS partition table exists\n");
	print("\nNr Type           Start      Len\n");
	for(dp = b.p; dp < &b.p[4]; dp++){
		case dp->dtype {
		case 0:
			continue;
		case DOS12:
			t = "12 bit FAT";
			break;
		case DOS16:
			t = "16 bit FAT";
			break;
		case DOSext:
			t = "extended";
			break;
		case DOShuge:
			t = "huge";
			break;
		case OS2hpfs:
			t = "OS/2 HPFS";
			break;
		case OS2boot:
			t = "OS/2 boot";
			break;
		case Minix:
			t = "Minix/Linux";
			break;
		case Linuxswap:
			t = "Linux swap";
			break;
		case Linuxfs:
			t = "Linux";
			break;
		case BSD386:
			t = "BSD/386";
			break;
		case BSDIfs:
			t = "BSDI fs";
			break;
		case BSDIswap:
			t = "BSDI swap";
			break;
		default:
			t = "unknown";
			break;
		}
		print("%2d %-11.11s %8d %8d %s\n", dp-b.p, t, 
			GL(dp->start), GL(dp->len),
			dp->flag==DOSactive ? "Active" : "");
		end = GL(dp->start) + GL(dp->len);
		if(end > dosend)
			dosend = end;
	}
	print("\n");
}
