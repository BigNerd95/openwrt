/*
 *  drivers/cordless/cmsg-coma.h - cordless messages / cordless manager
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

#ifndef CMSG_COMA_H
#define CMSG_COMA_H

#include "cmsg.h"
#include "cfifo.h"

enum cmsg_coma_types {
	CMSG_COMA_REQUEST_INIT = 0,
	CMSG_COMA_REPLY_INIT,
	CMSG_COMA_DEINIT,
	CMSG_COMA_REQUEST_REGISTER,
	CMSG_COMA_REPLY_REGISTER,
	CMSG_COMA_REQUEST_DEREGISTER,
	CMSG_COMA_REPLY_DEREGISTER,
	CMSG_COMA_REQUEST_TIMING,
	CMSG_COMA_REPLY_TIMING,
	CMSG_COMA_WRITE_CONFIGURATION,
	CMSG_REPLY_WRITE_CONFIGURATION,
	CMSG_COMA_MENU,
	CMSG_COMA_NUM_TYPES,
};

union cmsg_coma_params {
	/* CMSG_REQUEST_INIT */
	struct  request_init {
		int options;
	} request_init;

	/* CMSG_REPLY_INIT */
	struct  reply_init {
		int result;
	} reply_init;

	/* CMSG_REQUEST_REGISTER */
	struct  request_register {
		int id;
		struct cfifo *l2c;
		struct cfifo *c2l;
	} request_register;

	/* CMSG_REPLY_REGISTER */
	struct  reply_register {
		int id;
		int result;
	} reply_register;

	/* CMSG_REQUEST_DEREGISTER */
	struct  request_deregister {
		int id;
	} request_deregister;

	/* CMSG_REPLY_DEREGISTER */
	struct  reply_deregister {
		int id;
		int result;
	} reply_deregister;

	/* CMSG_REQUEST_TIMING */
	struct request_timing {
		int options;
	} request_timing;

	/* CMSG_REPLY_TIMING */
	struct reply_timing {
		unsigned long timestamp;
	} reply_timing;

	/* CMSG_WRITE_CONFIGURATION */
	struct write_configuration {
		int pos;
	} write_configuration;
	/*CMSG_REPLY_WRITE_CONFIGURATION*/
	struct reply_write_configuration{
		int bytes_written;
		int result;
	}reply_write_configuration;
};

#endif /* CMSG_COMA_H */
