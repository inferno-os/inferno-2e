#include <lib9.h>
#include <image.h>
#include <kernel.h>

Image*
allocimage(Display *d, Rectangle r, int ld, int repl, int val)
{
	return _allocimage(d, r, ld, repl, val, 0, 0);
}

Image*
_allocimage(Display *d, Rectangle r, int ld, int repl, int val, int screenid, int refresh)
{
	uchar *a;
	char *err;
	Image *i;
	Rectangle clipr;
	int id;

	err = 0;
	i = 0;

	if(ld<0 || ld>3){
		err = "invalid ldepth";
    Error:
		if(err)
			kwerrstr("allocimage: %s", err);
		else
			kwerrstr("allocimage: %r");
		if(i)
			free(i);
		return 0;
	}

	a = bufimage(d, 1+4+4+1+2+1+4*4+4*4+1);
	if(a == 0)
		goto Error;
	d->imageid++;
	id = d->imageid;
	a[0] = 'a';
	BPLONG(a+1, id);
	BPLONG(a+5, screenid);
	a[9] = refresh;
	BPSHORT(a+10, ld);
	a[12] = repl;
	BPLONG(a+13, r.min.x);
	BPLONG(a+17, r.min.y);
	BPLONG(a+21, r.max.x);
	BPLONG(a+25, r.max.y);
	if(repl)
		/* huge but not infinite, so various offsets will leave it huge, not overflow */
		clipr = Rect(-0x3FFFFFFF, -0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF);
	else
		clipr = r;
	BPLONG(a+29, clipr.min.x);
	BPLONG(a+33, clipr.min.y);
	BPLONG(a+37, clipr.max.x);
	BPLONG(a+41, clipr.max.y);
	a[45] = val;
	if(flushimage(d, 0) < 0)
		goto Error;

	i = malloc(sizeof(Image));
	if(i == nil){
		a = bufimage(d, 1+4);
		if(a){
			a[0] = 'f';
			BPLONG(a+1, id);
			flushimage(d, 0);
		}
		goto Error;
	}
	i->display = d;
	i->id = id;
	i->ldepth = ld;
	i->r = r;
	i->clipr = clipr;
	i->repl = repl;
	i->screen = 0;
	i->next = 0;
	return i;
}

int
freeimage(Image *i)
{
	uchar *a;
	Display *d;
	Image *w;

	if(i == 0)
		return 0;
	/* make sure no refresh events occur on this if we block in the write */
	i->refptr = nil;
	d = i->display;
	a = bufimage(d, 1+4);
	if(a == 0)
		return 0;
	a[0] = 'f';
	BPLONG(a+1, i->id);
	if(i->screen){
		w = d->windows;
		if(w == i)
			d->windows = i->next;
		else
			while(w){
				if(w->next == i){
					w->next = i->next;
					break;
				}
				w = w->next;
			}
	}
	if(flushimage(d, i->screen!=0) < 0)
		return -1;

	free(i);
	return 0;
}
