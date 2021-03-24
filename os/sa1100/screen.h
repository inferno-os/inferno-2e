#define CURSWID	16
#define CURSHGT	16

typedef struct Cursor {
	Point	offset;
	uchar	clr[CURSWID/BI2BY*CURSHGT];
	uchar	set[CURSWID/BI2BY*CURSHGT];
} Cursor;


typedef struct Vdisplay {
	uchar*	fb;		/* frame buffer */
	ulong	colormap[256][3];
	int	bwid;
	long	brightness;
	long	contrast;
	Lock	lock;
	Vmode; 
} Vdisplay;

Vdisplay	*lcd_init(Vmode*);
void	lcd_setcolor(ulong, ulong, ulong, ulong);
void	lcd_flush(void);

int vmodematch(Vmode *, Vmode *);

#define MAX_VCONTRAST		0xffff
#define MAX_VBRIGHTNESS		0xffff

int	getcontrast(void);
int	getbrightness(void);
void	setcontrast(int);
void	setbrightness(int);
