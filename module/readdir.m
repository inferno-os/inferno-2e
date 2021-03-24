Readdir: module
{
	PATH:	con	"/dis/lib/readdir.dis";

	# sortkey is one of NAME, ATIME, MTIME, SIZE, or NONE
	# possibly with DESCENDING or'd in

	NAME, ATIME, MTIME, SIZE, NONE: con iota;
	DESCENDING:	con (1<<5);

	init:	fn(path: string, sortkey: int): (array of ref Sys->Dir, int);
	sortdir:fn(a: array of ref Sys->Dir, key: int): (array of ref Sys->Dir, int);

	# Readdir init() modifiers DIR, FILE, and COMPACT
	# possibly  or'd into init() sortkey argument.
	# Modifiers are used to restrict the set of Dir adts returned
	# -- sortdir() ignores these modifiers in sortkey.

	DIR:		con (1<<6);
	FILE:		con (1<<7);

	# COMPACT remove name-dupplicate Dir adts
	# -- recommended when the directory read is a namespace
	#    union and other than NONE sortkey is used to sort Dir adts.
	# Note: sortdir does not preserve the ordering of same
	#       attribute Dir adts from a namespace union.

	COMPACT:	con (1<<4);
};
