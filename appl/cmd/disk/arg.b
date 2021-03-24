implement Arg;

#From: Roger Peppe <rog@ohm.york.ac.uk>

include "arg.m";

name:= "";
args: list of string;

curropt: string;

init(argv: list of string)
{
	if (argv == nil)
		return;
	name = hd argv;
	args = tl argv;
}

progname() : string
{
	return name;
}

# don't allow any more options after this function is invoked
argv() : list of string
{
	ret := args;
	args = nil;
	return ret;
}

# get next option argument
arg() : string
{
	if (curropt != "") {
		ret := curropt;
		curropt = nil;
		return ret;
	}

	if (args == nil)
		return nil;

	ret := hd args;
	if (ret[0] == '-')
		ret = nil;
	else
		args = tl args;
	return ret;
}

# get next option letter
# return 0 at end of options
opt() : int
{
	if (curropt != "") {
		opt := curropt[0];
		curropt = curropt[1:];
		return opt;
	}

	if (args == nil)
		return 0;

	nextarg := hd args;
	if (nextarg[0] != '-' || len nextarg < 2)
		return 0;

	if (nextarg == "--") {
		args = tl args;
		return 0;
	}

	opt := nextarg[1];
	if (len nextarg > 2)
		curropt = nextarg[2:];
	args = tl args;
	return opt;
}
