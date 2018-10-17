/*
 * drivers/cordless/cfifo.h - cordless FIFOs
 *
 * Cordless FIFOs store variable-length messages in a ring buffer. This FIFO
 * implementation is safe for parallel accesses of not more than one reader
 * and one writer.
 * cordless_fifo_msg_put() copies a message to the internal buffer of the
 * FIFO, while cordless_fifo_msg_get() retrieves the pointer to the next
 * message (if any) and its length. It does not copy the data, i.e. the caller
 * does not need to allocate memory in order to access the message. The FIFO
 * has to be informed using cordless_fifo_msg_processed() if the message is
 * processed and can be overwritten by the FIFO. A message is stored in a
 * consecutive memory area, i.e., it is not wrapped around at the end of the
 * FIFO as in many other ring buffer implementations.
 *
 * Copyright (C) 2007 NXP Semiconductors
 * Copyright (C) 2008, 2009 DSPG Technologies GmbH
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

#ifndef __CORDLESS_CFIFO_H
#define __CORDLESS_CFIFO_H

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#endif

#define CFIFO_MSGHEADER_SIZE sizeof(unsigned int)
#define CFIFO_PAD(x)         (((x) + (CFIFO_MSGHEADER_SIZE-1)) & \
                              ~(CFIFO_MSGHEADER_SIZE-1))

struct cfifo {
	unsigned char *buffer;	/* the buffer holding the data */
	unsigned int size;	/* the size of the allocated buffer */
	unsigned int in;	/* data is added at offset in */
	unsigned int out;	/* data is extracted from offset out */
	unsigned int lastin;	/* temporary offset in until message is
				   commited, i.e. can be read */
	unsigned int lastout;	/* temporary offset out until message is
				   processed, i.e. can be overwritten */
	int processed;		/* message processed */
#ifdef __KERNEL__
	spinlock_t spinlock;	/* protects concurrent modifications */
#endif
};

#ifdef __KERNEL__
#define cfifo_debug(msg)                  printk(KERN_ERR msg)

struct cfifo *cfifo_init(unsigned char *buffer, unsigned int size);
struct cfifo *cfifo_alloc(unsigned int size);
void          cfifo_free(struct cfifo *fifo);

#else /* __KERNEL__ */

#define cfifo_debug(msg)                  printf("cfifo: %s\n", (msg))

#endif /* __KERNEL__ */

void cfifo_reset(struct cfifo *fifo);

int  cfifo_request(struct cfifo *cfifo, void **buf, unsigned int len);
int  cfifo_commit(struct cfifo *cfifo, unsigned int len);
int  cfifo_get(struct cfifo *cfifo, void **buf);
void cfifo_processed(struct cfifo *cfifo);

int  cfifo_empty(struct cfifo *fifo);

#define CFIFO_NOTPROC               1
#define CFIFO_NOMEM                 2
#define CFIFO_BROKEN                3

//#define CFIFO_CHECKS 1
//#undef  CFIFO_CHECKS /* comment this out if you want checks */

#endif /* __CORDLESS_CFIFO_H */
