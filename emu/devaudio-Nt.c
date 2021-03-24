#include <windows.h>
#include <mmsystem.h>
#include "svp.h"

#include <windows.h>
#include "dat.h"
#include "fns.h"
#include "error.h"


#define 	Audio_8Bit_Val		8
#define 	Audio_16Bit_Val		16

#define 	Audio_Mono_Val		1
#define 	Audio_Stereo_Val	2

#define 	Audio_Mic_Val		0
#define 	Audio_Linein_Val	-1

#define		Audio_Speaker_Val	0
#define		Audio_Headphone_Val	-1
#define		Audio_Lineout_Val	-1

#define 	Audio_Pcm_Val		WAVE_FORMAT_PCM
#define 	Audio_Ulaw_Val		(WAVE_FORMAT_PCM+1)
#define 	Audio_Alaw_Val		(WAVE_FORMAT_PCM+2)

#define 	Audio_8K_Val		8000
#define 	Audio_11K_Val		11025
#define 	Audio_22K_Val		22050
#define 	Audio_44K_Val		44100

#define 	Audio_Max_Queue		8

#define BUFLEN		1000

#define NOTINUSE	0x00000000 // header struct is in use
#define INUSE		0x00000001 // header struct is in use
#define INISOPEN	0x00000002 // the microphone is open
#define OUTISOPEN	0x00000004 // the speaker is open
#define MICCLOSEDOWN	0x00000008 // microphone in process of closing down
#define SPEAKCLOSEDOWN	0x00000010 // speaker in process of closing down
#define INPUTISGOING	0x00000020 // microphone is being recorded/read
#define	DATATOREAD	0x00000040 // there is data the user can read
#define OUTSTANDREAD	0x00000080 // buffer sent to microphone and not returned.
#define OUTSTANDWRITE	0x00000100 // buffer sent to speaker and not returned.
#define AUDIOHOLD	0x00000200 // data block waiting to be sent to speaker
#define AUDIOBUSY	0x00000400 // data block sent to speak but not yet written
#define ABORTOUTPUT	0x00000800 // flush pending output before close
#define KICKINPUT	0x00001000 // restart the input stream

#include "devaudio.h"
#include "devaudio-tbls.c"

#define DEBUG 0
static int debug = DEBUG;

#define  Ping 0
#define  Pong 1

HWAVEIN audio_file_in;
HWAVEOUT audio_file_out;

long out_buf_count;

typedef struct _awin {
	WAVEHDR hdr;
	long	sz;
	char*	ptr;
	char	data[Audio_Max_Buf];
} AWin ;

AWin audio_ping;
AWin audio_pong;

long paddle = Ping;
int ping_is_filling;
int pong_is_filling;

long audio_flags = 0;
int audio_init = 0;

Rendez audioRendezRead;
Rendez audioRendezWrite;

Lock flag_lock;

Audio_t av;

HANDLE outlock;
HANDLE inlock;

static int audio_open_in(HWAVEIN*, Audio_d*);
static int audio_open_out(HWAVEOUT*, Audio_d*);
static void audio_close_in(void);
static void audio_close_out(void);
static void CALLBACK waveInProc(HWAVEIN, UINT, DWORD, DWORD, DWORD);
static void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD, DWORD, DWORD);
static void getMemory(char *);

#define AUDIOIN  0
#define AUDIOOUT  1
/* 
* Error routines
*/
int
AudioError(unsigned int code, int in_out, char *msg)
{
	char errorText[MAXERRORLENGTH];

	if (code != MMSYSERR_NOERROR) {
		switch(in_out) {
		case AUDIOIN:
			waveInGetErrorText(code, errorText, sizeof(errorText));
			//print("ERROR -- %s: %s\n", msg, errorText);
			return(-1);
		case AUDIOOUT:
			waveOutGetErrorText(code, errorText, sizeof(errorText));
			//print("ERROR -- %s: %s\n", msg, errorText);
			return(-1);
		default:
			print("%s: Unknown device\n", msg);
		}
	}
	//print("TRACE %s\n", msg);
	return 0;
}

void
audio_file_init(void)
{
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
	if(omode==OREAD) {

		lock(&flag_lock);

		if(waserror()) {
			nexterror();
			unlock(&flag_lock);
			return 0;
		}

		if(audio_flags & INISOPEN) {
			error(Einuse);
			return 0;
		}

		inlock = CreateMutex(NULL, FALSE, NULL);
		if(inlock == NULL) {
			error(Einuse);
			return 0;
		}

		if(! audio_open_in(&audio_file_in, &av.in) ) {
			CloseHandle(inlock);
			error(Ebadarg);
			return 0;
		}

		ping_is_filling = 0;
		pong_is_filling = 0;
		paddle = Ping;
		audio_flags |= INISOPEN;

		poperror();
		unlock(&flag_lock);
	} else
	if(omode==OWRITE) {
		lock(&flag_lock);
		if(waserror()){
		  unlock(&flag_lock);
			nexterror();
			return 0;
		}

		if(audio_flags & OUTISOPEN) {
			error(Einuse);
			return 0;
		}

		outlock = CreateMutex(NULL, FALSE, NULL);
		if(outlock == NULL) {
			error(Einuse);
			return 0;
		}

		if(! audio_open_out(&audio_file_out, &av.out) ) {
			CloseHandle(outlock);
			error(Ebadarg);
			return 0;
		}

		out_buf_count = 0;
		audio_flags |= OUTISOPEN;
		audio_flags &= ~SPEAKCLOSEDOWN;

		poperror();
		unlock(&flag_lock);
	} else if(omode==ORDWR) {
	int in_is_open = 0;

		lock(&flag_lock);
		if(waserror()){
		  unlock(&flag_lock);
			if(in_is_open)
				audio_close_in();
			nexterror();
		}

		if((audio_flags & INISOPEN) || (audio_flags & OUTISOPEN)) {
			error(Einuse);
			return 0;
		}

		if(! audio_open_in(&audio_file_in, &av.in) ) {
			error(Ebadarg);
			return 0;
		}

		in_is_open = 1;

		if(! audio_open_out(&audio_file_out, &av.out))  {
			CloseHandle(outlock);
			error(Ebadarg);
			return 0;
		}

		inlock = CreateMutex(NULL, FALSE, NULL);
		if(inlock == NULL) {
			error(Einuse);
			return 0;
		}

		outlock = CreateMutex(NULL, FALSE, NULL);
		if(outlock == NULL) {
			CloseHandle(inlock);
			error(Einuse);
			return 0;
		}

		audio_flags |= INISOPEN;
		audio_flags |= OUTISOPEN;
		ping_is_filling = 0;
		pong_is_filling = 0;
		paddle = Ping;
		out_buf_count = 0;

		poperror();
		unlock(&flag_lock);
	} else
		return 0;
	return 1;
}


int
audio_open_in(HWAVEIN* h, Audio_d* d)
{
	HWAVEIN th;
	WAVEFORMATEX format; 

	format.wFormatTag = d->enc;
	format.nChannels = d->chan;
	format.nSamplesPerSec = d->rate;
	format.wBitsPerSample = d->bits;
	format.nBlockAlign = (d->chan * d->bits) / Bits_Per_Byte;
	format.nAvgBytesPerSec = 
		format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize = 0;


	if (AudioError 
		(waveInOpen(&th, (UINT) WAVE_MAPPER, (LPWAVEFORMAT) &format,
		waveInProc, 0, CALLBACK_FUNCTION), 
		AUDIOIN,
		"audio_open_in opening microphone") == 0) {
			*h = th;
			return 1;
	}
	return 0;
}


int
audio_open_out(HWAVEOUT* h, Audio_d* d)
{
	unsigned int code;
	HWAVEOUT th;
	DWORD vol;
	DWORD l, r;
	WAVEFORMATEX format; 

	format.wFormatTag = d->enc;
	format.nChannels = d->chan;
	format.nSamplesPerSec = d->rate;
	format.wBitsPerSample = d->bits;
	format.nBlockAlign = (d->chan * d->bits) / Bits_Per_Byte;
	format.nAvgBytesPerSec = 
		format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize = 0;

	code = waveOutOpen(&th, (UINT) WAVE_MAPPER, (LPWAVEFORMAT) &format, 
		waveOutProc, 0, CALLBACK_FUNCTION);

	if (AudioError(code, AUDIOOUT, "audio_out_open open speaker") == 0) {
		out_buf_count = 0;
		*h = th;
		return 1;
	}

	return 0;
}


void
audio_file_close(Chan *c)
{
	if (c->mode == OREAD) {
		lock(&flag_lock);
		audio_close_in();
		audio_flags &= ~INISOPEN;
		audio_flags &= ~INPUTISGOING;
		CloseHandle(inlock);
		unlock(&flag_lock);
	} else 
	if (c->mode == OWRITE) {
		lock(&flag_lock);
		audio_close_out();
		audio_flags &= ~OUTISOPEN;
		CloseHandle(outlock);
		unlock(&flag_lock);
	} else 
	if (c->mode == ORDWR) {
		lock(&flag_lock);
		audio_close_in();
		audio_close_out();

		audio_flags &= ~INISOPEN;
		audio_flags &= ~INPUTISGOING;
		audio_flags &= ~OUTISOPEN;

		CloseHandle(outlock);
		CloseHandle(inlock);
		unlock(&flag_lock);
	}
}


void
audio_close_in()
{
	AudioError(waveInStop(audio_file_in), AUDIOIN, "audio_close_in Stop");
	AudioError(waveInReset(audio_file_in), AUDIOIN, "audio_close_in Reset");
	AudioError(waveInUnprepareHeader(audio_file_in, &audio_ping.hdr, 
			sizeof(WAVEHDR)), AUDIOIN, "in un prepare ping header");
	AudioError(waveInUnprepareHeader(audio_file_in, &audio_pong.hdr, 
			sizeof(WAVEHDR)), AUDIOIN, "in un prepare pong header");
	AudioError(waveInClose(audio_file_in), AUDIOIN, "in close");

	audio_ping.sz = 0;
	audio_ping.ptr = &audio_ping.data[0];
	audio_pong.sz = 0;
	audio_pong.ptr = &audio_pong.data[0];
	return;
}


void
audio_close_out()
{
int i;
	
again:
	WaitForSingleObject(outlock, INFINITE);
	while(out_buf_count > 0) {
		ReleaseMutex(outlock);
		sleep(0);
		goto again;
	}
	ReleaseMutex(outlock);

	AudioError(waveOutReset(audio_file_out), AUDIOOUT, "close wave out reset");
	AudioError(waveOutClose(audio_file_out), AUDIOOUT, "closing out device");
	return;
}


long
audio_file_read(Chan *c, void *va, long count, ulong offset)
{
	MMRESULT status;
	long len = av.in.buf * Audio_Max_Buf / Audio_Max_Val;
	char *v = (char *) va;
	char *p;
	long ba;
	long n;
	long chunk;
	long total;


 	lock(&flag_lock);
	WaitForSingleObject(inlock, INFINITE);

	if(waserror()) {
		AudioError(waveInStop(audio_file_in), AUDIOIN, 
			"audio_file_read Stop 1");
		AudioError(waveInReset(audio_file_in), AUDIOIN, 
			"audio_file_read Reset 1");
		AudioError(waveInUnprepareHeader(audio_file_in, 
			&audio_ping.hdr, sizeof(WAVEHDR)), AUDIOIN, 
			"in unprepare ping");
		AudioError(waveInUnprepareHeader(audio_file_in, 
			&audio_pong.hdr, sizeof(WAVEHDR)), 
			AUDIOIN, "in unprepare pong");

		audio_ping.sz = 0;
		audio_ping.ptr = &audio_ping.data[0];
		audio_pong.sz = 0;
		audio_pong.ptr = &audio_pong.data[0];

		ping_is_filling = pong_is_filling = 0;
		paddle = Ping;

		unlock(&flag_lock);
		ReleaseMutex(inlock);

		nexterror();
	}

	if(! (audio_flags & INISOPEN)) {
		error(Eperm);
		return -1;
	}

	/* check for block alignment */
	ba = av.in.bits * av.in.chan / Bits_Per_Byte;

	if(count % ba) {
		error(Ebadarg);
		return -1;
	}

	if(len < 1) {
		error(Ebadarg);
		return -1;
	}

	if(! (audio_flags & INPUTISGOING)) {
		if(AudioError(waveInStart(audio_file_in), AUDIOIN, 
			"in start") == -1)
				error(Eio);

		audio_ping.sz = 0;
		audio_ping.ptr = &audio_ping.data[0];
		audio_ping.hdr.lpData = audio_ping.ptr;
		audio_ping.hdr.dwBufferLength = len;  
		audio_ping.hdr.dwUser = Ping;
		audio_ping.hdr.dwFlags = 0;

		status = waveInPrepareHeader(audio_file_in, &audio_ping.hdr, 
			sizeof(WAVEHDR));

		if (AudioError(status, AUDIOIN, "in prepare header") == -1)
			error(Eio);

		audio_pong.sz = 0;
		audio_pong.ptr = &audio_pong.data[0];
		audio_pong.hdr.lpData = audio_pong.ptr;
		audio_pong.hdr.dwBufferLength = len;  
		audio_pong.hdr.dwUser = Pong;
		audio_pong.hdr.dwFlags = 0;

		status = waveInPrepareHeader(audio_file_in, &audio_pong.hdr, 
			sizeof(WAVEHDR));

		if (AudioError(status, AUDIOIN, "in prepare header") == -1)
			error(Eio);

		status = waveInAddBuffer(audio_file_in, &audio_ping.hdr, 
			sizeof(WAVEHDR));
		if (AudioError(status, AUDIOIN, "file_read Add Buffer")== -1){
			waveInUnprepareHeader(audio_file_in, &audio_ping.hdr, 
				sizeof(WAVEHDR));
			audio_ping.sz = 0;
			audio_ping.ptr = &audio_ping.data[0];
			error(Eio);
		}

		ping_is_filling = 1;
		pong_is_filling = 0;
		paddle = Ping;
		audio_flags |= INPUTISGOING;
	}
	poperror();
	ReleaseMutex(inlock);

	total = 0;

draining:

	WaitForSingleObject(inlock, INFINITE);
	if(waserror()) {
		AudioError(waveInStop(audio_file_in), AUDIOIN, 
			"audio_file_read Stop 2");
		AudioError(waveInReset(audio_file_in), AUDIOIN, 
			"audio_file_read Reset 2");
		AudioError(waveInUnprepareHeader(audio_file_in, 
			&audio_ping.hdr, sizeof(WAVEHDR)), AUDIOIN, 
			"in unprepare ping");
		AudioError(waveInUnprepareHeader(audio_file_in, 
			&audio_pong.hdr, sizeof(WAVEHDR)), AUDIOIN, 
			"in unprepare pong");

		audio_ping.sz = 0;
		audio_ping.ptr = &audio_ping.data[0];
		audio_pong.sz = 0;
		audio_pong.ptr = &audio_pong.data[0];

		lock(&flag_lock);
		audio_flags &= ~INPUTISGOING;
		unlock(&flag_lock);

		ReleaseMutex(inlock);
		unlock(&flag_lock);
		nexterror();
	}

	while((total < count) && ((audio_ping.sz > 0) || (audio_pong.sz > 0))) {
		n  = paddle == Ping ? audio_ping.sz : audio_pong.sz;
		p  = paddle == Ping ? audio_ping.ptr : audio_pong.ptr;

		chunk = min(n, count - total);

		memcpy(v+total, p , chunk);

		total += chunk;

		if(paddle == Ping) {
			if(! pong_is_filling) {

				if(AudioError(waveInAddBuffer(audio_file_in,
						&audio_pong.hdr, sizeof(WAVEHDR)), AUDIOIN, 
						"draining ping calling add buffer pong") == -1)
						error(Eio);

				pong_is_filling = 1;
			}

			audio_ping.sz -= chunk;
			if(audio_ping.sz > 0) {
				audio_ping.ptr += chunk;
			} else {
				audio_ping.ptr = &audio_ping.data[0];
				ping_is_filling = 0;
				paddle = Pong;
			}
		} else {
			if(! ping_is_filling) {

				if(AudioError(waveInAddBuffer(audio_file_in,
						&audio_ping.hdr, sizeof(WAVEHDR)), AUDIOIN, 
						"draining pong calling add buffer ping") == -1)
						error(Eio);

				ping_is_filling = 1;
			}

			audio_pong.sz -= chunk;
			if(audio_pong.sz > 0) {
				audio_pong.ptr += chunk;
			} else {
				audio_pong.ptr = &audio_pong.data[0];
				pong_is_filling = 0;
				paddle = Ping;
			}
		}
	}

	poperror();

	ReleaseMutex(inlock);

	if(total == count) {
		unlock(&flag_lock);
		return count;
	}

filling:
	WaitForSingleObject(inlock, INFINITE);
	while((audio_ping.sz < 1) && (audio_pong.sz < 1)) {
		ReleaseMutex(inlock);
		sleep(0);
		goto filling;	
	}
	ReleaseMutex(inlock);

	goto draining;
}


long
audio_file_write(Chan *c, void *va, long count, ulong offset)
{
	MMRESULT status;
	WAVEHDR *hHdr = (WAVEHDR *) NULL;
	char *hData = NULL;
	char *p = (char *) va;
	long ba;
	long bufsz;
	long chunk;
	long total;


	lock(&flag_lock);
	if(waserror()){
		unlock(&flag_lock);
		nexterror();
	}

	if(! (audio_flags & OUTISOPEN)) {
		error(Eperm);
		return -1;
	}

	/* check for block alignment */
	ba = av.out.bits * av.out.chan / Bits_Per_Byte;

	if(count % ba) {
		error(Ebadarg);
		return -1;
	}

	bufsz = av.out.buf * Audio_Max_Buf / Audio_Max_Val;

	if(bufsz < 1) {
		error(Ebadarg);
		return -1;
	}

	total = 0;

	while(total < count) {

again:
	chunk = min(bufsz, count - total);

drain:
	WaitForSingleObject(outlock, INFINITE);
	while(out_buf_count > bufsz) {
		ReleaseMutex(outlock);
		sleep(0);
		goto drain;
	}

	if(out_buf_count == 0)
		AudioError(waveOutReset(audio_file_out), AUDIOOUT, "wave out reset");
	ReleaseMutex(outlock);

	/* 
	 * allocate and lock the memory for the wave header 
	 * and data blocks 
	 */
	hHdr = (WAVEHDR *) malloc(sizeof(WAVEHDR));
	if (!hHdr) {
		error(Enomem);
		return(-1);
	}

	hData = malloc(chunk);
	if (!hData) {
		free(hHdr);
		error(Enomem);
		return(-1);
	}

	/*
	 * initialize the wave header struct
	 */

	/*
	 * copy user data into write Q 
	 */
	memcpy(hData, p+total, chunk);  

	hHdr->lpData = hData;
	hHdr->dwBufferLength = chunk; 
	hHdr->dwBytesRecorded = 0; 
	hHdr->dwUser = chunk;
	hHdr->dwFlags = 0;
	hHdr->dwLoops = 0;
	hHdr->lpNext = 0;
	hHdr->reserved = 0;

	status = 
	waveOutPrepareHeader(audio_file_out, hHdr, sizeof(WAVEHDR));

	if (AudioError(status, AUDIOOUT, "out prepare header") == -1) {
		free(hHdr);
		free(hData);
		error(Eio);
		return -1;
	}

	status =
	waveOutWrite(audio_file_out, hHdr, sizeof(WAVEHDR));

	if (AudioError(status, AUDIOOUT, "out write data") == -1) {
		waveOutUnprepareHeader(audio_file_out, hHdr, sizeof(WAVEHDR));
		free(hHdr);
		free(hData);
		error(Eio);
		return -1;
	}

	WaitForSingleObject(outlock, INFINITE);
	out_buf_count += chunk;
	ReleaseMutex(outlock);

	total += chunk;

	}

	unlock(&flag_lock);
	poperror();
	osmillisleep(1);	/* hack to get around thread scheduler */

	return (count);
}

void CALLBACK
waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	LPWAVEHDR hHdr;
	long count;

	switch(uMsg) {
	case WIM_OPEN:
		break;
	case WIM_CLOSE:
		break;
	case WIM_DATA:
		hHdr = (LPWAVEHDR)dwParam1;
		if(hHdr != NULL) {
			count = hHdr->dwBytesRecorded;
			if(count > 0) {
				WaitForSingleObject(inlock, INFINITE);
				if(hHdr->dwUser == Ping) 
					audio_ping.sz = count;
				else
					audio_pong.sz = count;
				ReleaseMutex(inlock);
			}
		}
		break;
	}
	return;
}


void CALLBACK
waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwOutstance, DWORD dwParam1, DWORD dwParam2)
{
	LPWAVEHDR hHdr;
	long count;

	switch(uMsg) {
	case WOM_DONE:
		hHdr = (LPWAVEHDR)dwParam1;
		if(hHdr != NULL) {
		WaitForSingleObject(outlock, INFINITE);
		out_buf_count -= hHdr->dwUser;
		ReleaseMutex(outlock);
		AudioError(
			waveOutUnprepareHeader(
			audio_file_out, hHdr, sizeof(WAVEHDR)),
			AUDIOOUT, "out un prepare header");
		if(hHdr->lpData != NULL) 
			free(hHdr->lpData);
		free(hHdr);
		}
		break;
	case WOM_CLOSE:
		WaitForSingleObject(outlock, INFINITE);
		out_buf_count = 0;
		ReleaseMutex(outlock);
		break;
	case WOM_OPEN:
		break;
	}
	return;
}


void
getMemory(char *msg)
{
	MEMORYSTATUS myMemStruct;

	print("inside of getMemory: %s\n", msg);
	myMemStruct.dwLength = 0;
	myMemStruct.dwMemoryLoad = 0;
	myMemStruct.dwTotalPhys = 0;
	myMemStruct.dwAvailPhys = 0;
	myMemStruct.dwTotalPageFile = 0;
	myMemStruct.dwAvailPageFile = 0;
	myMemStruct.dwTotalVirtual = 0;
	myMemStruct.dwAvailVirtual = 0;

	GlobalMemoryStatus(&myMemStruct);

	print("\tpercent of memory in use %d\n", myMemStruct.dwMemoryLoad);
	/*
	print("\tbytes of physical memory %d\n", myMemStruct.dwTotalPhys);
	print("\tfree physical memory bytes %d\n", myMemStruct.dwAvailPhys);
	print("\tuser bytes of address space %d\n", myMemStruct.dwTotalVirtual);
	print("\tfree user bytes %d\n", myMemStruct.dwAvailVirtual);
	*/
}



int
audio_ctl_open(Chan *c, int omode)
{
	return 1;
}



void
audio_ctl_close(Chan *c)
{
	return;
}


long
audio_ctl_read(Chan *c, void *va, long count, long offset)
{
	char buf[AUDIO_INFO_MAXBUF+1];
	char *p = va;

	if(waserror()){
		nexterror();
	}

	/* 
	 * check if buffer is big enough for all the info 
	 */
	if(count < AUDIO_INFO_MAXBUF) {
		error(Ebadarg);
		return -1;
	}

	WaitForSingleObject(inlock, INFINITE);
	av.in.count = audio_ping.sz + audio_pong.sz;
	ReleaseMutex(inlock);

	if((count = audio_get_info(buf, &av.in, &av.out)) == 0) {
		error(Ebadarg);
		return -1;
	} 
	poperror();
	return readstr(offset, p, count, buf);
}


long
audio_ctl_write(Chan *c, void *va, long count, long offset)
{
	WAVEFORMATEX format;
	Audio_t tmpav = av;
	int ff;

	ff = av.out.count;
	tmpav.in.flags = AUDIO_NO_FLAGS;
	tmpav.out.flags = AUDIO_NO_FLAGS;

	if(waserror()){
		nexterror();
	}

	if(! audioparse(va, count, &tmpav)) {
		error(Ebadarg);
		return -1;
	}

	if((tmpav.in.enc != Audio_Pcm_Val) || (tmpav.out.enc != Audio_Pcm_Val)){
		error(Ebadarg);
		return -1;
	}

	poperror();

	if(tmpav.in.flags & AUDIO_MOD_FLAG) {
		if(waserror())
			nexterror();

		format.wFormatTag = tmpav.in.enc;
		format.wBitsPerSample = tmpav.in.bits;
		format.nChannels = tmpav.in.chan;
		format.nSamplesPerSec = tmpav.in.rate;
		format.nBlockAlign = 
			(tmpav.in.chan * tmpav.in.bits) / Bits_Per_Byte;
		format.nAvgBytesPerSec = 
			format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;

		if(AudioError(
			waveInOpen(NULL, (UINT) WAVE_MAPPER, 
				(LPWAVEFORMAT) &format, 0, 0, 
				WAVE_FORMAT_QUERY), AUDIOIN, 
			"audio_ctl_write testing microphone open") == -1) {
				error(Ebadarg);
				return -1;
		}
		poperror();

		lock(&flag_lock);

		if(waserror()){
			unlock(&flag_lock);
			nexterror();
		}

		if(audio_flags & INISOPEN) {
			audio_close_in();
			audio_flags &= ~INISOPEN;
			audio_flags &= ~INPUTISGOING;
			if(! audio_open_in(&audio_file_in, &tmpav.in)) {
				error(Eio);
				return -1;
			}
			audio_flags |= INISOPEN;
		}
		poperror();
		unlock(&flag_lock);
	}

	if(tmpav.out.flags & AUDIO_MOD_FLAG) {

		if(waserror()){
			nexterror();
		}

		format.wFormatTag = tmpav.out.enc;
		format.wBitsPerSample = tmpav.out.bits;
		format.nChannels = tmpav.out.chan;
		format.nSamplesPerSec = tmpav.out.rate;
		format.nBlockAlign = 
			(tmpav.out.chan * tmpav.out.bits) / Bits_Per_Byte;
		format.nAvgBytesPerSec = 
			format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;

		if (AudioError(waveOutOpen(NULL, (UINT) WAVE_MAPPER, 
			(LPWAVEFORMAT) &format,
			0, 0, WAVE_FORMAT_QUERY), 
			AUDIOOUT, "audio_ctl_write testing speaker open") == -1) {
				error(Ebadarg);
			return -1;
		}

		poperror();


		lock(&flag_lock);

		if(waserror()){
			unlock(&flag_lock);
			nexterror();
		}

		if(audio_flags & OUTISOPEN) {
			audio_close_out(); 

			audio_flags &= ~ABORTOUTPUT;
			audio_flags &= ~OUTISOPEN;
			if(! audio_open_out(&audio_file_out, &tmpav.out)) {
				error(Eio);
				return -1;
			}
			audio_flags |= OUTISOPEN;
		}
		poperror();
		unlock(&flag_lock);
	}

	tmpav.in.flags = AUDIO_NO_FLAGS;
	tmpav.out.flags = AUDIO_NO_FLAGS;
	tmpav.out.count = ff;

	av = tmpav;

	return count;
}
