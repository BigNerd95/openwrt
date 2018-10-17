/*
 *  drivers/cordless/debug.c - cordless manager debug support
 *
 *  This driver receives debug output from cordless and forwards it to the
 *  cordless device driver. There it is stored in a kfifo until it is retrieved
 *  by the user when reading from /dev/cordless. The corresponding cfifo is
 *  rather big in order to capture as much debug messages as possible.
 *
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

#include <linux/err.h>

#include "coma.h"
#include "cfifo.h"
#include "cmsg-debug.h"

MODULE_AUTHOR("DSP Group Inc.");
MODULE_LICENSE("GPL");

#define SERVICE_DEBUG 1

static char debug_name[] = "coma-debug";

static struct cfifo *debug_c2l;

static int debug_initialized = 0;

int cordless_print(unsigned char *text, unsigned int len);

static int
debug_process_message(struct cmsg *cmsg)
{
	int ret = 0;

	switch(cmsg->type) {
	case CMSG_DEBUG_PRINT:
		ret = cordless_print(cmsg->payload,cmsg->payload_size);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int
debug_setup(void)
{
	debug_initialized = 1;

	return 0;
}

static void
debug_remove(void)
{
	debug_initialized = 0;
}

static int __init
debug_init(void)
{
	int ret;

	debug_c2l = cfifo_alloc(100*1024);
	if (IS_ERR(debug_c2l))
		return -ENOMEM;

	ret = coma_register(SERVICE_DEBUG, NULL, debug_c2l,
	                    debug_process_message, debug_setup, debug_remove,
	                    NULL);
	if (ret < 0)
		goto err_cfifo_free;

	printk(KERN_INFO "%s: coma debug support enabled\n", debug_name);

	return 0;

err_cfifo_free:
	cfifo_free(debug_c2l);
	return ret;
}

static void __exit
debug_exit(void)
{
	coma_deregister(SERVICE_DEBUG);
	cfifo_free(debug_c2l);
}

module_init(debug_init);
module_exit(debug_exit);
