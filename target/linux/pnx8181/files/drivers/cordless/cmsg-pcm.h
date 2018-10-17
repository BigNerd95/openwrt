/*
 *  drivers/cordless/cmsg-pcm.h - cordless messages / PCM playback
 *
 *  Cordless messages are used to exchange commands and data between Linux and
 *  cordless over cfifos. They consist of a type specifier, an optional
 *  parameter block and optional additional payload.
 *
 *  Copyright (C) 2009 DSPG Technologies GmbH
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

#ifndef CMSG_PCM_H
#define CMSG_PCM_H

#include "cmsg.h"

enum cmsg_pcm_types {
	CMSG_PCM_REQUEST_START_CHAN = 0,
	CMSG_PCM_REPLY_START_CHAN,
	CMSG_PCM_REQUEST_STOP_CHAN,
	CMSG_PCM_REPLY_STOP_CHAN,
	CMSG_PCM_NUM_TYPES,
};

union cmsg_pcm_params {
	/* CMSG_PCM_REQUEST_START_CHAN */
	struct  request_start_chan {
		int id;
		struct cfifo *fifo;
	} request_start_chan;

	/* CMSG_PCM_REPLY_START_CHAN */
	struct  reply_start_chan {
		int result;
	} reply_start_chan;

	/* CMSG_PCM_REQUEST_STOP_CHAN */
	struct  request_stop_chan {
		int id;
	} request_stop_chan;

	/* CMSG_PCM_REPLY_STOP_CHAN */
	struct  reply_stop_chan {
		int result;
	} reply_stop_chan;
};

#endif /* CMSG_PCM_H */
