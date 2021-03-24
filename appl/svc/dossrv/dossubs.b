implement DosSubs;

include "sys.m";
	sys: Sys;
	sprint: import sys;

include "dossubs.m";
	Dos: DosSubs;

include "iotrack.m";
	iotrack : IoTrack;
	Xfs, Xfile: import iotrack;

include "styx.m";
	styx: Styx;

include "dosfs.m";

debug: con 1;

errmsg := array[] of {
	Enevermind	=> "never mind",
	Eformat		=> "unknown format",
	Eio		=> "I/O error",
	Enomem		=> "server out of memory",
	Enonexist	=> "file does not exist",
	Eexist		=> "file exists",
	Eperm		=> "permission denied",
	Enofilsys	=> "no file system device specified",
	Eauth		=> "authentication failed",
};

fd: ref Sys->FD;
g: ref Global;
nowt, nowt1: int;

init(thisg: ref Global)
{
	g = thisg;
	iotrack = g.iotrack;
	Dos = g.Dos;

	sys = load Sys Sys->PATH;
	styx = load Styx Styx->PATH; 
	if(styx == nil)
		styx = load Styx "#/./styx";
}

setup()
{
	if(g.logfile != "")
		fd = sys->create(g.logfile, Sys->OWRITE, 8r644);
	if(fd == nil)
		fd = sys->fildes(1);

	nowfd := sys->open("/dev/time", sys->OREAD);
	if(nowfd != nil){
		buf := array[128] of byte;
		sys->seek(nowfd,0,Sys->SEEKSTART);
		n := sys->read(nowfd, buf, len buf);
		if(n >= 0)
			nowt = int ((big string buf[0:n]) / big 1000000);
	}

	nowt1 = sys->millisec();
	readtimezone();

}

# make xf into a Dos file system... or die trying to.
dosfs(xf : ref Xfs) : int
{
	mbroffset := 0;
	i: int;
	p: ref IoTrack->Iosect;

dmddo:	for(;;) {
		for(i=2; i>0; i--) {
			p = iotrack->getsect(xf, 0);
			if(p == nil)
				return -1;

			if((mbroffset == 0) && (p.iobuf[0] == byte 16re9))
				break;
			
			# Check if the jump displacement (magic[1]) is too 
			# short for a FAT. DOS 4.0 MBR has a displacement of 8.
			if(p.iobuf[0] == byte 16reb &&
			   p.iobuf[2] == byte 16r90 &&
			   p.iobuf[1] != byte 16r08)
				break;

			if(i < 2 ||
			   p.iobuf[16r1fe] != byte 16r55 ||
			   p.iobuf[16r1ff] != byte 16raa) {
				i = 0;
				break;
			}

			dp := 16r1be;
			for(j:=4; j>0; j--) {
				if(debug) {
					chat(sprint("16r%2.2ux (%d,%d) 16r%2.2ux (%d,%d) %d %d...",
					int p.iobuf[dp], int p.iobuf[dp+1], 
					bytes2short(p.iobuf[dp+2: dp+4]),
					int p.iobuf[dp+4], int p.iobuf[dp+5], 
					bytes2short(p.iobuf[dp+6: dp+8]),
					bytes2int(p.iobuf[dp+8: dp+12]), 
					bytes2int(p.iobuf[dp+12:dp+16])));
				}

				# Check for a disc-manager partition in the MBR.
				# Real MBR is at lba 63. Unfortunately it starts
				# with 16rE9, hence the check above against magic.
				if(p.iobuf[dp+4] == DMDDO) {
					mbroffset = 63*IoTrack->Sectorsize;
					iotrack->putsect(p);
					iotrack->purgebuf(xf);
					xf.offset += mbroffset;
					break dmddo;
				}
				
				# Make sure it really is the right type, other
				# filesystems can look like a FAT
				# (e.g. OS/2 BOOT MANAGER).
				if(p.iobuf[dp+4] == FAT12 ||
				   p.iobuf[dp+4] == FAT16 ||
				   p.iobuf[dp+4] == FATHUGE)
					break;
				dp+=16;
			}

			if(j <= 0) {
				if(debug)
					chat("no active partition...");
				iotrack->putsect(p);
				return -1;
			}

			offset := bytes2int(p.iobuf[dp+8:dp+12])* IoTrack->Sectorsize;
			iotrack->putsect(p);
			iotrack->purgebuf(xf);
			xf.offset = mbroffset+offset;
		}
		break;
	}
	if(i <= 0) {
		if(debug)
			chat("bad magic...");
		iotrack->putsect(p);
		return -1;
	}

	b := Dosboot.arr2Db(p.iobuf);
	if(g.chatty)
		bootdump(b);

	bp := ref Dosbpb;
	xf.ptr = bp;
	xf.fmt = 1;

	bp.sectsize = bytes2short(b.sectsize);
	bp.clustsize = int b.clustsize;
	bp.nresrv = bytes2short(b.nresrv);
	bp.nfats = int b.nfats;
	bp.rootsize = bytes2short(b.rootsize);
	bp.volsize = bytes2short(b.volsize);
	if(bp.volsize == 0)
		bp.volsize = bytes2int(b.bigvolsize);
	bp.mediadesc = int b.mediadesc;
	bp.fatsize = bytes2short(b.fatsize);

	bp.fataddr = int bp.nresrv;
	bp.rootaddr = bp.fataddr + bp.nfats*bp.fatsize;
	i = bp.rootsize*32 + bp.sectsize-1;
	i /= bp.sectsize;
	bp.dataaddr = bp.rootaddr + i;
	bp.fatclusters = 2+(bp.volsize - bp.dataaddr)/bp.clustsize;
	if(bp.fatclusters < 4087)
		bp.fatbits = 12;
	else
		bp.fatbits = 16;
	bp.freeptr = 2;
	if(debug)
		chat(sprint("fatbits=%d (%d clusters)...",
			bp.fatbits, bp.fatclusters));

	for(i=0; i< int b.nfats; i++)
		if(debug)
			chat(sprint("fat %d: %d...",
				i, bp.fataddr+i*bp.fatsize));

	if(debug) {
		chat(sprint("root: %d...", bp.rootaddr));
		chat(sprint("data: %d...", bp.dataaddr));
	}
	iotrack->putsect(p);
	return 0;
}

getfile(f: ref Xfile): int
{
	dp := f.ptr;

	if(dp.p!=nil)
		panic("getfile");

	p := iotrack->getsect(f.xf, dp.addr);
	if(p == nil)
		return -1;

	if(dp.addr != 0) {
		if((f.qid.path & ~Sys->CHDIR) != dp.addr*(IoTrack->Sectorsize/32) + dp.offset/32){
			if(debug) {
				chat(sprint("qid mismatch f=0x%x d=0x%x...",
				f.qid.path,
				dp.addr*(IoTrack->Sectorsize/32) + dp.offset/32));
			}

			iotrack->putsect(p);
			g.errno = Enonexist;
			return -1;
		}
	}
	dp.p = p;
	return 0;
}

putfile(f : ref Xfile)
{
	dp := f.ptr;

	if(dp.p==nil)
		panic("putfile");

	iotrack->putsect(dp.p);
	dp.p = nil;
}

fileaddr(f: ref Xfile, isect, cflag: int) : int
{
	bp := f.xf.ptr;
	dp := f.ptr;

	d := Dosdir.arr2Dd(dp.p.iobuf[dp.offset:dp.offset+32]);
	clust, nskip : int;
	next := 0;

	if(dp.addr == 0) {
		if(isect*bp.sectsize >= bp.rootsize*32)
			return -1;
		return bp.rootaddr + isect;
	}

	if(bytes2short(d.start) == 0) {
		if(!cflag)
			return -1;

		start := falloc(f.xf);
		if(start <= 0)
			return -1;
		puttime(d);
		d.start[0] = byte start;
		d.start[1] = byte (start>>8);
		dp.p.iobuf[dp.offset:] = Dosdir.Dd2arr(d);
		dp.p.flags |= IoTrack->BMOD;
		dp.clust = 0;
	}

	iclust := isect/bp.clustsize;
	if(dp.clust == 0 || iclust < dp.iclust) {
		clust = bytes2short(d.start);
		nskip = iclust;
	}
	else {
		clust = dp.clust;
		nskip = iclust - dp.iclust;
	}

	if(g.chatty > 1 && nskip > 0 && debug)
		chat(sprint("clust 0x%x, skip %d...", clust, nskip));

	if(clust <= 0)
		return -1;

	if(nskip > 0) {
		while(--nskip >= 0) {
			next = getfat(f.xf, clust);
			if(g.chatty > 1 && debug)
				chat(sprint(".0x%x", next));
			if(next > 0) {
				clust = next;
				continue;
			}
			else
			if(!cflag)
				break;

			next = falloc(f.xf);
			if(next < 0)
				break;

			putfat(f.xf, clust, next);
			clust = next;
		}
		if(next <= 0)
			return -1;

		dp.clust = clust;
		dp.iclust = iclust;
	}
	if(g.chatty > 1 && debug)
		chat(sprint(" clust(0x%x)=0x%x...", iclust, clust));

	return bp.dataaddr + (clust-2)*bp.clustsize + isect%bp.clustsize;
}

searchdir(f: ref Xfile, name: string, cflag: int, lflag: int): (int,ref Dosptr)
{
	xf := f.xf;
	bp := xf.ptr;
	addr1 := 0;
	o1 := 0;
	dp :=  ref Dosptr(0,0,0,0,0,0,-1,-1,nil);
	dp.paddr = f.ptr.addr;
	dp.poffset = f.ptr.offset;
	naddr := -1;
	addr : int;
	nsect :=0;
	islong :=0;
	deleted :=0;
	buf:="";
	
	length:=name2de(name);
	if(!lflag) {
		name = name[0:8]+"."+name[8:11];
		i := len name -1;
		while(name[i]==' ')
			i--;
		name = name[0:i+1];
	}

	for(isect:=0;; isect++) {
		if(islong)
			dp.prevaddr = addr;
		addr = fileaddr(f, isect, cflag);

		if(addr < 0)
			break;

		p := iotrack->getsect(xf, addr);
		if(p == nil)
			break;

		for(o:=0; o<bp.sectsize; o+=32) {
			dattr:=p.iobuf[o+11];
			dname0:=p.iobuf[o];
			if(dname0 == byte 16r00) {
				if(debug)
					chat("end dir(0)...");

				iotrack->putsect(p);
				if(!cflag)
					return (-1,nil);

				nleft := (bp.sectsize-o)/32;
				if (!deleted) nsect=0;
				if (nleft+nsect < length+1)
					naddr = fileaddr(f,isect+1,cflag);

				if(addr1 == 0 || !deleted) {
					addr1 = addr;
					o1 = o;
				}

				if(addr1!=addr)
					dp.naddr = addr;
				else 
					dp.naddr = naddr;

				dp.addr = addr1;
				dp.offset = o1;
				return (0,dp);
			}

			if(dname0 == byte 16re5) {
				if(!deleted)
					nsect = 0;
				deleted = 1;
				nsect++;
				if(addr1 == 0) {
					addr1 = addr;
					o1 = o;
				}
				continue;
			}
			deleted = 0;
			if(int (dattr&DVLABEL) && 
			  (((int dattr & 16rf)!=16rf) || !lflag))
				continue;

			if((int dattr & 16rf) == 16rf) {
				if(!islong)
					islong=1;
				buf = getnamesect(p.iobuf[o:o+32]) + buf;
				continue;
			} 

			if(!islong) 
				buf = getname(p.iobuf[o:o+32]);
			else
				islong =0;

			if(lflag && debug)
				dirdump(p.iobuf[o:o+32],addr,o);

			if(mystrcmp(buf,name) != 0) {
				buf="";
				continue;
			}

			if(cflag) {
				iotrack->putsect(p);
				return (-1,nil);
			}

			dp.addr = addr;
			dp.naddr = naddr;
			dp.offset = o;
			dp.p = p;
			if(!lflag) 
				iotrack->putsect(p);

			return (0,dp);
		}
		iotrack->putsect(p);
	}

	if(debug)
		chat("end dir(1)...");

	return (-1,nil);
}

emptydir(f: ref Xfile): int
{
	for(isect:=0;; isect++) {
		addr := fileaddr(f, isect, 0);
		if(addr < 0)
			break;

		p := iotrack->getsect(f.xf, addr);
		if(p == nil)
			return -1;

		for(o:=0; o<f.xf.ptr.sectsize; o+=32) {
			dname0 := p.iobuf[o];
			dattr := p.iobuf[o+11];

			if(dname0 == byte 16r00) {
				iotrack->putsect(p);
				return 0;
			}

			if(dname0 == byte 16re5 || dname0 == byte '.')
				continue;

			if(int(dattr&DVLABEL) && (dattr != byte 16rf))
				continue;

			iotrack->putsect(p);
			return -1;
		}
		iotrack->putsect(p);
	}
	return 0;
}

dosreaddir(f:ref Xfile, offset, count: int) : (int,array of byte)
{
	xf := f.xf;
	bp := xf.ptr;
	rcnt := 0;
	buf := array[8192+132] of byte;
	islong :=0;
	longnamebuf:="";
	dir: ref Sys->Dir;

	if(count <= 0)
		return (0,nil);

	for(isect:=0;; isect++) {
		addr := fileaddr(f, isect, 0);
		if(addr < 0)
			break;
		p := iotrack->getsect(xf, addr);
		if(p == nil)
			return (-1,nil);

		for(o:=0; o<bp.sectsize; o+=32) {
			dname0 := int p.iobuf[o];
			dattr := p.iobuf[o+11];

			if(dname0 == 16r00) {
				iotrack->putsect(p);
				return (rcnt,buf[0:rcnt]);
			}

			if(dname0 == 16re5)
				continue;

			if(dname0 == '.') {
				dname1 := int p.iobuf[o+1];
				if(dname1 == ' ' || dname1 == 0)
					continue;
				dname2 := int p.iobuf[o+2];
				if(dname1 == '.' &&
				  (dname2 == ' ' || dname2 == 0))
					continue;
			}

			if(int (dattr&DVLABEL) && !((dattr & byte 16rf)==byte 16rf))
				continue;

			if(offset > 0) {
				if(!int ((dattr & byte 16rf) == byte 16rf))
					offset -= Styx->DIRLEN;
				continue;
			}

			if((int dattr & 16rf) == 16rf) {
				# longdir
				if(addr == 0 && debug)
					chat("argh!!\n");
				longnamebuf = getnamesect(p.iobuf[o:o+32]) + longnamebuf;
				if(!islong) 
					islong=1;

				continue;
			}
			else {
				dir = getdir(p.iobuf[o:o+32], addr, o);
				if(islong) {
					dir.name = longnamebuf;
					longnamebuf = "";
					islong = 0;
				}
			}
			tmpbuf := styx->convD2M(dir);
			buf[rcnt:] = tmpbuf;
			rcnt += len tmpbuf;
			if(rcnt >= count) {
				iotrack->putsect(p);
				return (rcnt,buf[0:rcnt]);
			}
		}
		iotrack->putsect(p);
	}

	return (rcnt,buf[0:rcnt]);
}

walkup(f: ref Xfile) : (int, ref Dosptr)
{
	bp := f.xf.ptr;
	dp := f.ptr;
	o : int;
	ndp:= ref Dosptr(0,0,0,0,0,0,-1,-1,nil);
	ndp.addr = dp.paddr;
	ndp.offset = dp.poffset;

	if(debug)
		chat(sprint("walkup: paddr=0x%x...", dp.paddr));

	if(dp.paddr == 0)
		return (0,ndp);

	p := iotrack->getsect(f.xf, dp.paddr);
	if(p == nil)  
		return (-1,nil);

	if(debug)
		dirdump(p.iobuf[dp.poffset:dp.poffset+32],dp.paddr,dp.poffset);

	#xd := Dosdir.arr2Dd(p.iobuf[dp.poffset:dp.poffset+32]);
	#xd.start..
	start := bytes2short(p.iobuf[dp.poffset+26:dp.poffset+28]);
	if(g.chatty & CLUSTER_INFO)
		if(debug)
			chat(sprint("start=0x%x...", start));

	if(start == 0) { 
		if (p!=nil) 
			iotrack->putsect(p);
		return (-1,nil);
	}

	iotrack->putsect(p);
	p = iotrack->getsect(f.xf, bp.dataaddr + (start-2)*bp.clustsize);
	if(p == nil)
		return (-1,nil);

	if(debug)
		dirdump(p.iobuf,0,0);

	#xd = Dosdir.arr2Dd(p.iobuf);
	#xd.start
	if(p.iobuf[0]!= byte '.' ||
	   p.iobuf[1]!= byte ' ' ||
	   start != bytes2short(p.iobuf[26:28])) { 
 		if(p!=nil) 
			iotrack->putsect(p);
		return (-1,nil);
	}

	if(debug)
		dirdump(p.iobuf[32:],0,0);

	#xd = Dosdir.arr2Dd(p.iobuf[32:]);
	#xd.name
	if(p.iobuf[32] != byte '.' || p.iobuf[33] != byte '.') { 
 		if(p != nil) 
			iotrack->putsect(p);
		return (-1,nil);
	}

	# xd.start
	pstart := bytes2short(p.iobuf[32+26:32+28]);
	iotrack->putsect(p);
	if(pstart == 0)
		return (0,ndp);

	p = iotrack->getsect(f.xf, bp.dataaddr + (pstart-2)*bp.clustsize);
	if(p == nil) {
		if(debug)
			chat(sprint("getsect %d failed\n", pstart));
		return (-1,nil);
	}

	if(debug)
		dirdump(p.iobuf,0,0);

	#xd = Dosdir.arr2Dd(p.iobuf);
	#xd.start
	if(p.iobuf[0]!= byte '.' ||
	   p.iobuf[1]!=byte ' ' || 
	   pstart!=bytes2short(p.iobuf[26:28])) { 
 		if(p != nil) 
			iotrack->putsect(p);
		return (-1,nil);
	}

	if(debug)
		dirdump(p.iobuf[32:],0,0);

	#xd = Dosdir.arr2Dd(p.iobuf[32:]);
	if(p.iobuf[32] != byte '.' || p.iobuf[33] != byte '.') { 
 		if(p != nil) 
			iotrack->putsect(p);
		return (-1,nil);
	}

	ppstart := bytes2short(p.iobuf[32+26:32+28]);
	iotrack->putsect(p);
	if(ppstart!=0)
		k := bp.dataaddr + (ppstart-2)*bp.clustsize;
	else
		k = bp.rootaddr;

	p = iotrack->getsect(f.xf, k);
	if(p == nil) {
		if(debug)
			chat(sprint("getsect %d failed\n", k));
		return (-1,nil);
	}

	if(debug)
		dirdump(p.iobuf,0,0);

	#xd = Dosdir.arr2Dd(p.iobuf);
	if(ppstart) {
		if(p.iobuf[0]!= byte '.' ||
		   p.iobuf[1]!= byte ' ' || 
		   ppstart!=bytes2short(p.iobuf[26:28])) { 
 			if(p!=nil) 
				iotrack->putsect(p);
			return (-1,nil);
		}
	}

	for(so:=1; ;so++) {
		for(o=0; o<bp.sectsize; o+=32) {
			#xd = Dosdir.arr2Dd(p.iobuf[o:o+32]);
			xdname0 := p.iobuf[o];
			if(xdname0 == byte 16r00) {
				if(debug)
					chat("end dir\n");
 				if(p != nil) 
					iotrack->putsect(p);
				return (-1,nil);
			}

			if(xdname0 == byte 16re5)
				continue;

			xdstart:= p.iobuf[o+26:o+28];
			if(bytes2short(xdstart) == pstart) {
				iotrack->putsect(p);
				ndp.paddr = k;
				ndp.poffset = o;
				return (0,ndp);
			}
		}
		if(ppstart) {
			if(so%bp.clustsize == 0) {
				ppstart = getfat(f.xf, ppstart);
				if(ppstart < 0){
					if(debug)
						chat(sprint("getfat %d fail\n", 
							ppstart));
 					if(p != nil) 
						iotrack->putsect(p);
					return (-1,nil);
				}
			}
			k = bp.dataaddr + (ppstart-2)*bp.clustsize + 
				so%bp.clustsize;
		}
		else {
			if(so*bp.sectsize >= bp.rootsize*32) { 
 				if(p != nil) 
					iotrack->putsect(p);
				return (-1,nil);
			}
			k = bp.rootaddr + so;
		}
		iotrack->putsect(p);
		p = iotrack->getsect(f.xf, k);
		if(p == nil) {
			if(debug)
				chat(sprint("getsect %d failed\n", k));
			return (-1,nil);
		}
	}
	iotrack->putsect(p);
	ndp.paddr = k;
	ndp.poffset = o;
	return (0,ndp);
}

readfile(f : ref Xfile, offset,count : int) : (int, array of byte)
{
	xf := f.xf;
	bp := xf.ptr;
	dp := f.ptr;

	length := bytes2int(dp.p.iobuf[dp.offset+28:dp.offset+32]);
	rcnt := 0;
 	buf := array[8192] of byte;
	if(offset >= length)
		return (0,nil);

	if(offset+count >= length)
		count = length - offset;

	isect := offset/bp.sectsize;
	o := offset%bp.sectsize;
	while(count > 0) {
		addr := fileaddr(f, isect++, 0);
		if(addr < 0)
			break;

		c := bp.sectsize - o;
		if(c > count)
			c = count;

		p := iotrack->getsect(xf, addr);
		if(p == nil)
			return (-1,nil);

		buf[rcnt:]=p.iobuf[o:o+c];
		iotrack->putsect(p);
		count -= c;
		rcnt += c;
		o = 0;
	}
	return (rcnt,buf[:rcnt]);
}

writefile(f: ref Xfile, buf: array of byte, offset,count: int): int
{
	xf := f.xf;
	bp := xf.ptr;
	dp := f.ptr;
	addr := 0;
	c : int;
	rcnt := 0;
	p : ref iotrack->Iosect;

	d := Dosdir.arr2Dd(dp.p.iobuf[dp.offset:dp.offset+32]);
	isect := offset/bp.sectsize;

	o := offset%bp.sectsize;
	while(count > 0) {
		addr = fileaddr(f, isect++, 1);
		if(addr < 0)
			break;

		c = bp.sectsize - o;
		if(c > count)
			c = count;

		if(c == bp.sectsize){
			p = iotrack->getosect(xf, addr);
			if(p == nil)
				return -1;
			p.flags = 0;
		}
		else {
			p = iotrack->getsect(xf, addr);
			if(p == nil)
				return -1;
		}
		p.iobuf[o:] = buf[rcnt:rcnt+c];
		p.flags |= IoTrack->BMOD;
		iotrack->putsect(p);
		count -= c;
		rcnt += c;
		o = 0;
	}

	if(rcnt <= 0 && addr < 0)
		return -1;

	length := 0;
	dlen := bytes2int(d.length);
	if(rcnt > 0)
		length = offset+rcnt;
	else
	if(dp.addr && dp.clust) {
		c = bp.clustsize*bp.sectsize;
		if(dp.iclust > (dlen+c-1)/c)
			length = c*dp.iclust;
	}

	if(length > dlen) {
		d.length[0] = byte length;
		d.length[1] = byte (length>>8);
		d.length[2] = byte (length>>16);
		d.length[3] = byte (length>>24);
	}

	puttime(d);
	dp.p.flags |= IoTrack->BMOD;
	dp.p.iobuf[dp.offset:] = Dosdir.Dd2arr(d);
	return rcnt;
}

truncfile(f : ref Xfile) : int
{
	xf := f.xf;
	bp := xf.ptr;
	dp := f.ptr;
	d := Dosdir.arr2Dd(dp.p.iobuf[dp.offset:dp.offset+32]);

	clust := bytes2short(d.start);
	d.start[0] = byte 0;
	d.start[1] = byte 0;
	while(clust > 0) {
		next := getfat(xf, clust);
		putfat(xf, clust, 0);
		clust = next;
	}

	d.length[0] = byte 0;
	d.length[1] = byte 0;
	d.length[2] = byte 0;
	d.length[3] = byte 0;

	dp.p.iobuf[dp.offset:] = Dosdir.Dd2arr(d);
	dp.iclust = 0;
	dp.clust = 0;
	dp.p.flags |= IoTrack->BMOD;

	return 0;
}

getdir(arr : array of byte, addr,offset: int) :ref Sys->Dir 
{
	dp := ref Sys->Dir;

	if(arr == nil || addr == 0) {
		dp.name = "";
		dp.qid.path = Sys->CHDIR;
		dp.length =0;
		dp.mode = Sys->CHDIR|8r777;
	}
	else {
		dp.name=getname(arr);
		for(p_i:=0;p_i< len dp.name; p_i++)
			if(dp.name[p_i]>='A' && dp.name[p_i]<='Z')
				dp.name[p_i] = dp.name[p_i]-'A'+'a';

		# dp.qid.path = bytes2short(d.start); 
		dp.qid.path = addr*(IoTrack->Sectorsize/32) + offset/32;
		dattr:=arr[11];

		if(int (dattr & DRONLY))
			dp.mode = 8r444;
		else
			dp.mode = 8r666;

		dp.atime = gtime(arr);
		dp.mtime = dp.atime;
		if(int (dattr & DDIR)) {
			dp.length = 0;
			dp.qid.path |= Sys->CHDIR;
			dp.mode |= Sys->CHDIR|8r111;
		}
		else 
			dp.length = bytes2int(arr[28:32]);

		if(int (dattr & DSYSTEM))
			dp.mode |= Styx->CHEXCL;
	}

	dp.qid.vers = 0;
	dp.dtype = 0;
	dp.dev = 0;
	dp.uid = "dos";
	dp.gid = "srv";

	return dp;
}

getname(arr : array of byte) : string
{
	p: string;
	for(i:=0; i<8; i++) {
		c := int arr[i];
		if(c == 0 || c == ' ')
			break;
		if(i == 0 && c == 16r05)
			c = 16re5;
		p[len p] = c;
	}
	for(i=8; i<11; i++) {
		c := int arr[i];
		if(c == 0 || c == ' ')
			break;
		if(i == 8)
			p[len p] = '.';
		p[len p] = c;
	}

	return p;
}

putname(p: string, d: ref Dosdir)
{
	if ((d.attr & byte 16rf) == byte 16rf)
		panic("calling long name!\n");

	d.name = "        ";
	for(i := 0; i < len p && i < 8; i++) {
		c := p[i];
		if(c >= 'a' && c <= 'z')
			c = c - 'a'+'A';
		else
		if(c == '.')
			break;
		d.name[i] = c;
	}
	d.ext = "   ";
	for(j := len p - 1; j >= i; j--) {
		if(p[j] == '.') {
			q := 0;
			for(j++; j < len p && q < 3; j++) {
				c := p[j];
				if(c >= 'a' && c <= 'z')
					c = c - 'a'+'A';
				d.ext[q++] = c;
			}
			break;
		}
	}
}

mystrcmp(s1, s2 : string) : int
{
	n := len s1;
	if(n != len s2)
		return 1;

	for(i := 0; i < n; i++) {
		c := s1[i];
		if(c >= 'A' && c <= 'Z')
			c = c - ('A'-'a');
		d := s2[i];
		if(d >= 'A' && d <= 'Z')
			d = d - ('A'-'a');
		if(c != d)
			return 1;
	}
	return 0;
}

#
# return the length of a long name in directory
# entries or zero if its normal dos
#
name2de(p : string) : int
{
	ext := 0;
	name := 0;

	for(end := len p - 1; end >= 0 && p[end] != '.'; end--)
		ext++;

	if(end > 0) {
		name = end;
		for(i := 0; i < end; i++) {
			if(p[i] == '.')
				return 1+(name+ext)/13;
		}
	}
	else {
		name = ext;
		ext = 0;
	}

	if(name <= 8 && ext <= 3)
		return 0;

	return 1+(name+ext)/13;
}

getnamesect(arr : array of byte) : string
{
	retval : string;
	c: int;

	for(i := 1;i < 11;i += 2) {
		c = int arr[i];
		if(c == 0)
			return retval;

		retval[len retval] = c;
	}
	for(i = 14;i < 26;i += 2) {
		c = int arr[i];
		if(c == 0)
			return retval;

		retval[len retval] = c;
	}
	for(i = 28;i < 32;i += 2) {
		c = int arr[i];
		if(c == 0)
			return retval;

		retval[len retval] = c;
	}
	return retval;
}

Dosslot.arr2Ds(arr: array of byte) : ref Dosslot
{
	retval := ref Dosslot;
	retval.id = arr[0];
	for(i := 1;i < 11;i += 2)
		retval.name0_4[len retval.name0_4]= int arr[i];

	retval.attr = arr[11];
	retval.reserved = arr[12];
	retval.alias_checksum = arr[13];

	for(i = 14;i < 26;i += 2)
		retval.name5_10[len retval.name5_10]= int arr[i];

	retval.start = arr[26:28];
	for(i = 28;i < 32;i += 2)
		retval.name11_12[len retval.name11_12]= int arr[i];

	return retval;
}


# takes a long filename and converts to a short dos name, with a tag number.
long2short(src : string,val : int) : string
{
	dst :="           ";
	skip:=0;
	xskip:=0;
	ext:=len src-1;
	while(ext>=0 && src[ext]!='.')
		ext--;

	if (ext < 0)
		ext=len src -1;

	# convert name eliding periods 
	j:=0;
	for(name := 0; name < ext && j<8; name++)
		if(src[name]!='.' && src[name]!=' ' && src[name]!='\t') {
			if(src[name]>='a' && src[name]<='z')
				dst[j++]=src[name]-'a'+'A';
			else
				dst[j++]=src[name];
		}	
		else
			skip++;

	# convert extension 
	j=8;
	for(xname := ext+1; xname < len src && j<11; xname++) {
		if(src[xname]!=' ' && src[xname]!='\t')
			if (src[xname]>='a' && src[xname]<='z')
				dst[j++]=src[xname]-'a'+'A';
			else
				dst[j++]=src[xname];
		else
			xskip++;
	}
	
	# add tag number
	j=1; 
	for(i:=val;i;i/=10)
		j++;

	if (8-j<name) 
		name=8-j;
	else
		name-=skip;

	dst[name]='~';
	while(val) {
		dst[name+ --j]= (val%10)+'0';
		val/=10;
	}

	if(debug)
		chat(sprint("returning dst [%s] src [%s]\n",dst,src));

	return dst;			
}

getfat(xf : ref Xfs, n : int) : int
{
	bp := xf.ptr;
	k := 0; 

	if(n < 2 || n >= bp.fatclusters)
		return -1;

	case bp.fatbits {
	12 =>
		k = (3*n)/2; 
	16 =>
		k = 2*n; 
	}

	if(k >= bp.fatsize*bp.sectsize)
		panic("getfat");

	sect := k/bp.sectsize + bp.fataddr;
	o := k%bp.sectsize;
	p := iotrack->getsect(xf, sect);
	if(p == nil)
		return -1;

	k = int p.iobuf[o++];
	if(o >= bp.sectsize) {
		iotrack->putsect(p);
		p = iotrack->getsect(xf, sect+1);
		if(p == nil)
			return -1;
		o = 0;
	}

	k |= (int p.iobuf[o])<<8;
	iotrack->putsect(p);
	if(bp.fatbits == 12) {
		if(n&1)
			k >>= 4;
		else
			k &= 16rfff;
		if(k >= 16rff8)
			k |= 16rf000;
	}

	if(g.chatty & FAT_INFO)
		if(debug)
			chat(sprint("fat(0x%x)=0x%x...", n, k));

	if(k < 16rfff8)
		return k;
	else 
		return -1;
}

putfat(xf: ref Xfs, n, val: int)
{
	bp := xf.ptr;
	k := 0;

	if(n < 2 || n >= bp.fatclusters)
		panic(sprint("putfat n=%d", n));

	case bp.fatbits {
	12 =>
		k = (3*n)/2;
	16 =>
		k = 2*n; 
	}

	if(k >= bp.fatsize*bp.sectsize)
		panic("putfat");

	sect := k/bp.sectsize + bp.fataddr;
	for(; sect<bp.rootaddr; sect+=bp.fatsize) {
		o := k%bp.sectsize;
		p := iotrack->getsect(xf, sect);
		if(p == nil)
			continue;

		case bp.fatbits {
		12 =>
			if(n&1) {
				p.iobuf[o] &= byte 16r0f;
				p.iobuf[o++] |= byte (val<<4);
				if(o >= bp.sectsize) {
					p.flags |= IoTrack->BMOD;
					iotrack->putsect(p);
					p = iotrack->getsect(xf, sect+1);
					if(p == nil)
						continue;
					o = 0;
				}
				p.iobuf[o] = byte (val>>4);
			}
			else {
				p.iobuf[o++] = byte val;
				if(o >= bp.sectsize) {
					p.flags |= IoTrack->BMOD;
					iotrack->putsect(p);
					p = iotrack->getsect(xf, sect+1);
					if(p == nil)
						continue;
					o = 0;
				}
				p.iobuf[o] &= byte 16rf0;
				p.iobuf[o] |= byte ((val>>8)&16r0f);
			}
		16 =>
			p.iobuf[o++] = byte val;
			p.iobuf[o] = byte (val>>8);
		}

		p.flags |= IoTrack->BMOD;
		iotrack->putsect(p);
	}
}

falloc(xf: ref Xfs): int
{
	bp := xf.ptr;
	n := bp.freeptr;

	for(;;) {
		if(getfat(xf, n) == 0)
			break;

		if(++n >= bp.fatclusters)
			n = 2;

		if(n == bp.freeptr)
			return -1;
	}

	bp.freeptr = n+1;
	if(bp.freeptr >= bp.fatclusters)
		bp.freeptr = 2;

	putfat(xf, n, 16rffff);
	k := bp.dataaddr + (n-2)*bp.clustsize;

	for(i:=0; i<bp.clustsize; i++) {
		p := iotrack->getosect(xf, k+i);
		if(p == nil)
			break;
		for(j:=0;j<len p.iobuf;j++)
			p.iobuf[j] = byte 0;
		p.flags = IoTrack->BMOD;
		iotrack->putsect(p);
	}
	return n;
}

bootdump(b : ref Dosboot)
{
	if(!(g.chatty & VERBOSE))
		return;

	if(debug) {
		chat(sprint("magic: 0x%2.2x 0x%2.2x 0x%2.2x\n",
			int b.magic[0], int b.magic[1], int b.magic[2]));
		chat(sprint("version: \"%8.8s\"\n", string b.version));
		chat(sprint("sectsize: %d\n", bytes2short(b.sectsize)));
		chat(sprint("allocsize: %d\n", int b.clustsize));
		chat(sprint("nresrv: %d\n", bytes2short(b.nresrv)));
		chat(sprint("nfats: %d\n", int b.nfats));
		chat(sprint("rootsize: %d\n", bytes2short(b.rootsize)));
		chat(sprint("volsize: %d\n", bytes2short(b.volsize)));
		chat(sprint("mediadesc: 0x%2.2x\n", int b.mediadesc));
		chat(sprint("fatsize: %d\n", bytes2short(b.fatsize)));
		chat(sprint("trksize: %d\n", bytes2short(b.trksize)));
		chat(sprint("nheads: %d\n", bytes2short(b.nheads)));
		chat(sprint("nhidden: %d\n", bytes2int(b.nhidden)));
		chat(sprint("bigvolsize: %d\n", bytes2int(b.bigvolsize)));
		chat(sprint("driveno: %d\n", int b.driveno));
		chat(sprint("reserved0: 0x%2.2x\n", int b.reserved0));
		chat(sprint("bootsig: 0x%2.2x\n", int b.bootsig));
		chat(sprint("volid: 0x%8.8x\n", bytes2int(b.volid)));
		chat(sprint("label: \"%11.11s\"\n", string b.label));
	}
}

Tm: adt
{
	sec:	int;
	min:	int;
	hour:	int;
	mday:	int;
	mon:	int;
	year:	int;
	wday:	int;
	yday:	int;
	zone:	string;
	tzoff:	int;
};

TZSIZE:		con 150;

ldmsize := array[] of {
	31, 29, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31
};

Timezone: adt
{
	stname: string;
	dlname: string;
	stdiff:	int;
	dldiff: int;
	dlpairs: array of int;
};

timezone: Timezone;

readtimezone()
{
	timezone.dlpairs = array[TZSIZE] of int;
	timezone.stdiff = 0;
	timezone.stname = "GMT";
	timezone.dlpairs[0] = 0;

	i := sys->open("/locale/timezone", sys->OREAD);
	if(i == nil)
		return;

	buf := array[2048] of byte;
	cnt := sys->read(i, buf, len buf);
	if(cnt <= 0)
		return;

	(n, val) := sys->tokenize(string buf[0:cnt], "\t \n");
	if(n < 5)
		return;

	stname := hd val;
	val = tl val;
	timezone.stdiff = int hd val;
	val = tl val;
	timezone.dlname = hd val;
	val = tl val;
	timezone.dldiff = int hd val;
	val = tl val;

	for(j := 0; j < TZSIZE-1; j++) {
		timezone.dlpairs[j] = int hd val;
		val = tl val;
		if(val == nil) {
			timezone.stname = stname;
			timezone.dlpairs[j+1] = 0;
			return;
		}
	}

	timezone.dlpairs[0] = 0;
}

dysize(y: int): int
{
	if((y%4) == 0)
		return 366;
	return 365;
}

gmt(tim: int): ref Tm
{
	xtime := ref Tm;

	# break initial number into days
	hms := tim % 86400;
	day := tim / 86400;
	if(hms < 0) {
		hms += 86400;
		day -= 1;
	}

	# generate hours:minutes:seconds
	xtime.sec = hms % 60;
	d1 := hms / 60;
	xtime.min = d1 % 60;
	d1 /= 60;
	xtime.hour = d1;

	# day is the day number.
	# generate day of the week.
	# The addend is 4 mod 7 (1/1/1970 was Thursday)
	xtime.wday = (day + 7340036) % 7;

	# year number
	if(day >= 0)
		for(d1 = 70; day >= dysize(d1); d1++)
			day -= dysize(d1);
	else
		for (d1 = 70; day < 0; d1--)
			day += dysize(d1-1);
	xtime.year = d1;
	d0 := day;
	xtime.yday = d0;

	# generate month
	if(dysize(d1) == 366)
		dmsz := ldmsize;
	else
		dmsz = dmsize;
	for(d1 = 0; d0 >= dmsz[d1]; d1++)
		d0 -= dmsz[d1];
	xtime.mday = d0 + 1;
	xtime.mon = d1;
	xtime.zone = "GMT";
	return xtime;
}

local(tim: int): ref Tm
{
	ct: ref Tm;

	t := tim + timezone.stdiff;
	dlflag := 0;
	for(i := 0; timezone.dlpairs[i] != 0; i += 2) {
		if(t >= timezone.dlpairs[i] && t < timezone.dlpairs[i+1]) {
			t = tim + timezone.dldiff;
			dlflag++;
			break;
		}
	}
	ct = gmt(t);
	if(dlflag) {
		ct.zone = timezone.dlname;
		ct.tzoff = timezone.dldiff;
	}
	else {
		ct.zone = timezone.stname;
		ct.tzoff = timezone.stdiff;
	}
	return ct;
}

puttime(d : ref Dosdir)
{
	t := local((sys->millisec() - nowt1)/1000 + nowt);
	x := (t.hour<<11) | (t.min<<5) | (t.sec>>1);
	d.time[0] = byte x;
	d.time[1] = byte (x>>8);
	x = ((t.year-80)<<9) | ((t.mon+1)<<5) | t.mday;
	d.date[0] = byte x;
	d.date[1] = byte (x>>8);
}

dmsize := array[12] of { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


gtime(arr : array of byte) : int
{
	noonGMT := 12*60*60 ;
	dptime := arr[22:24];
	dpdate := arr[24:26];
	i := bytes2short(dptime);
	h := i>>11; m := (i>>5)&63; s := (i&31)<<1;
	i = bytes2short(dpdate);
	y := 80+(i>>9); M := (i>>5)&15; d := i&31;

	if (M < 1 || M > 12)
		return 0;
	if (d < 1 || d > dmsize[M-1])
		return 0;
	if (h > 23)
		return 0;
	if (m > 59)
		return 0;
	if (s > 59)
		return 0;
	y += 1900;
	t := 0;
	for(i=1970; i<y; i++)
		t += dysize(i);
	if (dysize(y)==366 && M >= 3)
		t++;
	while(--M)
		t += dmsize[M-1];
	t += d-1;
	noonGMT += 24*60*60*t;
	t = 24*t + h;
	t = 60*t + m;
	t = 60*t + s;
	t += (12-7)*60*60; # 7==localtime(noonGMT).hour
	return t;
}

dirdump(arr : array of byte, addr, offset : int) : string
{
	attrchar:= "rhsvda67";

	buf : string;
	if(!g.chatty)
		return "";

	d := Dosdir.arr2Dd(arr);
	buf += sprint("\"%.8s.%.3s\" ", d.name, d.ext);
	p_i:=7;

	for(i := 16r80; i != 0; i >>= 1) {
		if((d.attr & byte i) ==  byte i)
			ch := attrchar[p_i];
		else 
			ch = '-'; 
		buf += sprint("%c", ch);
		p_i--;
	}

	i = bytes2short(d.time);
	buf += sprint(" %2.2d:%2.2d:%2.2d", i>>11, (i>>5)&63, (i&31)<<1);
	i = bytes2short(d.date);
	buf += sprint(" %2.2d.%2.2d.%2.2d", 80+(i>>9), (i>>5)&15, i&31);
	buf += sprint(" %d %d", bytes2short(d.start), bytes2short(d.length));
	buf += sprint(" %d %d\n",addr,offset);

	if(debug)
		chat(buf);

	return buf;
}

putnamesect(longname: string, curslot, start, first: int, ds: ref Dosslot)
{
	ds.start[0] = byte(start);
	ds.start[1] = byte(start>>8);
	ds.name0_4=ds.name5_10=ds.name11_12="";	
	if(first)
		ds.id = byte(16r40 | curslot);
	else 
		ds.id = byte curslot;

	j := (curslot-1)*13;
	for(i := 0;i<5 && j<len longname; i++)
		ds.name0_4[len ds.name0_4]=longname[j++];

	for(i = 0; i<6 && j<len longname; i++)
		ds.name5_10[len ds.name5_10]=longname[j++];

	for(i=0; i<2 && j<len longname; i++)
		ds.name11_12[i]=longname[j++];
}

bytes2int(arr: array of byte): int 
{
	retval : int;
	retval = int arr[3];
	retval = retval << 8 | int arr[2];
	retval = retval << 8 | int arr[1];
	retval = retval << 8 | int arr[0];
	return retval;
}

bytes2short(arr: array of byte): int 
{
	retval : int;
	retval = int arr[1];
	retval = retval << 8 | int arr[0];
	return retval;
}

chat(s: string)
{
	if(g.chatty & VERBOSE)
		sys->fprint(fd, "%s\n", s);
}

panic(s: string)
{
	sys->fprint(fd, "dosfs: panic: %s\n", s);
	for(;;) sys->sleep(5000);
	exit;
}

xerrstr(e : int) : string 
{
	if (e < 0 || e >= len errmsg)
		return "no such error";
	else
		return errmsg[e];
}

Dosboot.arr2Db(arr : array of byte) : ref Dosboot
{
	retval := ref Dosboot;
	retval.magic = arr[0:3];
	retval.version = arr[3:11];
	retval.sectsize = arr[11:13];
	retval.clustsize = arr[13];
	retval.nresrv = arr[14:16];
	retval.nfats = arr[16];
	retval.rootsize = arr[17:19];
	retval.volsize = arr[19:21];
	retval.mediadesc = arr[21];
	retval.fatsize = arr[22:24];
	retval.trksize = arr[24:26];
	retval.nheads = arr[26:28];
	retval.nhidden = arr[28:32];
	retval.bigvolsize = arr[32:36];
	retval.driveno	= arr[36];
	retval.reserved0= arr[37];
	retval.bootsig	= arr[38];
	retval.volid = arr[39:43];
	retval.label = arr[43:54];
	retval.reserved1 = arr[54:62];
	return retval;
}

Dosdir.arr2Dd(arr : array of byte) : ref Dosdir
{
	retval := ref Dosdir;
	for(i := 0; i < 8; i++)
		retval.name[len retval.name] = int arr[i];

	for(; i < 11; i++)
		retval.ext[len retval.ext] = int arr[i];

	retval.attr = arr[11];
	retval.reserved = arr[12:22];
	retval.time = arr[22:24];
	retval.date = arr[24:26];
	retval.start = arr[26:28];
	retval.length = arr[28:32];

	return retval;
}

Dosdir.Dd2arr(d : ref Dosdir) : array of byte
{
	retval := array[32] of byte;
	i:=0;
	for(j := 0; j < len d.name; j++)
		retval[i++] = byte d.name[j];

	for(; j<8; j++)
		retval[i++]= byte 0;

	for(j=0; j<len d.ext; j++)
		retval[i++] = byte d.ext[j];

	for(; j<3; j++)
		retval[i++]= byte 0;

	retval[i++] = d.attr;

	for(j=0; j<10; j++)
		retval[i++] = d.reserved[j];

	for(j=0; j<2; j++)
		retval[i++] = d.time[j];

	for(j=0; j<2; j++)
		retval[i++] = d.date[j];

	for(j=0; j<2; j++)
		retval[i++] = d.start[j];

	for(j=0; j<4; j++)
		retval[i++] = d.length[j];

	return retval;
}

Dosslot.Ds2arr(d : ref Dosslot) : array of byte
{
	retval := array[32] of {* => byte 0};
	i:=0;
	retval[i++] = d.id;

	for(j := 0; j < len d.name0_4 && j<5 ; j++) {
		retval[i++] = byte d.name0_4[j];
		retval[i++] = byte 0;
	}

	retval[11] = d.attr;
	retval[12] = d.reserved;
	retval[13] = d.alias_checksum;

	i=14;
	for(j = 0; j < len d.name5_10 && j<6 ; j++) {
		retval[i++] = byte d.name5_10[j];
		retval[i++] = byte 0;
	}

	i=26;
	for(j=0; j<2; j++)
		retval[i++] = d.start[j];

	for(j = 0; j<len d.name11_12 && j<2; j++){
		retval[i++] = byte d.name11_12[j];
		retval[i++] = byte 0;
	}
	return retval;
}
