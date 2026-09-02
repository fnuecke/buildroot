/* SPDX-License-Identifier: MIT */
/*
 * Entry and M-mode trap glue. The board enters at the link address with the DTB in a1.
 *
 * mscratch permanently holds the firmware stack top: the trap handler must never use the
 * interrupted sp, which is an S-mode virtual address once the kernel runs and dereferences as
 * arbitrary physical memory with translation off.
 */

	.section .text.start
	.globl _start
_start:
	la	sp, _fw_stack_top
	csrw	mscratch, sp
	la	t0, trap_entry
	csrw	mtvec, t0

	la	t0, _bss_start
	la	t1, _bss_end
1:	bgeu	t0, t1, 2f
	sd	zero, 0(t0)
	addi	t0, t0, 8
	j	1b

2:	mv	a0, a1
	call	fw_main
3:	wfi
	j	3b

/* Saves only what the C handler may clobber; callee-saved registers survive the call. */
#define FRAME_SIZE (16 * 8)

	.align	2
trap_entry:
	csrrw	sp, mscratch, sp
	addi	sp, sp, -FRAME_SIZE
	sd	ra, 0*8(sp)
	sd	t0, 1*8(sp)
	sd	t1, 2*8(sp)
	sd	t2, 3*8(sp)
	sd	t3, 4*8(sp)
	sd	t4, 5*8(sp)
	sd	t5, 6*8(sp)
	sd	t6, 7*8(sp)
	sd	a0, 8*8(sp)
	sd	a1, 9*8(sp)
	sd	a2, 10*8(sp)
	sd	a3, 11*8(sp)
	sd	a4, 12*8(sp)
	sd	a5, 13*8(sp)
	sd	a6, 14*8(sp)
	sd	a7, 15*8(sp)

	mv	a0, sp
	call	handle_trap

	ld	ra, 0*8(sp)
	ld	t0, 1*8(sp)
	ld	t1, 2*8(sp)
	ld	t2, 3*8(sp)
	ld	t3, 4*8(sp)
	ld	t4, 5*8(sp)
	ld	t5, 6*8(sp)
	ld	t6, 7*8(sp)
	ld	a0, 8*8(sp)
	ld	a1, 9*8(sp)
	ld	a2, 10*8(sp)
	ld	a3, 11*8(sp)
	ld	a4, 12*8(sp)
	ld	a5, 13*8(sp)
	ld	a6, 14*8(sp)
	ld	a7, 15*8(sp)
	addi	sp, sp, FRAME_SIZE
	csrrw	sp, mscratch, sp
	mret

/* Enters the kernel: a0 = entry, a1 = DTB. */
	.globl enter_supervisor
enter_supervisor:
	csrw	mepc, a0
	mv	a0, zero
	mret
