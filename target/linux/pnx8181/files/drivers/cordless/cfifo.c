/*
 * drivers/cordless/cfifo.c - cordless FIFOs
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include "cfifo.h"

/**
 * cfifo_init - allocates a new FIFO using a preallocated buffer
 * @buffer: the preallocated buffer to be used.
 * @size: the size of the internal buffer.
 *
 * Do NOT pass the cfifo to cfifo_free() after use! Simply free the
 * &struct cordless_fifo with kfree().
 */
struct cfifo *
cfifo_init(unsigned char *buffer, unsigned int size)
{
	struct cfifo *cfifo;

	size = CFIFO_PAD(size);

	cfifo = kmalloc(sizeof(*cfifo), GFP_KERNEL);
	if (!cfifo)
		return ERR_PTR(-CFIFO_NOMEM);

	cfifo->buffer = buffer;
	cfifo->size = size;

	spin_lock_init(&cfifo->spinlock);

	cfifo_reset(cfifo);

	return cfifo;
}
EXPORT_SYMBOL(cfifo_init);

static void
cfifo_free_buffer(unsigned char *buffer, unsigned int size)
{
	if (size <= 0x10000) { /* 64 kBytes */
		kfree(buffer);
	} else {
		unsigned int buf_size = PAGE_SIZE, order = 0;

		while (buf_size < size) {
			order++;
			buf_size <<= 1;
		}
		free_pages((unsigned long)buffer, order);
	}
}

/*
 * cfifo_alloc - allocates a new cfifo and its internal buffer
 * @size: the size of the internal buffer to be allocated.
 */
struct cfifo *
cfifo_alloc(unsigned int size)
{
	unsigned char *buffer;
	struct cfifo *ret;

	size = CFIFO_PAD(size);

	if (size <= 0x10000) { /* 64 kBytes */
		buffer = kmalloc(size, GFP_KERNEL);
	} else {
		/* Do not use vmalloc for larger cfifo buffers as pages
		 * allocated this way are mapped on-demand by the page fault
		 * exception handler of Linux. The handler cannot run in the
		 * FIQ/cordless domain which will result in a bad mode
		 * exception if cordless tries to access the cfifo buffer
		 * after interrupting a Linux process that has never accessed
		 * this buffer.
		 * */
		unsigned int buf_size = PAGE_SIZE, order = 0;

		while (buf_size < size) {
			order++;
			buf_size <<= 1;
		}

		buffer = (unsigned char *)__get_free_pages(GFP_KERNEL, order);
		size = buf_size;
	}
	if (!buffer)
		return ERR_PTR(-CFIFO_NOMEM);

	ret = cfifo_init(buffer, size);

	if (IS_ERR(ret))
		cfifo_free_buffer(buffer, size);

	return ret;
}
EXPORT_SYMBOL(cfifo_alloc);

/*
 * cfifo_free - frees the cfifo
 * @cfifo: the fifo to be freed.
 */
void cfifo_free(struct cfifo *cfifo)
{
	cfifo_free_buffer(cfifo->buffer, cfifo->size);
	kfree(cfifo);
}
EXPORT_SYMBOL(cfifo_free);

