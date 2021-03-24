Rand: module
{
	PATH:	con "/dis/lib/rand.dis";
	# init sets a seed
	init:	fn(seed: int);

	# rand returns something in 0 .. modulus-1
	# (if 0 < modulus < 2^31)
	rand:       fn(modulus: int): int;
	# (if 0 < modulus < 2^53)
	bigrand:    fn(modulus: big): big;
};
