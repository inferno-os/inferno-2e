#ifndef _AUDIO_UNIXWARE_H_
#define _AUDIO_UNIXWARE_H_

#define 	Audio_8Bit_Val		8
#define 	Audio_16Bit_Val		16

#define 	Audio_Mono_Val		1
#define 	Audio_Stereo_Val	2

#define 	Audio_Mic_Val		0
#define 	Audio_Linein_Val	(Audio_Mic_Val+1)

#define		Audio_Speaker_Val	0
#define		Audio_Headphone_Val	(Audio_Speaker_Val+1)
#define		Audio_Lineout_Val	(Audio_Speaker_Val+2)

#define 	Audio_Pcm_Val		0
#define 	Audio_Ulaw_Val		(Audio_Pcm_Val+1)
#define 	Audio_Alaw_Val		(Audio_Pcm_Val+2)

#define 	Audio_8K_Val		8000
#define 	Audio_11K_Val		11025
#define 	Audio_22K_Val		22050
#define 	Audio_44K_Val		44100

#define		Bits_Per_Byte		8

#define 	Audio_Max_Queue		8
#define		Audio_Max_Buf		32768

#endif
