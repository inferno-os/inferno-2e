#include "dat.h"
#include "fns.h"
#include "error.h"
#define __EXTENSIONS__
#include <fcntl.h>
#include <stropts.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include "svp.h"
#include "devaudio.h"
#include <sys/audioio.h>

#define 	Audio_8Bit_Val		8
#define 	Audio_16Bit_Val		16

#define 	Audio_Mono_Val		1
#define 	Audio_Stereo_Val	2

#define 	Audio_Mic_Val		AUDIO_MICROPHONE
#define 	Audio_Linein_Val	AUDIO_LINE_IN

#define		Audio_Speaker_Val	AUDIO_SPEAKER
#define		Audio_Headphone_Val	AUDIO_HEADPHONE
#define		Audio_Lineout_Val	AUDIO_LINE_OUT

#define 	Audio_Pcm_Val		AUDIO_ENCODING_LINEAR
#define 	Audio_Ulaw_Val		AUDIO_ENCODING_ULAW
#define 	Audio_Alaw_Val		AUDIO_ENCODING_ALAW

#define 	Audio_8K_Val		8000
#define 	Audio_11K_Val		11025
#define 	Audio_22K_Val		22050
#define 	Audio_44K_Val		44100

#include "devaudio-tbls.c"

#define	min(a,b)	((a) < (b) ? (a) : (b))
#define DEBUG 0
static int debug = DEBUG;

#define AUDIO_FILE_STRING	"/dev/audio"

typedef enum {
	A_Pause,
	A_UnPause
} A_Flow ;

typedef enum {
	A_In,
	A_Out
} A_Dir ;

Lock inlock;
Lock outlock;

int	audio_file_in  = -1;	/* file in */
int	audio_file_out = -1;	/* file out */
int	audio_ctl_in  = -1;	/* ctl in */
int	audio_ctl_out = -1;	/* ctl out */

int	audio_swap_flag = 0;	/* endian swap */

A_Flow	audio_in_pause = A_UnPause;

Audio_t av;

static int audio_enforce(Audio_t*);
static int audio_open_in(void);
static int audio_open_out(void);
static int audio_pause_in(int, A_Flow);
static int audio_pause_out(int);
static int audio_set_blocking(int);
static int audio_set_info(int, Audio_d*, A_Dir);
static int audio_set_nonblocking(int);
static int audio_test_endian(void);
static void audio_swap_endian(char*, int);

void
audio_file_init(void)
{
	audio_swap_flag = audio_test_endian();
	audio_info_init(&av);
	return;
}

void
audio_ctl_init(void)
{
	return;
}

int
audio_file_open(Chan *c, int omode)
{
int in_is_open = 0;

	if(omode==OREAD) {
		lock(&inlock);
		if(waserror()){
			unlock(&inlock);
			nexterror();
		}

		if(-1 < audio_file_in) {
			error(Einuse);
			return 0;
		}

		if((audio_file_in = audio_open_in() ) < 0) 
			return 0;

		unlock(&inlock);
		poperror();
	}
	else if(omode==OWRITE) {
		lock(&outlock);
		if(waserror()){
			unlock(&outlock);
			nexterror();
			return 0;
		}

		if(-1 < audio_file_out) {
			error(Einuse);
			return 0;
		}

		if((audio_file_out = audio_open_out() ) < 0) 
			return 0;

		unlock(&outlock);
		poperror();
	}
	else if(omode==ORDWR) {
		lock(&inlock);
		lock(&outlock);
		if(waserror()){
			if(in_is_open)
				close(audio_file_in);
			unlock(&inlock);
			unlock(&outlock);
			nexterror();
		}
		if((-1 < audio_file_in) || (-1 < audio_file_out)) {
			error(Einuse);
			return 0;
		}

		if((audio_file_in = audio_open_in() ) < 0) 
			return 0;

		in_is_open = 1;

		if((audio_file_out = audio_open_out() ) < 0)  {
			return 0;
		}

		unlock(&inlock);
		unlock(&outlock);
		poperror();
	}
	else
		return 0;

	return 1;
}

int
audio_ctl_open(Chan *c, int omode)
{
	return 1;
}

void
audio_file_close(Chan *c)
{
	if(c->mode==OREAD) {
		lock(&inlock);
		close(audio_file_in);
		audio_file_in = -1;
		unlock(&inlock);
	}
	else if(c->mode==OWRITE) {
		lock(&outlock);
		close(audio_file_out);
		audio_file_out = -1;
		unlock(&outlock);
	}
	else if(c->mode==ORDWR) {
		lock(&inlock);
		lock(&outlock);
		close(audio_file_in);
		close(audio_file_out);
		audio_file_in = -1;
		audio_file_out = -1;
		unlock(&inlock);
		unlock(&outlock);
	}
	return ;
}

void
audio_ctl_close(Chan *c)
{
	return;
}

long
audio_file_read(Chan *c, void *va, long count, long offset)
{
struct  timespec time;
long ba;
long status;
long chunk;
long total;
char *pva = (char *) va;

	lock(&inlock);
	if(waserror()){
		unlock(&inlock);
		nexterror();
	}

	if(audio_file_in < 0) { 
		error(Eperm);
		return -1;
	} 

	/* check block alignment */
	ba = av.in.bits * av.in.chan / Bits_Per_Byte;

	if(count % ba) {
		error(Ebadarg);
		return -1;
	}

	if(! audio_pause_in(audio_file_in, A_UnPause)) {
		error(Eio);
		return -1;
	}
	
	total = 0;
	while(total < count) {
		chunk = count - total;
		osenter();
		status = read(audio_file_in, pva + total, chunk);
		osleave(); 
		if(status < 0) {
			error(Eio);
			return -1;
		}
		total += status;
	}

	if(total != count) {
		error(Eio);
		return -1;
	}

	if(audio_swap_flag && av.out.bits == Audio_16Bit_Val)
		audio_swap_endian(pva, count); 

	poperror();
	unlock(&inlock);

    time.tv_sec = 0; /* hack around broken thread scheduler in Solaris */
    time.tv_nsec= 1;
    nanosleep(&time,nil);

	return count;
}

long
audio_file_write(Chan *c, void *va, long count, long offset)
{
struct  timespec time;
long status = -1;
long ba;
long total;
long chunk;
long bufsz;

	lock(&outlock);
	if(waserror()){
		unlock(&outlock);
		nexterror();
	}

	if(audio_file_out < 0) { 
		error(Eperm);
		return -1;
	} 

	/* check block alignment */
	ba = av.out.bits * av.out.chan / Bits_Per_Byte;

	if(count % ba) {
		error(Ebadarg);
		return -1;
	}

	if(audio_swap_flag && av.out.bits == Audio_16Bit_Val)
		audio_swap_endian(va, count); 

	total = 0;
	bufsz = av.out.buf * Audio_Max_Buf / Audio_Max_Val;

	if(bufsz == 0) {
		error(Ebadarg);
		return -1;
	} 

	while(total < count) {
		chunk = min(bufsz, count - total);
		osenter();
		status = write(audio_file_out, va, chunk);
		osleave();
		if(status != count) {
			error(Eio);
			return -1;
		}
		total += chunk;
	}

	poperror();
	unlock(&outlock);

    time.tv_sec = 0; /* hack around broken thread scheduler in Solaris */
    time.tv_nsec= 1;
    nanosleep(&time,nil);

	return count;
}

int
audio_open_in(void)
{
int fd;

	/* open non-blocking in case someone already has it open */
	/* otherwise we would block until they close! */
	fd = open(AUDIO_FILE_STRING, O_RDONLY|O_NONBLOCK);

	if(fd < 0) {
		error(Einuse);
		return -1;
	}

	/* change device to be blocking */
	if(! audio_set_blocking(fd)) {
		close(fd);
		error(Eio);
		return -1;
	}

	if(! audio_pause_in(fd, A_Pause)) {
		close(fd);
		error(Eio);
		return -1;
	}

	if(! audio_flush(fd, A_In)) {
		close(fd);
		error(Eio);
		return -1;
	}

	/* set audio info */
	av.in.flags = ~AUDIO_NO_FLAGS;
	av.out.flags = AUDIO_NO_FLAGS;

	if(! audio_set_info(fd, &av.in, A_In)) {
		close(fd);
		error(Ebadarg);
		return -1;
	}

	av.in.flags = AUDIO_NO_FLAGS;

	/* tada, we're open, blocking, paused and flushed */
	return fd;
}

int
audio_open_out(void){
int fd;
struct audio_info	hdr;

	/* open non-blocking in case someone already has it open */
	/* otherwise we would block until they close! */
	fd = open(AUDIO_FILE_STRING, O_WRONLY|O_NONBLOCK);

	if(fd < 0) {
		error(Einuse);
		return -1;
	}

	/* change device to be blocking */
	if(! audio_set_blocking(fd)) {
		close(fd);
		error(Eio);
		return -1;
	}

	/* set audio info */
	av.in.flags = AUDIO_NO_FLAGS;
	av.out.flags = ~AUDIO_NO_FLAGS;

	if(! audio_set_info(fd, &av.out, A_Out)) {
		close(fd);
		error(Ebadarg);
		return -1;
	}

	av.out.flags = AUDIO_NO_FLAGS;

	return fd;
}

long
audio_ctl_read(Chan *c, void *va, long count, long offset)
{
char buf[AUDIO_INFO_MAXBUF+1];
audio_info_t	info;
char *p = va;
int status;

  lock(&inlock);
  if(waserror()){
    unlock(&inlock);
	nexterror();
  }

  /* check if buffer is big enough for all the info */
  if(count < AUDIO_INFO_MAXBUF) {
    error(Ebadarg);
    return -1;
  }

  if(-1 < audio_file_in) {
  	/* initialize header */
  	AUDIO_INITINFO(&info);

  	osenter();
  	status = ioctl(audio_file_in, AUDIO_GETINFO, &info);  
  	osleave();

    if(status == -1) {
      error(Ebadarg);
      return -1;
    } 
  }

  av.in.count = info.record.samples;
  av.out.count = info.play.samples;

  if((count = audio_get_info(buf, &av.in, &av.out)) == 0) {
    error(Ebadarg);
    return -1;
  } 

  poperror();
  unlock(&inlock);

  return readstr(offset, p, count, buf);
}

long
audio_ctl_write(Chan *c, void *va, long count, long offset)
{
int fd;		
int ff;
Audio_t tmpav = av;

  tmpav.in.flags = AUDIO_NO_FLAGS;
  tmpav.out.flags = AUDIO_NO_FLAGS;

  if(waserror()){
	nexterror();
  }

  if(! audioparse(va, count, &tmpav)) {
    error(Ebadarg);
    return -1;
  }

  if(! audio_enforce(&tmpav)) {
    error(Ebadarg);
    return -1;
  }

  poperror();

  lock(&inlock);
  if(waserror()){
    unlock(&inlock);
	nexterror();
  }

  if((-1 < audio_file_in) && (tmpav.in.flags & AUDIO_MOD_FLAG)) {
    if(! audio_pause_in(audio_file_in, A_Pause)) {
      error(Ebadarg);
      return -1;
    }
    if(! audio_flush(audio_file_in, A_In)) {
      error(Ebadarg);
      return -1;
    }
    if(! audio_set_info(audio_file_in, &tmpav.in, A_In)) {
      error(Ebadarg);
      return -1;
    }
  }
  unlock(&inlock);
  poperror();

  lock(&outlock);
  if(waserror()){
    unlock(&outlock);
	nexterror();
  }
  if((-1 < audio_file_out) && (tmpav.out.flags & AUDIO_MOD_FLAG)) {
    if(! audio_pause_out(audio_file_out)) {
      error(Ebadarg);
      return -1;
	}
    if(! audio_set_info(audio_file_out, &tmpav.out, A_Out)) {
      error(Ebadarg);
      return -1;
    }
  }
  poperror();
  unlock(&outlock);

  tmpav.in.flags = AUDIO_NO_FLAGS;
  tmpav.out.flags = AUDIO_NO_FLAGS;
  tmpav.in.count = tmpav.out.count = 0;

  av = tmpav;

  return count;
}


int
audio_set_blocking(int fd)
{
int val;
int flags = O_NONBLOCK;

	if((val = fcntl(fd, F_GETFL, 0)) == -1)
		return 0;
	
	val &= ~flags;

	if(fcntl(fd, F_SETFL, val) < 0)
		return 0;

	return 1;
}

int
audio_set_nonblocking(int fd)
{
int val;
int flags = O_NONBLOCK;

	if((val = fcntl(fd, F_GETFL, 0)) == -1)
		return 0;
	
	val |= flags;

	if(fcntl(fd, F_SETFL, val) < 0)
		return 0;

	return 1;

	
}

int
audio_set_info(int fd, Audio_d *i, A_Dir d)
{
int status;
int unequal_stereo = 0;
audio_info_t	info;
audio_prinfo_t  *dev;

	if(fd < 0)
		return 0;

	/* devitialize header */
	AUDIO_INITINFO(&info);

	if(d == A_In)
		dev = &info.record;
	else
		dev = &info.play;

	/* sample rate */
	if(i->flags & AUDIO_RATE_FLAG)
		dev->sample_rate = i->rate;

	/* channels */
	if(i->flags & AUDIO_CHAN_FLAG)
		dev->channels = i->chan;

	/* precision */
	if(i->flags & AUDIO_BITS_FLAG)
		dev->precision = i->bits;

	/* encoding */
	if(i->flags & AUDIO_ENC_FLAG)
		dev->encoding = i->enc;

	/* devices */
	if(i->flags & AUDIO_DEV_FLAG)
		dev->port = i->dev;

	/* dev volume */
	if(i->flags & (AUDIO_LEFT_FLAG|AUDIO_VOL_FLAG)) {
		dev->gain = (i->left * AUDIO_MAX_GAIN) / Audio_Max_Val;

		/* do left first then right later */
		if(i->left == i->right) 
			dev->balance = AUDIO_MID_BALANCE;
		else {
			dev->balance = AUDIO_LEFT_BALANCE;
			if(i->chan != Audio_Mono_Val)
				unequal_stereo = 1;
		}
	}

	osenter();
	status = ioctl(fd, AUDIO_SETINFO, &info);  /* lock and load general stuff */
	osleave();

	if(status == -1) {
		if(debug) printf("audio_set_info 1 failed: fd = %d errno = %d\n", fd, errno);
		return 0;
	}

	/* check for different right and left for dev */
	if(unequal_stereo) {

		/* re-init header */
		AUDIO_INITINFO(&info);

		dev->gain = (i->right * AUDIO_MAX_GAIN) / Audio_Max_Val;
		dev->balance == AUDIO_RIGHT_BALANCE;

		osenter();
		status = ioctl(fd, AUDIO_SETINFO, &info);
		osleave();

		if(status == -1) {
			if(debug) printf("audio_set_info 2 failed: fd = %d errno = %d\n",fd, errno);
			return 0;
		}
	}

	return 1;
}

void 
audio_swap_endian(char *p, int n)
{
    char    b;

	while (n > 1) {
		b = p[0];
		p[0] = p[1];
		p[1] = b;
		p += 2;
		n -= 2;
	}
}

int
audio_test_endian()
{
int test = 0x12345678;
char *p;

	p = &test;
	if((*p==0x12)&&(*(p+1)==0x34)&&(*(p+2)==0x56)&&(*(p+3)==0x78))
		return 1;
	else
		return 0;
}
int
audio_pause_out(int fd)
{
audio_info_t	info;
int	foo = 0;
int status;

	osenter();
	status = ioctl(fd, AUDIO_DRAIN, &foo);
	osleave();

	if(status == -1) 
		return 0;
	return 1;
}

int
audio_pause_in(int fd, A_Flow f)
{
audio_info_t	info;
int status;

	if(-1 > fd)
		return 0;

	if(audio_in_pause == f)
		return 1;
	
	/* initialize header */
	AUDIO_INITINFO(&info);

	/* unpause input */
	if(f == A_Pause)
		info.record.pause = 1;
	else
		info.record.pause = 0;

	osenter();
	status = ioctl(fd, AUDIO_SETINFO, &info);
	osleave();

	if(status == -1) 
		return 0;

	audio_in_pause = f;

	return 1;
}

int
audio_flush(int fd, A_Dir d)
{
int flag = d==A_In ? FLUSHR : FLUSHW;
int status;

	osenter();
	status = ioctl(fd, I_FLUSH, flag); /* drain anything already put into buffer */
	osleave();

	if(status == -1) 
		return 0;
	return 1;
}

int
audio_enforce(Audio_t *t)
{
	if(((t->in.enc == Audio_Ulaw_Val) || (t->in.enc == Audio_Alaw_Val)) && 
		((t->in.rate != Audio_8K_Val) || (t->in.chan != Audio_Mono_Val)))
		 return 0;

	if(((t->out.enc == Audio_Ulaw_Val) || (t->out.enc == Audio_Alaw_Val)) && 
		((t->out.rate != Audio_8K_Val) || (t->out.chan != Audio_Mono_Val)))
		 return 0;

	return 1;
}
