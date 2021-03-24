#implement Converter;
implement Mailtool_code;


include "sys.m";
	sys: Sys;
	char2byte : import sys;

include "draw.m";

include "mailtool_code.m";

#include "Gb2U.txt";
include "gb2u.m";
   gb2u:GB2U;

init()
{

 sys=load Sys Sys->PATH;

 if (sys==nil)
       exit;

  gb2u=load GB2U GB2U->PATH;

  if(gb2u==nil)
  {
    sys->print("GB2U not loaded:\n");
    exit;
  }
}

cbuf_ubuf( cbuf : array of byte, n :int): array of byte
{
 sys = load Sys  Sys->PATH;
 
 ubuf := array[8192] of byte;
 j:=0;

 for(i:=0; i<n; ){
	if ( ((int cbuf[i])& 16r80)==0 ) 
			{ ubuf[j] = cbuf[i]; i++; j++;}
	else{
 		s1 := int string cbuf[i];
		s2 := int string cbuf[i+1];
		g :=s1*16r100+s2;
		u := Cb2Uni( g); 
		l :=sys->char2byte(u,ubuf,j);
     		i=i+2; j=j+l;
		}
    }

return( ubuf[0:j]);

}


Cb2Uni( g : int ): int
{

return gb2u->Gb2Uni(g);

}

u2c(uniWord:int):int
{

return gb2u->u2gb(uniWord);

}

convertUTFtoGB(lg:int,cmd:array of byte):(int,array of byte)
{ 
  x1,x2,x3:int;
  ncmd:= array [lg] of byte; 
  nindex:=0;
  cWord:=array[2] of byte;
  mid:=0;

  index:=0;
  x2=1;
  while(index<lg)
  {  (x1,x2,x3)=sys->byte2char(cmd, index);  
     if(x3!=0) {
        		if(x1>=16r4e00 && x1<=16r9fff)
         		{   mid=gb2u->u2gb(x1);
                        nindex++;
			ncmd[nindex]=byte mid;
                            mid>>=8;
		      	nindex--;
			      ncmd[nindex]=byte mid;
      			nindex++;
                        nindex++;
                        index+=x2;
	            }    
                  else 
			for (i:=0; i<x2; i++) {
				ncmd[nindex]=cmd[index];
                        nindex++; index++;
                       }
               }
      else 
           {ncmd[nindex]=cmd[index];
            nindex++; index++;
           }  
      
  } 

 return(nindex,ncmd);
}

