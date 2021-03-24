#include "tdb.m";

Mail: module
{
	PATH: con "/dis/mailtool/mailtool.dis";

        toolbarinit: fn(ctxt: ref Draw->Context, argv:list of string, Ch1, Ch2:chan of string);
        init:   fn(ctxt: ref Draw->Context, argv: list of string);
};

