Styx : module
{
	PATH 	: con "/dis/svc/dossrv/styx.dis";

	ConvM2S: fn(a : array of byte) 	: (int,Smsg);
	convD2M: fn(f : ref Sys->Dir) : array of byte;
	convM2D: fn(f : array of byte) : ref Sys->Dir;

	Tnop,		#  0 
	Rnop,		#  1 
	Terror,		#  2, illegal 
	Rerror,		#  3 
	Tflush,		#  4 
	Rflush,		#  5 
	Tclone,		#  6 
	Rclone,		#  7 
	Twalk,		#  8 
	Rwalk,		#  9 
	Topen,		# 10 
	Ropen,		# 11 
	Tcreate,	# 12 
	Rcreate,	# 13 
	Tread,		# 14 
	Rread,		# 15 
	Twrite,		# 16 
	Rwrite,		# 17 
	Tclunk,		# 18 
	Rclunk,		# 19 
	Tremove,	# 20 
	Rremove,	# 21 
	Tstat,		# 22 
	Rstat,		# 23 
	Twstat,		# 24 
	Rwstat,		# 25 
	Tsession,	# 26
	Rsession,	# 27
	Tattach,	# 28 
	Rattach,	# 29
	Tmax		: con iota;
	
	NAMELEN : con 28;
	DIRLEN	: con 116;
	ERRLEN	: con 64;
	
	OREAD 	: con 0; 		# open for read 
	OWRITE 	: con 1; 		# write 
	ORDWR 	: con 2; 		# read and write 
	OEXEC 	: con 3; 		# execute, == read but check execute permission 
	OTRUNC 	: con 16; 		# or'ed in (except for exec), truncate file first 
	OCEXEC 	: con 32; 		# or'ed in, close on exec 
	ORCLOSE : con 64; 		# or'ed in, remove on close 
	CHEXCL	: con 16r20000000;	# mode bit for exclusive use files 	
	Smsg : adt {
		ConvS2M: fn(s : self Smsg) 		: array of byte;
		print : fn(s : self Smsg) : string;
		Mtype			: int;
		Tag			: int;
		fid			: int;
		oldtag			: int;			# T-Flush 
		qid			: Sys->Qid;		# R-Attach, R-Walk, 
								# R-Open, R-Create 
		uname			: string;		# T-Attach 
		aname			: string;		# T-Attach 
		ename			: string;		# R-Error 
		perm			: int;			# T-Create  
		newfid			: int;			# T-Clone 
		name			: string;		# T-Walk, T-Create 
		mode			: int;			# T-Create, T-Open 
		offset			: int;			# T-Read, T-Write 
		count			: int;			# T-Read, T-Write, R-Read 
		data			: array of byte;	# T-Write, R-Read 
		stat			: array of byte;	# T-Wstat, R-Stat 
	};

};


		
