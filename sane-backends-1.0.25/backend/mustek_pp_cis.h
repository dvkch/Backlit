/* sane - Scanner Access Now Easy.

   Copyright (C) 2001-2003 Eddy De Greef <eddy_de_greef at scarlet dot be>
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

   This file implements a SANE backend for Mustek PP flatbed _CIS_ scanners.
*/

#ifndef mustek_pp_cis_h
#define mustek_pp_cis_h

#include "../include/sane/sane.h"

/******************************************************************************
 * Read register symbols.
 *****************************************************************************/
typedef enum { 
                MA1015R_ASIC       = 0x00,
                MA1015R_SCAN_VAL   = 0x01,
                MA1015R_MOTOR      = 0x02,
                MA1015R_BANK_COUNT = 0x03 
             } 
Mustek_PP_1015R_reg;

/******************************************************************************
 * Read register bitmask symbols.
 *****************************************************************************/
typedef enum { 
                MA1015B_MOTOR_HOME   = 0x01,
                MA1015B_MOTOR_STABLE = 0x03 
             }
Mustek_PP_1015R_bit;

/******************************************************************************
 * Write register symbols: (bank number << 4) + register number.
 *****************************************************************************/
 
typedef enum { 
                MA1015W_RED_REF        = 0x00,
                MA1015W_GREEN_REF      = 0x01,
                MA1015W_BLUE_REF       = 0x02,
                MA1015W_DPI_CONTROL    = 0x03,

                MA1015W_BYTE_COUNT_HB  = 0x10,
                MA1015W_BYTE_COUNT_LB  = 0x11,
                MA1015W_SKIP_COUNT     = 0x12,
                MA1015W_EXPOSE_TIME    = 0x13,

                MA1015W_SRAM_SOURCE_PC = 0x20,
                MA1015W_MOTOR_CONTROL  = 0x21,
                MA1015W_UNKNOWN_42     = 0x22,
                MA1015W_UNKNOWN_82     = 0x23,

                MA1015W_POWER_ON_DELAY = 0x30,
                MA1015W_CCD_TIMING     = 0x31,
                MA1015W_CCD_TIMING_ADJ = 0x32,
                MA1015W_RIGHT_BOUND    = 0x33
             }
Mustek_PP_1015W_reg;


/******************************************************************************
 * Mustek MA1015 register tracing structure.
 * Can be used to trace the status of the registers of the MA1015 chipset
 * during debugging. Most fields are not used in production code.
 *****************************************************************************/
typedef struct Mustek_PP_1015_Registers
{
   SANE_Byte in_regs[4];
   SANE_Byte out_regs[4][4];
   SANE_Byte channel;
   
   Mustek_PP_1015R_reg current_read_reg;
   SANE_Int read_count;
   
   Mustek_PP_1015W_reg current_write_reg; /* always used */
   SANE_Int write_count;
} 
Mustek_PP_1015_Registers;


/******************************************************************************
 * CIS information
 *****************************************************************************/
typedef struct Mustek_PP_CIS_Info
{
   /* Expose time (= time the lamp is on ?) */
   SANE_Byte exposeTime;
   
   /* Power-on delay (= time between lamp on and start of capturing ?) */
   SANE_Byte powerOnDelay[3];
   
   /* Motor step control */
   SANE_Byte phaseType;
   
   /* Use 8K bank or 4K bank */
   SANE_Bool use8KBank;
   
   /* High resolution (600 DPI) or not (300 DPI) */
   SANE_Bool highRes;
   
   /* delay between pixels; reading too fast causes stability problems */
   SANE_Int delay;

   /* Register representation */
   Mustek_PP_1015_Registers regs;
   
   /* Current color channel */
   SANE_Int channel;
  
   /* Blocks motor movements during calibration */
   SANE_Bool dontMove;
   
   /* Prevents read increment the before the first read */
   SANE_Bool dontIncRead;
   
   /* Controls whether or not calibration parameters are transmitted 
      during CIS configuration */
   SANE_Bool setParameters;
   
   /* Number of lines to skip to reach the origin (used during calibration) */
   SANE_Int skipsToOrigin;
   
   /* Physical resolution of the CIS: either 300 or 600 DPI */
   SANE_Int cisRes;
   
   /* CCD mode (color/grayscale/lineart) */
   SANE_Int mode;
   
   /* how many positions to skip until scan area starts @ max res */
   SANE_Int skipimagebytes;
   
   /* how many image bytes to scan @ max res */
   SANE_Int imagebytes;
   
   /* total skip, adjusted to resolution */
   SANE_Int adjustskip;
   
   /* current resolution */
   SANE_Int res;
   
   /* current horizontal hardware resolution */
   SANE_Int hw_hres;
   
   /* current vertical hardware resolution */
   SANE_Int hw_vres;
   
   /* how many positions to scan for one pixel */
   SANE_Int hres_step;
   
   /* how many lines to scan for one scanline */
   SANE_Int line_step;
   
   /* inversion */
   SANE_Bool invert;
   
} Mustek_PP_CIS_Info;

struct Mustek_pp_Handle;
typedef struct Mustek_PP_CIS_dev
{
  /* device descriptor */
  struct Mustek_pp_Handle *desc;

  /* model identification (600CP/1200CP/1200CP+) */
  SANE_Int model;
  
  /* CIS status */
  Mustek_PP_CIS_Info CIS;

  /* used during calibration & return_home */
  Mustek_PP_CIS_Info Saved_CIS;

  /* bank count */
  int bank_count;

  /* those are used to count the hardware line the scanner is at, the
     line the current bank is at and the lines we've scanned */
  int line;
  int line_diff;
  int ccd_line;
  int lines_left;
  
  /* Configuration parameters that the user can calibrate */
  /* Starting position at the top */
  SANE_Int top_skip;
  /* Use fast skipping method for head movements ? (default: yes) */
  SANE_Bool fast_skip;
  /* Discrimination value to choose between black and white */
  SANE_Byte bw_limit; 
  /* Run in calibration mode ? (default: no) */
  SANE_Bool calib_mode;
  /* Extra delay between engine commands (ms). Default: zero. */
  SANE_Int engine_delay;
  
  /* temporary buffer for 1 line (of one color) */
  SANE_Byte *tmpbuf;

  /* calibration buffers (low cut, high cut) */
  SANE_Byte *calib_low[3];
  SANE_Byte *calib_hi[3];
  
  /* Number of pixels in calibration buffers (<= number of pixels to scan) */
  int calib_pixels;

} Mustek_PP_CIS_dev;

#define CIS_AVERAGE_NONE(dev)         Mustek_PP_1015_send_command(dev, 0x05)
#define CIS_AVERAGE_TWOPIXEL(dev)     Mustek_PP_1015_send_command(dev, 0x15)
#define CIS_AVERAGE_THREEPIXEL(dev)   Mustek_PP_1015_send_command(dev, 0x35)
#define CIS_WIDTH_4K(dev)             Mustek_PP_1015_send_command(dev, 0x05)
#define CIS_WIDTH_8K(dev)             Mustek_PP_1015_send_command(dev, 0x45)
#define CIS_STOP_TOGGLE(dev)          Mustek_PP_1015_send_command(dev, 0x85)

#define CIS_PIP_AS_INPUT(dev)         Mustek_PP_1015_send_command(dev, 0x46)
#define CIS_PIP_AS_OUTPUT_0(dev)      Mustek_PP_1015_send_command(dev, 0x06)
#define CIS_PIP_AS_OUTPUT_1(dev)      Mustek_PP_1015_send_command(dev, 0x16)
#define CIS_POP_AS_INPUT(dev)         Mustek_PP_1015_send_command(dev, 0x86)
#define CIS_POP_AS_OUTPUT_0(dev)      Mustek_PP_1015_send_command(dev, 0x06)
#define CIS_POP_AS_OUTPUT_1(dev)      Mustek_PP_1015_send_command(dev, 0x26)

#define CIS_INC_READ(dev)             Mustek_PP_1015_send_command(dev, 0x07)
#define CIS_CLEAR_WRITE_BANK(dev)     Mustek_PP_1015_send_command(dev, 0x17)
#define CIS_CLEAR_READ_BANK(dev)      Mustek_PP_1015_send_command(dev, 0x27)
#define CIS_CLEAR_FULLFLAG(dev)       Mustek_PP_1015_send_command(dev, 0x37)
#define CIS_POWER_ON(dev)             Mustek_PP_1015_send_command(dev, 0x47)
#define CIS_POWER_OFF(dev)            Mustek_PP_1015_send_command(dev, 0x57)
#define CIS_CLEAR_WRITE_ADDR(dev)     Mustek_PP_1015_send_command(dev, 0x67)
#define CIS_CLEAR_TOGGLE(dev)         Mustek_PP_1015_send_command(dev, 0x77)

#define CIS_NO(dev)                   Mustek_PP_1015_send_command(dev, 0x08)
#define CIS_OST_POS(dev)              Mustek_PP_1015_send_command(dev, 0x18)
#define CIS_OST_TYP(dev)              Mustek_PP_1015_send_command(dev, 0x28)
#define CIS_OP_MOD_0(dev)             Mustek_PP_1015_send_command(dev, 0x48)
#define CIS_OP_MOD_1(dev)             Mustek_PP_1015_send_command(dev, 0x88)

#endif /* __mustek_pp_cis_h */
