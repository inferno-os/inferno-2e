Lock: module {
	PATH:	con "/dis/lib/lock.dis";

	Semaphore: adt {
		q: array of ref Sys->FD;
		obtain:	fn(nil: self ref Semaphore);
		release: fn(nil: self ref Semaphore);

		init:	fn(nil: self ref Semaphore);
	};
	
	init:	fn(): ref Semaphore;
};

