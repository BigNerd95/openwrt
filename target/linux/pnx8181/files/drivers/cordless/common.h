/*
 *  drivers/cordless/common.h - common definitions for both kernel & cordless
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

#ifndef COMMON_H
#define COMMON_H

#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bug.h>

#else /* __KERNEL__ */

#define EXPORT_SYMBOL(sym)

#ifdef DEBUG_FIRETUX
# define BUG_ON(expr)       do { if (expr) while(1); } while (0)
#else
# define BUG_ON(expr)       do { } while (0)
#endif

#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* COMMON_H */
