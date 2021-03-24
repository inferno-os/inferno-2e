#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "../port/custom.h"
#include "../port/screen.h"
#include "kbd.h"
#include "io.h"

extern int cmdlock;

void
title(void)
{
	fadeinlogo("F!logo", 1000, 0x600);
}

void
debgboot(void)
{
	*(ulong*)0xb0000000=0xdeb9b007;	// flash for demon (1.06 and up)
					// or styxmon, to tell it to boot
					// in debug mode
	RESETREG->rsrr = 1;
}

void
check_manual_override(int k)
{
	ulong t = timer_start();
	while(timer_ticks(t) < MS2TMR(100) && !kbd_charav())
		;
	while(kbd_charav() || k) {
		ulong t;
		int s;
		char ch = k ? 27 : kbd_getc();
		switch(ch) {
		case 27:
			stdout = conout;
			screen_clear(-1);
			print("Enter maintenance mode?\n\n");
			wait_keyrelease();
			print("ENTER:confirm, ESC:cancel\n");
			t = timer_start();
			while((s = 10-tmr2ms(timer_ticks(t))/1000) >= 0) {
				if(kbd_charav() || k) {
					switch(k ? k : kbd_getc()) {
					case 27:
						goto cancel;
					case 13: case 10:
					case ('p'&0x1f):
						maintenance_mode();
						goto resettimer;
					case ('d'&0x1f):
						setautoboot(0);
						goto resettimer;
					case ('e'&0x1f):
						setautoboot(1);
						goto resettimer;
					case ('a'&0x1f):
						debgboot();
						break;
					case (']'&0x1f):	
						exit(0);
						break;
					case ('w'&0x0f):
						lineartrace();
						goto resettimer;
					case ('c'&0x1f):
						system("t 2 0");
						goto resettimer;
					case ('t'&0x1f):
						screen_clear(-1);
						system("t 0 0");
					resettimer:
						t = timer_start();
						break;
					}
				}
				status_bar("time left", s, 10);
			} cancel:
			print("Resuming autoboot...\n");
			delay(3000);
			break;
		}
	}
	wait_keyrelease();
}

static int key;
int doautoboot;

int
cmd_autoboot(int, char **argv, int *)
{
	doautoboot=1;
	key = argv[0][1];
	autoboot();
	return 0;
}

void
autoboot(void)
{
	Ostream *os, *is;

	text_fg = 0xff;
	text_bg = 0x00;
	text_wid = 1;
	text_hgt = 1;
	is = stdin;
	os = stdout;
	stdout = conout;
	stdin = conin;
	title();
	text_wid = 2;
	text_hgt = 2;
	stdout = nil;
	check_manual_override(key&0x1f);
	cmdlock = 1;
	if(system(". F!plan9.ini") < 0) {
		stdout = os;
		stdin = is;
		print("plan9.ini corruption: %r\n");
		return;
	}
	cmdlock=0;
	sprint(lasterr, "<none?>");
	system("b");
	stdout = os;
	stdin = is;
	print("autoboot: %r\n");
	return;
}

void
autobootlink()
{
	addcmd('A', cmd_autoboot, 0, 0, "autoboot");
}

