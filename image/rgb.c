#include <lib9.h>
#include <image.h>

int r_gamma = 0;		/* red gamma */
int g_gamma = 0;		/* green gamma */
int b_gamma = 0;		/* blue gamma */


int
rgb2cmap(int cr, int cg, int cb)
{
	int r, g, b, v, cv;

	if(cr < 0)
		cr = 0;
	else if(cr > 255)
		cr = 255;
	if(cg < 0)
		cg = 0;
	else if(cg > 255)
		cg = 255;
	if(cb < 0)
		cb = 0;
	else if(cb > 255)
		cb = 255;

	for(v=0; v<r_gamma; v++) 
		cr = ((cr+1)*cr)>>8;
	for(v=0; v<g_gamma; v++) 
		cg = ((cg+1)*cg)>>8;
	for(v=0; v<b_gamma; v++) 
		cb = ((cb+1)*cb)>>8;

	r = cr>>6;
	g = cg>>6;
	b = cb>>6;
	cv = cr;
	if(cg > cv)
		cv = cg;
	if(cb > cv)
		cv = cb;
	v = (cv>>4)&3;
	return 255-((((r<<2)+v)<<4)+(((g<<2)+b+v-r)&15));
}

int
cmap2rgb(int c)
{
	int j, num, den, r, g, b, v, rgb;

	c = 255-c;
	r = c>>6;
	v = (c>>4)&3;
	j = (c-v+r)&15;
	g = j>>2;
	b = j&3;
	den=r;
	if(g>den)
		den=g;
	if(b>den)
		den=b;
	if(den==0) {
		v *= 17;
		rgb = (v<<16)|(v<<8)|v;
	}
	else{
		num=17*(4*den+v);
		rgb = ((r*num/den)<<16)|((g*num/den)<<8)|(b*num/den);
	}
	return rgb;
}
