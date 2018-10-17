/*
 *  linux/arch/arm/mach-pnx8181/irq-devices.c - GPIO-IRQ handling
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

#include <asm/gpio.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <linux/init.h>
#include <linux/kernel.h>

/* 
 * We must setup the deboucing, polarity and the operation mode of the
 * EXTINTs.
 *
 * This is highly platform specific so do it here and keep the
 * drivers free of that stuff.
 *
 * Since this is all about External Interrupts we use the GPIO
 * notation here to make it more readable
 */
#define MODE_BYPASS     0
#define MODE_STRECH     1
#define MODE_DEBOUNCE   2
#define MODE_EDGE       3

#define INPUT_PIN       0
#define INPUT_ALTERNATE 1

static struct {
	unsigned gpio;
	u8 mode;
	u8 pol;
	u8 debounce;
	u8 input;
} irq_setups[] = {
	{ GPIO_PORTA(2),  MODE_DEBOUNCE, 0, 7, INPUT_PIN },
#if 0
	{ GPIO_PORTA(5),  MODE_STRECH, 0, 0, INPUT_PIN },
#endif

	/* set input of extint 13 and 15 to usbvbus and usb_need_clk */
	{ GPIO_PORTA(19), MODE_EDGE, 0, 7, INPUT_ALTERNATE }, /* EXTINT13 */
	{ GPIO_PORTA(21), MODE_EDGE, 0, 0, INPUT_ALTERNATE }, /* EXTINT15 */
};

static __init int irq_device_setup(void) {
	int i;
	int irq;

	/* Mux the correct outputs */
	for(i = 0; i < ARRAY_SIZE(irq_setups); i++)
	{
		irq = gpio_to_irq(irq_setups[i].gpio);
		if(irq < 0)
		{
			printk("%s: GPIO %X is not IRQ mappable\n", __FUNCTION__, 
					irq_setups[i].gpio);
			continue;
		}

		/* Just setup everything */
		pnx_extint_mode(irq, irq_setups[i].mode);
		pnx_extint_pol(irq, irq_setups[i].pol);
		pnx_extint_debounce(irq, irq_setups[i].debounce);
		pnx_extint_input(irq, irq_setups[i].input);
	}

	return 0;
}
module_init(irq_device_setup);
