/*
 * (c) 2008, Sebastian Hess, emlix GmbH
 * (C) 2008 NXP Semiconductors, Nuremberg
 *
 * Credits to  Joerg Rensing (2008, Siemens AG) for spotting some bugs
 *
 * GPIO extension for the firetux hardware platform.
 * This file is meant to replace the KID subsystem
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


#include <asm/io.h>
#include <asm/gpio.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>

static inline int _gpio_to_nr(unsigned gpio, struct gpio_usage *bank)
{
	return gpio & ~((u32)bank->pins);
}

static inline int _gpionr_in_use(int gpionr, struct gpio_usage *bank)
{
	return (bank->usage >> gpionr) & 0x01;
}

static inline int _gpionr_is_output(int gpionr, struct gpio_usage *bank)
{
	return (*bank->direction >> gpionr) & 0x01;
}

static struct gpio_usage *_find_regbank(unsigned gpio);
/*
 * Here is how we identify the different GPIO registers:
 * GPIOA Pin 0 is: 0xC2104000
 * GPIOB Pin 5 is: 0xC2104205
 *
 * This way we can use AND operations to test which register bank matches
 */

/*
 * Here we store the pointers of the different GPIO control registers
 *
 * Each GPIO bank has 32 pins, from 0..31
 */
static struct gpio_usage registers[] = {
	{
		.pins = (u32 *)0xC2104000,
		.output = (u32 *)0xC2104004,
		.direction = (u32 *)0xC2104008,
		.mux_low = (u32 *) 0xC220400C,
		.mux_high = (u32 *) 0xC2204010,
		.pad_low = (u32 *) 0xC2204034,
		.pad_high = (u32 *) 0xC2204038,
	},
	{
		.pins = (u32 *)0xC2104200,
		.output = (u32 *)0xC2104204,
		.direction = (u32 *)0xC2104208,
		.mux_low = (u32 *) 0xC2204014,
		.mux_high = (u32 *) 0xC2204018,
		.pad_low = (u32 *) 0xC220403C,
		.pad_high = (u32 *) 0xC2204040,
	},
	{
		.pins = (u32 *)0xC2104400,
		.output = (u32 *)0xC2104404,
		.direction = (u32 *)0xC2104408,
		.mux_low = (u32 *) 0xC220401C,
		.mux_high = (u32 *) 0xC2204020,
		.pad_low = (u32 *) 0xC2204044,
		.pad_high = (u32 *) 0xC2204048,
	},
};

/*
 * The extint controller of the PNX8181 uses three lines to
 * talk to the system controller. We will register these IRQs
 * and use them to call the correct linux irq handler routine
 */

/* mapping from extint irq to gpio */
static unsigned long irq_gpio[] = {
	GPIO_PORTA(0),
	GPIO_PORTA(1),
	GPIO_PORTA(2),
	GPIO_PORTA(3),
	GPIO_PORTA(4),
	GPIO_PORTA(5),
	GPIO_PORTA(12),
	GPIO_PORTA(13),
	GPIO_PORTA(14),
	GPIO_PORTA(15),
	GPIO_PORTA(16),
	GPIO_PORTA(17),
	GPIO_PORTA(18),
	GPIO_PORTA(19),
	GPIO_PORTA(20),
	GPIO_PORTA(21),
	GPIO_PORTC(0),
	GPIO_PORTC(1),
	GPIO_PORTC(2),
	GPIO_PORTC(3),
	GPIO_PORTC(20),
	GPIO_PORTA(25),
	GPIO_PORTC(4),
	GPIO_PORTC(25),
};
#undef GPIO_PORTA

/* Get the GPIO to IRQ mapping */
int gpio_to_irq(unsigned gpio)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irq_gpio); i++) {
		if (irq_gpio[i] == gpio)
			return i + CONFIG_ARCH_PNX8181_EXTINT_BASE;
	}

	return -ENODEV;
}

/* Find the GPIO for an IRQ */
int irq_to_gpio(unsigned irq)
{
	irq -= CONFIG_ARCH_PNX8181_EXTINT_BASE;

	if (irq >= ARRAY_SIZE(irq_gpio))
		return -ENODEV;

	return irq_gpio[irq];
}

/*
 * Try to request a GPIO from the subsystem
 */
int gpio_request(unsigned gpio, char *label)
{
	/* Find the correct register file */
	struct gpio_usage *bank = _find_regbank(gpio);
	int gpionr;

	if (!bank)
		return -ENODEV;

	/* Strip away the bank register location */
	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	if (_gpionr_in_use(gpionr, bank)) {
		printk(KERN_INFO "%s: The requested GPIO %d is in use\n",
				__FUNCTION__, gpio);
		return -EBUSY;
	}

	/* Mark the GPIO as used and set the label */
	bank->usage |= (1<<gpionr);
	bank->labels[gpionr] = label;

	return 0;
}
EXPORT_SYMBOL(gpio_request);

/*
 * Free a GPIO
 */
void gpio_free(unsigned gpio)
{
	/* Find the correct register file */
	struct gpio_usage *bank = _find_regbank(gpio);
	int gpionr;

	if (!bank)
		return;

	/* Strip away the bank register location */
	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);


	/* Mark the GPIO as used and set the label */
	bank->usage &= (1<<gpionr);
	bank->labels[gpionr] = NULL;

	/* Reset the pin to INPUT, if not a bootloader pin */
	gpio_direction_input(gpio);
}
EXPORT_SYMBOL(gpio_free);


/*
 * Set a GPIO as a Input value
 *
 * TODO: Check for special function
 */
int gpio_direction_input(unsigned gpio)
{
	struct gpio_usage *bank = _find_regbank(gpio);
	int gpionr;
	unsigned long flags;

	if (!bank)
		return -ENODEV;

	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	local_save_flags(flags);
	local_irq_disable();
	local_fiq_disable();

	/* Reset the corresponding bit */
	*bank->direction &= ~(1<<gpionr);

	local_irq_restore(flags);

	return 0;
}
EXPORT_SYMBOL(gpio_direction_input);

/*
 * Set a GPIO as a output
 *
 * also sets the first output value
 *
 * TODO: Check for special function
 */
int gpio_direction_output(unsigned gpio, int value)
{
	struct gpio_usage *bank = _find_regbank(gpio);
	int gpionr;
	unsigned long flags;

	if (!bank)
		return -ENODEV;

	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	local_save_flags(flags);
	local_irq_disable();
	local_fiq_disable();

	*bank->direction |= (1<<gpionr);

	local_irq_restore(flags);

	gpio_set_value(gpio, value);
	return 0;
}
EXPORT_SYMBOL(gpio_direction_output);

/*
 * Read the value of a pin, this can always be used on the PNX8181
 * no matter if it's input or output. Even special function should work
 */
int gpio_get_value(unsigned gpio)
{
	struct gpio_usage *bank = _find_regbank(gpio);
	int gpionr;

	if (!bank)
		return -ENODEV;

	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	return ((*bank->pins >> gpionr) & 0x1);
}
EXPORT_SYMBOL(gpio_get_value);

/*
 * Set the value of a GPIO pin
 */
void gpio_set_value(unsigned gpio, int value)
{
	struct gpio_usage *bank = _find_regbank(gpio);
	int gpionr;
	unsigned long flags;

	if (!bank)
		return;

	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	/* Only set if this is an output */
	if (!_gpionr_is_output(gpionr, bank))
		return;

	local_save_flags(flags);
	local_irq_disable();
	local_fiq_disable();

	/* Set the value */
	if (value)
		*bank->output |= (1<<gpionr);
	else
		*bank->output &= ~(1<<gpionr);

	local_irq_restore(flags);
}
EXPORT_SYMBOL(gpio_set_value);

/* Set the special function of a GPIO */
int gpio_set_specialfunction(unsigned gpio, int function)
{
	struct gpio_usage *bank = _find_regbank(gpio);
	u32 *reg;
	u32 data;

	int gpionr;

	if (!bank)
		return -ENODEV;

	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	/* Now set the special function in the register
	 * This is split to a 2x32 bit for the 32 GPIOs,
	 * because each one can be 2 bit long
	 */
	if (gpionr < 16) {
		reg = bank->mux_low;
	} else {
		reg = bank->mux_high;
		gpionr -= 16;
	}

	data = *reg;
	gpionr *= 2; /* We use two bits per GPIO */

	/*
	 * Set the correct value, this is more than one
	 * read / modify / write so buffer the data
	 */
	data &= ~(0x3<<gpionr);
	data |= (function & 0x3) << gpionr;

	*reg = data;
	return 0;
}
EXPORT_SYMBOL(gpio_set_specialfunction);

/* Get the special function for the GPIO */
int gpio_get_specialfunction(unsigned gpio)
{
	struct gpio_usage *bank = _find_regbank(gpio);
	u32 *reg;
	int gpionr;

	if (!bank)
		return -ENODEV;

	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	/* Now set the special function in the register
	 * This is split to a 2x32 bit for the 32 GPIOs,
	 * because each one can be 2 bit long
	 */
	if (gpionr < 16) {
		reg = bank->mux_low;
	} else {
		reg = bank->mux_high;
		gpionr -= 16;
	}

	return (*reg >> (gpionr*2)) & 0x3;
}
EXPORT_SYMBOL(gpio_get_specialfunction);

/*
 * Set the pullups and pulldowns in the syspad register
 * Beware: not every Port has one
 */
int gpio_set_padding(unsigned gpio, int type)
{
	struct gpio_usage *bank = _find_regbank(gpio);
	u32 *reg;
	int gpionr;
	u32 value;

	if (!bank)
		return -ENODEV;

	gpionr = _gpio_to_nr(gpio, bank);
	BUG_ON(gpionr < 0 || gpionr > 31);

	/* Now set the special function in the register
	 * This is split to a 2x32 bit for the 32 GPIOs,
	 * because each one can be 2 bit long
	 */
	if (gpionr < 16) {
		reg = bank->pad_low;
	} else {
		reg = bank->pad_high;
		gpionr -= 16;
	}

	value = __raw_readl(reg);
	gpionr *= 2; /* We use two bits per GPIO */

	/*
	 * Set the correct value, this is more than one
	 * read / modify / write so buffer the data
	 */
	value &= ~(0x3<<gpionr);
	value |= (type & 0x3) << gpionr;

	__raw_writel(value, reg);
	return 0;
}
EXPORT_SYMBOL(gpio_set_padding);

/* GPIO dumping function, this will print out the GPIO registers
 * and the muxer data
 */
void gpio_dump()
{
	int i;

	for (i = 0; i < ARRAY_SIZE(registers); i++) {
		printk(KERN_INFO "BANK %d\n", i);
		printk(KERN_INFO "pins: %X\n", *registers[i].pins);
		printk(KERN_INFO "output: %X\n", *registers[i].output);
		printk(KERN_INFO "direction: %X\n", *registers[i].direction);
		printk(KERN_INFO "sysmux low: %X\n", *registers[i].mux_low);
		printk(KERN_INFO "sysmux high: %X\n", *registers[i].mux_high);
	}

}
EXPORT_SYMBOL(gpio_dump);

/* Helper function to find the correct GPIO Bank location */
static struct gpio_usage *_find_regbank(unsigned gpio)
{
	int i;
	unsigned addr;
	struct gpio_usage *bank = NULL;
	unsigned search = gpio & ~0xFF;

	for (i = 0; i < ARRAY_SIZE(registers); i++) {
		addr = (u32)registers[i].pins;
		if (addr == search) {
			bank = &registers[i];
			break;
		}
	}

	BUG_ON(!bank);
	return bank;
}

/* add by Norman */
int gpio_get_hw_model()
{
    int gpiob1;
    int gpiob2;
    int gpiob3;
    int gpiob4;
    int model;
    
    gpio_direction_input(GPIO_PORTB(1));
    gpio_direction_input(GPIO_PORTB(2));
    gpio_direction_input(GPIO_PORTB(3));
    gpio_direction_input(GPIO_PORTB(4));
    
    gpiob1 = gpio_get_value(GPIO_PORTB(1));
    gpiob2 = gpio_get_value(GPIO_PORTB(2));
    gpiob3 = gpio_get_value(GPIO_PORTB(3));
    gpiob4 = gpio_get_value(GPIO_PORTB(4));
    
    model = (gpiob4<<3)|(gpiob3<<2)|(gpiob2<<1)|gpiob1;
    return(model);
}
/* add by Norman */

/* Initialization */
static int __init gpio_init(void)
{
	u32 *map;
	struct resource *mres;
	struct clk *clock;

	/*
	 * Map the space correctly as iomem
	 */
	resource_size_t start = (resource_size_t) registers[0].pins;
	resource_size_t end =
	    (resource_size_t) registers[ARRAY_SIZE(registers)-1].direction + 1;
	resource_size_t len;

	len = (end-start) ;

	mres = request_mem_region(start, len, "pnx8181-gpio");
	if (!mres) {
		printk(KERN_ERR
			"%s: Can't reserve the GPIO memory space (%X-%X)!\n",
				__FUNCTION__, start, end);

		return -ENODEV;
	}

	map = ioremap_nocache(start, len);
	if (!map) {
		printk(KERN_ERR "%s: Can't remap the GPIO memory space\n",
				__FUNCTION__);
		release_mem_region(start, len);
		return -ENOMEM;
	}

	/* Now map the sysmux registers */
	start = (resource_size_t) registers[0].mux_low;
	end = (resource_size_t) registers[ARRAY_SIZE(registers)-1].mux_high + 1;
	len = (end-start) * 4;

	mres = request_mem_region(start, len, "pnx8181-mux");
	if (!mres) {
		printk(KERN_ERR
			"%s: Can't reserve the SYSMUX memory space (%X-%X)!\n",
				__FUNCTION__, start, end);
		return -ENODEV;
	}

	map = ioremap_nocache(start, len);
	if (!map) {
		printk(KERN_ERR "%s: Can't remap the SYSMUX memory space\n",
				__FUNCTION__);
		release_mem_region(start, len);
		return -ENOMEM;
	}

	/* Activate the module clock */
	clock = clk_get(NULL, "gpio");
	if (!clock)
		printk(KERN_ERR "%s: No clock available\n", __FUNCTION__);
	else
		clk_enable(clock);

	printk(KERN_NOTICE "%s: Registered PNX818 GPIO device\n", __FUNCTION__);
	/* add by Norman */
	printk("Board HW MODEL : 0x%x\n",gpio_get_hw_model());
	return 0;
}
arch_initcall(gpio_init);
