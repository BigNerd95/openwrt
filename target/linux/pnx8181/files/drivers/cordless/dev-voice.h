/*
 *  drivers/cordless/dev-voice.h - voice character device
 *
 *  This driver registers one character device for each available voice channel.
 *  It allows only one reader and one writer at the same time.
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

#ifndef __LINUX_DRIVERS_CORDLESS_VOICE_H
#define __LINUX_DRIVERS_CORDLESS_VOICE_H

void voice_poll_fifos(void);
void voice_receive_dtmf(int id, int status, int event, int volume,
                        int duration);

#endif /* __LINUX_DRIVERS_CORDLESS_VOICE_H */
