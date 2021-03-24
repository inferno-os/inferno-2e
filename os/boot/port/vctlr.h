#define CURSWID	16
#define CURSHGT	16

typedef struct Cursor {
	Point	offset;
	uchar	clr[CURSWID/BI2BY*CURSHGT];
	uchar	set[CURSWID/BI2BY*CURSHGT];
} Cursor;

typedef struct Vdisplay {
	uchar*	fb;		/* frame buffer */
	struct Vctlr*	vctlr;
	ulong	colormap[256][3];
	int	bwid;
	ushort	brightness;
	ushort	contrast;
	Lock	lock;
	Vmode; 
} Vdisplay;

typedef struct Vctlr Vctlr;
struct Vctlr {
	char*	name;
	Vdisplay* (*init)(Vmode*);
	void	(*setcolor)(ulong, ulong, ulong, ulong);
	void	(*enable)(void);
	void	(*disable)(void);
	void	(*move)(int, int);
	void	(*load)(Cursor*);
	void	(*flush)(uchar*, int, int, Rectangle);
	void	(*setbrightness)(ushort);
	void	(*setcontrast)(ushort);
	void	(*sethz)(int);
	Vctlr*	link;
};

void	addvctlrlink(Vctlr *);
void*	screenalloc(ulong);

