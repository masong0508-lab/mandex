@ ARM9 start-up.  The ROM image is linked at 0x02000000 with 0x800 bytes of secure-area filler first; the entry point is here, at 0x02000800.
	.section .crt0, "ax"
	.arm
	.global _start
_start:
#if MX_CRT_STAGE == 1
	@ diagnostic: first instructions only - power the screens, show the backdrop colour (palette entry 0) in red, stop
	ldr	r0, =0x04000304
	ldr	r1, =0x0203
	strh	r1, [r0]
	ldr	r0, =0x04000000
	ldr	r1, =0x00010000
	str	r1, [r0]
	ldr	r0, =0x04001000
	str	r1, [r0]
	ldr	r0, =0x05000000
	ldr	r1, =0x001F
	strh	r1, [r0]
	ldr	r0, =0x05000400
	strh	r1, [r0]
0:	b	0b
	.pool
#endif
	mov	r0, #0x4000000
	mov	r1, #0
	str	r1, [r0, #0x208]		@ IME off (no interrupts used)
	msr	cpsr_c, #0xD3			@ supervisor, IRQ+FIQ masked
	ldr	sp, =0x02380000 - 0x1000	@ svc stack
	msr	cpsr_c, #0xDF			@ system mode
	ldr	sp, =0x02380000 - 0x2000
	ldr	r0, =__bss_start
	ldr	r1, =__bss_end
	mov	r2, #0
1:	cmp	r0, r1
	strlo	r2, [r0], #4
	blo	1b
	bl	main
2:	b	2b
	.pool
