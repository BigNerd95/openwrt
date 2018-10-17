#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "si3216.h"
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <linux/gpio.h>
#include <linux/module.h>


static int __init si3216_init(void)
{
    /* Reset the SLI*C chip */
	gpio_set_value(GPIO_PORTC(23),1);
	udelay(10);
	gpio_set_value(GPIO_PORTC(23),0);
	udelay(100);
	gpio_set_value(GPIO_PORTC(23),1);
	udelay(100);

	/* IPINT clock is derived from clk48m.  PLL_PER1 or PLL_FIX can generate clk48m
	   PLL_PER1 is used*/

	/*Enable PLL_PER1 PLL */
    __raw_writel(__raw_readl(PNX8181_CGU_PER1CON) & ~CGU_PER1CLKEN, PNX8181_CGU_PER1CON);
	__raw_writel(__raw_readl(PNX8181_CGU_PER1CON) & ~CGU_PER1BY2  , PNX8181_CGU_PER1CON);
	__raw_writel(__raw_readl(PNX8181_CGU_PER1CON) | CGU_PLLPER1EN , PNX8181_CGU_PER1CON);

     while(!__raw_readl(PNX8181_CGU_PER1CON) & CGU_PLLPER1_LOCK);

	__raw_writel(__raw_readl(PNX8181_CGU_PER1CON) | CGU_PER1CLKEN , PNX8181_CGU_PER1CON);

	/*Enable IPINT clock */
	__raw_writel((__raw_readl(PNX8181_CGU_GATESC) | (1<<14)), PNX8181_CGU_GATESC);


	/*Enable IPINT clock */
	__raw_writel(0, PNX8181_IPINT_GLOBAL);

    /* Set PCM clock */
	__raw_writel( (SYS2_IP_MASTER | SYS2_IP_IOCK | SYS2_IP_IOD_PUSH_PULL_ALWAYS | 
	               SYS2_IP_FSTYP_SFSFR | SYS2_IP_PCM2048khz) ,PNX8181_IPINT_CNTL0);

   	return 0;
}
module_init(si3216_init);

static void __exit si3216_exit(void)
{
	return;
}
module_exit(si3216_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DSPG");
MODULE_DESCRIPTION("SLIC FXS SI3216 driver");
