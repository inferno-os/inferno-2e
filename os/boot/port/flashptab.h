
#define FLASHPTAB_MAGIC_OFS	0x10		/* ulong */
#define FLASHPTAB_OFS_OFS	0x14		/* ulong */
#define FLASHPTAB_SIZE_OFS	0x18		/* ulong */

#define FLASH_MON_OFS_OFS	0x20		/* ulong */
#define FLASH_MON_SIZE_OFS	0x24		/* ulong */
#define FLASH_AUTOBOOT_OFS_OFS	0x28	/* ulong */
#define FLASH_BOOT_OFS_OFS	0x2c		/* ulong */

#define	FLASHPTAB_MAGIC		0xc001babe

#define FLASHPTAB_MAXNAME	20

#define FLASHPTAB_MIN_PNUM		-2
#define FLASHPTAB_PARTITION_PNUM	-2
#define FLASHPTAB_ALL_PNUM		-1

typedef struct FlashPTab {
	char 	name[FLASHPTAB_MAXNAME];
	ulong	start;
	ulong	length;
	ushort	perm;
	ushort	flags;
} FlashPTab;


enum {
	FLASHPTAB_FLAG_BOOT = 0x0010,
	FLASHPTAB_FLAG_KERN = 0x0020,
	FLASHPTAB_FLAG_TEST = 0x0040,
	FLASHPTAB_FLAG_FAIL = 0x0080,
};

int flashptab_get(FlashMap *f, const char *, FlashPTab *);
int flashptab_set(FlashMap *f, const char *, FlashPTab *);
ulong flashptab_getboot(FlashMap *f);
int flashptab_setboot(FlashMap *f, const char *);

