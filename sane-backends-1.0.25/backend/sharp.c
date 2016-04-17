/* sane - Scanner Access Now Easy.

   Copyright (C) 1998, 1999
   Kazuya Fukuda, Abel Deuring based on BYTEC GmbH Germany
   Written by Helmut Koeberle previous Work on canon.c file from the 
   SANE package.

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

   This file implements a SANE backend for Sharp flatbed scanners.  */

/*
   Version 0.32
   changes to version 0.31:
   - support for JX320 added (Thanks to Isaac Wilcox for providind the
     patch)
    
   Version 0.31
   changes to version 0.30:
   - support for JX350 added (Thanks to Shuhei Tomita for providind the
     patch)
    
   changes to version 0.20
   - support for the proposed extended open function in sanei_scsi.c added
   - support for ADF and FSU (transparency adapter) added
   - simple sense handler added
   - preview added
   - added several missing statements "s->fd = -1;" after 
     "sanei_scsi_close(s->fd)" to error returns in sane_start();
   - maximum scan sizes are read from the scanner, if a JX330 or JX250
     is used. (this avoids the guessing of scan sizes for the JX330)
   - gamma table support added
   - "Fixed gamma selection (1.0/2.2)", available for JX330 and JX610,
     is now implemented for the JX250 by downloading a gamma table
   - changed the calls to free() and strdup() in sane_control_option to
     strcpy. 
     (I don't like too frequent unchecked malloc()s and strdups :) Abel)
   - cleaned up some quirks in option handling, eg, that "threshold"
     was initially enabled, while the initial scan mode is "color"
   - cleaned up setting SANE_INFO_RELOAD_OPTIONS and SANE_INFO_RELOAD_PARAMS
     bits in sane_control_option
   - bi-level color scans now give useful (8 bit) output
   - separate thresholds for red, green, blue (bi-level color scan) added
*/
#include "../include/sane/config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"

/* QUEUEDEBUG should be undefined unless you want to play
   with the sanei_scsi.c under Linux and/or with the Linux's SG driver,
   or your suspect problems with command queueing
*/
#if 0
#define QUEUEDEBUG
#define DEBUG
#ifdef DEBUG
#include <unistd.h>
#include <sys/time.h>
#endif
#endif

/* USE_FORK: fork a special reader process
*/

#ifdef HAVE_SYS_SHM_H
#ifndef HAVE_OS2_H
#define USE_FORK
#endif
#endif

#ifdef USE_FORK
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#endif /* USE_FORK */

/* xxx I'm not sure, if I understood the JX610 and JX330 manuals right,
   that the data for the SEND command should be in ASCII format...
   SEND commands with a data bock are used, if USE_CUSTOM_GAMMA
   and / or USE_COLOR_THRESHOLD are enabled.
   Abel
*/
#define USE_CUSTOM_GAMMA
#define USE_COLOR_THRESHOLD
/* enable a short list of some standard resolutions. XSane provides 
   its own resolution list; therefore its is generally not reasonable
   to enable this list, if you mainly using XSane. But it might be handy
   if you are working with xscanimage
*/
/* #define USE_RESOLUTION_LIST */ 

/* enable separate specification of resolution in X and Y direction.
   XSane will show the Y-resolution at a quite different place than
   the X-resolution
*/
/* #define USE_SEPARATE_Y_RESOLUTION */


#define BACKEND_NAME sharp
#include "../include/sane/sanei_backend.h"

#include <sharp.h>

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#define DEFAULT_MUD_JX610 25
#define DEFAULT_MUD_JX320 25
#define DEFAULT_MUD_JX330 1200
#define DEFAULT_MUD_JX250 1200

#define PIX_TO_MM(x, mud) ((x) * 25.4 / mud)
#define MM_TO_PIX(x, mud) ((x) * mud / 25.4)

#include "../include/sane/sanei_config.h"
#define SHARP_CONFIG_FILE "sharp.conf"

static int num_devices = 0;
static SHARP_Device *first_dev = NULL;
static SHARP_Scanner *first_handle = NULL;

typedef enum
  {
    MODES_LINEART  = 0,
    MODES_GRAY,
    MODES_LINEART_COLOR,
    MODES_COLOR
  }
Modes;

#define M_LINEART            SANE_VALUE_SCAN_MODE_LINEART
#define M_GRAY               SANE_VALUE_SCAN_MODE_GRAY
#define M_LINEART_COLOR      SANE_VALUE_SCAN_MODE_COLOR_LINEART
#define M_COLOR              SANE_VALUE_SCAN_MODE_COLOR
static const SANE_String_Const mode_list[] =
{
  M_LINEART, M_GRAY, M_LINEART_COLOR, M_COLOR,
  0
};

#define M_BILEVEL        "none"
#define M_BAYER          "Dither Bayer"
#define M_SPIRAL         "Dither Spiral"
#define M_DISPERSED      "Dither Dispersed"
#define M_ERRDIFFUSION   "Error Diffusion"

static const SANE_String_Const halftone_list[] =
{
  M_BILEVEL, M_BAYER, M_SPIRAL, M_DISPERSED, M_ERRDIFFUSION,
  0
};

#define LIGHT_GREEN "green"
#define LIGHT_RED   "red"
#define LIGHT_BLUE  "blue"
#define LIGHT_WHITE "white"

#define MAX_RETRIES 50

static const SANE_String_Const light_color_list[] =
{
  LIGHT_GREEN, LIGHT_RED, LIGHT_BLUE, LIGHT_WHITE,
  0
};

/* possible values for ADF/FSU selection */
static SANE_String use_adf = "Automatic Document Feeder";
static SANE_String use_fsu = "Transparency Adapter";
static SANE_String use_simple = "Flatbed";

/* auto selection of ADF and FSU, as described in the JX330 manual, 
   is a nice idea -- but I assume that the possible scan window 
   sizes depend not only for the JX250, but also for JX330 on the 
   usage of ADF or FSU. Thus, the user might be able to select scan
   windows of an "illegal" size, which would have to be automatically
   corrected, and I don't see, how the user could be informed about 
   this "window clipping". More important, I don't see, how the 
   frontend could be informed that the ADF is automatically enabled.
   
   Insert a "#define ALLOW_AUTO_SELECT_ADF", if you want to play
   with this feature.
*/
#ifdef ALLOW_AUTO_SELECT_ADF
static SANE_String_Const use_auto = "AutoSelection";
#endif

#define HAVE_FSU 1
#define HAVE_ADF 2

/* The follow #defines are used in SHARP_Scanner.adf_fsu_mode 
   and as indexes for the arrays x_ranges, y_ranges in SHARP_Device 
*/
#define SCAN_SIMPLE 0
#define SCAN_WITH_FSU 1
#define SCAN_WITH_ADF 2
#ifdef ALLOW_AUTO_SELECT_ADF
#define SCAN_ADF_FSU_AUTO 3
#endif
#define LOAD_PAPER 1
#define UNLOAD_PAPER 0

#define PAPER_MAX  10
#define W_LETTER "11\"x17\""
#define INVOICE  "8.5\"x5.5\""
static const SANE_String_Const paper_list_jx610[] =
{
  "A3", "A4", "A5", "A6", "B4", "B5",
  W_LETTER, "Legal", "Letter", INVOICE,
  0
};

static const SANE_String_Const paper_list_jx330[] =
{
  "A4", "A5", "A6", "B5",
  0
};

#define GAMMA10    "1.0"
#define GAMMA22    "2.2"

static const SANE_String_Const gamma_list[] =
{
  GAMMA10, GAMMA22,
  0
};

#if 0
#define SPEED_NORMAL    "Normal"
#define SPEED_FAST      "Fast"
static const SANE_String_Const speed_list[] =
{
  SPEED_NORMAL, SPEED_FAST,
  0
};
#endif

#ifdef USE_RESOLUTION_LIST
#define RESOLUTION_MAX_JX610 8
static const SANE_String_Const resolution_list_jx610[] =
{
  "50", "75", "100", "150", "200", "300", "400", "600", "Select",
  0
};

#define RESOLUTION_MAX_JX250 7
static const SANE_String_Const resolution_list_jx250[] =
{
  "50", "75", "100", "150", "200", "300", "400", "Select",
  0
};
#endif

#define EDGE_NONE    "None"
#define EDGE_MIDDLE  "Middle"
#define EDGE_STRONG  "Strong"
#define EDGE_BLUR    "Blur"
static const SANE_String_Const edge_emphasis_list[] =
{
  EDGE_NONE, EDGE_MIDDLE, EDGE_STRONG, EDGE_BLUR,
  0
};

#ifdef USE_CUSTOM_GAMMA
static const SANE_Range u8_range =
  {
      0,				/* minimum */
    255,				/* maximum */
      0				/* quantization */
  };
#endif

static SANE_Status
sense_handler(int __sane_unused__ fd, u_char *sense_buffer, void *s)
{
  int sense_key;
  SHARP_Sense_Data *sdat = (SHARP_Sense_Data *) s;
  
#define add_sense_code sense_buffer[12]
#define add_sense_qual sense_buffer[13]

  memcpy(sdat->sb, sense_buffer, 16);
  
  DBG(10, "sense code: %02x %02x %02x %02x %02x %02x %02x %02x "
          "%02x %02x %02x %02x %02x %02x %02x %02x\n",
          sense_buffer[0], sense_buffer[1], sense_buffer[2], sense_buffer[3], 
          sense_buffer[4], sense_buffer[5], sense_buffer[6], sense_buffer[7], 
          sense_buffer[8], sense_buffer[9], sense_buffer[10], sense_buffer[11], 
          sense_buffer[12], sense_buffer[13], sense_buffer[14], sense_buffer[15]);

  sense_key = sense_buffer[2] & 0x0F;
  /* do we have additional information ? */
  if (sense_buffer[7] >= 5)
    {
      if (sdat->model == JX610)
        {
          /* The JX610 uses somewhat different error codes */
          switch (add_sense_code)
            {
              case 0x04:
                DBG(5, "error: scanner not ready\n");
                return SANE_STATUS_IO_ERROR;
              case 0x08:
                DBG(5, "error: scanner communication failure (time out?)\n");
                return SANE_STATUS_IO_ERROR;
              case 0x1A:
                DBG(10, "error: parameter list length error\n");
                return SANE_STATUS_IO_ERROR;
              case 0x20:
                DBG(10, "error: invalid command code\n");
                return SANE_STATUS_IO_ERROR;
              case 0x24:
                DBG(10, "error: invalid field in CDB\n");
                return SANE_STATUS_IO_ERROR;
              case 0x25:
                DBG(10, "error: LUN not supported\n");
                return SANE_STATUS_IO_ERROR;
              case 0x26:
                DBG(10, "error: invalid field in parameter list\n");
                return SANE_STATUS_IO_ERROR;
              case 0x29:
                DBG(10, "note: reset occured\n");
                return SANE_STATUS_GOOD;
              case 0x2a:
                DBG(10, "note: mode parameter change\n");
                return SANE_STATUS_GOOD;
              case 0x37:
                DBG(10, "note: rounded parameter\n");
                return SANE_STATUS_GOOD;
              case 0x39:
                DBG(10, "error: saving parameter not supported\n");
                return SANE_STATUS_IO_ERROR;
              case 0x47:
                DBG(10, "SCSI parity error\n");
                return SANE_STATUS_IO_ERROR;
              case 0x48:
                DBG(10, "initiator detected error message received\n");
                return SANE_STATUS_IO_ERROR;
              case 0x60:
                DBG(1, "error: lamp failure\n");
                return SANE_STATUS_IO_ERROR;
              case 0x62:
                DBG(1, "scan head positioning error\n");
                return SANE_STATUS_IO_ERROR;
            }
          
        }
      else if (sdat->model == JX250 || sdat->model == JX330 ||
	       sdat->model == JX350 || sdat->model == JX320)
        {
          switch (sense_key)
            {
              case 0x02: /* not ready */
                switch (add_sense_code)
                  {
                    case 0x80:
                      switch (add_sense_qual)
                        {
                          case 0:
                            DBG(1, "Scanner not ready: ADF cover open\n");
                            if (sdat->complain_on_errors & COMPLAIN_ON_ADF_ERROR)
                              return SANE_STATUS_COVER_OPEN;
                            else
                              return SANE_STATUS_GOOD;
                          case 1:
                            DBG(1, "Scanner not ready: ADF maintenance "
                                   "cover open\n");
                            if (sdat->complain_on_errors & COMPLAIN_ON_ADF_ERROR)
                              return SANE_STATUS_COVER_OPEN;
                            else
                              return SANE_STATUS_GOOD;
                          default:
                            DBG(5, "Scanner not ready: undocumented reason\n");
                            return SANE_STATUS_IO_ERROR;
                        }
                    case 0x81:
                      /* NOT TESTED -- I don't have a FSU */
                      switch (add_sense_qual)
                        {
                          case 0:
                            DBG(1, "Scanner not ready: FSU cover open\n");
                            if (sdat->complain_on_errors & COMPLAIN_ON_FSU_ERROR)
                              return SANE_STATUS_COVER_OPEN;
                            else
                              return SANE_STATUS_GOOD;
                          case 1:
                            DBG(1, "Scanner not ready: FSU light dispersion "
                                   "error\n");
                            if (sdat->complain_on_errors & COMPLAIN_ON_FSU_ERROR)
                              {
                                return SANE_STATUS_IO_ERROR;
                              }
                            else
                              return SANE_STATUS_GOOD;
                          default:
                            DBG(5, "Scanner not ready: undocumented reason\n");
                            return SANE_STATUS_IO_ERROR;
                        }
                    default:
                      DBG(5, "Scanner not ready: undocumented reason\n");
                      return SANE_STATUS_IO_ERROR;
                  }
              case 0x03: /* medium error */
                switch (add_sense_code)
                  {
                    case 0x3a:
                      DBG(1, "ADF is empty\n");
                            if (sdat->complain_on_errors & COMPLAIN_ON_ADF_ERROR)
                              return SANE_STATUS_NO_DOCS;
                            else
                              return SANE_STATUS_GOOD;
                    case 0x53:
                      DBG(1, "ADF paper jam\n"
                             "Open and close the maintenance cover to clear "
                             "this error\n");
                            if (sdat->complain_on_errors & COMPLAIN_ON_ADF_ERROR)
                              return SANE_STATUS_JAMMED;
                            else
                              return SANE_STATUS_GOOD;
                    default:
                      DBG(5, "medium error: undocumented reason\n");
                      return SANE_STATUS_IO_ERROR;
                  }
              case 0x04: /* hardware error */
                switch (add_sense_code)
                  {
                    case 0x08:
                      DBG(1, "hardware error: scanner communication failed\n");
                      return SANE_STATUS_IO_ERROR;
                    case 0x60:
                      DBG(1, "hardware error: lamp failure\n");
                      return SANE_STATUS_IO_ERROR;
                    case 0x62:
                      DBG(1, "hardware error: scan head positioning failed\n");
                      return SANE_STATUS_IO_ERROR;
                    default:
                      DBG(1, "general hardware error\n");
                      return SANE_STATUS_IO_ERROR;
                  }
              case 0x05: /* illegal request */
                DBG(10, "error: illegal request\n");
                return SANE_STATUS_IO_ERROR;
              case 0x06: /* unit attention */
                switch (add_sense_code)
                  {
                    case 0x29:
                      DBG(5, "unit attention: reset occured\n");
                      return SANE_STATUS_GOOD;
                    case 0x2a:
                      DBG(5, "unit attention: parameter changed by "
                             "another initiator\n");
                      return SANE_STATUS_IO_ERROR;
                    default:
                      DBG(5, "unit attention: exact reason not documented\n");
                      return SANE_STATUS_IO_ERROR;
                  }
              case 0x09: /* data remains */
                DBG(5, "error: data remains\n");
                return SANE_STATUS_IO_ERROR;
              default:
                DBG(5, "error: sense code not documented\n");
                return SANE_STATUS_IO_ERROR;
            }
        }
    }
  return SANE_STATUS_IO_ERROR;
}

static SANE_Status
test_unit_ready (int fd)
{
  static u_char cmd[] = {TEST_UNIT_READY, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< test_unit_ready ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

#if 0
static SANE_Status
request_sense (int fd, void *sense_buf, size_t *sense_size)
{
  static u_char cmd[] = {REQUEST_SENSE, 0, 0, 0, SENSE_LEN, 0};
  SANE_Status status;
  DBG (11, "<< request_sense ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), sense_buf, sense_size);

  DBG (11, ">>\n");
  return (status);
}
#endif

static SANE_Status
inquiry (int fd, void *inq_buf, size_t *inq_size)
{
  static u_char cmd[] = {INQUIRY, 0, 0, 0, INQUIRY_LEN, 0};
  SANE_Status status;
  DBG (11, "<< inquiry ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), inq_buf, inq_size);

  DBG (11, ">>\n");
  return (status);
}

static SANE_Status
mode_select_mud (int fd, int mud)
{
  static u_char cmd[6 + MODEPARAM_LEN] = 
                        {MODE_SELECT6, 0x10, 0, 0, MODEPARAM_LEN, 0};
  mode_select_param *mp;
  SANE_Status status;
  DBG (11, "<< mode_select_mud ");

  mp = (mode_select_param *)(cmd + 6);
  memset (mp, 0, MODEPARAM_LEN);
  mp->page_code = 3;
  mp->page_length = 6;
  mp->mud[0] = mud >> 8;
  mp->mud[1] = mud & 0xFF;

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

static SANE_Status
mode_select_adf_fsu (int fd, int mode)
{
  static u_char cmd[6 + MODE_SUBDEV_LEN] = 
                        {MODE_SELECT6, 0x10, 0, 0, MODE_SUBDEV_LEN, 0};
  mode_select_subdevice *mp;
  SANE_Status status;
  DBG (11, "<< mode_select_adf_fsu ");

  mp = (mode_select_subdevice *)(cmd + 6);
  memset (mp, 0, MODE_SUBDEV_LEN);
  mp->page_code = 0x20;
  mp->page_length = 26;
  switch (mode)
    {
      case SCAN_SIMPLE:
        mp->a_mode = 0x40;
        mp->f_mode = 0x40;
        break;
      case SCAN_WITH_FSU:
        mp->a_mode = 0;
        mp->f_mode = 0x40;
        break;
      case SCAN_WITH_ADF:
        mp->a_mode = 0x40;
        mp->f_mode = 0;
        break;
#ifdef ALLOW_AUTO_SELECT_ADF
      case: SCAN_ADF_FSU_AUTO:
        mp->a_mode = 0;
        mp->f_mode = 0;
        break;
#endif
    }

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

static SANE_Status wait_ready(int fd);

static SANE_Status
object_position(int fd, int load)
{
  static u_char cmd[] = {OBJECT_POSITION, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< object_position ");

  cmd[1] = load;

  wait_ready(fd);
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

#if 0
static SANE_Status
reserve_unit (int fd)
{
  static u_char cmd[] = {RESERVE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< reserve_unit ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}
#endif

#if 0
static SANE_Status
release_unit (int fd)
{
  static u_char cmd[] = {RELEASE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< release_unit ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}
#endif

static SANE_Status
mode_sense (int fd, void *modeparam_buf, size_t * modeparam_size, 
            int page)
{
  static u_char cmd[6];
  SANE_Status status;
  DBG (11, "<< mode_sense ");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1a;
  cmd[2] = page;
  cmd[4] = *modeparam_size;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), modeparam_buf, 
			   modeparam_size);

  DBG (11, ">>\n");
  return (status);
}

static SANE_Status
scan (int fd)
{
  static u_char cmd[] = {SCAN, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< scan ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}

#if 0
static SANE_Status
send_diagnostics (int fd)
{
  static u_char cmd[] = {SEND_DIAGNOSTIC, 0x04, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< send_diagnostics ");

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);
}
#endif

static SANE_Status
send (int fd, SHARP_Send * ss)
{
  static u_char cmd[] = {SEND, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  SANE_Status status;
  DBG (11, "<< send ");

  cmd[2] = ss->dtc;
  cmd[4] = ss->dtq >> 8;
  cmd[5] = ss->dtq;
  cmd[6] = ss->length >> 16;
  cmd[7] = ss->length >>  8;
  cmd[8] = ss->length >>  0;

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);

}

static SANE_Status
set_window (int fd, window_param *wp, int len)
{
  static u_char cmd[10 + WINDOW_LEN] = 
                        {SET_WINDOW, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  window_param *winp;
  SANE_Status status;
  DBG (11, "<< set_window ");

  cmd[8] = len;
  winp = (window_param *)(cmd + 10);
  memset (winp, 0, WINDOW_LEN);
  memcpy (winp, wp, len);
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, ">>\n");
  return (status);

}

static SANE_Status
get_window (int fd, void *buf, size_t * buf_size)
{

  static u_char cmd[10] = {GET_WINDOW, 0, 0, 0, 0, 0, 0, 0, WINDOW_LEN, 0};
  SANE_Status status;
  DBG (11, "<< get_window ");

  cmd[8] = *buf_size;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (11, ">>\n");
  return (status);
}

#ifdef USE_FORK

/* the following four functions serve simply the purpose
   to avoid "over-optimised" code when reader_process and 
   read_data wait for the buffer to become ready. The simple 
   while-loops in these functions which check the buffer 
   status may be optimised so that the machine code only 
   operates with registers instead of using the variable 
   values stored in memory. (This is only a workaround - 
   it would be better to set a compiler pragma, which ensures
   that the program looks into the RAM in these while loops --
   but unfortunately I could not find appropriate information
   about this at least for gcc, not to speak about other 
   compilers... 
   Abel)
*/

static int
cancel_requested(SHARP_Scanner *s)
{
  return s->rdr_ctl->cancel;
}

static SANE_Status 
rdr_status(SHARP_Scanner *s)
{
  return s->rdr_ctl->status;
}

static int
buf_status(SHARP_shmem_ctl *s)
{
  return s->shm_status;
}

static int
reader_running(SHARP_Scanner *s)
{
  return s->rdr_ctl->running;
}

static int
reader_process(SHARP_Scanner *s)
{
  SANE_Status status;
  sigset_t sigterm_set;
  static u_char cmd[] = {READ, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int full_count = 0, counted;
  size_t waitindex, cmdindex;
  size_t bytes_to_queue;
  size_t nread;
  size_t max_bytes_per_read;
  int max_queue;
  int i, retries = MAX_RETRIES;
  SHARP_shmem_ctl *bc;

  s->rdr_ctl->running = 1;
  DBG(11, "<< reader_process\n");

  sigemptyset (&sigterm_set);
  
  bytes_to_queue = s->bytes_to_read;
  
  /* it seems that some carriage stops can be avoided with the
     JX-250, if the data of an integral number of scan lines is
     read with one SCSI command
  */
  max_bytes_per_read = s->dev->info.bufsize / s->params.bytes_per_line;
  if (max_bytes_per_read)
    max_bytes_per_read *= s->params.bytes_per_line;
  else
    /* this is a really tiny buffer..*/
    max_bytes_per_read = s->dev->info.bufsize;
  
  /*  wait_ready(s->fd); */
  
  if (s->dev->info.queued_reads <= s->dev->info.buffers)
    max_queue = s->dev->info.queued_reads;
  else
    max_queue = s->dev->info.buffers;
  if (max_queue <= 0)
    max_queue = 1;
  for (i = 0; i < max_queue; i++)
    {
      bc = &s->rdr_ctl->buf_ctl[i];
      if (bytes_to_queue)
        {
          nread = bytes_to_queue;
          if (nread > max_bytes_per_read)
            nread = max_bytes_per_read;
          bc->used = nread;
          cmd[6] = nread >> 16;
          cmd[7] = nread >> 8;
          cmd[8] = nread;
#ifdef QUEUEDEBUG
          DBG(2, "reader: req_enter...\n");
#endif
          status = sanei_scsi_req_enter (s->fd, cmd, sizeof (cmd), 
                     bc->buffer, 
                    &bc->used, 
                    &bc->qid);
#ifdef QUEUEDEBUG
          DBG(2, "reader: req_enter ok\n");
#endif
          if (status != SANE_STATUS_GOOD)
            {
              DBG(1, "reader_process: read command failed: %s", 
                  sane_strstatus(status));
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
              sanei_scsi_req_flush_all_extended(s->fd);
#else
               sanei_scsi_req_flush_all();
#endif
              s->rdr_ctl->status = status;
              s->rdr_ctl->running = 0;
              return 2;
            }
          bc->shm_status = SHM_BUSY;
          bc->nreq = bc->used;
          bytes_to_queue -= bc->nreq;
        }
      else
        {
          bc->used = 0;
          bc->shm_status = SHM_EMPTY;
        }
    }
  waitindex = 0;
  cmdindex = i % s->dev->info.buffers;

  while(s->bytes_to_read > 0) 
    {
      if (cancel_requested(s)) 
        {
#ifdef QUEUEDEBUG
          DBG(2, "reader: flushing requests...\n");
#endif
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
          sanei_scsi_req_flush_all_extended(s->fd);
#else
          sanei_scsi_req_flush_all();
#endif
#ifdef QUEUEDEBUG
          DBG(2, "reader: flushing requests ok\n");
#endif
          s->rdr_ctl->cancel = 0;
          s->rdr_ctl->status = SANE_STATUS_CANCELLED;
          s->rdr_ctl->running = 0;
          DBG(11, " reader_process (cancelled) >>\n");
          return 1;
        }

      bc = &s->rdr_ctl->buf_ctl[waitindex];
      if (bc->shm_status == SHM_BUSY) 
        {
#ifdef DEBUG
          {
            struct timeval t;
            gettimeofday(&t, 0);
            DBG(2, "rd: waiting for data %li.%06li\n", t.tv_sec, t.tv_usec);
          }
#endif
#ifdef QUEUEDEBUG
          DBG(2, "reader: req_wait...\n");
#endif
          status = sanei_scsi_req_wait(bc->qid);
#ifdef QUEUEDEBUG
          DBG(2, "reader: req_wait ok\n");
#endif
#ifdef DEBUG
          {
            struct timeval t;
            gettimeofday(&t, 0);
            DBG(2, "rd: data received    %li.%06li\n", t.tv_sec, t.tv_usec);
          }
#endif
          if (status == SANE_STATUS_DEVICE_BUSY && retries) 
            {
              bc->used = 0;
              retries--;
              DBG(11, "reader: READ command returned BUSY\n");
              status = SANE_STATUS_GOOD;
              usleep(10000);
            }
          else if (status != SANE_STATUS_GOOD)
            {
              DBG(1, "reader_process: read command failed: %s\n", 
                  sane_strstatus(status));
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
              sanei_scsi_req_flush_all_extended(s->fd);
#else
              sanei_scsi_req_flush_all();
#endif
              s->rdr_ctl->status = status;
              s->rdr_ctl->running = 0;
              return 2;
            }
          else 
            {
              retries = MAX_RETRIES;
            }
#if 1
          s->bytes_to_read -= bc->used;
          bytes_to_queue += bc->nreq - bc->used;
#else
          /* xxxxxxxxxxxxxxxxxxx TEST xxxxxxxxxxxxxxx */
          s->bytes_to_read -= bc->nreq;
          /* memset(bc->buffer + bc->used, 0, bc->nreq - bc->used); */
          bc->used = bc->nreq;
          /* bytes_to_queue += bc->nreq - bc->used; */
          DBG(1, "btr: %i btq: %i nreq: %i nrcv: %i\n", 
            s->bytes_to_read, bytes_to_queue, bc->nreq, bc->used);
#endif
          bc->start = 0;
          bc->shm_status = SHM_FULL;

          waitindex++;
          if (waitindex == s->dev->info.buffers)
            waitindex = 0;

        }
      
      if (bytes_to_queue)
        {
          /* wait until the next buffer is completely read via read_data */
          bc = &s->rdr_ctl->buf_ctl[cmdindex];
          counted = 0;
          while (buf_status(bc) != SHM_EMPTY)
            {
              if (!counted)
                {
                  counted = 1;
                  full_count++;
                }
              if (cancel_requested(s))
                {
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
                  sanei_scsi_req_flush_all_extended(s->fd);
#else
                  sanei_scsi_req_flush_all();
#endif
                  s->rdr_ctl->cancel = 0;
                  s->rdr_ctl->status = SANE_STATUS_CANCELLED;
                  s->rdr_ctl->running = 0;
                  DBG(11, " reader_process (cancelled) >>\n");
                  return 1;
                }
            }

          nread = bytes_to_queue;
          if (nread > max_bytes_per_read)
            nread = max_bytes_per_read;
          bc->used = nread;
          cmd[6] = nread >> 16;
          cmd[7] = nread >> 8;
          cmd[8] = nread;
          status = sanei_scsi_req_enter (s->fd, cmd, sizeof (cmd), 
                    bc->buffer, &bc->used, &bc->qid);
          if (status != SANE_STATUS_GOOD)
            {
              DBG(1, "reader_process: read command failed: %s", 
                  sane_strstatus(status));
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
              sanei_scsi_req_flush_all_extended(s->fd);
#else
              sanei_scsi_req_flush_all();
#endif
              s->rdr_ctl->status = status;
              s->rdr_ctl->running = 0;
              return 2;
            }
          bc->shm_status = SHM_BUSY;
          bc->nreq = nread;
          bytes_to_queue -= nread;
          
          cmdindex++;
          if (cmdindex == s->dev->info.buffers)
            cmdindex = 0;
        }
      
      if (cancel_requested(s)) 
        {
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
          sanei_scsi_req_flush_all_extended(s->fd);
#else
          sanei_scsi_req_flush_all();
#endif
          s->rdr_ctl->cancel = 0;
          s->rdr_ctl->status = SANE_STATUS_CANCELLED;
          s->rdr_ctl->running = 0;
          DBG(11, " reader_process (cancelled) >>\n");
          return 1;
        }
    }
      
  DBG(1, "buffer full conditions: %i\n", full_count);
  DBG(11, " reader_process>>\n");

  s->rdr_ctl->running = 0;
  return 0;
}

static SANE_Status
read_data (SHARP_Scanner *s, SANE_Byte *buf, size_t * buf_size)
{
  size_t copysize, copied = 0;
  SHARP_shmem_ctl *bc;
  
  DBG(11, "<< read_data ");

  bc = &s->rdr_ctl->buf_ctl[s->read_buff];

  while (copied < *buf_size)
    {
      /* wait until the reader process delivers data or a scanner error occurs: */
      while (   buf_status(bc) != SHM_FULL
             && rdr_status(s) == SANE_STATUS_GOOD) 
        {
          usleep(10); /* could perhaps be longer. make this user configurable?? */
        }

      if (rdr_status(s) != SANE_STATUS_GOOD)
        {
          return rdr_status(s);
          DBG(11, ">>\n");
        }

      copysize = bc->used - bc->start;
      
      if (copysize > *buf_size - copied )
        copysize = *buf_size - copied;
        
      memcpy(buf, &(bc->buffer[bc->start]), copysize);

      copied += copysize;
      buf = &buf[copysize];

      bc->start += copysize;
      if (bc->start >= bc->used)
        {
          bc->start = 0;
          bc->shm_status = SHM_EMPTY;
          s->read_buff++;
          if (s->read_buff == s->dev->info.buffers)
            s->read_buff = 0;
          bc = &s->rdr_ctl->buf_ctl[s->read_buff];
        }
    }

  DBG(11, ">>\n");
  return SANE_STATUS_GOOD;
}

#else /* don't USE_FORK: */

static SANE_Status
read_data (SHARP_Scanner *s, SANE_Byte *buf, size_t * buf_size)
{
  static u_char cmd[] = {READ, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  SANE_Status status = SANE_STATUS_GOOD;
  size_t remain = *buf_size;
  size_t nread;
  int retries = MAX_RETRIES;
  DBG (11, "<< read_data ");

  /* sane_read_shuffled requires that read_data returns
     exactly *buf_size bytes, so it must be guaranteed here.
     Further make sure that not more bytes are read in than 
     sanei_scsi_max_request_size allows, to avoid a failure 
     of the read command
  */
  while (remain > 0)
    {
      nread = remain;
      if (nread > s->dev->info.bufsize)
        nread = s->dev->info.bufsize;
      cmd[6] = nread >> 16;
      cmd[7] = nread >> 8;
      cmd[8] = nread;
      status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 
                 &buf[*buf_size - remain], &nread);
      if (status == SANE_STATUS_DEVICE_BUSY && retries)
        {
          retries--;
          nread = 0;
          usleep(10000);
        }
      else if (status != SANE_STATUS_GOOD)
        {
          DBG(11, ">>\n");
          return(status);
        }
      else 
        {
          retries = MAX_RETRIES;
        }
      remain -= nread;
    }
  DBG (11, ">>\n");
  return (status);
}
#endif

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  DBG (10, "<< max_string_size ");

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  DBG (10, ">>\n");
  return max_size;
}

static SANE_Status
wait_ready(int fd)
{
  SANE_Status status;
  int retry = 0;

  while ((status = test_unit_ready (fd)) != SANE_STATUS_GOOD)
  {
    DBG (5, "wait_ready failed (%d)\n", retry);
    if (retry++ > 15){
	return SANE_STATUS_IO_ERROR;
    }
    sleep(3);
  }
  return (status);
    
}

/* ask the scanner for the maximum scan sizes with/without ADF and
   FSU. The JX330 manual does mention the sizes.
*/
static SANE_Status
get_max_scan_size(int fd, SHARP_Device *dev, int mode)
{
  SANE_Status status;
  mode_sense_subdevice m_subdev;
  size_t buf_size;
        
  status = mode_select_adf_fsu(fd, mode);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "get_scan_sizes: MODE_SELECT/subdevice page failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "get_scan_sizes: sending MODE SENSE/subdevice page\n");
  memset (&m_subdev, 0, sizeof (m_subdev));
  buf_size = sizeof (m_subdev);
  status = mode_sense (fd, &m_subdev, &buf_size, 0x20);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "get_scan_sizes: MODE_SENSE/subdevice page failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  dev->info.tl_x_ranges[mode].min = 0;
  dev->info.tl_x_ranges[mode].max = SANE_FIX(PIX_TO_MM(
    (m_subdev.max_x[0] << 24) + (m_subdev.max_x[1] << 16) +
    (m_subdev.max_x[2] << 8) + m_subdev.max_x[3] - 1, dev->info.mud));
  dev->info.tl_x_ranges[mode].quant = 0;
  
  dev->info.br_x_ranges[mode].min = SANE_FIX(PIX_TO_MM(1, dev->info.mud));
  dev->info.br_x_ranges[mode].max = SANE_FIX(PIX_TO_MM(
    (m_subdev.max_x[0] << 24) + (m_subdev.max_x[1] << 16) +
    (m_subdev.max_x[2] << 8) + m_subdev.max_x[3], dev->info.mud));
  dev->info.br_x_ranges[mode].quant = 0;

  dev->info.tl_y_ranges[mode].min = 0;
  if ((dev->sensedat.model != JX250 && dev->sensedat.model != JX350) ||
      mode != SCAN_WITH_FSU)
    dev->info.tl_y_ranges[mode].max = SANE_FIX(PIX_TO_MM(
      (m_subdev.max_y[0] << 24) + (m_subdev.max_y[1] << 16) +
      (m_subdev.max_y[2] << 8) + m_subdev.max_y[3] - 1, dev->info.mud));
  else
    /* The manual for the JX250 states on page 62 that the maximum
       value for tl_y in FSU mode is 13199, while the max value for
       br_y is 13900, which is (probably -- I don't have a FSU) returned 
       by mode sense/subdevice page. Therefore, we cannot simply 
       decrement that value and store it as max(tl_y).
    */
    dev->info.tl_y_ranges[mode].max = 13199;
  dev->info.tl_y_ranges[mode].quant = 0;
  
  dev->info.br_y_ranges[mode].min = SANE_FIX(PIX_TO_MM(1, dev->info.mud));
  dev->info.br_y_ranges[mode].max = SANE_FIX(PIX_TO_MM(
    (m_subdev.max_y[0] << 24) + (m_subdev.max_y[1] << 16) +
    (m_subdev.max_y[2] << 8) + m_subdev.max_y[3], dev->info.mud));
  dev->info.br_y_ranges[mode].quant = 0;
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (const char *devnam, SHARP_Device ** devp)
{
  SANE_Status status;
  SHARP_Device *dev;
  SHARP_Sense_Data sensedat;

  int fd;
  char inquiry_data[INQUIRY_LEN];
  const char *model_name;
  mode_sense_param msp;
  mode_sense_subdevice m_subdev;
  size_t buf_size;
  DBG (10, "<< attach ");

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devnam) == 0)
	{
	  if (devp)
	    *devp = dev;
	  return (SANE_STATUS_GOOD);
	}
    }

  sensedat.model = unknown;
  sensedat.complain_on_errors = 0;
  DBG (3, "attach: opening %s\n", devnam);
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  {
    int bufsize = 4096;
    status = sanei_scsi_open_extended (devnam, &fd, &sense_handler, &sensedat, &bufsize);
    if (status != SANE_STATUS_GOOD)
      {
        DBG (1, "attach: open failed: %s\n", sane_strstatus (status));
        return (status);
      }
    if (bufsize < 4096)
      {
        DBG(1, "attach: open failed. no memory\n");
        sanei_scsi_close(fd);
        return SANE_STATUS_NO_MEM;
      }
  }
#else
  status = sanei_scsi_open (devnam, &fd, &sense_handler, &sensedat);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: open failed: %s\n", sane_strstatus (status));
      return (status);
    }
#endif

  DBG (3, "attach: sending INQUIRY\n");
  memset (inquiry_data, 0, sizeof (inquiry_data));
  buf_size = sizeof (inquiry_data);
  status = inquiry (fd, inquiry_data, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: inquiry failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  if (inquiry_data[0] == 6 && strncmp (inquiry_data + 8, "SHARP", 5) == 0)
    {
      if (strncmp (inquiry_data + 16, "JX610", 5) == 0)
        sensedat.model = JX610;
      else if (strncmp (inquiry_data + 16, "JX250", 5) == 0)
        sensedat.model = JX250;
      else if (strncmp (inquiry_data + 16, "JX350", 5) == 0)
        sensedat.model = JX350;
      else if (   strncmp (inquiry_data + 16, "JX320", 5) == 0
               || strncmp (inquiry_data + 16, "JX325", 5) == 0)
        sensedat.model = JX320;
      else if (strncmp (inquiry_data + 16, "JX330", 5) == 0)
        sensedat.model = JX330;
    }

  if (sensedat.model == unknown)
    {
      DBG (1, "attach: device doesn't look like a Sharp scanner\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending TEST_UNIT_READY\n");
  status = test_unit_ready (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: test unit ready failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  DBG (3, "attach: sending MODE SELECT\n");
  /* JX-610 probably supports only 25 MUD size 
     JX-320 only supports 25 MUD size
  */
  if (strncmp (inquiry_data + 16, "JX610", 5) == 0)
    status = mode_select_mud (fd, DEFAULT_MUD_JX610);
  else if (strncmp (inquiry_data + 16, "JX320", 5) == 0)
    status = mode_select_mud (fd, DEFAULT_MUD_JX320);
  else
    status = mode_select_mud (fd, DEFAULT_MUD_JX330);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MODE_SELECT6 failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending MODE SENSE/MUP page\n");
  memset (&msp, 0, sizeof (msp));
  buf_size = sizeof (msp);
  status = mode_sense (fd, &msp, &buf_size, 3);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MODE_SENSE/MUP page failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    return (SANE_STATUS_NO_MEM);
  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devnam);
  dev->sane.vendor = "SHARP";
  model_name = (char*) inquiry_data + 16;
  dev->sane.model  = strndup (model_name, 10);
  dev->sane.type = "flatbed scanner";
  
  dev->sensedat.model = sensedat.model;

  DBG (5, "dev->sane.name = %s\n", dev->sane.name);
  DBG (5, "dev->sane.vendor = %s\n", dev->sane.vendor);
  DBG (5, "dev->sane.model = %s\n", dev->sane.model);
  DBG (5, "dev->sane.type = %s\n", dev->sane.type);

  dev->info.xres_range.quant = 0;
  dev->info.yres_range.quant = 0;

  dev->info.tl_x_ranges[SCAN_SIMPLE].min = SANE_FIX(0);
  dev->info.br_x_ranges[SCAN_SIMPLE].min = SANE_FIX(1);
  dev->info.tl_y_ranges[SCAN_SIMPLE].min = SANE_FIX(0);
  dev->info.br_y_ranges[SCAN_SIMPLE].min = SANE_FIX(1);
  dev->info.tl_x_ranges[SCAN_SIMPLE].quant = SANE_FIX(0);
  dev->info.br_x_ranges[SCAN_SIMPLE].quant = SANE_FIX(0);
  dev->info.tl_y_ranges[SCAN_SIMPLE].quant = SANE_FIX(0);
  dev->info.br_y_ranges[SCAN_SIMPLE].quant = SANE_FIX(0);

  dev->info.xres_default = 150;
  dev->info.yres_default = 150;
  dev->info.tl_x_ranges[SCAN_SIMPLE].max = SANE_FIX(209);
  dev->info.br_x_ranges[SCAN_SIMPLE].max = SANE_FIX(210);
  dev->info.tl_y_ranges[SCAN_SIMPLE].max = SANE_FIX(296);
  dev->info.br_y_ranges[SCAN_SIMPLE].max = SANE_FIX(297);

  dev->info.bmu = msp.bmu;
  dev->info.mud = (msp.mud[0] << 8) + msp.mud[1];
  
  dev->info.adf_fsu_installed = 0;
  if (dev->sensedat.model == JX610)
    {
      dev->info.xres_range.max = 600;
      dev->info.xres_range.min = 30;

      dev->info.yres_range.max = 600;
      dev->info.yres_range.min = 30;
      dev->info.x_default = SANE_FIX(210);
      dev->info.tl_x_ranges[SCAN_SIMPLE].max = SANE_FIX(303); /* 304.8mm is the real max */
      dev->info.br_x_ranges[SCAN_SIMPLE].max = SANE_FIX(304); /* 304.8mm is the real max */

      dev->info.y_default = SANE_FIX(297);
      dev->info.tl_y_ranges[SCAN_SIMPLE].max = SANE_FIX(430); /* 431.8 is the real max */
      dev->info.br_y_ranges[SCAN_SIMPLE].max = SANE_FIX(431); /* 431.8 is the real max */
    }
  else if (dev->sensedat.model == JX320)
    {
      dev->info.xres_range.max = 600;
      dev->info.xres_range.min = 30;

      dev->info.yres_range.max = 600;
      dev->info.yres_range.min = 30;
      dev->info.x_default = SANE_FIX(210);
      dev->info.tl_x_ranges[SCAN_SIMPLE].max = SANE_FIX(212);
      dev->info.br_x_ranges[SCAN_SIMPLE].max = SANE_FIX(213);

      dev->info.y_default = SANE_FIX(297);
      dev->info.tl_y_ranges[SCAN_SIMPLE].max = SANE_FIX(292);
      dev->info.br_y_ranges[SCAN_SIMPLE].max = SANE_FIX(293);
    }
  else
    {
      /* ask the scanner, if ADF or FSU are installed, and ask for 
         the maximum scan sizes with/without ADF and FSU.
      */

      DBG (3, "attach: sending MODE SENSE/subdevice page\n");
      memset (&m_subdev, 0, sizeof (m_subdev));
      buf_size = sizeof (m_subdev);
      status = mode_sense (fd, &m_subdev, &buf_size, 0x20);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (1, "attach: MODE_SENSE/subdevice page failed\n");
          sanei_scsi_close (fd);
          return (SANE_STATUS_INVAL);
        }
      
      /* The JX330 manual is not very clear about the ADF- und FSU-Bits
         returned by a JX320 and JX325 for the mode sense command: 
         Are these bits set to zero or not? To be on the safe side, let's
         clear them.
      */
      
      if (   strncmp(inquiry_data + 16, "JX320", 5) == 0
          || strncmp(inquiry_data + 16, "JX325", 5) == 0)
        {
          m_subdev.f_mode_type = 0;
          m_subdev.a_mode_type = 0;
        }
      
      get_max_scan_size(fd, dev, SCAN_SIMPLE);
      
      if (m_subdev.a_mode_type & 0x03) 
        {
          dev->info.adf_fsu_installed = HAVE_ADF;
          get_max_scan_size(fd, dev, SCAN_WITH_ADF);
        }
      if (m_subdev.f_mode_type & 0x07)
        {
          dev->info.adf_fsu_installed |= HAVE_FSU;
          get_max_scan_size(fd, dev, SCAN_WITH_FSU);
        }

      if (   dev->sensedat.model == JX320 
          || dev->sensedat.model == JX330
          || dev->sensedat.model == JX350)
        {
          dev->info.xres_range.max = 600;
          dev->info.xres_range.min = 30;

          dev->info.yres_range.max = 600;
          dev->info.yres_range.min = 30;
          dev->info.x_default = SANE_FIX(210);
          dev->info.y_default = SANE_FIX(297);
        }
      else if (dev->sensedat.model == JX250)
        {
          dev->info.xres_range.max = 400;
          dev->info.xres_range.min = 30;

          dev->info.yres_range.max = 400;
          dev->info.yres_range.min = 30;
          dev->info.x_default = SANE_FIX(210);
          dev->info.y_default = SANE_FIX(297);
        }
    }
  sanei_scsi_close (fd);
  
  dev->info.threshold_range.min = 1;
  dev->info.threshold_range.max = 255;
  dev->info.threshold_range.quant = 0;

  DBG (5, "xres_default=%d\n", dev->info.xres_default);
  DBG (5, "xres_range.max=%d\n", dev->info.xres_range.max);
  DBG (5, "xres_range.min=%d\n", dev->info.xres_range.min);
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);
  DBG (5, "yres_default=%d\n", dev->info.yres_default);
  DBG (5, "yres_range.max=%d\n", dev->info.yres_range.max);
  DBG (5, "yres_range.min=%d\n", dev->info.yres_range.min);
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);

  DBG (5, "x_default=%f\n", SANE_UNFIX(dev->info.x_default));
  DBG (5, "tl_x_range[0].max=%f\n", SANE_UNFIX(dev->info.tl_x_ranges[SCAN_SIMPLE].max));
  DBG (5, "tl_x_range[0].min=%f\n", SANE_UNFIX(dev->info.tl_x_ranges[SCAN_SIMPLE].min));
  DBG (5, "tl_x_range[0].quant=%d\n", dev->info.tl_x_ranges[SCAN_SIMPLE].quant);
  DBG (5, "br_x_range[0].max=%f\n", SANE_UNFIX(dev->info.br_x_ranges[SCAN_SIMPLE].max));
  DBG (5, "br_x_range[0].min=%f\n", SANE_UNFIX(dev->info.br_x_ranges[SCAN_SIMPLE].min));
  DBG (5, "br_x_range[0].quant=%d\n", dev->info.br_x_ranges[SCAN_SIMPLE].quant);
  DBG (5, "y_default=%f\n", SANE_UNFIX(dev->info.y_default));
  DBG (5, "tl_y_range[0].max=%f\n", SANE_UNFIX(dev->info.tl_y_ranges[SCAN_SIMPLE].max));
  DBG (5, "tl_y_range[0].min=%f\n", SANE_UNFIX(dev->info.tl_y_ranges[SCAN_SIMPLE].min));
  DBG (5, "tl_y_range[0].quant=%d\n", dev->info.tl_y_ranges[SCAN_SIMPLE].quant);
  DBG (5, "br_y_range[0].max=%f\n", SANE_UNFIX(dev->info.br_y_ranges[SCAN_SIMPLE].max));
  DBG (5, "br_y_range[0].min=%f\n", SANE_UNFIX(dev->info.br_y_ranges[SCAN_SIMPLE].min));
  DBG (5, "br_y_range[0].quant=%d\n", dev->info.br_y_ranges[SCAN_SIMPLE].quant);

  if (dev->info.adf_fsu_installed & HAVE_FSU)
    {
      DBG (5, "tl_x_range[1].max=%f\n", SANE_UNFIX(dev->info.tl_x_ranges[SCAN_WITH_FSU].max));
      DBG (5, "tl_x_range[1].min=%f\n", SANE_UNFIX(dev->info.tl_x_ranges[SCAN_WITH_FSU].min));
      DBG (5, "tl_x_range[1].quant=%d\n", dev->info.tl_x_ranges[SCAN_WITH_FSU].quant);
      DBG (5, "br_x_range[1].max=%f\n", SANE_UNFIX(dev->info.br_x_ranges[SCAN_WITH_FSU].max));
      DBG (5, "br_x_range[1].min=%f\n", SANE_UNFIX(dev->info.br_x_ranges[SCAN_WITH_FSU].min));
      DBG (5, "br_x_range[1].quant=%d\n", dev->info.br_x_ranges[SCAN_WITH_FSU].quant);
      DBG (5, "tl_y_range[1].max=%f\n", SANE_UNFIX(dev->info.tl_y_ranges[SCAN_WITH_FSU].max));
      DBG (5, "tl_y_range[1].min=%f\n", SANE_UNFIX(dev->info.tl_y_ranges[SCAN_WITH_FSU].min));
      DBG (5, "tl_y_range[1].quant=%d\n", dev->info.tl_y_ranges[SCAN_WITH_FSU].quant);
      DBG (5, "br_y_range[1].max=%f\n", SANE_UNFIX(dev->info.br_y_ranges[SCAN_WITH_FSU].max));
      DBG (5, "br_y_range[1].min=%f\n", SANE_UNFIX(dev->info.br_y_ranges[SCAN_WITH_FSU].min));
      DBG (5, "br_y_range[1].quant=%d\n", dev->info.br_y_ranges[SCAN_WITH_FSU].quant);
    }

  if (dev->info.adf_fsu_installed & HAVE_ADF)
    {
      DBG (5, "tl_x_range[2].max=%f\n", SANE_UNFIX(dev->info.tl_x_ranges[SCAN_WITH_ADF].max));
      DBG (5, "tl_x_range[2].min=%f\n", SANE_UNFIX(dev->info.tl_x_ranges[SCAN_WITH_ADF].min));
      DBG (5, "tl_x_range[2].quant=%d\n", dev->info.tl_x_ranges[SCAN_WITH_ADF].quant);
      DBG (5, "br_x_range[2].max=%f\n", SANE_UNFIX(dev->info.br_x_ranges[SCAN_WITH_ADF].max));
      DBG (5, "br_x_range[2].min=%f\n", SANE_UNFIX(dev->info.br_x_ranges[SCAN_WITH_ADF].min));
      DBG (5, "br_x_range[2].quant=%d\n", dev->info.br_x_ranges[SCAN_WITH_ADF].quant);
      DBG (5, "tl_y_range[2].max=%f\n", SANE_UNFIX(dev->info.tl_y_ranges[SCAN_WITH_ADF].max));
      DBG (5, "tl_y_range[2].min=%f\n", SANE_UNFIX(dev->info.tl_y_ranges[SCAN_WITH_ADF].min));
      DBG (5, "tl_y_range[2].quant=%d\n", dev->info.tl_y_ranges[SCAN_WITH_ADF].quant);
      DBG (5, "br_y_range[2].max=%f\n", SANE_UNFIX(dev->info.br_y_ranges[SCAN_WITH_ADF].max));
      DBG (5, "br_y_range[2].min=%f\n", SANE_UNFIX(dev->info.br_y_ranges[SCAN_WITH_ADF].min));
      DBG (5, "br_y_range[2].quant=%d\n", dev->info.br_y_ranges[SCAN_WITH_ADF].quant);
    }

  DBG (5, "bmu=%d\n", dev->info.bmu);
  DBG (5, "mud=%d\n", dev->info.mud);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

/* Enabling / disabling of gamma options.
   Depends on many user settable options, so lets put it into 
   one function to be called by init_options and by sane_control_option

*/
#ifdef USE_CUSTOM_GAMMA
static void 
set_gamma_caps(SHARP_Scanner *s)
{
  /* neither fixed nor custom gamma for line art modes */
  if (   strcmp(s->val[OPT_MODE].s, M_LINEART) == 0
      || strcmp(s->val[OPT_MODE].s, M_LINEART_COLOR) == 0)
    {
      s->opt[OPT_GAMMA].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
    }
  else if (strcmp(s->val[OPT_MODE].s, M_GRAY) == 0)
    {
      s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
      if (s->val[OPT_CUSTOM_GAMMA].w == SANE_FALSE)
        {
          s->opt[OPT_GAMMA].cap &= ~SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
        }
      else
        {
          s->opt[OPT_GAMMA].cap |= SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
        }
      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
    }
  else
    {
      /* color mode */
      s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
      if (s->val[OPT_CUSTOM_GAMMA].w == SANE_FALSE)
        {
          s->opt[OPT_GAMMA].cap &= ~SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
        }
      else
        {
          s->opt[OPT_GAMMA].cap |= SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
          s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
        }
    }
}
#endif /* USE_CUSTOM_GAMMA */

/* The next function is a slightly modified version of sanei_constrain_value
   Instead of returning status information like STATUS_INVAL, it adjusts
   an invaild value to the nearest allowed one.
*/
static void
clip_value (const SANE_Option_Descriptor * opt, void * value)
{
  const SANE_String_Const * string_list;
  const SANE_Word * word_list;
  int i, num_matches, match;
  const SANE_Range * range;
  SANE_Word w, v;
  size_t len;

  switch (opt->constraint_type)
    {
    case SANE_CONSTRAINT_RANGE:
      w = *(SANE_Word *) value;
      range = opt->constraint.range;

      if (w < range->min)
        w = range->min;
      else if (w > range->max)
	w = range->max;

      if (range->quant)
	{
	  v = (w - range->min + range->quant/2) / range->quant;
	  w = v * range->quant + range->min;
	  *(SANE_Word*) value = w;
	}
      break;

    case SANE_CONSTRAINT_WORD_LIST:
      w = *(SANE_Word *) value;
      word_list = opt->constraint.word_list;
      for (i = 1; w != word_list[i]; ++i)
	if (i >= word_list[0])
	  /* somewhat arbitrary... Would be better to have a default value
	     explicitly defined.
	  */
	  *(SANE_Word*) value = word_list[1];
      break;

    case SANE_CONSTRAINT_STRING_LIST:
      /* Matching algorithm: take the longest unique match ignoring
	 case.  If there is an exact match, it is admissible even if
	 the same string is a prefix of a longer option name. */
      string_list = opt->constraint.string_list;
      len = strlen (value);

      /* count how many matches of length LEN characters we have: */
      num_matches = 0;
      match = -1;
      for (i = 0; string_list[i]; ++i)
	if (strncasecmp (value, string_list[i], len) == 0
	    && len <= strlen (string_list[i]))
	  {
	    match = i;
	    if (len == strlen (string_list[i]))
	      {
		/* exact match... */
		if (strcmp (value, string_list[i]) != 0)
		  /* ...but case differs */
		  strcpy (value, string_list[match]);
	      }
	    ++num_matches;
	  }

      if (num_matches > 1)
        /* xxx quite arbitrary... We could also choose the first match
        */
        strcpy(value, string_list[match]);
      else if (num_matches == 1)
        strcpy (value, string_list[match]);
      else
        strcpy (value, string_list[0]);

    default:
      break;
    }
}

/* make sure that enough memory is allocated for each string,
   so that the strcpy in sane_control_option / set value cannot
   write behind the end of the allocated memory. 
*/
static SANE_Status
init_string_option(SHARP_Scanner *s, SANE_String_Const name, 
   SANE_String_Const title, SANE_String_Const desc, 
   const SANE_String_Const *string_list, int option, int default_index)
{
  int i;

  s->opt[option].name = name;
  s->opt[option].title = title;
  s->opt[option].desc = desc;
  s->opt[option].type = SANE_TYPE_STRING;
  s->opt[option].size = max_string_size (string_list);
  s->opt[option].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[option].constraint.string_list = string_list;
  s->val[option].s = malloc(s->opt[option].size);
  if (s->val[option].s == 0)
    {
      for (i = 1; i < NUM_OPTIONS; i++)
        {
          if (s->val[i].s && s->opt[i].type == SANE_TYPE_STRING)
            free(s->val[i].s);
        }
      return SANE_STATUS_NO_MEM;
    }
  strcpy(s->val[option].s, string_list[default_index]);
  return SANE_STATUS_GOOD;
}

static SANE_Status
init_options (SHARP_Scanner * s)
{
  int i, default_source, sourcename_index = 0;
  SANE_Word scalar;
  DBG (10, "<< init_options ");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      s->val[i].s = 0;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* Mode group: */
  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
#if 0
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[3]); /* color scan */
#endif
  init_string_option(s, SANE_NAME_SCAN_MODE, SANE_TITLE_SCAN_MODE,
    SANE_DESC_SCAN_MODE, mode_list, OPT_MODE, 3);

  /* half tone */
#if 0
  s->opt[OPT_HALFTONE].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE].desc = SANE_DESC_HALFTONE " (JX-330 only)";
  s->opt[OPT_HALFTONE].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE].size = max_string_size (halftone_list);
  s->opt[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE].constraint.string_list = halftone_list;
  s->val[OPT_HALFTONE].s = strdup (halftone_list[0]);
#endif
  init_string_option(s, SANE_NAME_HALFTONE_PATTERN, SANE_TITLE_HALFTONE_PATTERN,
    SANE_DESC_HALFTONE " (JX-330 only)", halftone_list, OPT_HALFTONE, 0);

  if (s->dev->sensedat.model == JX250 || s->dev->sensedat.model == JX350 ||
      s->dev->sensedat.model == JX610 || s->dev->sensedat.model == JX320)
    s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;

  i = 0;
  default_source = s->dev->info.default_scan_mode;

#ifdef ALLOW_AUTO_SELECT_ADF
  /* The JX330, but nut not the JX250 supports auto selection of ADF/FSU: */
  if (s->dev->info.adf_fsu_installed && (s->dev->sensedat.model == JX330))
    s->dev->info->scansources[i++] = use_auto;
#endif
  if (s->dev->info.adf_fsu_installed & HAVE_ADF)
    {
      if (default_source == -1)
        default_source = SCAN_WITH_ADF;
      if (default_source == SCAN_WITH_ADF)
        sourcename_index = i;
      s->dev->info.scansources[i++] = use_adf;
    }
  else 
    {
      if (default_source == SCAN_WITH_ADF)
        default_source = SCAN_SIMPLE;
    }
  if (s->dev->info.adf_fsu_installed & HAVE_FSU)
    {
      if (default_source == -1)
        default_source = SCAN_WITH_FSU;
      if (default_source == SCAN_WITH_FSU)
        sourcename_index = i;
      s->dev->info.scansources[i++] = use_fsu;
    }
  else
    {
      if (default_source == SCAN_WITH_FSU)
        default_source = SCAN_SIMPLE;
    }
  if (default_source < 0)
    default_source = SCAN_SIMPLE;
  if (default_source == SCAN_SIMPLE)
    sourcename_index = i;
  s->dev->info.scansources[i++] = use_simple;
  s->dev->info.scansources[i] = 0;
  
#if 0
  s->opt[OPT_SCANSOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SCANSOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SCANSOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SCANSOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SCANSOURCE].size = max_string_size (s->dev->info.scansources);
  s->opt[OPT_SCANSOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCANSOURCE].constraint.string_list = (SANE_String_Const*)s->dev->info.scansources;
  s->val[OPT_SCANSOURCE].s = strdup (s->dev->info.scansources[0]);
#endif

  init_string_option(s, SANE_NAME_SCAN_SOURCE, SANE_TITLE_SCAN_SOURCE,
    SANE_DESC_SCAN_SOURCE, (SANE_String_Const*)s->dev->info.scansources,
    OPT_SCANSOURCE, sourcename_index);

  if (i < 2)
    s->opt[OPT_SCANSOURCE].cap |= SANE_CAP_INACTIVE;

#if 0
  s->opt[OPT_PAPER].name = "Paper size";
  s->opt[OPT_PAPER].title = "Paper size";
  s->opt[OPT_PAPER].desc = "Paper size";
  s->opt[OPT_PAPER].type = SANE_TYPE_STRING;
  /* xxx the possible values for the paper size should be changeable,
     to reflect the different maximum scan sizes with/without ADF and FSU
  */
  if (s->dev->sensedat.model == JX610)
    {
      s->opt[OPT_PAPER].size = max_string_size (paper_list_jx610);
      s->opt[OPT_PAPER].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_PAPER].constraint.string_list = paper_list_jx610;
      s->val[OPT_PAPER].s = strdup (paper_list_jx610[1]);
    }
  else 
    {
      s->opt[OPT_PAPER].size = max_string_size (paper_list_jx330);
      s->opt[OPT_PAPER].constraint_type = SANE_CONSTRAINT_STRING_LIST;
      s->opt[OPT_PAPER].constraint.string_list = paper_list_jx330;
      s->val[OPT_PAPER].s = strdup (paper_list_jx330[0]);
    }
#endif

  if (s->dev->sensedat.model == JX610)
    init_string_option(s, "Paper size", "Paper size",
      "Paper size", paper_list_jx610, OPT_PAPER, 1);
  else
    init_string_option(s, "Paper size", "Paper size",
      "Paper size", paper_list_jx330, OPT_PAPER, 0);

  /* gamma */
#if 0
  s->opt[OPT_GAMMA].name = "Gamma";
  s->opt[OPT_GAMMA].title = "Gamma";
  s->opt[OPT_GAMMA].desc = "Gamma";
  s->opt[OPT_GAMMA].type = SANE_TYPE_STRING;
  s->opt[OPT_GAMMA].size = max_string_size (gamma_list);
  s->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_GAMMA].constraint.string_list = gamma_list;
  s->val[OPT_GAMMA].s = strdup (gamma_list[1]);
#endif

  init_string_option(s, "Gamma", "Gamma", "Gamma", gamma_list, OPT_GAMMA, 1);

  /* scan speed */
  s->opt[OPT_SPEED].name = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SPEED].title = "Scan speed [fast]";
  s->opt[OPT_SPEED].desc = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SPEED].type = SANE_TYPE_BOOL;
  s->val[OPT_SPEED].w = SANE_TRUE;

  /* Resolution Group */
  s->opt[OPT_RESOLUTION_GROUP].title = "Resolution";
  s->opt[OPT_RESOLUTION_GROUP].desc = "";
  s->opt[OPT_RESOLUTION_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_RESOLUTION_GROUP].cap = 0;
  s->opt[OPT_RESOLUTION_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* select resolution */
#ifdef USE_RESOLUTION_LIST
  if (s->dev->sensedat.model == JX610 || s->dev->sensedat.model == JX330 ||
      s->dev->sensedat.model == JX350 || s->dev->sensedat.model == JX320)
    init_string_option(s, "ResolutionList", "ResolutionList", "ResolutionList", 
      resolution_list_jx610, OPT_RESOLUTION_LIST, RESOLUTION_MAX_JX610);
  else
    init_string_option(s, "ResolutionList", "ResolutionList", "ResolutionList", 
      resolution_list_jx250, OPT_RESOLUTION_LIST, RESOLUTION_MAX_JX250);
#endif
  /* x resolution */
  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_X_RESOLUTION].constraint.range = &s->dev->info.xres_range;
  s->val[OPT_X_RESOLUTION].w = s->dev->info.xres_default;

#ifdef USE_SEPARATE_Y_RESOLUTION
  /* y resolution */
  s->opt[OPT_Y_RESOLUTION].name = "Y" SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].title = "Y " SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_Y_RESOLUTION].constraint.range = &s->dev->info.yres_range;
  s->val[OPT_Y_RESOLUTION].w = s->dev->info.yres_default;
#endif

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->dev->info.tl_x_ranges[default_source];
  s->val[OPT_TL_X].w = s->dev->info.tl_x_ranges[default_source].min;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->dev->info.tl_y_ranges[default_source];
  s->val[OPT_TL_Y].w = s->dev->info.tl_y_ranges[default_source].min;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->dev->info.br_x_ranges[default_source];
  scalar = s->dev->info.x_default;
  clip_value (&s->opt[OPT_BR_X], &scalar);
  s->val[OPT_BR_X].w = scalar;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->dev->info.br_y_ranges[default_source];
  /* The FSU for JX250 allows a maximum scan length of 11.5 inch, 
     which is less than the default value of 297 mm
  */
  scalar = s->dev->info.y_default;
  clip_value (&s->opt[OPT_BR_X], &scalar);
  s->val[OPT_BR_Y].w = scalar;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* edge emphasis */
#if 0
  s->opt[OPT_EDGE_EMPHASIS].name = "Edge emphasis";
  s->opt[OPT_EDGE_EMPHASIS].title = "Edge emphasis";
  s->opt[OPT_EDGE_EMPHASIS].desc = "Edge emphasis";
  s->opt[OPT_EDGE_EMPHASIS].type = SANE_TYPE_STRING;
  s->opt[OPT_EDGE_EMPHASIS].size = max_string_size (edge_emphasis_list);
  s->opt[OPT_EDGE_EMPHASIS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_EDGE_EMPHASIS].constraint.string_list = edge_emphasis_list;
  s->val[OPT_EDGE_EMPHASIS].s = strdup (edge_emphasis_list[0]);
#endif
  init_string_option(s, "Edge emphasis", "Edge emphasis",
    "Edge emphasis", edge_emphasis_list,
    OPT_EDGE_EMPHASIS, 0);

  if (   s->dev->sensedat.model == JX250 || s->dev->sensedat.model == JX350
      || s->dev->sensedat.model == JX320)
    s->opt[OPT_EDGE_EMPHASIS].cap |= SANE_CAP_INACTIVE;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &s->dev->info.threshold_range;
  s->val[OPT_THRESHOLD].w = 128;
  s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

#ifdef USE_COLOR_THRESHOLD
  s->opt[OPT_THRESHOLD_R].name = SANE_NAME_THRESHOLD "-red";
  /* xxx the titles and decriptions are confusing:
     "set white point (red)" 
     Any idea? maybe "threshold to get the red component on"
  */
  s->opt[OPT_THRESHOLD_R].title = SANE_TITLE_THRESHOLD " (red)";
  s->opt[OPT_THRESHOLD_R].desc = SANE_DESC_THRESHOLD " (red)";
  s->opt[OPT_THRESHOLD_R].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD_R].constraint.range = &s->dev->info.threshold_range;
  s->val[OPT_THRESHOLD_R].w = 128;
  s->opt[OPT_THRESHOLD_R].cap |= SANE_CAP_INACTIVE;

  s->opt[OPT_THRESHOLD_G].name = SANE_NAME_THRESHOLD "-green";
  s->opt[OPT_THRESHOLD_G].title = SANE_TITLE_THRESHOLD " (green)";
  s->opt[OPT_THRESHOLD_G].desc = SANE_DESC_THRESHOLD " (green)";
  s->opt[OPT_THRESHOLD_G].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD_G].constraint.range = &s->dev->info.threshold_range;
  s->val[OPT_THRESHOLD_G].w = 128;
  s->opt[OPT_THRESHOLD_G].cap |= SANE_CAP_INACTIVE;

  s->opt[OPT_THRESHOLD_B].name = SANE_NAME_THRESHOLD "-blue";
  s->opt[OPT_THRESHOLD_B].title = SANE_TITLE_THRESHOLD " (blue)";
  s->opt[OPT_THRESHOLD_B].desc = SANE_DESC_THRESHOLD " (blue)";
  s->opt[OPT_THRESHOLD_B].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD_B].constraint.range = &s->dev->info.threshold_range;
  s->val[OPT_THRESHOLD_B].w = 128;
  s->opt[OPT_THRESHOLD_B].cap |= SANE_CAP_INACTIVE;

#endif

  /* light color (for gray scale and line art scans) */
#if 0
  s->opt[OPT_LIGHTCOLOR].name = "LightColor";
  s->opt[OPT_LIGHTCOLOR].title = "Light Color";
  s->opt[OPT_LIGHTCOLOR].desc = "Light Color";
  s->opt[OPT_LIGHTCOLOR].type = SANE_TYPE_STRING;
  s->opt[OPT_LIGHTCOLOR].size = max_string_size (light_color_list);
  s->opt[OPT_LIGHTCOLOR].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_LIGHTCOLOR].constraint.string_list = light_color_list;
  s->val[OPT_LIGHTCOLOR].s = strdup (light_color_list[3]);
  s->opt[OPT_LIGHTCOLOR].cap |= SANE_CAP_INACTIVE;
#endif
  init_string_option(s, "LightColor", "LightColor", "LightColor",
    light_color_list, OPT_LIGHTCOLOR, 3);
  s->opt[OPT_LIGHTCOLOR].cap |= SANE_CAP_INACTIVE;

  s->opt[OPT_PREVIEW].name  = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc  = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type  = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w     = SANE_FALSE;
          

#ifdef USE_CUSTOM_GAMMA
  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* grayscale gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
#if 0
  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
#endif
  s->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR].wa = &s->gamma_table[0][0];

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
#if 0
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
#endif
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[1][0];

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
#if 0
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
#endif
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[2][0];

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
#if 0
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
#endif
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];
  set_gamma_caps(s);
#endif

  DBG (10, ">>\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
do_cancel (SHARP_Scanner * s)
{
  static u_char cmd[] = {READ, 0, 0, 0, 0, 2, 0, 0, 0, 0};

  DBG (10, "<< do_cancel ");

#ifdef USE_FORK
  if (s->reader_pid > 0)
    {
      int exit_status;
      int count = 0;
      /* ensure child knows it's time to stop: */

      DBG(11, "stopping reader process\n");
      s->rdr_ctl->cancel = 1;
      while(reader_running(s) && count < 100) 
        {
          usleep(100000);
          count++;
        };
      if (reader_running(s))
        {
          /* be brutal... 
             !! The waiting time of 10 seconds might be far too short
             !! if the resolution limit of the JX 250 is increased to
             !! to more than 400 dpi: for these (interpolated) resolutions,
             !! the JX 250 is awfully slow.
          */
          kill(s->reader_pid, SIGKILL);
        }
      wait(&exit_status);
      DBG(11, "reader process stopped\n");

      s->reader_pid = 0;
    }

#endif
  if (s->scanning == SANE_TRUE)
    {
      wait_ready(s->fd);
      sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
      /* if (s->adf_scan) */
      if (   s->dev->sensedat.model != JX610
          && s->dev->sensedat.model != JX320)
        object_position(s->fd, UNLOAD_PAPER);
    }

  s->scanning = SANE_FALSE;

  if (s->fd >= 0)
    {
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }
#ifdef USE_FORK
  {
    struct shmid_ds ds;
    if (s->shmid != -1)
      shmctl(s->shmid, IPC_RMID, &ds);
    s->shmid = -1;
  }
#endif
  if (s->buffer)
    free(s->buffer);
  s->buffer = 0;

  DBG (10, ">>\n");
  return (SANE_STATUS_CANCELLED);
}

static SHARP_New_Device *new_devs = 0;
static SHARP_New_Device *new_dev_pool = 0;

static SANE_Status 
attach_and_list(const char *devnam)
{
  SANE_Status res;
  SHARP_Device *devp;
  SHARP_New_Device *np;
  
  res = attach(devnam, &devp);
  if (res == SANE_STATUS_GOOD)
    {
      if (new_dev_pool)
        {
          np = new_dev_pool;
          new_dev_pool = np->next;
        }
      else
        {
          np = malloc(sizeof(SHARP_New_Device));
          if (np == 0)
            return SANE_STATUS_NO_MEM;
        }
      np->next =new_devs;
      np->dev = devp;
      new_devs = np;
    }
  return res;
}

static int buffers[2] = {DEFAULT_BUFFERS, DEFAULT_BUFFERS};
static int bufsize[2] = {DEFAULT_BUFSIZE, DEFAULT_BUFSIZE};
static int queued_reads[2] = {DEFAULT_QUEUED_READS, DEFAULT_QUEUED_READS};
static int stop_on_fsu_error[2] = {COMPLAIN_ON_FSU_ERROR | COMPLAIN_ON_ADF_ERROR,
                                   COMPLAIN_ON_FSU_ERROR | COMPLAIN_ON_ADF_ERROR};
static int default_scan_mode[2] = {-1, -1}; 

SANE_Status
sane_init (SANE_Int * version_code, 
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  char devnam[PATH_MAX] = "/dev/scanner";
  char line[PATH_MAX];
  const char *lp;
  char *word;
  char *end;
  FILE *fp;
  int opt_index = 0;
  int linecount = 0;
#if 1
  SHARP_Device sd; 
  SHARP_Device *dp = &sd;
#else
  SHARP_Device *dp;
#endif
  SHARP_New_Device *np;
  int i;

  DBG_INIT ();
  DBG (10, "<< sane_init ");

#if defined PACKAGE && defined VERSION
  DBG (2, "sane_init: " PACKAGE " " VERSION "\n");
#endif

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (SHARP_CONFIG_FILE);
  if (!fp)
    {
      /* use "/dev/scanner" as the default device name if no
         config file is available
      */
      attach (devnam, &dp);
      /* make sure that there are at least two buffers */
      if (DEFAULT_BUFFERS < 2)
        dp->info.buffers = DEFAULT_BUFFERS;
      else
        dp->info.buffers = 2;
      dp->info.wanted_bufsize = DEFAULT_BUFSIZE;
      dp->info.queued_reads = DEFAULT_QUEUED_READS;
      dp->info.complain_on_errors = COMPLAIN_ON_ADF_ERROR | COMPLAIN_ON_FSU_ERROR;
      dp->info.default_scan_mode = -1;
      return SANE_STATUS_GOOD;
    }

  while (fgets(line, PATH_MAX, fp))
    {
      linecount++;
      word = 0;
      lp = sanei_config_get_string(line, &word);
      if (word)
        {
          if (word[0] != '#')
            {
              if (strcmp(word, "option") == 0)
                {
                  free(word);
                  word = 0;
                  lp = sanei_config_get_string(lp, &word);
                  if (strcmp(word, "buffers") == 0)
                    {
                      free(word);
                      word = 0;
                      sanei_config_get_string(lp, &word);
                      i = strtol(word, &end, 0);
                      if (end == word)
                        {
                          DBG(1, "error in config file, line %i: number expected:\n",
                              linecount);
                          DBG(1, "%s\n", line);
                        }
                      else
                        if (i > 2)
                          buffers[opt_index] = i;
                        else
                          buffers[opt_index] = 2;
                    }
                  else if (strcmp(word, "buffersize") == 0)
                    {
                      free(word);
                      word = 0;
                      sanei_config_get_string(lp, &word);
                      i = strtol(word, &end, 0);
                      if (word == end)
                        {
                          DBG(1, "error in config file, line %i: number expected:\n",
                              linecount);
                          DBG(1, "%s\n", line);
                        }
                      else
                        bufsize[opt_index] = i;
                    }
                  else if (strcmp(word, "readqueue") == 0)
                    {
                      free(word);
                      word = 0;
                      sanei_config_get_string(lp, &word);
                      i = strtol(word, &end, 0);
                      if (word == end)
                        {
                          DBG(1, "error in config file, line %i: number expected:\n",
                              linecount);
                          DBG(1, "%s\n", line);
                        }
                      else 
                        queued_reads[opt_index] = i;
                    }
                  else if (strcmp(word, "stop_on_fsu_error") == 0)
                    {
                      free(word);
                      word = 0;
                      sanei_config_get_string(lp, &word);
                      i = strtol(word, &end, 0);
                      if (word == end)
                        {
                          DBG(1, "error in config file, line %i: number expected:\n",
                              linecount);
                          DBG(1, "%s\n", line);
                        }
                      else 
                        stop_on_fsu_error[opt_index] 
                          = i ? COMPLAIN_ON_FSU_ERROR : 0;
                    }
                  else if (strcmp(word, "default_scan_source") == 0)
                    {
                      free(word);
                      word = 0;
                      sanei_config_get_string(lp, &word);
                      if (strcmp(word, "auto") == 0)
                        default_scan_mode[opt_index] = -1;
                      else if (strcmp(word, "fsu") == 0)
                        default_scan_mode[opt_index] = SCAN_WITH_FSU;
                      else if (strcmp(word, "adf") == 0)
                        default_scan_mode[opt_index] = SCAN_WITH_ADF;
                      else if (strcmp(word, "flatbed") == 0)
                        default_scan_mode[opt_index] = SCAN_SIMPLE;
                      else
                        {
                          DBG(1, "error in config file, line %i: number expected:\n",
                              linecount);
                          DBG(1, "%s\n", line);
                        }
                    }
                  else
                    {
                      DBG(1, "error in config file, line %i: unknown option\n",
                          linecount);
                      DBG(1, "%s\n", line);
                    }
                }
              else
                {
                  while (new_devs)
                    {
                      if (buffers[1] >= 2) 
                        new_devs->dev->info.buffers = buffers[1];
                      else
                        new_devs->dev->info.buffers = 2;
                      if (bufsize[1] > 0)
                        new_devs->dev->info.wanted_bufsize = bufsize[1];
                      else
                        new_devs->dev->info.wanted_bufsize = DEFAULT_BUFSIZE;
                      if (queued_reads[1] >= 0)
                        new_devs->dev->info.queued_reads = queued_reads[1];
                      else
                        new_devs->dev->info.queued_reads = 0;
                      new_devs->dev->info.complain_on_errors = stop_on_fsu_error[1];
                      new_devs->dev->info.default_scan_mode = default_scan_mode[1];
                      np = new_devs->next;
                      new_devs->next = new_dev_pool;
                      new_dev_pool = new_devs;
                      new_devs = np;
                    }
                  if (line[strlen(line)-1] == '\n')
                    line[strlen(line)-1] = 0;
                  sanei_config_attach_matching_devices(line, &attach_and_list);
                  buffers[1] = buffers[0];
                  bufsize[1] = bufsize[0];
                  queued_reads[1] = queued_reads[0];
                  stop_on_fsu_error[1] = stop_on_fsu_error[0];
                  default_scan_mode[1] = default_scan_mode[0];
                  opt_index = 1;
                }
            }
          if (word) free(word);
        }
    }

  while (new_devs)
    {
      if (buffers[1] >= 2) 
        new_devs->dev->info.buffers = buffers[1];
      else
        new_devs->dev->info.buffers = 2;
      if (bufsize[1] > 0)
        new_devs->dev->info.wanted_bufsize = bufsize[1];
      else
        new_devs->dev->info.wanted_bufsize = DEFAULT_BUFSIZE;
      if (queued_reads[1] >= 0)
        new_devs->dev->info.queued_reads = queued_reads[1];
      else
        new_devs->dev->info.queued_reads = 0;
      new_devs->dev->info.complain_on_errors = stop_on_fsu_error[1];
      new_devs->dev->info.default_scan_mode = default_scan_mode[1];
      if (line[strlen(line)-1] == '\n')
        line[strlen(line)-1] = 0;
      np = new_devs->next;
      free(new_devs);
      new_devs = np;
    }
  while (new_dev_pool)
    {
      np = new_dev_pool->next;
      free(new_dev_pool);
      new_dev_pool = np;
    }
  fclose(fp);
  DBG (10, "sane_init >>\n");
  return (SANE_STATUS_GOOD);
}

static const SANE_Device **devlist = 0;
void
sane_exit (void)
{
  SHARP_Device *dev, *next;
  DBG (10, "<< sane_exit ");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
      free (dev);
    }
  
  if (devlist)
    free(devlist);
  devlist = 0;
  first_dev = 0;

  DBG (10, ">>\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool __sane_unused__ local_only)
{
  SHARP_Device *dev;
  int i;
  DBG (10, "<< sane_get_devices ");

  if (devlist)
    free (devlist);
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return (SANE_STATUS_NO_MEM);

  i = 0;
  for (dev = first_dev; dev; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  DBG (10, ">>\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devnam, SANE_Handle * handle)
{
  SANE_Status status;
  SHARP_Device *dev;
  SHARP_Scanner *s;
#ifdef USE_CUSTOM_GAMMA
  int i, j;
#endif

  DBG (10, "<< sane_open ");

  if (devnam[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devnam) == 0)
	    break;
	}

      if (!dev)
	{
	  status = attach (devnam, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return (status);
	  dev->info.buffers = buffers[0];
	  dev->info.wanted_bufsize = bufsize[0];
	  dev->info.queued_reads = queued_reads[0];
	}
    }
  else
    {
      dev = first_dev;
    }

  if (!dev)
    return (SANE_STATUS_INVAL);

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));

  s->fd = -1;
  s->dev = dev;
  
  s->buffer = 0;  
#ifdef USE_CUSTOM_GAMMA
  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      s->gamma_table[i][j] = j;
#endif
  status = init_options (s);
  if (status != SANE_STATUS_GOOD)
    {
      /* xxx clean up mallocs */
      return status;
    }

  s->next = first_handle;
  first_handle = s;

  *handle = s;

  DBG (10, ">>\n");
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  SHARP_Scanner *s = (SHARP_Scanner *) handle;
  DBG (10, "<< sane_close ");

  if (s->fd != -1)
    sanei_scsi_close (s->fd);
#ifdef USE_FORK
  {
    struct shmid_ds ds;
    if (s->shmid != -1)
      shmctl(s->shmid, IPC_RMID, &ds);
  }
#endif
  if (s->buffer)
    free(s->buffer);
  free (s);

  DBG (10, ">>\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  SHARP_Scanner *s = handle;
  DBG (10, "<< sane_get_option_descriptor ");

  if ((unsigned) option >= NUM_OPTIONS)
    return (0);

  DBG (10, ">>\n");
  return (s->opt + option);
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;
#ifdef USE_CUSTOM_GAMMA
  SANE_Word w, cap;
#else
  SANE_Word cap;
#endif
#ifdef USE_RESOLUTION_LIST
  int i;
#endif
  int range_index;
  DBG (10, "<< sane_control_option %i", option);

  if (info)
    *info = 0;

  if (s->scanning)
    return (SANE_STATUS_DEVICE_BUSY);
  if (option >= NUM_OPTIONS)
    return (SANE_STATUS_INVAL);

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return (SANE_STATUS_INVAL);

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_X_RESOLUTION:
#ifdef USE_SEPARATE_Y_RESOLUTION
	case OPT_Y_RESOLUTION:
#endif
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_THRESHOLD:
#ifdef USE_COLOR_THRESHOLD
	case OPT_THRESHOLD_R:
	case OPT_THRESHOLD_G:
	case OPT_THRESHOLD_B:
#endif
	case OPT_SPEED:
	case OPT_PREVIEW:
#ifdef USE_CUSTOM_GAMMA
	case OPT_CUSTOM_GAMMA:
#endif
	  *(SANE_Word *) val = s->val[option].w;
#if 0 /* here, values are read; reload should not be necessary */
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
#endif
	  return (SANE_STATUS_GOOD);

#ifdef USE_CUSTOM_GAMMA
	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;
#endif

	  /* string options: */
	case OPT_MODE:
	case OPT_HALFTONE:
	case OPT_PAPER:
	case OPT_GAMMA:
#ifdef USE_RESOLUTION_LIST
	case OPT_RESOLUTION_LIST:
#endif
	case OPT_EDGE_EMPHASIS:
	case OPT_LIGHTCOLOR:
	case OPT_SCANSOURCE:
	  strcpy (val, s->val[option].s);
#if 0
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
#endif

	  return (SANE_STATUS_GOOD);

	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return (SANE_STATUS_INVAL);

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_X_RESOLUTION:
#ifdef USE_SEPARATE_Y_RESOLUTION
	case OPT_Y_RESOLUTION:
#endif
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	case OPT_NUM_OPTS:
	case OPT_THRESHOLD:
	  /* xxx theoretically, we could use OPT_THRESHOLD in 
	     bi-level color mode to adjust all three other
	     threshold together. But this would require to set
	     the bit SANE_INFO_RELOAD_OPTIONS in *info, and that
	     would unfortunately cause a crash in both xscanimage
	     and xsane... Therefore, OPT_THRESHOLD is disabled
	     for bi-level color scan right now.
	  */
#ifdef USE_COLOR_THRESHOLD
	case OPT_THRESHOLD_R:
	case OPT_THRESHOLD_G:
	case OPT_THRESHOLD_B:
#endif
	case OPT_SPEED:
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Word *) val;
	  return (SANE_STATUS_GOOD);


	case OPT_MODE:
	  if (strcmp (val, M_LINEART) == 0)
	    {
	      s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
#ifdef USE_COLOR_THRESHOLD
	      s->opt[OPT_THRESHOLD_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_THRESHOLD_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_THRESHOLD_B].cap |= SANE_CAP_INACTIVE;
#endif
	      if (s->dev->sensedat.model == JX330)
                s->opt[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else if (strcmp (val, M_LINEART_COLOR) == 0)
	    {
	      s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
#ifdef USE_COLOR_THRESHOLD
	      s->opt[OPT_THRESHOLD_R].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_THRESHOLD_G].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_THRESHOLD_B].cap &= ~SANE_CAP_INACTIVE;
#endif
	      if (s->dev->sensedat.model == JX330)
                s->opt[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
#ifdef USE_COLOR_THRESHOLD
	      s->opt[OPT_THRESHOLD_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_THRESHOLD_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_THRESHOLD_B].cap |= SANE_CAP_INACTIVE;
#endif
              s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
            }
          
	  if (   strcmp (val, M_LINEART) == 0 
	      || strcmp (val, M_GRAY) == 0)
            {
	      s->opt[OPT_LIGHTCOLOR].cap &= ~SANE_CAP_INACTIVE;
            }
          else
            {
	      s->opt[OPT_LIGHTCOLOR].cap |= SANE_CAP_INACTIVE;
            }
            
          strcpy(s->val[option].s, val);
#ifdef USE_CUSTOM_GAMMA
          set_gamma_caps(s);
#endif
          if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return (SANE_STATUS_GOOD);

	case OPT_GAMMA:
	case OPT_HALFTONE:
	case OPT_EDGE_EMPHASIS:
	case OPT_LIGHTCOLOR:
#if 0
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
#endif
          strcpy(s->val[option].s, val);
	  return (SANE_STATUS_GOOD);

	case OPT_SCANSOURCE:
	  if (info && strcmp (s->val[option].s, (SANE_String) val))
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
#if 0
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
#endif
          strcpy(s->val[option].s, val);
	  if (strcmp(val, use_fsu) == 0)
	    range_index = SCAN_WITH_FSU;
	  else if (strcmp(val, use_adf) == 0)
	    range_index = SCAN_WITH_ADF;
	  else
	    range_index = SCAN_SIMPLE;

          s->opt[OPT_TL_X].constraint.range 
            = &s->dev->info.tl_x_ranges[range_index];
          clip_value (&s->opt[OPT_TL_X], &s->val[OPT_TL_X].w);

          s->opt[OPT_TL_Y].constraint.range 
            = &s->dev->info.tl_y_ranges[range_index];
          clip_value (&s->opt[OPT_TL_Y], &s->val[OPT_TL_Y].w);

          s->opt[OPT_BR_X].constraint.range 
            = &s->dev->info.br_x_ranges[range_index];
          clip_value (&s->opt[OPT_BR_X], &s->val[OPT_BR_X].w);

          s->opt[OPT_BR_Y].constraint.range 
            = &s->dev->info.br_y_ranges[range_index];
          clip_value (&s->opt[OPT_BR_Y], &s->val[OPT_BR_Y].w);

	  return (SANE_STATUS_GOOD);

	case OPT_PAPER:
          if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
#if 0
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
#endif
          strcpy(s->val[option].s, val);
	  s->val[OPT_TL_X].w = SANE_FIX(0);
	  s->val[OPT_TL_Y].w = SANE_FIX(0);
	  if (strcmp (s->val[option].s, "A3") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(297);
	      s->val[OPT_BR_Y].w = SANE_FIX(420);
	  }else if (strcmp (s->val[option].s, "A4") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(210);
	      s->val[OPT_BR_Y].w = SANE_FIX(297);
	  }else if (strcmp (s->val[option].s, "A5") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(148.5);
	      s->val[OPT_BR_Y].w = SANE_FIX(210);
	  }else if (strcmp (s->val[option].s, "A6") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(105);
	      s->val[OPT_BR_Y].w = SANE_FIX(148.5);
	  }else if (strcmp (s->val[option].s, "B4") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(250);
	      s->val[OPT_BR_Y].w = SANE_FIX(353);
	  }else if (strcmp (s->val[option].s, "B5") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(182);
	      s->val[OPT_BR_Y].w = SANE_FIX(257);
	  }else if (strcmp (s->val[option].s, W_LETTER) == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(279.4);
	      s->val[OPT_BR_Y].w = SANE_FIX(431.8);
	  }else if (strcmp (s->val[option].s, "Legal") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(215.9);
	      s->val[OPT_BR_Y].w = SANE_FIX(355.6);
	  }else if (strcmp (s->val[option].s, "Letter") == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(215.9);
	      s->val[OPT_BR_Y].w = SANE_FIX(279.4);
	  }else if (strcmp (s->val[option].s, INVOICE) == 0){
	      s->val[OPT_BR_X].w = SANE_FIX(215.9);
	      s->val[OPT_BR_Y].w = SANE_FIX(139.7);
	  }else{
	  }
	  return (SANE_STATUS_GOOD);

#ifdef USE_RESOLUTION_LIST
	case OPT_RESOLUTION_LIST:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  for (i = 0; s->opt[OPT_RESOLUTION_LIST].constraint.string_list[i]; i++) {
	    if (strcmp (val, 
	          s->opt[OPT_RESOLUTION_LIST].constraint.string_list[i]) == 0){
	      s->val[OPT_X_RESOLUTION].w 
	        = atoi(s->opt[OPT_RESOLUTION_LIST].constraint.string_list[i]);
	      s->val[OPT_Y_RESOLUTION].w 
	        = atoi(s->opt[OPT_RESOLUTION_LIST].constraint.string_list[i]);
	      if (info)
	        *info |= SANE_INFO_RELOAD_PARAMS;
	      break;
	    }
	  }
	  return (SANE_STATUS_GOOD);
#endif
#ifdef USE_CUSTOM_GAMMA
	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	case OPT_CUSTOM_GAMMA:
	  w = *(SANE_Word *) val;

	  if (w == s->val[OPT_CUSTOM_GAMMA].w)
	    return SANE_STATUS_GOOD;		/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  s->val[OPT_CUSTOM_GAMMA].w = w;
          set_gamma_caps(s);
	  return SANE_STATUS_GOOD;
#endif
	}
    }

  DBG (10, ">>\n");
  return (SANE_STATUS_INVAL);
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  int width, length, xres, yres;
  const char *mode;
  SHARP_Scanner *s = handle;
  DBG (10, "<< sane_get_parameters ");

  xres = s->val[OPT_X_RESOLUTION].w;
#ifdef USE_SEPARATE_Y_RESOLUTION
  yres = s->val[OPT_Y_RESOLUTION].w;
#else
  yres = xres;
#endif
  if (!s->scanning)
    {
      /* make best-effort guess at what parameters will look like once
         scanning starts.  */
      memset (&s->params, 0, sizeof (s->params));

      width = MM_TO_PIX(  SANE_UNFIX(s->val[OPT_BR_X].w) 
                        - SANE_UNFIX(s->val[OPT_TL_X].w),
			s->dev->info.mud);
      length = MM_TO_PIX(  SANE_UNFIX(s->val[OPT_BR_Y].w)
                          - SANE_UNFIX(s->val[OPT_TL_Y].w),
			 s->dev->info.mud);

      s->width = width;
      s->length = length;
      s->params.pixels_per_line = width * xres / s->dev->info.mud;
      s->params.lines = length * yres / s->dev->info.mud;
      s->unscanned_lines = s->params.lines;
    }
  else
    {
      static u_char cmd[] = {READ, 0, 0x81, 0, 0, 0, 0, 0, 4, 0};
      static u_char buf[4];
      size_t len = 4;
      SANE_Status status;

      /* if async reads are used, )ie. if USE_FORK is defined,
         this command may only be issued immediately after the
         "start scan" command. Later calls will confuse the 
         read queue.
      */
      if (!s->get_params_called)
        {
          wait_ready(s->fd);
          status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), buf, &len);
          
          if (status != SANE_STATUS_GOOD)
            {
              do_cancel(s);
              return (status);
            }
          s->params.pixels_per_line = (buf[1] << 8) + buf[0];
          s->params.lines = (buf[3] << 8) + buf[2];
          s->get_params_called = 1;
        }
    }

  xres = s->val[OPT_X_RESOLUTION].w;
#ifdef USE_SEPARATE_Y_RESOLUTION
  yres = s->val[OPT_Y_RESOLUTION].w;
#else
  yres = xres;
#endif

  mode = s->val[OPT_MODE].s;

  if (strcmp (mode, M_LINEART) == 0)
     {
       s->params.format = SANE_FRAME_GRAY;
       s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
       s->params.depth = 1;
       s->modes = MODES_LINEART;
     }
  else if (strcmp (mode, M_GRAY) == 0)
     {
       s->params.format = SANE_FRAME_GRAY;
       s->params.bytes_per_line = s->params.pixels_per_line;
       s->params.depth = 8;
       s->modes = MODES_GRAY;
     }
  else
     {
       s->params.format = SANE_FRAME_RGB;
       s->params.bytes_per_line = 3 * s->params.pixels_per_line;
       s->params.depth = 8;
       s->modes = MODES_COLOR;
     }
  s->params.last_frame = SANE_TRUE;

  if (params)
    *params = s->params;

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

#ifdef USE_CUSTOM_GAMMA

static int
sprint_gamma(Option_Value val, SANE_Byte *dst)
{
  int i;
  SANE_Byte *p = dst;
  
  p += sprintf((char *) p, "%i", val.wa[0] > 255 ? 255 : val.wa[0]);
  /* val.wa[i] is over 255, so val.wa[i] is limitied to 255 */
  for (i = 1; i < 256; i++)
    p += sprintf((char *) p, ",%i", val.wa[i] > 255 ? 255 : val.wa[i]);
  return p - dst;
}

static SANE_Status
send_ascii_gamma_tables (SHARP_Scanner *s)
{
  SANE_Status status;
  int i;
  
  DBG(11, "<< send_ascii_gamma_tables ");
    
  /* we need: 4 bytes for each gamma value (3 digits + delimiter)
             + 10 bytes for the command header
     i.e. 4 * 4 * 256 + 10 = 4106 bytes
  */
  
  if (s->dev->info.bufsize < 4106)
    return SANE_STATUS_NO_MEM;
  
  memset(s->buffer, 0, 4106);

  i = sprint_gamma(s->val[OPT_GAMMA_VECTOR_R], &s->buffer[10]);
  s->buffer[10+i++] = '/';
  i += sprint_gamma(s->val[OPT_GAMMA_VECTOR_G], &s->buffer[10+i]);
  s->buffer[10+i++] = '/';
  i += sprint_gamma(s->val[OPT_GAMMA_VECTOR_B], &s->buffer[10+i]);
  s->buffer[10+i++] = '/';
  i += sprint_gamma(s->val[OPT_GAMMA_VECTOR], &s->buffer[10+i]);
  
  DBG(11, "%s\n", &s->buffer[10]);

  s->buffer[0] = SEND;
  s->buffer[2] = 0x03;
  s->buffer[7] = i >> 8;
  s->buffer[8] = i & 0xff;
  
  wait_ready(s->fd);
  status = sanei_scsi_cmd (s->fd, s->buffer, i+10, 0, 0);

  DBG(11, ">>\n");
  
  return status;
}
#endif

static SANE_Status
send_binary_g_table(SHARP_Scanner *s, SANE_Word *a, int dtq)
{
  SANE_Status status;
  int i;
  
  DBG(11, "<< send_binary_g_table\n");

  memset(s->buffer, 0, 522);
  
  s->buffer[0] = SEND;
  s->buffer[2] = 0x03;
  s->buffer[5] = dtq;
  s->buffer[7] = 2;
  s->buffer[8] = 0;
  
  for (i = 0; i < 256; i++)
    {
      s->buffer[2*i+11] = a[i] > 255 ? 255 : a[i];
    }
  
  for (i = 0; i < 256; i += 16)
    {
      DBG(11, "%02x %02x %02x %02x %02x %02x %02x %02x "
              "%02x %02x %02x %02x %02x %02x %02x %02x\n",
              a[i  ], a[i+1], a[i+2], a[i+3], 
              a[i+4], a[i+5], a[i+6], a[i+7],
              a[i+8], a[i+9], a[i+10], a[i+11], 
              a[i+12], a[i+13], a[i+14], a[i+15]);
    }

  wait_ready(s->fd);
  status = sanei_scsi_cmd (s->fd, s->buffer, 2*i+10, 0, 0);

  DBG(11, ">>\n");

  return status;
}

#ifdef USE_CUSTOM_GAMMA
static SANE_Status
send_binary_gamma_tables (SHARP_Scanner *s)
{
  SANE_Status status;
  
  status = send_binary_g_table(s, s->val[OPT_GAMMA_VECTOR].wa, 0x10);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  status = send_binary_g_table(s, s->val[OPT_GAMMA_VECTOR_R].wa, 0x11);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  status = send_binary_g_table(s, s->val[OPT_GAMMA_VECTOR_G].wa, 0x12);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  status = send_binary_g_table(s, s->val[OPT_GAMMA_VECTOR_B].wa, 0x13);

  return status;
}

static SANE_Status
send_gamma_tables (SHARP_Scanner *s)
{
  if (s->dev->sensedat.model != JX250 && s->dev->sensedat.model != JX350)
    {
      return send_ascii_gamma_tables(s);
    }
  else
    {
      return send_binary_gamma_tables(s);
    }
  
}
#endif

#ifdef USE_COLOR_THRESHOLD
static SANE_Status
send_threshold_data(SHARP_Scanner *s)
{
  SANE_Status status;
  SANE_Byte cmd[26] = {SEND, 0, 0x82, 0, 0, 0, 0, 0, 0, 0};
  int len;
  
  memset(cmd, 0, sizeof(cmd));
  /* maximum string length: 3 bytes for each number (they are 
     restricted to the range 0..255), 3 '/' and the null-byte,
     total: 16 bytes.
  */
  len = sprintf((char *) &cmd[10], "%i/%i/%i/%i", 
                s->val[OPT_THRESHOLD_R].w,
                s->val[OPT_THRESHOLD_G].w,
                s->val[OPT_THRESHOLD_B].w,
                s->val[OPT_THRESHOLD].w);
  cmd[8] = len;
  
  wait_ready(s->fd);
  status = sanei_scsi_cmd(s->fd, cmd, len + 10, 0, 0);
  return status;
}
#endif


SANE_Status
sane_start (SANE_Handle handle)
{
  char *mode, *halftone, *paper, *gamma, *edge, *lightcolor, *adf_fsu;
  SHARP_Scanner *s = handle;
  SANE_Status status;
  size_t buf_size;
  SHARP_Send ss;
  window_param wp;
  mode_sense_subdevice m_subdev;

  DBG (10, "<< sane_start ");

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  s->dev->sensedat.complain_on_errors 
    = COMPLAIN_ON_ADF_ERROR | s->dev->info.complain_on_errors;

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  s->dev->info.bufsize = s->dev->info.wanted_bufsize;
  if (s->dev->info.bufsize < 32 * 1024)
    s->dev->info.bufsize = 32 * 1024;
  {
    int bsize = s->dev->info.bufsize;
    status = sanei_scsi_open_extended (s->dev->sane.name, &s->fd, 
              &sense_handler, &s->dev->sensedat, &bsize);
    s->dev->info.bufsize = bsize;
  }
  
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "open of %s failed: %s\n",
         s->dev->sane.name, sane_strstatus (status));
      return (status);
    }

  /* make sure that we got at least 32 kB. Even then, the scan will be
     awfully slow. 
     
     NOTE: If you need to decrease this value, remember that s->buffer
     is used in send_ascii_gamma_tables (JX330/JX610) and in 
     send_binary_g_table (JX250/JX350). send_ascii_gamma_tables needs 4106
     bytes, and send_binary_g_table needs 522 bytes.
  */
  if (s->dev->info.bufsize < 32 * 1024)
    {
      sanei_scsi_close(s->fd);
      s->fd = -1;
      return SANE_STATUS_NO_MEM;
    }
#else
  status = sanei_scsi_open(s->dev->sane.name, &s->fd, &sense_handler, 
              &s->dev->sensedat);
  if (s->dev->info.wanted_bufsize < sanei_scsi_max_request_size)
    s->dev->info.bufsize = s->dev->info.wanted_bufsize;
  else
    s->dev->info.bufsize = sanei_scsi_max_request_size;
    
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "open of %s failed: %s\n",
         s->dev->sane.name, sane_strstatus (status));
      return (status);
    }
#endif

  s->buffer = malloc(s->dev->info.bufsize);
  if (!s->buffer) {
    sanei_scsi_close(s->fd);
    s->fd = -1;
    free(s);
    return SANE_STATUS_NO_MEM;
  }

#ifdef USE_FORK
  {
    struct shmid_ds ds;
    size_t n;

    s->shmid = shmget(IPC_PRIVATE,
       sizeof(SHARP_rdr_ctl)
       + s->dev->info.buffers *
         (sizeof(SHARP_shmem_ctl) + s->dev->info.bufsize),
       IPC_CREAT | 0600);
    if (s->shmid == -1)
      {
        free(s->buffer);
        s->buffer = 0;
        sanei_scsi_close(s->fd);
        s->fd = -1;
        return SANE_STATUS_NO_MEM;
      }
    s->rdr_ctl = (SHARP_rdr_ctl*) shmat(s->shmid, 0, 0);
    if (s->rdr_ctl == (void *) -1)
     {
       shmctl(s->shmid, IPC_RMID, &ds);
       free(s->buffer);
       s->buffer = 0;
       sanei_scsi_close(s->fd);
       s->fd = -1;
       return SANE_STATUS_NO_MEM;
     }

    s->rdr_ctl->buf_ctl = (SHARP_shmem_ctl*) &s->rdr_ctl[1];
    for (n = 0; n < s->dev->info.buffers; n++)
      {
        s->rdr_ctl->buf_ctl[n].buffer =
          (SANE_Byte*) &s->rdr_ctl->buf_ctl[s->dev->info.buffers]
            + n * s->dev->info.bufsize;
      }
  }
#endif /* USE_FORK */

  DBG (5, "start: TEST_UNIT_READY\n");
  status = test_unit_ready (s->fd);
  
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "TEST UNIT READY failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  DBG (3, "start: sending MODE SELECT\n");
  status = mode_select_mud (s->fd, s->dev->info.mud);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start: MODE_SELECT6 failed\n");
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  mode = s->val[OPT_MODE].s;
  halftone = s->val[OPT_HALFTONE].s;
  paper = s->val[OPT_PAPER].s;
  gamma = s->val[OPT_GAMMA].s;
  edge = s->val[OPT_EDGE_EMPHASIS].s;
  lightcolor = s->val[OPT_LIGHTCOLOR].s;
  adf_fsu = s->val[OPT_SCANSOURCE].s;
  s->speed = s->val[OPT_SPEED].w;

  s->xres = s->val[OPT_X_RESOLUTION].w;
  if (s->val[OPT_PREVIEW].w == SANE_FALSE)
    {
#ifdef USE_SEPARATE_Y_RESOLUTION
      s->yres = s->val[OPT_Y_RESOLUTION].w;
#else
      s->yres = s->val[OPT_X_RESOLUTION].w;
#endif
      s->speed = s->val[OPT_SPEED].w;
    }
  else
    {
      s->yres = s->val[OPT_X_RESOLUTION].w;
      s->speed = SANE_TRUE;
    }

  s->ulx = MM_TO_PIX(SANE_UNFIX(s->val[OPT_TL_X].w), s->dev->info.mud);
  s->uly = MM_TO_PIX(SANE_UNFIX(s->val[OPT_TL_Y].w), s->dev->info.mud);
  s->threshold = s->val[OPT_THRESHOLD].w;
  s->bpp = s->params.depth;
  
  s->adf_fsu_mode = SCAN_SIMPLE; /* default: scan without ADF and FSU */
#ifdef ALLOW_AUTO_SELECT_ADF
  if (strcmp (adf_fsu, use_auto) == 0)
    s->adf_fsu_mode = SCAN_ADF_FSU_AUTO;
  else
#endif
  if (strcmp(adf_fsu, use_fsu) == 0)
    s->adf_fsu_mode = SCAN_WITH_FSU;
  else if (strcmp(adf_fsu, use_adf) == 0)
    s->adf_fsu_mode = SCAN_WITH_ADF;
  else if (strcmp(adf_fsu, use_adf) == 0)
    s->adf_fsu_mode = SCAN_SIMPLE;

  if (strcmp (mode, M_LINEART) == 0)
    {
      s->reverse = 0;
      if (strcmp(halftone, M_BILEVEL) == 0)
        {
          s->halftone = 1;
          s->image_composition = 0;
        }
      else if (strcmp(halftone, M_BAYER) == 0)
        {
          s->halftone = 2;
          s->image_composition = 1;
        }
      else if (strcmp(halftone, M_SPIRAL) == 0)
        {
          s->halftone = 3;
          s->image_composition = 1;
        }
      else if (strcmp(halftone, M_DISPERSED) == 0)
        {
          s->halftone = 4;
          s->image_composition = 1;
        }
      else if (strcmp(halftone, M_ERRDIFFUSION) == 0)
        {
          s->halftone = 5;
          s->image_composition = 1;
        }
    }
  else if (strcmp (mode, M_GRAY) == 0)
    {
      s->image_composition = 2;
      s->reverse = 1;
    }
  else if (strcmp (mode, M_LINEART_COLOR) == 0)
    {
      s->reverse = 1;
      if (strcmp(halftone, M_BILEVEL) == 0)
        {
          s->halftone = 1;
          s->image_composition = 3;
        }
      else if (strcmp(halftone, M_BAYER) == 0)
        {
          s->halftone = 2;
          s->image_composition = 4;
        }
      else if (strcmp(halftone, M_SPIRAL) == 0)
        {
          s->halftone = 3;
          s->image_composition = 4;
        }
      else if (strcmp(halftone, M_DISPERSED) == 0)
        {
          s->halftone = 4;
          s->image_composition = 4;
        }
      else if (strcmp(halftone, M_ERRDIFFUSION) == 0)
        {
          s->halftone = 5;
          s->image_composition = 4;
        }
    }
  else if (strcmp (mode, M_COLOR) == 0)
    {
      s->image_composition = 5;
      s->reverse = 1;
    }

  if (strcmp (edge, EDGE_NONE) == 0)
    {
      DBG (11, "EDGE EMPHASIS NONE\n");
      s->edge = 0;
    }
  else if (strcmp (edge, EDGE_MIDDLE) == 0)
    {
      DBG (11, "EDGE EMPHASIS MIDDLE\n");
      s->edge = 1;
    }
  else if (strcmp (edge, EDGE_STRONG) == 0)
    {
      DBG (11, "EDGE EMPHASIS STRONG\n");
      s->edge = 2;
    }
  else if (strcmp (edge, EDGE_BLUR) == 0)
    {
      DBG (11, "EDGE EMPHASIS BLUR\n");
      s->edge = 3;
    }
  
  s->lightcolor = 3;
  if (strcmp(lightcolor, LIGHT_GREEN) == 0)
    s->lightcolor = 0;
  else if (strcmp(lightcolor, LIGHT_RED) == 0)
    s->lightcolor = 1;
  else if (strcmp(lightcolor, LIGHT_BLUE) == 0)
    s->lightcolor = 2;
  else if (strcmp(lightcolor, LIGHT_WHITE) == 0)
    s->lightcolor = 3;

  s->adf_scan = 0;
  if (   s->dev->sensedat.model != JX610
      && s->dev->sensedat.model != JX320)
    {
      status = mode_select_adf_fsu(s->fd, s->adf_fsu_mode);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (10, "sane_start: mode_select_adf_fsu failed: %s\n", sane_strstatus (status));
          sanei_scsi_close (s->fd);
          s->fd = -1;
          return (status);
        }
      /* if the ADF is selected, check if it is ready */
      memset (&m_subdev, 0, sizeof (m_subdev));
      buf_size = sizeof (m_subdev);
      status = mode_sense (s->fd, &m_subdev, &buf_size, 0x20);
      DBG(11, "mode sense result a_mode: %x f_mode: %x\n", 
          m_subdev.a_mode_type, m_subdev.f_mode_type);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (10, "sane_start: MODE_SENSE/subdevice page failed\n");
          sanei_scsi_close (s->fd);
          s->fd = -1;
          return (status);
        }
      if (s->adf_fsu_mode == SCAN_WITH_ADF) 
        s->adf_scan = 1;
#ifdef ALLOW_AUTO_SELECT_ADF
      else if (s->adf_fsu_mode == SCAN_ADF_FSU_AUTO)
        {
          if (m_subdev.a_mode_type & 0x80)
            s->adf_scan = 1;
        }
#endif
    }

  
#ifdef USE_CUSTOM_GAMMA
  if (s->val[OPT_CUSTOM_GAMMA].w == SANE_FALSE)
    {
#endif
      if (s->dev->sensedat.model != JX250 && s->dev->sensedat.model != JX350)
        {
          ss.dtc = 0x03;
          if (strcmp (gamma, GAMMA10) == 0)
          {
              ss.dtq = 0x01;
          }else{
              ss.dtq = 0x02;
          }
          ss.length = 0;
          DBG (5, "start: SEND\n");
          status = send (s->fd,  &ss);
          if (status != SANE_STATUS_GOOD)
            {
              DBG (1, "send failed: %s\n", sane_strstatus (status));
              sanei_scsi_close (s->fd);
              s->fd = -1;
              return (status);
            }
       }
     else
       {
         /* the JX250 does not support the "fixed gamma selection", 
            therefore, lets calculate & send gamma values
         */
         int i;
         SANE_Word gtbl[256];
         if (strcmp (gamma, GAMMA10) == 0)
           for (i = 0; i < 256; i++)
             gtbl[i] = i;
         else
           {
             gtbl[0] = 0;
             for (i = 1; i < 256; i++)
               gtbl[i] = 255 * exp(0.45 * log(i/255.0));
           }
         send_binary_g_table(s, gtbl, 0x10);
         send_binary_g_table(s, gtbl, 0x11);
         send_binary_g_table(s, gtbl, 0x12);
         send_binary_g_table(s, gtbl, 0x13);
       }
#ifdef USE_CUSTOM_GAMMA
    }
  else
    status = send_gamma_tables(s);
      if (status != SANE_STATUS_GOOD)
        {
          sanei_scsi_close (s->fd);
          s->fd = -1;
          return (status);
        }
#endif

  if (s->dev->sensedat.model != JX250 && s->dev->sensedat.model != JX350)
    {
      ss.dtc = 0x86;
      ss.dtq = 0x05;
      ss.length = 0;
      DBG (5, "start: SEND\n");
      status = send (s->fd,  &ss);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (1, "send failed: %s\n", sane_strstatus (status));
          sanei_scsi_close (s->fd);
          s->fd = -1;
          return (status);
        }
      
#ifdef USE_COLOR_THRESHOLD
      status = send_threshold_data(s);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (1, "send threshold data failed: %s\n", sane_strstatus (status));
          sanei_scsi_close (s->fd);
          s->fd = -1;
          return (status);
        }
#endif
    }
  
  memset (&wp, 0, sizeof (wp));
  /* every Sharp scanner seems to have a different 
     window descriptor block...
  */
  if (   s->dev->sensedat.model == JX610
      || s->dev->sensedat.model == JX320)
    {
      buf_size = sizeof(WDB);
    }
  else if (s->dev->sensedat.model == JX330)
    {
      buf_size = sizeof (WDB) + sizeof(WDBX330);
    }
  else
    {
      buf_size = sizeof (WDB) + sizeof(WDBX330) + sizeof(WDBX250);
    }
    
  wp.wpdh.wdl[0] = buf_size >> 8;
  wp.wpdh.wdl[1] = buf_size;
  wp.wdb.x_res[0] = s->xres >> 8;
  wp.wdb.x_res[1] = s->xres;
  wp.wdb.y_res[0] = s->yres >> 8;
  wp.wdb.y_res[1] = s->yres;
  wp.wdb.x_ul[0] = s->ulx >> 24;
  wp.wdb.x_ul[1] = s->ulx >> 16;
  wp.wdb.x_ul[2] = s->ulx >> 8;
  wp.wdb.x_ul[3] = s->ulx;
  wp.wdb.y_ul[0] = s->uly >> 24;
  wp.wdb.y_ul[1] = s->uly >> 16;
  wp.wdb.y_ul[2] = s->uly >> 8;
  wp.wdb.y_ul[3] = s->uly;
  wp.wdb.width[0] = s->width >> 24;
  wp.wdb.width[1] = s->width >> 16;
  wp.wdb.width[2] = s->width >> 8;
  wp.wdb.width[3] = s->width;
  wp.wdb.length[0] = s->length >> 24;
  wp.wdb.length[1] = s->length >> 16;
  wp.wdb.length[2] = s->length >> 8;
  wp.wdb.length[3] = s->length;
  wp.wdb.brightness = 0;
  wp.wdb.threshold = s->threshold;
  wp.wdb.image_composition = s->image_composition;
  if (s->image_composition <= 2 || s->image_composition >= 5)
    wp.wdb.bpp = s->bpp;
  else
    wp.wdb.bpp = 1;
  wp.wdb.ht_pattern[0] = 0;
  if (   s->dev->sensedat.model == JX610
      || s->dev->sensedat.model == JX320)
    {
      wp.wdb.ht_pattern[1] = 0;
    }else{
      wp.wdb.ht_pattern[1] = s->halftone;
    }
  wp.wdb.rif_padding = (s->reverse * 128) + 0;
  wp.wdb.eletu = (!s->speed << 2) + (s->edge << 6) + (s->lightcolor << 4);

  if (s->dev->sensedat.model == JX250 || s->dev->sensedat.model == JX350)
    {
      wp.wdbx250.threshold_red   = s->val[OPT_THRESHOLD_R].w;
      wp.wdbx250.threshold_green = s->val[OPT_THRESHOLD_G].w;
      wp.wdbx250.threshold_blue  = s->val[OPT_THRESHOLD_B].w;
    }


  DBG (5, "wdl=%d\n", (wp.wpdh.wdl[0] << 8) + wp.wpdh.wdl[1]);
  DBG (5, "xres=%d\n", (wp.wdb.x_res[0] << 8) + wp.wdb.x_res[1]);
  DBG (5, "yres=%d\n", (wp.wdb.y_res[0] << 8) + wp.wdb.y_res[1]);
  DBG (5, "ulx=%d\n", (wp.wdb.x_ul[0] << 24) + (wp.wdb.x_ul[1] << 16) +
                      (wp.wdb.x_ul[2] << 8) + wp.wdb.x_ul[3]);
  DBG (5, "uly=%d\n", (wp.wdb.y_ul[0] << 24) + (wp.wdb.y_ul[1] << 16) +
                      (wp.wdb.y_ul[2] << 8) + wp.wdb.y_ul[3]);
  DBG (5, "width=%d\n", (wp.wdb.width[0] << 8) + (wp.wdb.width[1] << 16) +
                        (wp.wdb.width[2] << 8) + wp.wdb.width[3]);
  DBG (5, "length=%d\n", (wp.wdb.length[0] << 16) + (wp.wdb.length[1] << 16) +
                         (wp.wdb.length[2] << 8) + wp.wdb.length[3]);

  DBG (5, "threshold=%d\n", wp.wdb.threshold);
  DBG (5, "image_composition=%d\n", wp.wdb.image_composition);
  DBG (5, "bpp=%d\n", wp.wdb.bpp);
  DBG (5, "rif_padding=%d\n", wp.wdb.rif_padding);
  DBG (5, "eletu=%d\n", wp.wdb.eletu);
  
#if 0
  {
    unsigned char *p = (unsigned char*) &wp.wdb;
    int i;
    DBG(11, "set window:\n");
    for (i = 0; i < sizeof(wp.wdb) + + sizeof(wp.wdbx330) + sizeof(wp.wdbx250); i += 16)
     {
      DBG(1, "%2x %2x %2x %2x %2x %2x %2x %2x - %2x %2x %2x %2x %2x %2x %2x %2x\n",
      p[i], p[i+1], p[i+2], p[i+3], p[i+4], p[i+5], p[i+6], p[i+7], p[i+8],
      p[i+9], p[i+10], p[i+11], p[i+12], p[i+13], p[i+14], p[i+15]);
     }
  }
#endif

  buf_size += sizeof(WPDH);
  DBG (5, "start: SET WINDOW\n");
  status = set_window (s->fd, &wp, buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "SET WINDOW failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  memset (&wp, 0, buf_size);
  DBG (5, "start: GET WINDOW\n");
  status = get_window (s->fd, &wp, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "GET WINDOW failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }
  DBG (5, "xres=%d\n", (wp.wdb.x_res[0] << 8) + wp.wdb.x_res[1]);
  DBG (5, "yres=%d\n", (wp.wdb.y_res[0] << 8) + wp.wdb.y_res[1]);
  DBG (5, "ulx=%d\n", (wp.wdb.x_ul[0] << 24) + (wp.wdb.x_ul[1] << 16) +
                      (wp.wdb.x_ul[2] << 8) + wp.wdb.x_ul[3]);
  DBG (5, "uly=%d\n", (wp.wdb.y_ul[0] << 24) + (wp.wdb.y_ul[1] << 16) +
       (wp.wdb.y_ul[2] << 8) + wp.wdb.y_ul[3]);
  DBG (5, "width=%d\n", (wp.wdb.width[0] << 24) + (wp.wdb.width[1] << 16) +
                        (wp.wdb.width[2] << 8) + wp.wdb.width[3]);
  DBG (5, "length=%d\n", (wp.wdb.length[0] << 24) + (wp.wdb.length[1] << 16) +
                         (wp.wdb.length[2] << 8) + wp.wdb.length[3]);

  if (s->adf_scan)
    {
      status = object_position(s->fd, LOAD_PAPER);
      if (status != SANE_STATUS_GOOD)
        {
          sanei_scsi_close (s->fd);
          s->fd = -1;
          s->busy = SANE_FALSE;
          s->cancel = SANE_FALSE;
          return (status);
        }
    }

  DBG (5, "start: SCAN\n");
  s->scanning = SANE_TRUE;
  s->busy = SANE_TRUE;
  s->cancel = SANE_FALSE;
  s->get_params_called = 0;

  wait_ready(s->fd);
  status = scan (s->fd);
#ifdef DEBUG
          {
            struct timeval t;
            gettimeofday(&t, 0);
            DBG(2, "rd: scan started        %li.%06li\n", t.tv_sec, t.tv_usec);
          }
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start of scan failed: %s\n", sane_strstatus (status));
      do_cancel(s);
      return (status);
    }

  /* ask the scanner for the scan size */
  /* wait_ready(s->fd); */
#ifdef DEBUG
          {
            struct timeval t;
            gettimeofday(&t, 0);
            DBG(2, "rd: wait_ready ok       %li.%06li\n", t.tv_sec, t.tv_usec);
          }
#endif
  sane_get_parameters(s, 0);
#ifdef DEBUG
          {
            struct timeval t;
            gettimeofday(&t, 0);
            DBG(2, "rd: get_params ok       %li.%06li\n", t.tv_sec, t.tv_usec);
          }
#endif
  if (strcmp (mode, M_LINEART_COLOR) != 0)
    s->bytes_to_read = s->params.bytes_per_line * s->params.lines;
  else
    {
      s->bytes_to_read = (s->params.pixels_per_line+7) / 8;
      s->bytes_to_read *= 3 * s->params.lines;
    }

#ifdef USE_FORK
  {
    size_t i;
    for (i = 0; i < s->dev->info.buffers; i++) 
      s->rdr_ctl->buf_ctl[i].shm_status = SHM_EMPTY;
    s->read_buff = 0;
    s->rdr_ctl->cancel = 0;
    s->rdr_ctl->running = 0;
    s->rdr_ctl->status  = SANE_STATUS_GOOD;
  }
  s->reader_pid = fork();
#ifdef DEBUG
          {
            struct timeval t;
            gettimeofday(&t, 0);
            DBG(2, "rd: forked              %li.%06li %i\n", t.tv_sec, t.tv_usec,
              s->reader_pid);
          }
#endif
  if (s->reader_pid == 0) 
    {
      sigset_t ignore_set;
      struct SIGACTION act;

      sigfillset (&ignore_set);
      sigdelset (&ignore_set, SIGTERM);
      sigprocmask (SIG_SETMASK, &ignore_set, 0);

      memset (&act, 0, sizeof (act));
      sigaction (SIGTERM, &act, 0);

      /* don't use exit() since that would run the atexit() handlers... */
      _exit (reader_process (s));
    }
  else if (s->reader_pid == -1)
    {
      s->busy = SANE_FALSE;
      do_cancel(s);
      return SANE_STATUS_NO_MEM;
    }
   
#endif /* USE_FORK */
  

  DBG (1, "%d pixels per line, %d bytes, %d lines high, total %lu bytes, "
       "dpi=%d\n", s->params.pixels_per_line, s->params.bytes_per_line,
       s->params.lines, (u_long) s->bytes_to_read, s->val[OPT_X_RESOLUTION].w);

  s->busy = SANE_FALSE;
  s->buf_used = 0;
  s->buf_pos = 0;

  if (s->cancel == SANE_TRUE) 
    {
      do_cancel(s);
      DBG (10, ">>\n");
      return(SANE_STATUS_CANCELLED);
    }

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);

}

static SANE_Status
sane_read_direct (SANE_Handle handle, SANE_Byte *dst_buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;
  size_t nread;
  DBG (10, "<< sane_read_direct ");

  DBG (20, "remaining: %lu ", (u_long) s->bytes_to_read);
  *len = 0;

  if (s->bytes_to_read == 0)
    {
      do_cancel (s);
      return (SANE_STATUS_EOF);
    }

  if (!s->scanning)
    return (do_cancel (s));
  nread = max_len;
  if (nread > s->bytes_to_read)
    nread = s->bytes_to_read;
  if (nread > s->dev->info.bufsize)
    nread = s->dev->info.bufsize;
#ifdef USE_FORK
  status = read_data(s, dst_buf, &nread);
#else
  wait_ready(s->fd);
  status = read_data (s, dst_buf, &nread);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      do_cancel (s);
      return (SANE_STATUS_IO_ERROR);
    }
  *len = nread;
  s->bytes_to_read -= nread;
  DBG (20, "remaining: %lu ", (u_long) s->bytes_to_read);

  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

static SANE_Status
sane_read_shuffled (SANE_Handle handle, SANE_Byte *dst_buf, SANE_Int max_len,
	   SANE_Int * len, int eight_bit_data)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;
  SANE_Byte *dest, *red, *green, *blue, mask;
  SANE_Int transfer;
  size_t nread, ntest, pixel, max_pixel, line, max_line;
  size_t start_input, bytes_per_line_in;
  DBG (10, "<< sane_read_shuffled ");

  *len = 0;
  if (s->bytes_to_read == 0 && s->buf_pos == s->buf_used) 
    {
      do_cancel (s);
      DBG (10, ">>\n");
      return (SANE_STATUS_EOF);
    }
    
  if (!s->scanning) 
    {
      DBG (10, ">>\n");
      return(do_cancel(s));
    }

  if (s->buf_pos < s->buf_used)
    {
      transfer = s->buf_used - s->buf_pos;
      if (transfer > max_len)
        transfer = max_len;
      
      memcpy(dst_buf, &(s->buffer[s->buf_pos]), transfer);
      s->buf_pos += transfer;
      max_len -= transfer;
      *len = transfer;
    }

  while (max_len > 0 && s->bytes_to_read > 0) 
    {
      if (eight_bit_data)
        {
          nread = s->dev->info.bufsize / s->params.bytes_per_line - 1;
          nread *= s->params.bytes_per_line;
          if (nread > s->bytes_to_read)
            nread = s->bytes_to_read;
          max_line = nread / s->params.bytes_per_line;
          start_input = s->params.bytes_per_line;
          bytes_per_line_in = s->params.bytes_per_line;
        }
      else
        {
          bytes_per_line_in = (s->params.pixels_per_line + 7) / 8;
          bytes_per_line_in *= 3;
          max_line = s->params.bytes_per_line + bytes_per_line_in;
          max_line = s->dev->info.bufsize / max_line;
          nread = max_line * bytes_per_line_in;
          if (nread > s->bytes_to_read)
            {
              nread = s->bytes_to_read;
              max_line = nread / bytes_per_line_in;
            }
          start_input = s->dev->info.bufsize - nread;
        }
      ntest = nread;
      
#ifdef USE_FORK
      status = read_data (s, &(s->buffer[start_input]), &nread);
#else
      wait_ready(s->fd);
      status = read_data (s, &(s->buffer[start_input]), &nread);
#endif
      if (status != SANE_STATUS_GOOD)
        {
          do_cancel (s);
          DBG (10, ">>\n");
          return (SANE_STATUS_IO_ERROR);
        }
      
      if (nread != ntest) 
        {
          /* if this happens, something is wrong in the input buffer
             management...
          */
          DBG(1, "Warning: could not read an integral number of scan lines\n");
          DBG(1, "         image will be scrambled\n");
        }
      
      
      s->buf_used = max_line * s->params.bytes_per_line;
      s->buf_pos = 0;
      s->bytes_to_read -= nread;
      dest = s->buffer;
      max_pixel = s->params.pixels_per_line;

      if (eight_bit_data)
        for (line = 1; line <= max_line; line++)
          {
            red = &(s->buffer[line * s->params.bytes_per_line]);
            green = &(red[max_pixel]);
            blue = &(green[max_pixel]);
            for (pixel = 0; pixel < max_pixel; pixel++)
              {
                *dest++ = *red++;
                *dest++ = *green++;
                *dest++ = *blue++;
              }
          }
      else
        for (line = 0; line < max_line; line++)
          {
            red = &(s->buffer[start_input + line * bytes_per_line_in]);
            green = &(red[(max_pixel+7)/8]);
            blue = &(green[(max_pixel+7)/8]);
            mask = 0x80;
            for (pixel = 0; pixel < max_pixel; pixel++)
              {
                *dest++ = (*red & mask)   ? 0xff : 0;
                *dest++ = (*green & mask) ? 0xff : 0;
                *dest++ = (*blue & mask)  ? 0xff : 0;
                mask = mask >> 1;
                if (mask == 0)
                  {
                    mask = 0x80;
                    red++;
                    green++;
                    blue++;
                  }
              }
          }
      
      transfer = max_len;
      if (transfer > s->buf_used)
        transfer = s->buf_used;
      memcpy(&(dst_buf[*len]), s->buffer, transfer);
      
      max_len -= transfer;
      s->buf_pos += transfer;
      *len += transfer;
    }

  if (s->bytes_to_read == 0 && s->buf_pos == s->buf_used)
    do_cancel (s);
  DBG (10, ">>\n");
  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *dst_buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SHARP_Scanner *s = handle;
  SANE_Status status;

  s->busy = SANE_TRUE;
  if (s->cancel == SANE_TRUE) 
    {
      do_cancel(s);
      *len = 0;
      return (SANE_STATUS_CANCELLED);
    }
  
  /* RGB scans with a JX 250 and bi-level color scans
     must be handled differently: */
  if (s->image_composition <= 2)
    status = sane_read_direct(handle, dst_buf, max_len, len);
  else if (s->image_composition <= 4)
    status = sane_read_shuffled(handle, dst_buf, max_len, len, 0);
  else if (s->dev->sensedat.model != JX250 && s->dev->sensedat.model != JX350 )
    status = sane_read_direct(handle, dst_buf, max_len, len);
  else
    status = sane_read_shuffled(handle, dst_buf, max_len, len, 1);
  
  s->busy = SANE_FALSE;
  if (s->cancel == SANE_TRUE)
    {
      do_cancel(s);
      return (SANE_STATUS_CANCELLED);
    }

  return (status);
}

void
sane_cancel (SANE_Handle handle)
{
  SHARP_Scanner *s = handle;
  DBG (10, "<< sane_cancel ");

  s->cancel = SANE_TRUE;
  if (s->busy == SANE_FALSE)
      do_cancel(s);

  DBG (10, ">>\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ handle, 
		  SANE_Bool __sane_unused__ non_blocking)
{
  DBG (10, "<< sane_set_io_mode");
  DBG (10, ">>\n");

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ handle, 
		    SANE_Int __sane_unused__ * fd)
{
  DBG (10, "<< sane_get_select_fd");
  DBG (10, ">>\n");

  return SANE_STATUS_UNSUPPORTED;
}
