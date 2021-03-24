#
# rc4 symmetric key algorithm
#

RC4: module {

	PATH: con "/dis/lib/crypt/rc4.dis";

	RC4state: adt {
		state				: array of byte; # [256]
		x				: int;
		y				: int;
	};

	setupRC4state: fn(start: array of byte, n: int): ref RC4state;
	rc4: fn(key: ref RC4state, a: array of byte, n: int);
};

