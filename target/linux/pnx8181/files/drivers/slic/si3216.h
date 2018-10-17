/*
 * include/linux/spi/si3216.h
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  */

#ifndef SI3216_H
#define SI3216_H

#define CGU_PER1CLKEN                   0x00002000
#define CGU_PER1BY2                     0x00400000
#define CGU_PLLPER1EN                   0x00010000
#define CGU_PLLPER1_LOCK                0x00020000
#define SYS2_IP_MASTER                  0x4000
#define SYS2_IP_LOOP                    0x0800
#define SYS2_IP_IOCK                    0x0400
#define SYS2_IP_IOD_MASK                0x0300
#define SYS2_IP_IOD_PUSH_PULL_ALWAYS    0x0300
#define SYS2_IP_IOD_PUSH_PULL           0x0200
#define SYS2_IP_IOD_OPEN_DRAIN          0x0100
#define SYS2_IP_IOD_DISABLED            0x0000
#define SYS2_IP_FSTYP_MASK              0x00C0
#define SYS2_IP_FSTYP_LFS               0x00C0
#define SYS2_IP_FSTYP_SFSLF             0x0080
#define SYS2_IP_FSTYP_SFSFF             0x0040
#define SYS2_IP_FSTYP_SFSFR             0x0000
#define SYS2_IP_FREQ_MASK               0x0038
#define SYS2_IP_IOM4096khz              0x0038
#define SYS2_IP_IOM1536khz              0x0030
#define SYS2_IP_IOM768khz               0x0028
#define SYS2_IP_IOM512khz               0x0020
#define SYS2_IP_PCM2048khz              0x0018
#define SYS2_IP_PCM1536khz              0x0010
#define SYS2_IP_PCM768khz               0x0008
#define SYS2_IP_PCM512khz               0x0000

#endif /* SI3216_H */

