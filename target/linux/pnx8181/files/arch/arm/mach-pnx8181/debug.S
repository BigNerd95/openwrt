/*
 *  linux/arch/arm/mach-pnx8181/debug.S
 *
 *  Routines to get stack and frame pointer of undefined mode
 *
 *  Copyright (C) 2009 DSPG Technologies GmbH
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

#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/vfpmacros.h>
#include <asm/arch/entry-macro.S>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>

ENTRY(get_und_sp)
	mrs r2, cpsr
	mov r3, #0x1f      /* mode mask */
	bic r1, r2, r3
	mov r3, #0x1b      /* und mode */
	orr r1, r1, r3
	msr cpsr_c, r1
	mov r0, r13
	msr cpsr_c, r2
	bx lr

ENTRY(get_und_lr)
	mrs r2, cpsr
	mov r3, #0x1f      /* mode mask */
	bic r1, r2, r3
	mov r3, #0x1b      /* und mode */
	orr r1, r1, r3
	msr cpsr_c, r1
	mov r0, r14
	msr cpsr_c, r2
	bx lr

