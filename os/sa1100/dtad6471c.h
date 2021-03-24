/*
 *	D6471C ``EasyTad'' chip operations
 */

#define	DTAD_BASE 	0x0A000000	/* physical address of TAD */
#define	KEY_PAD_ADDR	0x0e000000	/* physical address for 4x4 Keypad */

void dtad_init( void );

int dtad_idle( void );

enum {
	TAD_WARM	= 0x8880,
	TAD_COLD	= 0x8881,
};

enum {
	/* common status fields when monitoring line */
	ExtendedTone	= 1<<5,
	ToneDetected	= 1<<4,
	ToneMask	= 0xf,
};

enum {
	DRec_Mode=	(0x6 << 7),
	Rec_Fixed=	0,
	Rec_Vari=	1,
	Rec_FixedGap= 	2,
	Rec_VariGap=	3,

	DRec_Vox=	(1 << 6),
	Vox=		1,
	NoVox=		0,

	DRec_LB=	(1 << 5),
	NoLoopBack=	1,
	LoopBack=	0,
};
int dtad_rec(int mode, int vox, int loopback );

enum {
	DRecT_Tgen=	(1 << 9),
	DRecT_MemFull=	(1 << 7),
	DRecT_Vox=	(1 << 6),
	DRecT_Ext=	(1 << 5),
	DRecT_Tone=	(1 << 4),
	DRecT_DTMF=	(0xf),
};
int dtad_rectgen(int tone, int gain, int tailcut);

int dtad_play( int speed, int msgnum );
int dtad_play_offset( int offset );

enum {
	DPlay_Pause=	(1 << 11),
	DPlay_Bin=	(1 << 9),
	DPlay_EOF=	(1 << 7),
	DPlay_Ext=	(1 << 5),
	DPlay_Tone=	(1 << 4),
	DPlay_DTMF=	(0xf),
};
int dtad_play_pause( int pause, int offset, int speed );

enum {
	DMem_VPStat=	(1 << 11),
	DMem_ROM=	(1 << 9),
	DMem_GC=	(1 << 8),
	DMem_Full=	(1 << 7),
	DMem_Num=	(0x7f),
};
int memory_status( int checksum );

int product_number( void );
int number_write( int directory, int word, int number );
int number_read( int directory, int word );

enum {
	DTgen_Ext=	(1 << 5),
	DTgen_Tone=	(1 << 4),
	DTgen_DTMF=	(0xf),
};
int tgen( int gain, int index );
int newtone( int index, int gain1, int gain2, int freq1, int freq2 );

enum {
	DMon_Ext=	(1 << 5),
	DMon_Tone=	(1 << 4),
	DMon_DTMF=	(0xf),
	DCID_ND=	(1 << 8),
	DCID_Data=	(0xff),
};
int linemonitor( int cid, int loopback, int seizemode );

int msg_delete( int gc, int nuke, int msgnum );
int msg_stamp( int modify, int msgnum, int stamp );
ulong msg_getstamp( int msgnum );
ulong get_timeleft(void);

int self_test(int mode, int params);

enum {
	DFlash_Size=	(0x7 << 5),
	DFlash_Dev3=	(1 << 3),
	DFlash_Dev2=	(1 << 2),
	DFlash_Dev1=	(1 << 1),
	DFlash_Dev0=	(1 << 0),
};
int dtadflash_init( int size, int devicemask );
int self_test_flashfast( int size, int devmask );

int flash_sel( int external, int num );
int codec_sel( int type, int input, int output, int law );
int self_test_codecloop( void );
int config_detector( int numdtmf );

int set_volume( int volume );
int get_volume( void );

int set_sensitivity( int system, int level );
int read_sensitivity( int system );

enum {
	DPrompt_Load=	(1 << 11),
	DPrompt_Ready=	(1 << 10),
	DPrompt_EOF=	(1 << 7),
	DPrompt_Ext=	(1 << 5),
	DPrompt_Tone=	(1 << 4),
	DPrompt_DTMF=	(0xf),
};
int prompt_playback( int load, int sector, int speed, int number );

enum {
	DSpkr_RT=	(0x3 << 10),
	DSpkr_TR=	(0x3 << 8),
	DSpkr_Line=	(0x3 << 5),
	DSpkr_Mic=	(0x3 << 2),
	DSpkr_Pri=	(1 << 1),
};
int speakerphone( int rt, int tr, int line, int mike, int priority );

enum {
	DSStat_Dir=	(0xf << 4),
	DSStat_Loop=	(0x3),
};
int spkr_stat(void);

enum {
	DSMon_New=	(1 << 7),
	DSMon_Ext=	(1 << 5),
	DSMon_DTMF=	(0xf),
};
int spkr_monitor(void);

int spkr_volume(int line_vol, int spkr_vol, int atten);
int spkr_config(int twist, int dt, int train, int autotrain);
int spkr_config2(int spkrnoise, int linenoise, int lineproc, int mvox_pos, int mvox_resp, int lvox_resp);
int spkr_tgen(int gain, int index);

enum {
	SPKRPARAM_LOOP= 	0,
	SPKRPARAM_SPKRVOL=	1,
	SPKRPARAM_ACCDEC=	2,
	SPKRPARAM_LINEVOL=	3,
	SPKRPARAM_ELEDEC=	4,
	SPKRPARAM_TRRT=		5,
};

int spkr_param2( int param, int value );

