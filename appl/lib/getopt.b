implement Getopt;
#############################################################
#
#   getopt.b
#
#   Get command line options
#
#   Copyright 1998, Lucent Technologies. All rights reserved.
#
#############################################################
include "getopt.m";

include "string.m";
 str: String;

include "sys.m";
 sys: Sys;

FirstTime: int = 1;
DashListPos: int;		# 0 for not in dash list
Debug: int = 0;
opterr = 1;

getopt(argv: list of string, options: string): int
{
  sys = load Sys Sys->PATH;
  str = load String String->PATH;

  if (FirstTime && argv != nil) {
    FirstTime = 0;
    optleft = tl argv;
    DashListPos = 0;
  }
  if (optleft == nil) {
    return '\0';
  }

  if (Debug) sys->print("hd optleft = %s\n", hd optleft);

  arg := hd optleft;

  if (!DashListPos) {
    # start processing new arg
    if (arg[0] == '-') {
      # new option list
      DashListPos = 1;
      return processOpt(arg, options);
    } else {
      # not an option, we are done
      return '\0';
    }
  } else {
    # process next option in option list
    return processOpt(arg, options);
  }
  return '*';
}

processOpt(arg, options: string): int
{
  opt := arg[DashListPos];
  optStr := arg[DashListPos:DashListPos+1];
  if (!str->in(opt, options)) {
    badOption(opt);
    optleft = tl optleft;
    DashListPos = 0;
    optarg = "";
    return '?';
  }
  (left, right) := str->splitr(options, optStr);
  if (right != nil && right[0] == ':') {
    # there is an argument for this option
    if (len arg > DashListPos+1) {
      # argument is right next to option
      optarg = arg[DashListPos+1:];
    } else if (tl optleft != nil) {
      optleft = tl optleft;
      optarg = hd optleft;
    } else {
      badArg(opt);
      optleft = tl optleft;
      DashListPos = 0;
      return '?';
    }
    optleft = tl optleft;
    DashListPos = 0;
    return opt;
  } else {
    # no argument
    DashListPos++;
    if (len arg == DashListPos) {
      # no more options in list
      optleft = tl optleft;
      DashListPos = 0;
    }
    optarg = "";
    return opt;
  }
}

badArg(opt: int)
{
  if (opterr) {
    sys = load Sys Sys->PATH;
    sys->print("no argument to option %c\n", opt);
  }
  optopt = opt;
}

badOption(opt: int)
{
  if (opterr) {
    sys = load Sys Sys->PATH;
    sys->print("unknown option %c\n", opt);
  }
  optopt = opt;
}

