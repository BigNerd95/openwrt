/*
 *  linux/arch/arm/mach-pnx8181/i2c.c - I2C initialization for PNX8181
 *
 *  Copyright (C) 2010 DSPG Technologies GmbH
 *
 *  based on arch/arm/mach-pnx4008/i2c.c by Vitaly Wool <vitalywool@gmail.com>
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

#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/i2c-pnx.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/i2c/ksz8873.h>

#include <asm/hardware.h>
#include <asm/arch/platform.h>
#include <asm/arch/i2c.h>


static int set_clock_run(struct platform_device *pdev)
{
	struct clk *clk;
	int retval = 0;

	clk = clk_get(&pdev->dev, "iic");
	if (!IS_ERR(clk)) {
		clk_enable(clk);
		//clk_put(clk);
	} else
		retval = -ENOENT;

	return retval;
}

static int set_clock_stop(struct platform_device *pdev)
{
	struct clk *clk;
	int retval = 0;

	clk = clk_get(&pdev->dev, "iic");
	if (!IS_ERR(clk)) {
		clk_disable(clk);
//		clk_put(clk);
	} else
		retval = -ENOENT;

	return retval;
}

static int i2c_pnx_suspend(struct platform_device *pdev, pm_message_t state)
{
	int retval = 0;
#ifdef CONFIG_PM
	retval = set_clock_run(pdev);
#endif
	return retval;
}

static int i2c_pnx_resume(struct platform_device *pdev)
{
	int retval = 0;
#ifdef CONFIG_PM
	retval = set_clock_run(pdev);
#endif
	return retval;
}

static u32 calculate_input_freq(struct platform_device *pdev)
{
	return HCLK_MHZ;
}


static struct i2c_pnx_algo_data pnx_algo_data0 = {
	.base = PNX8181_IIC,
	.irq = PNX8181_ID_I2C,
};

static struct i2c_adapter pnx_adapter0 = {
	.name = I2C_CHIP_NAME "0",
	.nr = 0,
	.algo_data = &pnx_algo_data0,
};

static struct i2c_pnx_data i2c0_data = {
	.suspend = i2c_pnx_suspend,
	.resume = i2c_pnx_resume,
	.calculate_input_freq = calculate_input_freq,
	.set_clock_run = set_clock_run,
	.set_clock_stop = set_clock_stop,
	.adapter = &pnx_adapter0,
};

static struct platform_device i2c0_device = {
	.name = "pnx-i2c",
	.id = 0,
	.dev = {
		.platform_data = &i2c0_data,
	},
};

static struct platform_device *devices[] __initdata = {
	&i2c0_device,
};


static struct ksz8873_platform_data ksz8873_data = {
	.reset_gpio = GPIO_PORTC(28),
	.pm_mode = 1, /* Energy detection mode */
};

static struct i2c_board_info ksz8873_i2c_board_info [] = {
	{
		I2C_BOARD_INFO("ksz8873", 0x5f),
		.platform_data = &ksz8873_data,
	}
};


static int __init pnx8181_register_i2c_devices(void)
{
	if (firetux_get_boardfeatures() & FEATURE_LC_KSZ8873) {
		i2c_register_board_info(0, ksz8873_i2c_board_info,
					ARRAY_SIZE(ksz8873_i2c_board_info));
	}

	return platform_add_devices(devices, ARRAY_SIZE(devices));
}
module_init(pnx8181_register_i2c_devices);

