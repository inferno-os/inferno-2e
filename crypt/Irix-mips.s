#include <regdef.h>

	.text	
	.align	2
	.globl	umult
	.ent	umult 2
umult:
	.set	reorder
	.frame	sp, 0, ra
	multu	a0,a1
	mfhi	t0
	sw	t0, 0(a2)
	mflo	v0
	j	ra
	.end	umult
