implement StyxServer;

#
#	Module:		devXYZ
#	Author:		Ravi Sharma (sharma@lucent.com)	
#	Merged to 2.3:	Eric Van Hensbergen (ericvh@lucent.com)		#	Purpose:		Framework synthetic clone file service using dev.b
#

include "sys.m";
	sys:	Sys;
	Qid, CHDIR:	import Sys;
include "draw.m";
include "styx.m";
	styx:	Styx;
include "dev.m";
	dev:	Dev;
	Dirtab:	import dev;

mountfd:	ref Sys->FD;

debug:	con 0;

#
# Enumeratied QID Path Constants
#
Qroot,
Qclone,
Qdir,
Qdata,
Qctl : con iota;

Ctl,
Dat : con iota;		# positions in subdir table

Container,
Contents : con iota;

Ndirs: 		con 16;		# entries for subdirectories 
Nfixed:		con 1;		# clone entry
NXYZtab:		con Ndirs + Nfixed;

NBTYPE:		con 4;
MTYPE:			con 16rf;	# mask of NBTYPE bits; type field

NBCONV:		con 4;
MCONV:			con 16rf;	# mask of NBCONV bits; conversation field

QFREE:			con 16r40000000;	# unused table entry
YSTAMP:		con int 'Y';		# XYZ qid mark


#
# functions for set/get field
#
QID_path(c, t: int): int
{
	return ((c << NBTYPE) | (t & MTYPE));
}

TYPE(qid: Qid): int
{
	return (qid.path & MTYPE);
}

CONV(qid: Qid): int
{
	return ((qid.path >> NBTYPE) & MCONV);
}

QID_path_root(): int
{
	return (QID_path(0, Qroot) | CHDIR);
}

#
# Device Table
#
XYZtab := array[ NXYZtab ] of Dirtab;		# top-level directory
XYZdir := array[ Ndirs ] of { * => array[2] of Dirtab };  # each subdirectory

XYZbuf := array[ Ndirs ] of string;	# content of each data file

connect() :array of ref Sys->FD
{
	fds := array[2] of ref Sys->FD;
	if (sys->pipe(fds) > 0) {
		sys->raise("fail: pipe failed");
		return nil;
	}

	return fds;
}

mount()
{
	if (sys->mount(mountfd, "/tmp", sys->MBEFORE, nil) < 0) {
		sys->raise("fail: mount: ");
		return;
	}
	mountfd = nil;	
}

reset( )
{

}

init(nil: ref Draw->Context, nil: list of string)
{
	if (sys == nil)
		sys = load Sys Sys->PATH;

	styx = load Styx Styx->PATH;
	if (styx == nil) {
		sys->raise("fail: couldn't load styx module");
		return;
	}	

	dev = load Dev Dev->PATH;
	if (dev == nil) {
		sys->raise("fail: couldn't load dev module");
		return;
	}

	me := load StyxServer "$self";
	if (me==nil) {
		sys->print("Couldn't load $self - update your kernel\n");
		exit;
	}		

	if(debug)
		sys->print("XYZ: setting up directory table\n");

	XYZtab[0] = Dirtab("clone", Qid(Qclone, YSTAMP), 0, 8r666);
	for(i := 0; i < Ndirs; i++)
		XYZtab[Nfixed + i] = Dirtab(
			string i, 
			Qid((QID_path(i, Qdir) | CHDIR | QFREE), YSTAMP),
			0,
			8r555);

	if(debug)
		for (i=0; i<len XYZtab; i++)
			sys->print("init: entry for %s\n", XYZtab[i].name);
	
	mountfds := connect();
	mountfd = mountfds[0];
	

	dev->init(styx, "#devXYZ");
	styx->init(me, mountfds[1]);

	mount();	
}

attach(c: ref Chan, spec: string): ref Chan
{
	qid: Qid;

	dev->devattach(c, spec);

	c.qid = (Qid)(QID_path_root(), YSTAMP);

	if(debug)
		sys->print("XYZ: attaching qid %d\n", TYPE(qid));

	return c;
}

clone(c: ref Chan, nc: ref Chan): ref Chan
{
	return dev->devclone(c, nc);
}

walk(c: ref Chan, name: string): int
{
	if(debug)
		sys->print("XYZ: walk: qid-in %d\n", TYPE(c.qid));

	if ((c.qid.path & CHDIR) == 0) {
		sys->raise("fail: "+Dev->Enotdir);
		return -1;
	}
	if (name == ".") 
		return 1;
	else if (name == "..") {
		case (TYPE(c.qid)) {
			Qroot =>
				sys->raise("fail: .. from Qroot!");
			Qdir =>
				c.qid = (Qid)(QID_path_root(), YSTAMP);
				return 1;
			* =>
				sys->raise("fail: walk: unexpected type " +
					string TYPE(c.qid));
		}
	} else {
		tab := get_dirtab(c.qid, Contents);
		dir := ref Sys->Dir;
		for(i := 0;; i++) {
			if (dev->devgen(i, tab, dir) < 0) {
				sys->raise("fail: "+Dev->Enotexist);
				return -1;
			}
			if (name == dir.name) {
				c.qid = dir.qid;
				break;
			}
		}
	}

	c.path = name;

	if(debug)
		sys->print("XYZ: walk: qid-out %d\n", TYPE(c.qid));

	return 1;
}

open(c: ref Chan, mode: int): ref Chan
{
	if(debug)
		sys->print("XYZ: open: qid %d\n", TYPE(c.qid));

	case (TYPE(c.qid)) {
		Qclone =>
			c.qid = alloc_subdir();
			dt := get_dirtab(c.qid, Container);
			return dev->devopen(c, mode, dt);
			break;
		* =>
			dt := get_dirtab(c.qid, Container);
			return dev->devopen(c, mode, dt);
			break;
	}

	return nil;
}

create(c: ref Chan, name: string, mode, perm: int)
{	
	dev->devcreate(c, name, mode, perm);
}

read(c: ref Chan, buf: array of byte, count: int, offset: int): int
{
	n := 0;

	if(debug)
		sys->print("XYZ: read: qid %d\n", TYPE(c.qid));

	case (TYPE(c.qid)) {
		Qroot or 
		Qdir =>
			tab := get_dirtab(c.qid, Contents);
			n = dev->devdirread(c, buf, count,  tab);
			break;
		Qclone =>
			sys->raise("fail: read from Qclone, not Qctl?!");
		Qctl =>
			# return the conversation number
			n = dev->readstr(c.offset, buf, count, string CONV(c.qid));
			break;
		Qdata =>
			n = dev->readstr(offset, buf, count, XYZbuf[CONV(c.qid)]);
			break;
		* =>
			sys->raise("fail: walk: unexpected type " +
				string TYPE(c.qid));
	}
	return n;
}

write(c: ref Chan, buf:array of byte, count: int, nil: int): int
{
	n := 0;

	conv := CONV(c.qid);

	if(debug)
		sys->print("XYZ: write: qid %d\n", TYPE(c.qid));

	case (TYPE(c.qid)) {
		Qroot or
		Qdir =>
			sys->raise("fail: file is a directory");
		Qclone =>
			sys->raise("fail: " + Dev->Eperm);
		Qctl =>
			# parse the control message here
			# instead, we just make it our data buffer
			XYZbuf[conv] = "ctl: " + string buf;
		Qdata =>
			XYZbuf[conv] = string buf;
		* =>
			sys->raise("fail: walk: unexpected type " +
				string TYPE(c.qid));
	};
	return count;
}

clunk(c: ref Chan)
{
	dev->devclunk(c);
}

remove(c: ref Chan)
{
	dev->devremove(c);
}

stat(c: ref Chan, db: array of byte)
{
	if(debug)
		sys->print("XYZ: stat: qid %d\n", TYPE(c.qid));

	dt := get_dirtab(c.qid, Container);
	dev->devstat(c, db,  dt);
}

wstat(c: ref Chan, db: array of byte)
{
	dev->devwstat(c, db);
}

get_dirtab(qid: Qid, which: int): array of Dirtab
{
	c : int;

	case (TYPE(qid)) {
		Qroot =>
			if(debug)
				sys->print("XYZ: get_dirtab: Qroot\n");
			if(which == Contents) {
				newtab := array[Ndirs] of Dirtab;
				tabcount := 0;
				for(i := 0; i < Ndirs; i++) {
					if (!(XYZtab[i].qid.path & QFREE))
						newtab[tabcount++] = XYZtab[i];
				}
				return newtab[0:tabcount];
			}
		Qclone =>
			if(debug)
				sys->print("XYZ: get_dirtab: Qclone\n");
			if(which == Contents)
				sys->raise("fail: get contents of Qclone?!");
			return XYZtab;
		Qdir =>
			if(which == Contents) {
				if(debug)
					sys->print("XYZ: get_dirtab: Qdir: Contents\n");
				# Contents (read)
				c = CONV(qid);
				if(c < 0 || c > len XYZdir)
					sys->raise("fail: conv out of range");
				return XYZdir[c];
			}
			if(debug)
				sys->print("XYZ: get_dirtab: Qdir: Container\n");
			# Container
			return XYZtab;
		Qdata or
		Qctl =>
			if(debug)
				sys->print("XYZ: get_dirtab: Qdata/Qctl\n");
			if(which == Contents)
				sys->raise("fail: get contents of Qdata/Qctl?!");
			c = CONV(qid);
			if(c < 0 || c > len XYZdir)
				sys->raise("fail: conv out of range");
			return XYZdir[c];
		* =>
			sys->raise("fail: walk: unexpected type " +
				string TYPE(qid));
	}
	# NOTREACHED
	return nil;
}

alloc_subdir(): Sys->Qid
{
	# find an unused slot
	for(i := Nfixed; i < NXYZtab; i++)
		if(XYZtab[i].qid.path & QFREE)
			break;

	if (i == NXYZtab)
		sys->raise("fail: no free devices");

	if(debug)
		sys->print("XYZ: alloc_subdir: using slot %d\n", i);

	j := i - Nfixed;
	XYZdir[j][Ctl] = Dirtab("ctl",  Qid(QID_path(j, Qctl),  YSTAMP), 0, 8r666);
	XYZdir[j][Dat] = Dirtab("data", Qid(QID_path(j, Qdata), YSTAMP), 0, 8r666);

	# update XYZtab
	XYZtab[i].qid.path &= ~QFREE;

	return XYZdir[j][Ctl].qid;
}

