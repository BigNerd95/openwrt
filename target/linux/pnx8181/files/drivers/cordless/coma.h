/*
 *  drivers/cordless/coma.h - the cordless manager
 *
 *  The cordless manager is responsible for maintaining the main cfifo between
 *  Linux and cordless which is used for loading and unloading cordless and for
 *  registration of other services(e.g., debug output, VoIP/RTP, etc.)
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

#ifndef COMA_H
#define COMA_H

#include "cfifo.h"
#include "cmsg.h"

/* Currently statically defined. To be replaced with a dynamic allocation
 * scheme (linked list) later. */
#define NR_SERVICES 13

int cordless_initialized(void);

void coma_signal(void);

/* Create a message to be sent over a cfifo to the cordless domain.
 * Parameters and payload are optional. */
int  coma_create_message(int id, int type,
                         void *params, unsigned int params_size,
                         unsigned char *payload, unsigned int payload_size);

/* Register the services with cordless in order to receive messages from and
 * send messages to the cordless domain. */
int  coma_register(int id, struct cfifo *l2c, struct cfifo *c2l,
                   int (*process_message)(struct cmsg *cmsg),
                   int (*setup)(void), void (*remove)(void),
                   void (*poll)(void));
/* De-register the services (e.g., when unloading the corresponding kernel
 * module). */
int  coma_deregister(int id);

int  coma_init(int options, unsigned char *configuration,
               unsigned int config_len);
int  coma_deinit(void);
int  coma_timing(int options);
int  coma_menu(void);
int  coma_setup(struct cfifo *l2c, struct cfifo *c2l);
int coma_reply_configuration_write(int bytes_written, int result);
void coma_release(void);

#endif /* COMA_H */
