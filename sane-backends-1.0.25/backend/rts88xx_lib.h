/* sane - Scanner Access Now Easy.

   Copyright (C) 2007-2012 stef.dev@free.fr

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/

#ifndef RTS88XX_LIB_H
#define RTS88XX_LIB_H
#include "../include/sane/sane.h"
#include "../include/sane/sanei_usb.h"
#include <unistd.h>
#include <string.h>

/* TODO put his in a place where it can be reused */

#define DBG_error0      0	/* errors/warnings printed even with debuglevel 0 */
#define DBG_error       1	/* fatal errors */
#define DBG_init        2	/* initialization and scanning time messages */
#define DBG_warn        3	/* warnings and non-fatal errors */
#define DBG_info        4	/* informational messages */
#define DBG_proc        5	/* starting/finishing functions */
#define DBG_io          6	/* io functions */
#define DBG_io2         7	/* io functions that are called very often */
#define DBG_data        8	/* log data sent and received */

/* 
 * defines for registers name
 */
#define CONTROL_REG             0xb3
#define CONTROLER_REG           0x1d

/* geometry registers */
#define START_LINE		0x60
#define END_LINE		0x62
#define START_PIXEL		0x66
#define END_PIXEL		0x6c

#define RTS88XX_MAX_XFER_SIZE 0xFFC0

#define LOBYTE(x)  ((uint8_t)((x) & 0xFF))
#define HIBYTE(x)  ((uint8_t)((x) >> 8))

/* this function init the rts88xx library */
void sanei_rts88xx_lib_init (void);
SANE_Bool sanei_rts88xx_is_color (SANE_Byte * regs);

void sanei_rts88xx_set_gray_scan (SANE_Byte * regs);
void sanei_rts88xx_set_color_scan (SANE_Byte * regs);
void sanei_rts88xx_set_offset (SANE_Byte * regs, SANE_Byte red,
			       SANE_Byte green, SANE_Byte blue);
void sanei_rts88xx_set_gain (SANE_Byte * regs, SANE_Byte red, SANE_Byte green,
			     SANE_Byte blue);
void sanei_rts88xx_set_scan_frequency (SANE_Byte * regs, int frequency);

/*
 * set scan area
 */
void sanei_rts88xx_set_scan_area (SANE_Byte * reg, SANE_Int ystart,
				  SANE_Int yend, SANE_Int xstart,
				  SANE_Int xend);

/*
 * read one register at given index 
 */
SANE_Status sanei_rts88xx_read_reg (SANE_Int devnum, SANE_Int index,
				    SANE_Byte * reg);

/*
 * read scanned data from scanner up to the size given. The actual length read is returned.
 */
SANE_Status sanei_rts88xx_read_data (SANE_Int devnum, SANE_Word * length,
				     unsigned char *dest);

/*
 * write one register at given index 
 */
SANE_Status sanei_rts88xx_write_reg (SANE_Int devnum, SANE_Int index,
				     SANE_Byte * reg);

/*
 * write length consecutive registers, starting at index
 * register 0xb3 is never wrote in bulk register write, so we split
 * write if it belongs to the register set sent
 */
SANE_Status sanei_rts88xx_write_regs (SANE_Int devnum, SANE_Int start,
				      SANE_Byte * source, SANE_Int length);

/* read several registers starting at the given index */
SANE_Status sanei_rts88xx_read_regs (SANE_Int devnum, SANE_Int start,
				     SANE_Byte * dest, SANE_Int length);

/*
 * get status by reading registers 0x10 and 0x11 
 */
SANE_Status sanei_rts88xx_get_status (SANE_Int devnum, SANE_Byte * regs);
/*
 * set status by writing registers 0x10 and 0x11 
 */
SANE_Status sanei_rts88xx_set_status (SANE_Int devnum, SANE_Byte * regs,
				      SANE_Byte reg10, SANE_Byte reg11);

/*
 * get lamp status by reading registers 0x84 to 0x8d  
 */
SANE_Status sanei_rts88xx_get_lamp_status (SANE_Int devnum, SANE_Byte * regs);

/* reset lamp */
SANE_Status sanei_rts88xx_reset_lamp (SANE_Int devnum, SANE_Byte * regs);

/* get lcd panel status */
SANE_Status sanei_rts88xx_get_lcd (SANE_Int devnum, SANE_Byte * regs);

/*
 * write to special control register CONTROL_REG=0xb3 
 */
SANE_Status sanei_rts88xx_write_control (SANE_Int devnum, SANE_Byte value);

/*
 * send the cancel control sequence
 */
SANE_Status sanei_rts88xx_cancel (SANE_Int devnum);

/*
 * read available data count from scanner
 */
SANE_Status sanei_rts88xx_data_count (SANE_Int devnum, SANE_Word * count);

/*
 * wait for scanned data to be available, if busy is true, check is scanner is busy
 * while waiting. The number of data bytes of available data is returned in 'count'.
 */
SANE_Status sanei_rts88xx_wait_data (SANE_Int devnum, SANE_Bool busy,
				     SANE_Word * count);

 /*
  * write the given number of bytes pointed by value into memory
  */
SANE_Status sanei_rts88xx_write_mem (SANE_Int devnum, SANE_Int length,
				     SANE_Int extra, SANE_Byte * value);

 /*
  * set memory with the given data
  */
SANE_Status sanei_rts88xx_set_mem (SANE_Int devnum, SANE_Byte ctrl1,
				   SANE_Byte ctrl2, SANE_Int length,
				   SANE_Byte * value);

 /*
  * read the given number of bytes from memory into buffer
  */
SANE_Status sanei_rts88xx_read_mem (SANE_Int devnum, SANE_Int length,
				    SANE_Byte * value);

 /*
  * get memory
  */
SANE_Status sanei_rts88xx_get_mem (SANE_Int devnum, SANE_Byte ctrl1,
				   SANE_Byte ctrl2, SANE_Int length,
				   SANE_Byte * value);

 /*
  * write to the nvram controler
  */
SANE_Status sanei_rts88xx_nvram_ctrl (SANE_Int devnum, SANE_Int length,
				      SANE_Byte * value);

 /*
  * setup nvram
  */
SANE_Status sanei_rts88xx_setup_nvram (SANE_Int devnum, SANE_Int length,
				       SANE_Byte * value);


 /* does a simple scan, putting data in image */
/* SANE_Status sanei_rts88xx_simple_scan (SANE_Int devnum, SANE_Byte * regs, int regcount, SANE_Word size, unsigned char *image); */
#endif /* not RTS88XX_LIB_H */
