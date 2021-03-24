implement IoTrack;

include "sys.m";
	sys: Sys;
	Qid, sprint: import sys;

include "iotrack.m";

include "styx.m";

include "dossubs.m";
	Dos : DosSubs;

# iotrack stuff ...

hiob := array[HIOB+1] of ref Iotrack;		# hash buckets + lru list 
iobuf := array[NIOBUF] of ref Iotrack;		# the real ones 
freelist: ref Iosect;

# xfile stuff here.....

FIDMOD: con 127;	# prime 
xhead:		ref Xfs;
xfreelist:	ref Xfile;
client:		int;
g:		ref Dos->Global;
debug:		con 1;

xfiles := array[FIDMOD] of ref Xfile;

Track.create() : ref Track
{
	retval := ref Track;
	retval.p = array[Sect2trk] of ref Iosect;
	retval.buf = array[Trksize] of byte;

	return retval;
}

getsect(xf: ref Xfs, addr: int): ref Iosect
{
	return getiosect(xf, addr, 1);
}

getosect(xf: ref Xfs, addr: int) : ref Iosect
{
	return getiosect(xf, addr, 0);
}

# get the sector corresponding to the address addr.
getiosect(xf: ref Xfs, addr , rflag: int): ref Iosect
{
	# offset from beginning of track.
	toff := addr %  Sect2trk;

	# adress of beginning of track.
	taddr := addr -  toff;
	t := getiotrack(xf, taddr);

	if(rflag && t.flags&BSTALE) {
		if(tread(t) < 0)
			return nil;

		t.flags &= ~BSTALE;
	}

	t.refn++;
	if(t.tp.p[toff] == nil) {
		p := newsect();
		t.tp.p[toff] = p;
		p.flags = t.flags&BSTALE;
		p.t = t;
		p.iobuf = t.tp.buf[toff*Sectorsize:(toff+1)*Sectorsize];
	}
	return t.tp.p[toff];
}

putsect(p: ref Iosect)
{
	t: ref Iotrack;

	t = p.t;
	t.flags |= p.flags;
	p.flags = 0;
	t.refn--;

	if(t.flags & BIMM) {
		if(t.flags & BMOD)
			twrite(t);
		t.flags &= ~(BMOD|BIMM);
	}
}

# get the track corresponding to addr
# (which is the address of the beginning of a track
getiotrack(xf: ref Xfs, addr: int): ref Iotrack
{
	p : ref Iotrack;
	mp := hiob[HIOB];
	
	if(g.chatty & Dos->CLUSTER_INFO)
 		if(debug)
			Dos->chat(sprint("iotrack %d,%d...", xf.dev.fd, addr));

	# find bucket in hash table.
	h := (xf.dev.fd<<24) ^ addr;
	if(h < 0)
		h = ~h;
	h %= HIOB;
	hp := hiob[h];

	out: do {
		loop: for(;;) {
		 	# look for it in the active list
			for(p = hp.hnext; p != hp; p=p.hnext) {
				if(p.addr != addr || p.xf != xf)
					continue;
				if(p.addr == addr && p.xf == xf) {
					break out;
				}
				continue loop;
			}
		
		 	# not found
		 	# take oldest unref'd entry
			for(p = mp.prev; p != mp; p=p.prev)
				if(p.refn == 0 )
					break;
			if(p == mp) {
				if(debug)
					Dos->chat("iotrack all ref'd\n");
				continue loop;
			}

			if((p.flags & BMOD)!= 0) {
				twrite(p);
				p.flags &= ~(BMOD|BIMM);
				continue loop;
			}
			purgetrack(p);
			p.addr = addr;
			p.xf = xf;
			p.flags = BSTALE;
			break out;
		}
	} while();

	if(hp.hnext != p) {
		p.hprev.hnext = p.hnext;
		p.hnext.hprev = p.hprev;			
		p.hnext = hp.hnext;
		p.hprev = hp;
		hp.hnext.hprev = p;
		hp.hnext = p;
	}
	if(mp.next != p) {
		p.prev.next = p.next;
		p.next.prev = p.prev;
		p.next = mp.next;
		p.prev = mp;
		mp.next.prev = p;
		mp.next = p;			
	}
	return p;
}

purgetrack(t : ref Iotrack)
{
	refn := Sect2trk;
	for(i := 0; i < Sect2trk; i++) {
		if(t.tp.p[i] == nil) {
			--refn;
			continue;
		}
		freesect(t.tp.p[i]);
		--refn;
		t.tp.p[i]=nil;
	}
	if(t.refn != refn)
		Dos->panic("purgetrack");
	if(refn!=0)
		Dos->panic("refn not 0");
}

twrite(t : ref Iotrack) : int
{
	if(debug)
		Dos->chat(sprint("[twrite %d...", t.addr));

	if((t.flags & BSTALE)!= 0) {
		refn:=0;
		for(i:=0; i<Sect2trk; i++)
			if(t.tp.p[i]!=nil)
				++refn;

		if(refn < Sect2trk) {
			if(tread(t) < 0) {
				if (debug) Dos->chat("error]");
				return -1;
			}
		}
		else
			t.flags &= ~BSTALE;
	}

	if(devwrite(t.xf, t.addr, t.tp.buf) < 0) {
		if(debug)
			Dos->chat("error]");
		return -1;
	}

	if(debug)
		Dos->chat(" done]");

	return 0;
}

tread(t : ref Iotrack) : int
{
	refn := 0;
	rval : int;

	for(i := 0; i < Sect2trk; i++)
		if(t.tp.p[i] != nil)
			++refn;

	if(debug)
		Dos->chat(sprint("[tread %d...", t.addr));

	tbuf := t.tp.buf;
	if(refn != 0)
		tbuf = array[Trksize] of byte;

	rval = devread(t.xf, t.addr, tbuf);
	if(rval < 0) {
		if(debug)
			Dos->chat("error]");
		return -1;
	}

	if(refn != 0) {
		for(i=0; i < Sect2trk; i++) {
			if(t.tp.p[i] == nil) {
				t.tp.buf[i*Sectorsize:]=tbuf[i*Sectorsize:(i+1)*Sectorsize];
				if(debug)
					Dos->chat(sprint("%d ", i));
			}
		}
	}

	if(debug)
		Dos->chat("done]");

	t.flags &= ~BSTALE;
	return 0;
}

purgebuf(xf : ref Xfs)
{
	for(p := 0; p < NIOBUF; p++) {
		if(iobuf[p].xf != xf)
			continue;
		if(iobuf[p].xf == xf) {
			if((iobuf[p].flags & BMOD) != 0)
				twrite(iobuf[p]);

			iobuf[p].flags = BSTALE;
			purgetrack(iobuf[p]);
		}
	}
}

sync()
{
	for(p := 0; p < NIOBUF; p++) {
		if(!(iobuf[p].flags & BMOD))
			continue;

		if(iobuf[p].flags & BMOD){
			twrite(iobuf[p]);
			iobuf[p].flags &= ~(BMOD|BIMM);
		}
	}
}

iotrack_init(og : ref Dos->Global)
{
	g = og;
	sys = load Sys Sys->PATH;
	Dos = g.Dos;

	freelist = nil;
	xfreelist = nil;

	for(i := 0;i < FIDMOD; i++)
		xfiles[i] = ref Xfile(nil,0,0,0,Sys->Qid(0,0),nil,nil);

	for(i = 0; i <= HIOB; i++)
		hiob[i] = ref Iotrack;

	for(i = 0; i < HIOB; i++) {
		hiob[i].hprev = hiob[i];
		hiob[i].hnext = hiob[i];
		hiob[i].refn = 0;
		hiob[i].addr = 0;
	}
	hiob[i].prev = hiob[i];
	hiob[i].next = hiob[i];
	hiob[i].refn = 0;
	hiob[i].addr = 0;

	for(i=0;i<NIOBUF;i++)
		iobuf[i] = ref Iotrack;

	for(i=0; i<NIOBUF; i++) {
		iobuf[i].hprev = iobuf[i].hnext = iobuf[i];
		iobuf[i].prev = iobuf[i].next = iobuf[i];
		iobuf[i].refn=iobuf[i].addr=0;
		iobuf[i].flags = 0;
		if(hiob[HIOB].next != iobuf[i]) {
			iobuf[i].prev.next = iobuf[i].next; 
			iobuf[i].next.prev = iobuf[i].prev; 
			iobuf[i].next = hiob[HIOB].next;
			iobuf[i].prev = hiob[HIOB];
			hiob[HIOB].next.prev = iobuf[i];
			hiob[HIOB].next = iobuf[i];
		}
		iobuf[i].tp =  Track.create();
	}
}


newsect(): ref Iosect
{

	p : ref Iosect;

	if((p = freelist)!=nil)	
		freelist = p.next;

	if (p==nil)
		p = ref Iosect(nil, 0, nil,nil);

	p.next = nil;
	return p;
}

freesect(p : ref Iosect)
{
	p.next = freelist;
	freelist = p;
}


# devio from here
deverror(name : string, xf : ref Xfs, addr,n,nret : int) : int
{
	if(nret < 0) {
		if(debug)
			Dos->chat(sprint("%s errstr=\"%r\"...", name));
		xf.dev = nil;
		return -1;
	}
	if(debug)
		Dos->chat(sprint("dev %d sector %d, %s: %d, should be %d\n",
			xf.dev.fd, addr, name, nret, n));

	Dos->panic(name);
	return -1;
}

devread(xf : ref Xfs, addr: int, buf: array of byte) : int
{
	if(xf.dev==nil)
		return -1;

	sys->seek(xf.dev, xf.offset+addr*Sectorsize, sys->SEEKSTART);
	nread := sys->read(xf.dev, buf, Trksize);
	if(nread != Trksize)
		return deverror("read", xf, addr, Trksize, nread);

	return 0;
}

devwrite(xf : ref Xfs, addr : int, buf : array of byte) : int
{
	if(xf.dev == nil)
		return -1;

	sys->seek(xf.dev, xf.offset+addr*Sectorsize, 0);
	nwrite := sys->write(xf.dev, buf, Trksize);
	if(nwrite != Trksize)
		return deverror("write", xf, addr, Trksize , nwrite);

	return 0;
}

devcheck(xf: ref Xfs) : int
{
	buf := array[Sectorsize] of byte;

	if(xf.dev == nil)
		return -1;

	sys->seek(xf.dev, 0, sys->SEEKSTART);
	if(sys->read(xf.dev, buf, Sectorsize) != Sectorsize){
		xf.dev = nil;
		return -1;
	}

	return 0;
}

# xfile stuff here.....


# setup and return the Xfs associated with "name"

getxfs(name : string) : ref Xfs
{
	xf,fxf : ref Xfs;

	if(name == nil)
		name = g.deffile;

	if(name == nil) {
		g.errno = Dos->Enofilsys;
		return nil;
	}

	
	 # If the name passed is of the form 'name:offset' then
	 # offset is used to prime xf->offset. This allows accessing
	 # a FAT-based filesystem anywhere within a partition.
	 # Typical use would be to mount a filesystem in the presence
	 # of a boot manager programm at the beginning of the disc.
	 
	offset := 0;
	for(i := 0;i < len name; i++) {
		if (name[i]==':') {
			i++;
			break;
		}
	}

	if(i < len name) {
		offset = int name[i:];
		if(offset < 0){
			g.errno = Dos->Enofilsys;
			return nil;
		}
		offset *= Sectorsize;
	}

	fd := sys->open(name, Sys->ORDWR);
	if(fd == nil) {
		if(debug)
			Dos->chat(sprint("getxfs: open(%s) failed: %r\n", name));
		g.errno = Dos->Enonexist;
		return nil;
	}

	(rval,dir):=sys->fstat(fd);
	if(rval < 0){
		g.errno = Dos->Eio;
		return nil;
	}
	
	# lock down the list of xf's.
	fxf=nil;
	for(xf=xhead; xf!=nil; xf=xf.next) {
		if(xf.refn == 0) {
			if(fxf == nil)
				fxf = xf;
			continue;
		}
		if(xf.qid.path != dir.qid.path || xf.qid.vers != dir.qid.vers)
			continue;

		if(xf.name!= name || xf.dev == nil)
			continue;

		if(devcheck(xf) < 0) # look for media change 
			continue;

		if(offset && xf.offset != offset)
			continue;

		if(debug)
			Dos->chat(sprint("incref \"%s\", dev=%d...",
				xf.name, xf.dev.fd));

		++xf.refn;
		return xf;
	}
	
	# this xf doesn't exist, make a new one and stick it on the list.
	if(fxf == nil){
		fxf = ref Xfs;
		fxf.next = xhead;
		xhead = fxf;
	}

	if(debug)
		Dos->chat(sprint("alloc \"%s\", dev=%d...", name, fd.fd));

	fxf.name = name;
	fxf.refn = 1;
	fxf.qid = dir.qid;
	fxf.dev = fd;
	fxf.fmt = 0;
	fxf.offset = offset;
	return fxf;
}

refxfs(xf : ref Xfs, delta : int)
{
	xf.refn += delta;
	if(xf.refn == 0) {
		if (debug)
			Dos->chat(sprint("free \"%s\", dev=%d...",
				xf.name, xf.dev.fd));

		purgebuf(xf);
		if(xf.dev !=nil)
			xf.dev = nil;
	}
}

xfile(fid, flag : int) : ref Xfile
{
	pf : ref Xfile;

	# find hashed file list in LRU? table.
	k := (fid^client)%FIDMOD;

	# find if this fid is in the hashed file list.
	f:=xfiles[k];
	for(pf = nil; f != nil; f = f.next) {
		if(f.fid == fid && f.client == client)
			break;
		pf=f;
	}
	
	# move this fid to the front of the list if it was further down.
	if(f != nil && pf != nil){
		pf.next = f.next;
		f.next = xfiles[k];
		xfiles[k] = f;
	}


	case flag {
	* =>
		Dos->panic("xfile");
	Dos->Asis =>
		if (f != nil && f.xf != nil && f.xf.dev == nil)
			return nil;
		return f;
	Dos->Clean =>
		break;
	Dos->Clunk =>
		# if we found f in the hashtable, stick it on the freelist
		if(f!=nil) {
			xfiles[k] = f.next;
			clean(f);
			f.next = xfreelist;
			xfreelist = f;
		} 
		return nil;
	}

	# clean it up ..
	if(f != nil)
		return clean(f);

	# f wasn't found in the hashtable, make a new one.
	# if we can, take it from the list of "free" Xfile nodes.
	if((f = xfreelist)!=nil)	
		xfreelist = f.next;
	else 
		f = ref Xfile;

	# put it in the Xfile hashtable.
	f.next = xfiles[k];
	xfiles[k] = f;
	# sort out the fid, etc.
	f.fid = fid;
	f.client = client;
	f.flags = 0;
	f.qid = Qid(0,0);
	f.xf = nil;
	f.ptr = ref Dos->Dosptr(0,0,0,0,0,0,-1,-1,nil);
	return f;
}

clean(f : ref Xfile) : ref Xfile
{
	if(f.ptr != nil)
		f.ptr = nil;

	if(f.xf!=nil) {
		refxfs(f.xf, -1);
		f.xf = nil;
	}
	f.xf = nil;
	f.flags = 0;
	f.qid = Qid(0,0);
	return f;
}

xfspurge() : int
{
	count:=0;
	f,pf,nf : ref Xfile;

	for(k := 0; k < FIDMOD; k++) {
		hp := xfiles[k];
		f = hp;
		for(pf = nil; f != nil; f = nf) {
			nf = f.next;
			if(f.client != client) {
				pf = f;
				continue;
			}
			if (pf!=nil)
				pf.next = f.next;
			else
				hp = f.next;
			clean(f);
			f.next = xfreelist;
			xfreelist = f;
			++count;
		}
	}
	return count;
}
