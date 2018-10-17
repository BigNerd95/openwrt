/*
 *  linux/arch/arm/mach-pnx8181/irq-ext.c
 *
 *  Copyright (C) 2008 DPSG Technologies GmbH, Nuremberg
 *  written by Sebastian Hess <sh@emlix.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <asm/irq.h>
#include <asm/fiq.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/arch/irqs.h>
#include <asm/io.h>

/* Shorten the macro */
#define IRQ_BASE CONFIG_ARCH_PNX8181_EXTINT_BASE
#define NR_EXTINTS 24

#if defined(CONFIG_ARCH_PNX8181_EXTINT_IRQ1)
	#define EXTINT_ENABLE 0x60
#elif defined(CONFIG_ARCH_PNX8181_EXTINT_IRQ2)
	#define EXTINT_ENABLE 0x64
#elif defined(CONFIG_ARCH_PNX8181_EXTINT_IRQ3)
	#define EXTINT_ENABLE 0x68
#else
	#error You need to set the external interrupt
#endif


/*
 * We request this mem regions even that the IRQ Core doesn't
 * understand or use platform_devices. It gives a good user
 * impression to see all used resources in /proc/iomem
 */
static struct resource pnx_extirq_res[] = {
	{
		.name = "extint-registers",
		.flags = IORESOURCE_MEM,
		.start = 0xC2105000,
		.end =   0xC2105070,
	},
	{
		.name = "extint-interrupt",
		.flags = IORESOURCE_IRQ,
		.start = 1,
		.end = 3,
	},
};

/* This lowlevel part can't use the correct platform way, this global struct
 * gives us access to the registers
 */
struct {
	/* The configuration of the IRQs */
	u8 *cfg[24];
	u32 *enable;
	u32 *status;
} registers;

/*
 * pnx_extirq_irq
 *
 * Handle the EXTINT IRQ. This IRQ is called for every external source, we must
 * look which bits are set and call the handler registered to this IRQ
 */
static irqreturn_t pnx_extirq_irq(int irq, void *dev_id)
{
	int i;
	int irqno;
	int regsave;
	struct irq_desc *desc;

	regsave = __raw_readl(registers.status);

	for (i = 0; i < NR_EXTINTS; i++) {
		/* Only send enabled IRQs */
		if ((regsave & (1<<i)) && (*registers.enable & (1<<i))) {
			/* Calc the correct IRQ */
			irqno = i + IRQ_BASE;
			desc = irq_desc + irqno;

			/* Mark the IRQ on the Linux side */
			desc_handle_irq(irqno, desc);
		}
	}

	return IRQ_HANDLED;
}

/*
 * pnx_extirq_mask
 *
 * Disable the given IRQ in the controller
 */
static void pnx_extirq_mask(unsigned int irqno)
{
	u32 enabled = __raw_readl(registers.enable);
	int irq = irqno - IRQ_BASE;

	if (irq < NR_EXTINTS)
		__raw_writel(enabled & ~(1<<irq), registers.enable);
}

/*
 * pnx_extirq_unmask
 *
 * Enable the given IRQ in the controller
 */
static void pnx_extirq_unmask(unsigned int irqno)
{
	u32 enabled = __raw_readl(registers.enable);
	int irq = irqno - IRQ_BASE;

	/* Enable this IRQ in the Hardware */
	if (irq < NR_EXTINTS)
		__raw_writel(enabled | (1<<irq), registers.enable);
}

/*
 * pnx_extirq_ack
 *
 * Acknowledge the IRQ
 */
static void pnx_extirq_ack(unsigned int irqno)
{
	u32 status = __raw_readl(registers.enable);
	int irq = irqno - IRQ_BASE;

	/* Clear the Interrupt flag */
	if (irq < NR_EXTINTS)
		__raw_writel(status & ~(1<<irq), registers.status);
}

/* Set the type of the IRQ, this descriptes level or edge for example */
static int pnx_extirq_type(unsigned irqno, unsigned type)
{
	int polarity = 0x0; /* falling edge as default */
	int mode = 0x0; /* level as default */
	int irq = irqno - IRQ_BASE;
	u8 regsave;

	if (irq >= NR_EXTINTS)
	    return -EINVAL;

	/* Remove unneeded settings */
	type &= IRQ_TYPE_SENSE_MASK;

	/* Activate both edges mode */
	if (type & IRQ_TYPE_EDGE_BOTH)
		mode = 0x3;

	/* Check for one edge and activate the debounce logic */
	if ((type & IRQ_TYPE_EDGE_FALLING) || (type & IRQ_TYPE_EDGE_RISING))
		mode = 0x2;

	/* Check for polarities other then falling/low*/
	if ((type & IRQ_TYPE_LEVEL_HIGH) || (type & IRQ_TYPE_EDGE_RISING))
		polarity = 0x1;

	/* Now do the real changes */
	pnx_extirq_mask(irqno);

	/* Use a local copy to avoid problems with the configuration */
	regsave = __raw_readb(registers.cfg[irq]);
	regsave &= ~(0x3<<6|0x1<<2);
	regsave |= (mode<<6)|(polarity<<2);
	__raw_writeb(regsave, registers.cfg[irq]);

	/* Reenable the IRQ */
	pnx_extirq_unmask(irqno);

	return 0;
}

static struct irq_chip pnx_irq_chip = {
	.name		= "external",
	.ack		= pnx_extirq_ack,	/* clear the interrupt */
	.mask		= pnx_extirq_mask,	/* disable irqs */
	.unmask		= pnx_extirq_unmask,	/* enable irqs */
	.set_type	= pnx_extirq_type,	/* Set level / edge */
};

/*
 * pnx_extint_debounce
 *
 * Setup the debounce functionality for the given IRQ
 */
int pnx_extint_debounce(unsigned int irqno, u8 duration)
{
	int irq = irqno - IRQ_BASE;
	u8 regsave;
	if (duration > 7 || irq >= NR_EXTINTS)
		return -EINVAL;

	/* Mask the IRQ while chaning the configuration */
	pnx_extirq_mask(irqno);

	/* Use a local copy to avoid problems with the configuration */
	regsave = __raw_readb(registers.cfg[irq]);
	regsave &= ~(0x7<<3);
	regsave |= (duration<<3);
	__raw_writeb(regsave, registers.cfg[irq]);

	/* Reenable the IRQ */
	pnx_extirq_unmask(irqno);

	return 0;
}

/*
 * pnx_extint_mode
 *
 * Setup the mode for the given IRQ
 */
int pnx_extint_mode(unsigned int irqno, u8 mode)
{
	int irq = irqno - IRQ_BASE;
	u8 regsave;
	if (mode > 3 || irq >= NR_EXTINTS)
		return -EINVAL;

	/* Mask the IRQ while chaning the configuration */
	pnx_extirq_mask(irqno);

	/* Use a local copy to avoid problems with the configuration */
	regsave = __raw_readb(registers.cfg[irq]);
	regsave &= ~(0x3<<6);
	regsave |= (mode<<6);
	__raw_writeb(regsave, registers.cfg[irq]);

	/* Reenable the IRQ */
	pnx_extirq_unmask(irqno);

	return 0;
}

/*
 * pnx_extint_pol
 *
 * Setup the polarity for the given IRQ
 * (maybe this should be moved to the generic level)
 */
int pnx_extint_pol(unsigned int irqno, u8 pol)
{
	int irq = irqno - IRQ_BASE;
	u8 regsave;
	if (irq >= NR_EXTINTS)
		return -EINVAL;

	/* Mask the IRQ while chaning the configuration */
	pnx_extirq_mask(irqno);

	/* Use a local copy to avoid problems with the configuration */
	regsave = __raw_readb(registers.cfg[irq]);
	regsave &= ~(0x1<<2);
	regsave |= ((pol&0x1)<<2);
	__raw_writeb(regsave, registers.cfg[irq]);

	/* Reenable the IRQ */
	pnx_extirq_unmask(irqno);

	return 0;
}

/*
 * pnx_extint_input
 *
 * Setup input selection (pin or alternate)
 */
int pnx_extint_input(unsigned int irqno, u8 sel)
{
	int irq = irqno - IRQ_BASE;
	u8 regsave;
	if (irq >= NR_EXTINTS)
		return -EINVAL;

	pnx_extirq_mask(irqno);

	regsave = __raw_readb(registers.cfg[irq]);
	regsave &= ~(0x1 << 1);
	regsave |= ((sel & 0x1) << 1);
	__raw_writeb(regsave, registers.cfg[irq]);

	pnx_extirq_unmask(irqno);

	return 0;
}

/**
 * pnx_extint_init
 */
static int __init pnx_extint_init(void)
{
	int i;
	int err;
	int len;
	struct resource *mres;
	struct clk *clock;

	resource_size_t start =
	    (resource_size_t) ((u32 *) pnx_extirq_res[0].start);

	/* Mark the Registers used */
	len = pnx_extirq_res[0].end - pnx_extirq_res[0].start + 1;

	mres = request_mem_region(start, len, pnx_extirq_res[0].name);

	if (!mres) {
		printk(KERN_ERR "%s: Unable to get the register memory\n",
			__func__);
		return -ENODEV;
	}

	/* Activate the needed clock */
	clock = clk_get(NULL, "extint");
	if (!clock)
		printk(KERN_ERR "%s: Clocking failed, trying to go on\n",
			__func__);
	else
		clk_enable(clock);

	/* map the registers */
	for (i = 0; i < NR_EXTINTS; i++)
		registers.cfg[i] = (void *) (start + i*4);

	/* The register is choosen depending on the kernel configuration */
	registers.enable = (void *) (start + EXTINT_ENABLE);
	registers.status = (void *) (start + 0x6C);

	/* Disable all EXTINTS */
	__raw_writel(0x00, (void *) (start + 0x60));
	__raw_writel(0x00, (void *) (start + 0x64));
	__raw_writel(0x00, (void *) (start + 0x68));
	__raw_writel(0x00, registers.status);

	/* Now we need to request the IRQ lines of the EXTINT controller */
	for (i = pnx_extirq_res[1].start;
			i <= pnx_extirq_res[1].end; i++) {
		err = request_irq(i, pnx_extirq_irq,
					IRQF_DISABLED | IRQ_TYPE_LEVEL_LOW,
					pnx_extirq_res[1].name, &registers);
		if (err < 0)
			printk(KERN_ERR "%s: Failed to register IRQ %d\n",
				__FUNCTION__, i);
	}

	/* The hardware is ready now register the IRQs above 100 */
	for (i = IRQ_BASE; i < IRQ_BASE+NR_EXTINTS; i++) {
		set_irq_chip(i, &pnx_irq_chip);
		set_irq_handler(i, handle_level_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	printk(KERN_INFO "External Interrupt Controller registered\n");
	return 0;
}

subsys_initcall(pnx_extint_init);
