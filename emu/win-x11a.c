/*
 * win-x11a.c -
 *
 * This is a memflushscreen based implementation of the emu x11
 * screen.  The advantages over the current win-x11.c is that this
 * driver supports all colour depths (including 15/16 bit!) but it
 * also has a different set of performance characteristics and thus
 * until a quantative bench mark of all the various drawing primatives
 * have been done, this file is included as an optional replacement
 * for the current win-x11.c.
 *
 * To Use:
 *
 *   1. copy memimage/mkfile-Inferno over memimage/mkfile-Posix and
 *   rebuild memimage.  
 *
 *   2. copy memlayer/mkfile-Inferno over memlayer/mkfile-Posix and
 *   rebuild memlayer.  
 *
 *   3. change win-x11.$O to win-x11a.$o in emu/mkfile-$(SYSTARG) and
 *   rebuild emu 4. enjoy emu at 16 bit colour ...
 *
 *       CraigN 
 */

#define _GNU_SOURCE 1
#include "dat.h"
#include "fns.h"
#include "cursor.h"
#include "keyboard.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

/*
 * alias defs for image types to overcome name conflicts
 */
typedef struct ICursor		ICursor;
typedef struct IPoint		IPoint;
typedef struct IRectangle	IRectangle;
typedef struct CRemapTbl	CRemapTbl;
struct ICursor
{
	int	w;
	int	h;
	int	hotx;
	int	hoty;
	char	*src;
	char	*mask;
};

struct IPoint
{
	int	x;
	int	y;
};

struct IRectangle
{
	IPoint	min;
	IPoint	max;
};

enum
{
	DblTime	= 300		/* double click time in msec */
};

/* screen data .... */
static unsigned char *gscreendata;
static unsigned char *xscreendata;

XColor			map[256];	/* Inferno colormap array */
XColor			map7[128];	/* Inferno colormap array */
uchar			map7to8[128][2];

static Colormap		xcmap;		/* Default shared colormap  */
static int 		infernotox11[256]; /* Values for mapping between */
static int		triedscreen;
static XModifierKeymap *modmap;
static int		keypermod;
static Drawable		xdrawable;
static void		xexpose(XEvent*);
static void		xmouse(XEvent*);
static void		xkeyboard(XEvent*);
static void		xmapping(XEvent*);
static void		xproc(void*);
static void		xinitscreen(int, int);
static void		initmap(Window);
static GC		creategc(Drawable);
static void		graphicscmap(XColor*);
static int		xscreendepth;
static Display		*xdisplay, *xkmcon;
static Visual		*xvis;
static GC		xgc;
static XImage 		*img;
static int              is_shm;
static XShmSegmentInfo	*shminfo;

/* The documentation for the XSHM extension implies that if the server
   supports XSHM but is not the local machine, the XShm calls will
   return False; but this turns out not to be the case.  Instead, the
   server throws a BadAccess error.  So, we need to catch X errors
   around all of our XSHM calls, sigh.  */
static int shm_got_x_error = False;
static XErrorHandler old_handler = 0;
static XErrorHandler old_io_handler = 0;

static int
shm_ehandler(Display *dpy, XErrorEvent *error)
{
	shm_got_x_error = 1;
	return 0;
}

static void
clean_errhandlers(void)
{
	/* remove X11 error handler(s) */
	if (old_handler)
		XSetErrorHandler(old_handler); 
	old_handler = 0;
	if (old_io_handler)
		XSetErrorHandler(old_io_handler); 
	old_io_handler = 0;
}

ulong*
attachscreen(IRectangle *r, int *ld, int *width, int *softscreen)
{
	r->min.x = 0;
	r->min.y = 0;
	r->max.x = Xsize;
	r->max.y = Ysize;
	*ld = 3;
	*width = Xsize/4;
	*softscreen = 1;
	if(!triedscreen){
		triedscreen = 1;
		xinitscreen(Xsize, Ysize);
		if(kproc("xproc", xproc, 0) < 0) {
			fprint(2, "emu: win-x11 can't make X proc\n");
			return 0;
		}
	}

	/* check for X Shared Memory Extension */
	is_shm = XShmQueryExtension(xdisplay);
	
	if (is_shm) {
		shminfo = malloc(sizeof(XShmSegmentInfo));

		/* setup to catch X11 error(s) */
		XSync(xdisplay, 0); 
		shm_got_x_error = 0; 
		if (old_handler != shm_ehandler)
			old_handler = XSetErrorHandler(shm_ehandler);
		if (old_io_handler != shm_ehandler)
			old_io_handler = XSetErrorHandler(shm_ehandler);

		img = XShmCreateImage(xdisplay, xvis, xscreendepth, ZPixmap, 
				      NULL, shminfo, Xsize, Ysize);
		XSync(xdisplay, 0);

		/* did we get an X11 error? if so then try without shm */
		if (shm_got_x_error) {
			is_shm = 0;
			free(shminfo);
			shminfo = NULL;
			clean_errhandlers();
			goto next;
		}
		
		if (!img) {
			fprint(2, "emu: can not allocate virtual screen buffer\n");
			cleanexit(0);
		}
		
		shminfo->shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height,
					IPC_CREAT|0777);
		shminfo->shmaddr = img->data = shmat(shminfo->shmid, 0, 0);
		shminfo->readOnly = True;

		if (!XShmAttach(xdisplay, shminfo)) {
			fprint(2, "emu: can not allocate virtual screen buffer\n");
			cleanexit(0);
		}
		XSync(xdisplay, 0);

		/* Delete the shared segment right now; the segment
		   won't actually go away until both the client and
		   server have deleted it.  The server will delete it
		   as soon as the client disconnects, so we should
		   delete our side early in case of abnormal
		   termination.  (And note that, in the context of
		   xscreensaver, abnormal termination is the rule
		   rather than the exception, so this would leak like
		   a sieve if we didn't do this...)  */
		shmctl(shminfo->shmid, IPC_RMID, 0);

		/* did we get an X11 error? if so then try without shm */
		if (shm_got_x_error) {
			is_shm = 0;
			XDestroyImage(img);
			XSync(xdisplay, 0);
			free(shminfo);
			shminfo = NULL;
			clean_errhandlers();
			goto next;
		}

		gscreendata = malloc(Xsize * Ysize);
		xscreendata = img->data;
		
		clean_errhandlers();
	}
 next:
	if (!is_shm) {
		
		/* allocate virtual screen */	
		gscreendata = malloc(Xsize * Ysize);
		xscreendata = malloc(Xsize * Ysize * (xscreendepth >> 3));
		if (!gscreendata || !xscreendata) {
			fprint(2, "emu: can not allocate virtual screen buffer\n");
			return 0;
		}

		img = XCreateImage(xdisplay, xvis, xscreendepth, ZPixmap, 0, 
				   xscreendata, Xsize, Ysize, 8, Xsize * (xscreendepth >> 3));
		if (!img) {
			fprint(2, "emu: can not allocate virtual screen buffer\n");
			return 0;
		}
		
	}
	

	return (ulong *)gscreendata;;
}

void
flushmemscreen(IRectangle r)
{
	int x, y, width, height, dx;
	unsigned char *p, *ep, *cp;

	// Clip to screen
	if (r.min.x < 0)
		r.min.x = 0;
	if (r.min.y < 0)
		r.min.y = 0;
	if (r.max.x >= Xsize)
		r.max.x = Xsize - 1;
	if (r.max.y >= Ysize)
                r.max.y = Ysize - 1;

	// is there anything left ...	
	width = r.max.x-r.min.x;
	height = r.max.y-r.min.y;
	if ((width < 1) | (height < 1))
		return;

	// Blit the pixel data ...
	if (xscreendepth == 16) {
		unsigned short *sp;
	
		dx = Xsize - width;
		p = gscreendata + r.min.y * Xsize + r.min.x;
		sp = (unsigned short *)(xscreendata + (r.min.y * Xsize + r.min.x) * 2);
		ep = gscreendata + r.max.y * Xsize + r.max.x;
		while (p < ep) {
			const unsigned char *lp = p + width;

			while (p < lp) 
				*sp++ = infernotox11[*p++];

			p += dx;
			sp += dx;
		}

	} else if (xscreendepth == 8) {

                dx = Xsize - width;
                p = gscreendata + r.min.y * Xsize + r.min.x;
                cp = xscreendata + r.min.y * Xsize + r.min.x;
                ep = gscreendata + r.max.y * Xsize + r.max.x;
                while (p < ep) {
                        const unsigned char *lp = p + width;

                        while (p < lp)
                                *cp++ = infernotox11[*p++];

                        p += dx;
                        cp += dx;
                }

	} else {
		for (y = r.min.y; y < r.max.y; y++) {
			x = r.min.x;
			p = gscreendata + y * Xsize + x;
			while (x < r.max.x)
				XPutPixel(img, x++, y, infernotox11[*p++]);
		}
	}

	/* Display image on X11 */
	if (is_shm)
		XShmPutImage(xdisplay, xdrawable, xgc, img, r.min.x, r.min.y, r.min.x, r.min.y, width, height, 0);
	else
		XPutImage(xdisplay, xdrawable, xgc, img, r.min.x, r.min.y, r.min.x, r.min.y, width, height);
	XSync(xdisplay, 0);
}

static int
revbyte(int b)
{
	int r;

	r = 0;
	r |= (b&0x01) << 7;
	r |= (b&0x02) << 5;
	r |= (b&0x04) << 3;
	r |= (b&0x08) << 1;
	r |= (b&0x10) >> 1;
	r |= (b&0x20) >> 3;
	r |= (b&0x40) >> 5;
	r |= (b&0x80) >> 7;
	return r;
}

static void
gotcursor(ICursor c)
{
	Cursor xc;
	XColor fg, bg;
	Pixmap xsrc, xmask;
	static Cursor xcursor;

	if(c.src == nil){
		if(xcursor != 0) {
			XFreeCursor(xdisplay, xcursor);
			xcursor = 0;
		}
		XUndefineCursor(xdisplay, xdrawable);
		XFlush(xdisplay);
		return;
	}
	xsrc = XCreateBitmapFromData(xdisplay, xdrawable, c.src, c.w, c.h);
	xmask = XCreateBitmapFromData(xdisplay, xdrawable, c.mask, c.w, c.h);

	fg = map[255];
	bg = map[0];
	fg.pixel = infernotox11[255];
	bg.pixel = infernotox11[0];
	xc = XCreatePixmapCursor(xdisplay, xsrc, xmask, &fg, &bg, c.hotx, c.hoty);
	if(xc != 0) {
		XDefineCursor(xdisplay, xdrawable, xc);
		if(xcursor != 0)
			XFreeCursor(xdisplay, xcursor);
		xcursor = xc;
	}
	XFreePixmap(xdisplay, xsrc);
	XFreePixmap(xdisplay, xmask);
	XFlush(xdisplay);
	free(c.src);
}

void
drawcursor(Drawcursor* c)
{
	ICursor ic;
	IRectangle ir;
	uchar *bs, *bc;
	int i, j;
	int h = 0, bpl = 0;
	char *src, *mask, *csrc, *cmask;

	/* Set the default system cursor */
	src = nil;
	mask = nil;
	if(c->data != nil){
		h = (c->maxy-c->miny)/2;
		ir.min.x = c->minx;
		ir.min.y = c->miny;
		ir.max.x = c->maxx;
		ir.max.y = c->maxy;
		/* passing IRectangle to Rectangle is safe */
		bpl = bytesperline(ir, 0);

		i = h*bpl;
		src = malloc(2*i);
		if(src == nil)
			return;
		mask = src + i;

		csrc = src;
		cmask = mask;
		bc = c->data;
		bs = c->data + h*bpl;
		for(i = 0; i < h; i++){
			for(j = 0; j < bpl; j++) {
				*csrc++ = revbyte(bs[j]);
				*cmask++ = revbyte(bs[j] | bc[j]);
			}
			bs += bpl;
			bc += bpl;
		}
	}
	ic.w = 8*bpl;
	ic.h = h;
	ic.hotx = c->hotx;
	ic.hoty = c->hoty;
	ic.src = src;
	ic.mask = mask;

	gotcursor(ic);
}

static void
xproc(void *arg)
{
	ulong mask;
	XEvent event;

	closepgrp(up->env->pgrp);
	closefgrp(up->env->fgrp);

	mask = 	KeyPressMask|
		ButtonPressMask|
		ButtonReleaseMask|
		PointerMotionMask|
		Button1MotionMask|
		Button2MotionMask|
		Button3MotionMask|
		ExposureMask;

	XSelectInput(xkmcon, xdrawable, mask);		
	for(;;) {
		XWindowEvent(xkmcon, xdrawable, mask, &event);
		switch(event.type) {
		case KeyPress:
			xkeyboard(&event);
			break;
		case ButtonPress:
		case ButtonRelease:
		case MotionNotify:
			xmouse(&event);
			break;
		case Expose:
			xexpose(&event);
			break;
		case MappingNotify:
			xmapping(&event);
			break;
		default:
			break;
		}
	}
}


static void
xinitscreen(int xsize, int ysize)
{
	char *argv[2];
	char *disp_val;
	Window rootwin;
	XWMHints hints;
	Screen *screen;
	int rootscreennum;
	XTextProperty name;
	XClassHint classhints;
	XSizeHints normalhints;
	XSetWindowAttributes attrs;
 
	xdrawable = 0;

	xdisplay = XOpenDisplay(NULL);
	if(xdisplay == 0){
		disp_val = getenv("DISPLAY");
		if(disp_val == 0)
			disp_val = "not set";
		fprint(2, "emu: win-x11 open %r, DISPLAY is %s\n", disp_val);
		cleanexit(0);
	}

	rootscreennum = DefaultScreen(xdisplay);
	rootwin = DefaultRootWindow(xdisplay);
	xscreendepth = DefaultDepth(xdisplay, rootscreennum);
	xvis = DefaultVisual(xdisplay, rootscreennum);
	screen = DefaultScreenOfDisplay(xdisplay);
	xcmap = DefaultColormapOfScreen(screen);

	if (xvis->class != StaticColor) {
		graphicscmap(map);
		initmap(rootwin);
	}

	if ((modmap = XGetModifierMapping(xdisplay)) != 0)
		keypermod = modmap->max_keypermod;

	attrs.colormap = xcmap;
	attrs.background_pixel = 0;
	attrs.border_pixel = 0;
	/* attrs.override_redirect = 1;*/ /* WM leave me alone! |CWOverrideRedirect */
	xdrawable = XCreateWindow(xdisplay, rootwin, 0, 0, xsize, ysize, 0, xscreendepth, 
				  InputOutput, xvis, CWBackPixel|CWBorderPixel|CWColormap, &attrs);

	/*
	 * set up property as required by ICCCM
	 */
	name.value = "inferno";
	name.encoding = XA_STRING;
	name.format = 8;
	name.nitems = strlen(name.value);
	normalhints.flags = USSize|PMaxSize;
	normalhints.max_width = normalhints.width = xsize;
	normalhints.max_height = normalhints.height = ysize;
	hints.flags = InputHint|StateHint;
	hints.input = 1;
	hints.initial_state = NormalState;
	classhints.res_name = "inferno";
	classhints.res_class = "Inferno";
	argv[0] = "inferno";
	argv[1] = nil;
	XSetWMProperties(xdisplay, xdrawable,
		&name,			/* XA_WM_NAME property for ICCCM */
		&name,			/* XA_WM_ICON_NAME */
		argv,			/* XA_WM_COMMAND */
		1,			/* argc */
		&normalhints,		/* XA_WM_NORMAL_HINTS */
		&hints,			/* XA_WM_HINTS */
		&classhints);		/* XA_WM_CLASS */

	XMapWindow(xdisplay, xdrawable);
	XFlush(xdisplay);

	xgc = creategc(xdrawable);

	xkmcon = XOpenDisplay(NULL);
        if(xkmcon == 0){
                disp_val = getenv("DISPLAY");
                if(disp_val == 0)
                        disp_val = "not set";
                fprint(2, "emu: win-x11 open %r, DISPLAY is %s\n", disp_val);
                cleanexit(0);
        }

}

static void
graphicscmap(XColor *map)
{
	int r, g, b, cr, cg, cb, v, num, den, idx, v7, idx7;

	for(r=0; r!=4; r++) {
		for(g = 0; g != 4; g++) {
			for(b = 0; b!=4; b++) {
				for(v = 0; v!=4; v++) {
					den=r;
					if(g > den)
						den=g;
					if(b > den)
						den=b;
					/* divide check -- pick grey shades */
					if(den==0)
						cr=cg=cb=v*17;
					else {
						num=17*(4*den+v);
						cr=r*num/den;
						cg=g*num/den;
						cb=b*num/den;
					}
					idx = r*64 + v*16 + ((g*4 + b + v - r) & 15);
					idx = 255 - idx;
					map[idx].red = cr*0x0101;
					map[idx].green = cg*0x0101;
					map[idx].blue = cb*0x0101;
					map[idx].pixel = idx;
					map[idx].flags = DoRed|DoGreen|DoBlue;

					v7 = v >> 1;
					idx7 = r*32 + v7*16 + g*4 + b;
					if((v & 1) == v7){
						map7to8[idx7][0] = idx;
						if(den == 0) { 		/* divide check -- pick grey shades */
							cr = ((255.0/7.0)*v7)+0.5;
							cg = cr;
							cb = cr;
						}
						else {
							num=17*15*(4*den+v7*2)/14;
							cr=r*num/den;
							cg=g*num/den;
							cb=b*num/den;
						}
						map7[idx7].red = cr*0x0101;
						map7[idx7].green = cg*0x0101;
						map7[idx7].blue = cb*0x0101;
						map7[idx7].pixel = idx7;
						map7[idx7].flags = DoRed|DoGreen|DoBlue;
					}
					else
						map7to8[idx7][1] = idx;
				}
			}
		}
	}
}

/*
 * Initialize and install the Inferno colormap as a private colormap for this
 * application.  Inferno gets the best colors here when it has the cursor focus.
 */  
static void 
initmap(Window w)
{
	XColor c;
	int i;

	if(xscreendepth <= 1)
		return;

	if(xvis->class == TrueColor || xvis->class == DirectColor) {
		for(i = 0; i < 256; i++) {
			c = map[i];
			/* find out index into colormap for our RGB */
			if(!XAllocColor(xdisplay, xcmap, &c)) {
				fprint(2, "emu: win-x11 can't alloc color\n");
				cleanexit(0);
			}
			infernotox11[map[i].pixel] = c.pixel;
		}
	}
	else if(xvis->class == PseudoColor) {
		if(xtblbit == 0){
			xcmap = XCreateColormap(xdisplay, w, xvis, AllocAll); 
			XStoreColors(xdisplay, xcmap, map, 256);
			for(i = 0; i < 256; i++)
				infernotox11[i] = i;
		} else {
			for(i = 0; i < 128; i++) {
				c = map7[i];
				if(!XAllocColor(xdisplay, xcmap, &c)) {
					fprint(2, "emu: win-x11 can't alloc colors in default map, don't use -7\n");
					cleanexit(0);
				}
				infernotox11[map7to8[i][0]] = c.pixel;
				infernotox11[map7to8[i][1]] = c.pixel;
			}
		}
	}
	else {
		xtblbit = 0;
		fprint(2, "emu: win-x11 unsupported visual class %d\n", xvis->class);
	}
	return;
}

static void
xmapping(XEvent *e)
{
	XMappingEvent *xe;

	if(e->type != MappingNotify)
		return;
	xe = (XMappingEvent*)e;
	if(modmap)
		XFreeModifiermap(modmap);
	modmap = XGetModifierMapping(xe->display);
	if(modmap)
		keypermod = modmap->max_keypermod;
}


/*
 * Disable generation of GraphicsExpose/NoExpose events in the GC.
 */
static GC
creategc(Drawable d)
{
	XGCValues gcv;

	gcv.function = GXcopy;
	gcv.graphics_exposures = False;
	return XCreateGC(xdisplay, d, GCFunction|GCGraphicsExposures, &gcv);
}

static void
xexpose(XEvent *e)
{
	IRectangle r;
	XExposeEvent *xe;

	if(e->type != Expose)
		return;
	xe = (XExposeEvent*)e;
	r.min.x = xe->x;
	r.min.y = xe->y;
	r.max.x = xe->x + xe->width;
	r.max.y = xe->y + xe->height;
	drawxflush(r);
}

static void
xkeyboard(XEvent *e)
{
        int ind;
        KeySym k;
        unsigned int md;

        if(gkscanq) {
                uchar ch = (KeyCode)e->xkey.keycode;
                if(e->xany.type == KeyRelease)
                        ch |= 0x80;
                qproduce(gkscanq, &ch, 1);
                return;
        }

        /*
         * I tried using XtGetActionKeysym, but it didn't seem to
         * do case conversion properly
         * (at least, with Xterminal servers and R4 intrinsics)
         */
        if(e->xany.type != KeyPress)
                return;

        md = e->xkey.state;
        ind = 0;
        if(md & ShiftMask)
                ind = 1;

        k = XKeycodeToKeysym(e->xany.display, (KeyCode)e->xkey.keycode, ind);

        /* May have to try unshifted version */
        if(k == NoSymbol && ind == 1)
                k = XKeycodeToKeysym(e->xany.display, (KeyCode)e->xkey.keycode, 0);

        if(k == XK_Multi_key || k == NoSymbol)
                return;
        if(k&0xFF00){
                switch(k){
                case XK_BackSpace:
                case XK_Tab:
                case XK_Escape:
                case XK_Delete:
                case XK_KP_0:
                case XK_KP_1:
                case XK_KP_2:
                case XK_KP_3:
                case XK_KP_4:
                case XK_KP_5:
                case XK_KP_6:
                case XK_KP_7:
                case XK_KP_8:
                case XK_KP_9:
                case XK_KP_Divide:
                case XK_KP_Multiply:
                case XK_KP_Subtract:
                case XK_KP_Add:
                case XK_KP_Decimal:
                        k &= 0x7F;
                        break;
                case XK_Linefeed:
                        k = '\r';
                        break;
                case XK_KP_Enter:
                case XK_Return:
                        k = '\n';
                        break;
                case XK_Alt_L:
                case XK_Alt_R:
                case XK_Meta_L:
                case XK_Meta_R:
                        k = Latin;
                        break;
                case XK_Left:
                case XK_KP_Left:
                  k = Left;
                  break;
                case XK_Down:
                case XK_KP_Down:
                  k = Down;
                  break;
                case XK_Right:
                case XK_KP_Right:
                  k = Right;
                  break;
                case XK_Up:
                case XK_KP_Up:
                  k = Up;
                  break;
		case XK_Home:
		case XK_KP_Home:
		  k = Home;
		  break;
		case XK_End:
		case XK_KP_End:
		  k = End;
		  break;
                case XK_Page_Up:
                case XK_KP_Page_Up:
                  k = Pgup;
                  break;
                case XK_Page_Down:
                case XK_KP_Page_Down:
                  k = Pgdown;
                  break;
                default:                /* not ISO-1 or tty control */
                        return;
                }
        }
        /* Compensate for servers that call a minus a hyphen */
        if(k == XK_hyphen)
                k = XK_minus;
        /* Do control mapping ourselves if translator doesn't */
        if(md & ControlMask)
                k &= 0x9f;
        if(k == '\t' && ind)
                k = BackTab;

        if(md & Mod1Mask)
                k = APP|(k&0xff);
        if(k == NoSymbol)
                return;

        gkbdputc(gkbdq, k);
}

static void
xmouse(XEvent *e)
{
	int s, dbl;
	XButtonEvent *be;
	XMotionEvent *me;
	XEvent motion;
	Pointer m;
	static ulong lastb, lastt;

	dbl = 0;
	switch(e->type){
	case ButtonPress:
		be = (XButtonEvent *)e;
		m.x = be->x;
		m.y = be->y;
		s = be->state;
		if(be->button == lastb && be->time - lastt < DblTime)
			dbl = 1;
		lastb = be->button;
		lastt = be->time;
		switch(be->button){
		case 1:
			s |= Button1Mask;
			break;
		case 2:
			s |= Button2Mask;
			break;
		case 3:
			s |= Button3Mask;
			break;
		}
		break;
	case ButtonRelease:
		be = (XButtonEvent *)e;
		m.x = be->x;
		m.y = be->y;
		s = be->state;
		switch(be->button){
		case 1:
			s &= ~Button1Mask;
			break;
		case 2:
			s &= ~Button2Mask;
			break;
		case 3:
			s &= ~Button3Mask;
			break;
		}
		break;
	case MotionNotify:
		me = (XMotionEvent *) e;

		/* remove excess MotionNotify events from queue and keep last one */
		while(XCheckTypedWindowEvent(xkmcon, xdrawable, MotionNotify, &motion) == True)
			me = (XMotionEvent *) &motion;

		s = me->state;
		m.x = me->x;
		m.y = me->y;
		break;
	default:
		return;
	}

	m.b = 0;
	if(s & Button1Mask)
		m.b |= 1;
	if(s & Button2Mask)
		m.b |= 2;
	if(s & Button3Mask)
		m.b |= 4;
	if(dbl)
		m.b |= 1<<4;

	m.modify = 1;
	if(ptrq != nil)
	  qproduce(ptrq, &m, sizeof(m));
}

