# find using limbo regexp - namespace aware
# -- to use with puttar: find . -type tar
# -- to generate mkfs proto file: find . -type proto[+]
# obc
implement Find;

Mod : con "find";

include "sys.m";
include "draw.m";

FD: import Sys;
Context: import Draw;

include "readdir.m";
rd : Readdir;

include "regex.m";
	regex: Regex;
	Re : import regex;

include "sh.m";

include "promptstring.b";

Find: module
{
        init :	fn(ctxt: ref Draw->Context, argv : list of string);
};

sys: Sys;
stdin, stdout, stderr : ref FD;

Used := 0;
usage(opt : string)
{
  if (opt != nil)
    sys->fprint(stderr, Mod+":\tunexpected: %s\n", opt);
  if (!Used)
    sys->fprint(stderr, "Usage:\t"+Mod+" path-list [option-list]\nOption:\t-type [f|d]\t-- list file or dir exclusively\n\t-type tar\t-- list for puttar (add "+NOP+" file to empty dirs)\n\t-type proto[+]\t-- format list for mkfs input\n\t-name exp\t-- match name to regular expression\n\t-path exp\t-- match full pathname to regular expression\n\t-[cma]time +-n\t-- match mtime or atime >, <, or = to n days ago\n\t-newer file\t-- match newer than file date\n\t-size [+-]n[c]\t-- match size >, <, = to n blocks or n[c] bytes\n\t-a -o ( )\t-- define boolean combination of rules\n\t! expr\t\t-- define rule negation\n\t-print -prune\t-- supported rule actions\n\t-[exec|ok] cmd\t-- execute cmd ..{}.. ';' [-ok : interactive]\n\t-depth -follow\t-- ignored rule actions\n");
  Used = 1;
}

init(ctxt : ref Context, argv : list of string)
{
  if (!ldrd())
    return;
  Used = 0;
  if (argv == nil)	# init only
    return;
  (files, opts) := splitopts(tl argv);

  if (files == nil) {
    usage(nil);
    return;
  }

  tflag := DIR | FILE;
  rule : ref Rule;
  (tflag, rule, opts) = parse(ctxt, tflag, opts);

  if (rule != nil) {
    if (regex == nil) {
      regex = load Regex Regex->PATH;
      if (regex == nil) {
	sys->fprint(stderr, Mod+": load failed: %s %r\n", Regex->PATH);
	return;
      }
    }
    if(!compileR(rule))
      return;
  }

  for (; files != nil; files = tl files)
    find(hd files, tflag, rule);
}

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

# Define rules

# General options
NOTREE, TARLS, DIR : con iota;
FILE : con (1<<2);
PROTO : con (1<<3);
PROTO_PLUS : con (1<<4);

# Rule actions
NONE, PRINT, PRUNE : con iota;
EXEC : con (1<<2);
OK : con (1<<3);
ALL : con NONE | PRINT | PRUNE | EXEC | OK;

# Boolean and all other rule types
NIL, OR, AND, SET, NAME, PATH, ATIME, MTIME, SIZE : con iota;
MORE : con (1<<5);
Rtypes := array[] of {"nil", "or", "and", "set", "name", "path", "atime", "mtime", "size"};

Rule: adt
{
  bool : int;
  not : int;
  act : int;
  num : int;
  pattern : string;
  rexp : Regex->Re;
  next : cyclic list of ref Rule;
  ctxt : ref Draw->Context;
  cmd : Command;
  args : list of string;
  reverse : fn(r : self ref Rule);
  sbool : fn(r : self ref Rule) : string;
  print : fn(r : self ref Rule);
  printif : fn(r : self ref Rule, tst : int);
  recursive : fn(r : self ref Rule) : int;
  boolean : fn(r : self ref Rule) : int;
  actual : fn(r : self ref Rule) : ref Rule;
  match : fn(rule : self ref Rule, f : string, dir : ref Sys->Dir) : int;
  execute : fn(r : self ref Rule, f : string, dir : ref Sys->Dir);
};

newR(bool, not, act, num : int, pattern : string, next : list of ref Rule) : ref Rule
{
  return ref Rule(bool, not, act, num, pattern, nil, next, nil, nil, nil);
}

Rule.reverse(r : self ref Rule)
{
  r.next = reverseR(r.next);
}

Rule.sbool(r : self ref Rule) : string
{
  if (r.bool & ~MORE >= len Rtypes)
    return "error";
  mr : string;
  if (r.bool & MORE) mr="+";
  return Rtypes[r.bool & ~MORE]+mr;
}

Rule.print(r : self ref Rule)
{
  snum : string;
  if (r.num != 0)
    snum = string r.num;
  cmd : string;
  ename : string;
  if (r.args != nil) {
    ename = hd r.args;
    if (tl r.args != nil)
      cmd = hd tl r.args;
  }
  sys->print("rule-%s %d(%s)%s-%s->%d(%s) ", r.sbool(), r.not, r.pattern, ename, snum, r.act, cmd);
}

Rule.printif(r : self ref Rule, tst : int)
{
   if (tst) r.print();
}

Rule.recursive(r : self ref Rule) : int
{
  case r.bool & ~MORE {
    OR or AND or SET => return r.bool;
    * => return NIL;
  }
}

Rule.boolean(r : self ref Rule) : int
{
  if (r.bool & OR || r.bool & AND)
    return r.bool;
  return NONE;
}

Rule.actual(r : self ref Rule) : ref Rule
{
  if (r.next != nil && tl r.next != nil) return hd r.next;
  else return r;
}

Rule.match(r : self ref Rule, f : string, dir : ref Sys->Dir) : int
{
  case r.bool {
    NIL => return 1;
    NAME => {
      if (r.pattern == nil) return 1;
      tname := nsname(f);
      if (tname == nil) tname = dir.name;
      return regex->execute(r.rexp, tname) != nil;
    }
    PATH => return (r.pattern == nil) || (regex->execute(r.rexp, f) != nil);
    MTIME => {
      if (test) printcmp(r, f, dir);
      case r.pattern {
	"+" => return dir.mtime > r.num;
	"-" => return dir.mtime < r.num;
      "%Day=" => return dir.mtime/Day == r.num/Day;
      "=" => return dir.mtime == r.num;
      }
    }
    ATIME =>
      case r.pattern {
      "+" => return dir.atime > r.num;
      "-" => return dir.atime < r.num;
      "=" => return dir.atime == r.num;
    }
    SIZE =>
	case r.pattern {
	  "+" => return dir.length > r.num;
	  "-" => return dir.length < r.num;
	  "=" => return dir.length == r.num;
	}
    * => sys->fprint(stderr, Mod+": cannot match rule of type %d\n", r.bool);
  }
  return 0;
}

printcmp(r : ref Rule, f : string, dir : ref Sys->Dir)
{
  cmp := r.pattern;
  d := 1;
  case r.pattern {
    "+" => cmp = ">";
    "-" => cmp = "<";
    "%Day=" => d = Day;
  }
  sys->print("%s(%d%s%d)\n", f, dir.mtime/d, cmp, r.num/d);
}

Rule.execute(r : self ref Rule, f : string, dir : ref Sys->Dir)
{
  if (r.act == OK) {
    name := r.pattern;
    if (r.args != nil)
      name = hd r.args;
    resp := promptstring("< "+name+" ... "+dir.name+" >? ", nil, RAWOFF);
    if (resp != "y") return;
  }
  args := replace("{}", f, r.args);
  sh := r.cmd;
  if (sh == nil) return;
  if (args != nil) args = tl args;
  sh->init(r.ctxt, args);
}

reverseR(l : list of ref Rule) : list of ref Rule
{
  t : list of ref Rule;
  for(; l != nil; l = tl l)
    t = hd l :: t;
  return t;
}

# Options parser

test := 0;

isopt(arg : string) : int
{
  return arg != nil && (arg[0] == '-' || arg[0] == '!' || arg[0] == '(');
}

splitopts(argv : list of string) : (list of string, list of string)
{
  files : list of string;
  for (args := argv; args != nil; args = tl args) {
    arg := hd args;
    if (isopt(arg))
      return (reverse(files), args);
    files = arg :: files;
  }
  return (argv, nil);
}

parse(ctxt : ref Draw->Context, tflag : int, opts : list of string) : (int, ref Rule, list of string)
{
  rule : ref Rule;
  rules : list of ref Rule;
  not := 0;
  depth := 0;
  subrule : ref Rule;
done:
  for (;opts != nil; opts = tl opts) {
    case hd opts {
      "-?" => test = 1;
      "(" => {
	(tflag, subrule, opts) = parse(ctxt, tflag, tl opts);
	if (subrule != nil) {
	  if (not) {subrule.not = !subrule.not; not = 0;}
	  (rule, rules) = insertrule(subrule, rule, rules);
	}
	if (opts == nil) {usage("missing closing )"); break done;}
      }
      ")" => break done;
      "!" => not = 1;
      "-type" =>
	if (not) {not = 0; usage("! "+hd opts);}
	else if (tl opts != nil) {
	  opts = tl opts;
	  case hd opts {
	    "f" => tflag = FILE;
	    "d" => tflag = DIR;
	    "tar" => tflag = TARLS | FILE;
	    "proto" => tflag |= PROTO;
	    "proto+" => tflag |= PROTO_PLUS;
	    * => usage("-type "+hd opts);
	  }
	}
	else usage(hd opts);
      "-a" or "-o" =>
	if (not) {not = 0; usage("! "+hd opts);}
	else {
	  op : int;
	  case hd opts {
	    "-o" => op = OR;
	    * => op = AND;
	  }
	  rule = extendrule(rule, op);
	}
      "-name" or "-path" =>
	if (tl opts != nil) {
	  op := NAME; if (hd opts == "-path") op = PATH;
	  opts = tl opts;
	  pattern := hd opts;
	  new := newR(op, not, PRINT, 0, pattern, nil);
	  not = 0;
	  (rule, rules) = insertrule(new, rule, rules);
	  if (opts == nil) break done;
	}
	else usage("no regexp after "+hd opts);
      "-newer" or "-mtime" or "-ctime" or "-atime" or "-size" =>
	if (tl opts != nil) {
	  (op, num, key, ropts) := parsenumopts(opts);
	  new := newR(op, not, PRINT, num, key, nil);
	  opts = ropts;
	  not = 0;
	  (rule, rules) = insertrule(new, rule, rules);
	  if (opts == nil) break done;
	}
      else usage("missing n after "+hd opts);
      "-print" or "-prune" or "-exec" or "-ok" => {
	act := NONE;
	cmd : string; args : list of string;
	case hd opts {
	  "-print" => act = PRINT;
	  "-prune" => act = PRUNE;
	  "-exec" => act = EXEC; (cmd, args, opts) = parsecmd(opts);
	  "-ok" => act = OK; (cmd, args, opts) = parsecmd(opts);
	}
	new := newR(NIL, not, act, 0, cmd, nil);
	if (cmd != nil) {
	  new.ctxt = ctxt;
	  (sh, cmdpath) := loadcmd(cmd);
	  new.cmd = sh;
	  new.args = hd args :: cmdpath :: tl args;
	}
	if (rule == nil) rule = new;
	else if (rule != nil) {
	  if (rule == subrule) {
	    if (test) sys->print("subrule act set to %s\n", hd opts);
	    rule.act = act;
	    if (cmd != nil) mvrulecmd(new, rule);
	    rule.printif(test); if (test) sys->print("\n");
	  }
	  else if (rule.boolean() && rule.bool & MORE)
	    (rule, rules) = insertrule(new, rule, rules);
	  else {
	    if (test) sys->print("actual rule set to %s\n", hd opts);
	    # for -type tar option
	    if ((rule.actual().act) & PRUNE && act == PRINT)
	      rule.actual().act |= act;
	    else
	      rule.actual().act = act;
	    if (cmd != nil) mvrulecmd(new, rule);
	  }
	}
	if (opts == nil) break done;
      }
      "-depth" or "-follow" =>
	if (not) {not = 0; usage("! "+hd opts);}
      * => usage("not supported "+hd opts);
    }
  }
  if (opts != nil && hd opts != ")")
    usage("unparsed options (unbalanced paren.): "+hd opts+"...");
  if (rule != nil) {
    if (rule.boolean() & MORE) {
      name : string;
      if (rule.next != nil)
	name = (hd rule.next).pattern;
      usage("missing boolean expr after: "+name);
    }
    rules = rule :: rules;
  }
  if (rules != nil)
    if (tl rules != nil)
      rule = newR(SET, 0, NONE, 0, nil, rules);
    else
      rule = hd rules;
  return (tflag, rule, opts);
}

parsecmd(opts : list of string) : (string, list of string, list of string)
{
  if (opts != nil && tl opts != nil) {
    exec := hd opts;
    opts = tl opts;
    cmd := hd opts;
    args : list of string;
    for(l := opts; l != nil; l = tl l)
      if (cmdend(hd l)) {
	args = reverse(args);
	opts = l;
	return (cmd, args, opts);
      } else args = hd l :: args;
    sys->fprint(stderr, Mod+": %s %s ... : missing command terminator ';'\n", exec, cmd);
    return (cmd, args, opts);
  }
  return (nil, nil, opts);
}

cmdend(arg : string) : int
{
  return arg == ";";
}

mvrulecmd(new, rule : ref Rule)
{
  if (rule.pattern == nil)
    rule.pattern = new.pattern;
  rule.ctxt = rule.ctxt;
  rule.cmd = new.cmd;
  rule.args = new.args;
}

Ddis : con ".dis";
loadcmd(cmd : string) : (Command, string)
{
  if(len cmd < len Ddis || cmd[len cmd -len(Ddis):] != Ddis)
    cmd += ".dis";
  sh := load Command cmd;
  if (sh == nil)
    cmd = "/dis/"+cmd;
  sh = load Command cmd;
  if (sh == nil)
    sys->fprint(stderr, "Error: "+Mod+" load %s %r\n", cmd);
  return (sh, cmd);
}

parsenumopts(opts : list of string) : (int, int, string, list of string)
{
  op := NIL;
  num := 0;
  key : string;
  case (hd opts) {
    "-newer" => op = MTIME; num = getfmtime(hd tl opts); key = "+";
    "-mtime" or "-ctime" or "-atime" => {
      op = MTIME; if (hd opts == "-atime") op = ATIME;
      snum := hd tl opts;
      case snum[0] {
	'+' => key = "-"; num = int snum[1:];
	'-' => key = "+"; num = int snum[1:];
	* => key = "%Day="; num = int snum;
      }
      num = reftime(num);
    }
    "-size" => {
      op = SIZE; snum := hd tl opts;
      block := 512;
      if (snum[len snum -1] == 'c') {snum = snum[0:len snum -1]; block = 1;}
      case snum[0] {
	'+' or '-' => key = snum[0:1]; num = int snum[1:];
	* => key = "="; num = int snum;
      }
      num *= block;
    }
  }
  return (op, num, key, tl opts);
}

include "daytime.m";
dt : Daytime;
Day : con 24*3600;
reftime(n : int) : int
{
  if (dt == nil) {
    dt = load Daytime Daytime->PATH;
    if (dt == nil) {
      sys->fprint(stderr, "Error["+Mod+"]: %s %r\n", Daytime->PATH);
      return 0;
    }
  }
  return dt->now() -(n * Day);
}

getfmtime(file : string) : int
{
  (ok, dir) := sys->stat(file);
  if (ok < 0) {
    sys->fprint(stderr, Mod+": can't stat %s: %r\n", file);
    return 0;
  }
  return dir.mtime;
}

extendrule(rule : ref Rule, op : int) : ref Rule
{
  if (rule != nil) {
    if (pop := rule.boolean()) {
      if (pop & MORE) {usage("no rule before operand"); return rule;}
      else if (pop & op) {
	if (test) sys->print("extend boolean rule: ");
	rule.printif(test);
	rule.bool = pop | MORE;
	rule.printif(test);
	if (test) sys->print("\n");
	return rule;
      }
    }
    rule = newR(op | MORE, 0, NONE, 0, nil, rule :: nil);
    if (test) {sys->print("mk boolean rule: "); rule.print(); sys->print("\n");}
  }
  else usage("no rule before operand");
  return rule;
}

insertrule(new, rule : ref Rule, rules : list of ref Rule) : (ref Rule, list of ref Rule)
{
  if (new == nil)
    return (rule, rules);
  if (rule != nil) {
    if (rule.boolean() & MORE) {
      rule.next = new :: rule.next;
      rule.bool &= ~MORE;
      if (test) {sys->print("insert rule: "); new.print(); sys->print("in rule: "); rule.print(); sys->print("\n");}
      return (rule, rules);
    }
    if (test) sys->print("add previous rule to current rule set\n");
    rules = rule :: rules;
  }
  return (new, rules);
}

compileR(rule : ref Rule) : int
{
  if (rule != nil) {
    r := rule;
    r.reverse();
    if (r.recursive()) {
      r.printif(test); if (test) sys->print("{");
      rules := r.next;
      for (; rules != nil; rules = tl rules)
	if (!compileR(hd rules))
	  return 0;
      if (test) sys->print("}\n");
    }
    else {
      r.printif(test);
      if (r.bool != NAME && r.bool != PATH)
	return 1;
      if (r.pattern == nil) {
	usage("empty pattern");
	return 0;
      }
      rexp := regex->compile(r.pattern, 0);
      if (rexp == nil) {
        sys->fprint(stderr, Mod+": invalid regular expression: %s\n", r.pattern);
        usage("-name "+r.pattern);
        return 0;
      }
      else
        r.rexp = rexp;
    }
  }
  return 1;
}

rexp(f : string, dir : ref Sys->Dir, rule : ref Rule) : int
{
  (match, val) := rexpval(f, dir, rule);
  if (match) return val;
  return NONE;
}

rexpval(f : string, dir : ref Sys->Dir, r : ref Rule) : (int, int)
{
  if (r == nil)
    return (1, PRINT);
  match := 0;
  rp := NONE;
  if (!r.recursive()) {
    r.printif(test);
    match = r.match(f, dir);
    if (r.not) match = !match;
    if (match) {
      case r.act {EXEC or OK => r.execute(f, dir);}
      rp = r.act;
    }
  }
  else {
    bool := r.bool;
    act := r.act;
    rules := r.next;
    nr : ref Rule;
    for (; rules != nil; rules = tl rules) {
      nr = hd rules;
      (mch, nrp) := rexpval(f, dir, nr);
      if (nr.not) mch = !mch;
      if (test) sys->print("mch=%d nrp=%d\n", mch, nrp);
      if (mch) {
	if (bool == OR) {match = 1; rp = nrp; break;}
	if (bool == AND)
	  if (nrp & ALL == NONE) {rp = NONE; break;}
	rp |= nrp;
	match = 1;
      }
      else if (bool == AND) return (0, NONE);
    }
    case act & ALL {
      PRUNE => rp |= PRUNE; if (test) sys->print("ruleset -prune\n");
      PRINT => rp |= PRINT; if (test) sys->print("ruleset -print\n");
    }
  }
  if (test) sys->print("rexpval: return match=%d rp=%d\n", match, rp);
  return (match, rp);
}

find(p : string, flag : int, rule : ref Rule)
{
  if (p == nil)
    return;

  (ok, dir) := sys->stat(p);
  if (ok < 0)
    sys->fprint(stderr, Mod+": can't stat %s: %r\n", p);
  else if (!(dir.mode & sys->CHDIR)) {
    if (flag & FILE && rexp(p, ref dir, rule) & PRINT)
      findprint(p, ref dir, flag, 0);
  }
  else findir(p, ref dir, flag, rule, 0);
}

# dir.name not portable across namespaces
nsname(p : string) : string
{
  np := 0;
  for (i := len p -1; i >= 0; i--)
    if (p[i] == '/' && np)
      return p[i+1:];
    else np = 1;
  return p;
}

findprint(p : string, dir : ref Sys->Dir, flag : int, depth : int)
{
  if (flag & PROTO) {
    for(i := 0; i < depth; i++)
      sys->print("\t");
    sys->print("%s\n", nsname(p));
  }
  else if (flag & PROTO_PLUS) {
    if (dir.mode & sys->CHDIR) {
      for(i := 0; i < depth; i++)
	sys->print("\t");
      sys->print("%s\n", nsname(p));
      for(i = 0; i < depth; i++)
	sys->print("\t");
      sys->print("\t+\n");
    }
  }
  else
    sys->print("%s\n", p);
}

findir (p : string, dir : ref Sys->Dir, flag : int,  rule : ref Rule, depth : int)
{
  if (flag & DIR || flag & TARLS)
    if (rp := rexp(p, dir, rule)) {
      if (rp & PRINT && !(flag & TARLS))
	 findprint(p, dir, flag, depth);
      if (rp & PRUNE) {
	if (test) sys->print("found %s to prune\n", p);
	if (rp & PRINT && flag & TARLS) noemptydir(p);
	return;
      }
    }

  (ad, nd) := rd->init(p, readdirflag(flag));

  if (nd == 0) if (flag & TARLS) noemptydir(p);
  if (nd <= 0) return;

  if (!(flag & FILE) && (flag & TARLS)) noemptydir(p);

  d := p;
  for (i := 0; i < nd; i++) {
    p = d+"/"+ad[i].name;
    if (ad[i].mode & sys->CHDIR)
      findir(p, ad[i], flag, rule, depth+1);
    else if (flag & FILE && rexp(p, ad[i], rule) & PRINT)
      findprint(p, ad[i], flag, depth+1);
  }
}

readdirflag(flag : int) : int
{
  DOF := 0;
  if (flag & DIR && flag & FILE)
    DOF = 0;
  else if (flag & DIR)
    DOF = Readdir->DIR;
  else if (flag & FILE)
    DOF = 0;	# need dir to get to files
  return DOF|Readdir->NONE|Readdir->COMPACT;
}

NOP : con "_";

# Cannot allow empty dirs for tar list option
# use two strategies to resolve issue
noemptydir(p : string)
{
  targ := p+"/"+NOP;
  sys->print("%s\n", targ);
  if (filep(targ))
    return;

  nuld := "/tmp/."+NOP;
  if (!dirp(nuld))
    if (!dcreate(nuld)) {
      sys->fprint(stderr, "Error: "+Mod+" %s %r\n", p);
      return;
    }

  np := nuld+"/"+NOP;
  if (!filep(np))
    if (!fcreate(np)) {
      if (!chmod(nuld, rwxa))
	sys->fprint(stderr, "Error: "+Mod+" cannot chmod %s %r\n", p);
      if (!fcreate(np))
	sys->fprint(stderr, "Error: "+Mod+" cannot create %s %r\n", np);
    }
    else
      sys->fprint(stderr, Mod+": created %s\n", np);

  if (sys->bind(nuld, p, Sys->MAFTER) < 0)
    sys->fprint(stderr, "Error: "+Mod+" bind -a %s %s %r\n", nuld, p);
  else
    sys->fprint(stderr, Mod+": bind -a %s %s\n", nuld, p);
}

rwa : con 8r666;
rwxa : con 8r777;

chmod(f : string, mode : int) : int
{
  (ok, dir) := sys->stat(f);

  if(ok < 0){
    sys->fprint(stderr, "chmod: can't stat %s: %r\n", f);
    return 0;
  }
  mask := rwxa;
  dir.mode = (dir.mode & ~mask) | (mode & mask);
  return !(sys->wstat(f, dir) < 0);
}

filep(f : string) : int
{
  (ok, dir) := sys->stat(f);
  return ok >= 0 && !(dir.mode & sys->CHDIR);
}

dirp(f : string) : int
{
  (ok, dir) := sys->stat(f);
  return ok >= 0 && (dir.mode & sys->CHDIR);
}

dcreate(f : string) : int
{
  return !(sys->create(f, sys->OREAD, sys->CHDIR + rwa) == nil);
}

fcreate(f : string) : int
{
  return !(sys->create(f, sys->ORDWR, rwa) == nil);
}

# notimpl := array[] of {"-atime", "-cpio", "-ctime", "-exec", "-fstype", "-group", "-inum", "-links", "-mtime", "-ncpio", "-newer", "-ok", "-perm", "-size", "-user"};

reverse(l: list of string) : list of string
{
  t : list of string;
  for(; l != nil; l = tl l)
    t = hd l :: t;
  return t;
}

replace(e, r : string, l: list of string) : list of string
{
  t : list of string;
  for(; l != nil; l = tl l)
    if (e == hd l)
      t = r :: t;
    else
      t = hd l :: t;
  return reverse(t);
}
