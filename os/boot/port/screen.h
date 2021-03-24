
#define MAX_VCONTRAST		0xffff
#define MAX_VBRIGHTNESS		0xffff

int	setcolor(ulong p, ulong r, ulong g, ulong b);
void	getcolor(ulong p, ulong *pr, ulong *pg, ulong *pb);
void	setbrightness(ushort b);
void	setcontrast(ushort c);
ushort	getbrightness(void);
ushort	getcontrast(void);
int	setvideohz(int hz);
int	getvideohz(void);
void	graphicscmap(int);
void	screen_copy(int sx1, int sy1, int sx2, int sy2, int dx, int dy);
void	screen_putchar(int x, int y, int c);
void	screen_putstr(int x, int y, const char *s);
void	screen_xhline(int x1, int x2, int y, int c, int m);
void	screen_hline(int x1, int x2, int y, int c);
void	screen_xfillbox(int x1, int y1, int x2, int y2, int c, int m);
void	screen_fillbox(int x1, int y1, int x2, int y2, int c);
void	screen_flush(void);
void	screen_cursor(int);
void	screen_clear(int);
void	screen_putpixel(int x, int y, int c);
int	screen_getpixel(int x, int y);

void	status_bar(const char *name, int pos, int total);
void 	statusbar_erase(void);

int	imagewid(uchar *image);
int	imagehgt(uchar *image);
void	putimage(int px, int py, uchar *image, int scale);

extern int fontwid, fonthgt;
extern int text_fg, text_bg;
extern int text_cursor;
extern int text_cols, text_rows;
extern int text_x, text_y;
extern int text_wid, text_hgt;
extern int vd_wid, vd_hgt;
extern uchar *vd_fb;

