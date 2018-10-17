/*
 *  drivers/cordless/dev-cordless.h - the cordless character device driver
 *
 *  This driver registers the character device for the communication between
 *  Linux user space and cordless.
 *
 *  Copyright (C) 2007 NXP Semiconductors
 *  Copyright (C) 2008, 2009 DSPG Technologies GmbH
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

#ifndef DEV_CORDLESS_H
#define DEV_CORDLESS_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <asm/string.h>
#include <asm/fiq.h>
#include <asm/arch/hardware.h>
#include "cfifo.h"

#define CORDLESS_MAJOR         254

#ifdef CONFIG_IP3912_USE_SCRAM
# include "../net/ip3912/ip3912_cfg.h"
#endif

#define CORDLESS_MAGIC		0xDADADADA

#define CORDLESS_IMG_VERSION	7

#define CORDLESS_INIT_FUNC	"int_handler"

#define CORDLESS_IMGFILE_EXT	".coma"

#define CORDLESS_TIMING_STATS	0xD00017F0

#define PNX8181_SCRAM_RESERVED	0x1000

/* total size of SCRAM - stacks (FIQ & UND) - BM04_SRAM & UnusedRAM -
   (if that is the case) descriptors of ethernet driver - reserved space */
#ifdef CONFIG_IP3912_USE_SCRAM
# define CORDLESS_SCRAM_AVAILABLE \
				(PNX8181_SCRAM0_SIZE + PNX8181_SCRAM1_SIZE - \
				 PNX8181_SCRAM_RESERVED - IP3912_SCRAM_SIZE)
#else
# define CORDLESS_SCRAM_AVAILABLE \
				(PNX8181_SCRAM0_SIZE + PNX8181_SCRAM1_SIZE - \
				 PNX8181_SCRAM_RESERVED)
#endif /* CONFIG_IP3912_USE_SCRAM */

#define CORDLESS_MAX_FIFO_SIZE	(100*1024)

#define COMA_CORDLESS_IID	39
#define COMA_LINUX_IID		40

/* Ioctl definitions */

#define CORDLESS_IOC_MAGIC	'D'

#define CORDLESS_IOCINIT	_IOW(CORDLESS_IOC_MAGIC, 0, struct cordless_init *)
#define CORDLESS_IOCDEINIT	_IO(CORDLESS_IOC_MAGIC, 1)
#define CORDLESS_IOCMENU	_IO(CORDLESS_IOC_MAGIC, 4)
#define CORDLESS_IOCTIMING	_IOW(CORDLESS_IOC_MAGIC, 5, struct timing_request *)
#define CORDLESS_IOCCONFIG	_IOR(CORDLESS_IOC_MAGIC, 7, unsigned char *)
#define CORDLESS_IOCCONFIG_REPLY _IOW(CORDLESS_IOC_MAGIC, 8,struct eeprom_write_reply *)
#define CORDLESS_IOCCONFIG_SET_STATUS _IOW(CORDLESS_IOC_MAGIC, 9, int *)

#define CORDLESS_IOC_MAXNR	9

struct cordless_img_hdr {
	unsigned long magic;
	unsigned long hdr_size;
	unsigned long img_size;
	unsigned long total_img_size;
	unsigned long dest_addr;
	unsigned long init_func_addr;
	unsigned long reserved_space;
	unsigned long img_version;
	char info[50];
	unsigned long fiq_handler_start;
	unsigned long fiq_handler_len;
};

struct cordless_init {
	unsigned long fifo_size;
	void *cordless_img;
	unsigned long cordless_img_size;
	int options;
	unsigned char *configuration;
	unsigned int config_len;
};

struct timing_request {
	int options;
	unsigned long timestamp;
	unsigned char interrupts[66 * 8];
};

struct config_write_reply
{
	int bytes_written_successfully;
	int result;
};
struct cordless_config {
	struct cfifo *fifo_c2l;
	struct cfifo *fifo_l2c;
	unsigned long firetux_board_revision;
	unsigned long end_of_memory;
	struct cordless_img_hdr img_hdr;
};

void cordless_signal_cordless(void);
int  cordless_print(unsigned char *text, unsigned int len);
void cordless_configuration_write(int pos, unsigned char *buf,
                                  unsigned int len);

#endif /* !DEV_CORDLESS_H */
