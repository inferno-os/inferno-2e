implement Dosfs;

include "sys.m";
	sys : Sys;
	sprint: import sys;

include "iotrack.m";
	iotrack : IoTrack;
	Xfs,Xfile: import iotrack;

include "styx.m";
	styx: Styx;
	Smsg: import styx;

include "dosfs.m";

include "dossubs.m";
	Dos: DosSubs;
	Dosptr,Dosbpb, Dosdir, Dosslot: import Dos;

g: ref Dos->Global;
initialised := 0;
debug:	con 1;

init(deffile: string, logfile: string, chatty: int)
{
	g = ref Dos->Global;
	sys = load Sys Sys->PATH;

	# Try and load from the normal file system,
	# if it fails try loading from kernel root
	#
	styx = load Styx Styx->PATH;
	if(styx == nil) {
		styx = load Styx "#/./styx";
		iotrack = load IoTrack "#/./iotrack";
		Dos = load DosSubs "#/./dossubs";
	}
	else {
		iotrack = load IoTrack IoTrack->PATH; 
		Dos = load DosSubs DosSubs->PATH; 
	}

	g.Dos = Dos;
	g.deffile = deffile;
	g.logfile = logfile;
	g.chatty = chatty;
	g.iotrack = iotrack;

	Dos->init(g);

	iotrack->iotrack_init(g);

	initialised = 1;
}

setup()
{
	Dos->setup();
}
	
rnop()
{
	if(debug)
		Dos->chat("nop...");
}

rflush()
{
	if(debug)
		Dos->chat("flush...");
}

rattach()
{
	xf : ref Xfs;
	root : ref Xfile;

	if(debug)
		Dos->chat(sprint("attach(fid=%d,uname=\"%s\",aname=\"%s\")...",
		g.thdr.fid, g.thdr.uname, g.thdr.aname));

	root = iotrack->xfile(g.thdr.fid, Dos->Clean);
	if(root == nil) {
		g.errno = Dos->Enomem;
		return;
	}
	root.xf = xf = iotrack->getxfs(g.thdr.aname);
	if(xf == nil) {
		if(root!=nil)
			iotrack->xfile(g.thdr.fid, Dos->Clunk);
		return;
	}
	if(xf.fmt == 0 && Dos->dosfs(xf) < 0){
		g.errno = Dos->Eformat;
		if(root!=nil)
			iotrack->xfile(g.thdr.fid, Dos->Clunk);
		return;
	}

	root.qid.path = Sys->CHDIR;
	root.qid.vers = 0;
	root.xf.rootqid = root.qid;
	g.rhdr.qid = root.qid;
	return;
}

rclone()
{
	ofl := iotrack->xfile(g.thdr.fid, Dos->Asis);
	nfl := iotrack->xfile(g.thdr.newfid, Dos->Clean);

	if(debug)
		Dos->chat(sprint("clone(fid=%d,newfid=%d)...",
			g.thdr.fid, g.thdr.newfid));

	if(ofl == nil)
		g.errno = Dos->Eio;
	else
	if(nfl == nil)
		g.errno = Dos->Enomem;
	else {
		next := nfl.next;
		dp := nfl.ptr;
		*nfl = *ofl;
		nfl.ptr = dp;
		nfl.next = next;
		nfl.fid = g.thdr.newfid;
		iotrack->refxfs(nfl.xf, 1);
		*nfl.ptr = *ofl.ptr;
		dp.p = nil;
	}
}

rwalk()
{
	f := iotrack->xfile(g.thdr.fid, Dos->Asis);
	dp: ref Dosptr;
	r: int;

	if(debug)
		Dos->chat(sprint("walk(fid=%d,name=\"%s\")...",
			g.thdr.fid, g.thdr.name));

	if(f==nil) {
		if(debug)
			Dos->chat("no xfile...");
		g.errno = Dos->Enonexist;
		return;
	}

	if((f.qid.path & Sys->CHDIR) == 0){
		if(debug)
			Dos->chat(sprint("qid.path=0x%x...", f.qid.path));
		g.errno = Dos->Enonexist;
		return;
	}

	if(g.thdr.name == ".") {
		g.rhdr.qid = f.qid;
		return;
	}

	if(g.thdr.name== "..") {
		if(f.qid.path==f.xf.rootqid.path) {
			if (debug)
				Dos->chat("walkup from root...");
			g.rhdr.qid = f.qid;
			return;
		}
		(r,dp) = Dos->walkup(f);
		if(r < 0) {
			g.errno = Dos->Enonexist;
			return;
		}

		f.ptr=dp;
		if(dp.addr == 0)
			f.qid.path = f.xf.rootqid.path;
		else
			f.qid.path = Sys->CHDIR | (dp.addr*(iotrack->Sectorsize/32) + dp.offset/32);
	}
	else {
		if(Dos->getfile(f) < 0) {
			g.errno = Dos->Enonexist;
			return;
		}
		(r,dp) = Dos->searchdir(f, g.thdr.name, 0,1);
		if(r < 0) {
			Dos->putfile(f);
			g.errno = Dos->Enonexist;
			return;
		}

		f.ptr=dp;
		f.qid.path = dp.addr*(iotrack->Sectorsize/32) + dp.offset/32;
		if(dp.addr == 0)
			f.qid.path = f.xf.rootqid.path;
		else {
			d := Dosdir.arr2Dd(dp.p.iobuf[dp.offset:dp.offset+32]);
			if((d.attr & Dos->DDIR) !=  byte 0)
				f.qid.path |= Sys->CHDIR;
		}
		Dos->putfile(f);
	}
	g.rhdr.qid = f.qid;
	return;
}

ropen()
{
	attr: int;
 
	omode := 0;

	if(debug)
		Dos->chat(sprint("open(fid=%d,mode=%d)...",
			g.thdr.fid, g.thdr.mode));

	f := iotrack->xfile(g.thdr.fid, Dos->Asis);
	if(f == nil || (f.flags&Dos->Omodes) != 0) {
		g.errno = Dos->Eio;
		return;
	}

	dp := f.ptr;
	if(dp.paddr && (g.thdr.mode & Styx->ORCLOSE) != 0) {
		# check on parent directory of file to be deleted
		p := iotrack->getsect(f.xf, dp.paddr);
		if(p == nil) {
			g.errno = Dos->Eio;
			return;
		}
		# 57 is the attr byte offset from 	
		# p.iobuf[dp.poffset] when viewed as a
		# Dosdir.
		attr = int p.iobuf[dp.poffset+57];
		iotrack->putsect(p);
		if((attr & int Dos->DRONLY) != 0) {
			g.errno = Dos->Eperm;
			return;
		}
		omode |= Dos->Orclose;
	}
	else
	if(g.thdr.mode & Styx->ORCLOSE)
		omode |= Dos->Orclose;

	if(Dos->getfile(f) < 0) {
		g.errno = Dos->Enonexist;
		return;
	}

	if(dp.addr != 0) {
		d := Dosdir.arr2Dd(dp.p.iobuf[dp.offset:dp.offset+32]);
		attr = int d.attr;
	}
	else
		attr = int Dos->DDIR;

	case (g.thdr.mode & 7) {
	Styx->OREAD or
	Styx->OEXEC =>
		omode |= Dos->Oread;
	Styx->ORDWR =>
		omode |= Dos->Oread;
		omode |= Dos->Owrite;
		if(attr & int Dos->DRONLY) {
			g.errno = Dos->Eperm;
			Dos->putfile(f);
			return;
		}
	Styx->OWRITE => 
		omode |= Dos->Owrite;
		if(attr & int Dos->DRONLY) {
			g.errno = Dos->Eperm;
			Dos->putfile(f);
			return;
		}
	* =>
		g.errno = Dos->Eio;
		Dos->putfile(f);
		return;
	}

	if(g.thdr.mode & Styx->OTRUNC) {
		if((attr & int Dos->DDIR)!=0 || (attr & int Dos->DRONLY) != 0) {
			g.errno = Dos->Eperm;
			Dos->putfile(f);
			return;
		}

		if(Dos->truncfile(f) < 0) {
			g.errno = Dos->Eio;
			Dos->putfile(f);
			return;
		}
	}

	f.flags |= omode;
	if(debug)
		Dos->chat(sprint("f.qid=0x%8.8ux...", f.qid.path));

	g.rhdr.qid = f.qid;
	Dos->putfile(f);
}

rcreate()
{
	bp: ref Dosbpb;
	omode:=0;
	start:=0;
	sname := "";
	islong :=0;
	first :=0;

	if(debug)
		Dos->chat(sprint("creat(fid=%d,name=\"%s\",perm=%x,mode=%d)...",
			g.thdr.fid, g.thdr.name, g.thdr.perm, g.thdr.mode));

	f := iotrack->xfile(g.thdr.fid, Dos->Asis);
	if(f == nil || (f.flags&Dos->Omodes) || Dos->getfile(f)<0) {
		g.errno = Dos->Eio;
		return;
	}

	pdp := f.ptr;
	if(pdp.addr != 0)
		pd := Dosdir.arr2Dd(pdp.p.iobuf[pdp.offset:pdp.offset+32]);
	else
		pd = nil;

	if(pd != nil)
		attr := pd.attr;
	else
		attr = Dos->DDIR;

	if(!(int(attr & Dos->DDIR)) || (int (attr & Dos->DRONLY))) {
		Dos->putfile(f);
		g.errno = Dos->Eperm;
		return;
	}

	if(g.thdr.mode & Styx->ORCLOSE)
		omode |= Dos->Orclose;

	case (g.thdr.mode & 7) {
	Styx->OREAD or
	Styx->OEXEC =>
		omode |= Dos->Oread;
	Styx->OWRITE or
	Styx->ORDWR =>
		if ((g.thdr.mode & 7) == Styx->ORDWR)
			omode |= Dos->Oread;
		omode |= Dos->Owrite;
		if(g.thdr.perm & Sys->CHDIR){
			Dos->putfile(f);
			g.errno = Dos->Eperm;
			return;
		}
	* =>
		Dos->putfile(f);
		g.errno = Dos->Eperm;
		return;
	}

	if(g.thdr.name=="." || g.thdr.name=="..") {
		Dos->putfile(f);
		g.errno = Dos->Eperm;
		return;
	}

	(r,ndp) := Dos->searchdir(f, g.thdr.name, 1, 1);
	if(r < 0) {
		Dos->putfile(f);
		g.errno = Dos->Eexist;
		return;
	}

	nds := Dos->name2de(g.thdr.name);	
	if(nds > 0) {
		# long file name, find "new" short name 
		i := 1;
		for(;;) {
			sname = Dos->long2short(g.thdr.name, i);
			(r1, tmpdp) := Dos->searchdir(f,sname, 0, 0);
			if(r1 < 0)
				break;
			i++;
		}
		islong = 1;
	}

	# allocate first cluster, if making directory
	if(g.thdr.perm & Sys->CHDIR) {
		bp = f.xf.ptr;
		start = Dos->falloc(f.xf);
		if(start <= 0) {
			Dos->putfile(f);
			g.errno = Dos->Eio;
			return;
		}
	}
	
	 # now we're committed
	if(pd != nil) {
		Dos->puttime(pd);
		pdp.p.flags |= IoTrack->BMOD;
	}

	f.ptr = ndp;
	ndp.p = iotrack->getsect(f.xf, ndp.addr);
	if(ndp.p == nil) {
		iotrack->putsect(pdp.p);
		g.errno = Dos->Eio;
		return;
	}

	if(islong) {
		bp = f.xf.ptr;
		first=1;
		i:=0;
		# calculate checksum
		for(sum:=0;i<11;i++)
			sum = (((sum&1)<<7)|((sum&16rfe)>>1))+sname[i];
		nd := ref Dosslot(byte 0,"",byte 16rf,byte 0,byte sum,"",
				array[2] of { * => byte 0},"");
		while(nds > 0) { 
			Dos->putnamesect(g.thdr.name, nds, start, first, nd);
			if(first)
				first=0;
			ndp.p.iobuf[ndp.offset:]= Dosslot.Ds2arr(nd);
			ndp.offset+=32;
			if(ndp.offset == bp.sectsize) {
				if(debug)
					Dos->chat("Moving over SECTOR\n");

				ndp.p.flags |= IoTrack->BMOD;
				iotrack->putsect(ndp.p);
				ndp.addr = ndp.naddr;
				if(ndp.addr < 0) {
					iotrack->putsect(pdp.p);
					g.errno = Dos->Eio;
					return;
				}

				ndp.p = iotrack->getsect(f.xf,ndp.addr);
				if(ndp.p == nil) {
					iotrack->putsect(pdp.p);
					g.errno = Dos->Eio;
					return;
				}
				ndp.offset=0;
			}
			nds--;
		}
	} 
	nd := ref Dosdir(".       ","   ",byte 0,array[10] of { * => byte 0},
			array[2] of { * => byte 0}, array[2] of { * => byte 0},
			array[2] of { * => byte 0},array[4] of { * => byte 0});

	if((g.thdr.perm & 8r222) == 0)
		nd.attr |= Dos->DRONLY;

	Dos->puttime(nd);
	nd.start[0] = byte start;
	nd.start[1] = byte (start>>8);

	if(islong)
		Dos->putname(sname[0:8]+"."+sname[8:11], nd);
	else
		Dos->putname(g.thdr.name, nd);

	f.qid.path = ndp.addr*(IoTrack->Sectorsize/32) + (ndp.offset/32);
	if(g.thdr.perm & Sys->CHDIR) {
		nd.attr |= Dos->DDIR;
		f.qid.path |= Sys->CHDIR;
		xp := iotrack->getsect(f.xf, bp.dataaddr+(start-2)*bp.clustsize);
		if(xp == nil) {
			if(ndp.p!=nil)
				Dos->putfile(f);
			iotrack->putsect(pdp.p);
			g.errno = Dos->Eio;
			return;
		}
		xd := ref *nd;
		xd.name = ".       ";
		xd.ext = "   ";
		xp.iobuf[0:] = Dosdir.Dd2arr(xd);
		if(pd!=nil)
			xd = ref *pd;
		else{
			xd = ref Dosdir("..      ","   ",byte 0,
				array[10] of { * => byte 0},
				array[2] of { * => byte 0},
				array[2] of { * => byte 0},
				array[2] of { * => byte 0},
				array[4] of { * => byte 0});

			Dos->puttime(xd);
			xd.attr = Dos->DDIR;
		}
		xd.name="..      ";
		xd.ext="   ";
		xp.iobuf[32:] = Dosdir.Dd2arr(xd);
		xp.flags |= IoTrack->BMOD;
		iotrack->putsect(xp);
	}

	ndp.p.flags |= IoTrack->BMOD;
	tmp := Dosdir.Dd2arr(nd);
	ndp.p.iobuf[ndp.offset:]= tmp;
	Dos->putfile(f);
	iotrack->putsect(pdp.p);

	f.flags |= omode;
	if(debug)
		Dos->chat(sprint("f.qid=0x%8.8ux...", f.qid.path));

	g.rhdr.qid = f.qid;
	return;
}

rread()
{
	r : int;
	f : ref Xfile;
	data : array of byte;

	if(debug)
		Dos->chat(sprint("read(fid=%d,offset=%d,count=%d)...",
			g.thdr.fid, g.thdr.offset, g.thdr.count));

	if(((f=iotrack->xfile(g.thdr.fid, Dos->Asis))==nil) ||
	    (f.flags&Dos->Oread == 0)) {
		g.errno=Dos->Eio;
		return;
	}

	if((f.qid.path & Sys->CHDIR) != 0) {
		g.thdr.count = (g.thdr.count/Styx->DIRLEN)*Styx->DIRLEN;
		if(g.thdr.count<Styx->DIRLEN || g.thdr.offset%Styx->DIRLEN) {
			if(debug)
				Dos->chat(sprint("count=%d,offset=%d,DIRLEN=%d...",
					g.thdr.count, g.thdr.offset, Styx->DIRLEN));
			g.errno = Dos->Eio;
			return;
		}

		if(Dos->getfile(f) < 0) {
			g.errno = Dos->Eio;
			return;
		}
		(r, data) = Dos->dosreaddir(f, g.thdr.offset, g.thdr.count);
	}
	else {
		if(Dos->getfile(f) < 0) {
			g.errno = Dos->Eio;
			return;
		}
		(r,data) = Dos->readfile(f, g.thdr.offset, g.thdr.count);
	}
	Dos->putfile(f);

	if(r < 0)
		g.errno = Dos->Eio;
	else {
		g.rhdr.count = r;
		g.rhdr.data = data;
		if(debug)
			Dos->chat(sprint("rcnt=%d...", r));
	}
}

rwrite()
{
	if(debug)
		Dos->chat(sprint("write(fid=%d,offset=%d,count=%d)...",
			g.thdr.fid, g.thdr.offset, g.thdr.count));

	if(((f:=iotrack->xfile(g.thdr.fid, Dos->Asis))==nil) || 
	   !(f.flags&Dos->Owrite)) {
		g.errno = Dos->Eio;
		return;
	}
		
	if(Dos->getfile(f) < 0) {
		g.errno = Dos->Eio;
		return;
	}
		
	r := Dos->writefile(f, g.thdr.data,g.thdr.offset, g.thdr.count);
	Dos->putfile(f);

	if(r < 0)
		g.errno = Dos->Eio;
	else {
		g.rhdr.count = r;
		if(debug)
			Dos->chat(sprint("rcnt=%d...", r));
	}
}

rclunk()
{
	if (debug) Dos->chat(sprint("clunk(fid=%d)...", g.thdr.fid));
	iotrack->xfile(g.thdr.fid, Dos->Clunk);
	iotrack->sync();
}

rremove()
{
	prevdo: int;

	f := iotrack->xfile(g.thdr.fid, Dos->Asis);
	if(debug)
		Dos->chat(sprint("remove(fid=%d,name=\"%s\")...",
			g.thdr.fid, g.thdr.name));

	if(f == nil) {
		g.errno = Dos->Eio;
		iotrack->xfile(g.thdr.fid, Dos->Clunk);
		iotrack->sync();
		return;
	}

	if(!f.ptr.addr) {
		if(debug)
			Dos->chat("root...");
		g.errno = Dos->Eperm;
		iotrack->xfile(g.thdr.fid, Dos->Clunk);
		iotrack->sync();
		return;
	}
	
	# check on parent directory of file to be deleted
	parp := iotrack->getsect(f.xf, f.ptr.paddr);
	if(parp == nil) {
		g.errno = Dos->Eio;
		iotrack->xfile(g.thdr.fid, Dos->Clunk);
		iotrack->sync();
		return;
	}

	pard := Dosdir.arr2Dd(parp.iobuf[f.ptr.poffset:f.ptr.poffset+32]);
	if(f.ptr.paddr && int (pard.attr & Dos->DRONLY)) {
		if(debug)
			Dos->chat("parent read-only...");
		iotrack->putsect(parp);
		g.errno = Dos->Eperm;
		iotrack->xfile(g.thdr.fid, Dos->Clunk);
		iotrack->sync();
		return;
	}

	if(Dos->getfile(f) < 0){
		if(debug)
			Dos->chat("getfile failed...");
		iotrack->putsect(parp);
		g.errno = Dos->Eio;
		iotrack->xfile(g.thdr.fid, Dos->Clunk);
		iotrack->sync();
		return;
	}

	dattr := f.ptr.p.iobuf[f.ptr.offset+11];
	if(int (dattr & Dos->DDIR) && Dos->emptydir(f) < 0){
		if(debug)
			Dos->chat("non-empty dir...");
		Dos->putfile(f);
		iotrack->putsect(parp);
		g.errno = Dos->Eperm;
		iotrack->xfile(g.thdr.fid, Dos->Clunk);
		iotrack->sync();
		return;
	}
	if(f.ptr.paddr == 0 && int (dattr&Dos->DRONLY)) {
		if(debug)
			Dos->chat("read-only file in root directory...");
		Dos->putfile(f);
		iotrack->putsect(parp);
		g.errno = Dos->Eperm;
		iotrack->xfile(g.thdr.fid, Dos->Clunk);
		iotrack->sync();
		return;
	}

	f.ptr.p.iobuf[f.ptr.offset] = byte 16re5;
	f.ptr.p.flags |= IoTrack->BMOD;
	for(prevdo = f.ptr.offset-32;prevdo >= 0;prevdo-=32){
		if (f.ptr.p.iobuf[prevdo+11] != byte 16rf)
			break;
		f.ptr.p.iobuf[prevdo] = byte 16re5;
	}

	if (prevdo <= 0 && f.ptr.prevaddr != -1){
		p:=iotrack->getsect(f.xf,f.ptr.prevaddr);
		for(prevdo = f.xf.ptr.sectsize-32;prevdo >= 0;prevdo-=32) {
			if(p.iobuf[prevdo+11] != byte 16rf)
				break;
			p.iobuf[prevdo] = byte 16re5;
			p.flags |= IoTrack->BMOD;
		}
		iotrack->putsect(p);
	}		
	if(f.ptr.paddr) {
		Dos->puttime(pard);
		parp.flags |= IoTrack->BMOD;
	}

	parp.iobuf[f.ptr.poffset:] = Dosdir.Dd2arr(pard);
	iotrack->putsect(parp);
	if(Dos->truncfile(f) < 0)
		g.errno = Dos->Eio;

	Dos->putfile(f);
	iotrack->xfile(g.thdr.fid, Dos->Clunk);
	iotrack->sync();
	return;
}

rstat()
{
	f := iotrack->xfile(g.thdr.fid, Dos->Asis);

	islong :=0;
	prevdo : int;
	longnamebuf:="";

	if(debug)
		Dos->chat(sprint("stat(fid=%d)...", g.thdr.fid));

	if(f == nil || Dos->getfile(f) < 0)
		g.errno = Dos->Eio;
	else {
		# get file info.
		dir := Dos->getdir(f.ptr.p.iobuf[f.ptr.offset:f.ptr.offset+32], 
						f.ptr.addr, f.ptr.offset);
		# get previous entry 
		if(f.ptr.prevaddr == -1) {
			# maybe extended, but will never cross sector boundary...
			# short filename at beginning of sector..
			if(f.ptr.offset!=0) {
				for(prevdo = f.ptr.offset-32;prevdo >=0;prevdo-=32) {
					prevdattr := f.ptr.p.iobuf[prevdo+11];
					if(prevdattr != byte 16rf)
						break;
					islong = 1;
					longnamebuf += Dos->getnamesect(f.ptr.p.iobuf[prevdo:prevdo+32]);
				}
			}
		}
		else {
			# extended and will cross sector boundary.
			islong =1;
			for(prevdo = f.ptr.offset-32;prevdo >=0;prevdo-=32) {
				prevdattr := f.ptr.p.iobuf[prevdo+11];
				if(prevdattr != byte 16rf)
					break;
				longnamebuf += Dos->getnamesect(f.ptr.p.iobuf[prevdo:prevdo+32]);
			}
			if (prevdo < 0) {
				p := iotrack->getsect(f.xf,f.ptr.prevaddr);
				for(prevdo = f.xf.ptr.sectsize-32;prevdo >=0;prevdo-=32){
					prevdattr := p.iobuf[prevdo+11];
					if(prevdattr != byte 16rf)
						break;
					longnamebuf += Dos->getnamesect(p.iobuf[prevdo:prevdo+32]);
				}
				iotrack->putsect(p);
			}
		}

		if(islong)
			dir.name = longnamebuf;
		if(debug)
			Dos->chat(sprint("rstat [%s]",dir.name));

		g.rhdr.stat=styx->convD2M(dir);
		Dos->putfile(f);
	}
}


nameok(elem : string) : int
{
	isfrog : array of int;
	isfrog = array[256] of {
	# NUL
	1, 1, 1, 1, 1, 1, 1, 1,
	# BKS
	1, 1, 1, 1, 1, 1, 1, 1,
	# DLE
	1, 1, 1, 1, 1, 1, 1, 1,
	# CAN
	1, 1, 1, 1, 1, 1, 1, 1,
	' ' =>	1,
	'/' =>	1, 16r7f =>	1, * => 0
	};

	for(i:=0;i < len elem;i++) {
		if(i >= Styx->NAMELEN)
			return -1;
		if(isfrog[elem[i]])
			return -1;
	}
	return 0;
}

rwstat()
{
	if(debug)
		Dos->chat(sprint("rwstat(fid=%d)...", g.thdr.fid));

	f := iotrack->xfile(g.thdr.fid, Dos->Asis);
	if(f == nil) {
		g.errno = Dos->Eio;
		return;
	}

	if(Dos->getfile(f) < 0) {
		g.errno = Dos->Eio;
		return;
	}

	xd := styx->convM2D(g.thdr.stat);

	#
	# Needs to be implemented
	#
	
	Dos->putfile(f);
}

dossrv(rfd : ref Sys->FD) 
{
	if(!initialised) {
		sys->print("dossrv: not initialized\n");
		return;
	}

	n,r: int;

	data := array[8350] of byte;
	for(;;) {
		n = sys->read(rfd, data, len data);
		if(n <= 0)
			break;
		(r,g.thdr) = styx->ConvM2S(data[0:n]);
		if(r <= 0)
			Dos->panic("convM2S");

		g.errno = 0;
		if(g.chatty & Dos->STYX_MESS) {
			if(debug) {
				Dos->chat("\nreceived :");
				Dos->chat(g.thdr.print());
			}
		}

		case (g.thdr.Mtype) {
		Styx->Tnop => 
 			rnop(); 
		Styx->Tflush => 
 			rflush(); 
		Styx->Tattach => 
 			rattach(); 
		Styx->Tclone => 
 			rclone(); 
		Styx->Twalk => 
 			rwalk(); 
		Styx->Topen => 
 			ropen(); 
		Styx->Tcreate => 
 			rcreate(); 
		Styx->Tread => 
 			rread(); 
		Styx->Twrite => 
 			rwrite(); 
		Styx->Tclunk => 
 			rclunk(); 
		Styx->Tremove => 
 			rremove(); 
		Styx->Tstat => 
 			rstat(); 
		Styx->Twstat => 
 			rwstat(); 
		* =>	
			Dos->panic("type");	
		}
		if(g.errno == 0)
			g.rhdr.Mtype = g.thdr.Mtype+1;
		else {
			g.rhdr.Mtype = Styx->Rerror;
			g.rhdr.ename=Dos->xerrstr(g.errno);
		}

		g.rhdr.fid = g.thdr.fid;
		g.rhdr.Tag = g.thdr.Tag;
		if(g.errno==0) {
			if(debug)
				Dos->chat("OK\n");
		}
		else {
			if(debug)
				Dos->chat(sprint("%s (%d)\n",
					Dos->xerrstr(g.errno), g.errno));
		}
		rbuf:=g.rhdr.ConvS2M();
		if(len rbuf <= 0)
			Dos->panic("convS2M");

		if(g.chatty & Dos->STYX_MESS) {
			if (debug) Dos->chat("\nreplying: \n");
			if (debug) Dos->chat(g.rhdr.print());
		}

		if(sys->write(rfd, rbuf, len rbuf) != len rbuf)
			Dos->panic("write");
	}

	if(debug == 0)
		exit;

	if(n < 0)
		Dos->chat("server read error: %r\n");
	else
		Dos->chat("server EOF\n");
}

msgname := array [Styx->Tmax] of {
	Styx->Tnop	=> "Tnop",
	Styx->Rnop	=> "Rnop",
	Styx->Terror	=> "Terror",
	Styx->Rerror	=> "Rerror",
	Styx->Tflush	=> "Tflush",
	Styx->Rflush	=> "Rflush",
	Styx->Tclone	=> "Tclone",
	Styx->Rclone	=> "Rclone",
	Styx->Twalk	=> "Twalk",
	Styx->Rwalk	=> "Rwalk",
	Styx->Topen	=> "Topen",
	Styx->Ropen	=> "Ropen",
	Styx->Tcreate	=> "Tcreate",
	Styx->Rcreate	=> "Rcreate",
	Styx->Tread	=> "Tread",
	Styx->Rread	=> "Rread",
	Styx->Twrite	=> "Twrite",
	Styx->Rwrite	=> "Rwrite",
	Styx->Tclunk	=> "Tclunk",
	Styx->Rclunk	=> "Rclunk",
	Styx->Tremove	=> "Tremove",
	Styx->Rremove	=> "Rremove",
	Styx->Tstat	=> "Tstat",
	Styx->Rstat	=> "Rstat",
	Styx->Twstat	=> "Twstat",
	Styx->Rwstat	=> "Rwstat",
	Styx->Tattach	=> "Tattach",
	Styx->Rattach	=> "Rattach",
};

