Dosfs : module {

	PATH : con "/dis/svc/dossrv/dosfs.dis";

	init : fn(s : string, l: string,i : int);
	setup : fn();	
	dossrv : fn(fd :ref Sys->FD);
	rnop : fn();
	rflush : fn();
	rattach : fn();
	rclone : fn();
	rwalk : fn();
	ropen : fn();
	rcreate : fn();
	rread : fn();
	rwrite : fn();
	rclunk : fn();
	rremove : fn();
	rstat : fn();
	rwstat : fn();
		
};
