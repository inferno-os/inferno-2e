#implement Converter;
implement Mailtool_code;


include "mailtool_code.m";


init()
{


}

cbuf_ubuf( cbuf : array of byte, n :int): array of byte
{

return( cbuf[0:n]);

}


Cb2Uni( g : int ): int
{

return g;

}

u2c(uniWord:int):int
{

return uniWord;

}

convertUTFtoGB(lg:int,cmd:array of byte):(int,array of byte)
{ 

 return(lg,cmd);
}

