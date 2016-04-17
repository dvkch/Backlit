/* sane - Scanner Access Now Easy.

   Copyright (C) 1999 Paul Mackerras
   Copyright (C) 2000 Adrian Perez Jorge
   Copyright (C) 2001 Frank Zago
   Copyright (C) 2001 Marcio Teixeira
   Parts copyright (C) 2006 Patrick Lessard

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

   Defines a driver and API for accessing the UMAX Astra 1220U
   USB scanner. Based on the original command line tool by Paul
   Mackerras.

   The UMAX Astra 1220U scanner uses the PowerVision PV8630
   Parallel Port to USB bridge. This chip is also used
   by the HP4200C flatbed scanner. Adrian Perez Jorge wrote
   a nice interface file for that chip and Frank Zago adapted
   it to use the sanei_usb interface. Thanks, guys, for making
   my life easier! :)

 */


#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <math.h>

/* 
 * The backend performs test scans in order to calibrate
 * the CCD and to find the zero location. If you would like
 * to look at those scans, define DEBUG_CALIBRATION to have
 * the backend save "find_zero.pgm" and "calibration.pgm" to
 * disk.
 */
/* #define DEBUG_CALIBRATION */

/*
 * Define DEBUG_BOUNDS to insert paranoid array bounds
 * overrun detection into the code.
 */
/* #define DEBUG_BOUNDS */

/* These values are empirically determined and are given
 * in 1/600 inch units. If UMAX_MAX_HEIGHT is too large,
 * the scanner may grind its gears. I assume there is a
 * physical limit to UMAX_MAX_WIDTH as well (based on the
 * sensor size) but I do not know what it is. The current
 * value can be increased beyond what it is now, but you
 * gain nothing in usuable scan area (you only scan more
 * of the underside of the scanner's plastic lid).
 */


#define UMAX_MAX_WIDTH    5400
#define UMAX_MAX_HEIGHT   7040

/* Buffer size. Specifies the size of the buffer that is
 * used to copy data from the scanner. The old command
 * line driver had this set at 0x80000 which is likely
 * the largest possible chunck of data that can be.
 * at once. This is probably most efficient, but using
 * a lower value for the SANE driver makes the driver
 * more responsive to interaction.
 */
#define BUFFER_SIZE 0x80000

/* Constants that can be used with set_lamp_state to
 * control the state of the scanner's lamp
 */
typedef enum
{
  UMAX_LAMP_OFF = 0,
  UMAX_LAMP_ON = 1
}
UMAX_Lamp_State;

/* Constants that can be used with move to control
 * the rate of scanner head movement
 */
typedef enum
{
  UMAX_NOT_FINE = 0,
  UMAX_FINE = 1
}
UMAX_Speed;

/* If anyone knows some descriptive names for these,
 * please update
 */
typedef enum
{
  CMD_0 = 0x00,
  CMD_1 = 0x01,
  CMD_2 = 0x02,
  CMD_4 = 0x04,
  CMD_8 = 0x08,
  CMD_40 = 0x40,
  CMD_WRITE = 0x80,
  CMD_READ = 0xc0
}
UMAX_Cmd;

/* Product IDs for Astra scanners
 */
typedef enum
{
  ASTRA_1220U = 0x0010,
  ASTRA_2000U = 0x0030,
  ASTRA_2100U = 0x0130
}
UMAX_Model;

/* The bytes UMAX_SYNC1 and UMAX_SYNC2 serve as a
 * synchronization signal. Unintentional sync bytes
 * in the data stream are escaped with UMAX_ESCAPE
 * character
 */

#define UMAX_SYNC1  0x55
#define UMAX_SYNC2  0xaa
#define UMAX_ESCAPE 0x1b

/* Status bits. These bits are active low.
 * In umax_pp, UMAX_REVERSE_BIT is called
 * MOTOR_BIT.
 */

#define UMAX_FORWARD_BIT       0x40
#define UMAX_ERROR_BIT         0x20
#define UMAX_MOTOR_OFF_BIT     0x08

#define UMAX_OK                0x48        /* Used to be 0xC8 */
#define UMAX_OK_WITH_MOTOR     0x40        /* Used to be 0xD0 */

#define UMAX_STATUS_MASK       0x68

/* This byte is used as a placeholder for bytes that are parameterized
 * in the opcode strings */

#define XXXX 0x00

/* This macro is used to check the return code of
 * functions
 */
#define CHK(A) {if( (res = A) != SANE_STATUS_GOOD ) { \
                 DBG( 1, "Failure on line of %s: %d\n", __FILE__, \
                      __LINE__ ); return A; }}

/* Macros that are used for array overrun detection
 * (when DEBUG_BOUNDS is defined)
 */
#ifdef DEBUG_BOUNDS
#define PAD 10
#define PAD_ARRAY( A, len ) {int i; \
                 for( i = 0; i < PAD; i++ ) {A[len+i]=0x55;}}

#define CHK_ARRAY( A, len ) {int i;for( i = 0; i < PAD; i++ ) {\
   if(A[len+i]!=0x55) { \
     DBG( 1, "Array overrun detected on line %d\n", __LINE__ ); \
   }}}
#else
#define PAD 0
#define PAD_ARRAY( A, len )
#define CHK_ARRAY( A, len )
#endif


/* This data structure contains data related
 * to the scanning process.
 */
typedef struct
{
  /* Constant data */

  int color;
  int w;
  int h;
  int xo;
  int yo;
  int xdpi;                      /* Physical x dpi */
  int ydpi;                      /* Physical y dpi */
  int xsamp;
  int ysamp;

  int xskip;
  int yskip;

  int fd;                        /* Device file handle */
  UMAX_Model model;

  /* Raw scan data buffer */

  unsigned char *p;
  int bh;                        /* Size of buffer in lines    */
  int hexp;                      /* Scan lines yet to be read  */

  /* Decoding logic */

  int x, y, maxh;
  int done;                      /* Boolean, all lines decoded */

  /* Calibration data */

  unsigned char caldata[16070 + PAD];

  /* Scan head position */

  int scanner_ypos;
  int scanner_yorg;
}
UMAX_Handle;

typedef unsigned char UMAX_Status_Byte;


#if 0
static void
unused_operations ()
{
  /* These operations are unused anywhere in the driver */

  unsigned char opb8[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0x18, 0x10, 0x03,
    0x06, 0x00, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x13, 0x1a
  };

  unsigned char opb9[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0x41, 0x20, 0x24,
    0x06, 0x00, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x13, 0x1a
  };

  unsigned char opb10[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0x41, 0x60, 0x4f,
    0x06, 0x00, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x93, 0x1a
  };

  unsigned char opc5[16] = {
    0x05, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2f, 0x2f, 0x00,
    0x00, 0x30, 0x0c, 0xc3, 0xa4, 0x00
  };

  unsigned char opc6[16] = {
    0x01, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2f, 0x2f, 0x00,
    0x88, 0x48, 0x0c, 0x83, 0xa4, 0x00
  };

  unsigned char opc7[16] = {
    0x05, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2f, 0x2f, 0x00,
    0xec, 0x4e, 0x0c, 0xc3, 0xa4, 0x00
  };

  unsigned char opd2[8] = {
    0x06, 0xf4, 0xff, 0x81, 0x3d, 0x00, 0x00, 0x30
  };
}
#endif

#if 0
static SANE_Status
calib (UMAX_Handle * scan)
{
  unsigned char buf[65536];
  opc5[11] = 0x30;
  opd2[7]  = 0x30;
  CHK (get_pixels (scan, opc5, opb8, opd2, ope, 24, 0, buff));

  opc5[11] = 0x40;
  opd2[7]  = 0x40;
  CHK (get_pixels (scan, opc5, opb8, opd2, ope, 24, 0, buff));

  opd2[6] = 8;
  opd2[7] = 0x30;
  CHK (get_pixels (scan, opc6, opb9, opd2, ope, 0x200, 1, buff));

  opc7[10] = 0xec;
  opd2[6]  = 0xc;
  opd2[7]  = 0x40;
  CHK (get_pixels (scan, opc7, opb10, opd2, ope, 5300, 1, buff));

  opc7[10] = 0xed;
  opd2[6]  = 0xd;
  CHK (get_pixels (scan, opc7, opb10, opd2, ope, 5300, 0, buff));
  return SANE_STATUS_GOOD;
}
#endif


/* This seems to configure the pv8630 chip somehow. I wish
 * all the magic numbers were defined as self-descriptive
 * constants somewhere. I made some guesses based on what
 * I found in "pv8630.c", but alas there wasn't enough in
 * there. If you know what this does, please let me know!
 */
static SANE_Status
xxxops (UMAX_Handle * scan)
{
  SANE_Status res;

  DBG (9, "doing xxxops\n");

  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_RMODE, 0x02));

  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_UNKNOWN, 0x0E));
  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_RDATA, 0x40));
  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_UNKNOWN, 0x06));
  CHK (sanei_pv8630_xpect_byte (scan->fd, PV8630_RSTATUS, 0x38, 0xFF));

  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_UNKNOWN, 0x07));
  CHK (sanei_pv8630_xpect_byte (scan->fd, PV8630_RSTATUS, 0x38, 0xFF));

  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_UNKNOWN, 0x04));
  CHK (sanei_pv8630_xpect_byte (scan->fd, PV8630_RSTATUS, 0xF8, 0xFF));

  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_UNKNOWN, 0x05));
  CHK (sanei_pv8630_xpect_byte (scan->fd, PV8630_UNKNOWN, 0x05, 0xFF));

  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_UNKNOWN, 0x04));

  CHK (sanei_pv8630_write_byte (scan->fd, PV8630_RMODE, 0x1E));

  return res;
}

/*
Apparently sends the two syncronization characters followed
by the command length, followed by the command number
*/
static SANE_Status
usync (UMAX_Handle * scan, UMAX_Cmd cmd, int len)
{
  UMAX_Status_Byte s0, s4;
  SANE_Status res;
  unsigned char buf[4];
  size_t nb;

  DBG (80, "usync: len = %d, cmd = %d\n", len, cmd);

  buf[0] = UMAX_SYNC1;
  buf[1] = UMAX_SYNC2;

  nb = 2;
  CHK (sanei_pv8630_flush_buffer (scan->fd));
  CHK (sanei_pv8630_prep_bulkwrite (scan->fd, nb));
  CHK (sanei_pv8630_bulkwrite (scan->fd, buf, &nb));

  CHK (sanei_pv8630_wait_byte
       (scan->fd, PV8630_RSTATUS, UMAX_OK, UMAX_STATUS_MASK, 20));

  buf[0] = len >> 16;
  buf[1] = len >> 8;
  buf[2] = len;
  buf[3] = cmd;

  nb = 4;
  CHK (sanei_pv8630_flush_buffer (scan->fd));
  CHK (sanei_pv8630_prep_bulkwrite (scan->fd, nb));
  CHK (sanei_pv8630_bulkwrite (scan->fd, buf, &nb));

  CHK (sanei_pv8630_read_byte (scan->fd, PV8630_RDATA, &s0));
  CHK (sanei_pv8630_read_byte (scan->fd, PV8630_RSTATUS, &s4));

  DBG (90, "usync: s0 = %#x s4 = %#x\n", s0, s4);

  return SANE_STATUS_GOOD;
}

/*
This function escapes any syncronization sequence that may be
in data, storing the result in buf. In the worst case where
every character gets escaped buf must be at least twice as
large as dlen.
*/
static int
bescape (const unsigned char *data, int dlen, unsigned char *buf, int blen)
{
  const unsigned char *p;
  unsigned char *q;
  int i, c;
  i = blen;   /* Eliminate compiler warning about unused param */

  p = data;
  q = buf;
  for (i = 0; i < dlen; ++i)
    {
      c = *p++;
      if (c == UMAX_ESCAPE
          || (c == UMAX_SYNC2 && i > 0 && p[-2] == UMAX_SYNC1))
        *q++ = UMAX_ESCAPE;
      *q++ = c;
    }
  return q - buf;
}



/* Write */

static SANE_Status
cwrite (UMAX_Handle * scan, UMAX_Cmd cmd, size_t len, const unsigned char *data,
        UMAX_Status_Byte * s)
{
  SANE_Status res;
  UMAX_Status_Byte s0, s4;

  static unsigned char *escaped = NULL;
  static size_t escaped_size = 0;

  DBG (80, "cwrite: cmd = %d, len = %lu\n", cmd, (u_long) len);

  CHK (usync (scan, cmd | CMD_WRITE, len));

  if (len <= 0)
    return SANE_STATUS_GOOD;

  if (escaped_size < len * 2)
    {
      escaped_size = len * 2;
      if (escaped)
        free (escaped);
      escaped = malloc (escaped_size);
      if (escaped == NULL)
        return SANE_STATUS_NO_MEM;
    }

  len = bescape (data, len, escaped, len * 2);

  CHK (sanei_pv8630_wait_byte
       (scan->fd, PV8630_RSTATUS, UMAX_OK, UMAX_STATUS_MASK, 20));

  CHK (sanei_pv8630_flush_buffer (scan->fd));
  CHK (sanei_pv8630_prep_bulkwrite (scan->fd, len));
  CHK (sanei_pv8630_bulkwrite (scan->fd, escaped, &len));

  CHK (sanei_pv8630_read_byte (scan->fd, PV8630_RSTATUS, &s4));
  CHK (sanei_pv8630_read_byte (scan->fd, PV8630_RDATA, &s0));

  DBG (90, "cwrite: s0 = %#x s4 = %#x\n", s0, s4);

  if (s)
    *s = s0;

  return SANE_STATUS_GOOD;
}

/* Read */

static SANE_Status
cread (UMAX_Handle * scan, UMAX_Cmd cmd, size_t len, unsigned char *data,
       UMAX_Status_Byte * s)
{
  SANE_Status res;
  UMAX_Status_Byte s0, s4;

  DBG (80, "cread: cmd = %d, len = %lu\n", cmd, (u_long) len);

  CHK (usync (scan, cmd | CMD_READ, len));

  if (len > 0)
    {
      CHK (sanei_pv8630_wait_byte
           (scan->fd, PV8630_RSTATUS, UMAX_OK_WITH_MOTOR, UMAX_STATUS_MASK,
            2000));

      while (len > 0)
        {
          size_t req, n;

          req = n = (len > 0xf000) ? 0xf000 : len;
          CHK (sanei_pv8630_prep_bulkread (scan->fd, n));
          CHK (sanei_pv8630_bulkread (scan->fd, data, &n));
          if (n < req)
            {
              DBG (1, "qread: Expecting to read %lu, only got %lu\n", (u_long) req, (u_long) n);
              return SANE_STATUS_IO_ERROR;
            }
          data += n;
          len -= n;
        }
    }

  CHK (sanei_pv8630_read_byte (scan->fd, PV8630_RSTATUS, &s4));
  CHK (sanei_pv8630_read_byte (scan->fd, PV8630_RDATA, &s0));

  DBG (90, "cwrite: s0 = %#x s4 = %#x\n", s0, s4);

  if (s)
    *s = s0;

  return SANE_STATUS_GOOD;
}



/* Seems to be like cwrite, with a verification option */

static SANE_Status
cwritev (UMAX_Handle * scan, UMAX_Cmd cmd, size_t len, const unsigned char *data,
         UMAX_Status_Byte * s)
{
  SANE_Status res;
  unsigned char buf[16384];

  /* Write out the opcode */

  CHK (cwrite (scan, cmd, len, data, s));
  if (len <= 0)
    return SANE_STATUS_GOOD;

  /* Read the opcode back */

  CHK (cread (scan, cmd, len, buf, NULL));
  if (bcmp (buf, data, len))
    {
      DBG (1, "cwritev: verification failed\n");
      return SANE_STATUS_IO_ERROR;
    }
  return SANE_STATUS_GOOD;
}


/* Send command */

static SANE_Status
csend (UMAX_Handle * scan, UMAX_Cmd cmd)
{
  DBG (80, "csend: cmd = %d\n", cmd);

  return usync (scan, cmd, 0);
}

/* Lamp control */

static SANE_Status
cwritev_opc1_lamp_ctrl (UMAX_Handle * scan, UMAX_Lamp_State state)
{
  unsigned char opc1[16] = {
    0x01, 0x00, 0x01, 0x70, 0x00, 0x00, 0x60, 0x2f, 0x13, 0x05,
    0x00, 0x00, 0x00, 0x80, 0xf0, 0x00
  };

  DBG (9, "cwritev_opc1: set lamp state = %s\n",
       (state == UMAX_LAMP_OFF) ? "off" : "on");
  opc1[14] = (state == UMAX_LAMP_OFF) ? 0x90 : 0xf0;
  return cwritev (scan, CMD_2, 16, opc1, NULL);
}


/* Restore Head 1220U */

static SANE_Status
cwritev_opb3_restore (UMAX_Handle * scan)
{
  unsigned char opb3[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x03,
    0xc1, 0x80, 0x00, 0x00, 0x04, 0x00, 0x16, 0x80, 0x15, 0x78,
    0x03, 0x03, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x1b, 0x1a,
  };

  SANE_Status res;

  DBG (9, "cwritev_opb3_restore:\n");
  CHK (cwritev (scan, CMD_8, 35, opb3, NULL));
  CHK (csend (scan, CMD_40));
  return SANE_STATUS_GOOD;
}


/* Restore Head 2100U */

static SANE_Status
cwritev_opb3_restore_2100U (UMAX_Handle * scan)
{
  unsigned char opb3[36] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x03,
    0xc1, 0x80, 0x00, 0x00, 0x04, 0x00, 0x16, 0x80, 0x15, 0x78,
    0x03, 0x03, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x2a,
    0xe9, 0x68, 0xdf, 0x0b, 0x1a, 0x00
  };

  SANE_Status res;

  DBG (9, "cwritev_opb3_restore:\n");
  CHK (cwritev (scan, CMD_8, 36, opb3, NULL));
  CHK (csend (scan, CMD_40));
  return SANE_STATUS_GOOD;
}


/* Initialize and turn lamp on 1220U */

/*
This function seems to perform various things. First, it loads a default
gamma information (which is used for the calibration scan), returns the
head to the park position, and turns the lamp on. This function used to
be split up into two parts, umaxinit and umaxinit2.
*/

static SANE_Status
umaxinit (UMAX_Handle * scan)
{
  unsigned char opb[34] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x03,
    0xc1, 0x80, 0x60, 0x20, 0x00, 0x00, 0x16, 0x41, 0xe0, 0xac,
    0x03, 0x03, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xf0
  };
  unsigned char opb1[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x03,
    0xc1, 0x80, 0x00, 0x20, 0x02, 0x00, 0x16, 0x41, 0xe0, 0xac,
    0x03, 0x03, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x1a
  };
  unsigned char opb2[35] = {
    0x00, 0x00, 0x06, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x03,
    0xc1, 0x80, 0x00, 0x20, 0x02, 0x00, 0x16, 0x41, 0xe0, 0xac,
    0x03, 0x03, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x1a
  };
  unsigned char opb4[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x03,
    0xc1, 0x80, 0x60, 0x20, 0x00, 0x00, 0x16, 0x41, 0xe0, 0xac,
    0x03, 0x03, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0xf3, 0x1b
  };
  unsigned char opbx[35];
  unsigned char opc[16] = {
    0x02, 0x80, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2f, 0x2f, 0x07,
    0x00, 0x00, 0x00, 0x80, 0xf0, 0x00
  };
  unsigned char opcx[16];
  unsigned char opd[8] = {
    0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa
  };

  SANE_Status res;
  UMAX_Status_Byte s;
  unsigned char ramp[800];
  int i;
  unsigned char *p;

  DBG (3, "umaxinit called\n");

  CHK (csend (scan, CMD_0));
  CHK (xxxops (scan));

  CHK (cwritev (scan, CMD_8, 34, opb, &s));
  CHK (cread (scan, CMD_8, 35, opbx, &s));

  CHK (cwritev (scan, CMD_8, 35, opb1, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (4, "umaxinit: checkpoint 2:\n");

  /* The following code appears to send three 256 entry, 8-bit gamma tables
   * to the scanner
   */
  p = ramp;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  for (i = 0; i < 256; ++i)
    *p++ = i;
  for (i = 0; i < 256; ++i)
    *p++ = i;
  for (i = 0; i < 256; ++i)
    *p++ = i;
  *p++ = 0xaa;
  *p++ = 0xaa;

  res = cwritev (scan, CMD_4, p - ramp, ramp, &s);
  if (res != SANE_STATUS_GOOD)
    {
      DBG (4, "umaxinit: Writing ramp 1 failed (is this a 2000U?)\n");
    }
  CHK (cwritev (scan, CMD_8, 35, opb1, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (4, "umaxinit: checkpoint 3:\n");

  /* The following code appears to send a 256 entry, 16-bit gamma table
   * to the scanner
   */
  p = ramp;
  for (i = 0; i < 256; ++i)
    {
      *p++ = i;
      *p++ = 0;
    }

  res = cwrite (scan, CMD_4, p - ramp, ramp, &s);
  if (res != SANE_STATUS_GOOD)
    {
      DBG (4, "umaxinit: Writing ramp 2 failed (is this a 2000U?)\n");
    }
  CHK (cwritev (scan, CMD_8, 35, opb2, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (4, "umaxinit: checkpoint 4:\n");

  /* The following code appears to send a 256 entry, 16-bit gamma table
   * to the scanner.
   */
  p = ramp;
  for (i = 0; i < 256; ++i)
    {
      *p++ = i;
      *p++ = 4;
    }

  res = cwritev (scan, CMD_4, p - ramp, ramp, &s);
  if (res != SANE_STATUS_GOOD)
    {
      DBG (4, "umaxinit: Writing ramp 3 failed (is this a 2000U?)\n");
    }
  CHK (cwritev (scan, CMD_8, 35, opb1, &s));

  CHK (cwritev (scan, CMD_2, 16, opc, NULL));
  CHK (cwritev (scan, CMD_1, 8, opd, NULL));
  CHK (csend (scan, CMD_0));
  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (4, "umaxinit: checkpoint 5: s = %#x\n", s);

  if ((s & 0x40) == 0)
    {
      DBG (4, "umaxinit: turning on lamp and restoring\n");
      CHK (cwritev_opc1_lamp_ctrl (scan, UMAX_LAMP_ON));
      CHK (cwritev_opb3_restore (scan));

      for (i = 0; i < 60; ++i)
        {
          CHK (cread (scan, CMD_2, 0, NULL, &s));
          DBG (4, "umaxinit: s = %#x\n", s);
          if ((s & 0x40) != 0)
            break;
          DBG (4, "umaxinit: sleeping\n");
          usleep (500000);
        }
    }

  DBG (4, "umaxinit: checkpoint 6\n");

  CHK (csend (scan, CMD_0));

  /* The following stuff used to be in umaxinit2() */

  DBG (4, "umaxinit: checkpoint 7\n");

  CHK (xxxops (scan));
  CHK (csend (scan, CMD_0));
  CHK (xxxops (scan));

  CHK (cwritev (scan, CMD_8, 34, opb4, &s));
  CHK (cread (scan, CMD_8, 35, opbx, &s));
  CHK (cread (scan, CMD_2, 16, opcx, &s));

  CHK (cwritev (scan, CMD_2, 16, opc, NULL));
  CHK (cwritev (scan, CMD_1, 8, opd, NULL));
  CHK (csend (scan, CMD_0));
  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (4, "umaxinit: checkpoint 8: s = %d\n", s);

  return SANE_STATUS_GOOD;
}

/* Initialize and turn lamp on 2100U */

static SANE_Status
umaxinit_2100U (UMAX_Handle * scan)
{

  unsigned char opx[36];
  unsigned char opy[16];


  SANE_Status res;
  UMAX_Status_Byte s;

  DBG (3, "umaxinit called\n");

  CHK (xxxops (scan));
  CHK (csend (scan, CMD_0));

  /* Turn lamp on */

  cwritev_opc1_lamp_ctrl (scan, UMAX_LAMP_ON);

  CHK (cread (scan, CMD_8, 36, opx, &s));
  CHK (cread (scan, CMD_2, 16, opy, &s));
  CHK (csend (scan, CMD_0));
  CHK (cread (scan, CMD_2, 0, NULL, &s));
  CHK (csend (scan, CMD_0));

  return SANE_STATUS_GOOD;
}


/* Move head 1220U */

static SANE_Status
move (UMAX_Handle * scan, int distance, UMAX_Speed fine)
{
  unsigned char opc4[16] = {
    0x01, XXXX, XXXX, XXXX, 0x00, 0x00, 0x60, 0x2f, XXXX, XXXX,
    0x00, 0x00, 0x00, 0x80, XXXX, 0x00
  };
  unsigned char opb5[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0xf6, 0x79, 0xbf,
    0x06, 0x00, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x13, 0x1a
  };
  unsigned char opb7[35] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x6e, 0xf6, 0x79, 0xbf,
    0x01, 0x00, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x13, 0x1a
  };

  unsigned char ope[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff
  };

  unsigned char ope2[3] = {
    0x00, 0xff, 0x8f
  };
  unsigned char buf[512 + PAD];

  SANE_Status res;
  UMAX_Status_Byte s;

  SANE_Bool rev = distance < 0;
  int skip = (rev ? -distance : distance) - 1;

  DBG (9, "move: distance = %d, scanner_ypos = %d\n", distance,
       scan->scanner_ypos);

  PAD_ARRAY (buf, 512);

  if (distance == 0)
    return SANE_STATUS_GOOD;

  opc4[1] = skip << 6;
  opc4[2] = skip >> 2;
  opc4[3] = (rev ? 0x20 : 0x70) + ((skip >> 10) & 0xf);
  opc4[9] = rev ? 0x01 : 0x05;

  if (fine == UMAX_FINE)
    {
      opc4[8]  = 0x2f;
      opc4[14] = 0xa4;
    }
  else
    {
      opc4[8]  = 0x17;
      opc4[14] = 0xac;
    }

  scan->scanner_ypos +=
    (fine == UMAX_FINE ? distance : 2 * distance + (rev ? -1 : 1));
  scan->scanner_ypos = (scan->scanner_ypos + (rev ? 0 : 3)) & ~3;

  CHK (cwrite (scan, CMD_2, 16, opc4, &s));
  CHK (cwrite (scan, CMD_8, 35, rev ? opb7 : opb5, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));
  DBG (10, "move: checkpoint 1: s = %d\n", s);

  CHK (csend (scan, CMD_0));
  if (rev)
    CHK (cwrite (scan, CMD_4, 3, ope2, &s))
  else
    CHK (cwrite (scan, CMD_4, 8, ope, &s));


  CHK (csend (scan, CMD_40));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (10, "move: checkpoint 2: s = %d\n", s);

  CHK (cread (scan, CMD_2, 0, NULL, &s));
  DBG (10, "move: checkpoint 3: s = %d\n", s);

  CHK (cread (scan, CMD_4, 512, buf, &s));

  CHK_ARRAY (buf, 512);

  return res;
}



/* Move head 2100U */

static SANE_Status
move_2100U (UMAX_Handle * scan, int distance, UMAX_Speed fine)
{


  unsigned char opc4[16] = {
    0x01, XXXX, XXXX, XXXX, 0x00, 0x00, 0x60, 0x2f, XXXX, XXXX,
    0x00, 0x00, 0x00, 0x80, XXXX, 0x00
  };
  unsigned char opb5[36] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0xf6, 0x79, 0xbf,
    0x06, 0x00, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x2a,
    0xe9, 0x68, 0xdf, 0x03, 0x1a, 0x00
  };
  unsigned char opb7[36] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x6e, 0xf6, 0x79, 0xbf,
    0x01, 0x00, 0x00, 0x00, 0x46, 0xa0, 0x00, 0x8b, 0x49, 0x2a,
    0xe9, 0x68, 0xdf, 0x03, 0x1a, 0x00
  };
  unsigned char ope[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff
  };
  unsigned char ope2[3] = {
    0x00, 0xff, 0xff
  };
  unsigned char buf[512];


  SANE_Status res;
  UMAX_Status_Byte s;

  SANE_Bool rev = distance < 0;
  int skip = (rev ? -distance : distance) - 1;

  DBG (9, "move: distance = %d, scanner_ypos = %d\n", distance,
       scan->scanner_ypos);

  PAD_ARRAY (buf, 512);

  if (distance == 0)
    return SANE_STATUS_GOOD;

  opc4[1] = skip << 6;
  opc4[2] = skip >> 2;
  opc4[3] = (rev ? 0x20 : 0x70) + ((skip >> 10) & 0x0f);
  opc4[9] = rev ? 0x01 : 0x05;

  if (fine == UMAX_FINE)
    {
      opc4[8]  = 0x2b;
      opc4[14] = 0xa4;
    }
  else
    {
      opc4[8]  = 0x15;
      opc4[14] = 0xac;
    }

  scan->scanner_ypos +=
    (fine == UMAX_FINE ? distance : 2 * distance + (rev ? -1 : 1));
  scan->scanner_ypos = (scan->scanner_ypos + (rev ? 0 : 3)) & ~3;

  CHK (cwrite (scan, CMD_2, 16, opc4, &s));
  CHK (cwrite (scan, CMD_8, 36, rev ? opb7 : opb5, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));
  DBG (10, "move: checkpoint 1: s = %d\n", s);

  CHK (csend (scan, CMD_0));

  if (rev)
    CHK (cwrite (scan, CMD_4, 3, ope2, &s))
  else
    CHK (cwrite (scan, CMD_4, 8, ope, &s));

  CHK (csend (scan, CMD_40));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (10, "move: checkpoint 2: s = %d\n", s);

  CHK (cread (scan, CMD_2, 0, NULL, &s));
  DBG (10, "move: checkpoint 3: s = %d\n", s);

  CHK (cread (scan, CMD_4, 512, buf, &s));

  CHK_ARRAY (buf, 512);

  return res;
}


/* Get pixel image 1220U */

static SANE_Status
get_pixels (UMAX_Handle * scan, unsigned char *op2, unsigned char *op8,
            unsigned char *op1, unsigned char *op4, int len, int zpos,
            unsigned char *buf)
{
  SANE_Status res;
  UMAX_Status_Byte s;

  DBG (9, "get_pixels: len = %d, zpos = %d\n", len, zpos);

  if (zpos == 0)
    CHK (csend (scan, CMD_0));

  CHK (cwrite (scan, CMD_2, 16, op2, &s));
  CHK (cwrite (scan, CMD_8, 35, op8, &s));
  CHK (cwrite (scan, CMD_1, 8, op1, &s));
  CHK (cread (scan, CMD_2, 0, NULL, &s));

  if (zpos == 1)
    CHK (csend (scan, CMD_0));

  CHK (cwrite (scan, CMD_4, 8, op4, &s));
  CHK (csend (scan, CMD_40));
  CHK (cread (scan, CMD_2, 0, NULL, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  CHK (cread (scan, CMD_4, len, buf, &s));
  return SANE_STATUS_GOOD;
}


/* Get pixel image 2100U */

static SANE_Status
get_pixels_2100U (UMAX_Handle * scan, unsigned char *op2, unsigned char *op8,
            unsigned char *op1, unsigned char *op4, int len, int zpos,
            unsigned char *buf)
{
  SANE_Status res;
  UMAX_Status_Byte s;

  DBG (9, "get_pixels: len = %d, zpos = %d\n", len, zpos);

  CHK (cwrite (scan, CMD_2, 16, op2, &s));
  CHK (cwrite (scan, CMD_8, 36, op8, &s));

  if (zpos == 1)
    CHK (cwritev (scan, CMD_1, 8, op1, &s))
  else
    CHK (cwrite (scan, CMD_1, 8, op1, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  if (zpos == 1)
    CHK (csend (scan, CMD_0));

  CHK (cwrite (scan, CMD_4, 8, op4, &s));
  CHK (csend (scan, CMD_40));
  CHK (cread (scan, CMD_2, 0, NULL, &s));

  CHK (cread (scan, CMD_2, 0, NULL, &s));

  CHK (cread (scan, CMD_4, len, buf, &s));
  return SANE_STATUS_GOOD;
}


/* This function locates the black stripe under scanner lid */

static int
locate_black_stripe (unsigned char *img, int w, int h)
{
  int epos, ecnt, x, y;
  unsigned char *p;

  epos = 0;
  ecnt = 0;
  p = img;
  for (x = 0; x < w; ++x, ++p)
    {
      int d, dmax = 0, dpos = 0;
      unsigned char *q = img + x;
      for (y = 1; y < h; ++y, q += w)
        {
          d = q[0] - q[w];
          if (d > dmax)
            {
              dmax = d;
              dpos = y;
            }
        }
      if (dmax > 0)
        {
          epos += dpos;
          ++ecnt;
        }
    }
  if (ecnt == 0)
    epos = 70;
  else
    epos = (epos + ecnt / 2) / ecnt;
  return epos;
}


/* To find the lowest head position 1220U */

static SANE_Status
find_zero (UMAX_Handle * scan)
{
  unsigned char opc3[16] = {
    0xb4, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2f, 0x2f, 0x05,
    0x00, 0x00, 0x00, 0x80, 0xa4, 0x00
  };
  unsigned char ope1[8] = {
    0x00, 0x00, 0x00, 0xaa, 0xcc, 0xee, 0x80, 0xff
  };
  unsigned char opb6[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0xfb, 0xc4, 0xe5,
    0x06, 0x00, 0x00, 0x60, 0x4d, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x13, 0x1a
  };
  unsigned char opd1[8] = {
    0x06, 0xf4, 0xff, 0x81, 0x3d, 0x00, 0x08, 0x00
  };

  SANE_Status res;
  int s;
  unsigned char *img;

  DBG (9, "find_zero:\n");

  img = malloc (54000);
  if (img == 0)
    {
      DBG (1, "out of memory (need 54000)\n");
      return SANE_STATUS_NO_MEM;
    }

  CHK (csend (scan, CMD_0));
  CHK (get_pixels (scan, opc3, opb6, opd1, ope1, 54000, 1, img));

#ifdef DEBUG_CALIBRATION
  {
    int w = 300, h = 180;
    FILE *f2 = fopen ("find_zero.pgm", "wb");
    fprintf (f2, "P5\n%d %d\n255\n", w, h);
    fwrite (img, 1, w * h, f2);
    fclose (f2);
  }
#endif

  s = locate_black_stripe (img, 300, 180);
  scan->scanner_yorg = scan->scanner_ypos + s + 64;
  scan->scanner_ypos += 180 + 3;
  scan->scanner_ypos &= ~3;

  free (img);
  return SANE_STATUS_GOOD;
}


/* To find the lowest head position 2100U */

static SANE_Status
find_zero_2100U (UMAX_Handle * scan)
{
  unsigned char opc3[16] = {
    0xb4, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2f, 0x2b, 0x05,
    0x00, 0x00, 0x00, 0x80, 0xa4, 0x00
  };
  unsigned char ope1[8] = {
    0x00, 0x00, 0x00, 0xaa, 0xcc, 0xee, 0x80, 0xff
  };
  unsigned char opb6[36] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0xfb, 0xc4, 0xe5,
    0x06, 0x00, 0x00, 0x60, 0x4d, 0xa0, 0x00, 0x8b, 0x49, 0x2a,
    0xe9, 0x68, 0xdf, 0x03, 0x1a, 0x00
  };
  unsigned char opd1[8] = {
    0x06, 0xf4, 0xff, 0x81, 0x1b, 0x00, 0x08, 0x00
  };

  SANE_Status res;
  int s;
  unsigned char *img;

  DBG (9, "find_zero:\n");

  img = malloc (54000);
  if (img == 0)
    {
      DBG (1, "out of memory (need 54000)\n");
      return SANE_STATUS_NO_MEM;
    }

  CHK (csend (scan, CMD_0));
  CHK (get_pixels_2100U (scan, opc3, opb6, opd1, ope1, 54000, 1, img));

#ifdef DEBUG_CALIBRATION
  {
    int w = 300, h = 180;
    FILE *f2 = fopen ("find_zero.pgm", "wb");
    fprintf (f2, "P5\n%d %d\n255\n", w, h);
    fwrite (img, 1, w * h, f2);
    fclose (f2);
  }
#endif

  s = locate_black_stripe (img, 300, 180);
  scan->scanner_yorg = scan->scanner_ypos + s + 64;
  scan->scanner_ypos += 180 + 3;
  scan->scanner_ypos &= ~3;

  free (img);
  return SANE_STATUS_GOOD;
}


/* Calibration 1220U */

/*
Format of caldata:

   5100 bytes of CCD calibration values
   5100 bytes of CCD calibration values
   5100 bytes of CCD calibration values
   256 bytes of gamma data for blue
   256 bytes of gamma data for green
   256 bytes of gamma data for red
   2 bytes of extra information

*/
static SANE_Status
get_caldata (UMAX_Handle * scan, int color)
{
  unsigned char opc9[16] = {
    XXXX, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x00, 0x17, 0x05,
    0xec, 0x4e, 0x0c, XXXX, 0xac
  };
  unsigned char opb11[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, 0xad, 0xa0, 0x49,
    0x06, 0x00, 0x00, XXXX, XXXX, 0xa0, 0x00, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, 0x93, 0x1b
  };

  unsigned char ope[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff
  };

  unsigned char opd4[8] = {
    0x06, 0xf4, 0xff, 0x81, 0x3d, 0x00, XXXX, XXXX
  };
  SANE_Status res;

  unsigned char *p;
  int h  = 66;
  int w  = color ? 3 * 5100 : 5100;
  int x0 = color ? 0 : 5100;
  int l  = w * h;
  int i, x, y;

  PAD_ARRAY (scan->caldata, 16070);

  DBG (9, "get_caldata: color = %d\n", color);

  p = malloc (l);
  if (p == 0)
    {
      DBG (1, "out of memory (need %d)\n", l);
      return SANE_STATUS_NO_MEM;
    }
  memset (scan->caldata, 0, 3 * 5100);

  CHK (csend (scan, CMD_0));
  opc9[0] = h + 4;
  if (color)
    {
      opc9[13]  = 0x03;
      opb11[23] = 0xc4;
      opb11[24] = 0x5c;
      opd4[6]   = 0x08;
      opd4[7]   = 0x00;
    }
  else
    {
      opc9[13]  = 0xc3;
      opb11[23] = 0xec;
      opb11[24] = 0x54;
      opd4[6]   = 0x0c;
      opd4[7]   = 0x40;
    }

  /* Do a test scan of the calibration strip (which is located
   * under the scanner's lid */

  CHK (get_pixels (scan, opc9, opb11, opd4, ope, l, 0, p));

#ifdef DEBUG_CALIBRATION
  {
    FILE *f2 = fopen ("calibration.pgm", "wb");
    fprintf (f2, "P5\n%d %d\n255\n", w, h);
    fwrite (p, 1, w * h, f2);
    fclose (f2);
  }
#endif

  scan->scanner_ypos += (h + 4) * 2 + 3;
  scan->scanner_ypos &= ~3;

  /* The following loop computes the gain for each of the CCD pixel
   * elements.
   */
  for (x = 0; x < w; ++x)
    {
      int t = 0, gn;
      double av, gain;

      for (y = 0; y < h; ++y)
        t += p[x + y * w];
      av = (double) t / h;
      gain = 250 / av;
      gn = (int) ((gain - 0.984) * 102.547 + 0.5);
      if (gn < 0)
        gn = 0;
      else if (gn > 255)
        gn = 255;
      scan->caldata[x + x0] = gn;
    }

  /* Gamma table for blue */

  for (i = 0; i < 256; ++i)
    scan->caldata[i + 3 * 5100 + 0] = i;

  /* Gamma table for green */

  for (i = 0; i < 256; ++i)
    scan->caldata[i + 3 * 5100 + 256] = i;

  /* Gamma table for red */

  for (i = 0; i < 256; ++i)
    scan->caldata[i + 3 * 5100 + 512] = i;

  free (p);

  CHK_ARRAY (scan->caldata, 16070);
  return SANE_STATUS_GOOD;
}

/* Calibration 2100U */

/*
Format of caldata:

   5100 bytes of CCD calibration values
   5100 bytes of CCD calibration values
   5100 bytes of CCD calibration values
   256 bytes of gamma data for blue
   256 bytes of gamma data for green
   256 bytes of gamma data for red
   2 bytes of extra information

*/
static SANE_Status
get_caldata_2100U (UMAX_Handle * scan, int color)
{
  unsigned char opc9[16] = {
    XXXX, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x00, 0x15, 0x05,
    XXXX, XXXX, XXXX, XXXX, 0xac, 0x00
  };
  unsigned char opb11[36] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0c, 0x00, 0x04,
    0x40, 0x01, 0x00, 0x00, 0x04, 0x00, 0x6e, XXXX, XXXX, 0x46,
    0x06, 0x00, 0x00, XXXX, XXXX, 0xa0, 0x00, 0x8b, 0x49, 0x2a,
    0xe9, 0x68, 0xdf, 0x83, XXXX, 0x00
  };
  unsigned char opd4[8] = {
    0x06, 0xf4, 0xff, 0x81, 0x1b, 0x00, XXXX, XXXX
  };
  unsigned char ope[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff
  };


/* default gamma translation table */
  unsigned char ggamma[256] = {
    0x00, 0x06, 0x0A, 0x0D, 0x10, 0x12, 0x14, 0x17, 0x19, 0x1B, 0x1D,
    0x1F, 0x21, 0x23, 0x24, 0x26, 0x28, 0x2A, 0x2B, 0x2D, 0x2E, 0x30,
    0x31, 0x33, 0x34, 0x36, 0x37, 0x39, 0x3A, 0x3B, 0x3D, 0x3E, 0x40,
    0x41, 0x42, 0x43, 0x45, 0x46, 0x47, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
    0x4F, 0x50, 0x51, 0x52, 0x53, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
    0x5B, 0x5C, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C,
    0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x86,
    0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x90,
    0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x97, 0x98, 0x99, 0x9A,
    0x9B, 0x9C, 0x9D, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA2, 0xA3,
    0xA4, 0xA5, 0xA6, 0xA7, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAC,
    0xAD, 0xAE, 0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB4, 0xB5,
    0xB6, 0xB7, 0xB8, 0xB8, 0xB9, 0xBA, 0xBB, 0xBB, 0xBC, 0xBD, 0xBE,
    0xBF, 0xBF, 0xC0, 0xC1, 0xC2, 0xC2, 0xC3, 0xC4, 0xC5, 0xC5, 0xC6,
    0xC7, 0xC8, 0xC8, 0xC9, 0xCA, 0xCB, 0xCB, 0xCC, 0xCD, 0xCE, 0xCE,
    0xCF, 0xD0, 0xD1, 0xD1, 0xD2, 0xD3, 0xD4, 0xD4, 0xD5, 0xD6, 0xD6,
    0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDB, 0xDC, 0xDC, 0xDD, 0xDE, 0xDE,
    0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE3, 0xE4, 0xE5, 0xE6, 0xE6,
    0xE7, 0xE8, 0xE8, 0xE9, 0xEA, 0xEA, 0xEB, 0xEC, 0xEC, 0xED, 0xEE,
    0xEF, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2, 0xF3, 0xF3, 0xF4, 0xF5, 0xF5,
    0xF6, 0xF7, 0xF7, 0xF8, 0xF9, 0xF9, 0xFA, 0xFB, 0xFB, 0xFC, 0xFD,
    0xFE, 0xFE, 0xFF
  };


  SANE_Status res;

  unsigned char *p;
  int h  = 66;
  int w  = color ? 3 * 5100 : 5100;
  int x0 = color ? 0 : 5100;
  int l  = w * h;
  int i, x, y;
  int t, gn;
  double av, pct;

  PAD_ARRAY (scan->caldata, 16070);

  DBG (9, "get_caldata: color = %d\n", color);

  p = malloc (l);
  if (p == 0)
    {
      DBG (1, "out of memory (need %d)\n", l);
      return SANE_STATUS_NO_MEM;
    }
  memset (scan->caldata, 0, 3 * 5100);

  CHK (csend (scan, CMD_0));
  CHK (csend (scan, CMD_0));

  opc9[0] = h + 4;

  if (color)
    {
      opc9[10]  = 0xb6;
      opc9[11]  = 0x3b;
      opc9[12]  = 0x0c;
      opc9[13]  = 0x03;
      opb11[17] = 0x7e;
      opb11[18] = 0xb0;
      opb11[23] = 0xc4;
      opb11[24] = 0x5c;
      opb11[34] = 0x1b;
      opd4[6]   = 0x0f;
      opd4[7]   = 0x40;
    }
  else
    {
      opc9[10]  = 0xa6;
      opc9[11]  = 0x2a;
      opc9[12]  = 0x08;
      opc9[13]  = 0xc2;
      opb11[17] = 0x7f;
      opb11[18] = 0xc0;
      opb11[23] = 0xec;
      opb11[24] = 0x54;
      opb11[34] = 0x1a;
      opd4[6]   = 0x06;
      opd4[7]   = 0x20;
    }

  /* Do a test scan of the calibration strip (which is located
   * under the scanner's lid */
  CHK (get_pixels_2100U (scan, opc9, opb11, opd4, ope, l, 0, p));

#ifdef DEBUG_CALIBRATION
  {
    FILE *f2 = fopen ("calibration.pgm", "wb");
    fprintf (f2, "P5\n%d %d\n255\n", w, h);
    fwrite (p, 1, w * h, f2);
    fclose (f2);
  }
#endif

  scan->scanner_ypos += (h + 4) * 2 + 3;
  scan->scanner_ypos &= ~3;

  /* The following loop computes the gain for each of the CCD pixel
   * elements.
   */
  for (x = 0; x < w; x++)
    {
      t = 0;
      for (y = 0; y < h; y++)
        t += p[x + y * w];
      av = (double) t / h;
      pct = 100.0 - (av * 100.0) / 250;
      gn = (int) (pct / 0.57);

      pct = gn;
      av = exp((-pct)/50)*2.5+0.9;
      gn = gn * av;


      if (gn < 0)
        gn = 0;
      else if (gn > 127)
        gn = 127;
      scan->caldata[x + x0] = gn;
    }

  /* Gamma table for blue */

  for (i = 0; i < 256; i++)
    scan->caldata[i + 3 * 5100 + 0] = ggamma[i];

  /* Gamma table for green */

  for (i = 0; i < 256; i++)
    scan->caldata[i + 3 * 5100 + 256] = ggamma[i];

  /* Gamma table for red */

  for (i = 0; i < 256; i++)
    scan->caldata[i + 3 * 5100 + 512] = ggamma[i];

  free (p);

  CHK_ARRAY (scan->caldata, 16070);
  return SANE_STATUS_GOOD;
}



/* Sends scan user parameters from frontend 1220U */

static SANE_Status
send_scan_parameters (UMAX_Handle * scan)
{
  SANE_Status res;
  UMAX_Status_Byte s;

  /* Appears to correspond to opscan in umax_pp_low.c */
  unsigned char opbgo[35] = {
    0x00, 0x00, 0xb0, 0x4f, 0xd8, 0xe7, 0xfa, 0x10, 0xef, 0xc4,
    0x3c, 0x71, 0x0f, 0x00, 0x04, 0x00, 0x6e, XXXX, XXXX, XXXX,
    0xc4, 0x7e, 0x00, XXXX, XXXX, 0xa0, 0x0a, 0x8b, 0x49, 0x4a,
    0xd0, 0x68, 0xdf, XXXX, 0x1a
  };

  /* Appears to correspond to opsc53 in umax_pp_low.c */
  unsigned char opcgo[16] = {
    XXXX, XXXX, XXXX, XXXX, 0xec, 0x03, XXXX, XXXX, XXXX, XXXX,
    0xec, 0x4e, XXXX, XXXX, XXXX
  };

  /* Appears to correspond to opsc04 in umax_pp_low.c */
  unsigned char opdgo[8] = {
    0x06, 0xf4, 0xff, 0x81, 0x3d, 0x00, XXXX, XXXX
  };

  unsigned char subsamp[9] = {
    0xff, 0xff, 0xaa, 0xaa, 0x88, 0x88, 0x88, 0x88, 0x80
  };

  const int xend =
    scan->xskip + scan->w * scan->xsamp + (scan->xsamp + 1) / 2;
  const int ytot = scan->hexp * scan->ysamp + 12;

  opbgo[17] = scan->xskip % 256;
  opbgo[18] = ((scan->xskip >> 8) & 0xf) + (xend << 4);
  opbgo[19] = xend >> 4;
  opbgo[33] = 0x33 + ((xend & 0x1000) >> 5) + ((scan->xskip & 0x1000) >> 6);

  /* bytes per line */

  opbgo[23] = scan->color ? 0xc6 : 0x77;
  opbgo[24] = scan->color ? 0x5b : 0x4a;

  /* Scan height */

  opcgo[0] = ytot;
  opcgo[1] = ((ytot >> 8) & 0x3f) + (scan->yskip << 6);
  opcgo[2] = scan->yskip >> 2;
  opcgo[3] = 0x50 + ((scan->yskip >> 10) & 0xf);

  /* This is what used to be here:

     opcgo[6] = bh == h? 0: 0x60;       // a guess

     I replaced it with what umax_pp_low.c uses, since it
     made more sense
   */
  opcgo[6]  = (scan->ydpi <= 300) ? 0x00 : 0x60;
  opcgo[8]  = (scan->ydpi <= 300) ? 0x17 : 0x2F;
  opcgo[9]  = (scan->ydpi >= 300) ? 0x05 : 0x07;
  opcgo[14] = (scan->ydpi == 600) ? 0xa4 : 0xac;

  opcgo[7]  = scan->color ? 0x2F : 0x40;
  opcgo[12] = scan->color ? 0x10 : 0x0C;
  opcgo[13] = scan->color ? 0x04 : 0xc3;

  opdgo[6]  = scan->color ? 0x88 : 0x8c;
  opdgo[7]  = scan->color ? 0x00 : 0x40;

  DBG (3, "send_scan_parameters: xskip = %d, yskip = %d\n", scan->xskip,
       scan->yskip);

  CHK (csend (scan, CMD_0));
  CHK (csend (scan, CMD_0));
  CHK (cwritev (scan, CMD_2, 16, opcgo, &s));
  CHK (cwritev (scan, CMD_8, 35, opbgo, &s));
  CHK (cwritev (scan, CMD_1, 8, opdgo, &s));
  CHK (cread (scan, CMD_2, 0, NULL, &s));
  DBG (4, "send_scan_parameters: checkpoint 1: s = %d\n", s);

  /* Loads the new calibration data (that was computed by get_caldata) into the
     scanner */

  scan->caldata[16068] = subsamp[scan->xsamp];
  scan->caldata[16069] = subsamp[scan->ysamp];
  CHK (cwrite (scan, CMD_4, 16070, scan->caldata, &s));

  CHK (csend (scan, CMD_40));
  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (4, "send_scan_parameters: checkpoint 2: s = %d\n", s);

  return SANE_STATUS_GOOD;
}

/* Sends scan user parameters from frontend 2100U */

static SANE_Status
send_scan_parameters_2100U (UMAX_Handle * scan)
{
  SANE_Status res;
  UMAX_Status_Byte s;
  int bpl;

  /* Appears to correspond to opscan in umax_pp_low.c */
  unsigned char opbgo[36] = {
    0x00, 0x00, 0xb0, 0x4f, 0xd8, 0xe7, 0xfa, 0x10, 0xef, 0xc4,
    0x3c, 0x71, 0x0f, 0x00, 0x04, 0x00, 0x6e, XXXX, XXXX, XXXX,
    0xc4, 0x7e, 0x00, XXXX, XXXX, 0xa0, 0x0a, 0x8b, 0x49, 0x2a,
    0xe9, 0x68, 0xdf, XXXX, 0x1a, 0x00
  };

  /* Appears to correspond to opsc53 in umax_pp_low.c */
  unsigned char opcgo[16] = {
    XXXX, XXXX, XXXX, XXXX, 0xec, 0x03, 0x60, XXXX, XXXX, XXXX,
    XXXX, XXXX, XXXX, XXXX, XXXX, 0x00
  };

  /* Appears to correspond to opsc04 in umax_pp_low.c */
  unsigned char opdgo[8] = {
    0x06, 0xf4, 0xff, 0x81, 0x1b, 0x00, XXXX, XXXX
  };

  unsigned char subsamp[9] = {
    0xff, 0xff, 0xaa, 0xaa, 0x88, 0x88, 0x88, 0x88, 0x80
  };

  const int xend =
    scan->xskip + scan->w * scan->xsamp + (scan->xsamp + 1) / 2;
  const int ytot = scan->hexp * scan->ysamp + 12;

  opbgo[17] = scan->xskip % 256;
  opbgo[18] = ((scan->xskip >> 8) & 0x0f) + (xend << 4);
  opbgo[19] = xend >> 4;
  opbgo[33] = 0x23 + ((xend & 0x1000) >> 5) + ((scan->xskip & 0x1000) >> 6);

  /* bytes per line */

  bpl = (scan->color ? 3 : 1) * scan->w * scan->xdpi;
  opbgo[23] = bpl % 256;
  opbgo[24] = 0x41 + ((bpl / 256) & 0x1f);

  /* Scan height */

  opcgo[0] = ytot;
  opcgo[1] = ((ytot >> 8) & 0x3f) + (scan->yskip << 6);
  opcgo[2] = (scan->yskip >> 2);
  opcgo[3] = 0x50 + ((scan->yskip >> 10) & 0x0f);


  opcgo[6]  = (scan->ydpi <= 300) ? 0x00 : 0x60;
  opcgo[8]  = (scan->ydpi <= 300) ? 0x17 : 0x2F;
  opcgo[9]  = (scan->ydpi >= 300) ? 0x05 : 0x07;
  opcgo[14] = (scan->ydpi == 600) ? 0xa4 : 0xac;


  opcgo[7]  = scan->color ? 0x2f : 0x40;
  opcgo[10] = scan->color ? 0xb6 : 0xa6;
  opcgo[11] = scan->color ? 0x3b : 0x2a;
  opcgo[12] = scan->color ? 0x0c : 0x08;
  opcgo[13] = scan->color ? 0x03 : 0xc2;

  opdgo[6]  = scan->color ? 0x8f : 0x86;
  opdgo[7]  = scan->color ? 0x40 : 0x20;

  DBG (3, "send_scan_parameters: xskip = %d, yskip = %d\n", scan->xskip,
       scan->yskip);

  CHK (csend (scan, CMD_0));
  CHK (csend (scan, CMD_0));
  CHK (cwritev (scan, CMD_2, 16, opcgo, &s));
  CHK (cwritev (scan, CMD_8, 36, opbgo, &s));
  CHK (cwritev (scan, CMD_1, 8, opdgo, &s));
  CHK (cread (scan, CMD_2, 0, NULL, &s));
  DBG (4, "send_scan_parameters: checkpoint 1: s = %d\n", s);

  /* Loads the new calibration data (that was computed by get_caldata) into the
     scanner */

  scan->caldata[16068] = subsamp[scan->xsamp];
  scan->caldata[16069] = subsamp[scan->ysamp];
  CHK (cwrite (scan, CMD_4, 16070, scan->caldata, &s));

  CHK (csend (scan, CMD_40));
  CHK (cread (scan, CMD_2, 0, NULL, &s));

  DBG (4, "send_scan_parameters: checkpoint 2: s = %d\n", s);

  return SANE_STATUS_GOOD;
}

/* Read raw data */

static SANE_Status
read_raw_data (UMAX_Handle * scan, unsigned char *data, int len)
{
  SANE_Status res;
  UMAX_Status_Byte s;

  CHK (cread (scan, CMD_2, 0, NULL, &s));
  CHK (cread (scan, CMD_4, len, data, &s));

  return SANE_STATUS_GOOD;
}

/* Read raw strip color */

static SANE_Status
read_raw_strip_color (UMAX_Handle * scan)
{
  /**
     yres = 75   => ydpi = 150 => ysamp = 2 => yoff_scale = 8
     yres = 150  => ydpi = 150 => ysamp = 1 => yoff_scale = 4
     yres = 300  => ydpi = 300 => ysamp = 1 => yoff_scale = 2
     yres = 600  => ydpi = 600 => ysamp = 1 => yoff_scale = 1
  */

  const int yoff_scale = 600 * scan->ysamp / scan->ydpi;
  const int linelen = 3 * scan->w;

  /*
     yoff_scale = 8  =>  roff = 5  * w,  goff = 1  * w,  boff = 0 * w, hextra = 1
     yoff_scale = 4  =>  roff = 8  * w,  goff = 4  * w,  boff = 0 * w, hextra = 2
     yoff_scale = 2  =>  roff = 14 * w,  goff = 7  * w,  boff = 0 * w, hextra = 4
     yoff_scale = 1  =>  roff = 26 * w,  goff = 13 * w,  boff = 0 * w, hextra = 8
   */

  const int hextra = 8 / yoff_scale;

  SANE_Status res;
  int lines_to_read = scan->hexp;

  DBG (9, "read_raw_strip_color: hexp = %d, bh = %d\n", scan->hexp, scan->bh);

  if (scan->maxh == -1)
    {
      DBG (10, "read_raw_strip_color: filling buffer for the first time\n");
      if (lines_to_read > scan->bh)
        lines_to_read = scan->bh;

      CHK (read_raw_data (scan, scan->p, lines_to_read * linelen));
      scan->maxh = lines_to_read - hextra;
    }
  else
    {
      DBG (10, "read_raw_strip_color: reading new rows into buffer\n");
      memmove (scan->p, scan->p + (scan->bh - hextra) * linelen,
               hextra * linelen);

      if (lines_to_read > (scan->bh - hextra))
        lines_to_read = scan->bh - hextra;

      CHK (read_raw_data
           (scan, scan->p + hextra * linelen, lines_to_read * linelen));
      scan->maxh = lines_to_read;
    }

  scan->hexp -= lines_to_read;
  scan->x = 0;
  scan->y = 0;

  return SANE_STATUS_GOOD;
}

/* Read raw strip grey */

static SANE_Status
read_raw_strip_gray (UMAX_Handle * scan)
{
  const int linelen = scan->w;

  SANE_Status res;

  int lines_to_read = scan->bh;

  DBG (9, "read_raw_strip_gray: hexp = %d\n", scan->hexp);

  if (lines_to_read > scan->hexp)
    lines_to_read = scan->hexp;
  scan->hexp -= lines_to_read;

  CHK (read_raw_data (scan, scan->p, lines_to_read * linelen));

  scan->maxh = lines_to_read;
  scan->x = 0;
  scan->y = 0;

  return SANE_STATUS_GOOD;
}


/* Read raw strip */

static SANE_Status
read_raw_strip (UMAX_Handle * scan)
{
  if (scan->color)
      return read_raw_strip_color (scan);
  else
      return read_raw_strip_gray (scan);
}

/* Set scan user pamaters Frontend */

static SANE_Status
UMAX_set_scan_parameters (UMAX_Handle * scan,
                          const int color,
                          const int xo,
                          const int yo,
                          const int w,
                          const int h, const int xres, const int yres)
{

  /* Validate the input parameters */

  int left = xo;
  int top = yo;
  int right = xo + w * 600 / xres;
  int bottom = yo + h * 600 / yres;

  DBG (2, "UMAX_set_scan_parameters:\n");
  DBG (2, "color = %d             \n", color);
  DBG (2, "xo    = %d, yo     = %d\n", xo, yo);
  DBG (2, "w     = %d, h      = %d\n", w, h);
  DBG (2, "xres  = %d, yres   = %d\n", xres, yres);
  DBG (2, "left  = %d, top    = %d\n", left, top);
  DBG (2, "right = %d, bottom = %d\n", right, bottom);

  if ((left < 0) || (right > UMAX_MAX_WIDTH))
    return SANE_STATUS_INVAL;

  if ((top < 0) || (bottom > UMAX_MAX_HEIGHT))
    return SANE_STATUS_INVAL;

  if (((right - left) < 10) || ((bottom - top) < 10))
    return SANE_STATUS_INVAL;

  if ((xres != 75) && (xres != 150) && (xres != 300) && (xres != 600))
    return SANE_STATUS_INVAL;

  if ((yres != 75) && (yres != 150) && (yres != 300) && (yres != 600))
    return SANE_STATUS_INVAL;

  /* If we get this far, begin initializing the data
     structure
   */

  scan->color = color;
  scan->w = w;
  scan->h = h;
  scan->xo = xo;
  scan->yo = yo;

  /*
     The scanner has a fixed X resolution of 600 dpi, but
     supports three choices for the Y resolution. We must
     choose an appropriate physical resolution and the
     corresponding sampling value.

     It is not clear to me why the choice depends on
     whether we are scanning in color or not, but the
     original code did this and I didn't want to mess
     with it.

     Physical X resolution choice:
     xres = 75   =>  xdpi = 600  (xsamp = 8)
     xres = 150  =>  xdpi = 600  (xsamp = 4)
     xres = 300  =>  xdpi = 600  (xsamp = 2)
     xres = 600  =>  xdpi = 600  (xsamp = 1)

     Physical Y resolution choice (if color):
     yres = 75    =>  ydpi = 150  (ysamp = 2)
     yres = 150   =>  ydpi = 150  (ysamp = 1)
     yres = 300   =>  ydpi = 300  (ysamp = 1)
     yres = 600   =>  ydpi = 600  (ysamp = 1)

     Physical Y resolution choice (if not color):
     yres = 75    =>  ydpi = 300  (ysamp = 4)
     yres = 150   =>  ydpi = 300  (ysamp = 2)
     yres = 300   =>  ydpi = 300  (ysamp = 1)
     yres = 600   =>  ydpi = 600  (ysamp = 1)
   */

  scan->xdpi = 600;
  if (yres <= 150 && color)
    scan->ydpi = 150;
  else if (yres > 300)
    scan->ydpi = 600;
  else
    scan->ydpi = 300;

  scan->xsamp = scan->xdpi / xres;
  scan->ysamp = scan->ydpi / yres;

  return SANE_STATUS_GOOD;
}

/* Start actual scan 1220U */

static SANE_Status
UMAX_start_scan (UMAX_Handle * scan)
{
  SANE_Status res;
  int linelen;
  int yd;

  DBG (3, "UMAX_start_scan called\n");

  if (scan->color)
    {
      const int yoff_scale = 600 * scan->ysamp / scan->ydpi;
      const int hextra = 8 / yoff_scale;

      linelen = 3 * scan->w;
      scan->hexp = scan->h + hextra;
    }
  else
    {
      linelen = scan->w;
      scan->hexp = scan->h;
    }

  scan->bh = BUFFER_SIZE / linelen;

  scan->p = malloc (scan->bh * linelen);
  if (scan->p == 0)
    return SANE_STATUS_NO_MEM;

  DBG (4, "UMAX_start_scan: bh = %d, linelen = %d\n", scan->bh, linelen);

  scan->maxh = -1;
  scan->done = 0;

  /* Initialize the scanner and position the scan head */

  CHK (umaxinit (scan));

  /* This scans in the black and white calibration strip that
   * is located under the scanner's lid. The scan of that strip
   * is used to pick correct values for the CCD calibration
   * values
   */

  scan->scanner_ypos = 0;
  CHK (move (scan, 196, UMAX_NOT_FINE));
  CHK (find_zero (scan));
  CHK (move (scan, scan->scanner_yorg - 232 - scan->scanner_ypos, UMAX_FINE));
  CHK (get_caldata (scan, scan->color));

  /* This moves the head back to the starting position */

  yd = scan->scanner_yorg + scan->yo - scan->scanner_ypos;
  if (yd < 0)
    CHK (move (scan, yd, UMAX_FINE));
  if (yd > 300)
    CHK (move (scan, (yd - 20) / 2, UMAX_NOT_FINE));
  yd = scan->scanner_yorg + scan->yo - scan->scanner_ypos;

  scan->yskip = yd / (600 / scan->ydpi);
  scan->xskip = scan->xo / (600 / scan->xdpi);

  /* Read in the first chunk of raw data */

  CHK (send_scan_parameters (scan));
  CHK (read_raw_strip (scan));

  DBG (4, "UMAX_start_scan successful\n");

  return SANE_STATUS_GOOD;
}


/* Start actual scan 2100U */

static SANE_Status
UMAX_start_scan_2100U (UMAX_Handle * scan)
{
  SANE_Status res;
  int linelen;
  int yd;

  DBG (3, "UMAX_start_scan called\n");

  if (scan->color)
    {
      const int yoff_scale = 600 * scan->ysamp / scan->ydpi;
      const int hextra = 8 / yoff_scale;

      linelen = 3 * scan->w;
      scan->hexp = scan->h + hextra;
    }
  else
    {
      linelen = scan->w;
      scan->hexp = scan->h;
    }

  scan->bh = BUFFER_SIZE / linelen;

  scan->p = malloc (scan->bh * linelen);
  if (scan->p == 0)
    return SANE_STATUS_NO_MEM;

  DBG (4, "UMAX_start_scan: bh = %d, linelen = %d\n", scan->bh, linelen);

  scan->maxh = -1;
  scan->done = 0;

  /* Initialize the scanner and position the scan head */

  CHK (umaxinit_2100U (scan));

  /* This scans in the black and white calibration strip that
   * is located under the scanner's lid. The scan of that strip
   * is used to pick correct values for the CCD calibration
   * values
   */

  scan->scanner_ypos = 0;
  CHK (move_2100U (scan, 196, UMAX_NOT_FINE));
  CHK (find_zero_2100U (scan));
  CHK (move_2100U (scan, scan->scanner_yorg - 232 - scan->scanner_ypos, UMAX_FINE));
  CHK (get_caldata_2100U (scan, scan->color));

  /* This moves the head back to the starting position */

  yd = scan->scanner_yorg + scan->yo - scan->scanner_ypos;
  if (yd < 0)
    CHK (move_2100U (scan, yd, UMAX_FINE));
  if (yd > 300)
    CHK (move_2100U (scan, (yd - 20) / 2, UMAX_NOT_FINE));
  yd = scan->scanner_yorg + scan->yo - scan->scanner_ypos;

  scan->yskip = yd / (600 / scan->ydpi);
  scan->xskip = scan->xo / (600 / scan->xdpi);

  /* Read in the first chunk of raw data */

  CHK (send_scan_parameters_2100U (scan));
  CHK (read_raw_strip (scan));

  DBG (4, "UMAX_start_scan successful\n");

  return SANE_STATUS_GOOD;
}

/* Set lamp state */

static SANE_Status
UMAX_set_lamp_state (UMAX_Handle * scan, UMAX_Lamp_State state)
{
  SANE_Status res;

  DBG (3, "UMAX_set_lamp_state: state = %d\n", (int) state);

  CHK (csend (scan, CMD_0));
  CHK (cwritev_opc1_lamp_ctrl (scan, state));
  return SANE_STATUS_GOOD;
}

/* Park head 1220U */

static SANE_Status
UMAX_park_head (UMAX_Handle * scan)
{
  SANE_Status res;
  UMAX_Status_Byte s;
  int i;

  DBG (3, "UMAX_park_head called\n");

  CHK (csend (scan, CMD_0));
  /* WARNING: Must call cwritev_opc1_lamp_ctrl before cwritev_opb3_restore,
   * otherwise the head moves the wrong way and makes ugly grinding noises. */

  CHK (cwritev_opc1_lamp_ctrl (scan, UMAX_LAMP_ON));
  CHK (cwritev_opb3_restore (scan));

  for (i = 0; i < 60; ++i)
    {
      CHK (cread (scan, CMD_2, 0, NULL, &s));
      DBG (4, "UMAX_park_head: s = %#x\n", s);
      if ((s & 0x40) != 0)
        break;
      DBG (4, "UMAX_park_head: sleeping\n");
      usleep (500000);
    }

  scan->scanner_ypos = 0;

  return SANE_STATUS_GOOD;
}


/* Park head 2100U */

static SANE_Status
UMAX_park_head_2100U (UMAX_Handle * scan)
{
  SANE_Status res;
  UMAX_Status_Byte s;
  int i;

  DBG (3, "UMAX_park_head called\n");

  CHK (csend (scan, CMD_0));
  /* WARNING: Must call cwritev_opc1_lamp_ctrl before cwritev_opb3_restore,
   * otherwise the head moves the wrong way and makes ugly grinding noises. */

  CHK (cwritev_opc1_lamp_ctrl (scan, UMAX_LAMP_ON));
  CHK (cwritev_opb3_restore_2100U (scan));

  for (i = 0; i < 60; ++i)
    {
      CHK (cread (scan, CMD_2, 0, NULL, &s));
      DBG (4, "UMAX_park_head: s = %#x\n", s);
      if ((s & 0x40) != 0)
        break;
      DBG (4, "UMAX_park_head: sleeping\n");
      usleep (500000);
    }

  /* CHK (csend (scan, CMD_0));
  CHK (csend (scan, CMD_0)); */

  scan->scanner_ypos = 0;

  return SANE_STATUS_GOOD;
}


/* Finish scan */

static SANE_Status
UMAX_finish_scan (UMAX_Handle * scan)
{
  DBG (3, "UMAX_finish_scan:\n");

  if (scan->p)
    free (scan->p);
  scan->p = NULL;
  return SANE_STATUS_GOOD;
}


/* RGB decoding for a color scan */

static SANE_Status
UMAX_get_rgb (UMAX_Handle * scan, unsigned char *rgb)
{

  if (scan->color)
    {
      const int linelen = 3 * scan->w;
      const int yoff_scale = 600 * scan->ysamp / scan->ydpi;
      const int roff = (8 / yoff_scale * 3 + 2) * scan->w;
      const int goff = (4 / yoff_scale * 3 + 1) * scan->w;
      const int boff = 0;

      unsigned char *base = scan->p + (scan->y * linelen) + scan->x;

      rgb[0] = base[roff];
      rgb[1] = base[goff];
      rgb[2] = base[boff];
    }
  else
    {
      const int linelen = scan->w;
      unsigned char *base = scan->p + (scan->y * linelen) + (scan->x);

      rgb[0] = base[0];
      rgb[1] = base[0];
      rgb[2] = base[0];
    }

  if (!(((scan->x + 1) == scan->w) && ((scan->y + 1) == scan->maxh)))
    {
      ++scan->x;
      if (scan->x == scan->w)
        {
          ++scan->y;
          scan->x = 0;
        }
      return SANE_STATUS_GOOD;
    }

  if (scan->hexp <= 0)
    {
      DBG (4, "UMAX_get_rgb: setting done flag\n");
      scan->done = 1;
      return SANE_STATUS_GOOD;
    }

  return read_raw_strip (scan);
}

/* Close device */

static SANE_Status
UMAX_close_device (UMAX_Handle * scan)
{
  DBG (3, "UMAX_close_device:\n");
  sanei_usb_close (scan->fd);
  return SANE_STATUS_GOOD;
}

/* Open device */

static SANE_Status
UMAX_open_device (UMAX_Handle * scan, const char *dev)
{
  SANE_Word vendor;
  SANE_Word product;
  SANE_Status res;

  DBG (3, "UMAX_open_device: `%s'\n", dev);

  res = sanei_usb_open (dev, &scan->fd);
  if (res != SANE_STATUS_GOOD)
    {
      DBG (1, "UMAX_open_device: couldn't open device `%s': %s\n", dev,
           sane_strstatus (res));
      return res;
    }

#ifndef NO_AUTODETECT
  /* We have opened the device. Check that it is a USB scanner. */
  if (sanei_usb_get_vendor_product (scan->fd, &vendor, &product) !=
      SANE_STATUS_GOOD)
    {
      DBG (1, "UMAX_open_device: sanei_usb_get_vendor_product failed\n");
      /* This is not a USB scanner, or SANE or the OS doesn't support it. */
      sanei_usb_close (scan->fd);
      scan->fd = -1;
      return SANE_STATUS_UNSUPPORTED;
    }

  /* Make sure we have a UMAX scanner */
  if (vendor != 0x1606)
    {
      DBG (1, "UMAX_open_device: incorrect vendor\n");
      sanei_usb_close (scan->fd);
      scan->fd = -1;
      return SANE_STATUS_UNSUPPORTED;
    }

  /* Now check whether it is a scanner we know about */
  switch (product)
    {
    case ASTRA_2000U:
      /* The UMAX Astra 2000U is only partially supported by
         this driver. Expect severe color problems! :)
       */
      DBG (1,
           "UMAX_open_device: Scanner is a 2000U. Expect color problems :)\n");
      scan->model = ASTRA_2000U;
      break;
     case ASTRA_2100U:
       scan->model = ASTRA_2100U;
      break;
    case ASTRA_1220U:
       scan->model = ASTRA_1220U;
      break;
    default:
      DBG (1, "UMAX_open_device: unknown product number\n");
      sanei_usb_close (scan->fd);
      scan->fd = -1;
      return SANE_STATUS_UNSUPPORTED;
    }
#endif

  res = csend (scan, CMD_0);
  if (res != SANE_STATUS_GOOD)
    UMAX_close_device (scan);
  CHK (res);

  res = xxxops (scan);
  if (res != SANE_STATUS_GOOD)
    UMAX_close_device (scan);
  CHK (res);

  return SANE_STATUS_GOOD;
}

/* Get scanner model name */

static const char *
UMAX_get_device_name (UMAX_Handle * scan)
{
  switch (scan->model)
    {
    case ASTRA_1220U:
      return "Astra 1220U";
    case ASTRA_2000U:
      return "Astra 2000U";
    case ASTRA_2100U:
      return "Astra 2100U";
    }
  return "Unknown";
}

/* End */
