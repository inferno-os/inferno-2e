implement Charon_code;

include "common.m";
include "rgb.b";
include "ycbcr.b";
include "charon_code.m";
include "b2u.m";
	b2u:B2U;

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
	b2u=load B2U B2U->PATH;
	if(b2u==nil)
        {
        sys->print("b2u not loaded:%r\n");
        exit;
        }

}



#drawtext(defaultFnt:ref Draw->Font,dst: ref Image,p:Draw->Point,src: ref Draw->Image,sp:Draw->Point,font : ref Draw->Font,str:string): Draw->Point
#{
#	#newstr:=array[3*(len str)] of byte;
        #utfbuf:=array[4] of byte;
        #newstr_i:=0;
#        chinese_char:=0;
#
#        for(i:=0;i<len str;i++)
#                {
#                c:=int str[i];
#                if(c>=16r4e00)
#                {
#                chinese_char=1;
#		break;
#                } #if 4e00
#                }
#	if(chinese_char)
#		return dst.text(p,src,sp,defaultFnt,str);	
#	else
#		return dst.text(p,src,sp,font,str);
#
#
#}

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
if(c>16ra0)
        {
                index++;
                secondbyte:=int buf[index];
                unicodechar:=b2u->Big5ToUnic(c*256+secondbyte);
                #sys->print("unicodechar:%x\n",unicodechar);
                index++;
                return (unicodechar,index);
        }
else 
	return (-1,index);

}
