implement Nsbuild;

include "sys.m";
include "draw.m";
sys: Sys;
FD: import Sys;
Context: import Draw;

include "newns.m";

stderr: ref FD;

Nsbuild: module
{
	init: fn(ctxt: ref Context, argv: list of string);
};

init(nil: ref Context, argv: list of string)
{
	nsfile: string;

	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	ns := load Newns "/dis/lib/newns.dis";
	if(ns == nil) {
		sys->fprint(stderr, "Error loading module Newns");
		return;
	}

	if(len argv > 2) {
		sys->fprint(stderr, "Usage: nsbuild [nsfile]\n");
		return;
	}

	if(len argv == 2)
		nsfile = hd tl argv;

   	e := ns->newns(nil, nsfile);
	if(e != "")
                sys->fprint(stderr, "nsbuild: error building namespace: %s\n", e);
} 
