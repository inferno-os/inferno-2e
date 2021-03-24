#Converter: module
Mailtool_code: module

{

  PATH: con "/dis/mailtool/mailtool_code.dis";
  flag:int;  

   init:fn();
#  convert from one buf to buf of UTF byte
   cbuf_ubuf:fn(cbuf:array of byte, n:int): array of byte;
   Cb2Uni: fn(g:int):int;
   u2c: fn(uniWord:int):int;
   convertUTFtoGB:fn(lg:int,cmd:array of byte):(int,array of byte);

};
