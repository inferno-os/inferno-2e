implement StyxServer;

#
#	Module:		dev123
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#	Purpose:		Synthetic hierarchy using dev.b
#

include "sys.m";
	sys:	Sys;
include "draw.m";
include "styx.m";
	styx:	Styx;

include "dev.m";
	dev:		Dev;
	Dirtab:	import dev;

mountfd:	ref Sys->FD;

#
# Enumeratied QID Path Constants
#

Qtopdir, Qdir, Qdata1, Qdata2, Qdata3 : con iota;

#
# Device Table
#

Dirnode: adt{
	qid:		Sys->Qid;
	parent:	Sys->Qid;
	tab:		array of Dev->Dirtab;
};

tree := array [] of {
	Dirnode( (Qtopdir|Sys->CHDIR,0), (Qtopdir|Sys->CHDIR,0),
		array [] of {
			Dirtab ("test",(Qdir|Sys->CHDIR,0), 0, 8r555),
		}),
	Dirnode( (Qdir|Sys->CHDIR,0),(Qtopdir|Sys->CHDIR,0),
		array[] of {
			Dirtab ("test1",(Qdata1,0),0,8r666),
			Dirtab ("test2",(Qdata2,0),0,8r666),
			Dirtab ("test3",(Qdata3,0),0,8r666),
		}),
};

#
# The following procedure is generalized, but not very efficient.
# The best approach is to structure your Qid's to allow easy lookup
# in the Dirnode tree.  However, the following procedure should work
# for small hierarchical synthetic file systems.
#

lookupnode( qid: Sys->Qid, tree: array of Dirnode ) :ref Dirnode
{
	n: ref Dirnode;
	for (count := 0; count < len tree; count++) 
		if (dev->eqqid( qid, tree[count].qid))
			return ref tree[count];
		else
			for (count2 := 0; count2 < len tree[count].tab; count2++)
				if (dev->eqqid( qid, tree[count].tab[count2].qid))
					n = ref tree[count];

	return n;
}

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
	if (sys->mount(mountfd, "/dev", sys->MBEFORE, nil) < 0) {
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

	dev->init(styx, "#123");
	styx->init(me, mountfds[1]);
	mount();
}

clone(c: ref Chan, nc: ref Chan): ref Chan
{
	return dev->devclone(c, nc);
}

walk(c: ref Chan, name: string): int
{
	if (name == "..") {
		n := lookupnode(c.qid, tree);
		if (n == nil) {
			sys->raise("fail: "+Dev->Eio);
			return -1;
		}
		c.qid = n.parent;
		return 1;
	}
	return dev->devwalk(c, name, lookupnode( c.qid, tree ).tab);
}

open(c: ref Chan, mode: int): ref Chan
{
	return dev->devopen(c, mode,  lookupnode( c.qid, tree ).tab);
}


create(c: ref Chan, name: string, mode, perm: int)
{	
	dev->devcreate(c, name, mode, perm);
}


read(c: ref Chan, buf: array of byte, n: int, nil: int): int
{
	case (c.qid.path & ~Sys->CHDIR) {
		Qtopdir or Qdir =>
			n = dev->devdirread(c, buf, n,  lookupnode( c.qid, tree ).tab);
			break;
		Qdata1 =>
			n = dev->readstr( c.offset, buf, n, "this is data1\n");			
			break;
		Qdata2 =>
			n = dev->readstr( c.offset, buf, n, "this is data2\n");
			break;
		Qdata3 =>
			n = dev->readstr( c.offset, buf, n, "this is data3\n");
			break;
		* =>
			sys->raise("fail: "+Dev->Enotexist);		
			n= 0;
	};
	return n;
}


write(c: ref Chan, nil:array of byte, n: int, nil: int): int
{
	case (c.qid.path & ~Sys->CHDIR) {
		Qdata1 =>
			break;
		Qdata2 =>
			break;
		Qdata3 =>
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
	dev->devstat(c, db, lookupnode( c.qid, tree ).tab);
}


wstat(c: ref Chan, db: array of byte)
{
	dev->devwstat(c, db);
}


attach(c: ref Chan, spec: string): ref Chan
{
	return dev->devattach( c, spec );
}

