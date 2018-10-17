/*
 *  drivers/cordless/cmsg.c - cordless messages
 *
 *  Cordless messages are used to exchange commands and data between Linux and
 *  cordless over cfifos. They consist of a type specifier, an optional
 *  parameter block and optional additional payload.
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

#include "cmsg.h"
#include "common.h"

int
cmsg_alloc_from_cfifo(struct cmsg **cmsg, struct cfifo *cfifo,
                      unsigned int params_size, unsigned int payload_size)
{
	int ret;
	struct cmsg *p;

	ret = cfifo_request(cfifo, (void **)cmsg,
	                    sizeof(*p) + params_size + payload_size);
	if (ret <= 0)
		return -1;

	p = *cmsg;
	memset(p, 0, ret);

	/* set pointer to parameter union to the data after cmsg struct */
	if (params_size > 0) {
		p->params = p + 1;
		p->params_size = params_size;
	}

	/* set payload pointer to the data after the param union */
	if (payload_size > 0) {
		p->payload = (unsigned char *)(p + 1) + params_size;
		p->payload_size = payload_size;
	}

	return 0;
}
EXPORT_SYMBOL(cmsg_alloc_from_cfifo);

int
cmsg_commit_to_cfifo(struct cmsg *cmsg, struct cfifo *cfifo)
{
	int ret;

	ret = cfifo_commit(cfifo, sizeof(*cmsg) + cmsg->params_size +
	                   cmsg->payload_size);
	if (ret <= 0)
		return -1;

	return 0;
}
EXPORT_SYMBOL(cmsg_commit_to_cfifo);
