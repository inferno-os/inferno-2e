!

	.section	".text",#alloc,#execinstr
/* 000000	   3 */		.file	"mul.c"

	.section	".text",#alloc,#execinstr
/* 0x0000	   0 */		.align	8
/* 0x0000	     */		.skip	16
! FILE mul.c
!    1		      !typedef unsigned long ulong;
!    2		      !typedef long long int vlong;
!    3		      !umult(ulong m1, ulong m2, ulong *dhi)
!    4		      !{
!
! SUBROUTINE umult
!
! OFFSET    SOURCE LINE	LABEL	INSTRUCTION

                       	.global umult
                       umult:
!    5		      !	vlong product;
!    6		      !	
!    7		      !	product= m1 * m2;
/* 000000	   7 */		umul	%o0,%o1,%o3
/* 0x0004	     */		rd	%y,%o4
!    8		      !	*dhi=product>>32;
!    9		      !	return(product&0xffffffff);
/* 0x0008	   9 */		and	%o3,-1,%o0
/* 0x000c	     */		retl
/* 0x0010	   8 */		st	%o4,[%o2] ! volatile
/* 0x0014	   0 */		.type	umult,2
/* 0x0014	     */		.size	umult,(.-umult)

! Begin Disassembling Stabs
	.xstabs	".stab.index","Xt ; V=3.0 ; R=3.0",60,0,0,0	! (/tmp/acompAAAa003X8:1)
	.xstabs	".stab.index","/home/limbo/cfy/21/crypt; /opt/SUNWspro/bin/../SC3.0.1/bin/cc -fast -S  mul.c -W0,-xp",52,0,0,0	! (/tmp/acompAAAa003X8:2)
! End Disassembling Stabs

! Begin Disassembling Ident
	.ident	"cg: SC3.0.1 13 Jul 1994"	! (unknown:0)
	.ident	"acomp: SC3.0.1 13 Jul 1994 Sun C 3.0.1"	! (/tmp/acompAAAa003X8:4)
! End Disassembling Ident
