typedef struct Vmode Vmode;

struct Vmode {
	int	wid;	/* 0 -> default or any match for all fields */
	int	hgt;
	uchar	d;
	uchar	hz;
	ushort	flags;
};

enum {
	VMODE_MONO = 0x0001,	/* monochrome display */
	VMODE_COLOR = 0x0002,	/* color (RGB) display */
	VMODE_TFT = 0x0004,	/* TFT (active matrix) display */
	VMODE_STATIC = 0x0010,	/* fixed palette */
	VMODE_PSEUDO = 0x0020,	/* changeable palette */
	VMODE_LINEAR = 0x0100,	/* linear frame buffer */
	VMODE_PAGED = 0x0200,	/* paged frame buffer */
	VMODE_PLANAR = 0x1000,	/* pixel bits split between planes */
	VMODE_PACKED = 0x2000,	/* pixel bits packed together */
	VMODE_LILEND = 0x4000,	/* little endian */
	VMODE_BIGEND = 0x8000,	/* big endian */
};

int 	vmodematch(Vmode *, Vmode *);
int 	setscreen(Vmode *mode);

