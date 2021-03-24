# bind recursively
# bindr [-options] source-list dest -- see bindr.m for options description.
implement Bindr;

Mod : con "Bindr";

include "sys.m";
include "draw.m";
include "readdir.m";
rd : Readdir;

FD: import Sys;
Context: import Draw;

include "bindr.m";

sys: Sys;
stdin, stdout, stderr: ref FD;

ldrd() : int
{
	if (sys == nil) {
	  sys = load Sys Sys->PATH;
	  stdin = sys->fildes(0);
	  stdout = sys->fildes(1);
	  stderr = sys->fildes(2);
	}
	if (rd == nil) {
	  rd = load Readdir Readdir->PATH;

	  if (rd == nil) {
	    sys->fprint(stderr, "Error["+Mod+"]: %s %r\n", Readdir->PATH);
	    return 0;
	  }
	}
	return 1;
}

usage()
{
  sys->fprint(stderr, "Usage:\tbindr [-[t]rab[CcxzZ]suwf] [source-list] target\n\tbindr -t*\t-- test only version\n\tbindr -C[uw]*\t-- apply create to first binding step\n\tbindr -c[uw]*\t-- apply create to second binding step\n\tbindr -x[uw]*\t-- recursive unmount (undo)\n\tbindr -z[x]*\t-- write [read] recurse work list to stdio\n\tbindr -Z[x]* FN\t-- read recurse work list from FileName\n\tbindr -zz*\t-- bindr/unmountr via io channel of recurse list\n\tbindr -s*\t-- recurse on source tree (instead of target)\n\tbindr -u*\t-- recursive union (recommended)\n\tbindr -w*\t-- recursive double union\n\tbindr -f[rc]*\t-- recursive file replacement\n");
}

init(nil: ref Context, argv: list of string)
{
	union := 0;
	file := 0;
	wnion := 0;
	copt := 0;
	srcr := 0;
	test := 0;
	umnt := 0;
	zioch := 0;
	fio : string;
	craft := 0;

	if(!ldrd())
	  return;

	if (argv == nil)	#init only
	  return;
	argv = tl argv;
	flags := sys->MREPL;
	while(argv != nil) {
		s := hd argv;
		if(s[0] != '-')
			break;
		for(i := 1; i < len s; i++) {
			case s[i] {
			'a' =>
				flags = sys->MAFTER;
			'b' =>
				flags = sys->MBEFORE;
			'r' =>
				flags = sys->MREPL;
			'u' =>
			  	union = 1;
			'w' =>
			  	wnion = 1;
			'f' =>
				file = 1;
			'c' =>
				copt++;
			's' =>
			  	srcr = 1;
			't' =>
			  	test = 1;
			'x' =>
				umnt = 1;
			'z' =>
			  	zioch++;
			'Z' =>  {
				argv = tl argv;
				if (argv == nil) {usage(); return;}
				fio = hd argv;
				if (fio[0] != '-') fio = "-"+fio;
				}
			'C' =>
			  	craft = 1;
			* =>
				usage();
				return;
			}
		}
		argv = tl argv;
	}
	if(union)
		flags |= MUNION;
	if(wnion)
		flags |= MWNION;
	if(copt)
		flags |= sys->MCREATE;
	if(test)
	  	flags |= MTEST;
	if(srcr)
	  	flags |= MSRCR;
	if(craft)
	  	flags |= MCRAFT;
	if(file)
		flags |= MFILE;

	if(zioch) {
	  case len argv {
	    0 => usage(); return;
	    2 => {
	      if (fio != nil) break;
	      if (zioch > 1)
		ziotestbindr(argv, flags);
	      else {
		if (!umnt)
		  if(bindrch(hd argv, hd tl argv, flags, nil) < 0)
		    sys->fprint(stderr, Mod+" bindrch: %r\n");
		if (umnt)
		  if (unmountrch(hd argv, hd tl argv, flags, nil) < 0)
		    sys->fprint(stderr, Mod+" unmountrch: %r\n");
	      }
	      return;
	    }
	  }
	  if (fio != nil) argv = fio :: argv;
	  if (zioch > 1)
	    ziotestbindrr(argv, flags);
	  else {
	    if (!umnt)
	      if(bindrrch(argv, flags, nil) < 0)
		sys->fprint(stderr, Mod+" bindrrch: %r\n");
	    if (umnt)
	      if (unmountrrch(argv, flags, nil) < 0)
		sys->fprint(stderr, Mod+" unmountrrch: %r\n");
	  }
	  return;
	}

	if(umnt) {
	  case len argv {
	    0 => sys->fprint(stderr, "Usage:\tbindr -x[[tszZ]uwf] [source-list] target\n"); usage(); return;
	    1 or 2 => {
	      if (fio != nil) break;
	      if (tl argv == nil) {
		if (unmountr(nil, hd argv, flags) < 0)
		  sys->fprint(stderr, Mod+": %r\n");
	      }
	      else if (unmountr(hd argv, hd tl argv, flags) < 0)
		sys->fprint(stderr, Mod+": %r\n");
	      return;
	    }
	  }
	  if (fio != nil) argv = fio :: argv;
	  if (unmountrr(argv, flags) < 0)
	    sys->fprint(stderr, Mod+": %r\n");
	  return;
	}

	case len argv {
	  0 => usage(); return;
	  2 => if (fio != nil) break;
	    if(bindr(hd argv, hd tl argv, flags) < 0)
	      sys->fprint(stderr, Mod+": %r\n");
	    return;
	}
	if (fio != nil) argv = fio :: argv;
	if(bindrr(argv, flags) < 0)
	  sys->fprint(stderr, Mod+": %r\n");
}

flagsok(flags : int) : int
{
  if (flags & MFILE) {
    tag : string;
    if (flags & Sys->MAFTER) tag += "a";
    if (flags & Sys->MBEFORE) tag += "b";
    if (flags & MUNION) tag += "u";
    if (flags & MWNION) tag += "w";
    if (flags & MCRAFT) tag += "C";
    if (tag != nil) {
      sys->fprint(stderr, Mod+": error: incompatible options [%s] and [f]\n", tag);
      return 0;
    }
  }
  return 1;
}

# multi source recursive version

ziotestbindrr(argv : list of string, flags : int)
{
  sys->print(Mod+": bindrrch...\n");
  ioch := chan of string;
  if(bindrrch(argv, flags, ioch) < 0)
    sys->fprint(stderr, Mod+" bindrch: %r\n");
  sys->print(Mod+": unmountrrch...\n");
  if (unmountrrch(argv, flags, ioch) < 0)
    sys->fprint(stderr, Mod+" unmountrrch: %r\n");
  sys->print(Mod+": bind/unmountrrch done\n");
}

bindrr(sd : list of string, flags : int) : int
{
  (ok, nil) := bindrrcoll(sd, flags);
  return ok;
}

bindrrch(sd : list of string, flags : int, out : chan of string) : int
{
  (ok, coll) := bindrrcoll(sd, flags);
  sp_printllz(coll, out, nil);
  return ok;
}

bindrrcoll(sd : list of string, flags : int) : (int, list of list of string)
{
  ok := 0;
  if(!ldrd() || !flagsok(flags))
    return (-1, nil);

  fio, file, new : string;
  if (sd != nil && fiop(hd sd)) {
    fio = hd sd; sd = tl sd;
  }
  if (sd == nil) {
    sys->fprint(stderr, Mod+": bindrr: missing arguments\n");
    return (-1, nil);
  }
  if (tl sd == nil) sd = hd sd :: sd;

  pl : list of ref pair;
  switchp := 0;
  if (flags & MSRCR) {
    flags &= ~MSRCR;
    pl = pairwise(sd);
    switchp = 1;
  }
  else
    pl = pairlast(sd);

  coll : list of list of string;
  if (fio != nil) {
    file = namefio(hd sd, fio);
    if (openfio(file, sys->OREAD) != nil)
      coll = readllz(nil, file);
    else {
      new = file;
      fio = nil;
    }
  }

  if (fio == nil)
    (ok, coll) = bindrrcollndo(pl, flags, switchp, Bind);
  else
    ok = bindrrdo(pl, coll, flags);

  if (new != nil)
    if (createfio(file) != nil)
      sp_printllz(coll, nil, new);
  return (ok, coll);      
}

unmountrr(sd : list of string, flags : int) : int
{
  if(!ldrd())
    return -1;
  fio, file : string;
  if (sd != nil && fiop(hd sd)) {
    fio = hd sd; sd = tl sd;
  }
  if (sd == nil) {
    sys->fprint(stderr, Mod+": unmountrr: missing arguments\n");
    return -1;
  }
  coll : list of list of string;
  collp := 0;
  if (fio != nil) {
    file = namefio(hd sd, fio);
    coll = revreadllz(nil, file);
    collp = 1;
  }
  return unmountrrcoll(sd, flags, collp, coll);
}

unmountrrch(sd : list of string, flags : int, in : chan of string) : int
{
  if(!ldrd())
    return -1;
  fio, file : string;
  if (sd != nil && fiop(hd sd)) {
    fio = hd sd; sd = tl sd;
  }
  if (sd == nil) {
    sys->fprint(stderr, Mod+": unmountrrch: missing arguments\n");
    return -1;
  }
  coll : list of list of string;
  collp := 0;
  if (fio != nil) {
    file = namefio(hd sd, fio);
    coll = revreadllz(nil, file);
    collp = 1;
  }
  if (!collp) {
    coll = revreadllz(in, nil);
    collp = 1;
  }
  return unmountrrcoll(sd, flags, collp, coll);
}

unmountrrcoll(sd : list of string, flags, collp : int, coll : list of list of string) : int
{
  if (!flagsok(flags))
    return -1;
  ok := 0;
  srcp := Srcp;
  if (tl sd == nil) {sd = hd sd :: sd; srcp = None;}

  pl : list of ref pair;
  switchp := 0;
  if (flags & MSRCR) {
    flags &= ~MSRCR;
    pl = revpairwise(sd);
    switchp = 1;
  }
  else
    pl = revpairlast(sd);

  if (!collp)
    (ok, coll) = bindrrcollndo(pl, flags, switchp, srcp|Unmount);
  else
    ok = unmountrrdo(pl, coll, flags, srcp);
  return ok;
}

# doers

None, Bind, Unmount : con iota;
Srcp : con (1<<2);
bindrrcollndo(pl : list of ref pair, flags, switchp, action : int) : (int, list of list of string)
{
  srcp := Srcp;
  if (action & Unmount) {
    srcp = action & Srcp;
    action &= ~Srcp;
  }
  ok := 0;
  coll : list of list of string;
  col : list of string;
  for (; pl != nil; pl = tl pl) {
    src := "";
    if (srcp)
      src = (hd pl).s;
    dest := (hd pl).d;
    if (switchp)
      col = relbindgatherer(dest, src, flags & MFILE);
    else
      col = relbindgatherer(src, dest, flags & MFILE);
    coll = col :: coll;
    case action {
      Bind => er := bindrl(src, dest, flags, col); if (er < 0) ok = er;
      Unmount => er := unmountrl(src, dest, flags, col); if (er < 0) ok = er;
    }
  }
  return (ok, lreverse(coll));
}

bindrrdo(pl : list of ref pair, coll : list of list of string, flags : int) : int
{
  ok := 0;
  if (len coll != len pl)
    sys->fprint(stderr, Mod+": bindrrch: input set length (%d) mismatch task (%d)\n", len coll, len pl);
  for (; pl != nil && coll != nil; (pl, coll) = (tl pl, tl coll)) {
    src := (hd pl).s;
    dest := (hd pl).d;
    col := hd coll;
    er := bindrl(src, dest, flags, col);
    if (er < 0) ok = er;
  }
  return ok;
}

unmountrrdo(pl : list of ref pair , coll : list of list of string, flags, srcp : int) : int
{
  ok := 0;
  if (len coll != len pl)
    sys->fprint(stderr, Mod+": unmountrr: input set length (%d) mismatch task (%d)\n", len coll, len pl);
  col : list of string;
  for (; pl != nil && coll != nil; (pl, coll) = (tl pl, tl coll)) {
    src := "";
    if (srcp)
      src = (hd pl).s;
    dest := (hd pl).d;
    col := hd coll;
    er := unmountrl(src, dest, flags, col);
    if (er < 0) ok = er;
  }
  return ok;
}

# read/write coll utilities

readllz(in : chan of string, fio : string) : list of list of string
{
  return ltocoll(readlz(in, fio));
}

revreadllz(in : chan of string, fio : string) : list of list of string
{
  return revltocoll(readlz(in, fio));
}

sp_printllz(coll : list of list of string, out : chan of string, fio : string)
{
  sp_printlz(colltol(coll), out, fio);
}

eoc : con " ";

ltocoll(l : list of string) : list of list of string
{
  return lreverse(revltocoll(l));
}

revltocoll(l : list of string) : list of list of string
{
  coll : list of list of string;
  sl : list of string;
  for (; l != nil; l = tl l) {
    line := hd l;
    if (line == eoc) {
      coll = reverse(sl) :: coll;
      sl = nil;
    }
    else
      sl = line :: sl;
  }
  return coll;
}

colltol(coll : list of list of string) : list of string
{
  l : list of string;
  for (; coll != nil; coll = tl coll) {
    col := hd coll;
    for (; col != nil; col = tl col)
      l = hd col :: l;
    l = eoc :: l;
  }
  return reverse(l);
}

# one source to one target - recursive version

ziotestbindr(argv : list of string, flags : int)
{
  sys->print(Mod+": bindrch...\n");
  ioch := chan of string;
  if(bindrch(hd argv, hd tl argv, flags, ioch) < 0)
    sys->fprint(stderr, Mod+" bindrch: %r\n");
  sys->print(Mod+": unmountrch...\n");
  if (unmountrch(hd argv, hd tl argv, flags, ioch) < 0)
    sys->fprint(stderr, Mod+" unmountrch: %r\n");
  sys->print(Mod+": bind/unmountrch done\n");
}

bindr(src, dest : string, flags : int) : int
{
  (ok, nil) := bindrcol(src, dest, flags);
  return ok;
}

bindrch(src, dest : string, flags : int, out : chan of string) : int
{
  (ok, col) := bindrcol(src, dest, flags);
  sp_printlz(col, out, nil);
  return ok;
}

bindrcol(src, dest : string, flags : int) : (int, list of string)
{
  if(!ldrd() || !flagsok(flags))
    return (-1, nil);

  col : list of string;
  if (flags & MSRCR) {
    flags &= ~MSRCR;
    col = relbindgatherer(dest, src, flags & MFILE);
  }
  else
    col = relbindgatherer(src, dest, flags & MFILE);
  return (bindrl(src, dest, flags, col), col);
}

bindrl(src, dest : string, flags : int, l : list of string) : int
{
  ok := 1;
  union := flags & MUNION;
  wnion := flags & MWNION;
  if (union || wnion) {
    flags &= ~MUNION & ~MWNION;
    
    crafter := flags & MCRAFT;
    if (crafter)
      flags &= ~MCRAFT;

    aflags := (flags & ~sys->MBEFORE) | sys->MAFTER;
    if (crafter)
      aflags |= sys->MCREATE;
    else
      aflags &= ~sys->MCREATE;

    ok = bindrlgrp(dest, src, aflags, l);

    if (wnion)
      ok &= bindrlgrp(src, dest, flags, l);
    else
      ok &= bindrlgrp(src, dest, flags, nil);
  }
  else
    ok = bindrlgrp(src, dest, flags, l);

  if (!ok)
    return -1;
  return ok;
}

bindrlgrp(src, dest : string, flags : int, l : list of string) : int
{
  ok := 1;
  typ : string;
  file := flags & MFILE;
  if (file)
    flags &= ~MFILE;
  if (flags & MTEST) {
    flags &= ~MTEST;
    typ = typetest(flags);
  }
  for(;l != nil; l = tl l) {
    nsrc := src+"/"+hd l;
    ndest := dest+"/"+hd l;
    ok &= tbind(typ, nsrc, ndest, flags);
  }
  if (!file) ok &= tbind(typ, src, dest, flags);
  return ok;
}

unmountr(src, dest : string, flags : int) : int
{
  if(!ldrd())
    return -1;
  col : list of string;
  if (flags & MSRCR) {
    flags &= ~MSRCR;
    col = relbindgatherer(dest, src, flags & MFILE);
  }
  else
    col = relbindgatherer(src, dest, flags & MFILE);
  return unmountrl(src, dest, flags, col);
}

unmountrch(src, dest : string, flags : int, in : chan of string) : int
{
  if(!ldrd())
    return -1;
  col := readlz(in, nil);
  return unmountrl(src, dest, flags, col);
}

# file io and channel read/write work list utilities

fiop(fio : string) : int
{
  return (fio != nil && fio[0] == '-');
}

namefio(src, fio : string) : string
{
  if (fio == nil || fio == "-")
    return nil;
  else {
    if (fio[0] == '-') fio = fio[1:];
    if (src != nil && fio[0] != '/') fio = src+"/"+fio;
  }
  return fio;
}

createfio(fio : string) : ref Sys->FD
{
  fd := sys->create(fio, sys->ORDWR, 8r666);
  if (fd == nil)
    sys->fprint(stderr, Mod+": createfio %s %r\n", fio);
  return fd;
}

openfio(fio : string, flag : int) : ref Sys->FD
{
  if (fio == nil) {
    if (flag == sys->OREAD)
      return stdin;
    else
      return stdout;
  }
  return sys->open(fio, flag);
}

readlfio(fio : string) : list of string
{
  delim := "\n";
  if ((fd := openfio(fio, sys->OREAD)) == nil)
    sys->fprint(stderr, Mod+": openfio %s %r\n", fio);
  else
    return readl(fd, delim);
  return nil;
}

readlz(in : chan of string, fio : string) : list of string
{
  if (fio != nil)
    return readlfio(fio);
  else if (in == nil)
    return readlfio(nil);
  else
    return readlch(in);
}

readlch(in : chan of string) : list of string
{
  delim := "\n";
  sbuf : string;
  while((s := <- in) != nil && s[0] != delim[0])
    sbuf += s;
  (nil, l) := sys->tokenize(sbuf, delim);
  return l;
}

sp_printlz(l : list of string, out : chan of string, fio : string)
{
  if (fio != nil)
    printlfio(l, fio);
  else if (out == nil)
    printlfio(l, nil);
  else
    spawn printlch(l, out);
}

printlfio(l : list of string, fio : string)
{
  if ((fd := openfio(fio, sys->OWRITE)) == nil) {
    sys->fprint(stderr, Mod+": printlfio %s %r\n", fio);
    return;
  }
  for(; l != nil; l = tl l)
    sys->fprint(fd, "%s\n", hd l);
  sys->fprint(fd, "\n");
}

printlch(l : list of string, out : chan of string)
{
  for(; l != nil; l = tl l)
    out <- = hd l +"\n";
  out <- = "\n";
}

# test option utilities

tbind(typ, src, dest : string, flags : int) : int
{
  if (typ != nil)
    sys->fprint(stderr, "bind %s %s %s\n", typ, src, dest);
  else {
    ok := !(sys->bind(src, dest, flags) < 0);
    if (!ok)
      sys->fprint(stderr, "bind %s %s %s: %r\n", typ, src, dest);
    return ok;
  }
  return 1;
}

typetest(flags : int) : string
{
  typ := "-";
  if (flags & sys->MCREATE)
    typ += "c";
  if (flags & sys->MAFTER)
    typ += "a";
  else if (flags & sys->MBEFORE)
    typ += "b";
  else
    typ += "r";
  return typ;
}

# from util/listio

L : con 1024;
readl(fd : ref Sys->FD, delim : string) : list of string
{
  buf := array[L] of byte;
  rlb, lb : list of array of byte;
  while ((n := sys->read(fd, buf, L)) > 0)
    rlb = ((array [n] of byte)[0:] = buf[0:n]) :: rlb;

  size := 0;
  for (lb = nil; rlb != nil; rlb = tl rlb) {
    lb = hd rlb :: lb;
    size += len hd lb;
  }
  
  if (!size) return nil;

  if (tl lb == nil)
    buf = hd lb;
  else {
    buf = array[size] of byte;
    for (bp := 0; lb != nil; lb = tl lb) {
      buf[bp:] = hd lb;
      bp += len hd lb;
    }
  }

  (nil, l) := sys->tokenize(string buf, delim);
  return l;
}

unmountrl(src, dest : string, flags : int, l : list of string) : int
{
  test := flags & MTEST;
  ok := 1;
  l = reverse(l);
  union := flags & MUNION;
  wnion := flags & MWNION;
  if (union || wnion) {
    if (wnion)
      ok = tunmountrgrp(test, src, dest, l, flags);
    else
      ok = tunmount(test, src, dest);
    ok &= tunmountrgrp(test, dest, src, l, flags);
  }
  else
    ok = tunmountrgrp(test, src, dest, l, flags);
  if(!ok)
    return -1;
  return ok;
}

tunmountrgrp(test : int, src, dest : string, l : list of string, flags : int) : int
{
  ok := 1;
  file := flags & MFILE;
  if (!file) ok &= tunmount(test, src, dest);
  for(;l != nil; l = tl l) {
    nsrc, ndest : string;
    if (src != nil)
      nsrc = src+"/"+hd l;
    if (dest != nil)
      ndest = dest+"/"+hd l;
    ok &= tunmount(test, nsrc, ndest);
  }
  return ok;
}
  
tunmount(test : int, src, dest : string) : int
{
  if (test)
    if (src != nil)
      sys->fprint(stderr, "unmount %s %s\n", src, dest);
    else
      sys->fprint(stderr, "unmount %s\n", dest);
  else {
    if (dest == nil) {dest = src; src = nil;}
    ok := !(sys->unmount(src, dest) < 0);
    if (!ok)
      sys->fprint(stderr, "unmount %s %s: %r\n", src, dest);
    return ok;
  }
  return 1;
}

relbindgatherer(src, dest : string, file : int) : list of string
{
  if (src == dest)
    src = nil;
  (dirs, col) := matchdirlevel(src, dest, nil, 0, file);
  home := pwd();
  if(sys->chdir(dest) < 0)
    sys->fprint(stderr, "Error["+Mod+"]: cd: %s: %r\n", dest);
  col = bindgatherer(src, dirs, col, file);
  if(sys->chdir(home) < 0)
    sys->fprint(stderr, "Error["+Mod+"]: cd: %s: %r\n", home);
  return col;
}

bindgatherer(src : string, dirs, col : list of string, file : int) : list of string
{
  next : list of string;
  for (; dirs != nil; dirs = tl dirs) {
    (next, col) = matchdirlevel(src, hd dirs, col, 1, file);
    if (next != nil)
      col = bindgatherer(src, next, col, file);
  }
  return col;
}

dirp(sdir : string) : int
{
  (ok, dir) := sys->stat(sdir);
  return ok >= 0 && (dir.mode & sys->CHDIR);
}

include "workdir.m";
pwd() : string
{
  gwd := load Workdir Workdir->PATH;
  if (gwd == nil) {
    sys->fprint(stderr, "Error:"+Mod+"%s %r\n", Workdir->PATH);
    return nil;
  }
  wd := gwd->init();
  if(wd == nil) {
    sys->fprint(stderr, "Error:"+Mod+": pwd: %r\n");
    return nil;
  }
  return wd;
}

matchdirlevel(root, path : string, col : list of string, fnp, file : int) : (list of string, list of string)
{
  if (file)
    return matchfiles(root, path, col, fnp);
  else
    return matchsubdirs(root, path, col, fnp);
}

# Excepted file name -- generated by: find . -type tar
NOP : con "_";

matchfiles(root, path : string, col : list of string, fnp : int) : (list of string, list of string)
{
  dirs : list of string;
  if (path != nil) {
    (ad, nd) := rd->init(path, Readdir->NONE|Readdir->COMPACT);
    if (nd <= 0)
      return (dirs, col);
    for (i := 0; i < nd; i++) {
      apath := ad[i].name;
      if (fnp)
	apath = path+"/"+apath;
      if (ad[i].mode & sys->CHDIR) {
        if (root == nil || dirp(root+"/"+apath))
	  dirs = apath :: dirs;
      }
      else if (ad[i].name != NOP)
	col = apath :: col;
    }
    return (dirs, col);
  }
  return (dirs, col);
}

matchsubdirs(root, path : string, col : list of string, fnp : int) : (list of string, list of string)
{
  dirs : list of string;
  if (path != nil) {
    (ad, nd) := rd->init(path, Readdir->NONE|Readdir->COMPACT|Readdir->DIR);
    if (nd <= 0)
      return (dirs, col);
    for (i := 0; i < nd; i++) {
      dpath := ad[i].name;
      if (fnp)
	dpath = path+"/"+dpath;
      if (root == nil || dirp(root+"/"+dpath)) {
	dirs = dpath :: dirs;
	col = dpath :: col;
      }
    }
    return (dirs, col);
  }
  return (dirs, col);
}

subdirs(path : string, col : list of string, fnp : int) : (list of string, list of string)
{
  return matchsubdirs(nil, path, col, fnp);
}

# from util/list

reverse(l : list of string) : list of string
{
  r : list of string;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

lreverse(l : list of list of string) : list of list of string
{
  r : list of list of string;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

slicelast(l : list of string) : (list of string, list of string)
{
  if (l == nil)
    return (l, l);
  k, p : list of string;
  for(i := 0; l != nil; l = tl l) {
    p = l; k = hd l :: k;
  }
  return (reverse(tl k), p);
}

pair : adt
{
  s : string;
  d : string;
  print : fn(p : self ref pair);
  switch : fn(p : self ref pair);
};

pair.print(p : self ref pair)
{
  sys->print("(%s, %s)", p.s, p.d);
}

pair.switch(p : self ref pair)
{
  (p.s, p.d) = (p.d, p.s);
}

pairlast(sd : list of string) : list of ref pair
{
  return preverse(revpairlast(sd));
}

revpairlast(sd : list of string) : list of ref pair
{
  (sl, dl) := slicelast(sd);
  if (dl == nil)
    return nil;
  r : list of ref pair;
  for (; sl != nil; sl = tl sl) 
    r = ref pair(hd sl, hd dl) :: r;
  return r;
}

pairwise(nl : list of string) : list of ref pair
{
  return preverse(revpairwise(nl));
}

revpairwise(nl : list of string) : list of ref pair
{
  if (nl == nil || tl nl == nil)
    return nil;
  r : list of ref pair;
  for (; tl nl != nil; nl = tl nl)
    r = ref pair(hd nl, hd tl nl) :: r;
  return r;
}

# switch order in pairs
pairunwise(nl : list of string) : list of ref pair
{
  return preverse(revpairunwise(nl));
}

revpairunwise(nl : list of string) : list of ref pair
{
  if (nl == nil || tl nl == nil)
    return nil;
  r : list of ref pair;
  for (; tl nl != nil; nl = tl nl)
    r = ref pair(hd tl nl, hd nl) :: r;
  return r;
}

preverse(l : list of ref pair) : list of ref pair
{
  r : list of ref pair;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}
