# qid comparison -- compare files for uniqueness of qid, dev, dtype
implement Qidcmp;

Mod : con "qidcmp";

include "sys.m";
include "draw.m";
include "sh.m";

sys: Sys;
stderr : ref Sys->FD;

include "qidcmp.m";

usage()
{
  sys->fprint(stderr, "Usage:\t"+Mod+" file [more-files]\n");
}

init(ctxt: ref Draw->Context, args: list of string)
{
  sys = load Sys Sys->PATH;
  stderr = sys->fildes(2);
  if (args == nil) return;
  args = tl args;
  if (args == nil) {usage(); return;}

  cdir := ref Cdir(nil, 0);
  for (; args != nil; args = tl args) {
    file := hd args;
    old := "";
    if (cdir.dir != nil)
      old = cdir.dir.name;
    sys->print("cmp(%s, %s) = %d\n", old, file, cdir.fcmp(file));
  }
}

Cdir.fcmp(c : self ref Cdir, path : string) : int
{
  (ok, dir) := sys->stat(path);
  if (ok < 0) {
    sys->print(Mod+": %s %r\n", path);
    c.m = SAME;
  }
  return c.cmp(ref dir);
}

Cdir.cmp(c : self ref Cdir, dir : ref Sys->Dir) : int
{
  if (c.dir == nil && dir != nil) {
    c.dir = dir;
    return c.m = INIT;
  }

  if (dir == nil)
    return c.m = SAME;

  diff := SAME;
  if (dir.dev != c.dir.dev ||
      dir.dtype != c.dir.dtype ||
      dir.qid.path != c.dir.qid.path ||
      dir.qid.vers != c.dir.qid.vers)
    diff = DIFF;

  if (diff == SAME)
    if (dir.mtime > c.dir.mtime)
      diff = NEW;
    else if (- (dir.mtime < c.dir.mtime))
      diff = OLD;

  if (diff != SAME)
    c.dir = dir;

  return c.m = diff;
}
