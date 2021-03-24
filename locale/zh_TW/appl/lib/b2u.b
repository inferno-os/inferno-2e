implement B2U;

include "b2u.m";
#include "b2u_lb.tbl";
include "sys.m";
	sys: Sys;
	char2byte : import sys; 

include "draw.m";
	draw: Draw;

#---------------------------------------------------------------------------
#  Translate Big 5 code array to Unicode array.
#  Input:  bbuf: Big 5 code array.
#          n: size of input array.  
#  Return: Unicode array
#---------------------------------------------------------------------------
bigbuf_ubuf( bbuf : array of byte, n :int): array of byte 
{ 
 ubuf_len: int;

 sys = load Sys  Sys->PATH; 
 
 #ubuf := array[8192] of byte; 
 if(n>16384) n=16384;

if (n<8192)
 ubuf_len=n*2;
else
 ubuf_len=24576;
 ubuf:=array[ubuf_len] of byte;
 j:=0; 

 for(i:=0; i<n; ){ 
		if ( ((int bbuf[i])& 16r80)==0 )  
			{ ubuf[j] = bbuf[i]; i++; j++; } 

		else{ 
 			s1 := int string bbuf[i]; 
			s2 := int string bbuf[i+1]; 
			
			g :=s1*16r100+s2; 
			u := Big5ToUnic( g);  
			l :=sys->char2byte(u,ubuf,j); 

      		i=i+2; j=j+l; 
			} 
   } 
return( ubuf[0:j]); 

} 

#---------------------------------------------------------------------------
#  Translate Big 5 code text to Unicode text.
#  Input:  bBig5Text: Big 5 code text.
#          iNum: size of input array.  
#  Return: Unicode array
#---------------------------------------------------------------------------
Big5ToUnicText ( bBig5Text: array of byte, iNum: int ): (int, array of int)
{
   iUnicText := array [ iNum ] of int;
   i, j, iBig5, iUnic : int;

   i = 0;
   j = 0;
   while ( i < iNum )
   {
       if ( bBig5Text [i] >= byte (16r80) )
       {
          iBig5 = int (bBig5Text [i]);
          iBig5 <<= 8;
          iBig5 += int (bBig5Text [i + 1]);
          iUnicText [j] = Big5ToUnic ( iBig5 );
          i += 2;
          ++j;
       }
       else
       {
          if ( bBig5Text [i] != byte (16r0d) )
          {
             iUnicText [j] = int (bBig5Text [i]);
             ++j;
          }
          ++i;
       }
   }

   return (j, iUnicText);
}


#---------------------------------------------------------------------------
#  Translate Big 5 code to Unicode.
#  Input:  iBig5Code = Big 5 code.
#  Return: -1     => error.
#           0     => No corresponding Unicode exist.
#          others => Unicode.
#---------------------------------------------------------------------------
Big5ToUnic ( iBig5Code : int ): int
{
   bHigh: byte;
   bLow : byte;

   sys=load Sys Sys->PATH;
   b2utbl:=sys->open(B2UTAB,sys->OREAD);
   if(b2utbl==nil)sys->print("error open b2u.tab\n");

   bHigh = byte (iBig5Code >> 8);
   bLow  = byte (iBig5Code);

   if ( bHigh < byte (16r81) )  return -1;
   if ( bHigh > byte (16rFE) )  return -1;
   bHigh -= byte (16r81);

   if ( (bLow >= byte (16r40)) && (bLow <= byte (16r7E)) )
      bLow -= byte (16r40);
   else if ( (bLow >= byte (16rA1)) && (bLow <= byte (16rFE)) )
           bLow = bLow - byte (16rA1) + byte (63);
        else
           return -1;

   wordByte:=array[2] of byte;
   offset:=int (bHigh)*157*2+ int (bLow)*2;
   sys->seek(b2utbl,offset,sys->SEEKSTART);
   sys->read(b2utbl,wordByte,2);
   low:=int wordByte[0];
   high:=int wordByte[1]<<8;
	#sys->print("low:%d,high:%d,num:%d\n",low,high,high+low);
   return (high+low);
   #return  B2UTbl [ int (bHigh) * 157 + int (bLow) ];
}
