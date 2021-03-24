#include	<windows.h>
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"keyboard.h"
#include	"cursor.h"

/*
 * defs for image types to overcome name conflicts
 */
typedef struct IPoint		IPoint;
typedef struct IRectangle	IRectangle;

struct IPoint
{
	LONG	x;
	LONG	y;
};

struct IRectangle
{
	IPoint	min;
	IPoint	max;
};

extern	int	bytesperline(IRectangle, int);
extern	int	main(int argc, char **argv);
static	void	dprint(char*, ...);
static	DWORD WINAPI	winproc(LPVOID);

static	HINSTANCE	inst;
static	HINSTANCE	previnst;
static	int		cmdshow;
static	HWND		window;
static	HDC		screen;
static	HPALETTE	palette;
static	int		maxxsize;
static	int		maxysize;
static	int		attached;
static	int		isunicode = 1;
static	HCURSOR		hcursor;

static	char	argv0[] = "inferno";
static	ulong	*data;

extern	DWORD	PlatformId;

int WINAPI
WinMain(HINSTANCE winst, HINSTANCE wprevinst, LPSTR cmdline, int wcmdshow)
{
	inst = winst;
	previnst = wprevinst;
	cmdshow = wcmdshow;

	/* cmdline passed into WinMain does not contain name of executable.
	 * The globals __argc and __argv to include this info - like UNIX
	 */
	main(__argc, __argv);
	return 0;
}

void
dprint(char *fmt, ...)
{
	va_list arg;
	char buf[128];

	va_start(arg, fmt);
	doprint(buf, buf+sizeof(buf), fmt, (LPSTR)arg);
	va_end(arg);
	OutputDebugString("inferno: ");
	OutputDebugString(buf);
}

int
col(int v, int n)
{
	int i, c;

	c = 0;
	for(i = 0; i < 8; i += n)
		c |= v << (16-(n+i));
	return c >> 8;
}

static void
graphicscmap(PALETTEENTRY *pal)
{
	int r, g, b, cr, cg, cb, v;
	int num, den;
	int i, j;
	for(r=0,i=0;r!=4;r++) for(v=0;v!=4;v++,i+=16){
		for(g=0,j=v-r;g!=4;g++) for(b=0;b!=4;b++,j++){
			den=r;
			if(g>den) den=g;
			if(b>den) den=b;
			if(den==0)	/* divide check -- pick grey shades */
				cr=cg=cb=v*17;
			else{
				num=17*(4*den+v);
				cr=r*num/den;
				cg=g*num/den;
				cb=b*num/den;
			}
			pal[255-i-(j&15)].peRed = cr*0x01010101;
			pal[255-i-(j&15)].peGreen = cg*0x01010101;
			pal[255-i-(j&15)].peBlue = cb*0x01010101;
			pal[255-i-(j&15)].peFlags = 0;
		}
	}
}

ulong*
attachscreen(IRectangle *r, int *ld, int *width, int *softscreen)
{
	int i;
	DWORD h;
	RECT bs;
	RGBQUAD *rgb;
	HBITMAP bits;
	BITMAPINFO *bmi;
	LOGPALETTE *logpal;
	PALETTEENTRY *pal;
	int bsh, bsw, sx, sy;

	if(attached)
		goto Return;

	/* Compute bodersizes */
	memset(&bs, 0, sizeof(bs));
	AdjustWindowRect(&bs, WS_OVERLAPPEDWINDOW, 0);
	bsw = bs.right - bs.left;
	bsh = bs.bottom - bs.top;
	sx = GetSystemMetrics(SM_CXFULLSCREEN) - bsw;
	if(Xsize > sx)
		Xsize = sx;
	sy = GetSystemMetrics(SM_CYFULLSCREEN) - bsh + 20;
	if(Ysize > sy)
		Ysize = sy;

	logpal = malloc(sizeof(LOGPALETTE) + 256*sizeof(PALETTEENTRY));
	if(logpal == nil)
		return nil;
	logpal->palVersion = 0x300;
	logpal->palNumEntries = 256;
	pal = logpal->palPalEntry;

	graphicscmap(pal);
	palette = CreatePalette(logpal);

	bmi = malloc(sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD));
	if(bmi == nil)
		return nil;
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = Xsize;
	bmi->bmiHeader.biHeight = -Ysize;	/* - => origin upper left */
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 8;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = Xsize*Ysize;
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed = 0;
	bmi->bmiHeader.biClrImportant = 0;	/* number of important colors: 0 means all */

	rgb = bmi->bmiColors;
	for(i = 0; i < 256; i++){
		rgb[i].rgbRed = pal[i].peRed;
		rgb[i].rgbGreen = pal[i].peGreen;
		rgb[i].rgbBlue = pal[i].peBlue;
	}

	screen = CreateCompatibleDC(NULL);
	if(screen == nil){
		fprint(2, "screen dc nil\n");
		return nil;
	}

	if(SelectPalette(screen, palette, 1) == nil){
		fprint(2, "select pallete failed\n");
	}
	i = RealizePalette(screen);
	GdiFlush();
	bits = CreateDIBSection(screen, bmi, DIB_RGB_COLORS, &data, nil, 0);
	if(bits == nil){
		fprint(2, "CreateDIBSection failed\n");
		return nil;
	}

	SelectObject(screen, bits);
	GdiFlush();
	CreateThread(0, 16384, winproc, nil, 0, &h);
	attached = 1;

    Return:
	r->min.x = 0;
	r->min.y = 0;
	r->max.x = Xsize;
	r->max.y = Ysize;
	*ld = 3;
	*width = Xsize/4;
	*softscreen = 1;
	return data;
}

void
flushmemscreen(IRectangle r)
{
	RECT wr;

	if(r.max.x<=r.min.x || r.max.y<=r.min.y)
		return;
	wr.left = r.min.x;
	wr.top = r.min.y;
	wr.right = r.max.x;
	wr.bottom = r.max.y;
	InvalidateRect(window, &wr, 0);
}

static int magic = 0; /* Magic ALT key flag */

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	PAINTSTRUCT paint;
	HDC hdc;
	LPMINMAXINFO mmi;
	LONG x, y, w, h, b, n;
	HCURSOR dcurs;
	char buf[4*12];
	Pointer	m;

	switch(msg) {
	case WM_SETCURSOR:
		/* User set */
		if(hcursor != NULL) {
			SetCursor(hcursor);
			break;
		}
		/* Pick the default */
		dcurs = LoadCursor(NULL, IDC_ARROW);
		SetCursor(dcurs);
		break;
	case WM_LBUTTONDBLCLK:
		b = (1<<4) | 1;
		goto process;
	case WM_MBUTTONDBLCLK:
		b = (1<<4) | 2;
		goto process;
	case WM_RBUTTONDBLCLK:
		b = (1<<4) | 4;
		goto process;
	case WM_MOUSEMOVE:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		b = 0;
	process:
		x = LOWORD(lparam);
		y = HIWORD(lparam);
		if(wparam & MK_LBUTTON)
			b |= 1;
		if(wparam & MK_MBUTTON)
			b |= 2;
		if(wparam & MK_RBUTTON)
			if(wparam & MK_CONTROL)
				b |= 2;  //simulate middle button
			else
				b |= 4;  //right button
		m.x = x;
		m.y = y;
		m.b = b;
		m.modify = 1;
		if(ptrq != nil)
			qproduce(ptrq, &m, sizeof(m));
		break;
    case WM_SYSKEYDOWN:
		break;
	case WM_KEYDOWN:
		switch(wparam){
		default:
			return 0;
		case VK_HOME:
			wparam = Home;
			break;
		case VK_END:
			wparam = End;
			break;
		case VK_UP:
			wparam = Up;
			break;
		case VK_DOWN:
			wparam = Down;
			break;
		case VK_LEFT:
			wparam = Left;
			break;
		case VK_RIGHT:
			wparam = Right;
			break;
		}
		gkbdputc(gkbdq, wparam);
		break;
	case WM_KEYUP:
		switch(wparam) 
		  {
		  default:
		    break;
		  }
		break;
	case WM_CHAR:
		/* Magic key overrides default behavior */
		/* if (magic) */
		/* Test if Alt key down */
		if (lparam & KF_ALTDOWN) 
		    gkbdputc(gkbdq, APP|(wparam&0xff));
		else 
		  {
		    /* repeat count is lparam & 0xf */
		    switch(wparam){
		    case '\n':
		      wparam = '\r';
		      break;
		    case '\r':
		      wparam = '\n';
		      break;
		    }
		    gkbdputc(gkbdq, wparam);
		  }
		break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_PALETTECHANGED:
		if((HWND)wparam == hwnd)
			break;
	/* fall through */
	case WM_QUERYNEWPALETTE:
		hdc = GetDC(hwnd);
		SelectPalette(hdc, palette, 0);
		if(RealizePalette(hdc) != 0)
			InvalidateRect(hwnd, nil, 0);
		ReleaseDC(hwnd, hdc);
		break;

	case WM_PAINT:
		hdc = BeginPaint(hwnd, &paint);
		SelectPalette(hdc, palette, 0);
		RealizePalette(hdc);
		x = paint.rcPaint.left;
		y = paint.rcPaint.top;
		w = paint.rcPaint.right - x;
		h = paint.rcPaint.bottom - y;
		BitBlt(hdc, x, y, w, h, screen, x, y, SRCCOPY);
		EndPaint(hwnd, &paint);
		break;

	case WM_GETMINMAXINFO:
		mmi = (LPMINMAXINFO)lparam;
		mmi->ptMaxSize.x = maxxsize;
		mmi->ptMaxSize.y = maxysize;
		mmi->ptMaxTrackSize.x = maxxsize;
		mmi->ptMaxTrackSize.y = maxysize;
		break;

	case WM_COMMAND:
	case WM_CREATE:
	case WM_SETFOCUS:
	case WM_DEVMODECHANGE:
	case WM_WININICHANGE:
	case WM_INITMENU:
	default:
		if(isunicode)
			return DefWindowProcW(hwnd, msg, wparam, lparam);
		return DefWindowProcA(hwnd, msg, wparam, lparam);
	}
	return 0;
}

static DWORD WINAPI
winproc(LPVOID x)
{
	MSG msg;
	RECT size;
	WNDCLASSW wc;
	WNDCLASSA wca;

	if(!previnst){
		wc.style = CS_DBLCLKS;
		wc.lpfnWndProc = WindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = inst;
		wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(100));
		wc.hCursor = NULL;
		wc.hbrBackground = GetStockObject(WHITE_BRUSH);

		wc.lpszMenuName = 0;
		wc.lpszClassName = L"inferno";

		if(RegisterClassW(&wc) == 0){
			wca.style = wc.style;
			wca.lpfnWndProc = wc.lpfnWndProc;
			wca.cbClsExtra = wc.cbClsExtra;
			wca.cbWndExtra = wc.cbWndExtra;
			wca.hInstance = wc.hInstance;
			wca.hIcon = wc.hIcon;
			wca.hCursor = wc.hCursor;
			wca.hbrBackground = wc.hbrBackground;

			wca.lpszMenuName = 0;
			wca.lpszClassName = "inferno";
			isunicode = 0;

			RegisterClassA(&wca);
		}
	}

	size.left = 0;
	size.top = 0;
	size.right = Xsize;
	size.bottom = Ysize;

	if(AdjustWindowRect(&size, WS_OVERLAPPEDWINDOW, 0)) {
		maxxsize = size.right - size.left;
		maxysize = size.bottom - size.top;
	}
	else {
		maxxsize = Xsize + 40;
		maxysize = Ysize + 40;
	}

	if(isunicode) {
		window = CreateWindowExW(
			0,			/* extended style */
			L"inferno",		/* class */
			L"Inferno",		/* caption */
			WS_OVERLAPPEDWINDOW,	/* style */
			CW_USEDEFAULT,		/* init. x pos */
			CW_USEDEFAULT,		/* init. y pos */
			maxxsize,		/* init. x size */
			maxysize,		/* init. y size */
			NULL,			/* parent window (actually owner window for overlapped) */
			NULL,			/* menu handle */
			inst,			/* program handle */
			NULL			/* create parms */
			);
	}
	else {
		window = CreateWindowExA(
			0,			/* extended style */
			"inferno",		/* class */
			"Inferno",		/* caption */
			WS_OVERLAPPEDWINDOW,	/* style */
			CW_USEDEFAULT,		/* init. x pos */
			CW_USEDEFAULT,		/* init. y pos */
			maxxsize,		/* init. x size */
			maxysize,		/* init. y size */
			NULL,			/* parent window (actually owner window for overlapped) */
			NULL,			/* menu handle */
			inst,			/* program handle */
			NULL			/* create parms */
			);
	}

	if(window == nil){
		fprint(2, "can't make window\n");
		ExitThread(0);
	}

	SetForegroundWindow(window);
	ShowWindow(window, cmdshow);
	UpdateWindow(window);
	// CloseWindow(window);

	if(isunicode) {
		while(GetMessageW(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	else {
		while(GetMessageA(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}
	ExitThread(msg.wParam);
	return 0;
}

void
drawcursor(Drawcursor* c)
{
	HCURSOR nh, oh;
	IRectangle ir;
	int i, h, j, bpl, ch, cw;
	uchar *bs, *bc, *and, *xor, *cand, *cxor;

	/* Set the default system cursor */
	if(c->data == nil) {
		oh = hcursor;
		hcursor = NULL;
		if(oh != NULL) {
			SendMessage(window, WM_SETCURSOR, (int)window, 0);
			DestroyCursor(oh);
		}
		return;
	}

	ir.min.x = c->minx;
	ir.min.y = c->miny;
	ir.max.x = c->maxx;
	ir.max.y = c->maxy;
	/* passing IRectangle to Rectangle is safe */
	bpl = bytesperline(ir, 0);

	h = (c->maxy-c->miny)/2;

	ch = h;
	cw = bpl;
	if(PlatformId != VER_PLATFORM_WIN32_NT) {
		ch = GetSystemMetrics(SM_CYCURSOR);
		cw = (GetSystemMetrics(SM_CXCURSOR)+7)/8;
	}

	i = ch*cw;
	and = mallocz(2*i, 0);
	if(and == nil)
		return;
	xor = and + i;
	memset(and, 0xff, i);
	memset(xor, 0, i);

	cand = and;
	cxor = xor;
	bc = c->data;
	bs = c->data + h*bpl;

	for(i = 0; i < ch && i < h; i++) {
		for(j = 0; j < cw && j < bpl; j++) {
			cand[j] = ~(bs[j] | bc[j]);
			cxor[j] = ~bs[j] & bc[j];
		}
		cand += cw;
		cxor += cw;
		bs += bpl;
		bc += bpl;
	}
	nh = CreateCursor(inst, c->hotx, c->hoty, 8*cw, ch, and, xor);
	if(nh != NULL) {
		oh = hcursor;
		hcursor = nh;
		SendMessage(window, WM_SETCURSOR, (int)window, 0);
		if(oh != NULL)
			DestroyCursor(oh);
	}
	free(and);
}
