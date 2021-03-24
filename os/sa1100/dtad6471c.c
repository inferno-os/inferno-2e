/*
 *	D6471C ``EasyTad'' physical interface
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include 	"io.h"
#include 	"dtad6471c.h"
#include 	"devtad.h"

enum {
	/* UMEC2000 assignment */
	GPIO_DTAD=        (1 << 15),	/* in  - dtad IRQ pin */
};

struct
{
	QLock;
	Rendez	r;
} dtad;


static void
dtadintr(Ureg *, void *)
{
	GPIOREG->gedr = GPIO_DTAD;	/* clear interrupt */
	wakeup(&dtad.r);
}

static int
dtadready(void *)
{
	return (GPIOREG->gplr & GPIO_DTAD) == 0;
}

static ulong
dtadrd(void)
{
	uchar *p;
	int s;

	p = KADDR(DTAD_BASE);
	/* return the status word (data sheet insists low byte must be read first) */
	s = p[0];
	s |= p[4] << 8;
	return s;
}

/*
 * apply a sequence of dtad commands atomically (for compound commands)
 */
static int
dtadops(ulong *ops, int n)
{
	uchar *p;
	ulong w, s;
	int i;

	qlock(&dtad);
	if(waserror()){
		qunlock(&dtad);
		nexterror();
	}

	p = KADDR(DTAD_BASE);
	for(i=0; i<n; i++){
		/* write command */
		w = ops[i];
		p[0] = w;
		p[4] = w>>8;

		if((GPIOREG->gplr & GPIO_DTAD) != 0) {
			if (tsleep(&dtad.r, dtadready, nil, 1000) == 0) {
				poperror();
				qunlock(&dtad);
				return -1;
			}
		}

		/* return the status word (low byte must be read first) */
		s = p[0];
		s |= p[4] << 8;
		ops[i] = s;
		if(n > 1 && s != w)
			break;	/* error status in multiword command */
	}

	poperror();
	qunlock(&dtad);

	return i;
}

static ulong
dtadop(ulong x)
{
	if(dtadops(&x, 1) != 1)
		return -1;
	return x;
}

static int
dtadcmd(ulong cmd)
{
	ulong x;

	x = cmd;
	if(dtadops(&x, 1) != 1)
		return -1;
	return x == cmd;
}

int 
dtad_idle(void)
{
	return dtadop(0) == 0;
}

int
dtad_rec(int mode, int vox, int loopback )
{
	return dtadop((1<<12)|((mode&7)<<7)|((vox&1)<<6)|((loopback&1)<<5));
}

int 
dtad_rectgen(int tone, int gain, int tailcut)
{
	if(tone) 
		return dtadop((1<<12)|(1<<9)|((gain&0xf)<<5)|(tone&0x1f));
	else
		return dtadop((1<<12)|(tailcut&0x7f));
}

int 
dtad_play(int speed, int msgnum)
{
	return dtadcmd((2 << 12) | ((speed&7) << 7) | (msgnum&0x7f));
}

int 
dtad_play_offset(int offset)
{
	return dtadcmd((2 << 12) | (offset&0x3ff));
}

int 
dtad_play_pause(int pause, int offset, int speed)
{
	return dtadop((2<<12)|((pause&1)<<11)|((offset&1)<<10)|((speed&7)<<7));
}

int 
memory_status(int checksum)
{
	return dtadop((3<<12)|((checksum&1)<<7));
}

int 
product_number(void)
{	
	return dtadop((3<<12)|(1<<10));
}

int 
number_write(int directory, int word, int number)
{
	ulong ops[2];

	ops[0] = (4<<12)|((directory&0x3f)<<2)|(word&3);
	ops[1] = number & 0xFF;
	return dtadops(ops, 2) == 2;
}

int 
number_read(int directory, int word)
{
	return dtadop((5<<12)|((directory&0x3f)<<2)|(word&3));
}

int 
tgen(int gain, int index)
{
	return dtadop((6<<12)|((gain&0xf)<<5)|(index&0x3f));
}

int 
newtone( int index, int gain1, int gain2, int freq1, int freq2 )
{
	ulong ops[4];

	ops[0] = (6<<12)|(2<<10)|(index&0x3f);
	ops[1] = (6<<12)|((gain1&0xf)<<4)|(gain2&0xf);
	ops[2] = freq1;
	ops[3] = freq2;
	return dtadops(ops, 4) == 4;
}

int 
linemonitor(int cid, int noloop, int seizemode)
{
	return dtadop((7<<12)|((cid&1)<<10)|((seizemode&1)<<7)|((noloop&1)<<5));
}

int 
msg_delete(int gc, int nuke, int msgnum)
{
	return dtadop((8<<12)|((gc&1)<<11)|((nuke&1)<<10)|(msgnum&0x7f));
}

int 
msg_stamp(int modify, int msgnum, int stamp)
{
	ulong ops[2];

	ops[0] = (9<<12)|((modify&1)<<10)|(msgnum&0x7f);
	ops[1] = stamp;
	return dtadops(ops, 2) == 2;
}

ulong
msg_getstamp(int msgnum)
{
	ulong op;

	op = (10<<12)|(msgnum&0x7F);
	return (dtadop(op)<<16) | dtadop(op|(1<<11));
}

ulong
get_timeleft(void)
{
	return dtadop(11<<12)&0xfff;
}

int 
self_test(int mode, int params)
{
	return dtadop((12<<12)|((mode&0xf)<<8)|(params&0xff));
}

int 
dtadflash_init( int size, int devicemask )
{
	return self_test(1,(((size&0x7)<<5)|(devicemask&0xf)));
}

int 
self_test_flashfast( int size, int devmask )
{
	return self_test(3,(((size&0x7)<<5)|(devmask&0xf)));
}

int 
flash_sel( int ext, int num )
{	
	return self_test(5,(((ext&1)<<7)|((num&7)<<4)|2));
}

int 
codec_sel( int type, int input, int output, int law )
{
	return self_test(4,((type&1)<<5)|((output&3)<<3)|((input&1)<<2)|((law&1)<<1));
}

int 
self_test_codecloop( void )
{
	return self_test(6,0)==((12<<12)|(6<<8));
}

int 
config_detector( int numdtmf )
{
	return self_test(7,(numdtmf&1))==((12<<12)|(7<<8)|(numdtmf&1));
}

int 
set_volume( int volume )
{
	return self_test(8,(1<<7)|(volume&0x3f)) == ((12<<12)|(8<<8)|(1<<7)|(volume&0x3f));
}

int 
get_volume( void )
{
	return self_test(8,0)&0x3f;
}

int
set_sensitivity( int system, int level )
{
	return (self_test(10,((system&3)<<6)|(1<<5)|(level&0x3f)) == 
			((12<<12)|(19<<8)|((system&3)<<6)|(1<<5)|(level&0x3f)));
}

int 
read_sensitivity( int system )
{
	return (self_test(10,((system&3)<<6)|(1<<5))&0x3f);
}

int 
prompt_playback( int load, int sector, int speed, int number )
{
	return dtadop((14<<12)|((load&1)<<11)|((sector&1)<<10)|((speed&7)<<7)|(number&0x3f));
}


int 
speakerphone( int rt, int tr, int line, int mike, int priority )
{
	return dtadop((14<<12)|((rt&3)<<10)|((tr&3)<<8)|((line&7)<<5)|((mike&7)<<2)|((priority&1)<<1));
}

int 
spkr_stat(void)
{
	return dtadop(14<<12);
}

int 
spkr_monitor(void)
{
	return dtadop((14<<12)|(1<<8));
}

int 
spkr_volume( int line_vol, int spkr_vol, int atten )
{
	return dtadcmd((14<<12)|(1<<9)|((line_vol&3)<<7)|((spkr_vol&0xf)<<3)|(atten&7));
}

int 
spkr_config( int twist, int dt, int train, int autotrain )
{
	return dtadcmd((14<<12)|(2<<9)|((twist&3)<<6)|((dt&3)<<4)|((train&3)<<2)|(autotrain&3));
}

int 
spkr_config2( int spkrnoise, int linenoise, int lineproc, int mvox_pos, int mvox_resp, int lvox_resp)
{
	return dtadcmd((14<<12)|(3<<9)|((spkrnoise&1)<<7)|((linenoise&1)<<6)|((lineproc&1)<<5)|
			((mvox_pos&1)<<4)|((mvox_resp&3)<<2)|(lvox_resp&3));
}

int 
spkr_tgen(int gain, int index)
{
	return dtadcmd((14<<12) | (4<<9) | ((gain&0x0f)<<5) | (index&0xf));
}

int 
spkr_param2( int param, int value )
{
	return dtadcmd((14<<12)|(5<<9)|((param&0x7)<<6)|(value&0x7f));
}

typedef struct Tone Tone;
struct Tone {
	int	n;
	int	f0;	/* 32767.0*cos(2*PI*freq/7200.0) */
	int	f1;
};
static Tone tones[] = {
	{0xD, 26890, 4758},	/* DTMF A: 697, 1633 */
	{0xE, 25643, 4758},	/* DTMF B: 770, 1633 */
	{0xF, 24119, 4758},	/* DTMF C: 852, 1633 */
	{0x10, 22326, 4758},	/* DTMF D: 941, 1633 */
};

void
dtad_init(void)
{
	ulong s;
	int i;

	/* set DTAD ACK Interrupt */
	intrenable(15, dtadintr, nil, BusGPIO);
	GPIOREG->gfer |= GPIO_DTAD;
	GPIOREG->gedr = GPIO_DTAD;	/* clear interrupt */

	s = dtadrd();
	print("		dtad boot status: %lux: ", s);
	if (s == TAD_COLD) {
		print("Cold Start\n");
		/* select the flash hw */
		s = flash_sel(0, 0);
		print("		flash selection returned: %lux\n",s);
		/* run flash self test */
		s = dtadflash_init(0,0xf);
		print("		flash self-test returned: %lux\n",s);
		print("			flash bank status: %d %d %d %d\n",((s&1)!=0),
			((s&2)!=0),((s&4)!=0),((s&8)!=0));
		/* read status */
		s = dtadrd();
		print("		dtad post-cold boot status: %lux\n", s);
		if (s == TAD_COLD) {
			print("		Initialization Failed!\n");
			return;
		}
	} else if(s == TAD_WARM) {
		print("Warm Start\n");
	} else
		print("TAD not reset correctly: status #%4.4lux\n", s);

	for(i=0; i<nelem(tones); i++){
		Tone *t;
		t = &tones[i];
		print("tone #%x: %d\n", t->n, newtone(t->n, 1, 0, t->f0, t->f1));
	}

	/* get product number & print */
	s = product_number();
	print("		Product Number: %lux\n",s);

	/* select codec (remember board has line <-> speaker the wrong way around!) */
#ifdef SWORD
	s = codec_sel(0, 0, 0, 0);
#else
	s = codec_sel(0, 0, 3, 0);
#endif
	print("		Codec Selection Returned: %lux\n", s);

	/* read memory status */
	s = memory_status( 0 );
	print("		Memory Status Reported: %lux\n", s);
	if (s & DMem_Full)
		print("				Memory Full\n");
	print("			Number of Messages: %lux\n",s & DMem_Num);
	if (s & DMem_GC) {
		print("				Garbage Collection Recommended... Collecting ...\n");
		s = msg_delete(1, 0, 0);
		print("				Garbage Collection Returned: %lux\n", s);
	}
	
	/* put us back into idle */
	dtad_idle();
}
