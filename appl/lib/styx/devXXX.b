implement StyxServer;

#
#	Module:		devXXX
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#	Purpose:	Framework synthetic file service using dev.b
#

include "sys.m";
	sys:	Sys;
include "draw.m";
include "styx.m";
	styx:	Styx;

include "dev.m";
	dev: Dev;
	Dirtab:	import dev;

mountfd:	ref Sys->FD;

#
# Enumeratied QID Path Constants
#

Qdir, Qdata: con iota;

#
# Device Table
#

tab :=	 array[] of {
		Dirtab ("data",(Qdata,0),0,8r600),
};

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
	if (sys->mount(mountfd, "/dev", Sys->MAFTER, nil) < 0) {
		sys->raise("fail: mount: ");
		return;
	}
	mountfd = nil;	
}

reset()
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

	mountfds := connect();	
	mountfd = mountfds[0];	

	dev->init(styx, "devXXX");
	styx->init(me, mountfds[1]);
	mount();
}

clone(c: ref Chan, nc: ref Chan): ref Chan
{
	return dev->devclone(c, nc);
}

walk(c: ref Chan, name: string): int
{
	return dev->devwalk(c, name, tab);
}

open(c: ref Chan, mode: int): ref Chan
{
	return dev->devopen(c, mode,  tab);
}


create(c: ref Chan, name: string, mode, perm: int)
{	
	dev->devcreate(c, name, mode, perm);
}


read(c: ref Chan, buf: array of byte, n: int, nil: int): int
{
	case (c.qid.path & ~Sys->CHDIR) {
		Qdir =>
			n = dev->devdirread(c, buf, n,  tab);
			break;
		Qdata=>
			n = dev->readstr( c.offset, buf, n, "this is some data in the file\n");		
			break;
		* =>
			n = 0;
	};
	return n;
}


write(c: ref Chan, nil:array of byte, n: int, nil: int): int
{
	case (c.qid.path & ~Sys->CHDIR) {
		Qdata =>
			break;
		* =>
			sys->raise("fail: "+Dev->Enotexist);
			return 0;
	};
	return n;
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
	dev->devstat(c, db,  tab);
}


wstat(c: ref Chan, db: array of byte)
{
	dev->devwstat(c, db);
}


attach(c: ref Chan, spec: string): ref Chan
{
	return dev->devattach( c, spec );
}

