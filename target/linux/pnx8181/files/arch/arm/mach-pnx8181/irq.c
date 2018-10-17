/*
 *  linux/arch/arm/mach-pnx8181/irq.c
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
#include <linux/module.h>
#include <asm/irq.h>
#include <asm/fiq.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/arch/irqs.h>
#include <asm/io.h>

#define REQ_SET_SWINT (1<<30)
#define REQ_CLR_SWINT (1<<29)

#define REQ_WE_PRIO   (1<<28)
#define REQ_WE_TARGET (1<<27)
#define REQ_WE_ENABLE (1<<26)
#define REQ_WE_ACTLOW (1<<25)
#define REQ_WE_ALL    (REQ_WE_PRIO|REQ_WE_TARGET|REQ_WE_ENABLE|REQ_WE_ACTLOW)

#define REQ_ACTLOW    (1<<17)
#define REQ_ENABLE    (1<<16)
#define REQ_TARGET    (1<<8)
#define REQ_PRIO(x)   ((x) & 0xF)

/* The default priority setup */
#define REQ_DEFAULT   (REQ_WE_PRIO|REQ_PRIO(5))

/*
 * We request this mem regions even that the IRQ Core doesn't
 * understand or use platform_devices. It gives a good user
 * impression to see all used resources in /proc/iomem
 */
#define PNX_IRQMEM_START 0xC1100000
#define PNX_IRQMEM_END   0xC1200000
static struct resource pnx_irq_res  = {
	.name = "intc-registers",
	.flags = IORESOURCE_MEM,
	.start = PNX_IRQMEM_START,
	.end =   PNX_IRQMEM_END,
};

static inline u32 *get_irq_config_reg(unsigned int irqno)
{
	/* Calcualte the position of the correct request register */
	u32 *reg = (u32 *)(pnx_irq_res.start + 0x400 + (irqno * 4));
	return reg;
}

static void pnx_irq_mask(unsigned int irqno)
{
	u32 *request_reg = get_irq_config_reg(irqno);

	__raw_writel(REQ_CLR_SWINT | REQ_WE_ENABLE | 0, request_reg);
}

/* Enable the Interrupt */
static void pnx_irq_unmask(unsigned int irqno)
{
	u32 *request_reg = get_irq_config_reg(irqno);

	/* On enabling the IRQ we also set the correct priority */
	__raw_writel(REQ_WE_ENABLE | REQ_ENABLE | REQ_DEFAULT, request_reg);
}

static void pnx_irq_ack(unsigned int irqno)
{
	/* We dont need to ack the IRQ, disabling and reanabling is fine */
	pnx_irq_mask(irqno);
}

/* Set the type of the IRQ, this descriptes level or edge for example */
static int gpio_irq_type(unsigned irqno, unsigned type)
{
	/*
	 * The internal IRQ only understands active low and active high,
	 * so we ignore the LEVEL and EDGE part and only look at the polarity
	 */
	int active_low;
	u32 *request_reg = get_irq_config_reg(irqno);

	/* Remove not needed settings */
	type &= IRQ_TYPE_SENSE_MASK;

	if (type & IRQ_TYPE_LEVEL_LOW)
		active_low = REQ_ACTLOW;
	else
		active_low = 0;

	__raw_writel(REQ_WE_ACTLOW | active_low, request_reg);
	return 0;
}

static struct irq_chip pnx8181_irq_chip = {
	.ack		= pnx_irq_ack,
	.mask		= pnx_irq_mask,		/* disable irqs */
	.unmask		= pnx_irq_unmask,	/* enable irqs */
	.set_type	= gpio_irq_type,	/* set the polarity */
};

/**
 * pnx_init_irq - Initialize Interrupt controller
 */
void __init pnx_init_irq(void)
{
	uint32_t	i;
	int		nr_of_irqs;
	u32		*request_reg;

	/* Now fetch the correct count of interrupts from the hardware */
	nr_of_irqs = __raw_readl(PNX_IRQMEM_START + 0x300) & 0xFF;

	request_reg = (u32 *) (PNX_IRQMEM_START + 0x400);

	/* configure Interrupts for use by Linux */
	for (i = 1; i <= nr_of_irqs; i++) {
		request_reg++;

		/* Setup the default and write protect the settings */
		__raw_writel(REQ_DEFAULT, request_reg);

		/* SW configurations */
		set_irq_chip(i, &pnx8181_irq_chip);
		set_irq_handler(i, handle_level_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	/* Disable the priority masks */
	__raw_writel(0, PNX_IRQMEM_START + 0x0000); /* Allow all IRQs */
	__raw_writel(0, PNX_IRQMEM_START + 0x0004); /* Allow all FIQs */

#ifdef CONFIG_FIQ
	init_FIQ();
#endif
	printk(KERN_INFO "PNX8181: Configured %d Interrupts\n", nr_of_irqs);
}

void trigger_irq(unsigned int irqno)
{
	u32 *request_reg = get_irq_config_reg(irqno);
	__raw_writel(REQ_SET_SWINT, request_reg);
}

EXPORT_SYMBOL(trigger_irq);
