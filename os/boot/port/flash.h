typedef struct FlashMap FlashMap;

struct FlashMap {
	ulong base;
	uchar width;		/* number of chips across bus */
	uchar l2width;		/* log2(width) */
	uchar man_id;		/* manufacturer's ID */
	uchar pmode;		/* programming mode */
	ushort dev_id;		/* device ID */
	ushort flags;		/* various config flags */
	ulong totsize; 		/* total flash size*width, in bytes */
	ulong secsize; 		/* sector size*width, in bytes */
	uint l2secsize;		/* log2(secsize) */
	ulong bootlo;		/* lowest address of boot area */
	ulong boothi;		/* highest address of boot area+1 */
	ulong bootmap;		/* bitmap of boot area layout 
				 * broken into pieces of MBSECSIZE*width */
	const char *man_name;	/* manufacturer name */
	const char *dev_name;	/* device name */
};

enum {
	FLASH_FLAG_NEEDUNLOCK = 0x01,	/* need unlock before access */
	FLASH_FLAG_NEEDERASE = 0x04,	/* need to erase before writing */
	FLASH_FLAG_WRITEMANY = 0x08,	/* can write many words at once */
};

enum {
	FLASH_PMODE_AMD_29,		/* use AMD29xxx-style */
	FLASH_PMODE_SHARP_SA,		/* use Sharp SA-style */
	FLASH_PMODE_SHARP_SU,		/* use Sharp SU-style */
};

ulong	flash_sectorsize(FlashMap*, ulong ofs);
ulong	flash_sectorbase(FlashMap*, ulong ofs);
uchar	flash_isprotected(FlashMap*, ulong ofs);
int	flash_protect(FlashMap*, ulong ofs, ulong size, int yes);
int	flash_write_sector(FlashMap*, ulong ofs, ulong *data);
int	flash_init(FlashMap*);

