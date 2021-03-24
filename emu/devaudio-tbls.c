svp_t audio_bits_tbl[] = {
	{ Audio_8Bit_String, Audio_8Bit_Val } ,	 /* 8 bits per sample */
	{ Audio_16Bit_String, Audio_16Bit_Val }  /* 16 bits per sample */
} ;
int audio_bits_sz = SVP_SZ(audio_bits_tbl);

svp_t audio_chan_tbl[] = {
	{ Audio_Mono_String, Audio_Mono_Val },		/* 1 channel */
	{ Audio_Stereo_String, Audio_Stereo_Val }	/* 2 channels */
} ;
int audio_chan_sz = SVP_SZ(audio_chan_tbl);

svp_t audio_indev_tbl[] = {
	{ Audio_Mic_String, Audio_Mic_Val }, 		/* input microphone */
	{ Audio_Linein_String, Audio_Linein_Val } 	/* line in */
} ;
int audio_indev_sz = SVP_SZ(audio_indev_tbl);

svp_t audio_outdev_tbl[] = {
	{ Audio_Speaker_String, Audio_Speaker_Val },	/* output speaker */
	{ Audio_Headphone_String, Audio_Headphone_Val },/* head phones */
	{ Audio_Lineout_String, Audio_Lineout_Val }	/* line out */
} ;
int audio_outdev_sz = SVP_SZ(audio_outdev_tbl);

svp_t audio_enc_tbl[] = {
	{ Audio_Ulaw_String, Audio_Ulaw_Val },	/* u-law encoding */
	{ Audio_Alaw_String, Audio_Alaw_Val },	/* A-law encoding */
	{ Audio_Pcm_String, Audio_Pcm_Val }	/* Pulse Code Modulation */
} ;
int audio_enc_sz = SVP_SZ(audio_enc_tbl);

svp_t audio_rate_tbl[] = {
	{ Audio_8K_String, Audio_8K_Val },	/* 8000 samples per second */
	{ Audio_11K_String, Audio_11K_Val },	/* 11025 samples per second */
	{ Audio_22K_String, Audio_22K_Val },	/* 22050 samples per second */
	{ Audio_44K_String, Audio_44K_Val },	/* 44100 samples per second */
} ;
int audio_rate_sz = SVP_SZ(audio_rate_tbl);

svp_t audio_val_tbl[] = {
	{ Audio_Min_String, Audio_Min_Val },	/* min */
	{ Audio_Max_String, Audio_Max_Val } 	/* max */
} ;
int audio_val_sz = SVP_SZ(audio_val_tbl);

#define 	Default_Audio_Bits 	Audio_16Bit_Val 
#define 	Default_Audio_Count 	Audio_Min_Val 
#define 	Default_Audio_Chan 	Audio_Stereo_Val 
#define 	Default_Audio_Indev 	Audio_Mic_Val 
#define 	Default_Audio_Outdev 	Audio_Speaker_Val 
#define 	Default_Audio_Val	Audio_Max_Val 
#define 	Default_Audio_Buf 	Audio_Max_Val 
#define 	Default_Audio_Enc 	Audio_Pcm_Val 
#define 	Default_Audio_Rate 	Audio_8K_Val 
#define 	Default_Audio_Left 	Audio_Max_Val 
#define 	Default_Audio_Right	Audio_Max_Val 


Audio_d Default_Audio_In =  {
	AUDIO_NO_FLAGS,
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
	AUDIO_NO_FLAGS,
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
