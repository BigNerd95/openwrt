/*
 *  linux/arch/arm/mach-pnx8181/loadsim-fiq.S - load simulation
 *
 *  Copyright (C) 2010 DSPG Technologies GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/linkage.h>
#include <asm/assembler.h>

#ifdef CONFIG_ARCH_PNX8181_LOADSIM_TIMINGS
# define PNX8181_TIM1_WR	0xC2102008
		
ENTRY(timing)
		.word 0

#endif
		

		.text		

		.global	loadsim_fiq_end
ENTRY(loadsim_fiq_start)
		sub	lr, lr, #4		@ adjust the return address before saving it
		stmfd	sp!, {lr}		@ stack return address (fiq sp-4)
		mrs	r14, spsr		@ get the SPSR
		stmfd	sp!, {r0-r7,r14}	@ save registers

#ifdef CONFIG_ARCH_PNX8181_LOADSIM_TIMINGS
		ldr	r0, =PNX8181_TIM1_WR
		ldr	r0, [r0]		@ load start timer value to r0
#endif

		str	r10, [r9]		@ write intc_edge_clear

		mov	r14, pc			@ setup link register
		mov	pc, r8			@ jump to handler routine

#ifdef CONFIG_ARCH_PNX8181_LOADSIM_TIMINGS
		ldr	r1, =PNX8181_TIM1_WR
		ldr	r1, [r1]		@ load end timer value to r1
		cmp     r1, r0			@ compare them ...
		subpl	r1, r1, r0		@ if end is greater than start, substract start from end ...
		bpl	timing_save		@ ... and jump to where the timer value gets saved

		@ only reached when end timing is less then start timing	
		mov	r2, #0x0000FF00
		add	r2, r2, #0x000000FF
		sub	r0, r2, r0		@ substract <start> from 0xFFFF
		add	r1, r1, r0		@ and add the result to <end> = difference

timing_save:
		ldr	r2, =timing
		ldr	r0, [r2]		@ load old timing value ...
		add	r0, r0, r1		@ ... add the difference of this run to it ...
		str	r0, [r2]		@ ... and save it back
#endif

		ldmfd	sp!, {r0-r7,r14}	@ restore registers
		msr	spsr_cf, r14		@ restore status register from r14
		ldmfd	sp!, {pc}^		@ return from handler using lr, fiq sp-4+4 = sp
loadsim_fiq_end:
