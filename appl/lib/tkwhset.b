# update an array tk cmd to configure a screen using tkargs -w and -h
# obc
implement Tkwh;

include "sys.m";
sys : Sys;

include "tkwhset.m";

tkwhset(tkc : array of string, head, tkargs : string) : array of string
{
  sys = load Sys Sys->PATH;
  for (i := 0; i < len tkc; i ++)
    if (len tkc[i] > len head && (tkc[i])[0:len head] == head)
      break;
#  sys->print("i=%d\n", i);
  if (i < len tkc) {
    (n, l) := sys->tokenize(tkc[i], " \t");
    (m, t) := sys->tokenize(tkargs, " \t");
    w := getl("-w", t);
    h := getl("-h", t);
    w = convc(w);
    h = convc(h);
    if (w != nil) l = setl("-width", w, l);
    if (h != nil) l = setl("-height", h, l);
    tkc[i] = concat(l);
  }
#  sys->print("tkc[%d]=%s\n", i, tkc[i]);
  return tkc;
}

getl(e : string, l : list of string) : string
{
  for (; l != nil; l = tl l)
    if (hd l == e && tl l != nil) return hd tl l;
  return nil;
}

setl(e, v : string, l : list of string) : list of string
{
  r : list of string;
  for (; l != nil; l = tl l)
    if (hd l == e) {
      if (tl l != nil)
	return rappend(r, e :: v :: tl tl l);
      else
	return rappend(r, e :: v :: nil);
    }
    else
      r = hd l :: r;
  return nil;
}

rappend(r, l : list of string) : list of string
{
  for (; r != nil; r = tl r)
    l = hd r :: l;
  return l;
}

concat(l : list of string) : string
{
  if (l == nil) return nil;
  r := hd l;
  while ((l = tl l) != nil)
    r += " " + hd l;
  return r;
}

# pixel to character conversion
magic : con 41;
convc(x : string) : string
{
  if (x == nil) return x;
  case x[len x -1] {
    'c' or 'w' or 'h' => return x;
  }
  i := int x;
  return string (i/magic) + "c";
}
