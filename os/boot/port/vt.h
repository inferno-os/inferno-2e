
#define VT_MAXPARAM 8	/* maximum number of arguments */

struct vtstate {
	short	y1;
	short	y2;
	ushort	mode;	/* misc mode parameters */
	ushort	qmode;	/* extended mode parameters */
	ushort	attr;	/* display attributes */
	uchar	fg;	/* foreground color */
	uchar	bg;	/* background color */
/* saved values */
	short	save_x;
	short	save_y;
	ushort	save_attr;
	uchar	save_fg;
	uchar	save_bg;
	ushort	save_mode;
	ushort	save_qmode;
/* escape code parsing; */
	uchar	esc;		/* escape mode */
	uchar	pcount;		/* parameter count */
	uchar	type;		/* escape code type */
	char	ptype;		/* current parameter type */
	short	value;		/* current value */
	short	param[VT_MAXPARAM];	/* args */
};

static void vt_write(VTPARAM_C char *adr, int len);
static void vt_init(VTPARAM);

