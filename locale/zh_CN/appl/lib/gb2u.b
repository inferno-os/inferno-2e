implement GB2U;

include "sys.m";
	sys: Sys;
	char2byte : import sys;

include "draw.m";

include "gb2u.m";
include "Gb2U.txt";


gbuf_ubuf( gbuf : array of byte, n :int): array of byte
{
 sys = load Sys  Sys->PATH;
 
 ubuf := array[8192] of byte;
 j:=0;

 for(i:=0; i<n; ){
	if ( ((int gbuf[i])& 16r80)==0 ) 
			{ sys->print("not chinese\n");
			ubuf[j] = gbuf[i]; i++; j++;}
	else{
 		s1 := int string gbuf[i];
		s2 := int string gbuf[i+1];
		g :=s1*16r100+s2;
		u := Gb2Uni( g); 
		sys->print("gb code:%x%x\n",s1,s2);
		l :=sys->char2byte(u,ubuf,j);
		for(k:=0;k<l;k++)
		sys->print("unicode :%x",int ubuf[j+k]);
		sys->print("\n");
     		i=i+2; j=j+l;
		}
    }

sys->print("unicode:%s\n",string ubuf);
return( ubuf[0:j]);

}


Gb2Uni( g : int ): int
{
	sys=load Sys Sys->PATH;
 low := g & 16rff;
 hig := (g & 16rff00)>>8;
	if ((low >= 16r40) && (low <= 16rfe) &&
		(hig >= 16r81) && (hig <= 16rfe) ) 
	{
		ord := (hig - 16r81) * 191 + (low - 16r40);

	return(GUbuf[ord]);
	}
	#sys->print("**"); # no match
	return(16r4e00);
}

#init(ctxt: ref Draw->Context,argv:list of string)
#{
#	sys=load Sys Sys->PATH;
#	fd:=sys->create("u2gb.txt",sys->ORDWR,8r700);
#	if(fd==nil)
#		sys->print("u2gb.txt:%r\n");
#	tmp_buf:=array[2] of byte;
#
#	for(i:=0;i<len GUbuf;i++)
#	{
#	uni:=GUbuf[i];
#	tmp_buf[1]=byte (uni& 16rff); #low	
#	tmp_buf[0]=byte ((uni & 16rff00)>>8); #high
#	sys->write(fd,tmp_buf,2);
#	
#	high:=i/191+129;	
#	low:=i%191 +64;
#	tmp_buf[0]=byte high;
#	tmp_buf[1]=byte low;
#	sys->write(fd,tmp_buf,2);
#
#
#	}
#
#}

u2gb(uniWord:int):int
{
	for(i:=0;i<len GUbuf;i++)
		if(uniWord==GUbuf[i]) break;	
	if(i==len GUbuf) return 0;
	high:=i/191+129;
	low:=i%191+64;
	return(high*256+low);
}
