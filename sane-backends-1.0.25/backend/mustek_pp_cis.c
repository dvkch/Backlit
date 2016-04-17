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

/*
   Global picture
   
      Mustek_PP_handle -> Mustek_PP_dev
                       -> priv = Mustek_PP_CIS_dev -> CIS
*/
                       
/*
 * This flag determines whether the scanner uses fast skipping at high
 * resolutions. It is possible that this fast skipping introduces 
 * inaccuracies. It if turns out to be a problem, fast skipping can
 * be disabled by setting this flag to 0.
 */
#define MUSTEK_PP_CIS_FAST_SKIP 1
#define MUSTEK_PP_CIS_WAIT_BANK 200

/*
 * These parameters determine where the scanable area starts at the top.
 * If there is a consistent offset error, you can tune it through these
 * parameters. Note that an inaccuracy in the order of 1 mm seems to be
 * normal for the Mustek 600/1200 CP series.
 */
#define MUSTEK_PP_CIS_600CP_DEFAULT_SKIP   	250
#define MUSTEK_PP_CIS_1200CP_DEFAULT_SKIP 	330

/*
 * Number of scan lines on which the average is taken to determine the 
 * maximum number of color levels.
 */
#define MUSTEK_PP_CIS_AVERAGE_COUNT 32

#define MUSTEK_PP_CIS600		1
#define MUSTEK_PP_CIS1200		2
#define MUSTEK_PP_CIS1200PLUS 		3

#define MUSTEK_PP_CIS_CHANNEL_RED	0
#define MUSTEK_PP_CIS_CHANNEL_GREEN	1
#define MUSTEK_PP_CIS_CHANNEL_BLUE	2
#define MUSTEK_PP_CIS_CHANNEL_GRAY	1

#define MUSTEK_PP_CIS_MAX_H_PIXEL 	5118
#define MUSTEK_PP_CIS_MAX_V_PIXEL 	7000

#define MUSTEK_PP_CIS_MOTOR_REVERSE 	0

#include "../include/sane/config.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include "../include/sane/sane.h"
#include "../include/sane/sanei_pa4s2.h"
#define DEBUG_DECLARE_ONLY
#include "mustek_pp.h"
#include "mustek_pp_decl.h"
#include "mustek_pp_cis.h"

/******************************************************************************
 ******************************************************************************
 ***                 MA1015 chipset related functionality                   ***
 ******************************************************************************
 *****************************************************************************/
 
/*
   These defines control some debugging functionality 

   #define M1015_TRACE_REGS   -> trace the status of the internal registers
   #define M1015_LOG_HL       -> create a high-level log file (register-level)
   #define M1015_LOG_LL       -> create a low-level log file (byte-level)
   
   By default, all logging/tracing is turned off.
*/

/******************************************************************************
 * Low level logging: logs read and writes at the byte level, similar to
 *                    the sequences produced by tool of Jochen Eisinger
 *                    for analysing the TWAIN driver communication.
 *                    This simplifies comparison of the sequences.
 *****************************************************************************/
#ifdef M1015_LOG_LL

   static FILE* M1015_LOG_1;
   
   #define M1015_START_LL\
      M1015_LOG_1 = fopen("cis_ll.log", "w");
        
   #define M1015_STOP_LL\
      fclose(M1015_LOG_1);
   
   #define SANEI_PA4S2_WRITEBYTE(fd, reg, val)\
      do\
      {\
         sanei_pa4s2_writebyte (fd, reg, val);\
         fprintf(M1015_LOG_1, "\tsanei_pa4s2_writebyte(fd, %d, 0x%02X);\n", \
                 reg, val);\
      } while (0)
   
   static const char* cis_last_rreg_name;
   static int cis_read_count;       
   
   #define SANEI_PA4S2_READBEGIN(fd, reg)\
      do\
      {\
         cis_last_rreg_name = Mustek_PP_1015_reg_r_name(reg);\
         cis_read_count = 0;\
         sanei_pa4s2_readbegin(fd, reg);\
      } while (0)
   
   #define SANEI_PA4S2_READBYTE(fd, val)\
      do\
      {\
         sanei_pa4s2_readbyte(fd, val);\
         ++cis_read_count;\
      } while (0)
   
   #define SANEI_PA4S2_READEND(fd)\
      do\
      {\
         sanei_pa4s2_readend(fd);\
         fprintf(M1015_LOG_1, "\tread_reg(%s, %d);\n", \
                 cis_last_rreg_name, cis_read_count);\
      } while (0)
      
   #define M1015_MARK_LL(info)\
      fprintf(M1015_LOG_1, "* %s\n", info);
      
#else /* M1015_LOG_LL */

   #define M1015_START_LL
   #define M1015_STOP_LL
   
   #define SANEI_PA4S2_WRITEBYTE(fd, reg, val)\
      sanei_pa4s2_writebyte (fd, reg, val)
      
   #define SANEI_PA4S2_READBEGIN(fd, reg)\
      sanei_pa4s2_readbegin(fd, reg)
   
   #define SANEI_PA4S2_READBYTE(fd, val)\
      sanei_pa4s2_readbyte(fd, val)
   
   #define SANEI_PA4S2_READEND(fd)\
      sanei_pa4s2_readend(fd)
      
   #define M1015_MARK_LL(info)
      
#endif /* M1015_LOG_LL */

 
/******************************************************************************
 * High-level logging: traces the flow of the driver in a hierarchical way
 *                     up to the level of register acccesses.
 *****************************************************************************/ 
#ifdef M1015_LOG_HL

   static FILE* M1015_LOG_2;
   static char hl_prev_line[4096], hl_next_line[4096], hl_repeat_count;
   
   /*
    * A few variables for hierarchical log message indentation.
    */

   static const char* cis_indent_start =
      "                                                                      ";
   static const char* cis_indent;
   static const char* cis_indent_end;
   
   #define M1015_START_HL\
       M1015_LOG_2 = fopen("cis_hl.log", "w");\
       cis_indent = cis_indent_start + strlen(cis_indent_start);\
       cis_indent_end = cis_indent;\
       hl_prev_line[0] = 0;\
       hl_next_line[0] = 0;\
       hl_repeat_count = 0;
       
   #define M1015_FLUSH_HL\
      if (strcmp(hl_prev_line, hl_next_line))\
      {\
         fprintf(M1015_LOG_2, &hl_prev_line[0]);\
         strcpy(&hl_prev_line[0], &hl_next_line[0]);\
         if (hl_repeat_count != 0)\
         {\
            fprintf(M1015_LOG_2, "%s [last message repeated %d times]\n",\
                    cis_indent, hl_repeat_count+1);  \
         }\
         hl_repeat_count = 0;\
      }\
      else\
      {\
         hl_repeat_count += 1;\
      }
       
   #define M1015_MARK(info)\
      sprintf(&hl_next_line[0], "%s+ %s\n", cis_indent, info);\
      M1015_FLUSH_HL

   #define M1015_STOP_HL\
       hl_next_line[0] = 0;\
       M1015_FLUSH_HL\
       fclose(M1015_LOG_2); 
   
#else  /* M1015_LOG_HL */ 

   #define M1015_START_HL
   #define M1015_STOP_HL
   #define M1015_MARK(info)
   #define M1015_FLUSH_HL
      
#endif /* M1015_LOG_HL */

#ifdef M1015_TRACE_REGS
   #define M1015_DISPLAY_REGS(dev, msg) Mustek_PP_1015_display_regs(dev, msg)
   #define M1015_DISPLAY_REG(msg, val)  Mustek_PP_1015_display_reg(msg, val)
#else
   #define M1015_DISPLAY_REGS(dev, msg) 
   #define M1015_DISPLAY_REG(msg, val)  
#endif

 
#if defined (M1015_LOG_HL) || defined (M1015_LOG_LL)
static const char* 
Mustek_PP_1015_reg_r_name(Mustek_PP_1015R_reg id)
{
   static const char* names[4] = { "ASIC", "SCAN_VAL", "MOTOR", "BANK_COUNT" };
   return names[id & 0x03];
}

static const char* 
Mustek_PP_1015_bit_name(Mustek_PP_1015R_bit id)
{
   static const char* names[4] = { "????", "MOTOR_HOME", "????", "MOTOR_BUSY" };
   return names[id & 0x03];
}

static const char* 
Mustek_PP_1015_reg_w_name(Mustek_PP_1015R_reg id)
{
   static const char* names[4][4] = 
   {
      { "RED_REF",        "GREEN_REF",     "BLUE_REF",       "DPI_CONTROL" },
      { "BYTE_COUNT_HB",  "BYTE_COUNT_LB", "SKIP_COUNT",     "EXPOSE_TIME" },
      { "SRAM_SOURCE_PC", "MOTOR_CONTROL", "UNKNOWN_42",     "UNKNOWN_82"  },
      { "POWER_ON_DELAY", "CCD_TIMING",    "CCD_TIMING_ADJ", "RIGHT_BOUND" }
   };
   return names[(id & 0x30) >> 4][id & 0x03];
}
#endif

/******************************************************************************
 * Converts a register value to a hex/dec/bin representation.
 *****************************************************************************/
static const char*
Mustek_PP_1015_show_val(int val)
{
   /*
      Since we use a static temporary buffer, we must make sure that the
      buffer isn't altered while it is still in use (typically because
      more than one value is converted in a printf statement). 
      Therefore the buffer is organized as a ring buffer. If should contain
      at least 21 elements in order to be able to display all registers 
      with one printf statement.
   */
   #define Mustek_PP_1015_RING_BUFFER_SIZE 50
   static char buf[Mustek_PP_1015_RING_BUFFER_SIZE][64];
   static int index = 0;
   int i;
   char* current = (char*)buf[index++];
   
   if (index >= Mustek_PP_1015_RING_BUFFER_SIZE) index = 0;
   
   if (val < 0)
   {
      /* The register has not been initialized yet. */
      sprintf(current, "---- (---) --------");
   }
   else
   {
      sprintf(current, "0x%02X (%3d) ", val & 0xFF, val & 0xFF);
      for (i=0; i<8; ++i)
      {
         sprintf(current+11+i, "%d", (val >> (7-i)) & 1);
      }
   }
   return current;
}

#ifdef M1015_TRACE_REGS
/******************************************************************************
 * Displays the contents of all registers of the scanner on stderr.
 *****************************************************************************/
static void
Mustek_PP_1015_display_regs(Mustek_PP_CIS_dev * dev, const char* info)
{
   /*
    * Register naming convention:
    *   Rx   : read-only register no. x
    *   ByWx : write-only register no. x of bank no. y
    */
   
   fprintf(stderr, 
           "\n"
           "Register status: %s\n"
           "\n"
           "    R0: %s  : ASIC info\n"
           "    R1: %s  : scan value\n"
           "    R2: %s  : CCD/motor info\n"
           "    R3: %s  : bank count\n"
           "\n"
           "  B0W0: %s  : red reference\n"
           "  B0W1: %s  : green reference\n"
           "  B0W2: %s  : blue reference\n"
           "  B0W3: %s  : DPI control\n"
           "\n"
           "  B1W0: %s  : byte count, high byte\n"
           "  B1W1: %s  : byte count, low byte\n"
           "  B1W2: %s  : skip x32 pixels\n"
           "  B1W3: %s  : expose time (CCDWIDTH)\n"
           "\n"
           "  B2W0: %s  : SRAM source PC\n"
           "  B2W1: %s  : motor control\n"
           "  B2W2: %s  : -\n"
           "  B2W3: %s  : -\n"
           "\n"
           "  B3W0: %s  : power on delay\n"
           "  B3W1: %s  : CCD timing - always 0x05\n"
           "  B3W2: %s  : CCD timing adjust - always 0x00\n"
           "  B3W3: %s  : right bound (not used)\n"
           "\n"
           "  CHAN: %s  : channel [%s]\n"
           "\n",
           info,
           Mustek_PP_1015_show_val (dev->CIS.regs.in_regs[0]),
           Mustek_PP_1015_show_val (dev->CIS.regs.in_regs[1]),
           Mustek_PP_1015_show_val (dev->CIS.regs.in_regs[2]),
           Mustek_PP_1015_show_val (dev->CIS.regs.in_regs[3]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[0][0]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[0][1]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[0][2]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[0][3]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[1][0]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[1][1]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[1][2]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[1][3]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[2][0]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[2][1]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[2][2]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[2][3]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[3][0]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[3][1]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[3][2]),
           Mustek_PP_1015_show_val (dev->CIS.regs.out_regs[3][3]),
           Mustek_PP_1015_show_val (dev->CIS.regs.channel),
           (dev->CIS.regs.channel == 0x80 ? "RED"   :
           (dev->CIS.regs.channel == 0x40 ? "GREEN" :
           (dev->CIS.regs.channel == 0xC0 ? "BLUE"  : "unknown")))
           );
}   

/******************************************************************************
 * Displays a single register value
 *****************************************************************************/
static void 
Mustek_PP_1015_display_reg(const char* info, int val)
{
   fprintf (stderr, "%s: %s\n", info, Mustek_PP_1015_show_val(val));
}

#endif /* M1015_TRACE_REGS */


/******************************************************************************
 *
 * Reads one of the 4 internal registers of the scanner
 *
 *   0: ASIC identification
 *   1: scan values
 *   2: CCD info / motor info
 *   3: bank count info
 *
 *****************************************************************************/
static SANE_Byte
Mustek_PP_1015_read_reg(Mustek_PP_CIS_dev * dev, Mustek_PP_1015R_reg reg)
{
   SANE_Byte tmp;
   assert(reg <= 3);
   
   SANEI_PA4S2_READBEGIN (dev->desc->fd, reg & 0x03); 
   SANEI_PA4S2_READBYTE (dev->desc->fd, &tmp); 
   SANEI_PA4S2_READEND (dev->desc->fd); 
   
#ifdef M1015_LOG_HL
   sprintf(&hl_next_line[0], "%s read_reg(%s); [%s]\n", cis_indent, 
           Mustek_PP_1015_reg_r_name(reg), Mustek_PP_1015_show_val(tmp));
   M1015_FLUSH_HL;
#endif
   
#ifdef M1015_TRACE_REGS   
   dev->CIS.regs.in_regs[reg & 0x03] = tmp;
#endif
   
   return tmp;
}

/******************************************************************************
 *
 * Waits for a bit of register to become 1 or 0. The period of checking can be
 * controlled through the sleep parameter (microseconds).
 *
 *****************************************************************************/
static SANE_Bool
Mustek_PP_1015_wait_bit(Mustek_PP_CIS_dev * dev, Mustek_PP_1015R_reg reg,
                        Mustek_PP_1015R_bit bit, SANE_Bool on, unsigned period)
{
   SANE_Byte tmp;
   SANE_Byte mask, val;
   int tries = 0;
   
   assert(reg <= 3);
   assert(bit <= 3);
   
   mask = 1 << bit;
   
   /* We don't want to wait forever */
   while (dev->desc->state != STATE_CANCELLED)
   {
#if defined (M1015_LOG_LL) || defined (M1015_LOG_HL)
      ++tries;
#endif

      sanei_pa4s2_readbegin (dev->desc->fd, reg & 0x03); 
      sanei_pa4s2_readbyte (dev->desc->fd, &tmp); 
      sanei_pa4s2_readend (dev->desc->fd); 
      
#ifdef M1015_LOG_HL
   sprintf(&hl_next_line[0], "%s wait_bit(%s, %s, %d): %s %s;\n", cis_indent, 
           Mustek_PP_1015_reg_r_name(reg), Mustek_PP_1015_bit_name(bit),
           on?1:0, Mustek_PP_1015_show_val(mask), Mustek_PP_1015_show_val(tmp));
   M1015_FLUSH_HL;
#endif
      val = ((on == SANE_TRUE) ? tmp : ~tmp ) & mask;
      
      if (val != 0) break;
            
      if (period) usleep(period);
      
      if (tries > 50000) 
      {
#ifdef M1015_LOG_HL
         sprintf(&hl_next_line[0], "%s wait_bit(%s, %s, %d): failed;\n", cis_indent, 
           Mustek_PP_1015_reg_r_name(reg), Mustek_PP_1015_bit_name(bit), on?1:0);
         M1015_FLUSH_HL;
#endif
         DBG(2, "Mustek_PP_1015_wait_bit: failed (reg %d, bit %d, on: %d)\n",
             reg, bit, on?1:0);
         return SANE_FALSE;
      }
   }
   
#ifdef M1015_LOG_HL
   sprintf(&hl_next_line[0], "%s wait_bit(%s, %s, %d);\n", cis_indent, 
           Mustek_PP_1015_reg_r_name(reg), Mustek_PP_1015_bit_name(bit), on?1:0);
   M1015_FLUSH_HL;
#endif
#ifdef M1015_LOG_LL
   fprintf(M1015_LOG_1, "\tread_reg(%s, %d);\n", Mustek_PP_1015_reg_r_name(reg),
           tries);
#endif
   
#ifdef M1015_TRACE_REGS   
   dev->CIS.regs.in_regs[reg & 0x03] = tmp;
#endif
   return dev->desc->state != STATE_CANCELLED ? SANE_TRUE : SANE_FALSE;
}   

/******************************************************************************
 *
 * Writes one out of 4 registers of one of the 4 register banks (I guess)
 *
 * Bank 0
 *    0: voltage red   --+
 *    1: voltage green   +-> always set to 0x96
 *    2: voltage blue  --+
 *    3: DPI control
 *
 * Bank 1
 *    0: line adjust (?) - high byte
 *    1: line adjust (?) - low  byte
 *    2: unknown                       (values seen: 0x00, 0x02, 0x03, 0x1D)
 *    3: expose time (?)               (values seen: 0xAA, 0xFD, 0xFE, 0xFF)
 *
 * Bank 2
 *    0: unknown, used to start linear sequence during calibration
 *    1: motor control code (forward, return home, ...)
 *    2: never used 
 *    3: never used
 * 
 * Bank 3
 *    0: reduction factor (16bit internal -> 8bit) -> target for calibration
 *    1: unknown -> always set to 0x05
 *    2: unknown -> always set to 0x00
 *    3: never used
 *
 *****************************************************************************/
  
static void
Mustek_PP_1015_write_reg(Mustek_PP_CIS_dev * dev, Mustek_PP_1015W_reg reg, SANE_Byte val)
{
   
   SANE_Byte regBank = (reg & 0xF0) >> 4;
   SANE_Byte regNo   = (reg & 0x0F);
   
   assert (regNo   <= 3);
   assert (regBank <= 3);
   
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, (1 << (4+regNo))+ regBank);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 5, val);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, regBank);
   
#ifdef M1015_TRACE_REGS   
   dev->CIS.regs.out_regs[regBank][regNo] = val;
#endif

#ifdef M1015_LOG_HL
   sprintf(&hl_next_line[0], "%s write_reg(%s, 0x%02X);\n", cis_indent, 
           Mustek_PP_1015_reg_w_name(reg), val);
   M1015_FLUSH_HL;
#endif
}

/******************************************************************************
 *
 * Writes 2 values to 2 adjecent registers.
 * It is probably equivalent to 2 simple write operations (but I'm not sure).
 *
 *   val1 is written to register[regNo]
 *   val2 is written to register[regNo+1]
 *
 *****************************************************************************/
static void
Mustek_PP_1015_write_reg2(Mustek_PP_CIS_dev * dev, Mustek_PP_1015W_reg reg, 
               SANE_Byte val1, SANE_Byte val2)
{
   SANE_Byte regBank = (reg & 0xF0) >> 4;
   SANE_Byte regNo   = (reg & 0x0F);
   
   assert (regNo   <= 2);
   assert (regBank <= 3);
   
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, (1 << (4+regNo))+ regBank);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 5, val1);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, (1 << (5+regNo))+ regBank);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 5, val2);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, regBank);
   
#ifdef M1015_TRACE_REGS   
   dev->CIS.regs.out_regs[regBank][regNo]   = val1;
   dev->CIS.regs.out_regs[regBank][regNo+1] = val2;
#endif
   
#ifdef M1015_LOG_HL
   sprintf(&hl_next_line[0], "%s write_reg2(%s, 0x%02X, 0x%02X);\n", 
           cis_indent, Mustek_PP_1015_reg_w_name(reg), val1, val2);
   M1015_FLUSH_HL;
#endif
}

/******************************************************************************
 *
 * Writes 3 values to 3 adjecent registers.
 * It is probably equivalent to 3 simple write operations (but I'm not sure).
 *
 *   val1 is written to register[regNo]
 *   val2 is written to register[regNo+1]
 *   val3 is written to register[regNo+2]
 *
 *****************************************************************************/
static void
Mustek_PP_1015_write_reg3(Mustek_PP_CIS_dev * dev, Mustek_PP_1015W_reg reg, 
               SANE_Byte val1, SANE_Byte val2, SANE_Byte val3)
{
   SANE_Byte regBank = (reg & 0xF0) >> 4;
   SANE_Byte regNo   = (reg & 0x0F);
   
   assert (regNo   <= 1);
   assert (regBank <= 3);
   
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, (1 << (4+regNo))+ regBank);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 5, val1);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, (1 << (5+regNo))+ regBank);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 5, val2);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, (1 << (6+regNo))+ regBank);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 5, val3);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, regBank);
   
#ifdef M1015_TRACE_REGS   
   dev->CIS.regs.out_regs[regBank][regNo  ] = val1;
   dev->CIS.regs.out_regs[regBank][regNo+1] = val2;
   dev->CIS.regs.out_regs[regBank][regNo+2] = val3;
#endif
   
#ifdef M1015_LOG_HL
   sprintf(&hl_next_line[0], "%s write_reg3(%s, 0x%02X, 0x%02X, 0x%02X);\n", 
           cis_indent, Mustek_PP_1015_reg_w_name(reg), val1, val2, val3);
   M1015_FLUSH_HL;
#endif
}

/******************************************************************************
 * Opens a register for a (series of) write operation(s).
 *****************************************************************************/ 
static void
Mustek_PP_1015_write_reg_start(Mustek_PP_CIS_dev * dev, Mustek_PP_1015W_reg reg)
{
   SANE_Byte regBank = (reg & 0xF0) >> 4;
   SANE_Byte regNo   = (reg & 0x0F);
   
   assert (regNo   <= 3);
   assert (regBank <= 3);
   
   dev->CIS.regs.current_write_reg = reg;
   
#ifdef M1015_LOG_HL
   dev->CIS.regs.write_count = 0;
#endif
      
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, (1 << (4+regNo))+ regBank);
}

/******************************************************************************
 * Writes a value to the currently open register.
 *****************************************************************************/ 
static void
Mustek_PP_1015_write_reg_val(Mustek_PP_CIS_dev * dev, SANE_Byte val)
{
#ifdef M1015_TRACE_REGS   
   SANE_Byte regBank = (dev->CIS.regs.current_write_reg & 0xF0) >> 4;
   SANE_Byte regNo   = (dev->CIS.regs.current_write_reg & 0x0F);
   
   assert (regNo   <= 3);
   assert (regBank <= 3);
   
   dev->CIS.regs.out_regs[regBank][regNo] = val;
#endif
   
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 5, val);
   
#ifdef M1015_LOG_HL
   ++dev->CIS.regs.write_count;
#endif
}

/******************************************************************************
 * Closes a register after a (series of) write operation(s).
 *****************************************************************************/ 
static void
Mustek_PP_1015_write_reg_stop(Mustek_PP_CIS_dev * dev)
{
   SANE_Byte regBank = (dev->CIS.regs.current_write_reg & 0xF0) >> 4;
#ifdef M1015_LOG_HL
   SANE_Byte regNo   = (dev->CIS.regs.current_write_reg & 0x0F);
   assert (regNo   <= 3);
   
   sprintf(&hl_next_line[0], "%s write_reg_multi(%s, *%d);\n",  cis_indent,
           Mustek_PP_1015_reg_w_name(dev->CIS.regs.current_write_reg), 
           dev->CIS.regs.write_count);
   M1015_FLUSH_HL;
#endif
   assert (regBank <= 3);
   
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, regBank);
}   

/******************************************************************************
 *
 * Sends a command to the scanner. The command should not access one of the
 * internal registers, ie., the 3rd bit should not be zero.
 *
 *****************************************************************************/
static void 
Mustek_PP_1015_send_command(Mustek_PP_CIS_dev * dev, SANE_Byte command)
{
   assert (command & 0x04);
   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, command);
   
#ifdef M1015_LOG_HL
   sprintf(&hl_next_line[0], "%s send_command(0x%02X);\n", cis_indent, command);
   M1015_FLUSH_HL;
#endif
}

/******************************************************************************
 ##############################################################################
 ##                              CIS driver                                  ##
 ##############################################################################
 *****************************************************************************/
 
/******************************************************************************
 * Resolution conversion functions
 *****************************************************************************/
static int
max2hw_hres(Mustek_PP_CIS_dev *dev, int dist)
{
   return (int)((dist * dev->CIS.hw_hres) / dev->desc->dev->maxres + 0.5);
}

#ifdef NOT_USED
static int
max2hw_vres(Mustek_PP_CIS_dev *dev, int dist)
{
   return (int)((dist * dev->CIS.hw_vres) / dev->desc->dev->maxres + 0.5);
}
#endif

static int
max2cis_hres(Mustek_PP_CIS_dev *dev, int dist)
{
   return (int)((dist * dev->CIS.cisRes) / dev->desc->dev->maxres + 0.5);
}

static int
cis2max_res(Mustek_PP_CIS_dev *dev, int dist)
{
   return (int)((dist * dev->desc->dev->maxres) / dev->CIS.cisRes + 0.5);
}

#ifdef NOT_USED
static int
hw2max_vres(Mustek_PP_CIS_dev *dev, int dist)
{
   return (int)((dist * dev->desc->dev->maxres) / dev->CIS.hw_vres + 0.5);
}
#endif

/******************************************************************************
 * Attempts to extract the current bank no.
 *****************************************************************************/
static void
cis_get_bank_count(Mustek_PP_CIS_dev *dev)
{
   dev->bank_count = (Mustek_PP_1015_read_reg(dev, MA1015R_BANK_COUNT) & 0x7);
   if (dev->CIS.use8KBank) dev->bank_count >>= 1;
}

/******************************************************************************
 * Triggers a bank switch (I assume).
 *****************************************************************************/
static void
cis_set_sti(Mustek_PP_CIS_dev *dev)
{
   SANEI_PA4S2_WRITEBYTE(dev->desc->fd, 3, 0xFF);
   dev->bank_count++;
   dev->bank_count &= (dev->CIS.use8KBank == SANE_TRUE) ? 3 : 7;
}

/******************************************************************************
 * Wait till the bank with a given number becomes available.
 *****************************************************************************/
static SANE_Bool
cis_wait_bank_change (Mustek_PP_CIS_dev * dev, int bankcount)
{
  struct timeval start, end;
  unsigned long diff;
  int firsttime = 1;
  
  gettimeofday (&start, NULL);

  do
    {
      if (1 /*niceload*/)
	{
	  if (firsttime)
	    firsttime = 0;
	  else
	    usleep (10);	/* for a little nicer load */
	}
      cis_get_bank_count (dev);

      gettimeofday (&end, NULL);
      diff = (end.tv_sec * 1000 + end.tv_usec / 1000) -
	(start.tv_sec * 1000 + start.tv_usec / 1000);

    }
  while ((dev->bank_count != bankcount) && (diff < MUSTEK_PP_CIS_WAIT_BANK));
  
  if (dev->bank_count != bankcount && dev->desc->state != STATE_CANCELLED)
  {
     u_char tmp;
     tmp = Mustek_PP_1015_read_reg(dev, 3);
     DBG(2, "cis_wait_bank_change: Missed a bank: got %d [%s], "
            "wanted %d, waited %d msec\n", 
             dev->bank_count, Mustek_PP_1015_show_val(tmp), bankcount, 
             MUSTEK_PP_CIS_WAIT_BANK);
  }

   return dev->bank_count == bankcount ? SANE_TRUE : SANE_FALSE;
}

/******************************************************************************
 * Configure the CIS for a given resolution.
 * 
 * CIS scanners seem to have 2 modes: 
 *
 *   low resolution (50-300 DPI) and 
 *   high resolution (300-600 DPI).
 *
 * Depending on the resolution requested by the user, the scanner is used
 * in high or low resolution mode. In high resolution mode, the motor step
 * sizes are also reduced by a factor of two.
 *
 *****************************************************************************/
static void
cis_set_dpi_value (Mustek_PP_CIS_dev * dev)
{
   u_char val = 0;
  
   if (dev->model == MUSTEK_PP_CIS1200PLUS)
   {
      /* Toshiba CIS: only 600 DPI + decimation */
      switch (dev->CIS.hw_hres)
      {
         case 75:
            val = 0x48; /* 1/8 */
            break;
         case 100:
            val = 0x08; /* 1/6 */
            break;
         case 200:
            val = 0x00; /* 1/3 */
            break;
         case 300:
            val = 0x50; /* 2/4 */
            break;
         case 400:
            val = 0x10; /* 2/3 */
            break;
         case 600:
            val = 0x20; /* 3/3 */
            break;
         default:
            assert (0);
      }      
   }
   else
   {
      /* Canon CIS: sensor can use 300 or 600 DPI */
      switch (dev->CIS.hw_hres)
      {
         case 50:
            val = 0x08; /* 1/6 */
            break;
         case 100:
            val = 0x00; /* 1/3 */
            break;
         case 200:
            val = 0x10; /* 2/3 */
            break;
         case 300:
            val = 0x20; /* 3/3 */
            break;
         case 400:
            val = 0x10; /* 2/3 */
            break;
         case 600:
            val = 0x20; /* 3/3 */
            break;
         default:
            assert (0);
      }
   }
   
   Mustek_PP_1015_write_reg(dev, MA1015W_DPI_CONTROL, val | 0x04);

   DBG (4, "cis_set_dpi_value: dpi: %d -> value 0x%02x\n", dev->CIS.hw_hres, val);
}

static void
cis_set_ccd_channel (Mustek_PP_CIS_dev * dev)
{

   SANE_Byte codes[] = { 0x84, 0x44, 0xC4 };
   SANE_Byte chancode;
   
   assert (dev->CIS.channel < 3);
   
   chancode = codes[dev->CIS.channel];
   
   /* 
      The TWAIN driver sets an extra bit in lineart mode. 
      When I do this too, I don't see any effect on the image. 
      Moreover, for 1 resolution, namely 400 dpi, the bank counter seems 
      to behave strangely, and the synchronization get completely lost.
      I guess the software conversion from gray to lineart is good enough,
      so I'll leave it like that.
      
      if (dev->CIS.setParameters)
      {
         chancode |= (dev->desc->mode == MODE_BW) ? 0x20: 0;
      }
   */

   SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, chancode);
   
#ifdef M1015_TRACE_REGS
   dev->CIS.regs.channel = chancode;
#endif
}

static void
cis_config_ccd (Mustek_PP_CIS_dev * dev)
{
   SANE_Int skipCount, byteCount;
   
   if (dev->CIS.res != 0)
     dev->CIS.hres_step =
       SANE_FIX ((float) dev->CIS.hw_hres / (float) dev->CIS.res);

   /* CIS:  <= 300 dpi -> 0x86
             > 300 dpi -> 0x96 */  

   if (dev->CIS.cisRes == 600)
      SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, 0x96);
   else
      SANEI_PA4S2_WRITEBYTE (dev->desc->fd, 6, 0x86);
   
   cis_set_dpi_value(dev);
   
   if (dev->CIS.setParameters)
   {
      dev->CIS.channel = dev->desc->mode == MODE_COLOR ?
                         MUSTEK_PP_CIS_CHANNEL_RED : MUSTEK_PP_CIS_CHANNEL_GRAY;
   }
   else
   { 
      dev->CIS.channel = MUSTEK_PP_CIS_CHANNEL_GRAY;
   }
   
   cis_set_ccd_channel (dev);
   
   Mustek_PP_1015_write_reg (dev, MA1015W_POWER_ON_DELAY, 0xAA);
   Mustek_PP_1015_write_reg (dev, MA1015W_CCD_TIMING,     0x05); 
   Mustek_PP_1015_write_reg (dev, MA1015W_CCD_TIMING_ADJ, 0x00); 

   Mustek_PP_1015_send_command (dev, 0x45);	/* or 0x05 for no 8kbank */

   /*
    * Unknown sequence.
    * Seems to be always the same during configuration, independent of the
    * mode and the resolution. 
    */
   CIS_CLEAR_FULLFLAG(dev);
   CIS_INC_READ(dev);
   CIS_CLEAR_READ_BANK(dev);
   CIS_CLEAR_WRITE_ADDR(dev);
   CIS_CLEAR_WRITE_BANK(dev);
   CIS_CLEAR_TOGGLE(dev);

   /*
    # SkipImage = expressed in max resolution (600 DPI)
    #
    # Formulas
    #
    #  <= 300 DPI:
    #
    #  Skip = 67 + skipimage/2
    #
    #   Skip1 = Skip / 32
    #   Skip2 = Skip % 32
    #
    #  Bytes = Skip2 * hw_hres/300 + (imagebytes * hw_hres/res) + 2
    #
    #  > 300 DPI
    #
    #  Skip = 67 + skipimage
    #  
    #   Skip1 = Skip / 32
    #   Skip2 = Skip % 32
    #
    #  Bytes = Skip2*hw_hres/600 + (imagebytes * hw_hres/res) + 2
    #
   */
   
   skipCount = 67; /* Hardware parameter - fixed */

   if (dev->CIS.setParameters == SANE_TRUE)
   {
      /*
       * It seems that the TWAIN driver always adds 2 mm extra. When I do the
       * inverse calculation from the parameters that driver sends, I always
       * get a difference of exactly 2mm, at every resolution and for
       * different positions of the scan area. Moreover, when I don't add this
       * offset, the resulting scan seems to start 2mm to soon.
       * I can't find this back in the backend of the TWAIN driver, but I
       * assume that this 2mm offset is taken care off at the higher levels.
       */
      DBG(4, "cis_config_ccd: Skip count: %d\n",  skipCount);
      skipCount += max2cis_hres(dev, dev->CIS.skipimagebytes);
      DBG(4, "cis_config_ccd: Skip count: %d (cis res: %d)\n",  skipCount, 
             dev->CIS.cisRes);
      skipCount += (int)(2.0/25.4*dev->CIS.cisRes);
      DBG(4, "cis_config_ccd: Skip count: %d\n",  skipCount);
      
      Mustek_PP_1015_write_reg (dev, MA1015W_SKIP_COUNT, skipCount / 32);
      DBG(4, "cis_config_ccd: Skip count: %d (x32)\n",  skipCount / 32);
   }
   else
   {
      Mustek_PP_1015_write_reg (dev, MA1015W_SKIP_COUNT, 0);
      DBG(4, "cis_config_ccd: Skip count: 67 (x32)\n");
   }
      
   skipCount %= 32;
   skipCount = cis2max_res(dev, skipCount);  /* Back to max res */
   
   Mustek_PP_1015_write_reg(dev, MA1015W_EXPOSE_TIME, dev->CIS.exposeTime); 
      
   DBG(4, "cis_config_ccd: skipcount: %d imagebytes: %d\n", skipCount, dev->CIS.imagebytes);
   /* set_initial_skip_1015 (dev); */
   if (dev->CIS.setParameters == SANE_TRUE)
   {
      Mustek_PP_1015_write_reg(dev, MA1015W_EXPOSE_TIME, dev->CIS.exposeTime);
      Mustek_PP_1015_write_reg(dev, MA1015W_POWER_ON_DELAY,   0xAA);
      /* The TWAIN drivers always sends the same value: 0x96 */
      Mustek_PP_1015_write_reg3(dev, MA1015W_RED_REF, 0x96, 0x96, 0x96);
      dev->CIS.adjustskip = max2hw_hres(dev, skipCount);
      byteCount = max2hw_hres(dev, skipCount + dev->CIS.imagebytes) + 2; 
      dev->CIS.setParameters = SANE_FALSE;
   }
   else
   {
      dev->CIS.adjustskip = 0;
      byteCount = max2hw_hres(dev, skipCount); 
   }
   DBG(4, "cis_config_ccd: adjust skip: %d bytecount: %d\n", 
          dev->CIS.adjustskip, byteCount);
   
   Mustek_PP_1015_write_reg2(dev, MA1015W_BYTE_COUNT_HB, 
                             byteCount >> 8, byteCount & 0xFF); 
   
   cis_get_bank_count (dev);
   DBG(5, "cis_config_ccd: done\n");
}

static SANE_Bool
cis_wait_motor_stable (Mustek_PP_CIS_dev * dev)
{
   static struct timeval timeoutVal;
   SANE_Bool ret = 
      Mustek_PP_1015_wait_bit (dev, MA1015R_MOTOR, MA1015B_MOTOR_STABLE, 
                               SANE_FALSE, 0);
#ifdef HAVE_SYS_SELECT_H
   if (dev->engine_delay > 0)
   {
      timeoutVal.tv_sec = 0;
      timeoutVal.tv_usec = dev->engine_delay*1000;
      select(0, NULL, NULL, NULL, &timeoutVal);
   }
#endif
   return ret;
}

static void
cis_motor_forward (Mustek_PP_CIS_dev * dev)
{
   SANE_Byte control;
   
   if (dev->model == MUSTEK_PP_CIS600)
   {
      switch (dev->CIS.hw_vres)
      {
         case 150:
            control = 0x7B;
            break;
         case 300:
            control = 0x73;
            break;
         case 600:
            control = 0x13;
            break;
         default:
            exit(1);
      }
   }
   else
   {
      switch (dev->CIS.hw_vres)
      {
         case 300:
            control = 0x7B;
            break;
         case 600:
            control = 0x73;
            break;
         case 1200:
            control = 0x13;
            break;
         default:
            exit(1);
      }
   }

#if MUSTEK_PP_CIS_MOTOR_REVERSE == 1
   control ^= 0x10;
#endif
   
   DBG(4, "cis_motor_forward: @%d dpi: 0x%02X.\n", dev->CIS.hw_vres, control);
   if (!cis_wait_motor_stable (dev))
      return;
   
   Mustek_PP_1015_write_reg(dev, MA1015W_MOTOR_CONTROL, control);
}

static void
cis_move_motor (Mustek_PP_CIS_dev * dev, SANE_Int steps) /* steps @ maxres */
{
   /* Note: steps is expressed at maximum resolution */
   SANE_Byte fullStep = 0x13, biStep = 0x73, quadStep = 0x7B;
   SANE_Int fullSteps, biSteps, quadSteps;
   /*
    * During a multi-step feed, the expose time is fixed. The value depends
    * on the type of the motor (600/1200 CP) 
    */
   SANE_Byte savedExposeTime = dev->CIS.exposeTime;
   dev->CIS.exposeTime = 85;
   
   DBG(4, "cis_move_motor: Moving motor %d steps.\n", steps);
   
   /* Just in case ... */
   if (steps < 0)
   {
      DBG(1, "cis_move_motor: trying to move negative steps: %d\n", steps);
      steps = 0; /* We must go through the configuration procedure */
   }      
   
   /*
    * Using the parameter settings for the 600 CP on a 1200 CP scanner
    * doesn't work: the engine doesn't move and makes a sharp noise, which
    * doesn't sound too healthy. It could be harmful to the motor !
    * Apparently, the same happens on a real 600 CP (reported by Disma
    * Goggia), so it's probably better to always use the 1200 CP settings.
    */
   dev->CIS.exposeTime <<= 1;
   cis_config_ccd(dev);
   dev->CIS.exposeTime = savedExposeTime;
   
   /* 
    * This is a minor speed optimization: when we are using the high
    * resolution mode, long feeds (eg, to move to a scan area at the bottom
    * of the page) can be made almost twice as fast by using double motor
    * steps as much as possible.
    * It is possible, though, that fast skipping (which is the default) is
    * not very accurate on some scanners. Therefore, the user can disable
    * this through the configuration file.
    */  
      
   fullSteps = steps  & 1;
   biSteps = steps >> 1;
   if (dev->fast_skip) {
      quadSteps = biSteps >> 1;
      biSteps &= 1;
   }
   else {
      quadSteps = 0;
   }
   
   M1015_DISPLAY_REGS(dev, "Before move");
   
#if MUSTEK_PP_CIS_MOTOR_REVERSE == 1
   fullStep ^= 0x10;
   biStep ^= 0x10;
   quadStep ^= 0x10;
#endif
   
   DBG(4, "cis_move_motor: 4x%d 2x%d 1x%d\n", quadSteps, biSteps, fullSteps);
   /* Note: the TWAIN driver opens the motor control register only 
      once before the loop, and closes it after the loop. I've tried this
      too, but it resulted in inaccurate skip distances; therefore, the
      motor control register is now opened and closed for each step. */
     
   while (quadSteps-- > 0 && dev->desc->state != STATE_CANCELLED)
   {
      cis_wait_motor_stable (dev);
      Mustek_PP_1015_write_reg(dev, MA1015W_MOTOR_CONTROL, quadStep);
   }
   
   while (biSteps-- > 0 && dev->desc->state != STATE_CANCELLED)
   {
      cis_wait_motor_stable (dev);
      Mustek_PP_1015_write_reg(dev, MA1015W_MOTOR_CONTROL, biStep);
   }
   
   while (fullSteps-- > 0 && dev->desc->state != STATE_CANCELLED)
   {
      cis_wait_motor_stable (dev);
      Mustek_PP_1015_write_reg(dev, MA1015W_MOTOR_CONTROL, fullStep);
   }
}

static void
cis_set_et_pd_sti (Mustek_PP_CIS_dev * dev)
{
   Mustek_PP_1015_write_reg(dev, MA1015W_EXPOSE_TIME, 
                                 dev->CIS.exposeTime);
   Mustek_PP_1015_write_reg(dev, MA1015W_POWER_ON_DELAY, 
                                 dev->CIS.powerOnDelay[dev->CIS.channel]);
   cis_set_ccd_channel (dev);
   cis_set_sti (dev);
}

/*
 * Prepare the scanner for catching the next channel and, if necessary, 
 * move the head one step further.
 */
static SANE_Bool
cis_wait_next_channel (Mustek_PP_CIS_dev * dev)
{
   int moveAtChannel = dev->desc->mode == MODE_COLOR ?
                       MUSTEK_PP_CIS_CHANNEL_BLUE : MUSTEK_PP_CIS_CHANNEL_GRAY;
   
   if (!cis_wait_bank_change (dev, dev->bank_count))
   {
      DBG(2, "cis_wait_next_channel: Could not get next bank.\n");
      return SANE_FALSE;
   }
   
   moveAtChannel = (dev->desc->mode == MODE_COLOR) ?
                    MUSTEK_PP_CIS_CHANNEL_BLUE : MUSTEK_PP_CIS_CHANNEL_GRAY;
      
   if (dev->CIS.channel == moveAtChannel && !dev->CIS.dontMove)
   {
      cis_motor_forward (dev);
   }
   
   cis_set_et_pd_sti (dev);
   
   if (dev->desc->mode == MODE_COLOR)
   {
      ++dev->CIS.channel;
      dev->CIS.channel %= 3;
   }
   
   return SANE_TRUE;
}
   
/*
 * Wait for the device to be ready for scanning. Cycles through the different
 * channels and sets the parameters (only green channel in gray/lineart).
 */
static SANE_Bool
cis_wait_read_ready (Mustek_PP_CIS_dev * dev)
{
   int channel;
   dev->CIS.dontIncRead = SANE_TRUE;
   
   dev->CIS.channel =  dev->desc->mode == MODE_COLOR ?
                       MUSTEK_PP_CIS_CHANNEL_RED : MUSTEK_PP_CIS_CHANNEL_GRAY;
   
   for (channel = 0; channel < 3; ++channel)
   {
      if (!cis_wait_next_channel(dev)) return SANE_FALSE;
   }
   return SANE_TRUE;
}

static int
delay_read (int delay)
{
   /* 
    * A (very) smart compiler may complete optimize the delay loop away. By
    * adding some difficult data dependencies, we can try to prevent this. 
    */
   static int prevent_removal, i;
   for (i = 0; i<delay; ++i)
   {
      prevent_removal = sqrt(prevent_removal+1.); /* Just waste some cycles */
   }
   return prevent_removal; /* another data dependency */
}

/*
** Reads one line of pixels
*/
static void
cis_read_line_low_level (Mustek_PP_CIS_dev * dev, SANE_Byte * buf, 
                         SANE_Int pixel, SANE_Byte * calib_low, 
                         SANE_Byte * calib_hi, SANE_Int * gamma)
{
   u_char color;
   int ctr, skips = dev->CIS.adjustskip, cval;
   int bpos = 0;
   SANE_Byte low_val = 0, hi_val = 255;

   if (pixel <= 0)
      return;
  
   SANEI_PA4S2_READBEGIN (dev->desc->fd, 1);

   while(skips-- >= 0)
   {
      if (dev->CIS.delay) delay_read(dev->CIS.delay);
      SANEI_PA4S2_READBYTE (dev->desc->fd, &color);
   }

   if (dev->CIS.hw_hres == dev->CIS.res)
   {
      /* One-to one mapping */
      DBG (6, "cis_read_line_low_level: one-to-one\n");
      for (ctr = 0; ctr < pixel; ctr++)
      {
         if (dev->CIS.delay) delay_read(dev->CIS.delay);
         SANEI_PA4S2_READBYTE (dev->desc->fd, &color);

	 cval = color;

         if (calib_low) {
           low_val = calib_low[ctr] ;
         }

         if (calib_hi) {
           hi_val = calib_hi[ctr] ;
         }

         cval  -= low_val ;
         cval <<= 8 ;
         cval  /= hi_val-low_val ;

         if (cval < 0)         cval = 0;
         else if (cval > 255)  cval = 255;

         if (gamma)
	    cval = gamma[cval];

	 buf[ctr] = cval;
      }
   }
   else if (dev->CIS.hw_hres > dev->CIS.res)
   {
      /* Sub-sampling */
      
      int pos = 0;
      DBG (6, "cis_read_line_low_level: sub-sampling\n");
      ctr = 0;
      do
      {
         if (dev->CIS.delay) delay_read(dev->CIS.delay);
         SANEI_PA4S2_READBYTE (dev->desc->fd, &color);

         cval = color;
         if (ctr < (pos >> SANE_FIXED_SCALE_SHIFT))
	 {
	    ctr++;
	    continue;
	 }

         ctr++;
         pos += dev->CIS.hres_step;

         if (calib_low) {
           low_val = calib_low[bpos] ;
         }

         if (calib_hi) {
           hi_val = calib_hi[bpos] ;
         }
         cval  -= low_val ;
         cval <<= 8 ;
         cval  /= hi_val-low_val ;

         if (cval < 0)         cval = 0 ;
         else if (cval > 255)  cval = 255 ;

         if (gamma) cval = gamma[cval];

         buf[bpos++] = cval;
      }
      while (bpos < pixel);
   }
   else
   {
      int calctr = 0;
      SANE_Int pos = 0, nextPos = 1;
      /* Step: eg: 600 DPI -> 700 DPI -> hres_step = 6/7 -> step = 1/7 */
      SANE_Int step = SANE_FIX(1) - dev->CIS.hres_step; 
      
      /* Super-sampling */
      DBG (6, "cis_read_line_low_level: super-sampling\n");
      do
      {
         if (dev->CIS.delay) delay_read(dev->CIS.delay);
         SANEI_PA4S2_READBYTE (dev->desc->fd, &color);

	 cval = color;

         if (calib_low) {
           low_val = calib_low[calctr] ;
         }

         if (calib_hi) {
           hi_val = calib_hi[calctr] ;
         }
         
         if (++calctr >= dev->calib_pixels) {
            /* Avoid array boundary violations due to rounding errors 
               (due to the incremental calculation, the current position
               may be inaccurate to up to two pixels, so we may need to 
               read a few extra bytes -> use the last calibration value) */
            calctr = dev->calib_pixels - 1;
            DBG (3, "cis_read_line_low_level: calibration overshoot\n");
         }

         cval  -= low_val ;
         cval <<= 8 ;
         cval  /= hi_val-low_val ;

         if (cval < 0)         cval = 0 ;
         else if (cval > 255)  cval = 255 ;

         if (gamma)
	    cval = gamma[cval];

         pos += step;
         
         if ((pos >> SANE_FIXED_SCALE_SHIFT) >= nextPos)
         {
            nextPos++;
            
            /* Insert an interpolated value */
            buf[bpos] = (buf[bpos-1] + cval)/2; /* Interpolate */
            ++bpos;
            
            /* Store the plain value, but only if we still need pixels */
            if (bpos < pixel)
               buf[bpos++] = cval;
            
            pos += step; /* Take interpolated value into account for pos */
         }
         else
         {
  	    buf[bpos++] = cval;
         }
      }
      while (bpos < pixel);
   }
   
   SANEI_PA4S2_READEND (dev->desc->fd);
   DBG (6, "cis_read_line_low_level: done\n");
}

static SANE_Bool
cis_read_line (Mustek_PP_CIS_dev * dev, SANE_Byte* buf, SANE_Int pixel,
               SANE_Bool raw)
{
   if (!dev->CIS.dontIncRead)
      CIS_INC_READ(dev);
   else
      dev->CIS.dontIncRead = SANE_FALSE;
   
   
   if (raw)
   {
      /* No color correction; raw data */
      cis_read_line_low_level (dev, buf, pixel, NULL, NULL, NULL);
   }
   else
   {
      /* Color correction */
      cis_read_line_low_level (dev, buf, pixel,
                               dev->calib_low[dev->CIS.channel], 
                               dev->calib_hi[dev->CIS.channel], 
                               (dev->desc->val[OPT_CUSTOM_GAMMA].w ? 
                               dev->desc->gamma_table[dev->CIS.channel] : NULL));
   }
   
   return cis_wait_next_channel(dev);
}

static void
cis_get_next_line (Mustek_PP_CIS_dev * dev, SANE_Byte * buf)
{
   SANE_Byte *dest, *tmpbuf = dev->tmpbuf;
   int ctr, channel, first, last, stride, ignore, step = dev->CIS.line_step;
   SANE_Byte gotline;
   
   if (dev->desc->mode == MODE_COLOR)
   {
      first = MUSTEK_PP_CIS_CHANNEL_RED;
      last = MUSTEK_PP_CIS_CHANNEL_BLUE;
      stride = 3;
      ignore = 1; /* 1 * 3 channels */
   }
   else
   {
      first = MUSTEK_PP_CIS_CHANNEL_GRAY;
      last = MUSTEK_PP_CIS_CHANNEL_GRAY;
      stride = 1;
      ignore = 3; /* 3 * 1 channel */
   }
   
   gotline = SANE_FALSE;
   do
   {
      dev->ccd_line++;
      if ((dev->line_diff >> SANE_FIXED_SCALE_SHIFT) != dev->ccd_line)
      {
         cis_motor_forward (dev);
         continue;
      }

      dev->line_diff += step;

      for (channel = first; channel <= last; ++channel)
      {
         if (!cis_read_line(dev, tmpbuf, dev->desc->params.pixels_per_line, 
                            SANE_FALSE)) 
            return;

         dest = buf + channel - first;
         for (ctr = 0; ctr < dev->desc->params.pixels_per_line; ctr++)
         {
	    *dest = tmpbuf[ctr];
            dest += stride;
         }
      }
      gotline = SANE_TRUE;
   }
   while (!gotline && dev->desc->state != STATE_CANCELLED);
}

static void
cis_get_grayscale_line (Mustek_PP_CIS_dev * dev, SANE_Byte * buf)
{
   cis_get_next_line(dev, buf);
}

static void
cis_get_lineart_line (Mustek_PP_CIS_dev * dev, SANE_Byte * buf)
{
   int ctr;
   SANE_Byte gbuf[MUSTEK_PP_CIS_MAX_H_PIXEL * 2];

   cis_get_grayscale_line (dev, gbuf);
   memset (buf, 0xFF, dev->desc->params.bytes_per_line);

   for (ctr = 0; ctr < dev->desc->params.pixels_per_line; ctr++)
      buf[ctr >> 3] ^= ((gbuf[ctr] > dev->bw_limit) ? (1 << (7 - ctr % 8)) : 0);
}

static void
cis_get_color_line (Mustek_PP_CIS_dev * dev, SANE_Byte * buf)
{
   cis_get_next_line(dev, buf);
}


/******************************************************************************
 * Saves the state of the device during reset and calibration.
 *****************************************************************************/
static void 
cis_save_state (Mustek_PP_CIS_dev * dev)
{
   dev->Saved_CIS = dev->CIS;
}

/******************************************************************************
 * Restores the state of the device after reset and calibration.
 *****************************************************************************/
static void 
cis_restore_state (Mustek_PP_CIS_dev * dev)
{
   dev->CIS = dev->Saved_CIS;
}

#define CIS_TOO_BRIGHT 1
#define CIS_OK         0
#define CIS_TOO_DARK  -1

static int
cis_check_result(SANE_Byte* buffer, int pixel)
{
   int i, maxVal = 0;
   
   for (i=0;i<pixel;++i) 
      if (buffer[i] > maxVal) maxVal = buffer[i];
   
   if (maxVal > 250) return CIS_TOO_BRIGHT;
   if (maxVal < 240) return CIS_TOO_DARK;
   return CIS_OK;
}

static SANE_Bool
cis_maximize_dynamic_range(Mustek_PP_CIS_dev * dev)
{
   /* The device is in its final configuration already. */
   int i, j, pixel, channel, minExposeTime, first, last;
   SANE_Byte powerOnDelayLower[3], powerOnDelayUpper[3], exposeTime[3];
   SANE_Byte buf[3][MUSTEK_PP_CIS_MAX_H_PIXEL];
   SANE_Int pixels = dev->calib_pixels;
   
   DBG(3, "cis_maximize_dynamic_range: starting\n");
   
   for (channel = 0; channel < 3; ++channel)
   {
      exposeTime[channel] = 254;
      dev->CIS.powerOnDelay[channel] = 170;
      powerOnDelayLower[channel] = 1;
      powerOnDelayUpper[channel] = 254;
   }         
   dev->CIS.setParameters  = SANE_TRUE;
   dev->CIS.exposeTime     = exposeTime[MUSTEK_PP_CIS_CHANNEL_GREEN];
   cis_config_ccd(dev);
   
   M1015_DISPLAY_REGS(dev, "before maximizing dynamic range");
   dev->CIS.dontMove = SANE_TRUE; /* Don't move while calibrating */
   
   if (!cis_wait_read_ready(dev) && dev->desc->state != STATE_CANCELLED)
   {
      DBG(2, "cis_maximize_dynamic_range: DEVICE NOT READY!\n");
      return SANE_FALSE;
   }
   
   if (dev->desc->mode == MODE_COLOR)
   {
      first = MUSTEK_PP_CIS_CHANNEL_RED;
      last  = MUSTEK_PP_CIS_CHANNEL_BLUE;
   }
   else
   {
      first = MUSTEK_PP_CIS_CHANNEL_GRAY;
      last  = MUSTEK_PP_CIS_CHANNEL_GRAY;
   }
   
   dev->CIS.channel = first;
   
   /* Perform a kind of binary search. In the worst case, we should find 
      the optimal power delay values after 8 iterations */
   for( i=0; i<8; i++)
   {
      for (channel = first; channel <= last; ++channel)
      {
         dev->CIS.powerOnDelay[channel] = (powerOnDelayLower[channel] +
                                           powerOnDelayUpper[channel]) / 2;
      }
      Mustek_PP_1015_write_reg(dev, MA1015W_POWER_ON_DELAY, 
                               dev->CIS.powerOnDelay[1]); /* Green */

      for (pixel = 0; pixel < pixels; ++pixel)
      {
         buf[0][pixel] = buf[1][pixel] = buf[2][pixel] = 255;
      }
      
      /* Scan 4 lines, but ignore the first 3 ones. */
      for (j = 0; j < 4; ++j)
      {
         for (channel = first; channel <= last; ++channel)
         {
            if (!cis_read_line(dev, &buf[channel][0], pixels, 
                               /* raw = */ SANE_TRUE))
               return SANE_FALSE;
         }
      }

      for (channel = first; channel <= last; ++channel)
      {
         switch (cis_check_result(buf[channel], pixels))
         {
            case CIS_TOO_BRIGHT:
               powerOnDelayLower[channel] = dev->CIS.powerOnDelay[channel];
               break;   
               
            case CIS_TOO_DARK:
               powerOnDelayUpper[channel] = dev->CIS.powerOnDelay[channel];
               break;   
               
            default:
               break;   
         }
      } 
      DBG (4, "cis_maximize_dynamic_range: power on delay %3d %3d %3d\n", 
           dev->CIS.powerOnDelay[0], dev->CIS.powerOnDelay[1], 
           dev->CIS.powerOnDelay[2]);
   }
   dev->CIS.dontMove = SANE_FALSE;
   
   DBG (3, "cis_maximize_dynamic_range: power on delay %3d %3d %3d\n", 
           dev->CIS.powerOnDelay[0], dev->CIS.powerOnDelay[1], 
           dev->CIS.powerOnDelay[2]);
           
   minExposeTime = (dev->CIS.hw_hres <= 300) ? 170 : 253;
   
   for (channel = first; channel <= last; ++channel)
   {
      dev->CIS.powerOnDelay[channel] = (powerOnDelayLower[channel] +
                                        powerOnDelayUpper[channel]) / 2;
      exposeTime[channel] -= dev->CIS.powerOnDelay[channel] - 1;
      dev->CIS.powerOnDelay[channel] = 1;
      
      if (exposeTime[channel] < minExposeTime)
      {
         dev->CIS.powerOnDelay[channel] += 
            minExposeTime - exposeTime[channel];
         exposeTime[channel] = minExposeTime;
      }
   }                                      
   
   dev->CIS.exposeTime = exposeTime[MUSTEK_PP_CIS_CHANNEL_GREEN];
   
   DBG (3, "cis_maximize_dynamic_range: expose time: %3d\n", exposeTime[1]);
   DBG (3, "cis_maximize_dynamic_range: power on delay %3d %3d %3d\n", 
           dev->CIS.powerOnDelay[0], dev->CIS.powerOnDelay[1], 
           dev->CIS.powerOnDelay[2]);
   
   /*
    * Short the calibration. Temporary, to find out what is wrong with
    * the calibration on a 600 CP.
    *
   dev->CIS.exposeTime = 170;
   dev->CIS.powerOnDelay[0] = 120;
   dev->CIS.powerOnDelay[1] = 120;
   dev->CIS.powerOnDelay[2] = 120;
   */
   return SANE_TRUE;
}

static SANE_Bool
cis_measure_extremes(Mustek_PP_CIS_dev * dev, SANE_Byte* calib[3],
		     SANE_Int pixels, SANE_Int first, SANE_Int last)
{
   SANE_Byte buf[3][MUSTEK_PP_CIS_MAX_H_PIXEL];
   SANE_Byte min[3][MUSTEK_PP_CIS_MAX_H_PIXEL];
   SANE_Byte max[3][MUSTEK_PP_CIS_MAX_H_PIXEL];
   SANE_Int  sum[3][MUSTEK_PP_CIS_MAX_H_PIXEL];
   int channel, cnt, p;
   
   memset((void*)&min, 255, 3*MUSTEK_PP_CIS_MAX_H_PIXEL*sizeof(SANE_Byte));
   memset((void*)&max,   0, 3*MUSTEK_PP_CIS_MAX_H_PIXEL*sizeof(SANE_Byte));
   memset((void*)&sum,   0, 3*MUSTEK_PP_CIS_MAX_H_PIXEL*sizeof(SANE_Int));
   
   dev->CIS.channel = first;
   
   /* Purge the banks first (there's always a 3-cycle delay) */
   for (channel = first; channel <= last; ++channel)
   {
      if (!cis_read_line(dev, &buf[channel%3][0], pixels, 
                         /* raw = */ SANE_TRUE))
         return SANE_FALSE;
   }
   --dev->CIS.skipsToOrigin;
   
   for (cnt = 0; cnt < MUSTEK_PP_CIS_AVERAGE_COUNT + 2; ++cnt)
   {
      for (channel = first; channel <= last; ++channel)
      {
         DBG(4, "cis_measure_extremes: Reading line %d - channel %d\n", 
                cnt, channel);
         if (!cis_read_line(dev, &buf[channel][0], pixels, 
                            /* raw = */ SANE_TRUE))
            return SANE_FALSE;

         for (p = 0; p < pixels; ++p)
         {
            SANE_Byte val = buf[channel][p];
            if (val < min[channel][p]) min[channel][p] = val;
            if (val > max[channel][p]) max[channel][p] = val;
            sum[channel][p] += val;
         }
      }
      --dev->CIS.skipsToOrigin;
   }
   DBG(4, "cis_measure_extremes: Averaging\n");
   for (channel = first; channel <= last; ++channel)      
   {
      /* Ignore the extreme values and take the average of the others. */
      for (p = 0; p < pixels; ++p)
      {
         sum[channel][p] -= min[channel][p] + max[channel][p];
         sum[channel][p] /= MUSTEK_PP_CIS_AVERAGE_COUNT;
         if (calib[channel]) calib[channel][p] = sum[channel][p];
      }
   }
   DBG(4, "cis_measure_extremes: Done\n");
   return SANE_TRUE;
}   

static SANE_Bool
cis_normalize_ranges(Mustek_PP_CIS_dev * dev)
{
   SANE_Byte cal_low, cal_hi ;
   SANE_Byte powerOnDelay[3] ;
   SANE_Int pixels = dev->calib_pixels;
   SANE_Int channel, p, first, last;
   
   if (dev->desc->mode == MODE_COLOR)
   {
      first = MUSTEK_PP_CIS_CHANNEL_RED;
      last  = MUSTEK_PP_CIS_CHANNEL_BLUE;
   }
   else
   {
      first = MUSTEK_PP_CIS_CHANNEL_GRAY;
      last  = MUSTEK_PP_CIS_CHANNEL_GRAY;
   }
   
   DBG(3, "cis_normalize_ranges: Measuring high extremes\n");
   /* Measure extremes with normal lighting */
   if (!cis_measure_extremes(dev, dev->calib_hi, pixels, first, last)) {
      return SANE_FALSE;
   }

   /* Measure extremes without lighting */
   for (channel=first; channel<=last; ++channel) {
      powerOnDelay[channel] = dev->CIS.powerOnDelay[channel];
      dev->CIS.powerOnDelay[channel] = dev->CIS.exposeTime;
   }
   
   DBG(3, "cis_normalize_ranges: Measuring low extremes\n");
   if (!cis_measure_extremes(dev, dev->calib_low, pixels, first, last)) {
      return SANE_FALSE;
   }
   
   /* Restore settings */
   for (channel=first; channel<=last; ++channel) {
      dev->CIS.powerOnDelay[channel] = powerOnDelay[channel];
   }
   
   /* Make sure calib_hi is greater than calib_low */
   for (channel = first; channel <= last; ++channel) {
     for (p = 0; p<pixels; p++) {
       if (dev->calib_low[channel]) {
         cal_low = dev->calib_low[channel][p];
       } else {
         cal_low = 0;
       }
       if (dev->calib_hi[channel]) {
         cal_hi = dev->calib_hi[channel][p];
       } else {
         cal_hi = 255;
       }
       if (cal_hi <= cal_low) {
         if(cal_hi<255) {
           /* calib_hi exists, else cal_hi would be 255 */
           dev->calib_hi[channel][p] = cal_low+1;
         } else {
           /* calib_low exists, else cal_low would be 0, < 255  */
           dev->calib_low[channel][p] = cal_hi-1;
         }
       }
     }
   }
   DBG(3, "cis_normalize_ranges: calibration done\n");
   return SANE_TRUE;
}   

/*
 * This routine measures the time that we have to wait between reading 
 * to pixels from the scanner. Especially at low resolutions, but also
 * for narrow-width scans at high resolutions, reading too fast cause
 * color stability problems. 
 * This routine sends a test pattern to the scanner memory banks and tries
 * to measure how fast it can be retrieved without errors. 
 * The same is done by the TWAIN driver (TESTIO.CPP:TestDelay). 
 */
static SANE_Bool
cis_measure_delay(Mustek_PP_CIS_dev * dev)
{
   SANE_Byte buf[2][2048];
   unsigned i, j, d;
   int saved_res;
   SANE_Bool error = SANE_FALSE;
   
   CIS_CLEAR_FULLFLAG(dev);
   CIS_CLEAR_WRITE_ADDR(dev);
   CIS_CLEAR_WRITE_BANK(dev);
   CIS_INC_READ(dev);
   CIS_CLEAR_READ_BANK(dev);

   M1015_DISPLAY_REGS(dev, "Before delay measurement");
   assert(dev->CIS.adjustskip == 0);
   
   /* Sawtooth */
   for (i=0; i<2048; ++i)
   {
      buf[0][i] = i % 255; /* Why 255 ? Seems to have no real importance */
   }
   
   Mustek_PP_1015_write_reg_start(dev, MA1015W_SRAM_SOURCE_PC);
   for (i=0; i<2048; ++i)
   {
      Mustek_PP_1015_write_reg_val(dev, buf[0][i]);
   }
   Mustek_PP_1015_write_reg_stop(dev);
   
   /* Bank offset measurement */
   dev->CIS.delay = 0; /* Initialize to zero, measure next */
   
   saved_res = dev->CIS.res;
   dev->CIS.res = dev->CIS.hw_hres;
   
   /*
    * Note: the TWAIN driver seems to have a fast EPP mode too. That one is
    * tried first, and then they try the normal mode. I haven't figured out
    * yet how the fast mode works, so I'll only check the normal mode for now.
    * Moreover, from the behaviour that I've witnessed from the TWAIN driver,
    * I must conclude that the fast mode probably doesn't work on my computer,
    * so I can't test it anyhow.
    */
   /* Gradually increase the delay till we have no more errors */
   for (d = 0; d < 75 /* 255 */  && dev->desc->state != STATE_CANCELLED; d += 5)
   {
      dev->CIS.delay = d;
      
      /*
       * We read the line 5 times to make sure that all garbage is flushed.
       */
      for (i=0; i<5; ++i)
      {
         CIS_INC_READ(dev);
         CIS_CLEAR_READ_BANK(dev);
         cis_read_line_low_level (dev, &buf[1][0], 2048, NULL, NULL, NULL);
         if (dev->desc->state == STATE_CANCELLED) return SANE_FALSE;
      }
      
      error = SANE_FALSE;
      /* Check 100 times whether we can read without errors. */
      for (i=0; i<100 && !error; ++i)
      {
         CIS_INC_READ(dev);
         CIS_CLEAR_READ_BANK(dev);
         cis_read_line_low_level (dev, &buf[1][0], 2048, NULL, NULL, NULL);
         if (dev->desc->state == STATE_CANCELLED) return SANE_FALSE;
         
         for (j=0; j<2048; ++j)
         {
            if (buf[0][j] != buf[1][j]) 
            {
               error = SANE_TRUE;
               break;
            }
         }
      }
      
      DBG (3, "cis_measure_delay: delay %d\n", dev->CIS.delay); 
      if (!error)
         break;
   }
   
   dev->CIS.res = saved_res;
   
   if (error)
   {
      fprintf(stderr, "mustek_pp_cis: failed to measure delay.\n");
      fprintf(stderr, "Buffer contents:\n");
      for (j = 0; j < 20; ++j)
      {
         fprintf(stderr, "%d ", buf[1][j]);
      }
      fprintf(stderr, "\n");
      dev->CIS.delay = 0; 
   }
   
   DBG (3, "cis_measure_delay: delay %d\n", dev->CIS.delay); 
   return SANE_TRUE;
}

static void
cis_motor_control (Mustek_PP_CIS_dev * dev, u_char control)
{
   cis_wait_motor_stable (dev);
   Mustek_PP_1015_write_reg(dev, MA1015W_MOTOR_CONTROL, control);
}

static void
cis_return_home (Mustek_PP_CIS_dev * dev, SANE_Bool nowait)
{
   SANE_Byte savedExposeTime = dev->CIS.exposeTime;
   DBG(4, "cis_return_home: returning home; nowait: %d\n", nowait);
   /* During a return-home, the expose time is fixed. */
   dev->CIS.exposeTime = 170;
   cis_config_ccd(dev);
   dev->CIS.exposeTime = savedExposeTime;
   
   cis_motor_control (dev, 0xEB);

   if (nowait == SANE_FALSE)
      Mustek_PP_1015_wait_bit(dev, MA1015R_MOTOR, MA1015B_MOTOR_HOME, 
                              SANE_TRUE, 1000);
}

/******************************************************************************
 * Does a full reset of the device, ie. configures the CIS to a default
 * resolution of 300 DPI (in high or low resolution mode, depending on the
 * resolution requested by the user).
 *****************************************************************************/
static void
cis_reset_device (Mustek_PP_CIS_dev * dev)
{
   DBG(4, "cis_reset_device: resetting device\n");
   dev->CIS.adjustskip         = 0;
   dev->CIS.dontIncRead        = SANE_TRUE;
   dev->CIS.dontMove           = SANE_FALSE;
   
   cis_save_state(dev);

   dev->CIS.hw_hres            = 300;
   dev->CIS.channel            = MUSTEK_PP_CIS_CHANNEL_GREEN;
   dev->CIS.setParameters      = SANE_FALSE;
   dev->CIS.exposeTime         = 0xAA;
   
   cis_config_ccd (dev);
   
   cis_restore_state(dev);
   
}

static SANE_Bool
cis_calibrate (Mustek_PP_CIS_dev * dev)
{
   int i, saved_res = dev->CIS.res, saved_vres = dev->CIS.hw_vres;
   
   /*
    * Flow of operation observed from the twain driver
    * (it is assumed that the lamp is at the origin, and that the CIS is
    * configured for 300 DPI, ie. cis_reset_device has been called.)
    *
    *  - Reset the device and return the lamp to its home position
    *
    *  - Unknown short sequence
    *
    *  - Send a sawtooth-like pattern to one of the memory banks.
    *
    *  - Repetitive read_line of 2048 bytes, interleaved with an unknown
    *    command. The number varies between 102 and 170 times, but there
    *    doesn't seem to be any correlation with the current mode of the
    *    scanner, so I assume that the exact number isn't really relevant.
    *    The values that are read are the one that were sent to the bank,
    *    rotated by 1 byte in my case. 
    *
    *
    *    It seems that the width of the black border is being measured at 
    *    this stage, possibly multiple times till it stabilizes. 
    *    I assume that the buffer is read 100 times to allow the lamp to
    *    warm up and that the width of the black border is then being
    *    measured till it stabilizes. That would explain the minimum number
    *    of 102 iterations that I've seen.
    *
    *  - reset the device
    *
    *  - move the motor 110 steps forward. The TWAIN driver moves 90 steps,
    *    and I've used 90 steps for a long time too, but occasionally, 
    *    90 steps is a fraction to short to reach the start of the 
    *    calibration strip (the motor movements are not very accurate;
    *    an offset of 1 mm is not unusual). Therefore, I've increased it to
    *    110 steps. This gives us an additional 1.6 mm slack, which should
    *    prevent calibration errors. 
    *    (Note that the MUSTEK_PP_CIS_????CP_DEFAULT_SKIP constants have to 
    *    be adjusted if the number of steps is altered.)
    *
    *  - configure the CIS : actual resolution + set parameters
    * 
    */
   
   /* 
    * We must make sure that we are in the scanning state; otherwise we may
    * still be in the canceled state from a previous scan (even if terminated
    * normally), and the whole calibration would go wrong. 
    */ 
   dev->desc->state = STATE_SCANNING;
    
   cis_reset_device (dev);
   cis_return_home (dev, SANE_FALSE);  /* Wait till it's home */
   
   /* Use maximum resolution during calibration; otherwise we may calibrate
      past the calibration strip. */
   dev->CIS.hw_vres = dev->desc->dev->maxres;
   /* This field remembers how many steps we still have to go @ max res */
   dev->CIS.skipsToOrigin = dev->top_skip; /*max2hw_vres(dev, dev->top_skip); */
   
   if (!cis_measure_delay(dev))
      return SANE_FALSE;
   
   cis_reset_device (dev);

   /* Move motor 110 steps @ 300 DPI */
   Mustek_PP_1015_write_reg_start(dev, MA1015W_MOTOR_CONTROL);
   for (i=0; i<110; ++i)
   {
      if (dev->model == MUSTEK_PP_CIS600)
      {
         Mustek_PP_1015_write_reg_val (dev, 0x73);
      }
      else
      {
         Mustek_PP_1015_write_reg_val (dev, 0x7B);
      }
      cis_wait_motor_stable (dev);
   }
   Mustek_PP_1015_write_reg_stop(dev);
   
   /* Next, we maximize the dynamic range of the scanner. During calibration
      we don't want to extrapolate, so we limit the resolution if necessary */
   
   if (dev->CIS.hw_hres < dev->CIS.res) 
      dev->CIS.res = dev->CIS.hw_hres;
   
   if (!cis_maximize_dynamic_range(dev))
      return SANE_FALSE;
   
   if (!cis_normalize_ranges(dev))
      return SANE_FALSE;
   
   dev->CIS.res = saved_res;
   dev->CIS.hw_vres = saved_vres;
   
   /* Convert steps back to max res size, which are used during skipping */
/*   dev->CIS.skipsToOrigin = hw2max_vres(dev, dev->CIS.skipsToOrigin); */
   
   /* Move to the origin */
   DBG(3, "cis_calibrate: remaining skips to origin @maxres: %d\n", 
          dev->CIS.skipsToOrigin);
   cis_move_motor(dev, dev->CIS.skipsToOrigin);
   
   if (dev->calib_mode)
   {
      /* In calibration mode, we scan the interior of the scanner before the
         glass plate in order to find the position of the calibration strip
         and the start of the glass plate. */
      DBG(3, "cis_calibrate: running in calibration mode. Returning home.\n");
      cis_return_home (dev, SANE_FALSE);  /* Wait till it's home */
   }
   
   return dev->desc->state != STATE_CANCELLED ? SANE_TRUE : SANE_FALSE;
   
}  

/******************************************************************************
 ******************************************************************************
 ***                           Mustek PP interface                          ***
 ******************************************************************************
 *****************************************************************************/

/******************************************************************************
* Init                                                                        *
******************************************************************************/

/* Shared initialization routine */
static SANE_Status cis_attach(SANE_String_Const port,
                              SANE_String_Const name, 
                              SANE_Attach_Callback attach,
                              SANE_Int driverNo,
                              SANE_Int info)
{
   int fd;
   SANE_Status status;
   u_char asic;

   status = sanei_pa4s2_open (port, &fd);

   if (status != SANE_STATUS_GOOD)
   {
      SANE_Status altStatus;
      SANE_String_Const altPort;
      
      DBG (2, "cis_attach: couldn't attach to `%s' (%s)\n", port,
           sane_strstatus (status));
      
      /* Make migration to libieee1284 painless for users that used 
         direct io in the past */
           if (strcmp(port, "0x378") == 0) altPort = "parport0";
      else if (strcmp(port, "0x278") == 0) altPort = "parport1";
      else if (strcmp(port, "0x3BC") == 0) altPort = "parport2";
      else return status;
      
      DBG (2, "cis_attach: trying alternative port name: %s\n", altPort);
      
      altStatus = sanei_pa4s2_open (altPort, &fd);
      if (altStatus != SANE_STATUS_GOOD)
      {
           DBG (2, "cis_attach: couldn't attach to alternative port `%s' "
              "(%s)\n", altPort, sane_strstatus (altStatus));
           return status; /* Return original status, not alternative status */
      }
   }
   
   M1015_START_LL;
   M1015_START_HL;


   sanei_pa4s2_enable (fd, SANE_TRUE);
   SANEI_PA4S2_READBEGIN (fd, 0);
   SANEI_PA4S2_READBYTE (fd, &asic);
   SANEI_PA4S2_READEND (fd);
   sanei_pa4s2_enable (fd, SANE_FALSE);

   sanei_pa4s2_close (fd);
   
   if (asic != 0xA5) /* Identifies the MA1015 chipset */
   {
      /* CIS driver only works for MA1015 chipset */
      DBG (2, "cis_attach: asic id (0x%02x) not recognized\n", asic);
      return SANE_STATUS_INVAL; 
   }

   DBG (3, "cis_attach: device %s attached\n", name);
   DBG (3, "cis_attach: asic 0x%02x\n", asic);
   
   return attach(port, name, driverNo, info);
}

SANE_Status cis600_drv_init(SANE_Int options, SANE_String_Const port,
		SANE_String_Const name, SANE_Attach_Callback attach)
{
   if (options != CAP_NOTHING)
      return SANE_STATUS_INVAL;

   return cis_attach(port, name, attach, MUSTEK_PP_CIS600, MUSTEK_PP_CIS600);
}

SANE_Status cis1200_drv_init(SANE_Int options, SANE_String_Const port,
		SANE_String_Const name, SANE_Attach_Callback attach)
{
   if (options != CAP_NOTHING)
      return SANE_STATUS_INVAL;

   return cis_attach(port, name, attach, MUSTEK_PP_CIS1200, MUSTEK_PP_CIS1200);
}

SANE_Status cis1200p_drv_init(SANE_Int options, SANE_String_Const port,
		SANE_String_Const name, SANE_Attach_Callback attach)
{
   if (options != CAP_NOTHING)
      return SANE_STATUS_INVAL;

   return cis_attach(port, name, attach, MUSTEK_PP_CIS1200PLUS, MUSTEK_PP_CIS1200PLUS);
}

/******************************************************************************
* Capabilities                                                                *
******************************************************************************/
void cis_drv_capabilities(SANE_Int info, SANE_String *model,
                          SANE_String *vendor, SANE_String *type,
                          SANE_Int *maxres, SANE_Int *minres,
                          SANE_Int *maxhsize, SANE_Int *maxvsize,
                          SANE_Int *caps)
{
   *vendor = strdup("Mustek");
   *type = strdup("flatbed scanner");
   *caps = CAP_NOTHING;

   switch(info)
   {   
      case MUSTEK_PP_CIS600:
        *model = strdup("600CP");
        *maxres = 600;
        *minres = 50;
        *maxhsize = MUSTEK_PP_CIS_MAX_H_PIXEL;
        *maxvsize = MUSTEK_PP_CIS_MAX_V_PIXEL;
        break;
      case MUSTEK_PP_CIS1200:
        *model = strdup("1200CP");
        *maxres = 1200;
        *minres = 50;
        *maxhsize = MUSTEK_PP_CIS_MAX_H_PIXEL*2;
        *maxvsize = MUSTEK_PP_CIS_MAX_V_PIXEL*2;
        break;
      case MUSTEK_PP_CIS1200PLUS:
        *model = strdup("1200CP+");
        *maxres = 1200;
        *minres = 50;
        *maxhsize = MUSTEK_PP_CIS_MAX_H_PIXEL*2;
        *maxvsize = MUSTEK_PP_CIS_MAX_V_PIXEL*2;
        break;
   }
}

/******************************************************************************
* Open                                                                        *
******************************************************************************/
SANE_Status cis_drv_open (SANE_String port, SANE_Int caps, SANE_Int *fd)
{
   SANE_Status status;

   if (caps != CAP_NOTHING)
   {
      DBG (1, "cis_drv_open: called with unknown capabilities (0x%02X)\n", caps);
      return SANE_STATUS_INVAL;
   }

   DBG (3, "cis_drv_open: called for port %s\n", port);

   status = sanei_pa4s2_open (port, fd);

   if (status != SANE_STATUS_GOOD)
   {
      SANE_Status altStatus;
      SANE_String_Const altPort;
      
      DBG (2, "cis_attach: couldn't attach to `%s' (%s)\n", port,
           sane_strstatus (status));
      
      /* Make migration to libieee1284 painless for users that used 
         direct io in the past */
           if (strcmp(port, "0x378") == 0) altPort = "parport0";
      else if (strcmp(port, "0x278") == 0) altPort = "parport1";
      else if (strcmp(port, "0x3BC") == 0) altPort = "parport2";
      else return status;
      
      DBG (2, "cis_attach: trying alternative port name: %s\n", altPort);
      
      altStatus = sanei_pa4s2_open (altPort, fd);
      if (altStatus != SANE_STATUS_GOOD)
      {
           DBG (2, "cis_attach: couldn't attach to alternative port `%s' "
              "(%s)\n", altPort, sane_strstatus (altStatus));
           return status; /* Return original status, not alternative status */
      }
   }

   return SANE_STATUS_GOOD;
}

/******************************************************************************
* Setup                                                                       *
******************************************************************************/
void cis_drv_setup (SANE_Handle hndl)
{
   Mustek_pp_Handle *dev = hndl;
   Mustek_PP_CIS_dev *cisdev;
   cisdev = (Mustek_PP_CIS_dev*)malloc(sizeof(Mustek_PP_CIS_dev));
   if (cisdev == NULL)
   {
      DBG (2, "cis_drv_setup: not enough memory for device descriptor\n");
      sanei_pa4s2_close (dev->fd);
      return;
   }
   memset(cisdev, 0, sizeof(Mustek_PP_CIS_dev));
   DBG(3, "cis_drv_setup: cis device allocated\n");
   
   dev->lamp_on = 0;
   dev->priv = cisdev;
   
   cisdev->desc = dev;
   cisdev->model = dev->dev->info;
   cisdev->CIS.hw_hres = 300;
   cisdev->CIS.cisRes = 300;
   cisdev->CIS.hw_vres = 300;
   
   /* Default values for configurable parameters; configuration file
      may override them. */
   cisdev->fast_skip = SANE_TRUE;
   cisdev->bw_limit = 127;
   cisdev->calib_mode = SANE_FALSE;
   cisdev->engine_delay = 0;
   if (cisdev->model == MUSTEK_PP_CIS600)
   {
      cisdev->top_skip = MUSTEK_PP_CIS_600CP_DEFAULT_SKIP;
   }
   else
   {
      cisdev->top_skip = MUSTEK_PP_CIS_1200CP_DEFAULT_SKIP;
   }
}

/******************************************************************************
* Config                                                                      *
******************************************************************************/
SANE_Status cis_drv_config(SANE_Handle hndl, SANE_String_Const optname,
	 		   SANE_String_Const optval)
{
   Mustek_pp_Handle *dev = hndl;
   Mustek_PP_CIS_dev *cisdev = dev->priv;
   int value = 0;
   double dvalue = 0;
   DBG (3, "cis_drv_cfg option: %s=%s\n", optname, optval ? optval : "");
   if (!strcmp(optname, "top_adjust"))
   {
      if (!optval)
      {
         DBG (1, "cis_drv_config: missing value for option top_adjust\n");
         return SANE_STATUS_INVAL;
      }
      dvalue = atof(optval);
      
      /* An adjustment of +/- 5 mm should be sufficient and safe */
      if (dvalue < -5.0)
      {
         DBG (1, "cis_drv_config: value for option top_adjust too small: "
                 "%.2f < -5; limiting to -5 mm\n", dvalue);
         dvalue = -5.0;
      }
      if (dvalue > 5.0)
      {
         DBG (1, "cis_drv_config: value for option top_adjust too large: "
                 "%.2f > 5; limiting to 5 mm\n", dvalue);
         dvalue = 5.0;
      }
      /* In practice, there is a lower bound on the value that can be used,
         but if the top_skip value is smaller than that value, the only result
         will be that the driver tries to move the head a negative number 
         of steps after calibration. The move routine just ignores negative
         steps, so no harm can be done. */
      cisdev->top_skip += MM_TO_PIXEL(dvalue, dev->dev->maxres);
      DBG (3, "cis_drv_config: setting top skip value to %d\n", 
              cisdev->top_skip);
      
      /* Just to be cautious; we don't want the head to hit the bottom */
      if (cisdev->top_skip > 600) cisdev->top_skip = 600;
      if (cisdev->top_skip < -600) cisdev->top_skip = -600;
   }
   else if (!strcmp(optname, "slow_skip"))
   {
      if (optval)
      {
         DBG (1, "cis_drv_config: unexpected value for option slow_skip\n");
         return SANE_STATUS_INVAL;
      }
      DBG (3, "cis_drv_config: disabling fast skipping\n");
      cisdev->fast_skip = SANE_FALSE;
   }
   else if (!strcmp(optname, "bw"))
   {
      if (!optval)
      {
         DBG (1, "cis_drv_config: missing value for option bw\n");
         return SANE_STATUS_INVAL;
      }
      value = atoi(optval);
      if (value < 0 || value > 255)
      {
         DBG (1, "cis_drv_config: value for option bw out of range: "
                 "%d < 0 or %d > 255\n", value, value);
         return SANE_STATUS_INVAL;
      }
      cisdev->bw_limit = value;
   }
   else if (!strcmp(optname, "calibration_mode"))
   {
      if (optval)
      {
         DBG (1, "cis_drv_config: unexpected value for option calibration_mode\n");
         return SANE_STATUS_INVAL;
      }
      DBG (3, "cis_drv_config: using calibration mode\n");
      cisdev->calib_mode = SANE_TRUE;
   }
   else if (!strcmp(optname, "engine_delay"))
   {
      if (!optval)
      {
         DBG (1, "cis_drv_config: missing value for option engine_delay\n");
         return SANE_STATUS_INVAL;
      }
      value = atoi(optval);
      if (value < 0 || value > 100) /* 100 ms is already pretty slow */
      {
         DBG (1, "cis_drv_config: value for option engine_delay out of range: "
                 "%d < 0 or %d > 100\n", value, value);
         return SANE_STATUS_INVAL;
      }
      cisdev->engine_delay = value;
   }
   else
   {
      DBG (1, "cis_drv_config: unknown options %s\n", optname);
      return SANE_STATUS_INVAL;
   }
   return SANE_STATUS_GOOD;
}

/******************************************************************************
* Close                                                                       *
******************************************************************************/
void cis_drv_close (SANE_Handle hndl)
{
   Mustek_pp_Handle *dev = hndl;
   Mustek_PP_CIS_dev *cisdev = dev->priv;
   DBG (3, "cis_close: resetting device.\n");
   sanei_pa4s2_enable (dev->fd, SANE_TRUE); 
   cis_reset_device (cisdev);
   DBG (3, "cis_close: returning home.\n");
   cis_return_home (cisdev, SANE_TRUE); /* Don't wait */
   DBG (3, "cis_close: disabling fd.\n");
   sanei_pa4s2_enable (dev->fd, SANE_FALSE);
   DBG (3, "cis_close: closing fd.\n");
   sanei_pa4s2_close (dev->fd);
   DBG (3, "cis_close: done.\n");
   DBG (6, "cis_close: lamp_on: %d\n", (int)dev->lamp_on);
   M1015_STOP_LL;
   M1015_STOP_HL;
}

/******************************************************************************
* Start                                                                       *
******************************************************************************/
SANE_Status cis_drv_start (SANE_Handle hndl)
{
   Mustek_pp_Handle *dev = hndl;
   Mustek_PP_CIS_dev *cisdev = dev->priv;
   SANE_Int pixels = dev->params.pixels_per_line;
   
   if (!cisdev)
   {
      DBG (2, "cis_drv_start: not enough memory for device\n");
      return SANE_STATUS_NO_MEM;
   }

   cisdev->CIS.exposeTime = 0xAA;
   cisdev->CIS.setParameters = SANE_FALSE;
   cisdev->CIS.use8KBank = SANE_TRUE;
   cisdev->CIS.imagebytes = dev->bottomX - dev->topX;
   cisdev->CIS.skipimagebytes = dev->topX;

   cisdev->CIS.res = dev->res;

   DBG (3, "cis_drv_start: %d dpi\n", dev->res);

   if (dev->res <= 50 && cisdev->model != MUSTEK_PP_CIS1200PLUS)
   {
     cisdev->CIS.hw_hres = 50;
   }
   else if (dev->res <= 75 && cisdev->model == MUSTEK_PP_CIS1200PLUS)
   {
     cisdev->CIS.hw_hres = 75;
   }
   else if (dev->res <= 100)
   {
     cisdev->CIS.hw_hres = 100;
   }
   else if (dev->res <= 200)
   {
     cisdev->CIS.hw_hres = 200;
   }
   else if (dev->res <= 300)
   {
     cisdev->CIS.hw_hres = 300;
   }
   else
   {
      if (cisdev->model == MUSTEK_PP_CIS600)
      {
         cisdev->CIS.hw_hres = 300; /* Limit for 600 CP */
      }
      else if (dev->res <= 400)
      {
   	 cisdev->CIS.hw_hres = 400;
      }
      else
      {
	 cisdev->CIS.hw_hres = 600; /* Limit for 1200 CP/CP+ */
      }
   }

   if (cisdev->model == MUSTEK_PP_CIS600)
   {
      if (dev->res <= 150)
      {
         cisdev->CIS.hw_vres = 150;
      }
      else if (dev->res <= 300)
      {
         cisdev->CIS.hw_vres = 300;
      }
      else
      {
         cisdev->CIS.hw_vres = 600;
      }
   }
   else
   {
      if (dev->res <= 300)
      {
         cisdev->CIS.hw_vres = 300;
      }
      else if (dev->res <= 600)
      {
         cisdev->CIS.hw_vres = 600;
      }
      else
      {
         cisdev->CIS.hw_vres = 1200;
      }
   }

   if (cisdev->model == MUSTEK_PP_CIS600 ||
       (cisdev->model == MUSTEK_PP_CIS1200 && dev->res <= 300))
     cisdev->CIS.cisRes = 300;
   else
     cisdev->CIS.cisRes = 600;         

   /* Calibration only makes sense for hardware pixels, not for interpolated
      pixels, so we limit the number of calibration pixels to the maximum
      number of hardware pixels corresponding to the selected area */
   if (dev->res > cisdev->CIS.hw_hres)
      cisdev->calib_pixels = (pixels * cisdev->CIS.hw_hres) / dev->res;
   else
      cisdev->calib_pixels = pixels;
   
   DBG (3, "cis_drv_start: hres: %d vres: %d cisres: %d\n", 
            cisdev->CIS.hw_hres, cisdev->CIS.hw_vres, cisdev->CIS.cisRes);
   
   sanei_pa4s2_enable (dev->fd, SANE_TRUE);

   cis_reset_device (cisdev);
   cis_return_home (cisdev, SANE_TRUE); /* Don't wait here */

#ifdef M1015_TRACE_REGS
   {
      int i, j;

      /*
       * Set all registers to -1 (uninitialized)
       */  
      for (i=0; i<4; ++i)
      {
         cisdev->CIS.regs.in_regs[i] = -1;
         for (j=0; j<4; ++j)
         {
            cisdev->CIS.regs.out_regs[i][j] = -1;
         }
      }

      cisdev->CIS.regs.channel = -1;
      /* These values have been read earlier. */
      cisdev->CIS.regs.in_regs[0] = 0xA5;
   }
#endif

   cis_reset_device (cisdev);
   cis_return_home (cisdev, SANE_TRUE); /* no wait */

   /* Allocate memory for temporary color buffer */
   cisdev->tmpbuf = malloc (pixels);
   if (cisdev->tmpbuf == NULL)
   {
      sanei_pa4s2_enable (dev->fd, SANE_FALSE);
      DBG (2, "cis_drv_start: not enough memory for temporary buffer\n");
      free(cisdev);
      dev->priv = NULL;
      return SANE_STATUS_NO_MEM;
   }
   
   /* Allocate memory for calibration; calibrating interpolated pixels 
      makes no sense */
   if (pixels > (dev->dev->maxhsize >> 1)) 
      pixels = (dev->dev->maxhsize >> 1);
   
   cisdev->calib_low[1] = malloc (pixels);
   cisdev->calib_hi[1] = malloc (pixels);

   if (cisdev->calib_low[1] == NULL || cisdev->calib_hi[1] == NULL)
   {
      free (cisdev->calib_low[1]); cisdev->calib_low[1] = NULL;
      free (cisdev->calib_hi[1]);  cisdev->calib_hi[1] = NULL;
      sanei_pa4s2_enable (dev->fd, SANE_FALSE);
      DBG (2, "cis_drv_start: not enough memory for calibration buffer\n");
      free(cisdev->tmpbuf); cisdev->tmpbuf = NULL;
      free(cisdev); dev->priv = NULL;
      return SANE_STATUS_NO_MEM;
   }

   cisdev->calib_low[0] = NULL;
   cisdev->calib_low[2] = NULL;
   cisdev->calib_hi[0] = NULL;
   cisdev->calib_hi[2] = NULL;
   if (dev->mode == MODE_COLOR)
   {
      cisdev->calib_low[0] = malloc (pixels);
      cisdev->calib_low[2] = malloc (pixels);
      cisdev->calib_hi[0] = malloc (pixels);
      cisdev->calib_hi[2] = malloc (pixels);

      if ((cisdev->calib_low[0] == NULL) || (cisdev->calib_low[2] == NULL) ||
          (cisdev->calib_hi[0] == NULL) || (cisdev->calib_hi[2] == NULL))
      {
         free (cisdev->calib_low[0]); cisdev->calib_low[0] = NULL;
	 free (cisdev->calib_low[1]); cisdev->calib_low[1] = NULL;
         free (cisdev->calib_low[2]); cisdev->calib_low[2] = NULL;
         free (cisdev->calib_hi[0]); cisdev->calib_hi[0] = NULL;
	 free (cisdev->calib_hi[1]); cisdev->calib_hi[1] = NULL;
         free (cisdev->calib_hi[2]); cisdev->calib_hi[2] = NULL;
         free(cisdev->tmpbuf); cisdev->tmpbuf = NULL;
         free(cisdev); dev->priv = NULL;
	 sanei_pa4s2_enable (dev->fd, SANE_FALSE);
	 DBG (2, "cis_drv_start: not enough memory for color calib buffer\n");
	 return SANE_STATUS_NO_MEM;
      }
   }

   DBG (3, "cis_drv_start: executing calibration\n");

   if (!cis_calibrate (cisdev))
   {
      free (cisdev->calib_low[0]); cisdev->calib_low[0] = NULL;
      free (cisdev->calib_low[1]); cisdev->calib_low[1] = NULL;
      free (cisdev->calib_low[2]); cisdev->calib_low[2] = NULL;
      free (cisdev->calib_hi[0]); cisdev->calib_hi[0] = NULL;
      free (cisdev->calib_hi[1]); cisdev->calib_hi[1] = NULL;
      free (cisdev->calib_hi[2]); cisdev->calib_hi[2] = NULL;
      free(cisdev->tmpbuf); cisdev->tmpbuf = NULL;
      free(cisdev); dev->priv = NULL;
      return SANE_STATUS_CANCELLED; /* Most likely cause */
   }

/*   M1015_DISPLAY_REGS(dev, "after calibration"); */

   cis_get_bank_count(cisdev);

   cis_move_motor (cisdev, dev->topY); /* Measured in max resolution */

   /* It is vital to reinitialize the scanner right before we start the 
      real scanning. Otherwise the bank synchronization may have gotten lost
      by the time we reach the top of the scan area */

   cisdev->CIS.setParameters = SANE_TRUE;
   cis_config_ccd(cisdev);
   cis_wait_read_ready(cisdev);

   sanei_pa4s2_enable (dev->fd, SANE_FALSE); 

   cisdev->CIS.line_step =
     SANE_FIX ((float) cisdev->CIS.hw_vres / (float) cisdev->CIS.res);

   /* 
    * It is very important that line_diff is not initialized at zero !
    * If it is set to zero, the motor will keep on moving forever (or better,
    * till the scanner breaks).
    */
   cisdev->line_diff = cisdev->CIS.line_step;
   cisdev->ccd_line = 0;
   cisdev->line = 0;
   cisdev->lines_left = dev->params.lines;

   dev->state = STATE_SCANNING;

   DBG (3, "cis_drv_start: device ready for scanning\n");

   return SANE_STATUS_GOOD;
}

/******************************************************************************
* Read                                                                        *
******************************************************************************/
void cis_drv_read (SANE_Handle hndl, SANE_Byte *buffer)
{
   Mustek_pp_Handle *dev = hndl;
   Mustek_PP_CIS_dev *cisdev = dev->priv;
   DBG(6, "cis_drv_read: Reading line\n");
   sanei_pa4s2_enable (dev->fd, SANE_TRUE); 
   switch (dev->mode)
   {
      case MODE_BW:
         cis_get_lineart_line(cisdev, buffer);
         break;

      case MODE_GRAYSCALE:
         cis_get_grayscale_line(cisdev, buffer);
         break;

      case MODE_COLOR:
         cis_get_color_line(cisdev, buffer);
         break;
   }
   sanei_pa4s2_enable (dev->fd, SANE_FALSE);
}

/******************************************************************************
* Stop                                                                        *
******************************************************************************/
void cis_drv_stop (SANE_Handle hndl)
{
   Mustek_pp_Handle *dev = hndl;
   Mustek_PP_CIS_dev *cisdev = dev->priv;
   
   /* device is scanning: return lamp and free buffers */
   DBG (3, "cis_drv_stop: stopping current scan\n");
   dev->state = STATE_CANCELLED;

   DBG (9, "cis_drv_stop: enabling fd\n");
   sanei_pa4s2_enable (dev->fd, SANE_TRUE); 
   Mustek_PP_1015_write_reg(cisdev, MA1015W_MOTOR_CONTROL, 0); /* stop */
   DBG (9, "cis_drv_stop: resetting device (1)\n");
   cis_reset_device (cisdev);
   DBG (9, "cis_drv_stop: returning home\n");
   cis_return_home (cisdev, SANE_TRUE); /* don't wait */
   DBG (9, "cis_drv_stop: resetting device (2)\n");
   cis_reset_device (cisdev);
   DBG (9, "cis_drv_stop: disabling fd\n");
   sanei_pa4s2_enable (dev->fd, SANE_FALSE); 
   DBG (9, "cis_drv_stop: freeing buffers\n");

   /* This is no good: canceling while the device is scanning and
      freeing the data buffers can result in illegal memory accesses if
      the device is still scanning in another thread. */
   free (cisdev->calib_low[1]); cisdev->calib_low[1] = NULL;
   free (cisdev->calib_hi[1]); cisdev->calib_hi[1] = NULL;
   free (cisdev->tmpbuf); cisdev->tmpbuf = NULL;
   DBG (3, "cis_drv_stop: freed green and temporary buffers\n");

   if (cisdev->CIS.mode == MODE_COLOR)
   {
      free (cisdev->calib_low[0]); cisdev->calib_low[0] = NULL;
      free (cisdev->calib_low[2]); cisdev->calib_low[2] = NULL;
      free (cisdev->calib_hi[0]); cisdev->calib_hi[0] = NULL;
      free (cisdev->calib_hi[2]); cisdev->calib_hi[2] = NULL;
   }
   DBG (3, "cis_drv_stop: freed buffers\n");
   DBG (6, "cis_drv_stop: lamp_on: %d\n", (int)dev->lamp_on);
}
