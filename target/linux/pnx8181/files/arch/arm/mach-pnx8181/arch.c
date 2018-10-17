/*
 *  linux/arch/arm/mach-pnx8181/arch.c - machine definition
 *
 *  Copyright (C) 2007,2008 NXP Semiconductors, Nuremberg
 *  (c) 2007,2008, Juergen Schoew, emlix GmbH <js@emlix.com>
 *  (c) 2008, Sebastian Hess, emlix GmbH
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/hardware.h>
#include <asm/arch/memory.h>
#include <asm/arch/irqs.h>
#include <asm/arch/timer.h>

static unsigned char firetux_boardrevision = unknown_PNX8181;
static unsigned long firetux_boardfeatures = 0;
unsigned int PNX8181_SCRAM_EXT=0x20f00000;

/*
 * initialization of IO mapping
 */

/* directly map the whole SC register map */
static struct map_desc pnx_io_desc[] __initdata = {
	{
		.virtual  = IO_ADDRESS(IO_PHYS),
		.pfn      = __phys_to_pfn(IO_PHYS),
		.length   = IO_SIZE,
		.type     = MT_DEVICE,
	},
	/* map SCRAM, multiple times */
	{
		.virtual  = PNX8181_SCRAM_VA_MEM,
		.pfn      = __phys_to_pfn(PNX8181_SCRAM_BASE),
		.length   = PNX8181_SCRAM_SIZE,
		.type     = MT_MEMORY,
	},
	{
		.virtual  = PNX8181_SCRAM_VA_MEM + 0x02000000,
		.pfn      = __phys_to_pfn(PNX8181_SCRAM_BASE),
		.length   = PNX8181_SCRAM_SIZE,
		.type     = MT_MEMORY,
	},
	{
		.virtual  = PNX8181_SCRAM_VA_MEM + 0x03000000,
		.pfn      = __phys_to_pfn(PNX8181_SCRAM_BASE),
		.length   = PNX8181_SCRAM_SIZE,
		.type     = MT_MEMORY,
	},
	/* map SCRAM for IP3912 */
	{
		.virtual  = PNX8181_SCRAM_VA_DEV,
		.pfn      = __phys_to_pfn(PNX8181_SCRAM_BASE),
		.length   = PNX8181_SCRAM_SIZE,
		.type     = MT_DEVICE,
	},
	/*
	 * NOTE! This entry is a bit special:
	 * 
	 * It gets modified later on in firetux_fixup(). So make sure
	 * SCRAM_EXT_MAPPING_NR is defined correctly!
	 */
#define SCRAM_EXT_MAPPING_NR 5
	{
		.virtual  = PNX8181_SCRAM_EXT_VA,
		/* pfn dummy value, will be overwritten in fixup */
		.pfn      = __phys_to_pfn(0x20f00000),
		.length   = PNX8181_SCRAM_EXT_SIZE,
		.type     = MT_MEMORY,
	},
	{
		.virtual  = 0xFFFF1000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x11000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF2000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x12000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF3000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x13000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF4000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x14000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF5000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x15000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF6000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x16000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF7000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x17000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF8000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x18000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFF9000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x19000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFFA000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x1A000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFFB000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x1B000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFFC000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x1C000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFFD000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x1D000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFFE000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x1E000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = 0xFFFFF000,
		.pfn      = __phys_to_pfn(PNX8181_SCROM + 0x1F000),
		.length   = 0x1000,
		.type     = MT_HIGH_VECTORS,
	},
	/* 1:1 SCROM mapping */
	{
		.virtual  = PNX8181_SCROM,
		.pfn      = __phys_to_pfn(PNX8181_SCROM),
		.length   = PNX8181_SCROM_SIZE,
		/* XXX: hacky, allows mapping < SECTION_SIZE */
		.type     = MT_HIGH_VECTORS,
	},
	{
		.virtual  = PNX8181_LCD_VA,
		.pfn      = __phys_to_pfn(PNX8181_LCD_BASE),
		.length   = PNX8181_LCD_SIZE,
		.type     = MT_DEVICE,
	},

};

static void __init pnx_map_io(void)
{
	iotable_init(pnx_io_desc, ARRAY_SIZE(pnx_io_desc));
}

unsigned char firetux_get_boardrevision(void)
{
	return firetux_boardrevision;
}
EXPORT_SYMBOL(firetux_get_boardrevision);

unsigned long firetux_get_boardfeatures(void)
{
	return firetux_boardfeatures;
}
EXPORT_SYMBOL(firetux_get_boardfeatures);

#define IP3912_ETN_MAC1				0x0000
#define IP3912_ETN_SUPP				0x0018
#define IP3912_ETN_MCFG				0x0020
#define IP3912_ETN_MCMD				0x0024
#define IP3912_ETN_MADR				0x0028
#define IP3912_ETN_MWTD				0x002C
#define IP3912_ETN_MRDD				0x0030
#define IP3912_ETN_MIND				0x0034
#define IP3912_ETN_COMMAND			0x0100

#define IP3912_NO_PHY				0xFFFF

static inline void firetux_etn_start(void)
{
	/* reset MII mgmt, set MII clock */
	__raw_writel(0x0000801c, PNX8181_ETN1 + IP3912_ETN_MCFG);
	__raw_writel(0x0000001c, PNX8181_ETN1 + IP3912_ETN_MCFG);

	/* enable RMMI */
	__raw_writel(0x00000400, PNX8181_ETN1 + IP3912_ETN_COMMAND);

	/* reset MAC layer */
	__raw_writel(0x0000cf00, PNX8181_ETN1 + IP3912_ETN_MAC1);
	/* release MAC soft reset */
	__raw_writel(0x00000000, PNX8181_ETN1 + IP3912_ETN_MAC1);
	/* reset rx-path, tx-path, host registers */
	__raw_writel(0x00000038, PNX8181_ETN1 + IP3912_ETN_COMMAND);
	/* reset RMII, 100Mbps MAC, 10Mbps MAC */
	__raw_writel(0x1888, PNX8181_ETN1 + IP3912_ETN_SUPP);
	__raw_writel(0x1000, PNX8181_ETN1 + IP3912_ETN_SUPP);
}

/*
 * Wait for the ETN engine to be ready
 */
static inline int firetux_phy_wait(void)
{
	int i, status;

	for (i = 0; i < 100000; i++) {
		status = __raw_readl(PNX8181_ETN1 + IP3912_ETN_MIND) & 0x7;
		if (!status)
			break;
	}

	return status;
}

/*
 * Write the address to the special function register
 */
static int firetux_match_phy_id(u32 phyaddr, u32 id)
{
	u32 phy_id;
	int result;

	/* read PHY ID 1 register */
	__raw_writel((phyaddr << 8) | 0x02, (PNX8181_ETN1 + IP3912_ETN_MADR));
	__raw_writel(0x00000001, (PNX8181_ETN1 + IP3912_ETN_MCMD));
	if (firetux_phy_wait())
		return 0;

	phy_id = (__raw_readl(PNX8181_ETN1 + IP3912_ETN_MRDD) & 0xffff) << 16;
	__raw_writel(0x00000000, (PNX8181_ETN1 + IP3912_ETN_MCMD));
	udelay(1);

	/* read PHY ID 2 register */
	__raw_writel((phyaddr << 8) | 0x03, (PNX8181_ETN1 + IP3912_ETN_MADR));
	__raw_writel(0x00000001, (PNX8181_ETN1 + IP3912_ETN_MCMD));
	if (firetux_phy_wait())
		return 0;

	phy_id |= __raw_readl(PNX8181_ETN1 + IP3912_ETN_MRDD) & 0xffff;
	__raw_writel(0x00000000, (PNX8181_ETN1 + IP3912_ETN_MCMD));

	result = (phy_id & 0xfffffff0) == (id & 0xfffffff0);
	if (result)
		printk(KERN_INFO "Board: found phy at 0x%02x, id 0x%08x\n",
		       phyaddr, phy_id);
	return result;
}

static void __init firetux_detect_board(void)
{
    /* modified by Norman */
    firetux_etn_start();

	if (firetux_match_phy_id(0x3, 0x001cc815)) /* Platform IV RTL8201 */
	{ 
		firetux_boardfeatures = FEATURE_LC_RTL8201;
		firetux_boardrevision = Vega_PNX8181_BaseStation_LC;
	}
	else /* Platform IV w/ KSZ8873 L2 switch (verify via I2C?) */
	{ 		
    	firetux_boardfeatures = FEATURE_LC_KSZ8873;
    	firetux_boardrevision = Vega_PNX8181_BaseStation_LC;
	}
}

static void __init firetux_machine_init(void)
{
	/* check board version */
	firetux_detect_board();

	printk(KERN_INFO "Board: ");
	switch (firetux_boardrevision) {
	case EZ_MCP_PNX8181:
		printk("EZ_MCP_PNX8181");
		break;
	case Vega_PNX8181_BaseStation_V1:
		printk("Vega_PNX8181_BaseStation Rev.1");
		break;
	case Vega_PNX8181_BaseStation_V2:
		printk("Vega_PNX8181_BaseStation Rev.2");
		break;
	case Vega_PNX8181_BaseStation_V3:
		printk("Vega_PNX8181_BaseStation Rev.3 (III-C)");
		break;
	case Vega_PNX8181_BaseStation_LC:
		printk("Vega_PNX8181_BaseStation low-cost version");
		break;
	case unknown_PNX8181:
	default:
		printk("unknown PNX8181 board");
		break;
	}
	printk(" detected.\n");
}

#ifdef CONFIG_ARCH_PNX8181_KERNEL_WATCHDOG
extern int pnx8181_wdt_driver_active;

static int panic_event(struct notifier_block *this, unsigned long event,
                       void *ptr)
{
	/* disable watchdog updates in timer interrupt handler - there was a
	 * kernel panic and we want the watchdog timer to trigger a reset */
	pnx8181_wdt_driver_active = 1;

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call	= panic_event,
};

static int __init firetux_watchdog_setup(void)
{
	/* Setup panic notifier */
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);

	return 0;
}
postcore_initcall(firetux_watchdog_setup);
#endif

static void __init firetux_fixup(struct machine_desc *desc,
	        struct tag *tags, char **cmdline, struct meminfo *mi)
{
	int memsize = 0;
	struct tag *t;

	if (tags->hdr.tag != ATAG_CORE) {
	    printk("Opps, no tags set by uboot\n");
	} else {
	    for_each_tag(t, tags)
	    if (t->hdr.tag == ATAG_MEM) {
		/* FIXUP */
		memsize = t->u.mem.size;
		PNX8181_SCRAM_EXT = t->u.mem.start +
				    t->u.mem.size -
				    PNX8181_SCRAM_EXT_SIZE;
		t->u.mem.size = t->u.mem.size - PNX8181_SCRAM_EXT_SIZE;
	    }
	}

	/* ugly fixup for cordless coma */
	pnx_io_desc[SCRAM_EXT_MAPPING_NR].pfn =
		__phys_to_pfn(PNX8181_SCRAM_EXT);
}

/*
 * definition of the machine
 * this just pulls in all the above initialization functions
 */
MACHINE_START(PNX8181, "NXP PNX8181")
	.phys_io	= IO_PHYS,
	.io_pg_offst	= ((IO_PHYS) >> 18) & 0xfffc,
	.boot_params	= 0x20000100,
	.map_io		= pnx_map_io,
	.init_machine   = firetux_machine_init,
	.fixup		= &firetux_fixup,
	.init_irq	= pnx_init_irq,
	.timer		= &pnx8181_timer,
MACHINE_END

