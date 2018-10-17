/*
 * (c) 2008, Sebastian Hess, emlix GmbH
 * (c) 2008, Juergen Schoew, emlix GmbH <js@emlix.com>
 * (C) 2008 NXP Semiconductors, Nuremberg
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



#include <asm/gpio.h>
#include <asm/arch/hardware.h>
#include <linux/init.h>
#include <linux/kernel.h>

/*
 * This file is the only place where the
 * real topology of the pins is known
 *
 * every later module will just use the
 * numbers, nothing more
 */
struct gpio_output {
	unsigned gpio;
	int output; /* 1 = value */
	int value; /* value to set */
	enum firetux_revisions board;
};

/*
 * This description is taken from the internal WIKI about the GPIOs
 * Please set this up correctly for your board here and leave it out
 * of the device drivers. Put your values behind the default ones
 */
static __initdata struct gpio_output inout[] = {
	{ GPIO_PORTA(2), 0, 0, Vega_PNX8181_BaseStation_LC}, /* ETH_INTN */
	{ GPIO_PORTA(5), 0, 0, Vega_PNX8181_BaseStation_LC}, /* FXS_INTN */
	{ GPIO_PORTA(13), 0, 0, Vega_PNX8181_BaseStation_LC}, /* FXO_RING_N */
	{ GPIO_PORTA(16), 1, 1, Vega_PNX8181_BaseStation_LC}, /* CSN_SFLASH */
	{ GPIO_PORTA(30), 0, 0, Vega_PNX8181_BaseStation_LC}, /* IOM_PCM_DI */
	{ GPIO_PORTA(26), 1, 1, Vega_PNX8181_BaseStation_LC}, /* IOM_PCM_DO */
	{ GPIO_PORTA(25), 1, 1, Vega_PNX8181_BaseStation_LC}, /* IOM_PCM_FSC */
	{ GPIO_PORTA(24), 1, 1, Vega_PNX8181_BaseStation_LC}, /* IOM_PCM_CLK */
	{ GPIO_PORTC(12), 0, 0, Vega_PNX8181_BaseStation_LC}, /* NAND_BSY */
	{ GPIO_PORTC(22), 1, 0, Vega_PNX8181_BaseStation_LC}, /* RELAY_COUNT */
	{ GPIO_PORTC(23), 1, 1, Vega_PNX8181_BaseStation_LC}, /* FXS_RSTN */	
	{ GPIO_PORTC(27), 1, 1, Vega_PNX8181_BaseStation_LC}, /* Flash_WP , add by Norman */	
    { GPIO_PORTB(25), 1, 1, Vega_PNX8181_BaseStation_LC}, /* FXS_SPI_CS_N */
    { GPIO_PORTB(19), 1, 0, Vega_PNX8181_BaseStation_LC}, /* RELAY_COUNT for XC version,add by Norman */

	/* GPIOA0, reset REF1MO */
	{ GPIO_PORTA(0), 1, 1, Any_PNX8181_BaseStation}, /* #LED2_N OH*/
	{ GPIO_PORTA(2), 0, 0, Any_PNX8181_BaseStation}, /* #KEY1_N */
	/* GPIOA3 reset RF_SIG7 is LED1 or FXS1_SWITCH here */
	{ GPIO_PORTA(3), 1, 0, Any_PNX8181_BaseStation},
	{ GPIO_PORTA(3), 1, 1, Vega_PNX8181_BaseStation_V3}, /* #ETH_RESET_N on V3 */
	/* GPIO6 is used as GPIO, not PWM for us */
	{ GPIO_PORTA(6), 1, 1, Any_PNX8181_BaseStation},
	/* GPIOA7, reset GPIO, must be OH or FMP41 */
	{ GPIO_PORTA(7), 1, 1, Any_PNX8181_BaseStation},
	{ GPIO_PORTA(9), 1, 1, Any_PNX8181_BaseStation}, /* #LED1_N */
	{ GPIO_PORTA(10), 0, 0, Any_PNX8181_BaseStation}, /* #KEY2_N */
	{ GPIO_PORTA(12), 1, 1, Any_PNX8181_BaseStation}, /* ETH_RESET */
	/* GPIOA13 is used for BOOT on newer boards, as HOOK on V1 */
	{ GPIO_PORTA(13), 0, 0, Any_PNX8181_BaseStation}, /* BOOT */
	{ GPIO_PORTA(13), 1, 1, Vega_PNX8181_BaseStation_V1},
	{ GPIO_PORTA(15), 1, 1, Any_PNX8181_BaseStation}, /* FXO2_HOOK */
	/* GPIOA16 is used for FXS and FXO, so OL, because of the FXS reset */
	{ GPIO_PORTA(16), 1, 0, Any_PNX8181_BaseStation},
	{ GPIO_PORTA(17), 0, 0, Any_PNX8181_BaseStation}, /* #FXS01_RING_N */
	{ GPIO_PORTA(19), 1, 0, Any_PNX8181_BaseStation}, /* #FXS01_RST_N */
	/* GPIO20 is FXS2_INT_N on > V1 and PHY1_IRQ on EZ_MCP both Input */
	{ GPIO_PORTA(20), 0, 0, Any_PNX8181_BaseStation}, /* #FXS2_INT_N */
	/* Pin 29 is quite different on the boards:
	 * Input on < V1, output on > V2 */
	{ GPIO_PORTA(29), 0, 0, Any_PNX8181_BaseStation},
	{ GPIO_PORTA(29), 1, 1, Vega_PNX8181_BaseStation_V2},
	{ GPIO_PORTA(31), 1, 1, Any_PNX8181_BaseStation}, /* LED, FXO or unused */
	{ GPIO_PORTA(0), 0, 0, unknown_PNX8181}, /* dummy/last entry */
};

struct gpio_function {
	unsigned gpio;
	int function;
	enum firetux_revisions board;
};

/*
 * Only stuff not used as default is listed here
 * see Table 12ff for the valid modes
 */
static __initdata struct gpio_function muxer[] = {

   { GPIO_PORTA(31), 0x02, Vega_PNX8181_BaseStation_LC}, /* SDATIN1 */
   { GPIO_PORTA(30), 0x03, Vega_PNX8181_BaseStation_LC}, /* DI_IP */
   { GPIO_PORTA(29), 0x03, Vega_PNX8181_BaseStation_LC}, /* SDATIO1 */
   { GPIO_PORTA(28), 0x01, Vega_PNX8181_BaseStation_LC}, /* SDA */
   { GPIO_PORTA(27), 0x01, Vega_PNX8181_BaseStation_LC}, /* SCL */
   { GPIO_PORTA(26), 0x02, Vega_PNX8181_BaseStation_LC}, /* DO_IP*/
   { GPIO_PORTA(25), 0x02, Vega_PNX8181_BaseStation_LC}, /* FSC_IP*/
   { GPIO_PORTA(24), 0x02, Vega_PNX8181_BaseStation_LC}, /* DCK_IP*/
   { GPIO_PORTA(19), 0x03, Vega_PNX8181_BaseStation_LC}, /* SCLK1 */
   { GPIO_PORTA(18), 0x02, Vega_PNX8181_BaseStation_LC}, /* USBCN*/   
   { GPIO_PORTA(16), 0x0,  Vega_PNX8181_BaseStation_LC}, /* CSN_SFLASH*/   
   { GPIO_PORTA(13), 0x0,  Vega_PNX8181_BaseStation_LC}, /* FXO_RINGN*/   
   { GPIO_PORTA(11), 0x01, Vega_PNX8181_BaseStation_LC}, /* UART TXD2*/   
   { GPIO_PORTA(5),  0x0,  Vega_PNX8181_BaseStation_LC}, /* FXS_INTN*/   
   { GPIO_PORTA(2),  0x0,  Vega_PNX8181_BaseStation_LC}, /* ETH_INTN*/      
   { GPIO_PORTA(1),  0x01, Vega_PNX8181_BaseStation_LC}, /* UART RXD2*/   
   

   { GPIO_PORTB(30), 0x01, Vega_PNX8181_BaseStation_LC}, /* DECT LED*/
   { GPIO_PORTB(29), 0x02, Vega_PNX8181_BaseStation_LC}, /* HMP30 SD_CLK */
   { GPIO_PORTB(28), 0x02, Vega_PNX8181_BaseStation_LC}, /* HMP29 SD_CLKI*/
   { GPIO_PORTB(27), 0x02, Vega_PNX8181_BaseStation_LC}, /* HMP28 SD_A12 */
   { GPIO_PORTB(26), 0x01, Vega_PNX8181_BaseStation_LC}, /* POWER LED,modified by Norman */
   { GPIO_PORTB(25), 0x01, Vega_PNX8181_BaseStation_LC}, /* FXO LED, modified by Norman   */
   //{ GPIO_PORTB(24), 0x02, Vega_PNX8181_BaseStation_LC}, /* FMP53 SPI_CSN_LCD*/
   { GPIO_PORTB(24), 0x01, Vega_PNX8181_BaseStation_LC}, /* FMP53 SPI_CSN_LCD*/
   { GPIO_PORTB(23), 0x01, Vega_PNX8181_BaseStation_LC}, /* ROM - BOOT SEL */ 
   { GPIO_PORTB(19), 0x1, Vega_PNX8181_BaseStation_LC}, /* Relay count for XC version, add by Norman */   
   { GPIO_PORTB(18), 0x0, Vega_PNX8181_BaseStation_LC}, /* FMP40 NAND_ALE*/   
   { GPIO_PORTB(11), 0x0, Vega_PNX8181_BaseStation_LC}, /* KROW5*/      
   { GPIO_PORTB(10), 0x0, Vega_PNX8181_BaseStation_LC}, /* KROW4*/   
   { GPIO_PORTB(9), 0x0, Vega_PNX8181_BaseStation_LC}, /* KROW3*/   
   { GPIO_PORTB(8), 0x0, Vega_PNX8181_BaseStation_LC}, /* KROW2*/   
   { GPIO_PORTB(7), 0x0, Vega_PNX8181_BaseStation_LC}, /* KROW1*/   
   { GPIO_PORTB(6), 0x0, Vega_PNX8181_BaseStation_LC}, /* KROW0*/   
   { GPIO_PORTB(5), 0x0, Vega_PNX8181_BaseStation_LC}, /* KCOL5*/   
   { GPIO_PORTB(4), 0x1, Vega_PNX8181_BaseStation_LC}, /* for HW Model setting */   
   { GPIO_PORTB(3), 0x1, Vega_PNX8181_BaseStation_LC}, /* for HW Model setting */   
   { GPIO_PORTB(2), 0x1, Vega_PNX8181_BaseStation_LC}, /* for HW Model setting */   
   { GPIO_PORTB(1), 0x1, Vega_PNX8181_BaseStation_LC}, /* for HW Model setting */   
   { GPIO_PORTB(0), 0x1, Vega_PNX8181_BaseStation_LC}, /* DECT pairing button */   

   { GPIO_PORTC(30), 0x0, Vega_PNX8181_BaseStation_LC}, /* FMP50 EBI1_OE*/
   //{ GPIO_PORTC(29), 0x0, Vega_PNX8181_BaseStation_LC}, /* FMP46 NAND_CSN(EBI_CS3) */ /*Mod by Nash*/
   { GPIO_PORTC(29), 0x1, Vega_PNX8181_BaseStation_LC}, /* FMP46 NAND_CSN(EBI_CS3) */
   { GPIO_PORTC(28), 0x1, Vega_PNX8181_BaseStation_LC}, /* FMP39 ETH_RSTN*/
   { GPIO_PORTC(27), 0x1, Vega_PNX8181_BaseStation_LC}, /* FMP38 NAND_WPN/SFLASH_WPN */
   { GPIO_PORTC(26), 0x0, Vega_PNX8181_BaseStation_LC}, /* FMP37 NAND_CLE (EBI1_ADR21)*/
   { GPIO_PORTC(25), 0x1, Vega_PNX8181_BaseStation_LC}, /* FXS1 LED,modified by Norman */
   { GPIO_PORTC(24), 0x1, Vega_PNX8181_BaseStation_LC}, /* FXS2 LED,modified by Norman */
   { GPIO_PORTC(23), 0x1, Vega_PNX8181_BaseStation_LC}, /* Select FXS_RSTN */ 
   { GPIO_PORTC(22), 0x1, Vega_PNX8181_BaseStation_LC}, /* Relay count, modified by Nash */   
   { GPIO_PORTC(18), 0x0, Vega_PNX8181_BaseStation_LC}, /* HMP27 SD_A11*/      
   { GPIO_PORTC(17), 0x0, Vega_PNX8181_BaseStation_LC}, /* HMP26 SD_A10*/   
   { GPIO_PORTC(16), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETNMDC (PHY)*/   
   { GPIO_PORTC(15), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETH MDIO,modified by Norman */   
   { GPIO_PORTC(14), 0x1, Vega_PNX8181_BaseStation_LC}, /* reset button,modified by Norman */   
   { GPIO_PORTC(12), 0x1, Vega_PNX8181_BaseStation_LC}, /* GPIO NAND_BSY/sFlash_RSTN*/   
   { GPIO_PORTC(6), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETN1TXEN*/   
   { GPIO_PORTC(5), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETN1RXER*/   
   { GPIO_PORTC(4), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETN1CRSDV1*/   
   { GPIO_PORTC(3), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETN1RXD1*/   
   { GPIO_PORTC(2), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETN1RXD0*/   
   { GPIO_PORTC(1), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETN1TXD1*/   
   { GPIO_PORTC(0), 0x0, Vega_PNX8181_BaseStation_LC}, /* ETN ETN1TXD0*/   


	{ GPIO_PORTA(0), 0x00, Any_PNX8181_BaseStation}, /* LED2_N */
	{ GPIO_PORTA(1), 0x01, Any_PNX8181_BaseStation}, /* UART2 */
	/* Port 3 differs for the EZ_MCP */
	{ GPIO_PORTA(3), 0x00, Any_PNX8181_BaseStation}, /* FXS1_SWITCH */
	{ GPIO_PORTA(3), 0x01, EZ_MCP_PNX8181}, /* RF_SIG7 */
	{ GPIO_PORTA(4), 0x01, Any_PNX8181_BaseStation}, /* SIF_CLK */
	{ GPIO_PORTA(6), 0x01, EZ_MCP_PNX8181}, /* PWM1 on EZ_MCP */
	/* Pin 7 differs on the Vega Boards */
	{ GPIO_PORTA(7), 0x01, Any_PNX8181_BaseStation}, /* FMP41 */
	{ GPIO_PORTA(7), 0x00, Vega_PNX8181_BaseStation_V1}, /* DFL_CS_N */
	{ GPIO_PORTA(9), 0x00, Any_PNX8181_BaseStation}, /* LED1_N */
	{ GPIO_PORTA(18), 0x02, Any_PNX8181_BaseStation}, /* USB_CN */
	{ GPIO_PORTA(21), 0x01, Any_PNX8181_BaseStation}, /* MISO */
	{ GPIO_PORTA(22), 0x01, Any_PNX8181_BaseStation}, /* SCLK */
	{ GPIO_PORTA(23), 0x01, Any_PNX8181_BaseStation}, /* MOSI */
	{ GPIO_PORTA(27), 0x01, Any_PNX8181_BaseStation}, /* SCL */
	{ GPIO_PORTA(28), 0x01, Any_PNX8181_BaseStation}, /* SDA */
	{ GPIO_PORTC(14), 0x00, Any_PNX8181_BaseStation}, /* USBVBUS */
	{ GPIO_PORTA(0), 0x00, unknown_PNX8181}, /* dummy/last entry */
};

struct gpio_pad {
	unsigned gpio;
	int type;
	enum firetux_revisions board;
};

/* This array holds the padding for the different GPIOs */
static __initdata struct gpio_pad syspad[] = {
	{ GPIO_PORTA(2), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* ETH_INTN */
	{ GPIO_PORTA(5), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* FXS_INTN */
	{ GPIO_PORTA(13), GPIO_PAD_PLAININ, Vega_PNX8181_BaseStation_LC}, /* FXO_RING_N */
	{ GPIO_PORTA(16), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* CSN_SFLASH */
	{ GPIO_PORTB(25), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* SPI_FXS_CS */		
	{ GPIO_PORTB(26), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* FXO_HOOK_N */
	{ GPIO_PORTB(30), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* FXO_CID_N */
	{ GPIO_PORTC(12), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* NAND_BSY */
	{ GPIO_PORTC(23), GPIO_PAD_REPEATER, Vega_PNX8181_BaseStation_LC}, /* FXS_RSTN */
	{ GPIO_PORTA(19), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* SCLK1 */		
	{ GPIO_PORTA(29), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* SDATIO1 */		
	{ GPIO_PORTA(31), GPIO_PAD_PULLUP, Vega_PNX8181_BaseStation_LC}, /* SDATIN1 */				

 
	{ GPIO_PORTA(2), GPIO_PAD_PULLUP, Any_PNX8181_BaseStation }, /* Keys */
	{ GPIO_PORTA(10), GPIO_PAD_PULLUP, Any_PNX8181_BaseStation }, /* Keys */
	{ GPIO_PORTA(0), GPIO_PAD_PULLUP, unknown_PNX8181}, /* dummy/last entry */
};

static __init int gpio_setup(void)
{
	int i;
	int revision = firetux_get_boardrevision();
	unsigned activegpio;
	int best;

	best = -1;
	activegpio = 0;

	/* Mux the correct outputs */
	for (i = 0; i < ARRAY_SIZE(muxer); i++) {
		/* Check if the one before was the last setting for the GPIO */
		if (muxer[i].gpio != activegpio && best != -1) {
			gpio_set_specialfunction(muxer[best].gpio,
						muxer[best].function);
			best = -1;
		}

		activegpio = muxer[i].gpio;
		/* Only apply the correct values */
		if (muxer[i].board != revision &&
			muxer[i].board != Any_PNX8181_BaseStation)
			continue;

        /* Skip the  Any_PNX8181_BaseStation setting in case of LC */
		if (revision == Vega_PNX8181_BaseStation_LC &&
			muxer[i].board == Any_PNX8181_BaseStation)
			continue;

		best = i;
	}

	best = -1;
	activegpio = 0;

	/* Setup the input and outputs */
	for (i = 0; i < ARRAY_SIZE(inout); i++) {
		/* Check if the one before was the last setting for the GPIO */
		if (inout[i].gpio != activegpio && best != -1) {
			if (inout[best].output)
				gpio_direction_output(inout[best].gpio,
							inout[best].value);
			else
				gpio_direction_input(inout[best].gpio);

			best = -1;
		}

		activegpio = inout[i].gpio;

		/* Only apply the correct values */
		if (inout[i].board != revision &&
			inout[i].board != Any_PNX8181_BaseStation)
			continue;
        /* Skip the  Any_PNX8181_BaseStation setting in case of LC */
		if (revision == Vega_PNX8181_BaseStation_LC &&
			inout[i].board == Any_PNX8181_BaseStation)
			continue;
        

		best = i;
	}

	best = -1;
	activegpio = 0;

	/* Setup the padding */
	for (i = 0; i < ARRAY_SIZE(syspad); i++) {
		/* Check if the one before was the last setting for the GPIO */
		if (syspad[i].gpio != activegpio && best != -1) {
			gpio_set_padding(syspad[best].gpio, syspad[best].type);
			best = -1;
		}

		activegpio = syspad[i].gpio;

		/* Only apply the correct values */
		if (syspad[i].board != revision &&
			syspad[i].board != Any_PNX8181_BaseStation)
			continue;
        /* Skip the  Any_PNX8181_BaseStation setting in case of LC */
		if (revision == Vega_PNX8181_BaseStation_LC &&
			syspad[i].board == Any_PNX8181_BaseStation)
			continue;

		best = i;
	}

	return 0;
}
module_init(gpio_setup);
