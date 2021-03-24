Rtoken : module
{
  PATH : con "/dis/lib/rtoken.dis";

  # generate rtoken id -- invoke once for each fd.
  # return a unique ref Id to be stored in variable used in readtoken calls.
  id : fn () : ref Id;

  # read next token delimited (delim) from fd (assume utf format)
  # -- no seek on fd while readtoken used
  readtoken : fn(fd : ref Sys->FD, delim : string, id : ref Id) : string;

  # include (default) or exclude a delimiter character (at end of token string)
  NOEOT, WITHEOT : con iota;

  Id : adt
  {
    l : list of string;
    buf : array of byte;
    BLEN : int;
    eot : int;
    n : int;		# n < 0 when error occured while reading fd

    # set buffer size used to read fd chunks
    setbuflen : fn (id : self ref Id, blen : int);

    # set end of token mode (include/exclude)
    seteot : fn(id : self ref Id, eot : int);
  };

  # test readtoken
  init : fn(ctxt : ref Draw->Context, args : list of string);
};
