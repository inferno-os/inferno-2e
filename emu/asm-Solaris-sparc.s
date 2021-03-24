
	.section	".text", #alloc, #execinstr
	.align		8
	.skip		16
	.global		segflush
	.type		segflush,2

	!	The flush instruction works on 8-byte chunks.
	!	We truncate the pointer and increase the count
	!	to make sure we flush the right range.

	!	SPARC requires 5 instructions after flush to
	!	let the caches settle.  The loop code supplies
	!	the delay instructions.

segflush:				! int segflush(void *p, ulong len)

	and	%o0,-8,%o0		! clear low 3 bits of p
	add	%o1, 7, %o1		! len += 7
1:
	flush	%o0			! synchronize cache
	sub	%o1, 8, %o1		! len -= 8
	cmp	%o1, 0			! if len > 0, repeat
	bg	1b
	add	%o0, 8, %o0		! p += 8 in delay slot

	retl
	add	%g0, %g0, %o0		! return 0
	.size	segflush,(.-segflush)


        .section        ".text", #alloc, #execinstr
        .align          8
        .skip           16
        .global FPsave
        .type   FPsave,2
FPsave:
	retl
	st	%fsr,[%o0]
        .size   FPsave,(.-FPsave)


        .section        ".text", #alloc, #execinstr
        .align          8
        .skip           16
        .global FPrestore
        .type   FPrestore,2
FPrestore:
	retl
	ld	[%o0],%fsr
        .size   FPrestore,(.-FPrestore)

	.section	".text", #alloc, #execinstr
	.align		8
	.skip		16
	.global canlock
	.type	canlock,2

canlock:
	or	%g0,1,%o1
	swap	[%o0],%o1	! o0 points to lock; key is first word
	cmp	%o1,1
	bne	.gotit
	nop
	retl			! key was 1
	or	%g0,0,%o0
.gotit:
	retl			! key was not 1
	or	%g0,1,%o0

	.size	canlock,(.-canlock)

	.section	".text", #alloc, #execinstr
	.align		8
	.skip		16
	.global getcallerpc
	.type getcallerpc, 2
getcallerpc:                  ! ignore argument
	retl                    
	add %i7,0,%o0

	.size   getcallerpc,(.-getcallerpc)

