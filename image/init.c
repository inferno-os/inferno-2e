#include <lib9.h>
#include <kernel.h>
#include <image.h>
#include <interp.h>

Display*
initdisplay(char *dev)
{
	char buf[128], info[7*12+1];
	int datafd, ctlfd, reffd;
	Display *disp;
	Image *image;
	void *q;
	Dir dir;

	if(dev == 0)
		dev = "/dev";
	if(strlen(dev) > sizeof buf - 25){
		kwerrstr("initdisplay: device name too long");
		return 0;
	}

	q = libqlalloc();
	if(q == nil)
		return nil;

	sprint(buf, "%s/draw/new", dev);
	ctlfd = libopen(buf, ORDWR);
	if(ctlfd < 0){
		if(libbind("#d", dev, MBEFORE) < 0){ 
    Error1:
			libqlfree(q);
			kwerrstr("initdisplay: %s: %r", buf);
			return 0;
		}
		ctlfd = libopen(buf, ORDWR);
	}
	if(ctlfd < 0)
		goto Error1;
	if(libread(ctlfd, info, sizeof info) != sizeof info){
    Error2:
		libclose(ctlfd);
		goto Error1;
	}
	sprint(buf, "%s/draw/%d/data", dev, atoi(info+0*12));
	datafd = libopen(buf, ORDWR);
	if(datafd < 0)
		goto Error2;
	sprint(buf, "%s/draw/%d/refresh", dev, atoi(info+0*12));
	reffd = libopen(buf, OREAD);
	if(reffd < 0){
    Error3:
		libclose(datafd);
		goto Error2;
	}
	disp = malloc(sizeof(Display));
	if(disp == 0){
    Error4:
		libclose(reffd);
		goto Error3;
	}
	image = malloc(sizeof(Image));
	if(image == 0){
		free(disp);
		goto Error4;
	}
	image->display = disp;
	image->id = 0;
	image->ldepth = atoi(info+2*12);
	image->r.min.x = atoi(info+3*12);
	image->r.min.y = atoi(info+4*12);
	image->r.max.x = atoi(info+5*12);
	image->r.max.y = atoi(info+6*12);
	image->clipr = image->r;
	disp->dirno = atoi(info+0*12);
	disp->datachan = libfdtochan(datafd, ORDWR);
	disp->refchan = libfdtochan(reffd, OREAD);
	if(disp->datachan == nil || disp->refchan == nil)
		goto Error4;
	kclose(datafd);
	kclose(reffd);
	disp->image = image;
	disp->bufp = disp->buf;
	disp->qlock = q;
	libqlock(q);
	disp->ones = allocimage(disp, Rect(0, 0, 1, 1), image->ldepth, 1, 0xFF);
	disp->zeros = allocimage(disp, Rect(0, 0, 1, 1), image->ldepth, 1, 0x00);
	if(kdirfstat(ctlfd, &dir)>=0 && dir.type=='d'){
		disp->local = 1;
		disp->dataqid = dir.qid.path;
	}

	kclose(ctlfd);

	return disp;
}

/*
 * Call with d unlocked.
 * Note that disp->defaultfont and defaultsubfont are not freed here.
 */
void
closedisplay(Display *disp)
{
	libqlock(disp->qlock);
	freeimage(disp->ones);
	freeimage(disp->zeros);
	free(disp->image);
	libchanclose(disp->datachan);
	/* should cause refresh slave to shut down */
	libchanclose(disp->refchan);
	libqunlock(disp->qlock);
	libqlfree(disp->qlock);
	free(disp);
}

/*
 * See comment at top of interp/draw.c
 */

int
lockdisplay(Display *disp, int recursive)
{
	if(disp->local)
		return 0;
	if(recursive==0 || libqlowner(disp->qlock) != currun()){
		libqlock(disp->qlock);
		return 1;
	}
	return 0;
}

void
unlockdisplay(Display *disp)
{
	if(disp->local)
		return;
	libqunlock(disp->qlock);
}
