#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "../port/flash.h"

/* 
Flash chips supported:

	AMD:
		Am29F040B
		Am29LV800BT
		Am29LV800BB
		Am29LV160BT
		Am29LV160BB
	Atmel:
		AT29LV1024
	Sharp:
		LH28F016SU
	Intel:  TE28F160S3 -support UMEC and Duoyuan phone (Peter)

Flash chips that haven't been tested, but can probably be made to work easily:

	Sharp:
		LH28F016SA
		LH28F008SA
*/

#define BUSWIDTH	4	/* number of bytes across bus */
#define L2BUSWIDTH	2	/* log2(BUSWIDTH) */
#define MBSECSIZE	4096	/* minimum boot sector size */
#define L2MBSECSIZE	12	/* log2(MBSECSIZE) */

#define BS_16K_8K_8K_32K	((1<<3)|(1<<5)|(1<<7)|(1<<15))
#define BS_32K_8K_8K_16K	((1<<7)|(1<<9)|(1<<11)|(1<<15))

ulong
flash_sectorsize(FlashMap *f, ulong offset)
{
	if(f->secsize >= MBSECSIZE
			&& offset >= f->bootlo && offset < f->boothi) {
		int n = (offset-f->bootlo)>>(L2MBSECSIZE+f->l2width);
		ulong b = f->bootmap;
		ulong s;
		SET(s);
		while(n >= 0) {
			s = 1;
			while(!(b&1)) {
				b >>= 1;
				++s;
				n--;
			}
			while(b&1) {
				b >>= 1;
				n--;
			}
		}
		return s<<(L2MBSECSIZE+f->l2width);
	}
	if(offset >= f->totsize)
		return 0;
	return f->secsize;
}

ulong
flash_sectorbase(FlashMap *f, ulong offset)
{
	/* assumes sectors are aligned to their size */
	ulong s = flash_sectorsize(f, offset);
	if(s)
		return offset&~(s-1);
	else
		return (ulong)-1;
}

static void
unlock_write(FlashMap *f)
{
	if(f->flags & FLASH_FLAG_NEEDUNLOCK) {
		*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0xaaaaaaaa;
		*(ulong *)(f->base + 0x2aaa*BUSWIDTH) = 0x55555555;
	}
}

static void
reset_read_mode(FlashMap *f)
{
	unlock_write(f);
	switch(f->pmode) {
	case FLASH_PMODE_AMD_29:
		*(ulong*)(f->base + 0x5555*BUSWIDTH) = 0xf0f0f0f0;
		delay(20);
		break;
	case FLASH_PMODE_SHARP_SA:
	case FLASH_PMODE_SHARP_SU:
	default:
		*(ulong*)(f->base) = 0xffffffff;
		break;
	}
}

static void
abort_and_reset(FlashMap *f)
{
	switch(f->pmode) {
	case FLASH_PMODE_SHARP_SA:
	case FLASH_PMODE_SHARP_SU:
		// *(ulong*)(f->base) = 0x80808080;
		break;
	}
	reset_read_mode(f);
}

static void 
autoselect_mode(FlashMap *f)
{
	unlock_write(f);
  	*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0x90909090;
	delay(20);
}


static void
flash_readid(FlashMap *f)
{
	ulong id;
	char *chipman = "?";
	char *chipname = "?";

	f->flags = FLASH_FLAG_NEEDUNLOCK;
	autoselect_mode(f);
	f->man_id = *(ulong *)(f->base + 0x00*BUSWIDTH) & 0xff;
	id = *(ulong*)(f->base + 0x01*BUSWIDTH);
	f->dev_id = id & 0xff;
	f->secsize = 0;
	f->totsize = 0;
	f->l2width = L2BUSWIDTH;
	f->bootmap = 0;
	f->bootlo = f->boothi = 0;
	switch(f->man_id) {
	case 0x01:
		chipman = "AMD";
		f->flags |= FLASH_FLAG_NEEDERASE;
		f->pmode = FLASH_PMODE_AMD_29;
		switch(f->dev_id) {
		case 0xa4:
			chipname = "Am29F040B";
			f->totsize = 512*_K_;
			f->secsize = 64*_K_;
			f->l2secsize = 16;
			break;
		case 0xda:
			chipname = "Am29LV800BT";
			f->totsize = 1*_M_;
			f->bootlo = (1*_K_-64)*_K_;
			f->bootmap = BS_32K_8K_8K_16K;
			goto Am29LV;
		case 0x5b:
			chipname = "Am29LV800BB";
			f->totsize = 1*_M_;
			f->bootmap = BS_16K_8K_8K_32K;
			goto Am29LV;
		case 0xc4:
			chipname = "Am29LV160BT";
			f->totsize = 2*_M_;
			f->bootlo = (2*_K_-64)*_K_;
			f->bootmap = BS_32K_8K_8K_16K;
			goto Am29LV;
		case 0x49:	
			chipname = "Am29LV160BB";
			f->totsize = 2*_M_;
			f->bootmap = BS_16K_8K_8K_32K;
		    Am29LV:
			f->l2secsize = 16;		/* 64K */
			if((id&0xff00) == 0x2200)
				f->l2width = L2BUSWIDTH-1;
			break;
		}
		break;
	case 0x1f:
		chipman = "Atmel";
		f->pmode = FLASH_PMODE_AMD_29;
		switch(f->dev_id) {
		case 0x26:
			chipname = "AT29LV1024";
			f->totsize = 128*_K_;
			f->l2secsize = 8;		/* 256 bytes */
			f->l2width = L2BUSWIDTH-1;
			f->flags |= FLASH_FLAG_WRITEMANY;
			break;
		}
		break;
	case 0x89:	// from LH28F008SA docs
	case 0xb0:	// from LH28F016SU docs
		chipman = "Sharp";
		f->flags &= ~FLASH_FLAG_NEEDUNLOCK;
		f->pmode = FLASH_PMODE_SHARP_SA;
		autoselect_mode(f);
		id = *(ulong*)(f->base + 0x01*BUSWIDTH);
		switch((f->dev_id = id&0xffff)) {
		case 0x6688:
			chipname = "LH28F016SU";
			goto SharpLH28F016;
		case 0x66a0:
			chipname = "LH28F016SA";
		SharpLH28F016:
			f->totsize = 2*_M_;
			goto SharpLH28F;
		case 0xa2a2:
			chipname = "LH28F008SA";
			f->totsize = 1*_M_;
		SharpLH28F:
			f->l2secsize = 16;		/* 64K */
			f->l2width = L2BUSWIDTH-1;
			goto End;
		case 0xd0:
			chipman = "Intel";
			chipname = "TE28F160S3";
			f->l2secsize =16;
			f->totsize = 2*_M_;
			f->l2width = L2BUSWIDTH-1;
		End:	break;
		}
		break;
	default:
		break;
	}
	reset_read_mode(f);
	f->secsize = 1<<f->l2secsize;
	if(!f->boothi)
		f->boothi = f->bootlo + f->secsize;
	if(!f->bootmap)
		f->bootmap = 1<<(f->secsize/MBSECSIZE-1);
	f->width = 1<<f->l2width;

	f->secsize *= f->width;
	f->totsize *= f->width;
	f->bootlo *= f->width;
	f->boothi *= f->width;

	f->man_name = chipman;
	f->dev_name = chipname;

	print("flash@%ux: id=%ux/%ux (%s %s), ss=%ux fs=%ux w=%d b=%ux-%ux f=%ux\n",
		f->base,
		f->man_id, f->dev_id,
		chipman, chipname,
		f->secsize, f->totsize, f->width,
		f->bootlo, f->boothi, f->flags);
}


static ulong
protmask(FlashMap *f, ulong x)
{
	int i;
	ulong r, b, m;
	r = 0;
	b = 1<<(L2BUSWIDTH-f->l2width);
	m = b-1;
	b <<= 3;
	for(i=0; i<BUSWIDTH; i++) {
		r |= ((x&1)<<i);
		if((i&m) == m)
			x >>= b;
	}
	return r;
}

uchar
flash_isprotected(FlashMap *f, ulong offset)
{
	ulong x;
	switch(f->pmode) {
	case FLASH_PMODE_AMD_29:
		autoselect_mode(f);
		x = *(ulong *)(f->base + offset + 0x02*BUSWIDTH);
		reset_read_mode(f);
		return protmask(f, x);
	default:
		return 0;
	}
}


static int
amd_erase_sector (FlashMap *f, ulong offset)
{
	unlock_write(f);
	*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0x80808080;
	unlock_write(f);
	*(ulong *)(f->base + offset) = 0x30303030;
	return timer_devwait((ulong*)(f->base + offset),
				0xa0a0a0a0, 0xa0a0a0a0, MS2TMR(2000)) >= 0;
}

static int
flash_protect_sector (FlashMap *f, ulong ofs, int yes)
{
	ulong *p;
	int i;
	ofs += f->base;
	switch(f->pmode) {
	case FLASH_PMODE_AMD_29:
		p = (ulong*)(ofs + (yes ? 0x02*BUSWIDTH : 0x42*BUSWIDTH));
		if(!archflash12v(1))
			return 0;
		*p = 0x60606060;
		i = 0;
		do {
			if(++i > 25) {
				reset_read_mode(f);
				return 0;
			}
			*p = 0x60606060;
			microdelay(yes ? 150 : 15000);
			*p = 0x40404040;
		} while(protmask(f, *p) != protmask(f, 0xffffffff));
		reset_read_mode(f);
		return 1;
	default:
		return 0;
	}
}


int
flash_protect(FlashMap *f, ulong ofs, ulong size, int yes)
{
	ulong x = flash_sectorbase(f, ofs);
	while(x < ofs+size) {
		if(!flash_protect_sector(f, x, yes))
			return 0;
		x += flash_sectorsize(f, x);
	}
	return 1;
}


static int
sharp_write_sector(FlashMap *f, ulong offset, ulong *buf, ulong secsize)
{
	int i;
	ulong *p = (ulong*)(f->base + offset);
	/*print("sharp flash write\n");*/
	*p = 0x50505050;	// clear CSR
	*p = 0x20202020;	// erase
	*p = 0xd0d0d0d0;	// verify
	if(timer_devwait(p, 0x00800080, 0x00800080, MS2TMR(2000)) < 0
			|| (*p & 0x00280028)) {
		abort_and_reset(f);
		return 0;
	}
	for(i=secsize >> 2; i; i--, p++, buf++) {
		*p = 0x40404040;
		/*print("p=%ux,buf=%ux\n",*p,*buf);*/
		*p = *buf;
		if(timer_devwait(p, 0x00800080, 0x00800080, US2TMR(100)) < 0) {
			abort_and_reset(f);
			return 0;
		}
	}
	reset_read_mode(f);
	return 1;
}


static int
fast_write_sector(FlashMap *f, ulong offset, ulong *buf, ulong secsize)
{
	int i;
	unlock_write(f);
	*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0xa0a0a0a0;
	for(i=secsize/sizeof(ulong); i; i--, offset += sizeof(ulong), buf++)
		*(ulong*)(f->base + offset) = *buf;
	offset -= sizeof(ulong);
	--buf;
	if(timer_devwait((ulong*)(f->base + offset),
			0x80808080, *buf&0x80808080, MS2TMR(2000)) < 0) {
		abort_and_reset(f);
		return 0;
	}
	return 1;
}

static int
slow_write_sector(FlashMap *f, ulong offset, ulong *buf, ulong secsize)
{
	int i;
	for(i=secsize/sizeof(ulong); i; i--, offset += sizeof(ulong), buf++) {
		ulong val = *buf;
		unlock_write(f);
		*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0xa0a0a0a0;
		*(ulong *)(f->base + offset) = val;
		if(timer_devwait((ulong*)(f->base + offset),
				0x80808080, val&0x80808080, US2TMR(100)) < 0) {
			abort_and_reset(f);
			return 0;
		}
	}
	return 1;
}


int
flash_write_sector(FlashMap *f, ulong offset, ulong *buf)
{
	int tries;
	ulong secsize = flash_sectorsize(f, offset);
	if(offset+secsize > f->totsize)
		panic("flash@%ux: attempt to erase %8.8ux\n", f->base, offset);
	if(flash_isprotected(f, offset)) {
		print("flash@%ux: %ux-%ux is protected\n",
			f->base, offset, offset+secsize-1);
		return -2;
	}
	if(offset >= f->bootlo && offset < f->boothi) 
		print("flash@%ux: erasing boot area! (%ux-%ux)\n",
			f->base, offset, offset+secsize-1);
	for(tries = 3; tries; tries--)
	{
		switch(f->pmode) {
		case FLASH_PMODE_SHARP_SA:
			if(!sharp_write_sector(f, offset, buf, secsize))
				continue;
			break;
		case FLASH_PMODE_AMD_29:
			if((f->flags & FLASH_FLAG_NEEDERASE)
					&& !amd_erase_sector(f, offset))
				continue;
			if(!((f->flags & FLASH_FLAG_WRITEMANY)
				? fast_write_sector(f, offset, buf, secsize)
				: slow_write_sector(f, offset, buf, secsize)))
				continue;
			break;
		default:
			continue;
		}
		if(!memcmp((void*)(f->base+offset), buf, secsize))
			return 1;
		print("flash@%ux: verify error (%ux-%ux)\n",
			f->base, offset, offset+secsize-1);
		return -1;
	}
	print("flash@%ux: write failure (%ux-%ux)\n",
		f->base, offset, offset+secsize-1);
	return 0;
}


int
flash_init(FlashMap *f)
{
	archflashwp(0);
	flash_readid(f);
	return (f->secsize > 0) ? 0 : -1;
}

