/* @file plustek-pp_scan.h
 * @brief the global header for the plustek driver
 *
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - changed _DODELAY macro to work properly for delays > 5 ms
 * 0.33 - no changes
 * 0.34 - no changes
 * 0.35 - removed _PTDRV_PUT_SCANNER_MODEL from ioctl interface
 * 0.36 - now including plustek-share.h from the backend path
 *        changed _INB/_OUTB to _INB_STATUS, _INB_CTRL, _INB_DATA ...
 * 0.37 - added _OUTB_ECTL/_INB_ECTL, _MAX_PTDEVS, _DRV_NAME and _MAX_BTNS
 *        added _OUTB_STATUS
 * 0.38 - added _IS_ASIC96() and _IS_ASIC98() macros
 * 0.39 - no changes
 * 0.40 - no changes
 * 0.41 - no changes
 * 0.42 - changed include names
 * 0.43 - no changes
 * 0.44 - no changes
 * .
 * <hr>
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
#ifndef __PLUSTEK_SCAN_H__
#define __PLUSTEK_SCAN_H__

#ifndef __KERNEL__

# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <stdio.h>
# include <unistd.h>
# include <sys/time.h>
# ifdef HAVE_SYS_SIGNAL_H
#  include <sys/signal.h>
# else
#  include <signal.h>
# endif
# include <sys/ioctl.h>
# ifdef HAVE_SYS_IO_H
#  include <sys/io.h>
# endif
#else
# include <linux/kernel.h>
# include <linux/init.h>
# include <linux/version.h>
# include "plustek-pp_sysdep.h"
# include <linux/delay.h>
# include <linux/parport.h>

#ifdef LINUX_24
# include <linux/parport_pc.h>
#endif	/* LINUX_24   */

#endif  /* __KERNEL__ */

/*.............................................................................
 * driver properties
 */
#define _DRV_NAME        "pt_drv"   /**< driver's name         */
#define	_MAX_PTDEVS      4          /**< support for 4 devices */
#define	_MAX_BTNS        6          /**< support for 6 buttons */
#define _PTDRV_MAJOR    40          /**< our major number      */

/*.............................................................................
 * for port operations
 */
# define _OPF	ps->IO.fnOut
# define _IPF	ps->IO.fnIn

#ifdef __KERNEL__

#define _OUTB_CTRL(pSD,port_value)	 _OPF(port_value,pSD->IO.pbControlPort)
#define _OUTB_DATA(pSD,port_value)	 _OPF(port_value,pSD->IO.pbSppDataPort)
#define _OUTB_ECTL(pSD,port_value)	 _OPF(port_value,(pSD->IO.portBase+0x402))

#define _INB_CTRL(pSD)				_IPF(pSD->IO.pbControlPort)
#define _INB_DATA(pSD)				_IPF(pSD->IO.pbSppDataPort)
#define _INB_EPPDATA(pSD)			_IPF(pSD->IO.pbEppDataPort)
#define _INB_STATUS(pSD)			_IPF(pSD->IO.pbStatusPort)
#define _INB_ECTL(pSD)				_IPF((pSD->IO.portBase+0x402))

#else

#define _OUTB_CTRL(pSD,port_value)   sanei_pp_outb_ctrl(pSD->pardev, port_value)
#define _OUTB_DATA(pSD,port_value)   sanei_pp_outb_data(pSD->pardev, port_value)
#define _OUTB_ECTL(pSD,port_value)

#define _INB_CTRL(pSD)               sanei_pp_inb_ctrl(pSD->pardev)
#define _INB_DATA(pSD)               sanei_pp_inb_data(pSD->pardev)
#define _INB_EPPDATA(pSD)            sanei_pp_inb_epp(pSD->pardev)
#define _INB_STATUS(pSD)             sanei_pp_inb_stat(pSD->pardev)

#endif

/*.............................................................................
 * for memory allocation
 */
#ifndef __KERNEL__
# define _KALLOC(x,y)   malloc(x)
# define _KFREE(x)		free(x)
# define _VMALLOC(x)	malloc(x)
# define _VFREE(x)		free(x)
#else
# define _KALLOC(x,y)   kmalloc(x,y)
# define _KFREE(x)		kfree(x)
# define _VMALLOC(x)	vmalloc(x)
# define _VFREE(x)		vfree(x)
#endif

/*
 * WARNING - never use the _SECOND define with the _DODELAY macro !!
 * they are for use the MiscStartTimer function and the _DO_UDELAY macro
 */
#ifndef __KERNEL__
typedef double TimerDef, *pTimerDef;
#else
typedef long long TimerDef, *pTimerDef;
#endif

#define _MSECOND    1000             /* based on 1 us */
#define _SECOND     (1000*_MSECOND)

/*.............................................................................
 * timer topics
 */
#ifndef __KERNEL__
# define _DO_UDELAY(usecs)   sanei_pp_udelay(usecs)
# define _DODELAY(msecs)     { int i; for( i = msecs; i--; ) _DO_UDELAY(1000); }
#else
# define _DO_UDELAY(usecs)   udelay(usecs)
# define _DODELAY(msecs)     mdelay(msecs)
#endif

/*.............................................................................
 * include the shared stuff right here, this concerns the ioctl interface
 * and the communication stuff
 */
#include "plustek-pp.h"

/*.............................................................................
 * WARNING: don't move the following headers above the previous defines !!!!!!!
 *
 * the include files for user-mode and kernel-mode program
 */
#include "plustek-pp_types.h"
#include "plustek-pp_hwdefs.h"
#include "plustek-pp_scandata.h"
#include "plustek-pp_procs.h"
#include "plustek-pp_dbg.h"

/*.............................................................................
 * some macros for convenience
 */
#define _IS_ASIC96(aid)    ((_ASIC_IS_96001 == aid) || (_ASIC_IS_96003 == aid))
#define _IS_ASIC98(aid)    ((_ASIC_IS_98001 == aid) || (_ASIC_IS_98003 == aid))

#endif /* guard __PLUSTEK_SCAN_H__ */

/* END PLUSTEK-PP_SCAN.H ....................................................*/
