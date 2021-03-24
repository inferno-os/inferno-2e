/*
 *  UMEC ISP 2000 EasyTAD Telephony Driver
 *
 * TO DO:
 *	software debounce where required
 *		(probably requires user process)
 *	revised interface to telephony apps (distinguish phone board effects)
 *	track Mute state or add MuteOn/MuteOff keys
 *	line monitoring code: idle, cid, cw/cid, speakerphone
 *		(use single line monitor)
 *	program DTMF tones on cold start for use after warm start
 *	correct locking/ilocking/qlocking
 *	eliminate :/.*_run variables
 *	error checking in CID parsing
 *	better CID/CWCID implementation (reduce kprocs, Rendez, shared variable)
 *	ring volumes
 *	dial out via connect requires mute if handsfree but not handset
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"
#include	"devtad.h"
#include	"dtad6471c.h"
#include	"i2c.h"

enum {
	UMEC = 1,	/* 0 for SWORD (incomplete) */

	/* parameters */
	LINE_VOL = 50,	/* telephone line volume */
	DTMF_VOL = 10,	/* tone volume (in EasyTAD units) */
	DTMF_WAIT = 100,	/* time in msec between tone-on/tone-off */
};

#define DPRINT	if(taddebug) print

typedef struct Cons Cons;
typedef struct Led Led;
typedef struct PhoneLine PhoneLine;
typedef struct Tad Tad;
typedef struct Tflash Tflash;
typedef struct Tmsg Tmsg;

static Rune chpad[] =  {
	0,	/* key not pressed */
	'1',	'2',	'3',	'4',	'5',
	'6',	'7',	'8',	'9',	'0',
	C_flash,	/* flash */
	'#',	'*',
	'P',	/* ? */
};
static int taddebug = 0;
static int codec = 3;
static int onspeaker, onhandset;
static char Eunimp[] = "unimplemented";
static char Ebadreq[] = "bad control message";
static char *bools[2] = {"off", "on"};

/******************************************************************************/
/* i2c.h */

enum {
	I2C_MUTE_SPKR=		0,	/* out - mute speaker */
	I2C_MUTE_HEADSET=	1,	/* out - mute handset */
	I2C_HANDFREE=		2,	/* in  - PB Handfree Switch/I2C_HEAD_SENSE */
	I2C_VMAIL=		3,	/* out - voice mail LED (1 = off, 0 = on) */
	I2C_TAD_FL_WP=		4,	/* out - flash wp */
	I2C_CID=		5,	/* out - CID enable */
	I2C_LCD_BACK_EN=	7,	/* out - LCD vEE enable (0 = on, 1 = off) */
	I2C_LCD_YEE_EN=		6,	/* out - LCD inverter on (1 = on, 0 = off) */
};

/******************************************************************************/

static void speaker(int on);
static void handset(int on);

struct Led
{
	char*	name;
	int	value;
	int	pin;
	int	sense;
};

static Led	umec_leds[] = {
	{"Msg-bin",	0,	I2C_VMAIL, 0},
	{nil},
};

struct Cons
{
	char*    name;
	char*    hook_status;
	char*    ringer_status;
	int      ringer_volume;
	char*    speaker_status;
	int      speaker_volume;
	char*    handset_status;
	int      handset_volume;
	char     mute;
	char     hold;
	Led*	leds;
	Queue*   eventq;
} thecons = {
	"EasyTAD",	
	"on",
	"off",
	100,
	"off",
	80,
	"off",
	5,
	0,
	0,
	umec_leds,
	nil,
};

struct PhoneLine {
	char 	*address;
	Queue	*eventq;

	QLock	listenq;
	Rendez	listenr;
} theline;

enum {	/* TAD hardware states */ 
	/* mutually exclusive states */
	Idling = 0,
	Playing,
	Recording,
	Monitoring,
	Sounding,
	Writing,
	Reading,

	/* concurrently active devices */
	Linemon =	1<<0,
	Speaker =	1<<1,
	Handset =	1<<2,
};

struct Tad {
	QLock;
	int	state;
	int	active;
};

struct Tmsg {
	int	n;		/* device's idea of message's number */
	int	deleted;
	int	recording;
	long	size;
	long	mtime;
};

struct Tflash {
	/* message flash */
	QLock;
	int	nmsg;
	Tmsg	*msg[128];
	int	needgc;
	int	full;
	int	timeavail;
	int	recording;
};

static	Tad	thetad;
static	Tflash	msgflash;

/*
 * GPIO Interfaces (should move?)
 */

/* GPIO Lines Associated With Telephony (UMEC variant) */
enum {
	GPIO_HANDSET=     (1 << 1),	/* in  - handset switch */
	GPIO_HOLD=        (1 << 16),    /*  */
	GPIO_HFREE=        (1 << 17),    /* in  - HandFree In check */
	GPIO_ENMUTE=      (1 << 18),    /* out - ENABLE MUTE (unclear what this does) */
	GPIO_MUTE=        (1 << 19),    /* in  - Mute DETECT */
	GPIO_PCLK=        (1 << 20),    /* in  - Phone board clock */
	GPIO_PPAD=        (1 << 21),    /* in  - Phone pad data in */
	GPIO_RING=        (1 << 23),    /* in  -  */
	GPIO_HOOK=        (1 << 24),    /* out -  */
};

static struct {	/* phone board and physical line state */
	Lock;
	ulong	state;
} phone;


static void
line_hook(int offline)
{
	GpioReg *g;

	ilock(&phone);
	g = GPIOREG;
	g->grer &= ~GPIO_RING;
	g->gfer &= ~GPIO_RING;
	if(offline)
		g->gpcr = GPIO_HOOK;
	else
		g->gpsr = GPIO_HOOK;
	iunlock(&phone);
	delay(250);
	ilock(&phone);
	/* had ringstate = 0 */
	g->grer |= GPIO_RING;
	g->gfer |= GPIO_RING;
	iunlock(&phone);
}

static int
isonline(void)
{
	ulong v;

	ilock(&phone);
	v = (phone.state ^ GPIO_HANDSET) | (GPIOREG->gplr & GPIO_HOOK);
	iunlock(&phone);
	return v & (GPIO_HANDSET | GPIO_HFREE | GPIO_HOOK);
}

static int
ishandseton(void)
{
	return (GPIOREG->gplr & GPIO_HANDSET)==0;
}

static char *
onoff(int v)
{
	if(v)
		return "on";
	return "off";
}

/*
 *	I2C Interfaces (should move?)
 */
static void
cid_enable(int on)
{
	if (!on)
		i2c_clrpin(I2C_CID);
	else
		i2c_setpin(I2C_CID);
}

static void
speaker(int on)
{
	onspeaker=on;
	if (on)
		i2c_clrpin(I2C_MUTE_SPKR);
	else
		i2c_setpin(I2C_MUTE_SPKR);
}

static void
handset(int on)
{
	onhandset=on;
	if (on)
		i2c_clrpin(I2C_MUTE_HEADSET);
	else
		i2c_setpin(I2C_MUTE_HEADSET);
}

static void
handfree(int on)
{
	int hf;

	hf = (GPIOREG->gplr & GPIO_HFREE) != 0;
	if(on ^ hf){
		i2c_setpin(I2C_HANDFREE);
		delay(300);
		i2c_clrpin(I2C_HANDFREE);
	}
}

static void
telonline(int on)
{
	line_hook(!on);
	if(on){
		if(!ishandseton()){
			handfree(1);
			//handset(0);
		}
	}else
		handfree(0);
}

static void
tadflash_protect(int on)
{
	if (on)
		i2c_setpin(I2C_TAD_FL_WP);
	else
		i2c_clrpin(I2C_TAD_FL_WP);
}

static void
led_set(Led *led, int on)
{
	if(((led->value = on) != 0) ^ led->sense)
		i2c_clrpin(led->pin);
	else
		i2c_setpin(led->pin);
}

/*
 * Caller ID
 */
typedef struct Cid Cid;
struct Cid {
	int	len;
	uchar	data[128];
	char	str[128];
};

static int parse_cid(Cid*);
static	Cid	lastcid;
static Rendez     cidreadr;
static int cid_run = 0;

static int
cidwant(void *v)
{
	return *(int*)v != 0;
}

static void
cid_reader(void*)
{
	Cid *c;

	setpri(PriHi);

	for (;;) {
		ulong t0, ost;
		
		/* wait for ring to wake up */
		sleep(&cidreadr, cidwant, &cid_run);

		/* start running */	
		qlock(&thetad);
		if(waserror()){
			qunlock(&thetad);
			nexterror();
		}

		/* clear out old CID data */
		c = &lastcid;
		c->len = 0;
		c->str[0] = '\0';
		memset(c->data, 0, sizeof(c->data));

		/* enable idle mode (CID detection only works from idle mode) */
		dtad_idle();
		cid_enable(1);

		t0 = timer_start();
		ost = ms2tmr(6000);    /* nominal 4 sec. between 1st and 2nd ring with CID plus 2 sec. ring */
		do {
			/* read in CID data */
			int status;
			do {
				status = linemonitor(1, 0, 0);
				if (status & DCID_ND)
					break;
				c->data[c->len++] = status & DCID_Data;
				
			} while (c->len < sizeof(c->data));

			/* check timeout */
			if (timer_ticks(t0) > ost)
				break;

			/* check if all data has been processed */
			if (!cid_run) 
				break;

		} while (cid_run && c->len < sizeof(c->data));

		/* clean up */
		cid_enable(0);

		poperror();
		qunlock(&thetad);
	}
}

/*
 * Call Waiting Caller ID
 */

static Rendez cwcidreadr;
static int cwcid_run;

static void
cwcid_reader(void*)
{
	Cid *c;
	char buf[40];
	int n;

	setpri(PriHi);

	for (;;) {
		ulong t0, ost;

		/* wait for call setup */
		sleep(&cwcidreadr, cidwant, &cwcid_run);

		tsleep(&up->sleep, return0, 0, 1000);
	loop:
		/* wait for CAS */
		DPRINT("cwcid_reader: waiting for CAS\n");
		do {
			int status;

			qlock(&thetad);
			status = spkr_monitor();
			qunlock(&thetad);
			if(status & ToneDetected){
				DPRINT("cwcid: call progress #%4.4ux\n", status);
				if(status & ToneMask){
					n = snprint(buf, sizeof(buf), "call-progress %d\n", status&0x2F);
					qproduce(theline.eventq, buf, n);
				}
			} else if(status & ExtendedTone){
				DPRINT("cwcid: extended tone: #%4.4ux\n", status);
				if((status & ToneMask) == 0xF)
					break;
				n = snprint(buf, sizeof(buf), "ext-tone %d\n", status&0xF);
				qproduce(theline.eventq, buf, n);
			} else if(status&ToneMask){
				DPRINT("cwcid: tone #%4.4ux\n", status&ToneMask);
				n = snprint(buf, sizeof(buf), "tone %d\n", status&ToneMask);
				qproduce(theline.eventq, buf, n);
			}

			/* wait 10 ms */
			tsleep(&up->sleep, return0, 0, 10);

		} while (cwcid_run);
		if (!cwcid_run){
			DPRINT("cwcid: cancelled\n");
			continue;
		}
		DPRINT("cwcid: active\n");
		
		qlock(&thetad);
		if(waserror()){
			qunlock(&thetad);
			nexterror();
		}

		/* mute mic and speaker */
		if (thetad.active & Speaker)
			speaker(0);
		if (thetad.active & Handset)
			handset(0);
		
		/* send 60 ms of DMTF "A" for the ACK */
		DPRINT("cwcid_reader: sending ACK\n");
		spkr_tgen(DTMF_VOL, 0x0d);
		delay(60);	
		spkr_tgen(0, 0);
		spkr_stat();

		/* clear out old CID data */
		c = &lastcid;
		c->len = 0;
		c->str[0] = '\0';
		memset(c->data, 0, sizeof(c->data));

		/* read in CID Data */
		t0 = timer_start();
		ost = ms2tmr(3500);    /* nominal 4 sec. between 1st and 2nd ring with CID plus 2 sec. ring */
		DPRINT("cwcid_reader: reading CID data\n");
		dtad_idle();
		do {
			/* read in CID data */
			int status;
			do {
				status = linemonitor(1, 0, 1);
				if (status & DCID_ND)
					break;   
				DPRINT("cwcid_reader: status = 0x%x\n", status);
				c->data[c->len++] = status & DCID_Data;
				
			} while (c->len < sizeof(c->data));

			/* check timeout */
			if (timer_ticks(t0) > ost) {
				DPRINT("cwcid_reader: timeout\n");
				break;
			}

			/* */
			if (!cwcid_run) {
				DPRINT("cwcid_reader: cancelled\n");
				break;
			}

		} while (c->len < sizeof(c->data));
		speakerphone(1, 1, 3, 2, 0);

		/* unmute mic and speaker */
		if (thetad.active & Speaker)
			speaker(1);
		if (thetad.active & Handset)
			handset(1);

		poperror();
		qunlock(&thetad);

		/* parse CID data and send event */               
		DPRINT("cwcid_reader: parsing CID data:\n%s\n", (char*)c->data);
		parse_cid(c);
		
		if (c->data[0]) {
			char obuf[128];
			int n;
			
			n = snprint(obuf, sizeof(obuf), "incoming %s\n", (char*)c->data);
			qproduce(theline.eventq, obuf, n);
		} else
			qproduce(theline.eventq, "incoming\n", 9);
	}
}

static int
parse_cid(Cid *c)
{
	char datetime[12];
	char number[16];
	char name[64];

	if (c->len <= 0)
		return 0;
	
	/* check the checksum */
	{
		int i, csum = 0;

		for (i=0; i<c->len; i++)
			csum += c->data[i];
		
		if (csum & 0xff) {
			print("BAD CID Checksum (0x%x) len = %d, data = %s\n", csum, c->len, (char*)c->data);
			return 0;			
		}
	}

	datetime[0] = number[0] = name[0] = 0;

	/* parse the message */
	if (c->data[0] == 0x04) { /* SDMF */
		
		uchar *s = c->data+1;
		int len = *s++;
		
		if (len < 9) {
			print("SDMF CID message too short\n");
			return 0;
		}

		memmove(datetime, s, 8);
		datetime[8] = 0;
		s += 8;
		len -= 8;

		memmove(number, s, len);
		number[len] = 0;

	} else if (c->data[0] == 0x80) { /* MDMF */
		
		int len = c->data[1];
		int pos = 2;

		while (pos < (len + 2)) {

			int type = c->data[pos++];
			int slen = c->data[pos++];

			switch (type) {
				
			case 1: /* datetime */
				memmove(datetime, &c->data[pos], slen);
				datetime[slen] = 0;
				break;
				
			case 2: /* number */
				memmove(number, &c->data[pos], slen);
				number[slen] = 0;
				break;
			
			case 4: /* no number */
				if (c->data[pos] == 'O')
					strncpy(number, "Out of area", sizeof(number));
				else if (c->data[pos] == 'P')
					strncpy(number, "Private", sizeof(number));
				else
					print("Unknown CID number reason <%c>\n", c->data[pos]);
				break;
	
			case 7: /* name */
				memmove(name, &c->data[pos], slen);
				name[slen] = 0;
				break;

			case 8: /* no name */
				if (c->data[pos] == 'O')
					strncpy(name, "Out of area", sizeof(name));
				else if (c->data[pos] == 'P')
					strncpy(name, "Private", sizeof(name));
				else
					print("Unknown CID name reason <%c>\n", c->data[pos]);
				break;
				
			default:
				print("Unknown MDMF CID message field type 0x%x\n", type);
				break;
			}
			
			pos += slen;
		}
	} else {
		print("Unknown CID message type 0x%x\n", c->data[0]);
		return 0;
	}

	/* debugging */
	DPRINT("CID Data:\n");
	DPRINT("number = %s\n", number);
	DPRINT("datetime = %s\n", datetime);
	DPRINT("name = %s\n", name);

	/* reassemble string */
	snprint((char *)c->str, sizeof(c->str), "%s %s %s", number, datetime, name);
	return 1;
}

/*
 * Incoming calls and button presses
 */

/*
 * Ring Input:
 *   ___________________________________________________________________________________
 *     |||||||||                 |||||||||                 |||||||||                 ||| 
 *     < 2 sec ><     4 sec.    >< 2 sec ><     4 sec.    >< 2 sec ><     4 sec.    >      
 *      1st Ring   CID Data       2nd Ring
 */
static void
ringintr(Ureg *, void *)
{
	static ulong lastring = 0;
	static int newring = 0;
	int ms;
	ulong t;
	Cid *c;

	t = timer_start();
	GPIOREG->gedr = GPIO_RING;	

	ilock(&phone);
	ms = tmr2ms(t-lastring);
	lastring = t;

	if (ms > 6200) {						
		DPRINT("NEWCALL\n");
		newring = 1;

		/* start CID reader */
		cid_run = 1;
		wakeup(&cidreadr);

	} else if (ms > 2100) {					
		DPRINT("RING\n");
		if (newring) {
			newring = 0;
			wakeup(&theline.listenr);
		}

		c = &lastcid;

		/* stop cid_reader */	/* BUG: race */
		if (cid_run) {
			cid_run = 0;
			parse_cid(c);
		}

		DPRINT("CID [%s]\n", c->str);
		/* send incoming event */
		if (c->str[0]) {
			char obuf[128];
			int n = snprint(obuf, sizeof(obuf), "incoming %s\n", c->str);
			qproduce(theline.eventq, obuf, n);
		} else {
			qproduce(theline.eventq, "incoming\n", 9);
		}
	} 

	iunlock(&phone);
}

static void line_hook(int on);
static void handfree(int on);

static void
putconsc(Queue *q, Rune r)
{
	char buf[UTFmax];
	int n;

	n = runetochar(buf, &r);
	if(n != 0)
		qproduce(q, buf, n);
}

/*
 * handset up/down and `handsfree' (speaker key) are handled
 * together to ensure the application sees the right state in terms of
 * the Inferno Telephony Interface messages (HandsetOn/Off, SpeakerOn/Off).
 */
static void
handsetintrs(Ureg *, void *)
{
	ulong val, e, diff;
	Rune r;

	/* debounce? */
	ilock(&phone);
	for(;;){
		e = GPIOREG->gedr & (GPIO_HANDSET | GPIO_HFREE);
		val = GPIOREG->gplr & (GPIO_HANDSET | GPIO_HFREE);
		if(e == 0)
			break;
		GPIOREG->gedr = e;	/* acknowledge them (should be earlier once debounced?) */
		delay(6);/*microdelay(100);*/
	}
	diff = phone.state ^ val;
	DPRINT("handsetintrs(): old=%lux e=%lux new=%lux diff=%lux lr=%lux\n", phone.state, e, val, diff, GPIOREG->gplr);
	phone.state = (phone.state & ~(GPIO_HANDSET | GPIO_HFREE)) | val;
	if(diff & GPIO_HANDSET){
		if(val & GPIO_HANDSET){
			DPRINT("HandsetOff\n");
			r = HandsetOff;		/* handset switched off since on hook */
		}else{
			DPRINT("HandsetOn\n");
			r = HandsetOn;
		}
		r ^= HandsetOn^HandsetOff;	/* BUG: the Limbo code has the sense reversed */
		putconsc(thecons.eventq, r);
	}
	if(diff & GPIO_HFREE){
		if(val & GPIO_HFREE){
			DPRINT("SpeakerOn\n");
			r = SpeakerOn;
		}else{
			DPRINT("SpeakerOff\n");
			r = SpeakerOff;	/* will have sent HandsetOn first if lifting handset switched p/b mode */
		}
		putconsc(thecons.eventq, r);
	}
	iunlock(&phone);
}

static void
mutekeyintr(Ureg*, void*)
{
	ulong lr = GPIOREG->gplr;
	Rune r;

	/* TO DO: debounce */
	DPRINT("muteintr(): lr=%lux\n", lr);
	if((lr & GPIO_MUTE) == 0){
		DPRINT("Mute+\n");
		r = C_mute;
	}else{
		DPRINT("Mute-");
		r = C_mute;
	}
	putconsc(thecons.eventq, r);
	GPIOREG->gedr = GPIO_MUTE;
}

static void
keypadintr(Ureg *, void *)
{
	static int ipad=4, kpad=0;
	static ulong tpad;
	GpioReg *g;

	/* TO DO: debounce */

	g = GPIOREG;

	/* elapse too much time from last pulse */
	if (ipad < 4 && tmr2ms(timer_start()-tpad) > 100){
		ipad = 4;
		kpad = 0;
	}

	ipad--;
	if(g->gplr & GPIO_PPAD)
		kpad |= 1<<ipad;
	g->gedr = GPIO_PCLK;

	if (ipad <= 0) {
		if (kpad > 0 && kpad <= nelem(chpad)) {
			putconsc(thecons.eventq, chpad[kpad]);
			DPRINT("keypadintr(): %d <%x>\n", kpad, chpad[kpad]);
		}
		ipad = 4;
		kpad = 0;
	}
	tpad = timer_start();   /* keep last pulse time */
}


/*
 * 	Intermediate Functions
 */

static void
line_volume(int level)
{
	if (level == 0)
		level = 21;	/* mute */
	else
		level = 20-(level/5);
	spkr_param2(SPKRPARAM_LINEVOL, level);
}

static void
flash_hook(void)
{
	if (GPIOREG->gplr & GPIO_HOOK) 
		return;

	line_hook(1);
	delay(500);	
	line_hook(0);
	line_volume(LINE_VOL);
}

static void
speaker_volume(int level)
{
	if (level == 0)
		level = 21;	/* mute */
	else
		level = 20-(level/5);	
	spkr_param2(SPKRPARAM_SPKRVOL, level);
}

static void
mode(int dev, int disable)
{
	qlock(&thetad);
	switch(dev) {
	case Speaker:
		if (disable) {
			thetad.active&=~Speaker;
			speaker(0);
			if (ishandseton()) {
				speaker_volume(thecons.handset_volume);
				thetad.active |= Handset;
				handset(1);
			} 
			cwcid_run = 0;         /* stop Call Waiting/CID reader */
		} else {
			thetad.active|=Speaker;
			thetad.active &= ~Handset;
			handset(0);
			speakerphone(1, 1, 3, 2, 0);
			line_volume(LINE_VOL);
			speaker_volume(thecons.speaker_volume);
			speaker(1);
			cwcid_run = 1;			
			wakeup(&cwcidreadr);   /* start Call Waiting/CID reader */
		}
		break;
	case Handset:
		if (disable) {
			thetad.active &= ~Handset;
			handset(0);
			cwcid_run = 0;         /* stop Call Waiting/CID reader */
		} else {
			thetad.active |= Handset;
			thetad.active &= ~Speaker;
			speaker(0);
			speakerphone(1, 1, 3, 2, 0);
			line_volume(LINE_VOL);
			speaker_volume(thecons.handset_volume);
			handset(1);
			cwcid_run = 1;
			wakeup(&cwcidreadr);   /* start Call Waiting/CID reader */
		}
	}
	qunlock(&thetad);
}

static void
tonegen( int tone )
{
	qlock(&thetad);
	if(waserror()){
		qunlock(&thetad);
		nexterror();
	}
	if ((tone >= '1') && (tone <= '9'))
		tone = tone-'0';
	else
		switch(tone) {
		case '#': 
			tone = 0xC;
			break;
		case '0':
			tone = 0xB;
			break;
		case '*': 
			tone = 0xA;
			break;
		case '!': 
			flash_hook();
			tone = 0;
			break;
		case ',':
			tsleep(&up->sleep, return0, 0, 1000);
			tone = 0;
			break;
			/* only for cold start? */
		case 'A':
		case 'a':
			tone = 0x0d;
			break;
		case 'B':
		case 'b':
			tone = 0x0e;
			break;
		case 'C':
		case 'c':
			tone = 0x0f;
			break;
		case 'D':
		case 'd':
			tone = 0x10;
			break;
		default:
			tone = 0;
			break;
		}
	if (tone) {
		spkr_tgen(DTMF_VOL, tone);
		delay(DTMF_WAIT);	
		spkr_tgen(0, 0);
		delay(DTMF_WAIT);	
		spkr_stat();
	}
	poperror();
	qunlock(&thetad);
}

static void
ring( int tone, int dur, int sep )
{
	int count;
	int maxcount;

	maxcount = dur / sep;
	qlock(&thetad);
	if(waserror()){
		dtad_idle();
		qunlock(&thetad);
		nexterror();
	}
	speaker(1);
	handset(0);
	line_volume(0);
	speaker_volume(thecons.ringer_volume);
	for (count = 0;count < maxcount; count++) {
//		spkr_tgen(((101-thecons.ringer_volume)/16/2), tone);
		spkr_tgen(DTMF_VOL, tone);
		/* pause a bit */
		delay(sep);	/* BUG: should use tsleep */
		/* end the DTMF tone */
		spkr_tgen(0, 0);
		delay(sep);
	}
	
	if(thetad.active & Speaker)
		speaker_volume(thecons.speaker_volume );
	else
		speaker(0);
	if (thetad.active & Handset) {
		handset(1);
		speaker_volume(thecons.handset_volume);
	}
	line_volume(LINE_VOL);
	poperror();
	qunlock(&thetad);
}

/*
 * Namespace Interface
 */

enum
{
	Qtopdir		= 1,	/* top level directory */
	Qprotodir,		/* directory for a protocol */
	Qclonus,
	Qconvdir,		/* directory for a conversation */
	Qdata,			/* this is body for message & prompt */
	Qctl,
	Qstatus,		/* this is type for message & prompt */
	Qremote,
	Qlocal,
	Qevent,			/* for tel, prompt, msg flash, & cons */
	Qlisten,

	Ptel		= 0,		
	Pcons,
	Ptgen,
	Pmsg,

	MAXPROTO	= 4,
	MAXCONV		= 128
};

#define TYPE(x) 	((x).path & 0xf)
#define CONV(x) 	(((x).path >> 4)&(MAXCONV-1))
#define PROTO(x) 	(((x).path >> 16)&0xff)
#define QID(p, c, y) 	(((p)<<16) | ((c)<<4) | (y))

/* telephony service function prototypes */
static Chan *	telopen(Chan *c, int omode);
static int 	telgen(Chan *, Dirtab *, int, int, Dir *);
static long	telread(Chan*, void*, long, ulong);
static long	telwrite(Chan*, void*, long, ulong);
static int 	consgen(Chan *, Dirtab *, int, int, Dir *);
static long	consread(Chan*, void*, long, ulong);
static long	conswrite(Chan*, void*, long, ulong);
static int 	tgengen(Chan *, Dirtab *, int, int, Dir *);
static long	tgenread(Chan*, void*, long, ulong);
static long	tgenwrite(Chan*, void*, long, ulong);
static int      msggen(Chan *, Dirtab *, int, int, Dir *);
static long	msgread(Chan*, void*, long, ulong);
static long	msgwrite(Chan*, void*, long, ulong);
static void	msgdsync(Chan*);

typedef struct Proto	Proto;
typedef struct Conv	Conv;

struct Conv
{
	Lock;

	int	        x;
	Ref	        r;

	int             perm;
	char		owner[NAMELEN];
	char*		state;
	char		local[NAMELEN];
	char		remote[NAMELEN];

	Queue		*event;
	QLock		*listenq;
	Rendez		*listenr;

	Conv		*next;

	Proto*		p;
};

struct Prototab
{
	int 	(*gen)(Chan *, Dirtab *, int, int, Dir *);
	Chan*	(*open)(Chan*, int);
	void	(*close)(Chan*);
	long	(*read)(Chan*, void*, long, ulong);
	long	(*write)(Chan*, void*, long, ulong);
	void	(*dirsync)(Chan*);
} prototab[] = {
	{ 	/* tel service */
		telgen,	
		telopen,
		nil,
		telread,
		telwrite,
		nil,
	},
	{	/* cons service */
		consgen,
		nil,
		nil,
		consread,
		conswrite,
		nil,
	},
	{	/* tgen service */
		tgengen,
		nil,
		nil,
		tgenread,
		tgenwrite,
		nil,
	},
	{	/* msg service */
		msggen,
		nil,
		nil,
		msgread,
		msgwrite,
		msgdsync,
	},
};

typedef struct Prototab Prototab;

struct Proto
{
	Lock		l;
	int             x;
	int             stype;
	char		name[NAMELEN];
	int             nc;
	int             maxconv;
	Conv**		conv;
	Qid             qid;

	Prototab;
};

static	int		np;
static	Conv*	incall;
static	Proto	proto[MAXPROTO];
static	Conv*	protoclone(Proto*, char*, int);
static	Conv*	newconv(Proto*, Conv **);

static void
setremote(Conv *c, char *r)
{
	strncpy(c->remote, r, sizeof(c->remote)-1);
}

static void
setlocal(Conv *c, char *r)
{
	strncpy(c->local, r, sizeof(c->local)-1);
}

static int
tadgen(Chan *c, Dirtab *d, int nd, int s, Dir *dp)
{
	Qid q;
	Conv *cv;
	char name[16], *p;

	USED(nd);
	USED(d);
	q.vers = 0;
	switch(TYPE(c->qid)) {
	case Qtopdir:
		if (s < np) {
			q.path = QID(s, 0, Qprotodir)|CHDIR;
			devdir(c, q, proto[s].name, 0, "network", CHDIR|0555, dp);
			return 1;
		}
	case Qprotodir:
		if(s == 0 && proto[PROTO(c->qid)].dirsync)
			proto[PROTO(c->qid)].dirsync(c);
		if(s < proto[PROTO(c->qid)].nc) {
			cv = proto[PROTO(c->qid)].conv[s];
			sprint(name, "%d", s);
			q.path = QID(PROTO(c->qid), s, Qconvdir)|CHDIR;
			devdir(c, q, name, 0, cv->owner, CHDIR|0555, dp);
			return 1;
		}
		s -= proto[PROTO(c->qid)].nc;
		switch(s) {
		default:
			return -1;
		case 0:
			p = "clone";
			q.path = QID(PROTO(c->qid), 0, Qclonus);
			break;
		}
		devdir(c, q, p, 0, "network", 0555, dp);
		return 1;
	case Qconvdir:
		return proto[PROTO(c->qid)].gen(c, d, nd, s, dp);
	}
	return -1;
}

static void
newproto(char *name, int type, int maxconv, Prototab tab)
{
	int l;
	Proto *p;

	if(np >= MAXPROTO) 
		panic("increase MAXPROTO");

	p = &proto[np];
	strcpy(p->name, name);
	p->stype = type;
	p->qid.path = CHDIR|QID(np, 0, Qprotodir);
	p->x = np++;
	p->maxconv = maxconv;
	l = sizeof(Conv*)*p->maxconv;
	p->conv = malloc(l);
	if(p->conv == 0)
		panic("no memory");
	memset(p->conv, 0, l);
	
	p->gen = tab.gen;
	p->open = tab.open;
	p->close = tab.close;
	p->read = tab.read;
	p->write = tab.write; 

}

void
tadinit(void)
{
	GpioReg *g;

	DPRINT("Initializing Telephony Hardware\n");
	tadflash_protect(0);
	dtad_init();
	config_detector(1);

	/* set up theline and thecons queues */
	theline.eventq = qopen(1024, 1, 0, 0);
	thecons.eventq = qopen(1024, 1, 0, 0);

	speaker(0);
	handset(0);
	speakerphone(1, 1, 3, 2, 0);		/* r/t, t/r, line sensitivity, mic sensitivity, priority */
//	spkr_volume(2, 7, 0);			/* All set to 0 dB - Line Vol., Speaker Vol., Looop Att. */
	spkr_volume(0, 2, 2);
	line_volume(LINE_VOL);

	intrenable(1, handsetintrs, nil, BusGPIO);	/* phoneboard handset up/down */
	intrenable(17, handsetintrs, nil, BusGPIO);	/* phoneboard handfree key */
	intrenable(23, ringintr, nil, BusGPIO);
	intrenable(20, keypadintr, nil, BusGPIO);	/* receive phoneboard keypad input */
	intrenable(19, mutekeyintr, nil, BusGPIO);

	/* configure GPIO pins */
	g = GPIOREG;
	g->gfer |= GPIO_HANDSET | GPIO_RING | GPIO_HFREE | GPIO_MUTE;
	g->gfer |= GPIO_PCLK;
	g->grer |= GPIO_HANDSET | GPIO_RING | GPIO_HFREE | GPIO_MUTE;
	g->gedr = GPIO_HANDSET | GPIO_RING | GPIO_PCLK | GPIO_HFREE | GPIO_MUTE;
	g->gpdr &= ~(GPIO_HANDSET | GPIO_RING | GPIO_PCLK | GPIO_HFREE | GPIO_PPAD | GPIO_MUTE);
	g->gpdr |= GPIO_HOOK;
	g->gafr &= ~(GPIO_HANDSET | GPIO_RING | GPIO_PCLK | GPIO_HFREE | GPIO_PPAD | GPIO_MUTE);
	phone.state = g->gplr & (GPIO_HANDSET | GPIO_HFREE);

	/* set up protocols */
	newproto("tel", Ptel, 5, prototab[Ptel]);
	newproto("cons", Pcons, 1, prototab[Pcons]);
	newproto("tgen", Ptgen, 1, prototab[Ptgen]);
	newproto("msg", Pmsg, 64, prototab[Pmsg]);

	/* start CID reader task */	
	kproc("CID reader", cid_reader, 0);
	kproc("CW/CID reader", cwcid_reader, 0);
}

static Chan *
tadattach(void *spec)
{
	Chan *c;

	c = devattach('P', spec);
	c->qid.path = QID(0, 0, Qtopdir)|CHDIR;
	c->qid.vers = 0;
	return c;
}

static int
tadwalk(Chan* c, char* name)
{
	Path *op;

	if(strcmp(name, "..") == 0){
		switch(TYPE(c->qid)){
		case Qtopdir:
		case Qprotodir:
			c->qid.path = QID(0, 0, Qtopdir)|CHDIR;
			c->qid.vers = 0;
			break;
		case Qconvdir:
			c->qid.path = QID(PROTO(c->qid), 0, Qprotodir)|CHDIR;
			c->qid.vers = 0;
			break;
		default:
			panic("tadwalk %lux", c->qid.path);
		}
		op = c->path;
		c->path = ptenter(&syspt, op, name);
		decref(op);
		return 1;
	}

	return devwalk(c, name, nil, 0, tadgen);
}

static void
tadstat(Chan *c, char *db)
{	
	devstat(c, db, 0, 0, tadgen);
}

static Chan *
tadopen(Chan *c, int omode)
{
	Proto *p;
	int perm;
	Conv *cv, *lcv;

	SET(cv);
	SET(lcv);
	USED(cv);
	USED(lcv);

	if (proto[PROTO(c->qid)].open != 0) {
		return proto[PROTO(c->qid)].open( c, omode );
	}

	perm = 0;
	omode &= 3;
	switch(omode) {
	case OREAD:
		perm = 4;
		break;
	case OWRITE:
		perm = 2;
		break;
	case ORDWR:
		perm = 6;
		break;
	}

	switch(TYPE(c->qid)) {
	default:
		break;
	case Qevent:
	case Qtopdir:
	case Qprotodir:
	case Qconvdir:
	case Qlisten:
	case Qstatus:
	case Qremote:
	case Qlocal:
		if(omode != OREAD)
			error(Eperm);
		break;
	case Qclonus:
		p = &proto[PROTO(c->qid)];
		cv = protoclone(p, "inferno", -1);
		if(cv == 0)
			error(Enodev);
		c->qid.path = QID(p->x, cv->x, Qctl);
		c->qid.vers = 0;
		break;
	case Qdata:
	case Qctl:
		p = &proto[PROTO(c->qid)];
		lock(&p->l);
		cv = p->conv[CONV(c->qid)];
		lock(&cv->r);
		if((perm & (cv->perm>>6)) != perm) {
			if(strcmp("inferno", cv->owner) != 0 ||
		 	  (perm & cv->perm) != perm) {
				unlock(&cv->r);
				unlock(&p->l);
				error(Eperm);
			}
		}
		cv->r.ref++;
		if(cv->r.ref == 1) {
			memmove(cv->owner, "inferno", NAMELEN);
			cv->perm = 0660;
		}
		unlock(&cv->r);
		unlock(&p->l);
		break;

	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static void
tadclose(Chan *c)
{
	Conv *cc;

	if (proto[PROTO(c->qid)].close != 0) {
		proto[PROTO(c->qid)].close( c );
		return;
	}
		
	switch(TYPE(c->qid)) {
	case Qdata:
	case Qctl:
		if((c->flag & COPEN) == 0)
			break;
		cc = proto[PROTO(c->qid)].conv[CONV(c->qid)];
		if (decref(&cc->r) != 0) {
			break;
		}
		
		cc->perm = 0666;
		cc->state = "Closed";
		setlocal(cc, "0");
		setremote(cc, "0");
		break;
	}
}

static long
tadread(Chan *ch, void *a, long n, ulong offset)
{
	switch(TYPE(ch->qid)) {
	default:
		return proto[PROTO(ch->qid)].read(ch, a, n, offset);
	case Qprotodir:
	case Qtopdir:
	case Qconvdir:
		return devdirread(ch, a, n, 0, 0, tadgen);
	}
}

static long
tadwrite(Chan *ch, void *a, long n, ulong offset)
{
	return proto[PROTO(ch->qid)].write(ch, a, n, offset);
}

static Conv*
protoclone(Proto *p, char *user, int nfd)
{
	Conv *c, **pp, **ep, **np;

	USED(nfd);
	SET(np);
	USED(np);

	c = 0;
	
	lock(&p->l);
	if(waserror()) {
		unlock(&p->l);
		nexterror();
	}
	
	ep = &p->conv[p->maxconv];
	for(pp = p->conv; pp < ep; pp++) {
		c = *pp;
		if(c == 0) {
			
			c = newconv(p, pp);
			
			break;
		}
		lock(&c->r);
		if(c->r.ref == 0) {
			c->r.ref++;
			break;
		}
		unlock(&c->r);
	}
	if(pp >= ep) {
		unlock(&p->l);
		poperror();
		return 0;
	}

	strcpy(c->owner, user);

	c->perm = 0660;
	c->state = "Closed";
	setlocal(c, "0");
	setremote(c, "0");

	switch (p->x) {
	case Ptel:
		c->event = theline.eventq;
		c->listenq = &theline.listenq;
		c->listenr = &theline.listenr;
		break;
	case Pcons:
		c->event = thecons.eventq;
		break;
	};

	unlock(&c->r);
	unlock(&p->l);
	poperror();
	return c;
}

static Conv*
newconv(Proto *p, Conv **pp)
{
	Conv *c;

	*pp = c = malloc(sizeof(Conv));
	if(c == 0)
		error(Enomem);

	lock(&c->r);
	c->r.ref = 1;
	c->p = p;
	c->x = pp - p->conv;

 	unlock(&c->r);
	p->nc++;
	return c;
}

/* tel functionality */
static int 	
telgen(Chan *c, Dirtab *d, int nd, int s, Dir *dp) 
{
	Qid q;
	Conv *cv;
	char *p = nil;

	USED(nd,d);

	cv = proto[PROTO(c->qid)].conv[CONV(c->qid)];
	switch(s) {
	default:
		return -1;
	case 0:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qdata);
		devdir(c, q, "data", 0, cv->owner, cv->perm, dp);
		return 1;
	case 1:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qctl);
		devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
		return 1;
	case 2:
		p = "status";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qstatus);			
		break;
	case 3:
		p = "remote";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qremote);
		break;
	case 4:
		p = "local";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qlocal);
		break;
	case 5:
		p = "listen";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qlisten);
		break;
	case 6:
		p = "event";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qevent);
		break;
	}
	devdir(c, q, p, 0, cv->owner, 0444, dp);
	return 1;
}

static int
incoming(void *n)
{
	USED(n);
	return incall != nil;
}

static Chan *
telopen(Chan *c, int omode)
{
	Proto *p;
	int perm;
	Conv *cv, *lcv, *nc;

	SET(cv);
	SET(lcv);
	USED(cv);
	USED(lcv);

	perm = 0;
	omode &= 3;
	switch(omode) {
	case OREAD:
		perm = 4;
		break;
	case OWRITE:
		perm = 2;
		break;
	case ORDWR:
		perm = 6;
		break;
	}

	switch(TYPE(c->qid)) {
	default:
		break;
	case Qevent:
	case Qtopdir:
	case Qprotodir:
	case Qconvdir:
	case Qstatus:
	case Qremote:
	case Qlocal:
		if(omode != OREAD)
			error(Eperm);
		break;
	case Qclonus:
		p = &proto[PROTO(c->qid)];
		cv = protoclone(p, "inferno", -1);
		if(cv == 0)
			error(Enodev);
		c->qid.path = QID(p->x, cv->x, Qctl);
		c->qid.vers = 0;
		break;
	case Qdata:
	case Qctl:
		p = &proto[PROTO(c->qid)];
		lock(&p->l);
		cv = p->conv[CONV(c->qid)];
		lock(&cv->r);
		if((perm & (cv->perm>>6)) != perm) {
			if(strcmp("inferno", cv->owner) != 0 ||
		 	  (perm & cv->perm) != perm) {
				unlock(&cv->r);
				unlock(&p->l);
				error(Eperm);
			}
		}
		cv->r.ref++;
		if(cv->r.ref == 1) {
			memmove(cv->owner, "inferno", NAMELEN);
			cv->perm = 0660;
		}
		unlock(&cv->r);
		unlock(&p->l);
		if(TYPE(c->qid) == Qdata){
			telonline(1);
			line_volume(LINE_VOL);
		}
		break;
	case Qlisten:
		p = &proto[PROTO(c->qid)];
		lcv = p->conv[CONV(c->qid)];
 		USED(lcv);
		
		if(strcmp(lcv->state,"Announced") != 0)
			error("not announced");	

		nc = nil;
		while(nc == nil) {
			qlock(lcv->listenq);
			
			if(waserror()) {
				qunlock(lcv->listenq);
				nexterror();
			}
		
			sleep(lcv->listenr, incoming, lcv);
	
			lock(lcv);
			nc = incall;
			if(nc != nil){
				incall = nc->next;
			} else {
				/* find a free conversation */
				nc = protoclone(p, "inferno", -1);
				if(nc == nil) {
					unlock(lcv);
					error("no available conversations");
				}
			}
			c->qid = (Qid){QID(PROTO(c->qid), nc->x, Qctl), 0};
			unlock(lcv);

			qunlock(lcv->listenq);
			poperror();
		}	

		break;
	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static long	
telread(Chan *ch, void *a, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	char buf[160], *p;

	p = a;
	x = &proto[PROTO(ch->qid)];
	c = x->conv[CONV(ch->qid)];

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		sprint(buf, "%lud", CONV(ch->qid));
		return readstr(offset, p, n, buf);
	case Qremote:
		return readstr(offset, p, n, c->remote);
	case Qlocal:
		return readstr(offset, p, n, c->local);
	case Qstatus:
		snprint(buf, sizeof(buf), "%d %s %s %s\n", c->x, c->local, c->remote, c->state);
		return readstr(offset, p, n, buf);
	case Qdata:
		error(Eunimp);
		return -1;
	case Qevent:
		return qread(theline.eventq, a, n);
	}
}

static long	
telwrite(Chan*ch, void *a, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	int nf, count, tmp_speaker;
	char *fields[3], buf[128];

	USED(offset);
	x = &proto[PROTO(ch->qid)];
	c = x->conv[CONV(ch->qid)];
DPRINT("telwrite: qid %lud x %lux c %lux\n", PROTO(ch->qid), x, c);

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = 0;
		nf = parsefields(buf, fields, 3, " \n");
		if(nf < 1)
			error(Ebadarg);
		if(strcmp(fields[0], "connect") == 0){
			switch(nf) {
			default:
				error("bad args to connect");
				return -1;
			case 2:
				setremote(c,  fields[1]);
				setlocal(c, "local" );
				break;
			case 3:	
				setremote(c, fields[1]);
				setlocal(c, fields[2]);
				break;
			}
			if(UMEC){
				/* the phone board has its own ideas: force it on if need be */
				if(!ishandseton())
					handfree(1);
				line_hook(0);
				if((tmp_speaker=onspeaker)!=0)
					speaker(0);
			}else{
				line_hook(0);
				line_volume(LINE_VOL);
				tmp_speaker = 0;
			}
			delay(500);
			for (count = 0; c->remote[count]; count++){
				DPRINT("connect %c\n", c->remote[count]);
				tonegen(c->remote[count]);
			}
			if(tmp_speaker)
				speaker(1);
			c->state = "Dialing";
			return n;
		}
		if(strcmp(fields[0], "announce") == 0) {
			switch(nf){
			default:
				error("bad args to announce");
				return -1;
			case 2:
				if (strncmp(fields[1],"*", 1) != 0)
					error("can't announce specific local address");
				break;
			}
			c->state = "Announced";
			return n;
		}
		if(strncmp(fields[0], "bind", 4) == 0){
			error("Not Implemented");
			return -1;
		}
		if(strncmp(fields[0], "flash", 5) == 0){
			/* send a flash to the underlying device */
			flash_hook();
			return n;
		}
		if(strcmp(fields[0], "hangup") == 0){
			thecons.hold = 0;
			thecons.mute = 0;
			line_volume(LINE_VOL);
			setlocal(c, "0");
			setremote(c, "0");
			telonline(0);
			c->state = "Hungup";
			cwcid_run = 0;
			return n;
		}
		if(strcmp(fields[0], "debug") == 0)
			taddebug ^= 1;
		error(Ebadreq);
	case Qdata:
		error(Eunimp);
		return -1;
	}
}

/* cons functionality */
static int 	
consgen(Chan *c, Dirtab *d, int nd, int s, Dir *dp) 
{
	Qid q;
	Conv *cv;
	char *p = nil;

	USED(nd,d);

	cv = proto[PROTO(c->qid)].conv[CONV(c->qid)];
	switch(s) {
	default:
		return -1;
	case 0:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qdata);
		devdir(c, q, "data", 0, cv->owner, cv->perm, dp);
		return 1;
	case 1:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qctl);
		devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
		return 1;
	case 2:
		p = "status";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qstatus);			
		break;
	case 3:
		p = "cons";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qevent);
		break;
	}
	devdir(c, q, p, 0, cv->owner, 0444, dp);
	return 1;
}

static long	
consread(Chan *ch, void *a, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	char buf[160], *p;
	int l;
	char *sep;
	Led *led;

	p = a;
	x = &proto[PROTO(ch->qid)];
	c = x->conv[CONV(ch->qid)];

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		sprint(buf, "%lud", CONV(ch->qid));
		return readstr(offset, p, n, buf);
	case Qstatus:
		l = snprint(buf, sizeof(buf), "%s, 0, 0, %s, %s, %d\n%s, %d, %s, %d\n",
				thecons.name, onoff(!ishandseton()), thecons.ringer_status,
				thecons.ringer_volume, onoff(thetad.active & Speaker), thecons.speaker_volume,
				onoff(thetad.active & Handset), thecons.handset_volume);
		if((led = thecons.leds) != nil){
			l += snprint(buf+l, sizeof(buf)-l, "{");
			for(; led->name != nil; led++){
				sep = (led+1)->name!=nil? ", ": "}\n";	/* syntax should be simpler */
				l += snprint(buf+l, sizeof(buf)-l, "%s=%d%s", led->name, led->value, sep);
			}
		}
		return readstr(offset, p, n, buf);
	case Qevent:
		return qread(c->event, a, n);	
	case Qdata:
		error(Eunimp);
		return -1;
	}
}

static long	
conswrite(Chan*ch, void *a, long n, ulong offset)
{
	int nf, on;
	char *fields[3], buf[128];
	Led *led;

	USED(offset);

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qdata:
		error("Not Implemented");
		return -1;
	case Qctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';

		nf = parsefields(buf, fields, 3, " \n");

		if(strcmp(fields[0], "led") == 0){
			if (nf < 3)
				error("bad args to led");
			on = strncmp(fields[2], "on", 2) == 0;
			if((led = thecons.leds) != nil){
				for(; led->name != nil; led++)
					if(strcmp(fields[1], led->name) == 0){
						led_set(led, on);
						return n;
					}
			}
			error("no such led");
			return -1;
		}
		if(strcmp(fields[0], "volume") == 0) {
			if (nf < 3) {
				error("bad args to volume");
				return -1;
			}
			if (strncmp(fields[1], "handset",7) == 0) {
				thecons.handset_volume = strtol(fields[2],nil,10);
				qlock(&thetad);
				if (thetad.active & Handset)
					speaker_volume( thecons.handset_volume );
				qunlock(&thetad);
			}
			if (strncmp(fields[1], "speaker",7) == 0) {
				thecons.speaker_volume = strtol(fields[2],nil,10);
				qlock(&thetad);
				if (thetad.active & Speaker)
					speaker_volume( thecons.speaker_volume );
				qunlock(&thetad);				
			}
			if (strncmp(fields[1], "ringer",6) == 0) {
				thecons.ringer_volume = strtol(fields[2],nil,10);				
			}		
			return n;
		}
		if(strcmp(fields[0], "on-hook") == 0 || strcmp(fields[0], "off") == 0) {
			if (nf != 2) {
				error("bad args to on-hook");
				return -1;
			}
			if (strncmp(fields[1], "handset",7) == 0) {
				thecons.handset_status = "off";
				mode(Handset, 1);
			}
			if (strncmp(fields[1], "speaker",7) == 0) {
				thecons.speaker_status = "off";
				mode(Speaker, 1);
			}
			return n;
		}
		if(strcmp(fields[0], "off-hook") == 0 || strcmp(fields[0], "on") == 0){
			if (nf != 2) {
				error("bad args to off-hook");
				return -1;
			}
			if (strncmp(fields[1], "handset",7) == 0) {
				thecons.handset_status = "on";
				mode(Handset, 0);
			}
			if (strncmp(fields[1], "speaker",7) == 0) {
				thecons.speaker_status = "on";
				mode(Speaker, 0);
			}			
			return n;
		}
		if(strcmp(fields[0], "mute") == 0){
			if (thecons.mute) {
				thecons.mute = 0;
				line_volume(LINE_VOL);
				GPIOREG->gpcr = GPIO_ENMUTE;
			} else {
				thecons.mute = 1;
				line_volume(0);
				GPIOREG->gpsr = GPIO_ENMUTE;
			}					
			return n;
		}
		if(strcmp(fields[0], "hold") == 0){
			if (thecons.hold) {
				thecons.hold = 0;
				thecons.mute = 0;
				line_volume(LINE_VOL);
				if (thetad.active & Speaker) 
					speaker(1);	
				else if (thetad.active & Handset) 
					handset(1);
			} else {
				thecons.hold = 1;
				line_volume(0);
				speaker(0);
				handset(0);
			}		
			return n;
		}
		if(strcmp(fields[0], "ring") == 0) {
			ulong dur;
			if (nf != 3) {
				error("bad args to ring");
				return -1;
			}
			dur = strtol( fields[2], 0, 10 );
			if (strcmp(fields[1], "Ring") == 0) {
				ring( 5, dur, 10 );				
			}
			if (strcmp(fields[1], "Ring1") == 0) {
				ring( 0, dur, 10 );				
			}
			if (strcmp(fields[1], "Ring2") == 0) {
				ring( 3, dur, 10 );				
			}
			if (strcmp(fields[1], "Ring3") == 0) {
				ring( 5, dur, 10 );				
			}
			if (strcmp(fields[1], "Ring4") == 0) {
				ring( 7, dur, 10 );			
			}
			if (strcmp(fields[1], "Ring5") == 0) {
				ring( 9, dur, 10 );				
			}
			if (strcmp(fields[1], "Ring6") == 0) {
				ring( 9, dur, 20 );
			}
			if (strcmp(fields[1], "Beep") == 0) {
				ring( 1, dur, dur );
			}
			if (strcmp(fields[1], "Page") == 0) {
				ring( 9, dur, dur );
			}
			if (strcmp(fields[1], "Happy") == 0) {
				ulong scale;
				for (scale = 0; scale < 10; scale++)
					ring( scale, dur, dur );
			}		
			return n;
		}
		error(Ebadreq);		
	}
	return n;
}

/* tgen functionality */
static int 	
tgengen(Chan *c, Dirtab *d, int nd, int s, Dir *dp) 
{
	Qid q;
	Conv *cv;
	char *p = nil;

	USED(nd,d);

	cv = proto[PROTO(c->qid)].conv[CONV(c->qid)];
	switch(s) {
	default:
		return -1;
	case 0:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qdata);
		devdir(c, q, "data", 0, cv->owner, cv->perm, dp);
		return 1;
	case 1:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qctl);
		devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
		return 1;
	case 2:
		p = "status";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qstatus);			
		break;
	}
	devdir(c, q, p, 0, cv->owner, 0444, dp);
	return 1;
}

static long	
tgenread(Chan *ch, void *a, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	char buf[160], *p;
	p = a;
	x = &proto[PROTO(ch->qid)];
	c = x->conv[CONV(ch->qid)];

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		sprint(buf, "%lud", CONV(ch->qid));
		return readstr(offset, p, n, buf);
	case Qstatus:
		sprint(buf, "%d %s\n", c->x, c->state);
		return readstr(offset, p, n, buf);
	case Qdata:
		error("Not Implemented");
		return -1;
	}
}

static long	
tgenwrite(Chan*ch, void *a, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	int count, nf, tmp_speaker;
	char buf[128];
	char *fields[3];

	memmove(buf, a, n);
	buf[n] = '\0';
	x = &proto[PROTO(ch->qid)];
	c = x->conv[CONV(ch->qid)];

	USED(offset);

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';
		nf = parsefields(buf, fields, 3, " \n");
		if(strcmp(fields[0], "TGEN") == 0){
			switch(nf) {
			default:
				error("bad args to connect");
				return -1;
			case 2:
				c->state = "dialing";
				if(UMEC){
					//telonline(1);
					if(!ishandseton())
						handfree(1);
					if((tmp_speaker=onspeaker)!=0)
						speaker(0);
					delay(500);
					for (count = 0; count < strlen(fields[1]); count++)
						tonegen(fields[1][count]);
					if(tmp_speaker)
						speaker(1);
				}else{
					line_hook(0);
					line_volume(LINE_VOL);
					delay(500);
					for (count = 0; count < strlen(fields[1]); count++)
						tonegen(fields[1][count]);
				}
				return n;
			}
		}
		if(strcmp(fields[0], "TONE") == 0){
			switch(nf) {
			default:
				error("bad args to connect");
				return -1;
			case 2:
				if((tmp_speaker=onspeaker)==0)
					speaker(1);
				delay(500);
				line_volume(LINE_VOL);
				for (count = 0; count < strlen(fields[1]); count++)
					tonegen(fields[1][count]);
				if(tmp_speaker == 0)
					speaker(0);
				return n;
			}
		}
		if(strcmp(fields[0], "prompt") == 0) {
			error(Eunimp);
		}
		if(strcmp(fields[0], "tone") == 0){
			error(Eunimp);
		}
		if(strcmp(fields[0], "record") == 0){
			if(nf > 1){
			}else{
			}
		}
		error(Ebadreq);
	}
	return -1;
}

static Tmsg *
msgfetch(Tflash *f, int i)
{
	Tmsg *m;

	if(i < 0 || i >= nelem(f->msg))
		return nil;
	m = malloc(sizeof(*m));
	if(m == nil)
		error(Enovmem);
	m->n = i;
	m->deleted = 0;
	m->recording = 0;
	m->size = 0;		/* EasyTAD doesn't provide it */
	m->mtime = msg_getstamp(i);
	return m;
}

static void
msgbuild(Tflash *f)
{
	int s, n;
	Tmsg *m;

	qlock(f);
	if(waserror()){
		qunlock(f);
		nexterror();
	}
	s = memory_status(1);
	n = s & 0x7F;
	if(n == 0)
		n |= s & 0x80;
	f->full = (s & (1<<7))!=0;
	f->needgc = (s & (1<<8)) != 0;
	DPRINT("msgbuild #%4.4ux nmsg %d was %d needgc %d full %d\n", s, n, f->nmsg, f->needgc, f->full);
	if(f->nmsg > n){
		/* message deleted somehow: refresh the list */
		while(f->nmsg > 0){
			m = f->msg[--f->nmsg];
			if(m != nil)
				free(m);
			f->msg[f->nmsg] = nil;
		}
	}
	if(f->nmsg < n){
		/* add new messages */
		while(f->nmsg < n){
			m = msgfetch(f, f->nmsg);
			if(m == nil)
				error(Eio);
			f->msg[f->nmsg++] = m;
		}
	}
	poperror();
	qunlock(f);
}

/* msg functionality */

static void
msgdsync(Chan *ch)
{
	Proto *x;
	int n;

	/* synchronise our message list with device's */
	x = &proto[PROTO(ch->qid)];
	msgbuild(&msgflash);
	n = msgflash.nmsg;
	if(n > x->maxconv)
		n = x->maxconv;
	x->nc = n;
}

static int 	
msggen(Chan *c, Dirtab *d, int nd, int s, Dir *dp) 
{
	Qid q;
	Conv *cv;
	char *p = nil;

	USED(nd,d);

	cv = proto[PROTO(c->qid)].conv[CONV(c->qid)];
	switch(s) {
	default:
		return -1;
	case 0:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qdata);
		devdir(c, q, "data", 0, cv->owner, cv->perm, dp);
		return 1;
	case 1:
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qctl);
		devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
		return 1;
	case 2:
		p = "status";
		q.path = QID(PROTO(c->qid), CONV(c->qid), Qstatus);			
		break;
	}
	devdir(c, q, p, 0, cv->owner, 0444, dp);
	return 1;
}

static long	
msgread(Chan *ch, void *a, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	char buf[160], *p;
	p = a;
	x = &proto[PROTO(ch->qid)];
	c = x->conv[CONV(ch->qid)];

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		snprint(buf, sizeof(buf), "%lud", CONV(ch->qid));
		return readstr(offset, p, n, buf);
	case Qstatus:
		snprint(buf, sizeof(buf), "%d %s (%lud %lud %lud)\n", c->x, c->state, TYPE(ch->qid), PROTO(ch->qid), CONV(ch->qid));
		return readstr(offset, p, n, buf);
	case Qdata:
		error(Eunimp);
		return -1;
	}
}

static long	
msgwrite(Chan*ch, void *a, long n, ulong offset)
{
	Conv *c;
	Proto *x;
	int nf;
	char buf[128];
	char *fields[3];

	memmove(buf, a, n);
	buf[n] = '\0';
	x = &proto[PROTO(ch->qid)];
	c = x->conv[CONV(ch->qid)];

	USED(offset);

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';
		nf = parsefields(buf, fields, 3, " \n");
		if(strcmp(fields[0], "play") == 0) {
			c->state = "Playing";
			error(Eunimp);
			return nf;
		}
		if(strcmp(fields[0], "record") == 0) {
			c->state = "Recording";
			error(Eunimp);
		}
		if(strcmp(fields[0], "stop") == 0) {
			c->state = "Idle";
			error(Eunimp);
		}
		if(strcmp(fields[0], "delete") == 0) {
			c->state = "Deleted";
			error(Eunimp);
		}
		if(strcmp(fields[0], "pause") == 0) {
			error(Eunimp);
		}
		if(strcmp(fields[0], "continue") == 0) {
			error(Eunimp);
		}
		error(Ebadreq);
	}
	return -1;
}

Dev taddevtab = {
	'P',
	"telephony",

	devreset,
	tadinit,
	tadattach,
	devdetach,
	devclone,
	tadwalk,
	tadstat,
	tadopen,
	devcreate,
	tadclose,
	tadread,
	devbread,
	tadwrite,
	devbwrite,
	devremove,
	devwstat
};


