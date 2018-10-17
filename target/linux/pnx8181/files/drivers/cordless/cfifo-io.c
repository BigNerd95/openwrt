/*
 * drivers/cordless/cfifo-io.c - cordless FIFOs
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

#include "cfifo.h"
#include "common.h"

/*
 * cfifo_request - request a message buffer from the cfifo
 * @cfifo: the cfifo to be used.
 * @buf: pointer to the message buffer
 * @len: the length of the message to be added.
 *
 * After this call, the message buffer can be filled by the
 * application. A call to cfifo_commit() makes the
 * message visible to readers.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these functions.
 */
int
cfifo_request(struct cfifo *cfifo, void **buf, unsigned int len)
{
	unsigned int in, out;
	/* padding to distinguish empty from full state and to reserve space
	   for wraparound marker (msg_len == 0) at the end of the cfifo */
	unsigned int off = len + 2 * CFIFO_MSGHEADER_SIZE;

	BUG_ON(cfifo == NULL);

	/* cfifo->out has be sampled once to avoid race condition */
	out = cfifo->out;

	if (((cfifo->in >= out) && (off <= (cfifo->size - cfifo->in))) ||
		/* free space at end of cfifo: reserve additional space for wraparound marker:
		 *    XXXXXXXX________0000
		 *    ^       ^       ^
		 *    out     in      wraparound marker
		 */
	    ((cfifo->in <  out) && (off <= (out - cfifo->in)))) {
		/* free space at beginning/middle/end of cfifo
		 *    XXXXXXXX________XXXXXXXX0000
		 *            ^       ^
		 *            in      out
		 */
		in = cfifo->in;
	}
#if 0
 else if ((cfifo->in == out) && (off <= cfifo->size)) {
		/* fifo is empty */
		out = cfifo->out = in = cfifo->in = 0;
	}
#endif
	 else if ((cfifo->in >= out) && (off <= out)) {
		/* free space at beginning of cfifo
		 *    ________XXXXXXXX____
		 *            ^       ^
		 *            out     in
		 */
		in = 0;

		/* signal cfifo_get() that next message starts at
		   beginning of cfifo */
		memset(cfifo->buffer + cfifo->in, 0, CFIFO_MSGHEADER_SIZE);
	} else {
		/* no space available */
		return 0;
	}

#ifdef CFIFO_CHECKS
	if (in >= (cfifo->size - CFIFO_MSGHEADER_SIZE)) {
		cfifo_debug("cfifo_request: cfifo broken (1)\n");
		return -CFIFO_BROKEN;
	}
	if ((in >= cfifo->size) || (in+len >= cfifo->size)) {
		cfifo_debug("cfifo_request: cfifo broken (2)\n");
		return -CFIFO_BROKEN;
	}
	/* only one that is really necessary: in+len >= cfifo->size */
#endif

	cfifo->lastin = in;
	*buf = cfifo->buffer + in + CFIFO_MSGHEADER_SIZE;

	return len;
}
EXPORT_SYMBOL(cfifo_request);

/*
 * cfifo_commit - commit the previously requested message to the cfifo
 * @cfifo: the cfifo to be used.
 * @len: the length of the message to be added.
 *
 * The message has already been copied to the cfifo after a
 * call to cfifo_request().
 *
 * On success, the length of the message is returned. If the cfifo is
 * currently full, the function returns 0.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these functions.
 */
int cfifo_commit(struct cfifo *cfifo, unsigned int len)
{
	BUG_ON(cfifo == NULL);

#ifdef CFIFO_CHECKS
	if (cfifo->lastin >= (cfifo->size - CFIFO_MSGHEADER_SIZE)) { /* - len */
		cfifo_debug("cfifo_commit: cfifo broken (1)\n");
		return -CFIFO_BROKEN;
	}
#endif

	/* first, store the length of the message */
	memcpy(cfifo->buffer + cfifo->lastin, (unsigned char *)&len, CFIFO_MSGHEADER_SIZE);
	cfifo->lastin += CFIFO_MSGHEADER_SIZE;

#ifdef CFIFO_CHECKS
	if ((cfifo->lastin >= cfifo->size) || (cfifo->lastin+len >= cfifo->size)) {
		cfifo_debug("cfifo_commit: cfifo broken (2)\n");
		return -CFIFO_BROKEN;
	}
#endif

	/* next, store the message */
	cfifo->lastin += len;

	/* finally, update the cfifo->in index (this has to be done after
	   adding the message) with a padded address */
	cfifo->in = CFIFO_PAD(cfifo->lastin);

	return len;
}
EXPORT_SYMBOL(cfifo_commit);

/**
 * cfifo_get - get one message from the cfifo
 * @cfifo: the cfifo to be used.
 * @buf: the pointer to the message.
 *
 * This function returns the address (in @buf) and length
 * of the next message in the cfifo (if any).
 *
 * On success, the size of the message is returned. If the cfifo is
 * currently empty, the function returns 0.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these functions.
 */
int __cfifo_get(struct cfifo *cfifo, void **buf)
{
	unsigned int in, out;
	unsigned int msg_len;

	BUG_ON(cfifo == NULL);

	/* cfifo->in has be sampled once to avoid race condition */
	in = cfifo->in;
	out = cfifo->out;

	if (in == out)
		return 0;

	if (!cfifo->processed)
		return -CFIFO_NOTPROC;

#ifdef CFIFO_CHECKS
	/* this should not happen due to padding! */
	if ((cfifo->size - out) < CFIFO_MSGHEADER_SIZE) {
		cfifo_debug("cfifo_get: cfifo broken (1)\n");
		return -CFIFO_BROKEN;
	}
#endif /* CFIFO_CHECKS */

	msg_len = *(unsigned int *)(cfifo->buffer + out);
	/* zero length means cfifo->out is really at the beginning of the
	   cfifo */
	if (msg_len == 0) {
#ifdef CFIFO_CHECKS
		if (out == 0) {
			cfifo_debug("cfifo_get: cfifo broken (2)\n");
			return -CFIFO_BROKEN;
		}
#endif

		out = 0;

#ifdef CFIFO_CHECKS
		/* this should not happen -> wrap around, but no message */
		if (in == 0) {
			cfifo_debug("cfifo_get: cfifo broken (3)\n");
			return -CFIFO_BROKEN;
		}
#endif

		msg_len = *(unsigned int *)(cfifo->buffer + out);

#ifdef CFIFO_CHECKS
		/* this should not happen -> invalid message length */
		if ((msg_len == 0) ||
		    (msg_len > cfifo->size - CFIFO_MSGHEADER_SIZE)) {
			cfifo_debug("cfifo_get: cfifo broken (4)\n");
			return -CFIFO_BROKEN;
		}
#endif
	}

	out += CFIFO_MSGHEADER_SIZE;

#ifdef CFIFO_CHECKS
	/* not enough space for message */
	if ((in - out) < msg_len) {
		cfifo_debug("cfifo_get: cfifo broken (5)\n");
		return -CFIFO_BROKEN;
	}
#endif

	*buf = cfifo->buffer + out;
	out += msg_len;

	/* compute new out pointer, but do not yet increase it (this is done
	   by cfifo_processed()) */
	cfifo->lastout = CFIFO_PAD(out);
	cfifo->processed = 0;

	return msg_len;
}

int cfifo_get(struct cfifo *cfifo, void **buf)
{
	unsigned long flags;
	unsigned int ret;

	BUG_ON(cfifo == NULL);

	spin_lock_irqsave(&cfifo->spinlock, flags);

	ret = __cfifo_get(cfifo, buf);

	spin_unlock_irqrestore(&cfifo->spinlock, flags);

	return ret;
}
EXPORT_SYMBOL(cfifo_get);

void __cfifo_processed(struct cfifo *cfifo)
{
	BUG_ON(cfifo == NULL);

	if (cfifo->processed)
		return;

	/* now it is save to update cfifo->out */
	cfifo->out = cfifo->lastout;
	cfifo->processed = 1;
}

void cfifo_processed(struct cfifo *cfifo)
{
	unsigned long flags;

	BUG_ON(cfifo == NULL);

	spin_lock_irqsave(&cfifo->spinlock, flags);

	__cfifo_processed(cfifo);

	spin_unlock_irqrestore(&cfifo->spinlock, flags);
}
EXPORT_SYMBOL(cfifo_processed);

/**
 * cfifo_reset - removes the entire cfifo contents
 * @cfifo: the cfifo to be emptied.
 */
void cfifo_reset(struct cfifo *cfifo)
{
	BUG_ON(cfifo == NULL);

	cfifo->in = cfifo->out = cfifo->lastout = cfifo->lastin = 0;
	cfifo->processed = 1;
	memset(cfifo->buffer, 0, cfifo->size);
}
EXPORT_SYMBOL(cfifo_reset);

/**
 * cfifo_empty - checks if cfifo is empty
 * @cfifo: the fifo to check
 *
 * returns 0 if not empty, any other value if empty
 */
int cfifo_empty(struct cfifo *cfifo)
{
	BUG_ON(cfifo == NULL);

	return (cfifo->in == cfifo->out);
}
EXPORT_SYMBOL(cfifo_empty);

