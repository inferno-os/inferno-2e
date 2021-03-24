#include "dat.h"
#include "fns.h"
#include "error.h"
#include "devaudio.h"
#include "svp.h"

svp_t audio_bits_tbl[] = {
	{ Audio_8Bit_String, Audio_8Bit_Val } ,	 /* 8 bits per sample */
	{ Audio_16Bit_String, Audio_16Bit_Val }  /* 16 bits per sample */
} ;

svp_t audio_chan_tbl[] = {
	{ Audio_Mono_String, Audio_Mono_Val },		/* 1 channel */
	{ Audio_Stereo_String, Audio_Stereo_Val }	/* 2 channels */
} ;

svp_t audio_indev_tbl[] = {
	{ Audio_Mic_String, Audio_Mic_Val }, 		/* input microphone */
	{ Audio_Linein_String, Audio_Linein_Val }  	/* line in */
} ;

svp_t audio_outdev_tbl[] = {
	{ Audio_Speaker_String, Audio_Speaker_Val },	/* output speaker */
	{ Audio_Headphone_String, Audio_Headphone_Val },/* head phones */
	{ Audio_Lineout_String, Audio_Lineout_Val }	/* line out */
} ;

svp_t audio_enc_tbl[] = {
	{ Audio_Ulaw_String, Audio_Ulaw_Val },	/* u-law encoding */
	{ Audio_Alaw_String, Audio_Alaw_Val },	/* A-law encoding */
	{ Audio_Pcm_String, Audio_Pcm_Val }	/* Pulse Code Modulation */
} ;

svp_t audio_rate_tbl[] = {
	{ Audio_8K_String, Audio_8K_Val },	/* 8000 samples per second */
	{ Audio_11K_String, Audio_11K_Val },	/* 11025 samples per second */
	{ Audio_22K_String, Audio_22K_Val },	/* 22050 samples per second */
	{ Audio_44K_String, Audio_44K_Val }	/* 44100 samples per second */
} ;

svp_t audio_val_tbl[] = {
	{ Audio_Min_String, Audio_Min_Val },	/* min */
	{ Audio_Max_String, Audio_Max_Val } 	/* max */
} ;

Audio_d Default_Audio_In =  {
	AUDIO_NO_FLAG,
	Default_Audio_Bits,		/* bits per sample */
	Default_Audio_Buf,		/* buffer size */
	Default_Audio_Chan,		/* number of channels */
	Default_Audio_Count,		/* samples remaining */
	Default_Audio_Indev,		/* device */
	Default_Audio_Enc,		/* encoding format */
	Default_Audio_Rate,		/* samples per second */
	Default_Audio_Left,		/* left channel gain */
	Default_Audio_Right,		/* right channel gain */
} ;

Audio_d Default_Audio_Out =  {
	AUDIO_NO_FLAG,
	Default_Audio_Bits,		/* bits per sample */
	Default_Audio_Buf,		/* buffer size */
	Default_Audio_Chan,		/* number of channels */
	Default_Audio_Count,		/* samples remaining */
	Default_Audio_Outdev,		/* device */
	Default_Audio_Enc,		/* encoding format */
	Default_Audio_Rate,		/* samples per second */
	Default_Audio_Left,		/* left channel gain */
	Default_Audio_Right,		/* right channel gain */
} ;

Dirtab audiotab[] =
{
	"audio",	{Qaudio},	0,	0777,
	"audioctl",	{Qaudioctl},	0,	0777
};

void
audioinit(void)
{
	audio_file_init();
	audio_ctl_init();
}
Chan*
audioattach(void *spec)
{
	static int kp;

	return devattach('A', spec);
}
Chan*
audioclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}
int
audiowalk(Chan *c, char *name)
{
	return devwalk(c, name, audiotab, nelem(audiotab), devgen);
}
void
audiostat(Chan *c, char *db)
{
	devstat(c, db, audiotab, nelem(audiotab), devgen);
}
void
audiocreate(Chan *c, char *name, int omode, ulong perm)
{
	USED(c);
	USED(name);
	USED(omode);
	USED(perm);
	error(Eperm);
}
void
audioremove(Chan *c)
{
	USED(c);
	error(Eperm);
}
void
audiowstat(Chan *c, char *dp)
{
	USED(c);
	USED(dp);
	error(Eperm);
}
Block*
audiobread(Chan* chan, long len, ulong flag)
{
	return NULL;
}
long
audiobwrite(Chan* chan, Block* block, ulong flag)
{
	return (long) 0;
}


Chan*
audioopen(Chan *c, int omode)
{
	switch(c->qid.path) {
	default:
		return devopen(c, omode, audiotab, nelem(audiotab), devgen);
	case Qaudio:
		if(! audio_file_open(c, omode)) {
			error(Eperm);
			return c;
		}
		break;
	case Qaudioctl:
		if(! audio_ctl_open(c, omode)) {
			error(Eperm);
			return c;
		}
		break;
	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}
//...............................................
void
audioclose(Chan *c)
{
	int i;
	switch(c->qid.path) {
	default:
	case Qaudio:
		audio_file_close(c);
		break;
	case Qaudioctl:
		audio_ctl_close(c);
		break;
	}
}
//...............................................
long
audioread(Chan *c, void *va, long count, ulong offset)
{
	if (c->qid.path & CHDIR) {
		return devdirread(c, va, count, audiotab, nelem(audiotab), devgen);
	}
	switch(c->qid.path) {
	case Qaudio:
		return(audio_file_read(c, va, count));
	case Qaudioctl:
		return(audio_ctl_read(c, va, count));
	default:
		error(Egreg);
	}
	return 0;
}
//...............................................
long
audiowrite(Chan *c, void *va, long count, ulong offset)
{
	switch(c->qid.path) {
	case Qaudio:
		return(audio_file_write(c, va, count));
	case Qaudioctl:
		return(audio_ctl_write(c, va, count));
	default:
		error(Egreg);
	}
	return 0;
}

int
audioparse(char* buf, Audio_t *t)
{
int i;
int n;
ulong v;
aflag_t tf = AUDIO_IN_FLAG|AUDIO_OUT_FLAG;
char *argv[AUDIO_CMD_MAX];
Audio_t info = *t;

  n = parsefields(buf, argv, AUDIO_CMD_MAX, " =,\t\n");

  if(debug) printf("%d args\n", n);

  for(i = 0; i < n - 1; i++) {

    if(debug) printf("arg[%d] = %s\n", i, argv[i]);

    if(! strcmp(argv[i], Audio_In_String)) {
	tf &= ~AUDIO_OUT_FLAG;
	tf |= AUDIO_IN_FLAG;
	continue;
    }
    if(! strcmp(argv[i], Audio_Out_String)) {
	tf &= ~AUDIO_IN_FLAG;
	tf |= AUDIO_OUT_FLAG;
	continue;
    }
    if(! strcmp(argv[i], Audio_Bits_String)) {
	if(! svpmatchs(audio_bits_tbl, SVP_SZ(audio_bits_tbl), argv[i+1], &v))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_BITS_FLAG|AUDIO_MOD_FLAG;
	  info.in.bits = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_BITS_FLAG|AUDIO_MOD_FLAG;
	  info.out.bits = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_Buf_String)) {
	if(! sval(argv[i+1], &v, Audio_Max_Val, Audio_Min_Val))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_BUF_FLAG|AUDIO_MOD_FLAG;
	  info.in.buf = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_BUF_FLAG|AUDIO_MOD_FLAG;
	  info.out.buf = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_Chan_String)) {
	if(! svpmatchs(audio_chan_tbl, SVP_SZ(audio_chan_tbl), argv[i+1], &v))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_CHAN_FLAG|AUDIO_MOD_FLAG;
	  info.in.chan = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_CHAN_FLAG|AUDIO_MOD_FLAG;
	  info.out.chan = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_Count_String)) {
	if(! sval(argv[i+1], &v, Audio_Max_Val, Audio_Min_Val))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_COUNT_FLAG|AUDIO_MOD_FLAG;
	  info.in.count = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_COUNT_FLAG|AUDIO_MOD_FLAG;
	  info.out.count = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_InDev_String)) {
	if(! svpmatchs(audio_indev_tbl, SVP_SZ(audio_indev_tbl), argv[i+1], &v))
        	break;
	i++;
	info.in.flags |= AUDIO_DEV_FLAG|AUDIO_MOD_FLAG;
	info.in.dev = v;
    }
    else if(! strcmp(argv[i], Audio_OutDev_String)) {
	if(! svpmatchs(audio_outdev_tbl, SVP_SZ(audio_outdev_tbl), argv[i+1], &v))
         	break;
	i++;
	info.out.flags |= AUDIO_DEV_FLAG|AUDIO_MOD_FLAG;
	info.out.dev = v;
	continue;
    }
    else if(! strcmp(argv[i], Audio_Enc_String)) {
	if(! svpmatchs(adio_enc_tbl, SVP_SZ(adio_enc_tbl), argv[i+1], &v))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_ENC_FLAG|AUDIO_MOD_FLAG;
	  info.in.enc = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_ENC_FLAG|AUDIO_MOD_FLAG;
	  info.out.enc = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_Rate_String)) {
	if(! svpmatchs(audio_rate_tbl, SVP_SZ(audio_rate_tbl), argv[i+1], &v))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_RATE_FLAG|AUDIO_MOD_FLAG;
	  info.in.rate = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_RATE_FLAG|AUDIO_MOD_FLAG;
	  info.out.rate = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_Vol_String)) {
	if(! sval(argv[i+1], &v, Audio_Max_Val, Audio_Min_Val))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_VOL_FLAG|AUDIO_MOD_FLAG;
	  info.in.left = v;
	  info.in.right = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_VOL_FLAG|AUDIO_MOD_FLAG;
	  info.out.left = v;
	  info.out.right = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_Left_String)) {
	if(! sval(argv[i+1], &v, Audio_Max_Val, Audio_Min_Val))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_LEFT_FLAG|AUDIO_MOD_FLAG;
	  info.in.left = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_LEFT_FLAG|AUDIO_MOD_FLAG;
	  info.out.left = v;
	}

	continue;
    }
    else if(! strcmp(argv[i], Audio_Right_String)) {
	if(! sval(argv[i+1], &v, Audio_Max_Val, Audio_Min_Val))
        	break;

	i++;
	if(tf & AUDIO_IN_FLAG) {
	  info.in.flags |= AUDIO_RIGHT_FLAG|AUDIO_MOD_FLAG;
	  info.in.right = v;
	}
	if(tf & AUDIO_OUT_FLAG) {
	  info.out.flags |= AUDIO_RIGHT_FLAG|AUDIO_MOD_FLAG;
	  info.out.right = v;
	}

	continue;
    }
    else
	continue;
  }
  if(i < n - 1)
	return 0;

  *t = info;	/* set information back */
  return n;	/* return number of affected fields */
}

int
audio_get_info(void *buf, Audio_d *in, Audio_d *out)
{
char p[AUDIO_INFO_MAXBUF+1];
char *s;
int l = 0;
Audio_d tmpin = *in;
Audio_d tmpout = *out;

  p[0] = '\0';

  /* in device */
  if(! svpmatchv(audio_indev_tbl, SVP_SZ(audio_indev_tbl), &s, in.dev))
    return 0;
  else
    sprintf(p, "%s %s\n", Audio_InDev_String, s);

  /* out device */
  if(! svpmatchv(audio_outdev_tbl, SVP_SZ(audio_outdev_tbl), &s, out.dev))
    return 0;
  else
    sprintf(p+strlen(p), " %s %s\n", Audio_OutDev_String, s);

  tmpin.flags = AUDIO_NO_FLAGS;
  tmpout.flags = AUDIO_NO_FLAGS;
  tmpin.dev = 0;
  tmpout.dev = 0;

  if(tmpin != tmpout) {

    sprintf(p+strlen(p), " %s\n", Audio_Out_String);

    /* encode */
    if(! svpmatchv(audio_enc_tbl, SVP_SZ(audio_enc_tbl), &s, out.enc))
	return 0;
    else
    	sprintf(p+strlen(p), "%s %s\n", Audio_Enc_String, s);

    /* rate */
    if(! svpmatchv(audio_rate_tbl, SVP_SZ(audio_rate_tbl), &s, out.rate))
	return 0;
    else
    	sprintf(p+strlen(p), " %s %s\n", Audio_Rate_String, s);

    /* bits */
    if(! svpmatchv(audio_bits_tbl, SVP_SZ(audio_bits_tbl), &s, out.bits))
	return 0;
    else
    	sprintf(p+strlen(p), " %s %s\n", Audio_Bits_String, s);

    /* channels */
    if(! svpmatchv(audio_chan_tbl, SVP_SZ(audio_chan_tbl), &s, out.chan))
	return 0;
    else
    	sprintf(p+strlen(p), " %s %s\n", Audio_Chan_String, s);

    /* left channel */
    sprintf(p+strlen(p), " %s %d\n", Audio_Left_String, out.left);

    /* right channel */
    sprintf(p+strlen(p), " %s %d\n", Audio_Right_String, out.right);

    sprintf(p+strlen(p), " %s\n", Audio_In_String);
  }

  /* encode */
  if(! svpmatchv(audio_enc_tbl, SVP_SZ(audio_enc_tbl), &s, in.enc))
	return 0;
  else
  	sprintf(p+strlen(p), "%s %s\n", Audio_Enc_String, s);

  /* rate */
  if(! svpmatchv(audio_rate_tbl, SVP_SZ(audio_rate_tbl), &s, in.rate))
	return 0;
  else
  	sprintf(p+strlen(p), " %s %s\n", Audio_Rate_String, s);

  /* bits */
  if(! svpmatchv(audio_bits_tbl, SVP_SZ(audio_bits_tbl), &s, in.bits))
	return 0;
  else
  	sprintf(p+strlen(p), " %s %s\n", Audio_Bits_String, s);

  /* channels */
  if(! svpmatchv(audio_chan_tbl, SVP_SZ(audio_chan_tbl), &s, in.chan))
	return 0;
  else
  	sprintf(p+strlen(p), " %s %s\n", Audio_Chan_String, s);

  /* left channel */
  sprintf(p+strlen(p), " %s %d\n", Audio_Left_String, in.left);

  /* right channel */
  sprintf(p+strlen(p), " %s %d\n", Audio_Right_String, in.right);

  memmove(buf, p, strlen(p)+1);

  return 1;
}

void
audioinit(Audio_t *t)
{
	t->in = Default_Audio_In;
	t->out = Default_Audio_Out;
	return;	
}
