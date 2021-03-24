implement Charon_code;

include "common.m";
include "rgb.b";
include "ycbcr.b";
include "charon_code.m";
include "gb2u.m";
	gb2u:GB2U;

sys: Sys;
CU: CharonUtils;
        Header, ByteSource, MaskedImage, ImageCache, ResourceState: import
CU;
D: Draw;
        Point, Rect, Image, Display: import D;
E: Events;
        Event: import E;
G: Gui;


init()
{
	
	sys=load Sys Sys->PATH;
	if(sys==nil)
        {
        sys->print("sys not loaded:%r\n");
        exit;
        }
	D=load Draw Draw->PATH;
	gb2u=load GB2U GB2U->PATH;
	if(gb2u==nil)
        {
        sys->print("gb2u not loaded:%r\n");
        exit;
        }

}

remeasure_font(str:string,fnt:int):int
{
	nonormal_char:=0;
	for(i:=0;i<len str;i++)
		{
		c:=int str[i];
		if(c>=16r4e00)
		{
		nonormal_char=1;
		break;
		}
		}
	if (nonormal_char)
		return 2;
	else return fnt;


}

convCode(buf:array of byte,index:int):(int,int)
{
c:=int buf[index];
if(c>16r80)
        {
                index++;
                secondbyte:=int buf[index];
                unicodechar:=gb2u->Gb2Uni(c*256+secondbyte);
                #sys->print("unicodechar:%x\n",unicodechar);
                index++;
                return (unicodechar,index);
        }
else 
	return (-1,index);

}

getRightcode(s:string):string
{
	for(i:=0;i<len s;i++)	
	{
		c:=s[i];
		if(c>=16r4e00)
			s[i]=gb2u->u2gb(c);
	}
	return s;
}
