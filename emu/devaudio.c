#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"svp.h"
#include	"devaudio.h"

#define DEBUG 0
static int debug = DEBUG;

#define OPEN_MASK 0x7

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
	switch(c->qid.path & ~CHDIR) {
	default:
		error(Eperm);
		break;
	case Qdir:
		break;
	case Qaudio:
		if(! audio_file_open(c, omode&OPEN_MASK)) 
			return c;
		break;
	case Qaudioctl:
		if(! audio_ctl_open(c, omode&OPEN_MASK)) 
			return c;
		break;
	}
	c = devopen(c, omode, audiotab, nelem(audiotab), devgen);
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

	if((c->flag & COPEN) == 0)
		return;

	switch(c->qid.path & ~CHDIR) {
	default:
		error(Eperm);
		break;
	case Qdir:
		break;
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
	switch(c->qid.path & ~CHDIR) {
	case Qaudio:
		return(audio_file_read(c, va, count, offset));
	case Qaudioctl:
		return(audio_ctl_read(c, va, count, offset));
	default:
		error(Egreg);
	}
	return 0;
}
//...............................................
long
audiowrite(Chan *c, void *va, long count, ulong offset)
{
	switch(c->qid.path & ~CHDIR) {
	case Qaudio:
		return(audio_file_write(c, va, count, offset));
	case Qaudioctl:
		return(audio_ctl_write(c, va, count, offset));
	default:
		error(Egreg);
	}
	return 0;
}

int
audioparse(char* args, int len, Audio_t *t)
{
int i;
int n;
ulong v;
char *argv[AUDIO_CMD_MAXNUM];
char buf[AUDIO_INFO_MAXBUF+1];
aflag_t tf = AUDIO_IN_FLAG|AUDIO_OUT_FLAG;
Audio_t info = *t;

  if(len > sizeof(buf)-1)
    len = sizeof(buf)-1;
  memmove(buf, args, len);
  buf[len] = '\0';

  if(debug) print("buf = <<%s>>\n", buf);

  n = parsefields(buf, argv, AUDIO_CMD_MAXNUM, " =,\t\n");

  if(debug) print("%d args\n", n);

  for(i = 0; i < n - 1; i++) {

    if(debug) print("arg[%d] = %s\n", i, argv[i]);

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
	if(! svpmatchs(audio_bits_tbl, audio_bits_sz, argv[i+1], &v))
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
	if(! svpmatchs(audio_chan_tbl, audio_chan_sz, argv[i+1], &v))
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
/* 	COUNT was supposed to implement a flushing mechanism */
/* 	but we are still undecided about how it will work */
/*	so we will revisit this code later */
/*
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
*/
    else if(! strcmp(argv[i], Audio_InDev_String)) {
	if(! svpmatchs(audio_indev_tbl, audio_indev_sz, argv[i+1], &v))
        	break;
	i++;
	info.in.flags |= AUDIO_DEV_FLAG|AUDIO_MOD_FLAG;
	info.in.dev = v;
    }
    else if(! strcmp(argv[i], Audio_OutDev_String)) {
	if(! svpmatchs(audio_outdev_tbl, audio_outdev_sz, argv[i+1], &v))
         	break;
	i++;
	info.out.flags |= AUDIO_DEV_FLAG|AUDIO_MOD_FLAG;
	info.out.dev = v;
	continue;
    }
    else if(! strcmp(argv[i], Audio_Enc_String)) {
	if(! svpmatchs(audio_enc_tbl, audio_enc_sz, argv[i+1], &v))
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
	if(! svpmatchs(audio_rate_tbl, audio_rate_sz, argv[i+1], &v))
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
audio_get_info(char *p, Audio_d *in, Audio_d *out)
{
char *s;
int i;
int l = 0;
Audio_d tmpin = *in;
Audio_d tmpout = *out;
svp_t *sv;

  /* in device */
  if(! svpmatchv(audio_indev_tbl, audio_indev_sz, &s, in->dev))
    return 0;
  else
    sprint(p, "%s %s", Audio_InDev_String, s);

  /* rest of input devices */
  sv = audio_indev_tbl;
  for(i = 0; i < audio_indev_sz; i++, sv++)
    if(sv->v != in->dev && sv->s)
      sprint(p+strlen(p), " %s", sv->s);
  sprint(p+strlen(p), "\n");
    

  /* out device */
  if(! svpmatchv(audio_outdev_tbl, audio_outdev_sz, &s, out->dev))
    return 0;
  else
    sprint(p+strlen(p), "%s %s", Audio_OutDev_String, s);

  /* rest of output devices */
  sv = audio_outdev_tbl;
  for(i = 0; i < audio_outdev_sz; i++, sv++)
    if(sv->v != out->dev && sv->s)
      sprint(p+strlen(p), " %s", sv->s);
  sprint(p+strlen(p), "\n");

  tmpin.flags = AUDIO_NO_FLAGS;
  tmpout.flags = AUDIO_NO_FLAGS;
  tmpin.dev = 0;
  tmpout.dev = 0;
  tmpin.count = 0;
  tmpout.count = 0;

  if(memcmp(&tmpin, &tmpout, sizeof(Audio_d)) != 0) {

    sprint(p+strlen(p), "%s\n", Audio_Out_String);

    /* encode */
    if(! svpmatchv(audio_enc_tbl, audio_enc_sz, &s, out->enc))
      return 0;
    else
      sprint(p+strlen(p), "%s %s", Audio_Enc_String, s);
    
    /* rest of encoding */
    sv = audio_enc_tbl;
    for(i = 0; i < audio_enc_sz; i++, sv++)
      if(sv->v != out->enc && sv->s)
        sprint(p+strlen(p), " %s", sv->s);
    sprint(p+strlen(p), "\n");

    /* rate */
    if(! svpmatchv(audio_rate_tbl, audio_rate_sz, &s, out->rate))
      return 0;
    else
      sprint(p+strlen(p), "%s %s", Audio_Rate_String, s);

    /* rest of rates */
    sv = audio_rate_tbl;
    for(i = 0; i < audio_rate_sz; i++, sv++)
      if(sv->v != out->rate && sv->s)
        sprint(p+strlen(p), " %s", sv->s);
    sprint(p+strlen(p), "\n");

    /* bits */
    if(! svpmatchv(audio_bits_tbl, audio_bits_sz, &s, out->bits))
      return 0;
    else
      sprint(p+strlen(p), "%s %s", Audio_Bits_String, s);

    /* rest of bits */
    sv = audio_bits_tbl;
    for(i = 0; i < audio_bits_sz; i++, sv++)
      if(sv->v != out->bits && sv->s)
        sprint(p+strlen(p), " %s", sv->s);
    sprint(p+strlen(p), "\n");

    /* channels */
    if(! svpmatchv(audio_chan_tbl, audio_chan_sz, &s, out->chan))
      return 0;
    else
      sprint(p+strlen(p), "%s %s", Audio_Chan_String, s);

    /* rest of channels */
    sv = audio_chan_tbl;
    for(i = 0; i < audio_chan_sz; i++, sv++)
      if(sv->v != out->chan && sv->s)
        sprint(p+strlen(p), " %s", sv->s);
    sprint(p+strlen(p), "\n");

    /* left channel */
    sprint(p+strlen(p), "%s %d %d %d\n", 
      Audio_Left_String, out->left, Audio_Min_Val, Audio_Max_Val);

    /* right channel */
    sprint(p+strlen(p), "%s %d %d %d\n", 
      Audio_Right_String, out->right, Audio_Min_Val, Audio_Max_Val);

    /* buf */
    sprint(p+strlen(p), "%s %d %d %d\n", 
      Audio_Buf_String, out->buf, Audio_Min_Val, Audio_Max_Val);

    /* count */
    sprint(p+strlen(p), "%s %d\n", 
      Audio_Count_String, out->count);

    sprint(p+strlen(p), "%s\n", Audio_In_String);
  }

  /* encode */
  if(! svpmatchv(audio_enc_tbl, audio_enc_sz, &s, in->enc))
    return 0;
  else
    sprint(p+strlen(p), "%s %s", Audio_Enc_String, s);
  
  /* rest of encoding */
  sv = audio_enc_tbl;
  for(i = 0; i < audio_enc_sz; i++, sv++) 
    if(sv->v != in->enc && sv->s)
      sprint(p+strlen(p), " %s", sv->s);
  sprint(p+strlen(p), "\n");

  /* rate */
  if(! svpmatchv(audio_rate_tbl, audio_rate_sz, &s, in->rate))
    return 0;
  else
    sprint(p+strlen(p), "%s %s", Audio_Rate_String, s);

  /* rest of rates */
  sv = audio_rate_tbl;
  for(i = 0; i < audio_rate_sz; i++, sv++)
    if(sv->v != in->rate && sv->s)
      sprint(p+strlen(p), " %s", sv->s);
  sprint(p+strlen(p), "\n");

  /* bits */
  if(! svpmatchv(audio_bits_tbl, audio_bits_sz, &s, in->bits))
    return 0;
  else
    sprint(p+strlen(p), "%s %s", Audio_Bits_String, s);

  /* rest of bits */
  sv = audio_bits_tbl;
  for(i = 0; i < audio_bits_sz; i++, sv++)
    if(sv->v != in->bits && sv->s)
      sprint(p+strlen(p), " %s", sv->s);
  sprint(p+strlen(p), "\n");

  /* channels */
  if(! svpmatchv(audio_chan_tbl, audio_chan_sz, &s, in->chan))
    return 0;
  else
    sprint(p+strlen(p), "%s %s", Audio_Chan_String, s);

  /* rest of channels */
  sv = audio_chan_tbl;
  for(i = 0; i < audio_chan_sz; i++, sv++)
    if(sv->v != in->chan && sv->s)
      sprint(p+strlen(p), " %s", sv->s);
  sprint(p+strlen(p), "\n");

  /* left channel */
  sprint(p+strlen(p), "%s %d %d %d\n", 
    Audio_Left_String, in->left, Audio_Min_Val, Audio_Max_Val);

  /* right channel */
  sprint(p+strlen(p), "%s %d %d %d\n", 
    Audio_Right_String, in->right, Audio_Min_Val, Audio_Max_Val);

  /* buf */
  sprint(p+strlen(p), "%s %d %d %d\n", 
    Audio_Buf_String, in->buf, Audio_Min_Val, Audio_Max_Val);

  /* count */
  sprint(p+strlen(p), "%s %d\n", 
    Audio_Count_String, in->count);

  return strlen(p)+1;
}

void
audio_info_init(Audio_t *t)
{
	t->in = Default_Audio_In;
	t->out = Default_Audio_Out;
	return;	
}

Dev audiodevtab = {
        'A',
        "audio",

        audioinit,
        audioattach,
        audioclone,
        audiowalk,
        audiostat,
        audioopen,
        devcreate,
        audioclose,
        audioread,
        audiobread,
        audiowrite,
        audiobwrite,
        devremove,
        devwstat
};

