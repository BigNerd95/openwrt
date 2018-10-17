/*
 *  linux/arch/arm/mach-pnx8181/loadsim.c - simulates FIQ load
 *
 *  Copyright (C) 2007 NXP Semiconductors Germany GmbH
 *  Dirk Hoerner <dirk.hoerner@nxp.com>
 *  (c) 2007 Juergen Schoew, emlix GmbH <js@emlix.com>
 *
 *  This module uses the DRT block interrupts to generate some dummy FIQ load
 *  on the system. Useful for debugging and testing. Based on the DRT audio
 *  driver.
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
#define DEBUG

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/fiq.h>
#include <asm/hardware.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Semiconductors");

static char loadsim_name[] = "loadsim";

#define FIQ_STACK_SIZE 200
static char fiq_stack[FIQ_STACK_SIZE] __attribute__((aligned(8)));
static volatile int loadsim_counter = 100, loadsim_temp, loadsim_called = 0;
static struct proc_dir_entry *loadsim_procdirentry;

#define PNX8181_BMP_CNT_MOD   0xC1800014

#define FSI_FIQ               64
#define FSI_FIQ_ADDRESS       (PNX8181_INTC + 0x608)
#define FSI_FIQ_MASK          (1 << 22)

#define loadsim_enable_fiq()  do {  enable_fiq(FSI_FIQ); } while (0)
#define loadsim_disable_fiq() do { disable_fiq(FSI_FIQ); } while (0)

#ifdef CONFIG_ARCH_PNX8181_LOADSIM_TIMINGS
static void
loadsim_timing_setup(void)
{
	*(volatile unsigned short *)PNX8181_TIM1_CR = 0;
	*(volatile unsigned char *) PNX8181_TIM1_SR = 0;
	*(volatile unsigned char *) PNX8181_TIM1_PR = 0xFF;
	*(volatile unsigned short *)PNX8181_TIM1_RR = 0;
	*(volatile unsigned short *)PNX8181_TIM1_CR = 1;
}
#endif /* CONFIG_ARCH_PNX8181_LOADSIM_TIMINGS */

extern unsigned int timing;

static void
loadsim_fiq_handler(void)
{
	register int i = 0;

	loadsim_called++;
	if (loadsim_called < 0)
	    loadsim_called = 0;
	while (i < loadsim_counter){
	    i++;
	    loadsim_temp = i;
	}
}


int loadsim_proc_read(char *buf, char **start, off_t offset,
	int size, int *peof, void *data)
{
	int count;

	if( offset > 0 ) {
	        *peof = 1;
		return 0;
	}

	count = snprintf(buf, size,
		"loadsim already %d called\nloadsim count value %d\ncounted upto %d\n",
		loadsim_called, loadsim_counter, loadsim_temp);
	/* *start = 1; */

	return (count>size)? size : count;
}


static int loadsim_proc_write( struct file *instanz,
	const char __user *userbuffer, unsigned long count, void *data )
{
	char state_string[12] = { '\0' };

	if (count > 10)
		count = 10;
	if (copy_from_user(state_string, userbuffer, count))
		return -EFAULT;
	state_string[count] = '\0';
	loadsim_counter = (int)simple_strtoul(state_string, NULL, 0);

	return count;
}


static struct fiq_handler fh = {
	.name = loadsim_name,
};

static int __init
loadsim_fiq_setup(void)
{
	void *fiq_hander_start;
	unsigned int fiq_handler_len;
	unsigned int fiq_stack_ptr;
	struct pt_regs fiq_regs;

	extern unsigned char loadsim_fiq_start, loadsim_fiq_end;
	fiq_hander_start = &loadsim_fiq_start;
	fiq_handler_len = &loadsim_fiq_end - &loadsim_fiq_start;
	fiq_handler_len += 20; /* HACKY: also copy the indirection table */

	if (claim_fiq(&fh)) {
		printk(KERN_ERR "%s(): could not claim fiq.\n", __FUNCTION__);
		return -EFAULT;
	}

	/* stack grows downwards from address */
	fiq_stack_ptr = (unsigned long)&fiq_stack[FIQ_STACK_SIZE];

	fiq_regs.ARM_r8  = (unsigned long)loadsim_fiq_handler;
	fiq_regs.ARM_r9  = FSI_FIQ_ADDRESS;
	fiq_regs.ARM_r10 = FSI_FIQ_MASK;
	fiq_regs.ARM_sp  = fiq_stack_ptr;
	set_fiq_regs(&fiq_regs);

	set_fiq_handler(fiq_hander_start, fiq_handler_len);
	return 0;
}

static int __init
loadsim_init(void)
{
	int ret;

	printk("LoadSIM - FIQ");

#if 0
	loadsim_clk = clk_get(NULL, loadsim_clk_name);
	if (IS_ERR(loadsim_clk))
		return PTR_ERR(loadsim_err);

	ret = clk_enable(loadsim_clk);
	if (ret)
		return ret;
#endif

	/* set the BMP timings */
	writel(0x01E02D00, PNX8181_BMP_CNT_MOD);

	ret = loadsim_fiq_setup();
	if (ret)
		return ret;

#ifdef CONFIG_ARCH_PNX8181_LOADSIM_TIMINGS
	loadsim_timing_setup();
#endif

	loadsim_enable_fiq();
	loadsim_procdirentry = create_proc_entry( "loadsim", 0600, NULL);
	if( loadsim_procdirentry ) {
		loadsim_procdirentry->read_proc  = loadsim_proc_read;
		loadsim_procdirentry->write_proc = loadsim_proc_write;
		loadsim_procdirentry->owner = THIS_MODULE;
		loadsim_procdirentry->data = NULL;
	}
	printk(" ... started\n");
	return 0;
}
module_init(loadsim_init)

static void __exit
loadsim_exit(void)
{
	if (loadsim_procdirentry)
		remove_proc_entry( "loadsim", NULL);
}
module_exit(loadsim_exit)
