Qidcmp : module
{
	PATH : con "/dis/lib/qidcmp.dis";

	# Compare the full qid information of file or directory
	# and return a non-zero value if the file has changed

	SAME, INIT, DIFF, OLD, NEW : con iota;

	# INIT	- file is recorded to initialize the comparison
	# DIFF	- dev, qtype, qid.path or qid.version are different
	# OLD	- the file is older than the recorded one
	# NEW	- the file is newer than the recorded one
  
	init : fn(ctxt: ref Draw->Context, args: list of string);

	# Initialize a comparison instance
	# c := Cdir(nil, SAME);

	Cdir : adt {
		dir : ref Sys->Dir;
		m : int;

		# Compare filename with previous
		fcmp : fn(c : self ref Cdir, file : string) : int;

		# Compare dir adt with previous
		cmp : fn(c : self ref Cdir, dir : ref Sys->Dir) : int;
	};
};
