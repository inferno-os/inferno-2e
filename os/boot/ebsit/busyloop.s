#include "mem.h"

/* 
 * Micro Delays
 */

TEXT busyloop(SB), $0
	MOVW	R0, R2
	MOVW	4(FP), R1
	CMP	$0, R1
	BEQ	_busydone
_busyloop:
	SUB.S	$1, R0
	BGT	_busyloop
	SUB.S	$1, R1
	MOVW	R2, R0
	BGT	_busyloop
_busydone:
	RET

