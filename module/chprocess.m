ChProcess: module
{
  init : fn(ctxt: ref Draw->Context, argv: list of string);
  process : fn(buf : array of byte) : array of byte;
};
