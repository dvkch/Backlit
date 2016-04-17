/** @file plustek-pp_dbg.c
 *  @brief definition of some debug macros
 *
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 * <hr>
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

/* uncomment this to have an SW-simulatet 98001 device - don't expect to scan*/
/* #define _ASIC_98001_SIM */

/*
 * the print macros
 */
#ifdef __KERNEL__
# define _PRINT	printk
#endif

/*
 * some debug definitions
 */
#ifdef DEBUG
# ifndef __KERNEL__
#  include <assert.h>
#  define _ASSERT(x) assert(x)
# else
#  define _ASSERT(x)
# endif

# ifndef DBG
#  define DBG(level, msg, args...)		if ((dbg_level) & (level)) {	\
											_PRINT(msg, ##args);		\
										}
# endif
#else
# define _ASSERT(x)
# ifndef DBG
#  define DBG(level, msg, args...)
# endif
#endif

/* different debug level */
#define DBG_LOW         0x01
#define DBG_MEDIUM      0x02
#define DBG_HIGH        0x04
#define DBG_HELPERS     0x08
#define DBG_TIMEOUT     0x10
#define DBG_SCAN        0x20
#define DBG_IO          0x40
#define DBG_IOF         0x80
#define DBG_ALL         0xFF

/*
 * standard debug level
 */
#ifdef DEBUG
static int dbg_level=(DBG_ALL & ~(DBG_IO | DBG_IOF));
#endif

#endif /* guard __DEBUG_H__ */

/* END PLUSTEK-PP_DBG.H .....................................................*/
