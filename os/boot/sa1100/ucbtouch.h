
typedef struct TouchPnt TouchPnt;
struct TouchPnt {
	int	x;
	int	y;
};

typedef struct TouchTrans TouchTrans;
struct TouchTrans {
	int	xxm;
	int	xym;
	int	yxm;
	int	yym;
	int	xa;
	int	ya;
};

typedef struct TouchCal TouchCal;
struct TouchCal {
	TouchPnt	p[4];	// screen points
	TouchPnt	r[4][4];// raw points
	TouchTrans 	t[4];	// transformations
	TouchPnt	err;	// maximum error
	TouchPnt	var;	// usual maximum variance for readings
	int 		ptp;	// pressure threshold for press
	int		ptr;	// pressure threshold for release
};

extern TouchCal touchcal;

extern void	touchrawcal(int q, int px, int py);
extern int	touchcalibrate(void);
extern int	touchreadxy(int *fx, int *fy);
extern int	touchpressed(void);
extern int	touchreleased(void);
extern void	touchsetrawcal(int q, int n, int v);
extern int	touchgetrawcal(int q, int n);

enum {
	TOUCH_NUMRAWCAL = 10,
};

