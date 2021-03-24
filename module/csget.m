CsGet : module
{
  PATH : con "/dis/lib/csget.dis";
  hostinfo : fn(name : string) : (string, string, list of string);
  init : fn(context : ref Draw->Context, argv : list of string);
};
