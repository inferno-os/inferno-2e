IoTrack : module {
	
	PATH :	con "/dis/svc/dossrv/iotrack.dis";


	# An Xfs represents the root of an external file system, anchored
	# to the server and the client
	Xfs : adt {
		next 	: cyclic ref Xfs;
		name	: string;	# of file containing external f.s. 
		qid	: Sys->Qid;	# of file containing external f.s. 
		refn	: int;		# attach count 
		rootqid	: Sys->Qid;	# of inferno constructed root directory 
		dev	: ref Sys->FD;  # FD of the file containing external f.s.
		fmt	: int;		# ?
		offset	: int;		# ?
		ptr 	: ref DosSubs->Dosbpb;
	};
	
	# An Xfile represents the mapping of fid's & qid's to the server.
	Xfile : adt {
		next 	: cyclic ref Xfile;		# in hash bucket 
		client	: int;
		fid	: int;
		flags	: int;
		qid	: Sys->Qid;
		xf 	: ref Xfs;
		ptr 	: ref DosSubs->Dosptr;
	};
	
	Iosect : adt
	{
		next 	: cyclic ref Iosect;
		flags	: int;
		t	: cyclic ref Iotrack;
		iobuf	: array of byte;
	};
	
	Iotrack : adt 
	{
		flags	: int;
		xf	: ref Xfs;
		addr	: int;
		next 	: cyclic ref Iotrack;		# in lru list 
		prev 	: cyclic ref Iotrack;
		hnext 	: cyclic ref Iotrack;		# in hash list 
		hprev 	: cyclic ref Iotrack;
		refn	: int;
		tp	: cyclic ref Track;
	};
	
	
	Track : adt
	{
		create  : fn() : ref Track;
		p	: cyclic array of ref Iosect;
		buf	: array of byte;
	};
	
	BMOD		: con	1<<0;
	BIMM		: con	1<<1;
	BSTALE		: con	1<<2;
	
	HIOB 		: con 31;	# a prime 
	NIOBUF		: con 160;
	
	Sectorsize 	: con 512;
	Sect2trk 	: con 9;
	Trksize 	: con Sectorsize*Sect2trk;

	iotrack_init: fn(g : ref DosSubs->Global);	
	xfile: fn(fid, flag : int) : ref Xfile;
	getxfs: fn(name : string) : ref Xfs;
	refxfs: fn(xf : ref Xfs, delta : int);
	getsect: fn(xf : ref Xfs, addr : int) : ref Iosect;
	putsect: fn(p : ref Iosect);
	purgebuf: fn(xf : ref Xfs);
	getosect: fn(xf : ref Xfs, addr : int) : ref Iosect;
	sync: fn();
};
