SSLHS: module {
	PATH: con "/dis/lib/sslhs.dis";

	Key : adt {
		modulus : array of byte;
		modlen: int;
		exponent: array of byte;
	};

	init: fn(v: int);
	client: fn(fd: ref Sys->FD, servname: string): (string, Sys->Connection);
	server: fn(fd: ref Sys->FD, clientname: string, cert: array of byte, privkey: ref Key):
			(string, Sys->Connection);
};
