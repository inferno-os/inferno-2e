#ifndef _AUDIO_H_
#define _AUDIO_H_

#define	AUDIO_NO_FLAGS		0x00000000
#define	AUDIO_BITS_FLAG		0x00000001
#define	AUDIO_BUF_FLAG		0x00000002
#define	AUDIO_CHAN_FLAG		0x00000004
#define	AUDIO_COUNT_FLAG	0x00000008
#define	AUDIO_DEV_FLAG		0x00000010
#define	AUDIO_ENC_FLAG		0x00000020
#define	AUDIO_RATE_FLAG		0x00000040
#define	AUDIO_VOL_FLAG		0x00000080
#define	AUDIO_LEFT_FLAG		0x00000100
#define	AUDIO_RIGHT_FLAG	0x00000200
#define	AUDIO_IN_FLAG		0x00000400
#define	AUDIO_OUT_FLAG		0x00000800
#define	AUDIO_MOD_FLAG		0x10000000

typedef	unsigned long aflag_t;

#define 	Audio_ok		1
#define 	Audio_error		-1

#define		Audio_Min_Val		0
#define		Audio_Max_Val		100

#define 	Audio_No_Val		0
#define 	Audio_In_Val		1
#define 	Audio_Out_Val		2

#define 	Audio_8Bit_String	"8"
#define 	Audio_16Bit_String	"16"

#define 	Audio_Mono_String	"1"
#define 	Audio_Stereo_String	"2"

#define 	Audio_Mic_String	"mic"
#define 	Audio_Linein_String	"line"

#define		Audio_Speaker_String	"spkr"
#define		Audio_Headphone_String	"hdph"	
#define		Audio_Lineout_String	"line"

#define 	Audio_Ulaw_String	"ulaw"
#define 	Audio_Alaw_String	"alaw"
#define 	Audio_Pcm_String	"pcm"

#define 	Audio_8K_String		"8000"
#define 	Audio_11K_String	"11025"	
#define 	Audio_22K_String	"22050"
#define 	Audio_44K_String	"44100"

#define 	Audio_Min_String	"min"
#define 	Audio_Max_String	"max"

#define		Audio_In_String    	"in"
#define		Audio_Out_String	"out"
#define		Audio_Bits_String	"bits"
#define		Audio_Buf_String	"buf"
#define		Audio_Chan_String	"chans"
#define		Audio_Count_String	"count"
#define		Audio_InDev_String	"indev"
#define		Audio_OutDev_String	"outdev"
#define		Audio_Enc_String	"enc"
#define		Audio_Rate_String	"rate"
#define		Audio_Vol_String	"vol"
#define		Audio_Left_String	"left"
#define		Audio_Right_String	"right"

#define		Audio_Max_Buf		32768
#define		Bits_Per_Byte		8

typedef struct _audio_d {
	aflag_t	flags;		/* bit flag for fields */
	ulong	bits;		/* bits per sample */
	ulong	buf;		/* buffer size */
	ulong	chan;		/* number of channels */
	ulong	count;		/* samples remaining */
	ulong	dev;		/* device */
	ulong	enc;		/* encoding format */
	ulong	rate;		/* samples per second */
	ulong	left;		/* left channel gain */
	ulong	right;		/* right channel gain */
} Audio_d ;

typedef struct _audio_t {
	Audio_d	in;		/* input device */
	Audio_d	out;		/* output device */
} Audio_t ;

#define AUDIO_CMD_MAXNUM 32
#define AUDIO_INFO_MAXBUF 512

void audio_info_init(Audio_t*);
int audio_get_info(char*, Audio_d*, Audio_d*);
int audioparse(char*, int n, Audio_t*);

enum
{
	Qdir = 0,		/* must start at 0 representing a directory */
	Qaudio,
	Qaudioctl
};

/* required external platform specific functions */
void	audio_file_init(void);
void	audio_ctl_init(void);
int	audio_file_open(Chan*, int);
int	audio_ctl_open(Chan*, int);
long	audio_file_read(Chan*, void*, long, long);
long	audio_ctl_read(Chan*, void*, long, long);
long	audio_file_write(Chan*, void*, long, long);
long	audio_ctl_write(Chan*, void*, long, long);
void	audio_file_close(Chan*);
void	audio_ctl_close(Chan*);

/* string value pairs for default audio values */
extern svp_t audio_bits_tbl[];
extern svp_t audio_chan_tbl[];
extern svp_t audio_indev_tbl[];
extern svp_t audio_outdev_tbl[];
extern svp_t audio_enc_tbl[];
extern svp_t audio_rate_tbl[];
extern svp_t audio_val_tbl[];

extern int audio_bits_sz;
extern int audio_chan_sz;
extern int audio_indev_sz;
extern int audio_outdev_sz;
extern int audio_enc_sz;
extern int audio_rate_sz;
extern int audio_val_sz;

extern Audio_d Default_Audio_In;
extern Audio_d Default_Audio_Out;

#endif
