typedef struct Flash Flash;

/*
 * structure defining a flash memory card
 */
struct Flash {
	QLock;	/* interlock on flash operations */
	Flash*	next;

	/* the following are filled in by devflash before Flash.reset called */
	char*	name;
	void*	addr;
	ulong	size;
	int	(*reset)(Flash*);

	/* the following are filled in by the reset routine */
	int	(*eraseall)(Flash*);
	int	(*erasezone)(Flash*, int);
	int	(*write)(Flash*, ulong, void*, long);	/* writes of correct width and alignment */
	int	(*suspend)(Flash*);
	int	(*resume)(Flash*);

	uchar	id;	/* flash manufacturer ID */
	uchar	devid;	/* flash device ID */
	int	width;	/* bytes per flash line */
	int	erasesize;	/* size of erasable unit (accounting for width) */
	void*	data;		/* flash type routines' private storage, or nil */
	ulong	unusable;	/* bit mask of unusable sections */
};

/*
 * called by link routine of driver for specific flash type: arguments are
 * conventional name for card type/model, and card driver's reset routine.
 */
void	addflashcard(char*, int (*)(Flash*));

/*
 * called by devflash.c:/^flashreset; if flash exists,
 * sets type, address, and size in bytes of flash
 * and returns 0; returns -1 if flash doesn't exist
 */
int	archflashreset(char*, void**, long*);
