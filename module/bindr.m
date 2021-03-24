# Bind recursively
Bindr: module
{
	PATH : con "/dis/bindr.dis";

	MREPL:		con 0;
	MBEFORE:	con 1;
	MAFTER:		con 2;
	MCREATE:	con 4;

	MUNION : con 8;		# [-u] recursive union
	MWNION : con 16;	# [-w] double recursive union
	MTEST : con 32;		# [-t] test only
	MSRCR : con 64;		# [-s] recurse on source tree
	MCRAFT : con 128;	# [-C] MCREATE is applied to the -a of a union
				# by default -c applies MCREATE to -b of union
	MFILE : con 256;	# [-f] bind replacing files only

	# extend bind init to recurse on target (default) matching source stat
	# -[rc] -- recursive replacement of directory trees
	# -t -- print only mode (testing)
	#	MTEST test only modifier
	# -s -- recurse on source tree (instead of target -- default)
	#	MSRCR source recursion modifier
	# -x -- apply unmount recursively (undo)
	# -u[abr] -- recursive union:
	#	MUNION recurse on target (-s source) binds in two steps:
	#	1) bindr -a targets sources
	#	2) bind -[abr] source target
	# -w[abr] -- double recursive union:
        # 	MWUNION recurse once on target (-s source) binds in two steps:
	# 	1) bindr -a targets sources
	# 	2) bindr -[abr] sources targets
	# -f[rc] -- recursive file replace:
	#	MFILE bind replacing each file in the tree.
	# -z -- use stdio to read/write recurse work list
	# -Z -- next argument is the name of a file used to store
	#	the recurse work list(s) the first time bindrr is invoked
	#	and if this file does not already exist. The file argument
	#	is used to read the recurse work list(s) used by bindrr.
	# -zz - bindr[r] then unmountr[r] passing recurse list on channel
	#
	# recursive union modifiers (allow creation):
	# -C[uw] -- create option added to the first step (-a) of a union
	#	MCRAFT create after modifier
	# -c[uw] -- create option added to the second step of a union
	#
	# bindrr first argument -- special notation:
	# -<file>
	#	if this is the first argument in the argument list of a
	#	bindrr function (bindrr, bindrrch, unmountrr, unmountrrch)
	#	then <file> is the name of a file containing the recurse
	#	work list(s) as in the -Z option to Bindr->init(,).
	#	If <file> does not exist, the bindrr function creates it
	#	to store the work list(s). If <file> is empty (the first
	#	argument is "-") then stdin/stdout are used to input/output
	#	the recurse work list(s). The value of <file> specifies
	#	the path relative to the first source directory (second
	#	argument in the args list). However, if <file> begins with
	#	"/" it specifies an absolute pathname.

	# use init(nil, nil) to init module or pass args to do work
	init :	fn(ctxt: ref Context, argv: list of string);

	# single source to single target

	bindr :	fn(src, dest : string, flags : int) : int;
	unmountr : fn(src, dest : string, flags : int) : int;

	# channel interface for exact and efficient undo

	# write on channel its recurse-work-list
	bindrch : fn(src, dest : string, flags : int, out : chan of string) : int;
	# read recurse work list from channel (instead of computing it)
	unmountrch : fn(src, dest : string, flags : int, in : chan of string) : int;

	# multi source to single target

	bindrr :	fn(st : list of string, flags : int) : int;
	unmountrr :	fn(st : list of string, flags : int) : int;

	# channel interface for exact and efficient undo

	# write on channel its recurse work list
	bindrrch : 	fn(st : list of string, flags : int, out : chan of string) : int;
	# read recurse-work-list from channel (instead of computing it)
	unmountrrch : 	fn(st : list of string, flags : int, in : chan of string) : int;
};
