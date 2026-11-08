	.section .crt0, "ax"
	.arm
	.global _start
_start:
	msr	cpsr_c, #0xD3
	ldr	sp, =0x0380FF00
	ldr	r0, =__bss_start
	ldr	r1, =__bss_end
	mov	r2, #0
1:	cmp	r0, r1
	strlo	r2, [r0], #4
	blo	1b
	bl	arm7_main
2:	b	2b
	.pool
