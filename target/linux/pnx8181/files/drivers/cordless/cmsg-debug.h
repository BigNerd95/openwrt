/*
 *  drivers/cordless/cmsg-debug.h - cordless messages / debug support
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

#ifndef CMSG_DEBUG_H
#define CMSG_DEBUG_H

#include "cmsg.h"

enum cmsg_debug_types {
	CMSG_DEBUG_PRINT = 0,
	CMSG_DEBUG_NUM_TYPES,
};

#endif /* CMSG_DEBUG_H */
