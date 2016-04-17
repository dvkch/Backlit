/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 BYTEC GmbH Germany
   Written by Helmut Koeberle, Email: helmut.koeberle@bytec.de
   Modified by Manuel Panea <Manuel.Panea@rzg.mpg.de>
   and Markus Mertinat <Markus.Mertinat@Physik.Uni-Augsburg.DE>
   FB620 and FB1200 support by Mitsuru Okaniwa <m-okaniwa@bea.hi-ho.ne.jp>
   FS2710 support by Ulrich Deiters <ulrich.deiters@uni-koeln.de>

   backend version: 1.13e

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
   If you do not wish that, delete this exception notice. */

/* This file implements the sane-api */

/* SANE-FLOW-DIAGRAMM

   - sane_init() : initialize backend, attach scanners(devicename,0)
   . - sane_get_devices() : query list of scanner-devices
   . - sane_open() : open a particular scanner-device and attach_scanner(devicename,&dev)
   . . - sane_set_io_mode : set blocking-mode
   . . - sane_get_select_fd : get scanner-fd
   . . - sane_get_option_descriptor() : get option informations
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image aquisition
   . .   - sane_get_parameters() : returns actual scan-parameters
   . .   - sane_read() : read image-data (from pipe)
   . . - sane_cancel() : cancel operation, kill reader_process
   
   . - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
   - sane_exit() : terminate use of backend, free devicename and device-struture
*/

/* This driver's flow:

 - sane_init
 . - attach_one
 . . - inquiry
 . . - test_unit_ready
 . . - medium_position
 . . - extended inquiry
 . . - mode sense
 . . - get_density_curve
 - sane_get_devices
 - sane_open
 . - init_options
 - sane_set_io_mode : set blocking-mode
 - sane_get_select_fd : get scanner-fd
 - sane_get_option_descriptor() : get option informations
 - sane_control_option() : change option values
 - sane_start() : start image aquisition
   - sane_get_parameters() : returns actual scan-parameters
   - sane_read() : read image-data (from pipe)
   - sane_cancel() : cancel operation, kill reader_process
 - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
 - sane_exit() : terminate use of backend, free devicename and device-struture
*/

#include "../include/sane/config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include <fcntl.h> /* for FB1200S */
#include <unistd.h> /* for FB1200S */
#include <errno.h> /* for FB1200S */

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"

#define BACKEND_NAME canon

#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#include "../include/sane/sanei_config.h"
#define CANON_CONFIG_FILE "canon.conf"

#include <canon.h>

#ifndef SANE_I18N
#define SANE_I18N(text)	text
#endif


static SANE_Byte primaryHigh[256], primaryLow[256], secondaryHigh[256],
		 secondaryLow[256];	/* modification for FB1200S */

static int num_devices = 0;
static CANON_Device *first_dev = NULL;
static CANON_Scanner *first_handle = NULL;

static const SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_HALFTONE,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  0     
};

/* modification for FS2710 */
static const SANE_String_Const mode_list_fs2710[] = {
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_I18N("Raw"), 0
};

/* modification for FB620S */
static const SANE_String_Const mode_list_fb620[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_I18N("Fine color"), 0
};

/* modification for FB1200S */
static const SANE_String_Const mode_list_fb1200[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  0
};

static const SANE_String_Const tpu_dc_mode_list[] = {
  SANE_I18N("No transparency correction"),
  SANE_I18N("Correction according to film type"),
  SANE_I18N("Correction according to transparency ratio"),
  0
};

static const SANE_String_Const filmtype_list[] = {
  SANE_I18N("Negatives"), SANE_I18N("Slides"),
  0
};

static const SANE_String_Const negative_filmtype_list[] = {
  "Kodak", "Fuji", "Agfa", "Konica",
  0
};

static const SANE_String_Const scanning_speed_list[] = {
  SANE_I18N("Automatic"), SANE_I18N("Normal speed"),
  SANE_I18N("1/2 normal speed"), SANE_I18N("1/3 normal speed"),
  0
};

static const SANE_String_Const tpu_filmtype_list[] = {
  "Film 0", "Film 1", "Film 2", "Film 3",
  0
};

static const SANE_String_Const papersize_list[] = {
  "A4", "Letter", "B5", "Maximal",
  0
};

/**************************************************/

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};

#include "canon-scsi.c"

/**************************************************************************/

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  DBG (11, ">> max_string_size\n");

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  DBG (11, "<< max_string_size\n");
  return max_size;
}

/**************************************************************************/

static void
get_tpu_stat (int fd, CANON_Device * dev)
{
  unsigned char tbuf[12 + 5];
  size_t buf_size, i;
  SANE_Status status;

  DBG (3, ">> get tpu stat\n");

  memset (tbuf, 0, sizeof (tbuf));
  buf_size = sizeof (tbuf);
  status = get_scan_mode (fd, TRANSPARENCY_UNIT, tbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "get scan mode failed: %s\n", sane_strstatus (status));
      return;
    }

  for (i = 0; i < buf_size; i++)
    DBG (3, "scan mode control byte[%d] = %d\n", (int) i, tbuf[i]);
  dev->tpu.Status = (tbuf[2 + 4 + 5] >> 7) ?
    TPU_STAT_INACTIVE : TPU_STAT_NONE;
  if (dev->tpu.Status != TPU_STAT_NONE)	/* TPU available */
    {
      dev->tpu.Status = (tbuf[2 + 4 + 5] & 0x04) ?
	TPU_STAT_INACTIVE : TPU_STAT_ACTIVE;
    }
  dev->tpu.ControlMode = tbuf[3 + 4 + 5] & 0x03;
  dev->tpu.Transparency = tbuf[4 + 4 + 5] * 256 + tbuf[5 + 4 + 5];
  dev->tpu.PosNeg = tbuf[6 + 4 + 5] & 0x01;
  dev->tpu.FilmType = tbuf[7 + 4 + 5];
  if(dev->tpu.FilmType > 3)
    dev->tpu.FilmType = 0;

  DBG (11, "TPU Status: %d\n", dev->tpu.Status);
  DBG (11, "TPU ControlMode: %d\n", dev->tpu.ControlMode);
  DBG (11, "TPU Transparency: %d\n", dev->tpu.Transparency);
  DBG (11, "TPU PosNeg: %d\n", dev->tpu.PosNeg);
  DBG (11, "TPU FilmType: %d\n", dev->tpu.FilmType);

  DBG (3, "<< get tpu stat\n");

  return;
}

/**************************************************************************/

static void
get_adf_stat (int fd, CANON_Device * dev)
{
  size_t buf_size = 0x0C, i;
  unsigned char abuf[0x0C];
  SANE_Status status;

  DBG (3, ">> get adf stat\n");

  memset (abuf, 0, buf_size);
  status = get_scan_mode (fd, AUTO_DOC_FEEDER_UNIT, abuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "get scan mode failed: %s\n", sane_strstatus (status));
      perror ("get scan mode failed");
      return;
    }

  for (i = 0; i < buf_size; i++)
    DBG (3, "scan mode control byte[%d] = %d\n", (int) i, abuf[i]);

  dev->adf.Status = (abuf[ADF_Status] & ADF_NOT_PRESENT) ?
    ADF_STAT_NONE : ADF_STAT_INACTIVE;

  if (dev->adf.Status != ADF_STAT_NONE)	/* ADF available / INACTIVE */
    {
      dev->adf.Status = (abuf[ADF_Status] & ADF_PROBLEM) ?
	ADF_STAT_INACTIVE : ADF_STAT_ACTIVE;
    }
  dev->adf.Problem = (abuf[ADF_Status] & ADF_PROBLEM);
  dev->adf.Priority = (abuf[ADF_Settings] & ADF_PRIORITY);
  dev->adf.Feeder = (abuf[ADF_Settings] & ADF_FEEDER);

  DBG (11, "ADF Status: %d\n", dev->adf.Status);
  DBG (11, "ADF Priority: %d\n", dev->adf.Priority);
  DBG (11, "ADF Problem: %d\n", dev->adf.Problem);
  DBG (11, "ADF Feeder: %d\n", dev->adf.Feeder);

  DBG (3, "<< get adf stat\n");
  return;
}

/**************************************************************************/

static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  static char me[] = "canon_sense_handler";
  u_char sense;
  int asc;
  char *sense_str = NULL;
  SANE_Status status;

  DBG (1, ">> sense_handler\n");
  DBG (11, "%s(%ld, %p, %p)\n", me, (long) scsi_fd, (void *) result,
    (void *) arg);
  DBG (11, "sense buffer: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
    "%02x %02x %02x %02x %02x %02x\n", result[0], result[1], result[2],
    result[3], result[4], result[5], result[6], result[7], result[8],
    result[9], result[10], result[11], result[12], result[13], result[14],
    result[15]);

  status = SANE_STATUS_GOOD;

  DBG(11, "sense data interpretation for SCSI-2 devices\n");
  sense = result[2] & 0x0f;		/* extract the sense key */
  if (result[7] > 3)		/* additional sense code available? */
    {
      asc = (result[12] << 8) + result[13];	/* 12: additional sense code */
    }					/* 13: a.s.c. qualifier */
  else
    asc = 0xffff;

  switch (sense)
    {
    case 0x00:
      DBG(11, "sense category: no error\n");
      status = SANE_STATUS_GOOD;
      break;

    case 0x01:
      DBG(11, "sense category: recovered error\n");
      switch (asc)
        {
        case 0x3700:
          sense_str = SANE_I18N("rounded parameter");
          break;
        default:
          sense_str = SANE_I18N("unknown");
        }
      status = SANE_STATUS_GOOD;
      break;

    case 0x03:
      DBG(11, "sense category: medium error\n");
      switch (asc)
        {
        case 0x8000:
          sense_str = SANE_I18N("ADF jam");
          break;
        case 0x8001:
          sense_str = SANE_I18N("ADF cover open");
          break;
        default:
          sense_str = SANE_I18N("unknown");
        }
      status = SANE_STATUS_IO_ERROR;
      break;

    case 0x04:
      DBG(11, "sense category: hardware error\n");
      switch (asc)
        {
        case 0x6000:
          sense_str = SANE_I18N("lamp failure");
          break;
        case 0x6200:
          sense_str = SANE_I18N("scan head positioning error");
          break;
        case 0x8001:
          sense_str = SANE_I18N("CPU check error");
          break;
        case 0x8002:
          sense_str = SANE_I18N("RAM check error");
          break;
        case 0x8003:
          sense_str = SANE_I18N("ROM check error");
          break;
        case 0x8004:
          sense_str = SANE_I18N("hardware check error");
          break;
        case 0x8005:
          sense_str = SANE_I18N("transparency unit lamp failure");
          break;
        case 0x8006:
          sense_str = SANE_I18N("transparency unit scan head "
          "positioning failure");
          break;
        default:
          sense_str = SANE_I18N("unknown");
        }
      status = SANE_STATUS_IO_ERROR;
      break;

    case 0x05:
      DBG(11, "sense category: illegal request\n");
      switch (asc)
        {
        case 0x1a00:
          sense_str = SANE_I18N("parameter list length error");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x2000:
          sense_str = SANE_I18N("invalid command operation code");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        case 0x2400:
          sense_str = SANE_I18N("invalid field in CDB");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x2500:
          sense_str = SANE_I18N("unsupported LUN");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        case 0x2600:
          sense_str = SANE_I18N("invalid field in parameter list");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        case 0x2c00:
          sense_str = SANE_I18N("command sequence error");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        case 0x2c01:
          sense_str = SANE_I18N("too many windows specified");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        case 0x3a00:
          sense_str = SANE_I18N("medium not present");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x3d00:
          sense_str = SANE_I18N("invalid bit IDENTIFY message");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        case 0x8002:
          sense_str = SANE_I18N("option not connect");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        default:
          sense_str = SANE_I18N("unknown");
          status = SANE_STATUS_UNSUPPORTED;
        }
      break;

    case 0x06:
      DBG(11, "sense category: unit attention\n");
      switch (asc)
        {
        case 0x2900:
          sense_str = SANE_I18N("power on reset / bus device reset");
          status = SANE_STATUS_GOOD;
          break;
        case 0x2a00:
          sense_str = SANE_I18N("parameter changed by another initiator");
          status = SANE_STATUS_IO_ERROR;
          break;
        default:
          sense_str = SANE_I18N("unknown");
          status = SANE_STATUS_IO_ERROR;
        }
      break;

    case 0x0b:
      DBG(11, "sense category: non-standard\n");
      switch (asc)
        {
        case 0x0000:
          sense_str = SANE_I18N("no additional sense information");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x4500:
          sense_str = SANE_I18N("reselect failure");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x4700:
          sense_str = SANE_I18N("SCSI parity error");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x4800:
          sense_str = SANE_I18N("initiator detected error message "
          "received");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x4900:
          sense_str = SANE_I18N("invalid message error");
          status = SANE_STATUS_UNSUPPORTED;
          break;
        case 0x8000:
          sense_str = SANE_I18N("timeout error");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x8001:
          sense_str = SANE_I18N("transparency unit shading error");
          status = SANE_STATUS_IO_ERROR;
          break;
        case 0x8003:
          sense_str = SANE_I18N("lamp not stabilized");
          status = SANE_STATUS_IO_ERROR;
          break;
        default:
          sense_str = SANE_I18N("unknown");
          status = SANE_STATUS_IO_ERROR;
        }
      break;
    default:
      DBG(11, "sense category: else\n");
    }
  DBG (11, "sense message: %s\n", sense_str);
#if 0					/* superfluous? [U.D.] */
  s->sense_str = sense_str;
#endif
  DBG (1, "<< sense_handler\n");
  return status;
}

/***************************************************************/
static SANE_Status
do_gamma (CANON_Scanner * s)
{
  SANE_Status status;
  u_char gbuf[256];
  size_t buf_size;
  int i, j, neg, transfer_data_type, from;


  DBG (7, "sending SET_DENSITY_CURVE\n");
  buf_size = 256 * sizeof (u_char);
  transfer_data_type = 0x03;

  neg = (s->hw->info.is_filmscanner) ?
    strcmp (filmtype_list[1], s->val[OPT_NEGATIVE].s)
    : s->val[OPT_HNEGATIVE].w;

  if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY))
    {
      /* If scanning in gray mode, use the first curve for the
         scanner's monochrome gamma component                    */
      for (j = 0; j < 256; j++)
	{
	  if (!neg)
	    {
	      gbuf[j] = (u_char) s->gamma_table[0][j];
	      DBG (22, "set_density %d: gbuf[%d] = [%d]\n", 0, j, gbuf[j]);
	    }
	  else
	    {
	      gbuf[255 - j] = (u_char) (255 - s->gamma_table[0][j]);
	      DBG (22, "set_density %d: gbuf[%d] = [%d]\n", 0, 255 - j,
		   gbuf[255 - j]);
	    }
	}
      if ((status = set_density_curve (s->fd, 0, gbuf, &buf_size,
	transfer_data_type)) != SANE_STATUS_GOOD)
	{
	  DBG (7, "SET_DENSITY_CURVE\n");
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (SANE_STATUS_INVAL);
	}
    }
  else
    {				/* colour mode */
      /* If in RGB mode but with gamma bind, use the first curve
         for all 3 colors red, green, blue */
      for (i = 1; i < 4; i++)
	{
	  from = (s->val[OPT_CUSTOM_GAMMA_BIND].w) ? 0 : i;
	  for (j = 0; j < 256; j++)
	    {
	      if (!neg)
		{
		  gbuf[j] = (u_char) s->gamma_table[from][j];
		  DBG (22, "set_density %d: gbuf[%d] = [%d]\n", i, j, gbuf[j]);
		}
	      else
		{
		  gbuf[255 - j] = (u_char) (255 - s->gamma_table[from][j]);
		  DBG (22, "set_density %d: gbuf[%d] = [%d]\n", i, 255 - j,
		       gbuf[255 - j]);
		}
	    }
	  if (s->hw->info.model == FS2710)
	    status = set_density_curve_fs2710 (s, i, gbuf);
	  else
	    {
	      if ((status = set_density_curve (s->fd, i, gbuf, &buf_size,
		transfer_data_type)) != SANE_STATUS_GOOD)
		{
		  DBG (7, "SET_DENSITY_CURVE\n");
		  sanei_scsi_close (s->fd);
		  s->fd = -1;
		  return (SANE_STATUS_INVAL);
		}
	    }
	}
    }

  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

static SANE_Status
attach (const char *devnam, CANON_Device ** devp)
{
  SANE_Status status;
  CANON_Device *dev;

  int fd;
  u_char ibuf[36], ebuf[74], mbuf[12];
  size_t buf_size, i;
  char *str;

  DBG (1, ">> attach\n");

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (!strcmp (dev->sane.name, devnam))
	{
	  if (devp) *devp = dev;
	  return (SANE_STATUS_GOOD);
	}
    }

  DBG (3, "attach: opening %s\n", devnam);
  status = sanei_scsi_open (devnam, &fd, sense_handler, dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: open failed: %s\n", sane_strstatus (status));
      return (status);
    }

  DBG (3, "attach: sending (standard) INQUIRY\n");
  memset (ibuf, 0, sizeof (ibuf));
  buf_size = sizeof (ibuf);
  status = inquiry (fd, 0, ibuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: inquiry failed: %s\n", sane_strstatus (status));
      sanei_scsi_close (fd);
      fd = -1;
      return (status);
    }

  if (ibuf[0] != 6
      || strncmp ((char *) (ibuf + 8), "CANON", 5) != 0
      || strncmp ((char *) (ibuf + 16), "IX-", 3) != 0)
    {
      DBG (1, "attach: device doesn't look like a Canon scanner\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending TEST_UNIT_READY\n");
  status = test_unit_ready (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: test unit ready failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      fd = -1;
      return (status);
    }

#if 0
  DBG (3, "attach: sending REQUEST SENSE\n");
  memset (sbuf, 0, sizeof (sbuf));
  buf_size = sizeof (sbuf);
  status = request_sense (fd, sbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: REQUEST_SENSE failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

  DBG (3, "attach: sending MEDIUM POSITION\n");
  status = medium_position (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MEDIUM POSITION failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }
/*   s->val[OPT_AF_NOW].w == SANE_TRUE; */
#endif

  DBG (3, "attach: sending RESERVE UNIT\n");
  status = reserve_unit (fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: RESERVE UNIT failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

#if 0
  DBG (3, "attach: sending GET SCAN MODE for transparency unit\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = sizeof (ebuf);
  buf_size = 12;
  status = get_scan_mode (fd, TRANSPARENCY_UNIT, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: GET SCAN MODE for transparency unit failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    DBG(3, "scan mode trans byte[%d] = %d\n", i, ebuf[i]);
#endif

  DBG (3, "attach: sending GET SCAN MODE for scan control conditions\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = sizeof (ebuf);
  status = get_scan_mode (fd, SCAN_CONTROL_CONDITIONS, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    {
      DBG (3, "scan mode byte[%d] = %d\n", (int) i, ebuf[i]);
    }

  DBG (3, "attach: sending (extended) INQUIRY\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = sizeof (ebuf);
  status = inquiry (fd, 1, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: (extended) INQUIRY failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

#if 0
  DBG (3, "attach: sending GET SCAN MODE for transparency unit\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 64;
  status = get_scan_mode (fd, ALL_SCAN_MODE_PAGES,	/* transparency unit */
			  ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    DBG (3, "scan mode control byte[%d] = %d\n", i, ebuf[i]);
#endif

#if 0
  DBG (3, "attach: sending GET SCAN MODE for all scan mode pages\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 32;
  status = get_scan_mode (fd, (u_char)ALL_SCAN_MODE_PAGES, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n");
      sanei_scsi_close (fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    DBG(3, "scan mode control byte[%d] = %d\n", i, ebuf[i]);
#endif

  DBG (3, "attach: sending MODE SENSE\n");
  memset (mbuf, 0, sizeof (mbuf));
  buf_size = sizeof (mbuf);
  status = mode_sense (fd, mbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MODE_SENSE failed\n");
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_INVAL);
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    {
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_NO_MEM);
    }
  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devnam);
  dev->sane.vendor = "CANON";
  if ((str = calloc (16 + 1, 1)) == NULL)
    {
      sanei_scsi_close (fd);
      fd = -1;
      return (SANE_STATUS_NO_MEM);
    }
  strncpy (str, (char *) (ibuf + 16), 16);
  dev->sane.model = str;

  /* Register the fixed properties of the scanner below:
     - whether it is a film scanner or a flatbed scanner
     - whether it can have an automatic document feeder (ADF)
     - whether it can be equipped with a transparency unit (TPU)
     - whether it has got focus control
     - whether it can optimize image parameters (autoexposure)
     - whether it can calibrate itself
     - whether it can diagnose itself
     - whether it can eject the media
     - whether it can mirror the scanned data
     - whether it is a film scanner (or can be used as one)
     - whether it has fixed, hardware-set scan resolutions only
  */
  if (!strncmp (str, "IX-27015", 8))		/* FS2700S */
    {
      dev->info.model = CS2700;
      dev->sane.type = SANE_I18N("film scanner");
      dev->adf.Status = ADF_STAT_NONE;
      dev->tpu.Status = TPU_STAT_NONE;
      dev->info.can_focus = SANE_TRUE;
      dev->info.can_autoexpose = SANE_TRUE;
      dev->info.can_calibrate = SANE_FALSE;
      dev->info.can_diagnose = SANE_FALSE;
      dev->info.can_eject = SANE_TRUE;
      dev->info.can_mirror = SANE_TRUE;
      dev->info.is_filmscanner = SANE_TRUE;
      dev->info.has_fixed_resolutions = SANE_TRUE;
    }
  else if (!strncmp (str, "IX-27025E", 9))	/* FS2710S */
    {
      dev->info.model = FS2710;
      dev->sane.type = SANE_I18N("film scanner");
      dev->adf.Status = ADF_STAT_NONE;
      dev->tpu.Status = TPU_STAT_NONE;
      dev->info.can_focus = SANE_TRUE;
      dev->info.can_autoexpose = SANE_TRUE;
      dev->info.can_calibrate = SANE_FALSE;
      dev->info.can_diagnose = SANE_FALSE;
      dev->info.can_eject = SANE_TRUE;
      dev->info.can_mirror = SANE_TRUE;
      dev->info.is_filmscanner = SANE_TRUE;
      dev->info.has_fixed_resolutions = SANE_TRUE;
    }
  else if (!strncmp (str, "IX-06035E", 9))	/* FB620S */
    {
      dev->info.model = FB620;
      dev->sane.type = SANE_I18N("flatbed scanner");
      dev->adf.Status = ADF_STAT_NONE;
      dev->tpu.Status = TPU_STAT_NONE;
      dev->info.can_focus = SANE_FALSE;
      dev->info.can_autoexpose = SANE_FALSE;
      dev->info.can_calibrate = SANE_TRUE;
      dev->info.can_diagnose = SANE_TRUE;
      dev->info.can_eject = SANE_FALSE;
      dev->info.can_mirror = SANE_FALSE;
      dev->info.is_filmscanner = SANE_FALSE;
      dev->info.has_fixed_resolutions = SANE_TRUE;
    }
  else if (!strncmp (str, "IX-12015E", 9))	/* FB1200S */
    {
      dev->info.model = FB1200;
      dev->sane.type = SANE_I18N("flatbed scanner");
      dev->adf.Status = ADF_STAT_INACTIVE;
      dev->tpu.Status = TPU_STAT_INACTIVE;
      dev->info.can_focus = SANE_FALSE;
      dev->info.can_autoexpose = SANE_FALSE;
      dev->info.can_calibrate = SANE_FALSE;
      dev->info.can_diagnose = SANE_FALSE;
      dev->info.can_eject = SANE_FALSE;
      dev->info.can_mirror = SANE_FALSE;
      dev->info.is_filmscanner = SANE_FALSE;
      dev->info.has_fixed_resolutions = SANE_TRUE;
    }
  else if (!strncmp (str, "IX-4015", 7))	/* IX-4015 */
    {
      dev->info.model = IX4015;
      dev->sane.type = SANE_I18N("flatbed scanner");
      dev->adf.Status = ADF_STAT_INACTIVE;
      dev->tpu.Status = TPU_STAT_INACTIVE;
      dev->info.can_focus = SANE_FALSE;
      dev->info.can_autoexpose = SANE_TRUE;
      dev->info.can_calibrate = SANE_FALSE;
      dev->info.can_diagnose = SANE_TRUE;
      dev->info.can_eject = SANE_FALSE;
      dev->info.can_mirror = SANE_TRUE;
      dev->info.is_filmscanner = SANE_FALSE;
      dev->info.has_fixed_resolutions = SANE_FALSE;
    }
  else						/* CS300, CS600 */
    {
      dev->info.model = CS3_600;
      dev->sane.type = SANE_I18N("flatbed scanner");
      dev->adf.Status = ADF_STAT_INACTIVE;
      dev->tpu.Status = TPU_STAT_INACTIVE;
      dev->info.can_focus = SANE_FALSE;
      dev->info.can_autoexpose = SANE_FALSE;
      dev->info.can_calibrate = SANE_FALSE;
      dev->info.can_diagnose = SANE_FALSE;
      dev->info.can_eject = SANE_FALSE;
      dev->info.can_mirror = SANE_TRUE;
      dev->info.is_filmscanner = SANE_FALSE;
      dev->info.has_fixed_resolutions = SANE_FALSE;
    }

  DBG (5, "dev->sane.name = '%s'\n", dev->sane.name);
  DBG (5, "dev->sane.vendor = '%s'\n", dev->sane.vendor);
  DBG (5, "dev->sane.model = '%s'\n", dev->sane.model);
  DBG (5, "dev->sane.type = '%s'\n", dev->sane.type);

  if (dev->tpu.Status != TPU_STAT_NONE)
    get_tpu_stat (fd, dev);		/* Query TPU */
  if (dev->adf.Status != ADF_STAT_NONE)
    get_adf_stat (fd, dev);		/* Query ADF */

  dev->info.bmu = mbuf[6];
  DBG (5, "bmu=%d\n", dev->info.bmu);
  dev->info.mud = (mbuf[8] << 8) + mbuf[9];
  DBG (5, "mud=%d\n", dev->info.mud);

  dev->info.xres_default = (ebuf[5] << 8) + ebuf[6];
  DBG (5, "xres_default=%d\n", dev->info.xres_default);
  dev->info.xres_range.max = (ebuf[10] << 8) + ebuf[11];
  DBG (5, "xres_range.max=%d\n", dev->info.xres_range.max);
  dev->info.xres_range.min = (ebuf[14] << 8) + ebuf[15];
  DBG (5, "xres_range.min=%d\n", dev->info.xres_range.min);
  dev->info.xres_range.quant = ebuf[9] >> 4;
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);

  dev->info.yres_default = (ebuf[7] << 8) + ebuf[8];
  DBG (5, "yres_default=%d\n", dev->info.yres_default);
  dev->info.yres_range.max = (ebuf[12] << 8) + ebuf[13];
  DBG (5, "yres_range.max=%d\n", dev->info.yres_range.max);
  dev->info.yres_range.min = (ebuf[16] << 8) + ebuf[17];
  DBG (5, "yres_range.min=%d\n", dev->info.yres_range.min);
  dev->info.yres_range.quant = ebuf[9] & 0x0f;
  DBG (5, "xres_range.quant=%d\n", dev->info.xres_range.quant);

  dev->info.x_range.min = SANE_FIX (0.0);
  dev->info.x_range.max = (ebuf[20] << 24) + (ebuf[21] << 16)
    + (ebuf[22] << 8) + ebuf[23] - 1;
  dev->info.x_range.max =
    SANE_FIX (dev->info.x_range.max * MM_PER_INCH / dev->info.mud);
  DBG (5, "x_range.max=%d\n", dev->info.x_range.max);
  dev->info.x_range.quant = 0;

  dev->info.y_range.min = SANE_FIX (0.0);
  dev->info.y_range.max = (ebuf[24] << 24) + (ebuf[25] << 16)
    + (ebuf[26] << 8) + ebuf[27] - 1;
  dev->info.y_range.max =
    SANE_FIX (dev->info.y_range.max * MM_PER_INCH / dev->info.mud);
  DBG (5, "y_range.max=%d\n", dev->info.y_range.max);
  dev->info.y_range.quant = 0;

  dev->info.x_adf_range.max = (ebuf[30] << 24) + (ebuf[31] << 16)
    + (ebuf[32] << 8) + ebuf[33] - 1;
  DBG (5, "x_adf_range.max=%d\n", dev->info.x_adf_range.max);
  dev->info.y_adf_range.max = (ebuf[34] << 24) + (ebuf[35] << 16)
    + (ebuf[36] << 8) + ebuf[37] - 1;
  DBG (5, "y_adf_range.max=%d\n", dev->info.y_adf_range.max);

  dev->info.brightness_range.min = 0;
  dev->info.brightness_range.max = 255;
  dev->info.brightness_range.quant = 0;

  dev->info.contrast_range.min = 1;
  dev->info.contrast_range.max = 255;
  dev->info.contrast_range.quant = 0;

  dev->info.threshold_range.min = 1;
  dev->info.threshold_range.max = 255;
  dev->info.threshold_range.quant = 0;

  dev->info.HiliteR_range.min = 0;
  dev->info.HiliteR_range.max = 255;
  dev->info.HiliteR_range.quant = 0;

  dev->info.ShadowR_range.min = 0;
  dev->info.ShadowR_range.max = 254;
  dev->info.ShadowR_range.quant = 0;

  dev->info.HiliteG_range.min = 0;
  dev->info.HiliteG_range.max = 255;
  dev->info.HiliteG_range.quant = 0;

  dev->info.ShadowG_range.min = 0;
  dev->info.ShadowG_range.max = 254;
  dev->info.ShadowG_range.quant = 0;

  dev->info.HiliteB_range.min = 0;
  dev->info.HiliteB_range.max = 255;
  dev->info.HiliteB_range.quant = 0;

  dev->info.ShadowB_range.min = 0;
  dev->info.ShadowB_range.max = 254;
  dev->info.ShadowB_range.quant = 0;

  dev->info.focus_range.min = 0;
  dev->info.focus_range.max = 255;
  dev->info.focus_range.quant = 0;

  dev->info.TPU_Transparency_range.min = 0;
  dev->info.TPU_Transparency_range.max = 10000;
  dev->info.TPU_Transparency_range.quant = 100;

  sanei_scsi_close (fd);
  fd = -1;

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  DBG (1, "<< attach\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

static SANE_Status
do_cancel (CANON_Scanner * s)
{
  SANE_Status status;

  DBG (1, ">> do_cancel\n");

  s->scanning = SANE_FALSE;

  if (s->fd >= 0)
    {
      if (s->val[OPT_EJECT_AFTERSCAN].w && !(s->val[OPT_PREVIEW].w
	&& s->hw->info.is_filmscanner))
	{
	  DBG (3, "do_cancel: sending MEDIUM POSITION\n");
	  status = medium_position (s->fd);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1, "do_cancel: MEDIUM POSITION failed\n");
	      return (SANE_STATUS_INVAL);
	    }
	  s->AF_NOW = SANE_TRUE;
	  DBG (1, "do_cancel AF_NOW = '%d'\n", s->AF_NOW);
	}

      DBG (21, "do_cancel: reset_flag = %d\n", s->reset_flag);
      if ((s->reset_flag == 1) && (s->hw->info.model == FB620))
	{
	  status = reset_scanner (s->fd);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (21, "RESET SCANNER failed\n");
	      sanei_scsi_close (s->fd);
	      s->fd = -1;
	      return (SANE_STATUS_INVAL);
	    }
	  DBG (21, "RESET SCANNER\n");
	  s->reset_flag = 0;
	  DBG (21, "do_cancel: reset_flag = %d\n", s->reset_flag);
	  s->time0 = -1;
	  DBG (21, "time0 = %ld\n", s->time0);
	}

      if (s->hw->info.model == FB1200)
	{
	  DBG (3, "CANCEL FB1200S\n");
	  status = cancel (s->fd);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1, "CANCEL FB1200S failed\n");
	      return (SANE_STATUS_INVAL);
	    }
	  DBG (3, "CANCEL FB1200S OK\n");
	}

      sanei_scsi_close (s->fd);
      s->fd = -1;
    }

  DBG (1, "<< do_cancel\n");
  return (SANE_STATUS_CANCELLED);
}

/**************************************************************************/

static SANE_Status
init_options (CANON_Scanner * s)
{
  int i;
  DBG (1, ">> init_options\n");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  s->AF_NOW = SANE_TRUE;

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = SANE_I18N("Scan mode");
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;

  switch (s->hw->info.model)
    {
    case FB620:
      s->opt[OPT_MODE].size = max_string_size (mode_list_fb620);
      s->opt[OPT_MODE].constraint.string_list = mode_list_fb620;
      s->val[OPT_MODE].s = strdup (mode_list_fb620[3]);
      break;
    case FB1200:
      s->opt[OPT_MODE].size = max_string_size (mode_list_fb1200);
      s->opt[OPT_MODE].constraint.string_list = mode_list_fb1200;
      s->val[OPT_MODE].s = strdup (mode_list_fb1200[2]);
      break;
    case FS2710:
      s->opt[OPT_MODE].size = max_string_size (mode_list_fs2710);
      s->opt[OPT_MODE].constraint.string_list = mode_list_fs2710;
      s->val[OPT_MODE].s = strdup (mode_list_fs2710[0]);
      break;
    default:
      s->opt[OPT_MODE].size = max_string_size (mode_list);
      s->opt[OPT_MODE].constraint.string_list = mode_list;
      s->val[OPT_MODE].s = strdup (mode_list[3]);
    }

  /* Slides or negatives */
  s->opt[OPT_NEGATIVE].name = "film-type";
  s->opt[OPT_NEGATIVE].title = SANE_I18N("Film type");
  s->opt[OPT_NEGATIVE].desc = SANE_I18N("Selects the film type, i.e. "
  "negatives or slides");
  s->opt[OPT_NEGATIVE].type = SANE_TYPE_STRING;
  s->opt[OPT_NEGATIVE].size = max_string_size (filmtype_list);
  s->opt[OPT_NEGATIVE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_NEGATIVE].constraint.string_list = filmtype_list;
  s->opt[OPT_NEGATIVE].cap |=
    (s->hw->info.is_filmscanner)? 0 : SANE_CAP_INACTIVE;
  s->val[OPT_NEGATIVE].s = strdup (filmtype_list[1]);

  /* Negative film type */
  s->opt[OPT_NEGATIVE_TYPE].name = "negative-film-type";
  s->opt[OPT_NEGATIVE_TYPE].title = SANE_I18N("Negative film type");
  s->opt[OPT_NEGATIVE_TYPE].desc = SANE_I18N("Selects the negative film type");
  s->opt[OPT_NEGATIVE_TYPE].type = SANE_TYPE_STRING;
  s->opt[OPT_NEGATIVE_TYPE].size = max_string_size (negative_filmtype_list);
  s->opt[OPT_NEGATIVE_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_NEGATIVE_TYPE].constraint.string_list = negative_filmtype_list;
  s->opt[OPT_NEGATIVE_TYPE].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_NEGATIVE_TYPE].s = strdup (negative_filmtype_list[0]);

  /* Scanning speed */
  s->opt[OPT_SCANNING_SPEED].name = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SCANNING_SPEED].title = SANE_TITLE_SCAN_SPEED;
  s->opt[OPT_SCANNING_SPEED].desc = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SCANNING_SPEED].type = SANE_TYPE_STRING;
  s->opt[OPT_SCANNING_SPEED].size = max_string_size (scanning_speed_list);
  s->opt[OPT_SCANNING_SPEED].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCANNING_SPEED].constraint.string_list = scanning_speed_list;
  s->opt[OPT_SCANNING_SPEED].cap |=
    (s->hw->info.model == CS2700) ? 0 : SANE_CAP_INACTIVE;
  if (s->hw->info.model != CS2700)
    s->opt[OPT_SCANNING_SPEED].cap &= ~SANE_CAP_SOFT_SELECT;
  s->val[OPT_SCANNING_SPEED].s = strdup (scanning_speed_list[0]);


  /* "Resolution" group: */
  s->opt[OPT_RESOLUTION_GROUP].title = SANE_I18N("Scan resolution");
  s->opt[OPT_RESOLUTION_GROUP].desc = "";
  s->opt[OPT_RESOLUTION_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_RESOLUTION_GROUP].cap = 0;
  s->opt[OPT_RESOLUTION_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* bind resolution */
  s->opt[OPT_RESOLUTION_BIND].name = SANE_NAME_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].title = SANE_TITLE_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].desc = SANE_DESC_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].type = SANE_TYPE_BOOL;
  s->val[OPT_RESOLUTION_BIND].w = SANE_TRUE;

  /* hardware resolutions only */
  s->opt[OPT_HW_RESOLUTION_ONLY].name = "hw-resolution-only";
  s->opt[OPT_HW_RESOLUTION_ONLY].title = SANE_I18N("Hardware resolution");
  s->opt[OPT_HW_RESOLUTION_ONLY].desc = SANE_I18N("Use only hardware "
  "resolutions");
  s->opt[OPT_HW_RESOLUTION_ONLY].type = SANE_TYPE_BOOL;
  s->val[OPT_HW_RESOLUTION_ONLY].w = SANE_TRUE;
  s->opt[OPT_HW_RESOLUTION_ONLY].cap |=
    (s->hw->info.has_fixed_resolutions)? 0 : SANE_CAP_INACTIVE;

  /* x-resolution */
  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
  if (s->hw->info.has_fixed_resolutions)
    {
      int iCnt;
      float iRes;		/* modification for FB620S */
      s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      iCnt = 0;

      iRes = s->hw->info.xres_range.max;
      DBG (5, "hw->info.xres_range.max=%d\n", s->hw->info.xres_range.max);
      s->opt[OPT_X_RESOLUTION].constraint.word_list = s->xres_word_list;

      /* go to minimum resolution by dividing by 2 */
      while (iRes >= s->hw->info.xres_range.min)
	iRes /= 2;
      /* fill array up to maximum resolution */
      while (iRes < s->hw->info.xres_range.max)
	{
	  iRes *= 2;
	  s->xres_word_list[++iCnt] = iRes;
	}
      s->xres_word_list[0] = iCnt;
      s->val[OPT_X_RESOLUTION].w = s->xres_word_list[2];
    }
  else
    {
      s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
      s->opt[OPT_X_RESOLUTION].constraint.range = &s->hw->info.xres_range;
      s->val[OPT_X_RESOLUTION].w = 300;
    }

  /* y-resolution */
  s->opt[OPT_Y_RESOLUTION].name = SANE_NAME_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
  if (s->hw->info.has_fixed_resolutions)
    {
      int iCnt;
      float iRes;		/* modification for FB620S */
      s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
      iCnt = 0;

      iRes = s->hw->info.yres_range.max;
      DBG (5, "hw->info.yres_range.max=%d\n", s->hw->info.yres_range.max);
      s->opt[OPT_Y_RESOLUTION].constraint.word_list = s->yres_word_list;

      /* go to minimum resolution by dividing by 2 */
      while (iRes >= s->hw->info.yres_range.min)
	iRes /= 2;
      /* fill array up to maximum resolution */
      while (iRes < s->hw->info.yres_range.max)
	{
	  iRes *= 2;
	  s->yres_word_list[++iCnt] = iRes;
	}
      s->yres_word_list[0] = iCnt;
      s->val[OPT_Y_RESOLUTION].w = s->yres_word_list[2];
    }
  else
    {
      s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
      s->opt[OPT_Y_RESOLUTION].constraint.range = &s->hw->info.yres_range;
      s->val[OPT_Y_RESOLUTION].w = 300;
    }

  /* Focus group: */
  s->opt[OPT_FOCUS_GROUP].title = SANE_I18N("Focus");
  s->opt[OPT_FOCUS_GROUP].desc = "";
  s->opt[OPT_FOCUS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_FOCUS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_FOCUS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_FOCUS_GROUP].cap |=
    (s->hw->info.can_focus) ? 0 : SANE_CAP_INACTIVE;

  /* Auto-Focus switch */
  s->opt[OPT_AF].name = "af";
  s->opt[OPT_AF].title = SANE_I18N("Auto focus");
  s->opt[OPT_AF].desc = SANE_I18N("Enable/disable auto focus");
  s->opt[OPT_AF].type = SANE_TYPE_BOOL;
  s->opt[OPT_AF].cap |= (s->hw->info.can_focus) ? 0 : SANE_CAP_INACTIVE;
  s->val[OPT_AF].w = s->hw->info.can_focus;

  /* Auto-Focus once switch */
  s->opt[OPT_AF_ONCE].name = "afonce";
  s->opt[OPT_AF_ONCE].title = SANE_I18N("Auto focus only once");
  s->opt[OPT_AF_ONCE].desc = SANE_I18N("Do auto focus only once between "
  "ejects");
  s->opt[OPT_AF_ONCE].type = SANE_TYPE_BOOL;
  s->opt[OPT_AF_ONCE].cap |= (s->hw->info.can_focus) ? 0 : SANE_CAP_INACTIVE;
  s->val[OPT_AF_ONCE].w = s->hw->info.can_focus;

  /* Manual focus */
  s->opt[OPT_FOCUS].name = "focus";
  s->opt[OPT_FOCUS].title = SANE_I18N("Manual focus position");
  s->opt[OPT_FOCUS].desc = SANE_I18N("Set the optical system's focus "
  "position by hand (default: 128).");
  s->opt[OPT_FOCUS].type = SANE_TYPE_INT;
  s->opt[OPT_FOCUS].unit = SANE_UNIT_NONE;
  s->opt[OPT_FOCUS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_FOCUS].constraint.range = &s->hw->info.focus_range;
  s->opt[OPT_FOCUS].cap |= (s->hw->info.can_focus) ? 0 : SANE_CAP_INACTIVE;
  s->val[OPT_FOCUS].w = (s->hw->info.can_focus) ? 128 : 0;

  /* Margins group: */
  s->opt[OPT_MARGINS_GROUP].title = SANE_I18N("Scan margins");
  s->opt[OPT_MARGINS_GROUP].desc = "";
  s->opt[OPT_MARGINS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MARGINS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_MARGINS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->info.x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->info.y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->info.x_range;
  s->val[OPT_BR_X].w = s->hw->info.x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->info.y_range;
  s->val[OPT_BR_Y].w = s->hw->info.y_range.max;

  /* Colors group: */
  s->opt[OPT_COLORS_GROUP].title = SANE_I18N("Extra color adjustments");
  s->opt[OPT_COLORS_GROUP].desc = "";
  s->opt[OPT_COLORS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_COLORS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_COLORS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Positive/Negative switch for the CanoScan 300/600 models */
  s->opt[OPT_HNEGATIVE].name = SANE_NAME_NEGATIVE;
  s->opt[OPT_HNEGATIVE].title = SANE_TITLE_NEGATIVE;
  s->opt[OPT_HNEGATIVE].desc = SANE_DESC_NEGATIVE;
  s->opt[OPT_HNEGATIVE].type = SANE_TYPE_BOOL;
  s->opt[OPT_HNEGATIVE].cap |=
    (s->hw->info.model == CS2700 || s->hw->info.model == FS2710) ?
    SANE_CAP_INACTIVE : 0;
  s->val[OPT_HNEGATIVE].w = SANE_FALSE;

  /* Same values for highlight and shadow points for red, green, blue */
  s->opt[OPT_BIND_HILO].name = "bind-highlight-shadow-points";
  s->opt[OPT_BIND_HILO].title = SANE_TITLE_RGB_BIND;
  s->opt[OPT_BIND_HILO].desc = SANE_DESC_RGB_BIND;
  s->opt[OPT_BIND_HILO].type = SANE_TYPE_BOOL;
  s->opt[OPT_BIND_HILO].cap |= (s->hw->info.model == FB620 ||
    s->hw->info.model == IX4015) ? SANE_CAP_INACTIVE : 0;
  s->val[OPT_BIND_HILO].w = SANE_TRUE;

  /* highlight point for red   */
  s->opt[OPT_HILITE_R].name = SANE_NAME_HIGHLIGHT_R;
  s->opt[OPT_HILITE_R].title = SANE_TITLE_HIGHLIGHT_R;
  s->opt[OPT_HILITE_R].desc = SANE_DESC_HIGHLIGHT_R;
  s->opt[OPT_HILITE_R].type = SANE_TYPE_INT;
  s->opt[OPT_HILITE_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_HILITE_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HILITE_R].constraint.range = &s->hw->info.HiliteR_range;
  s->opt[OPT_HILITE_R].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_HILITE_R].w = 255;

  /* shadow point for red   */
  s->opt[OPT_SHADOW_R].name = SANE_NAME_SHADOW_R;
  s->opt[OPT_SHADOW_R].title = SANE_TITLE_SHADOW_R;
  s->opt[OPT_SHADOW_R].desc = SANE_DESC_SHADOW_R;
  s->opt[OPT_SHADOW_R].type = SANE_TYPE_INT;
  s->opt[OPT_SHADOW_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_SHADOW_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SHADOW_R].constraint.range = &s->hw->info.ShadowR_range;
  s->opt[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_SHADOW_R].w = 0;

  /* highlight point for green */
  s->opt[OPT_HILITE_G].name = SANE_NAME_HIGHLIGHT;
  s->opt[OPT_HILITE_G].title = SANE_TITLE_HIGHLIGHT;
  s->opt[OPT_HILITE_G].desc = SANE_DESC_HIGHLIGHT;
  s->opt[OPT_HILITE_G].type = SANE_TYPE_INT;
  s->opt[OPT_HILITE_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_HILITE_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HILITE_G].constraint.range = &s->hw->info.HiliteG_range;
  s->opt[OPT_HILITE_G].cap |=
    (s->hw->info.model == IX4015) ? SANE_CAP_INACTIVE : 0;
  s->val[OPT_HILITE_G].w = 255;

  /* shadow point for green */
  s->opt[OPT_SHADOW_G].name = SANE_NAME_SHADOW;
  s->opt[OPT_SHADOW_G].title = SANE_TITLE_SHADOW;
  s->opt[OPT_SHADOW_G].desc = SANE_DESC_SHADOW;
  s->opt[OPT_SHADOW_G].type = SANE_TYPE_INT;
  s->opt[OPT_SHADOW_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_SHADOW_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SHADOW_G].constraint.range = &s->hw->info.ShadowG_range;
  s->opt[OPT_SHADOW_G].cap |=
    (s->hw->info.model == IX4015) ? SANE_CAP_INACTIVE : 0;
  s->val[OPT_SHADOW_G].w = 0;

  /* highlight point for blue  */
  s->opt[OPT_HILITE_B].name = SANE_NAME_HIGHLIGHT_B;
  s->opt[OPT_HILITE_B].title = SANE_TITLE_HIGHLIGHT_B;
  s->opt[OPT_HILITE_B].desc = SANE_DESC_HIGHLIGHT_B;
  s->opt[OPT_HILITE_B].type = SANE_TYPE_INT;
  s->opt[OPT_HILITE_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_HILITE_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HILITE_B].constraint.range = &s->hw->info.HiliteB_range;
  s->opt[OPT_HILITE_B].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_HILITE_B].w = 255;

  /* shadow point for blue  */
  s->opt[OPT_SHADOW_B].name = SANE_NAME_SHADOW_B;
  s->opt[OPT_SHADOW_B].title = SANE_TITLE_SHADOW_B;
  s->opt[OPT_SHADOW_B].desc = SANE_DESC_SHADOW_B;
  s->opt[OPT_SHADOW_B].type = SANE_TYPE_INT;
  s->opt[OPT_SHADOW_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_SHADOW_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SHADOW_B].constraint.range = &s->hw->info.ShadowB_range;
  s->opt[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_SHADOW_B].w = 0;


  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N("Enhancement");
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &s->hw->info.brightness_range;
  s->opt[OPT_BRIGHTNESS].cap |= 0;
  s->val[OPT_BRIGHTNESS].w = 128;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &s->hw->info.contrast_range;
  s->opt[OPT_CONTRAST].cap |= 0;
  s->val[OPT_CONTRAST].w = 128;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &s->hw->info.threshold_range;
  s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_THRESHOLD].w = 128;

  s->opt[OPT_MIRROR].name = "mirror";
  s->opt[OPT_MIRROR].title = SANE_I18N("Mirror image");
  s->opt[OPT_MIRROR].desc = SANE_I18N("Mirror the image horizontally");
  s->opt[OPT_MIRROR].type = SANE_TYPE_BOOL;
  s->opt[OPT_MIRROR].cap |= (s->hw->info.can_mirror) ? 0: SANE_CAP_INACTIVE;
  s->val[OPT_MIRROR].w = SANE_FALSE;

  /* analog-gamma curve */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* bind analog-gamma */
  s->opt[OPT_CUSTOM_GAMMA_BIND].name = "bind-custom-gamma";
  s->opt[OPT_CUSTOM_GAMMA_BIND].title = SANE_TITLE_RGB_BIND;
  s->opt[OPT_CUSTOM_GAMMA_BIND].desc = SANE_DESC_RGB_BIND;
  s->opt[OPT_CUSTOM_GAMMA_BIND].type = SANE_TYPE_BOOL;
  s->opt[OPT_CUSTOM_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CUSTOM_GAMMA_BIND].w = SANE_TRUE;

  /* grayscale gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
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
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
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
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
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
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];

  s->opt[OPT_AE].name = "ae";
  s->opt[OPT_AE].title = SANE_I18N("Auto exposure");
  s->opt[OPT_AE].desc = SANE_I18N("Enable/disable the auto exposure feature");
  s->opt[OPT_AE].cap |= (s->hw->info.can_autoexpose) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_AE].type = SANE_TYPE_BOOL;
  s->val[OPT_AE].w = SANE_FALSE;


  /* "Calibration" group */
  s->opt[OPT_CALIBRATION_GROUP].title = SANE_I18N("Calibration");
  s->opt[OPT_CALIBRATION_GROUP].desc = "";
  s->opt[OPT_CALIBRATION_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_CALIBRATION_GROUP].cap |= (s->hw->info.can_calibrate ||
    s->hw->info.can_diagnose) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_CALIBRATION_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* calibration now */
  s->opt[OPT_CALIBRATION_NOW].name = "calibration-now";
  s->opt[OPT_CALIBRATION_NOW].title = SANE_I18N("Calibration now");
  s->opt[OPT_CALIBRATION_NOW].desc = SANE_I18N("Execute calibration *now*");
  s->opt[OPT_CALIBRATION_NOW].type = SANE_TYPE_BUTTON;
  s->opt[OPT_CALIBRATION_NOW].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATION_NOW].cap |=
    (s->hw->info.can_calibrate) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_CALIBRATION_NOW].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_CALIBRATION_NOW].constraint.range = NULL;

  /* scanner self diagnostic */
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].name = "self-diagnostic";
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].title = SANE_I18N("Self diagnosis");
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].desc = SANE_I18N("Perform scanner "
  "self diagnosis");
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].type = SANE_TYPE_BUTTON;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].unit = SANE_UNIT_NONE;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].cap |=
    (s->hw->info.can_diagnose) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_SCANNER_SELF_DIAGNOSTIC].constraint.range = NULL;

  /* reset scanner for FB620S */
  s->opt[OPT_RESET_SCANNER].name = "reset-scanner";
  s->opt[OPT_RESET_SCANNER].title = SANE_I18N("Reset scanner");
  s->opt[OPT_RESET_SCANNER].desc = SANE_I18N("Reset the scanner");
  s->opt[OPT_RESET_SCANNER].type = SANE_TYPE_BUTTON;
  s->opt[OPT_RESET_SCANNER].unit = SANE_UNIT_NONE;
  s->opt[OPT_RESET_SCANNER].cap |=
    (s->hw->info.model == FB620) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_RESET_SCANNER].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_RESET_SCANNER].constraint.range = NULL;


  /* "Eject" group (active only for film scanners) */
  s->opt[OPT_EJECT_GROUP].title = SANE_I18N("Medium handling");
  s->opt[OPT_EJECT_GROUP].desc = "";
  s->opt[OPT_EJECT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_EJECT_GROUP].cap |=
    (s->hw->info.can_eject) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_EJECT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* eject after scan */
  s->opt[OPT_EJECT_AFTERSCAN].name = "eject-after-scan";
  s->opt[OPT_EJECT_AFTERSCAN].title = SANE_I18N("Eject film after each scan");
  s->opt[OPT_EJECT_AFTERSCAN].desc = SANE_I18N("Automatically eject the "
  "film from the device after each scan");
  s->opt[OPT_EJECT_AFTERSCAN].cap |=
    (s->hw->info.can_eject) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_EJECT_AFTERSCAN].type = SANE_TYPE_BOOL;
  /* IX-4015 requires medium_position command after cancel */
  s->val[OPT_EJECT_AFTERSCAN].w =
    (s->hw->info.model == IX4015) ? SANE_TRUE : SANE_FALSE;

  /* eject before exit */
  s->opt[OPT_EJECT_BEFOREEXIT].name = "eject-before-exit";
  s->opt[OPT_EJECT_BEFOREEXIT].title = SANE_I18N("Eject film before exit");
  s->opt[OPT_EJECT_BEFOREEXIT].desc = SANE_I18N("Automatically eject the "
  "film from the device before exiting the program");
  s->opt[OPT_EJECT_BEFOREEXIT].cap |=
    (s->hw->info.can_eject) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_EJECT_BEFOREEXIT].type = SANE_TYPE_BOOL;
  s->val[OPT_EJECT_BEFOREEXIT].w = s->hw->info.can_eject;

  /* eject now */
  s->opt[OPT_EJECT_NOW].name = "eject-now";
  s->opt[OPT_EJECT_NOW].title = SANE_I18N("Eject film now");
  s->opt[OPT_EJECT_NOW].desc = SANE_I18N("Eject the film *now*");
  s->opt[OPT_EJECT_NOW].type = SANE_TYPE_BUTTON;
  s->opt[OPT_EJECT_NOW].unit = SANE_UNIT_NONE;
  s->opt[OPT_EJECT_NOW].cap |=
    (s->hw->info.can_eject) ? 0 : SANE_CAP_INACTIVE;
  s->opt[OPT_EJECT_NOW].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_EJECT_NOW].constraint.range = NULL;

  /* "NO-ADF" option: */
  s->opt[OPT_ADF_GROUP].title = SANE_I18N("Document feeder extras");
  s->opt[OPT_ADF_GROUP].desc = "";
  s->opt[OPT_ADF_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ADF_GROUP].cap = 0;
  s->opt[OPT_ADF_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_FLATBED_ONLY].name = "noadf";
  s->opt[OPT_FLATBED_ONLY].title = SANE_I18N("Flatbed only");
  s->opt[OPT_FLATBED_ONLY].desc = SANE_I18N("Disable auto document feeder "
  "and use flatbed only");
  s->opt[OPT_FLATBED_ONLY].type = SANE_TYPE_BOOL;
  s->opt[OPT_FLATBED_ONLY].unit = SANE_UNIT_NONE;
  s->opt[OPT_FLATBED_ONLY].size = sizeof (SANE_Word);
  s->opt[OPT_FLATBED_ONLY].cap |=
    (s->hw->adf.Status == ADF_STAT_NONE) ? SANE_CAP_INACTIVE : 0;
  s->val[OPT_FLATBED_ONLY].w = SANE_FALSE;

  /* "TPU" group: */
  s->opt[OPT_TPU_GROUP].title = SANE_I18N("Transparency unit");
  s->opt[OPT_TPU_GROUP].desc = "";
  s->opt[OPT_TPU_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_TPU_GROUP].cap = 0;
  s->opt[OPT_TPU_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_TPU_GROUP].cap |=
    (s->hw->tpu.Status != TPU_STAT_NONE) ? 0 : SANE_CAP_INACTIVE;

  /* Transparency Unit (FAU, Film Adapter Unit) */
  s->opt[OPT_TPU_ON].name = "transparency-unit-on-off";
  s->opt[OPT_TPU_ON].title = SANE_I18N("Transparency unit");
  s->opt[OPT_TPU_ON].desc = SANE_I18N("Switch on/off the transparency unit "
  "(FAU, film adapter unit)");
  s->opt[OPT_TPU_ON].type = SANE_TYPE_BOOL;
  s->opt[OPT_TPU_ON].unit = SANE_UNIT_NONE;
  s->val[OPT_TPU_ON].w =
    (s->hw->tpu.Status == TPU_STAT_ACTIVE) ? SANE_TRUE : SANE_FALSE;
  s->opt[OPT_TPU_ON].cap |=
    (s->hw->tpu.Status != TPU_STAT_NONE) ? 0 : SANE_CAP_INACTIVE;

  s->opt[OPT_TPU_PN].name = "transparency-unit-negative-film";
  s->opt[OPT_TPU_PN].title = SANE_I18N("Negative film");
  s->opt[OPT_TPU_PN].desc = SANE_I18N("Positive or negative film");
  s->opt[OPT_TPU_PN].type = SANE_TYPE_BOOL;
  s->opt[OPT_TPU_PN].unit = SANE_UNIT_NONE;
  s->val[OPT_TPU_PN].w = s->hw->tpu.PosNeg;
  s->opt[OPT_TPU_PN].cap |=
    (s->hw->tpu.Status == TPU_STAT_ACTIVE) ? 0 : SANE_CAP_INACTIVE;

  /* density control mode */
  s->opt[OPT_TPU_DCM].name = "TPMDC";
  s->opt[OPT_TPU_DCM].title = SANE_I18N("Density control");
  s->opt[OPT_TPU_DCM].desc = SANE_I18N("Set density control mode");
  s->opt[OPT_TPU_DCM].type = SANE_TYPE_STRING;
  s->opt[OPT_TPU_DCM].size = max_string_size (tpu_dc_mode_list);
  s->opt[OPT_TPU_DCM].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_TPU_DCM].constraint.string_list = tpu_dc_mode_list;
  s->val[OPT_TPU_DCM].s = strdup (tpu_dc_mode_list[s->hw->tpu.ControlMode]);
  s->opt[OPT_TPU_DCM].cap |=
    (s->hw->tpu.Status == TPU_STAT_ACTIVE) ? 0 : SANE_CAP_INACTIVE;

  /* Transparency Ratio */
  s->opt[OPT_TPU_TRANSPARENCY].name = "Transparency-Ratio";
  s->opt[OPT_TPU_TRANSPARENCY].title = SANE_I18N("Transparency ratio");
  s->opt[OPT_TPU_TRANSPARENCY].desc = "";
  s->opt[OPT_TPU_TRANSPARENCY].type = SANE_TYPE_INT;
  s->opt[OPT_TPU_TRANSPARENCY].unit = SANE_UNIT_NONE;
  s->opt[OPT_TPU_TRANSPARENCY].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TPU_TRANSPARENCY].constraint.range =
    &s->hw->info.TPU_Transparency_range;
  s->val[OPT_TPU_TRANSPARENCY].w = s->hw->tpu.Transparency;
  s->opt[OPT_TPU_TRANSPARENCY].cap |=
    (s->hw->tpu.Status == TPU_STAT_ACTIVE &&
     s->hw->tpu.ControlMode == 3) ? 0 : SANE_CAP_INACTIVE;

  /* Select Film type */
  s->opt[OPT_TPU_FILMTYPE].name = "Filmtype";
  s->opt[OPT_TPU_FILMTYPE].title = SANE_I18N("Select film type");
  s->opt[OPT_TPU_FILMTYPE].desc = SANE_I18N("Select the film type");
  s->opt[OPT_TPU_FILMTYPE].type = SANE_TYPE_STRING;
  s->opt[OPT_TPU_FILMTYPE].size = max_string_size (tpu_filmtype_list);
  s->opt[OPT_TPU_FILMTYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_TPU_FILMTYPE].constraint.string_list = tpu_filmtype_list;
  s->val[OPT_TPU_FILMTYPE].s =
    strdup (tpu_filmtype_list[s->hw->tpu.FilmType]);
  s->opt[OPT_TPU_FILMTYPE].cap |=
    (s->hw->tpu.Status == TPU_STAT_ACTIVE && s->hw->tpu.ControlMode == 1) ?
    0 : SANE_CAP_INACTIVE;


  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  DBG (1, "<< init_options\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

static SANE_Status
attach_one (const char *dev)
{
  DBG (1, ">> attach_one\n");
  attach (dev, 0);
  DBG (1, "<< attach_one\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

static SANE_Status
do_focus (CANON_Scanner * s)
{
  SANE_Status status;
  u_char ebuf[74];
  size_t buf_size;

  DBG (3, "do_focus: sending GET FILM STATUS\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 4;
  status = get_film_status (s->fd, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "do_focus: GET FILM STATUS failed\n");
      if (status == SANE_STATUS_UNSUPPORTED)
	return (SANE_STATUS_GOOD);
      else
	{
	  DBG (1, "do_focus: ... for unknown reasons\n");
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (SANE_STATUS_INVAL);
	}
    }
  DBG (3, "focus point before autofocus : %d\n", ebuf[3]);

  status = execute_auto_focus (s->fd, s->val[OPT_AF].w,
    (s->scanning_speed == 0 && !s->RIF && s->hw->info.model == CS2700),
    (int) s->AE, s->val[OPT_FOCUS].w);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (7, "execute_auto_focus failed\n");
      if (status == SANE_STATUS_UNSUPPORTED)
	  return (SANE_STATUS_GOOD);
      else
	{
	  DBG (1, "do_focus: ... for unknown reasons\n");
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (SANE_STATUS_INVAL);
	}
    }

  DBG (3, "do_focus: sending GET FILM STATUS\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 4;
  status = get_film_status (s->fd, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "do_focus: GET FILM STATUS failed\n");
      if (status == SANE_STATUS_UNSUPPORTED)
	  return (SANE_STATUS_GOOD);
      else
	{
	  DBG (1, "do_focus: ... for unknown reasons\n");
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (SANE_STATUS_INVAL);
	}
    }
  else
      DBG (3, "focus point after autofocus : %d\n", ebuf[3]);

  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

#include "canon-sane.c"
