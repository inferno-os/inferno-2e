DosSubs : module {
	
	PATH :	con "/dis/svc/dossrv/dossubs.dis";
	
	Global : adt {
		thdr, rhdr :  Styx->Smsg;
		deffile : string;
		logfile : string;
		chatty : int;
		errno : int;
		iotrack : IoTrack;
		Dos : DosSubs;
	};


	Dospart : adt {
		active		: byte;
		hstart		: byte;
		cylstart	: array of byte;
		typ		: byte;
		hend		: byte;
		cylend		: array of byte;
		start		: array of byte;
		length		: array of byte;
	};
	
	Dosboot : adt {
		arr2Db		: fn(arr : array of byte) : ref Dosboot;
		magic		: array of byte;
		version		: array of byte;
		sectsize	: array of byte;
		clustsize	: byte;
		nresrv		: array of byte;
		nfats		: byte;
		rootsize	: array of byte;
		volsize		: array of byte;
		mediadesc	: byte;
		fatsize		: array of byte;
		trksize		: array of byte;
		nheads		: array of byte;
		nhidden		: array of byte;
		bigvolsize	: array of byte;
		driveno		: byte;
		reserved0	: byte;
		bootsig		: byte;
		volid		: array of byte;
		label		: array of byte;
		reserved1	: array of byte;
	};
	
	Dosbpb : adt {
		sectsize	: int;	# in bytes 
		clustsize	: int;	# in sectors 
		nresrv		: int;	# sectors 
		nfats		: int;	# usually 2 
		rootsize	: int;	# number of entries 
		volsize		: int;	# in sectors 
		mediadesc	: int;
		fatsize		: int;	# in sectors 
		fatclusters	: int;
		fatbits		: int;	# 12 or 16 
		fataddr		: int; #big;	# sector number 
		rootaddr	: int; #big;
		dataaddr	: int; #big;
		freeptr		: int; #big;	# next free cluster candidate 
	};
	
	Dosdir : adt {
		Dd2arr		: fn(d :ref Dosdir) : array of byte;
		arr2Dd		: fn(arr : array of byte) : ref Dosdir;
		name		: string;
		ext		: string;
		attr		: byte;
		reserved	: array of byte;
		time		: array of byte;
		date		: array of byte;
		start		: array of byte;
		length		: array of byte;
	};
	
	Dosslot : adt {
		Ds2arr		: fn(d : ref Dosslot) : array of byte;
		arr2Ds		: fn(arr : array of byte) : ref Dosslot;
		id 		: byte;
		name0_4 	: string;
		attr 		: byte;
		reserved 	: byte;
		alias_checksum 	: byte;
		name5_10 	: string;
		start 		: array of byte;
		name11_12 	: string;
	};

	
	Dosptr : adt {
		addr	: int;	# of file's directory entry 
		offset	: int;
		paddr	: int;	# of parent's directory entry 
		poffset	: int;
		iclust	: int;	# ordinal within file 
		clust	: int;
		prevaddr: int;
		naddr	: int;
		p	: ref IoTrack->Iosect;
	};
	
	
	Asis, Clean, Clunk : con iota;
	
	FAT12	: con byte 16r01;
	FAT16	: con byte 16r04;
	FATHUGE	: con byte 16r06;
	DMDDO	: con byte 16r54;
	DRONLY	: con byte 16r01;
	DHIDDEN	: con byte 16r02;
	DSYSTEM	: con byte 16r04;
	DVLABEL	: con byte 16r08;
	DDIR	: con byte 16r10;
	DARCH	: con byte 16r20;

	Oread 	: con  1;
	Owrite 	: con  2;
	Orclose	: con  4;
	Omodes 	: con  3;
	
	VERBOSE, STYX_MESS, FAT_INFO, CLUSTER_INFO : con (1 << iota);

	Enevermind,
	Eformat,
	Eio,
	Enomem,
	Enonexist,
	Eexist,
	Eperm,
	Enofilsys,
	Eauth: con iota;
	
	chat : fn(s : string);
	panic : fn(s: string);
	xerrstr : fn(s: int) : string;
	
	dosfs: fn(xf : ref IoTrack->Xfs) : int;
	init : fn(g :ref Global);		
	setup : fn();	
	putfile: fn(f : ref IoTrack->Xfile);

	getdir: fn(a : array of byte, addr,offset: int) : ref Sys->Dir;
	getfile: fn(f : ref IoTrack->Xfile) :int;

	truncfile: fn(f : ref IoTrack->Xfile) : int;

	dosreaddir: fn(f :ref IoTrack->Xfile, offset, count : int) : (int,array of byte);
	readfile: fn(f : ref IoTrack->Xfile, offset,count : int) : (int,array of byte);

	searchdir: fn(f :ref IoTrack->Xfile,name:string,cflag: int,lflag: int) 
				: (int,ref Dosptr);
	walkup: fn(f : ref IoTrack->Xfile) : (int , ref Dosptr);

	puttime: fn(d : ref Dosdir);
	putname: fn(p : string, d : ref Dosdir);

	falloc: fn(xf : ref IoTrack->Xfs) : int;

	writefile: fn(f : ref IoTrack->Xfile, a : array of byte, offset,count : int) : int;

	emptydir : fn(f : ref IoTrack->Xfile) : int;

	name2de : fn(s : string) : int;
	long2short : fn(s : string,i : int) : string;
	putnamesect : fn(s: string, nds,start,first : int,ds: ref Dosslot);
	getnamesect : fn(arr : array of byte) : string;
};
