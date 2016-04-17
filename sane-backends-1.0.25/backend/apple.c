/* sane - Scanner Access Now Easy.

   Copyright (C) 1998 Milon Firikis based on David Mosberger-Tang previous
   Work on mustek.c file from the SANE package.

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

   This file implements a SANE backend for Apple flatbed scanners.  */

#include "../include/sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/_stdint.h"

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"


/* SCSI commands that the Apple scanners understand: */
#define APPLE_SCSI_TEST_UNIT_READY	0x00
#define APPLE_SCSI_REQUEST_SENSE	0x03
#define APPLE_SCSI_INQUIRY		0x12
#define APPLE_SCSI_MODE_SELECT		0x15
#define APPLE_SCSI_RESERVE		0x16
#define APPLE_SCSI_RELEASE		0x17
#define APPLE_SCSI_START		0x1b
#define APPLE_SCSI_AREA_AND_WINDOWS	0x24
#define APPLE_SCSI_READ_SCANNED_DATA	0x28
#define APPLE_SCSI_GET_DATA_STATUS	0x34


#define INQ_LEN	0x60

#define ENABLE(OPTION)  s->opt[OPTION].cap &= ~SANE_CAP_INACTIVE
#define DISABLE(OPTION) s->opt[OPTION].cap |=  SANE_CAP_INACTIVE
#define IS_ACTIVE(OPTION) (((s->opt[OPTION].cap) & SANE_CAP_INACTIVE) == 0)

#define XQSTEP(XRES,BPP) (SANE_Int) (((double) (8*1200)) / ((double) (XRES*BPP)))
#define YQSTEP(YRES) (SANE_Int) (((double) (1200)) / ((double) (YRES)))


/* Very low info, Apple Scanners only */

/* TODO: Ok I admit it. I am not so clever to do this operations with bitwised
   operators. Sorry. */

#define STORE8(p,v)				\
  {						\
  *(p)=(v);					\
  }

#define STORE16(p,v)				\
  {						\
  *(p)=(v)/256;					\
  *(p+1)=(v-*(p)*256);				\
  }

#define STORE24(p,v)				\
  {						\
  *(p)=(v)/65536;				\
  *(p+1)=(v-*(p)*65536)/256;			\
  *(p+2)=(v-*(p)*65536-*(p+1)*256);		\
  }


#define STORE32(p,v)				\
  {						\
  *(p)=(v)/16777216;				\
  *(p+1)=(v-*(p)*16777216)/65536;		\
  *(p+2)=(v-*(p)*16777216-*(p+1)*65536)/256;	\
  *(p+3)=(v-*(p)*16777216-*(p+1)*65536-*(p+2)*256);\
  }

#define READ24(p) *(p)*65536 + *(p+1)*256 + *(p+2)

#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include "../include/sane/sanei_config.h"
#define APPLE_CONFIG_FILE "apple.conf"

#include "apple.h"


static const SANE_Device **devlist = 0;
static int num_devices;
static Apple_Device *first_dev;
static Apple_Scanner *first_handle;


static SANE_String_Const mode_list[6];

static SANE_String_Const SupportedModel[] =
{
"3",
"AppleScanner 4bit, 16 Shades of Gray",
"OneScanner 8bit, 256 Shades of Gray",
"ColorOneScanner, RGB color 8bit per band",
NULL
};

static const SANE_String_Const graymap_list[] =
{
  "dark", "normal", "light",
  0
};

#if 0
static const SANE_Int resbit4_list[] =
{
  5,
  75, 100, 150, 200, 300
};

static const SANE_Int resbit1_list[] =
{
  17,
  75, 90, 100, 120, 135, 150, 165, 180, 195,
  200, 210, 225, 240, 255, 270, 285, 300
};
#endif

static const SANE_Int resbit_list[] =
{
  5,
  75, 100, 150, 200, 300
};

static const SANE_String_Const speed_list[] =
{
  "normal", "high", "high wo H/S",
  0
};

static SANE_String_Const halftone_pattern_list[6];

static const SANE_String_Const color_sensor_list[] =
{
  "All", "Red", "Green", "Blue",
  0
};

/* NOTE: This is used for Brightness, Contrast, Threshold, AutoBackAdj
   and 0 is the default value */
static const SANE_Range byte_range =
{
  1, 255, 1
};

static const SANE_Range u8_range =
{
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};


/* NOTE: However I can select from different lists during the hardware
   probing time. */




static const uint8_t inquiry[] =
{
  APPLE_SCSI_INQUIRY, 0x00, 0x00, 0x00, INQ_LEN, 0x00
};

static const uint8_t test_unit_ready[] =
{
  APPLE_SCSI_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
};


#if 0
SANE_Int 
xqstep (unsigned int Xres, unsigned int bpp)
{
  return (SANE_Int) ((double) (8 * 1200)) / ((double) (Xres * bpp));
}


SANE_Int 
yqstep (unsigned int Yres, unsigned int bpp)
{
  return (SANE_Int) ((double) (1200)) / ((double) (Yres));
}
#endif



/* The functions below return the quantized value of x,y in scanners dots
   aka 1/1200 of an inch */

static SANE_Int 
xquant (double x, unsigned int Xres, unsigned int bpp, int dir)
{
  double tmp;
  unsigned int t;

  tmp = (double) x *Xres * bpp / (double) 8;
  t = (unsigned int) tmp;

  if (tmp - ((double) t) >= 0.1)
    if (dir)
      t++;;

  return t * 8 * 1200 / (Xres * bpp);
}



static SANE_Int 
yquant (double y, unsigned int Yres, int dir)
{
  double tmp;
  unsigned int t;

  tmp = (double) y *Yres;
  t = (unsigned int) tmp;

  if (tmp - ((double) t) >= 0.1)
    if (dir)
      t++;;

  return t * 1200 / Yres;
}

static SANE_Status
wait_ready (int fd)
{
#define MAX_WAITING_TIME	60	/* one minute, at most */
  struct timeval now, start;
  SANE_Status status;

#ifdef NEUTRALIZE_BACKEND
return SANE_STATUS_GOOD;
#else

  gettimeofday (&start, 0);

  while (1)
    {
      DBG (USER_MESSAGE, "wait_ready: sending TEST_UNIT_READY\n");

      status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready),
			       0, 0);
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG (ERROR_MESSAGE, "wait_ready: test unit ready failed (%s)\n",
	       sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  gettimeofday (&now, 0);
	  if (now.tv_sec - start.tv_sec >= MAX_WAITING_TIME)
	    {
	      DBG (ERROR_MESSAGE, "wait_ready: timed out after %lu seconds\n",
		   (u_long) now.tv_sec - start.tv_sec);
	      return SANE_STATUS_INVAL;
	    }
	  usleep (100000);	/* retry after 100ms */
	  break;

	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  return SANE_STATUS_INVAL;
#endif /* NEUTRALIZE_BACKEND */
}

static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  scsi_fd = scsi_fd;			/* silence gcc */
  arg = arg;					/* silence gcc */

  switch (result[2] & 0x0F)
    {
    case 0:
      DBG (USER_MESSAGE, "Sense: No sense Error\n");
      return SANE_STATUS_GOOD;
    case 2:
      DBG (ERROR_MESSAGE, "Sense: Scanner not ready\n");
      return SANE_STATUS_DEVICE_BUSY;
    case 4:
      DBG (ERROR_MESSAGE, "Sense: Hardware Error. Read more...\n");
      return SANE_STATUS_IO_ERROR;
    case 5:
      DBG (ERROR_MESSAGE, "Sense: Illegall request\n");
      return SANE_STATUS_UNSUPPORTED;
    case 6:
      DBG (ERROR_MESSAGE, "Sense: Unit Attention (Wait until scanner "
	   "boots)\n");
      return SANE_STATUS_DEVICE_BUSY;
    case 9:
      DBG (ERROR_MESSAGE, "Sense: Vendor Unique. Read more...\n");
      return SANE_STATUS_IO_ERROR;
    default:
      DBG (ERROR_MESSAGE, "Sense: Unknown Sense Key. Read more...\n");
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;
}


static SANE_Status
request_sense (Apple_Scanner * s)
{
  uint8_t cmd[6];
  uint8_t result[22];
  size_t size = sizeof (result);
  SANE_Status status;

  memset (cmd, 0, sizeof (cmd));
  memset (result, 0, sizeof (result));

#ifdef NEUTRALIZE_BACKEND
return SANE_STATUS_GOOD;
#else

  cmd[0] = APPLE_SCSI_REQUEST_SENSE;
  STORE8 (cmd + 4, sizeof (result));
  sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), result, &size);

  if (result[7] != 14)
    {
      DBG (ERROR_MESSAGE, "Additional Length %u\n", (unsigned int) result[7]);
      status = SANE_STATUS_IO_ERROR;
    }


  status = sense_handler (s->fd, result, NULL);
  if (status == SANE_STATUS_IO_ERROR)
    {

/* Now we are checking for Harware and Vendor Unique Errors for all models */
/* First check the common Error conditions */

      if (result[18] & 0x80)
	DBG (ERROR_MESSAGE, "Sense: Dim Light (output of lamp below 70%%).\n");

      if (result[18] & 0x40)
	DBG (ERROR_MESSAGE, "Sense: No Light at all.\n");

      if (result[18] & 0x20)
	DBG (ERROR_MESSAGE, "Sense: No Home.\n");

      if (result[18] & 0x10)
	DBG (ERROR_MESSAGE, "Sense: No Limit. Tried to scan out of range.\n");


      switch (s->hw->ScannerModel)
	{
	case APPLESCANNER:
	  if (result[18] & 0x08)
	    DBG (ERROR_MESSAGE, "Sense: Shade Error. Failed Calibration.\n");
	  if (result[18] & 0x04)
	    DBG (ERROR_MESSAGE, "Sense: ROM Error.\n");
	  if (result[18] & 0x02)
	    DBG (ERROR_MESSAGE, "Sense: RAM Error.\n");
	  if (result[18] & 0x01)
	    DBG (ERROR_MESSAGE, "Sense: CPU Error.\n");
	  if (result[19] & 0x80)
	    DBG (ERROR_MESSAGE, "Sense: DIPP Error.\n");
	  if (result[19] & 0x40)
	    DBG (ERROR_MESSAGE, "Sense: DMA Error.\n");
	  if (result[19] & 0x20)
	    DBG (ERROR_MESSAGE, "Sense: GA1 Error.\n");
	  break;
	case ONESCANNER:
	  if (result[18] & 0x08)
	    DBG (ERROR_MESSAGE, "Sense: CCD clock generator failed.\n");
	  if (result[18] & 0x04)
	    DBG (ERROR_MESSAGE, "Sense: LRAM (Line RAM) Error.\n");
	  if (result[18] & 0x02)
	    DBG (ERROR_MESSAGE, "Sense: CRAM (Correction RAM) Error.\n");
	  if (result[18] & 0x01)
	    DBG (ERROR_MESSAGE, "Sense: ROM Error.\n");
	  if (result[19] & 0x08)
	    DBG (ERROR_MESSAGE, "Sense: SRAM Error.\n");
	  if (result[19] & 0x04)
	    DBG (ERROR_MESSAGE, "Sense: CPU Error.\n");
	  break;
	case COLORONESCANNER:
	  if (result[18] & 0x08)
	    DBG (ERROR_MESSAGE, "Sense: Calibration cirquit cannot "
		 "support normal shading.\n");
	  if (result[18] & 0x04)
	    DBG (ERROR_MESSAGE, "Sense: PSRAM (Correction RAM) Error.\n");
	  if (result[18] & 0x02)
	    DBG (ERROR_MESSAGE, "Sense: SRAM Error.\n");
	  if (result[18] & 0x01)
	    DBG (ERROR_MESSAGE, "Sense: ROM Error.\n");
	  if (result[19] & 0x10)
	    DBG (ERROR_MESSAGE, "Sense: ICP (CPU) Error.\n");
	  if (result[19] & 0x02)
	    DBG (ERROR_MESSAGE, "Sense: Over light. (Too bright lamp ?).\n");
	  break;
	default:
	  DBG (ERROR_MESSAGE,
	       "Sense: Unselected Scanner model. Please report this.\n");
	  break;
	}
    }

  DBG (USER_MESSAGE, "Sense: Optical gain %u.\n", (unsigned int) result[20]);
  return status;
#endif /* NEUTRALIZE_BACKEND */
}





static SANE_Status
attach (const char *devname, Apple_Device ** devp, int may_wait)
{
  char result[INQ_LEN];
  const char *model_name = result + 44;
  int fd, apple_scanner, fw_revision;
  Apple_Device *dev;
  SANE_Status status;
  size_t size;

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	return SANE_STATUS_GOOD;
      }

  DBG (USER_MESSAGE, "attach: opening %s\n", devname);

#ifdef NEUTRALIZE_BACKEND
result[0]=0x06;
strcpy(result +  8, "APPLE   ");

if (APPLE_MODEL_SELECT==APPLESCANNER)
  strcpy(result + 16, "SCANNER A9M0337 ");
if (APPLE_MODEL_SELECT==ONESCANNER)
  strcpy(result + 16, "SCANNER II      ");
if (APPLE_MODEL_SELECT==COLORONESCANNER)
  strcpy(result + 16, "SCANNER III     ");

#else
  status = sanei_scsi_open (devname, &fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "attach: open failed (%s)\n",
	   sane_strstatus (status));
      return SANE_STATUS_INVAL;
    }

  if (may_wait)
    wait_ready (fd);

  DBG (USER_MESSAGE, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = sanei_scsi_cmd (fd, inquiry, sizeof (inquiry), result, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "attach: inquiry failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return status;
    }

  status = wait_ready (fd);
  sanei_scsi_close (fd);
  if (status != SANE_STATUS_GOOD)
    return status;
#endif /* NEUTRALIZE_BACKEND */

  /* check for old format: */
  apple_scanner = (strncmp (result + 8, "APPLE   ", 8) == 0);
  model_name = result + 16;

  apple_scanner = apple_scanner && (result[0] == 0x06);

  if (!apple_scanner)
    {
      DBG (ERROR_MESSAGE, "attach: device doesn't look like an Apple scanner"
	   "(result[0]=%#02x)\n", result[0]);
      return SANE_STATUS_INVAL;
    }

  /* get firmware revision as BCD number: */
  fw_revision =
    (result[32] - '0') << 8 | (result[34] - '0') << 4 | (result[35] - '0');
  DBG (USER_MESSAGE, "attach: firmware revision %d.%02x\n",
       fw_revision >> 8, fw_revision & 0xff);

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;

  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devname);
  dev->sane.vendor = "Apple";
  dev->sane.model = strndup (model_name, 16);
  dev->sane.type = "flatbed scanner";

  dev->x_range.min = 0;
  dev->x_range.max = SANE_FIX (8.51 * MM_PER_INCH);
  dev->x_range.quant = 0;

  dev->y_range.min = 0;
  dev->y_range.max = SANE_FIX (14.0 * MM_PER_INCH);
  dev->y_range.quant = 0;

  dev->MaxHeight = 16800;

  if (strncmp (model_name, "SCANNER A9M0337 ", 16) == 0)
    {
      dev->ScannerModel = APPLESCANNER;
      dev->dpi_range.min = SANE_FIX (75);
      dev->dpi_range.max = SANE_FIX (300);
      dev->dpi_range.quant = SANE_FIX (1);
      dev->MaxWidth = 10208;
    }
  else if (strncmp (model_name, "SCANNER II      ", 16) == 0)
    {
      dev->ScannerModel = ONESCANNER;
      dev->dpi_range.min = SANE_FIX (72);
      dev->dpi_range.max = SANE_FIX (300);
      dev->dpi_range.quant = SANE_FIX (1);
      dev->MaxWidth = 10200;
    }
  else if (strncmp (model_name, "SCANNER III     ", 16) == 0)
    {
      dev->ScannerModel = COLORONESCANNER;
      dev->dpi_range.min = SANE_FIX (72);
      dev->dpi_range.max = SANE_FIX (300);
      dev->dpi_range.quant = SANE_FIX (1);
      dev->MaxWidth = 10200;
    }
  else
    {
      DBG (ERROR_MESSAGE,
	   "attach: Cannot found Apple scanner in the neighborhood\n");
      free (dev);
      return SANE_STATUS_INVAL;
    }

  DBG (USER_MESSAGE, "attach: found Apple scanner model %s (%s)\n",
       dev->sane.model, dev->sane.type);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  return SANE_STATUS_GOOD;
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}


static SANE_Status
scan_area_and_windows (Apple_Scanner * s)
{
  uint8_t cmd[10 + 8 + 42];
#define CMD cmd + 0
#define WH  cmd + 10
#define WP  WH + 8

#ifdef NEUTRALIZE_BACKEND
return SANE_STATUS_GOOD;
#else

  /* setup SCSI command (except length): */
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = APPLE_SCSI_AREA_AND_WINDOWS;


  if (s->hw->ScannerModel == COLORONESCANNER)
    {
      STORE24 (CMD + 6, 50);
      STORE16 (WH + 6, 42);
    }
  else
    {
      STORE24 (CMD + 6, 48);
      STORE16 (WH + 6, 40);
    }

/* Store resolution. First X, the Y */

  STORE16 (WP + 2, s->val[OPT_RESOLUTION].w);
  STORE16 (WP + 4, s->val[OPT_RESOLUTION].w);

/* Now the Scanner Window in Scanner Parameters */

  STORE32 (WP + 6, s->ULx);
  STORE32 (WP + 10, s->ULy);
  STORE32 (WP + 14, s->Width);
  STORE32 (WP + 18, s->Height);

/* Now The Enhansment Group */

  STORE8 (WP + 22, s->val[OPT_BRIGHTNESS].w);
  STORE8 (WP + 23, s->val[OPT_THRESHOLD].w);
  STORE8 (WP + 24, s->val[OPT_CONTRAST].w);

/* The Mode */

  if      (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART))
    STORE8 (WP + 25, 0)
  else if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_HALFTONE))
    STORE8 (WP + 25, 1)
  else if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY) ||
	   !strcmp (s->val[OPT_MODE].s, "Gray16"))
    STORE8 (WP + 25, 2)
  else if (!strcmp (s->val[OPT_MODE].s, "BiColor"))
    STORE8 (WP + 25, 3)
  else if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR))
    STORE8 (WP + 25, 5)
  else
    {
      DBG (ERROR_MESSAGE, "Cannot much mode %s\n", s->val[OPT_MODE].s);
      return SANE_STATUS_INVAL;
    }

  STORE8 (WP + 26, s->bpp)

/* HalfTone */
if (s->hw->ScannerModel != COLORONESCANNER)
  {
  if	  (!strcmp (s->val[OPT_HALFTONE_PATTERN].s, "spiral4x4"))
    STORE16 (WP + 27, 0)
  else if (!strcmp (s->val[OPT_HALFTONE_PATTERN].s, "bayer4x4"))
    STORE16 (WP + 27, 1)
  else if (!strcmp (s->val[OPT_HALFTONE_PATTERN].s, "download"))
    STORE16 (WP + 27, 1)
  else if (!strcmp (s->val[OPT_HALFTONE_PATTERN].s, "spiral8x8"))
    STORE16 (WP + 27, 3)
  else if (!strcmp (s->val[OPT_HALFTONE_PATTERN].s, "bayer8x8"))
    STORE16 (WP + 27, 4)
  else
    {
      DBG (ERROR_MESSAGE, "Cannot much haftone pattern %s\n",
					s->val[OPT_HALFTONE_PATTERN].s);
      return SANE_STATUS_INVAL;
    }
  }
/* Padding Type */
  STORE8 (WP + 29, 3);

  if (s->hw->ScannerModel == COLORONESCANNER)
    {
    if (s->val[OPT_VOLT_REF].w)
      {
      STORE8(WP+40,s->val[OPT_VOLT_REF_TOP].w);
      STORE8(WP+41,s->val[OPT_VOLT_REF_BOTTOM].w);
      }
    else
      {
      STORE8(WP+40,0);
      STORE8(WP+41,0);
      }
    return sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
    }
  else
    return sanei_scsi_cmd (s->fd, cmd, sizeof (cmd) - 2, 0, 0);

#endif /* NEUTRALIZE_BACKEND */
}

static SANE_Status
mode_select (Apple_Scanner * s)
{
  uint8_t cmd[6 + 12];
#define CMD cmd + 0
#define PP  cmd + 6

  /* setup SCSI command (except length): */
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = APPLE_SCSI_MODE_SELECT;

/* Apple Hardware Magic */
  STORE8 (CMD + 1, 0x10);

/* Parameter list length */
  STORE8 (CMD + 4, 12);

  STORE8 (PP + 5, 6);

  if (s->val[OPT_LAMP].w) *(PP+8) |= 1;

  switch (s->hw->ScannerModel)
    {
    case APPLESCANNER:
      if      (!strcmp (s->val[OPT_GRAYMAP].s, "dark"))
	STORE8 (PP + 6, 0)
      else if (!strcmp (s->val[OPT_GRAYMAP].s, "normal"))
	STORE8 (PP + 6, 1)
      else if (!strcmp (s->val[OPT_GRAYMAP].s, "light"))
	STORE8 (PP + 6, 2)
      else
	{
	DBG (ERROR_MESSAGE, "Cannot mach GrayMap Function %s\n",
						s->val[OPT_GRAYMAP].s);
	return SANE_STATUS_INVAL;
	}
				/* And the auto background threshold */
      STORE8 (PP + 7, s->val[OPT_AUTOBACKGROUND_THRESHOLD].w)
      break;
    case ONESCANNER:
      if (s->val[OPT_LED].w) *(PP+7) |= 4;
      if (s->val[OPT_CCD].w) *(PP+8) |= 2;
      if      (!strcmp (s->val[OPT_SPEED].s, "high"))
	*(PP+8) |= 4;
      else if (!strcmp (s->val[OPT_SPEED].s, "high wo H/S"))
	*(PP+8) |= 8;
      else if (!strcmp (s->val[OPT_SPEED].s, "normal"))
	{ /* Do nothing. Zeros are great */}
      else
	{
	DBG (ERROR_MESSAGE, "Cannot mach speed selection %s\n",
						s->val[OPT_SPEED].s);
	return SANE_STATUS_INVAL;
	}
      break;
    case COLORONESCANNER:
      if (s->val[OPT_LED].w)		*(PP+7) |= 4;
      if (!s->val[OPT_CUSTOM_GAMMA].w)	*(PP+7) |= 2;
      if (!s->val[OPT_CUSTOM_CCT].w)	*(PP+7) |= 1;
      if (s->val[OPT_MTF_CIRCUIT].w)	*(PP+8) |= 16;
      if (s->val[OPT_ICP].w)		*(PP+8) |= 8;
      if (s->val[OPT_POLARITY].w)	*(PP+8) |= 4;
      if (s->val[OPT_CCD].w)		*(PP+8) |= 2;

      if      (!strcmp (s->val[OPT_COLOR_SENSOR].s, "All"))
	STORE8 (PP + 9, 0)
      else if (!strcmp (s->val[OPT_COLOR_SENSOR].s, "Red"))
	STORE8 (PP + 9, 1)
      else if (!strcmp (s->val[OPT_COLOR_SENSOR].s, "Green"))
	STORE8 (PP + 9, 2)
      else if (!strcmp (s->val[OPT_COLOR_SENSOR].s, "Blue"))
	STORE8 (PP + 9, 3)
      else
	{
	DBG (ERROR_MESSAGE, "Cannot mach Color Sensor for gray scans %s\n",
						s->val[OPT_COLOR_SENSOR].s);
	return SANE_STATUS_INVAL;
	}

      break;
    default:
      DBG(ERROR_MESSAGE,"Bad Scanner.\n");
      break;
    }

#ifdef NEUTRALIZE_BACKEND
  return SANE_STATUS_GOOD;
#else
  return sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
#endif /* NEUTRALIZE_BACKEND */

}

static SANE_Status
start_scan (Apple_Scanner * s)
{
  SANE_Status status;
  uint8_t start[7];


  memset (start, 0, sizeof (start));
  start[0] = APPLE_SCSI_START;
  start[4] = 1;

  switch (s->hw->ScannerModel)
    {
    case APPLESCANNER:
      if (s->val[OPT_WAIT].w)  start[5]=0x80;
      /* NOT TODO  NoHome */
      break;
    case ONESCANNER:
      if (!s->val[OPT_CALIBRATE].w)  start[5]=0x20;
      break;
    case COLORONESCANNER:
      break;
    default:
      DBG(ERROR_MESSAGE,"Bad Scanner.\n");
      break;
    }


#ifdef NEUTRALIZE_BACKEND
  return SANE_STATUS_GOOD;
#else
  status = sanei_scsi_cmd (s->fd, start, sizeof (start), 0, 0);
  return status;
#endif /* NEUTRALIZE_BACKEND */
}

static SANE_Status
calc_parameters (Apple_Scanner * s)
{
  SANE_String val = s->val[OPT_MODE].s;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool OutOfRangeX, OutOfRangeY, Protect = SANE_TRUE;
  SANE_Int xqstep, yqstep;

  DBG (FLOW_CONTROL, "Entering calc_parameters\n");

  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 1;
      s->bpp = 1;
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_HALFTONE))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 1;
      s->bpp = 1;
    }
  else if (!strcmp (val, "Gray16"))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 8;
      s->bpp = 4;
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_GRAY))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 8;
      s->bpp = 8;
    }
  else if (!strcmp (val, "BiColor"))
    {
      s->params.last_frame = SANE_TRUE;
      s->params.format = SANE_FRAME_RGB;
      s->params.depth = 24;
      s->bpp = 3;
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_COLOR))
    {
      s->params.last_frame = SANE_FALSE;
      s->params.format = SANE_FRAME_RED;
      s->params.depth = 24;
      s->bpp = 24;
    }
  else
    {
      DBG (ERROR_MESSAGE, "calc_parameters: Invalid mode %s\n", (char *) val);
      status = SANE_STATUS_INVAL;
    }

  s->ulx = SANE_UNFIX (s->val[OPT_TL_X].w) / MM_PER_INCH;
  s->uly = SANE_UNFIX (s->val[OPT_TL_Y].w) / MM_PER_INCH;
  s->wx = SANE_UNFIX (s->val[OPT_BR_X].w) / MM_PER_INCH - s->ulx;
  s->wy = SANE_UNFIX (s->val[OPT_BR_Y].w) / MM_PER_INCH - s->uly;

  DBG (VARIABLE_CONTROL, "Desired [%g,%g] to +[%g,%g]\n",
       s->ulx, s->uly, s->wx, s->wy);

  xqstep = XQSTEP (s->val[OPT_RESOLUTION].w, s->bpp);
  yqstep = YQSTEP (s->val[OPT_RESOLUTION].w);

  DBG (VARIABLE_CONTROL, "Quantization steps of [%u,%u].\n", xqstep, yqstep);

  s->ULx = xquant (s->ulx, s->val[OPT_RESOLUTION].w, s->bpp, 0);
  s->Width = xquant (s->wx, s->val[OPT_RESOLUTION].w, s->bpp, 1);
  s->ULy = yquant (s->uly, s->val[OPT_RESOLUTION].w, 0);
  s->Height = yquant (s->wy, s->val[OPT_RESOLUTION].w, 1);

  DBG (VARIABLE_CONTROL, "Scanner [%u,%u] to +[%u,%u]\n",
       s->ULx, s->ULy, s->Width, s->Height);

  do
    {

      OutOfRangeX = SANE_FALSE;
      OutOfRangeY = SANE_FALSE;

      if (s->ULx + s->Width > s->hw->MaxWidth)
	{
	  OutOfRangeX = SANE_TRUE;
	  Protect = SANE_FALSE;
	  s->Width -= xqstep;
	}

      if (s->ULy + s->Height > s->hw->MaxHeight)
	{
	  OutOfRangeY = SANE_TRUE;
	  Protect = SANE_FALSE;
	  s->Height -= yqstep;
	}

      DBG (VARIABLE_CONTROL, "Adapting to [%u,%u] to +[%u,%u]\n",
	   s->ULx, s->ULy, s->Width, s->Height);

    }
  while (OutOfRangeX || OutOfRangeY);

  s->ulx = (double) s->ULx / 1200;
  s->uly = (double) s->ULy / 1200;
  s->wx = (double) s->Width / 1200;
  s->wy = (double) s->Height / 1200;


  DBG (VARIABLE_CONTROL, "Real [%g,%g] to +[%g,%g]\n",
       s->ulx, s->uly, s->wx, s->wy);


/* 

   TODO: Remove this ugly hack (Protect). Read to learn why!

   NOTE: I hate the Fixed Sane type. This type gave me a terrible
   headache and a difficult bug to find out. The xscanimage frontend
   was looping and segfaulting all the time with random order. The
   problem was the following:

   * You select new let's say BR_X
   * sane_control_option returns info inexact (always for BR_X) but
     does not modify val because it fits under the constrained
     quantization.

   Hm... Well sane_control doesn't change the (double) value of val
   but the Fixed interpatation may have been change (by 1 or something
   small).

   So now we should protect the val if the change is smaller than the
   quantization step or better under the SANE_[UN]FIX accuracy.

   Looks like for two distinct val (Fixed) values we get the same
   double. How come ?

   This hack fixed the looping situtation. Unfortunately SIGSEGV
   remains when you touch the slice bars (thouhg not all the
   time). But it's OK if you select scan_area from the preview window
   (cool).

 */

  if (!Protect)
    {
      s->val[OPT_TL_X].w = SANE_FIX (s->ulx * MM_PER_INCH);
      s->val[OPT_TL_Y].w = SANE_FIX (s->uly * MM_PER_INCH);
      s->val[OPT_BR_X].w = SANE_FIX ((s->ulx + s->wx) * MM_PER_INCH);
      s->val[OPT_BR_Y].w = SANE_FIX ((s->uly + s->wy) * MM_PER_INCH);
    }
  else
    DBG (VARIABLE_CONTROL, "Not adapted. Protecting\n");


  DBG (VARIABLE_CONTROL, "GUI [%g,%g] to [%g,%g]\n",
       SANE_UNFIX (s->val[OPT_TL_X].w),
       SANE_UNFIX (s->val[OPT_TL_Y].w),
       SANE_UNFIX (s->val[OPT_BR_X].w),
       SANE_UNFIX (s->val[OPT_BR_Y].w));

  /* NOTE: remember that AppleScanners quantize the scan area to be a
     byte multiple */


  s->params.pixels_per_line = s->Width * s->val[OPT_RESOLUTION].w / 1200;
  s->params.lines = s->Height * s->val[OPT_RESOLUTION].w / 1200;
  s->params.bytes_per_line = s->params.pixels_per_line * s->params.depth / 8;


  DBG (VARIABLE_CONTROL, "format=%d\n", s->params.format);
  DBG (VARIABLE_CONTROL, "last_frame=%d\n", s->params.last_frame);
  DBG (VARIABLE_CONTROL, "lines=%d\n", s->params.lines);
  DBG (VARIABLE_CONTROL, "depth=%d (%d)\n", s->params.depth, s->bpp);
  DBG (VARIABLE_CONTROL, "pixels_per_line=%d\n", s->params.pixels_per_line);
  DBG (VARIABLE_CONTROL, "bytes_per_line=%d\n", s->params.bytes_per_line);
  DBG (VARIABLE_CONTROL, "Pixels %dx%dx%d\n",
       s->params.pixels_per_line, s->params.lines, 1 << s->params.depth);

  DBG (FLOW_CONTROL, "Leaving calc_parameters\n");
  return status;
}



static SANE_Status
gamma_update(SANE_Handle handle)
{
Apple_Scanner *s = handle;


if (s->hw->ScannerModel == COLORONESCANNER)
  {
  if (	!strcmp(s->val[OPT_MODE].s,SANE_VALUE_SCAN_MODE_GRAY)	||
	!strcmp(s->val[OPT_MODE].s,"Gray16")	 )
    {
    ENABLE (OPT_CUSTOM_GAMMA);
    if (s->val[OPT_CUSTOM_GAMMA].w)
      {
      ENABLE (OPT_DOWNLOAD_GAMMA);
      if (! strcmp(s->val[OPT_COLOR_SENSOR].s,"All"))
	{
	ENABLE (OPT_GAMMA_VECTOR_R);
	ENABLE (OPT_GAMMA_VECTOR_G);
	ENABLE (OPT_GAMMA_VECTOR_B);
	}
      if (! strcmp(s->val[OPT_COLOR_SENSOR].s,"Red"))
	{
	ENABLE (OPT_GAMMA_VECTOR_R);
	DISABLE(OPT_GAMMA_VECTOR_G);
	DISABLE (OPT_GAMMA_VECTOR_B);
	}
      if (! strcmp(s->val[OPT_COLOR_SENSOR].s,"Green"))
	{
	DISABLE (OPT_GAMMA_VECTOR_R);
	ENABLE (OPT_GAMMA_VECTOR_G);
	DISABLE (OPT_GAMMA_VECTOR_B);
	}
      if (! strcmp(s->val[OPT_COLOR_SENSOR].s,"Blue"))
	{
	DISABLE (OPT_GAMMA_VECTOR_R);
	DISABLE (OPT_GAMMA_VECTOR_G);
	ENABLE (OPT_GAMMA_VECTOR_B);
	}
      }
    else /* Not custom gamma */
      {
      goto discustom;
      }
    }
  else if (!strcmp(s->val[OPT_MODE].s,SANE_VALUE_SCAN_MODE_COLOR))
    {
    ENABLE (OPT_CUSTOM_GAMMA);
    if (s->val[OPT_CUSTOM_GAMMA].w)
      {
      ENABLE (OPT_DOWNLOAD_GAMMA);
      ENABLE (OPT_GAMMA_VECTOR_R);
      ENABLE (OPT_GAMMA_VECTOR_G);
      ENABLE (OPT_GAMMA_VECTOR_B);
      }
    else /* Not custom gamma */
      {
      goto discustom;
      }
    }
  else /* Not Gamma capable mode */
    {
    goto disall;
    }
  }	/* Not Gamma capable Scanner */
else
  {
disall:
  DISABLE (OPT_CUSTOM_GAMMA);
discustom:
  DISABLE (OPT_GAMMA_VECTOR_R);
  DISABLE (OPT_GAMMA_VECTOR_G);
  DISABLE (OPT_GAMMA_VECTOR_B);
  DISABLE (OPT_DOWNLOAD_GAMMA);
  }

return SANE_STATUS_GOOD;
}


static SANE_Status
mode_update (SANE_Handle handle, char *val)
{
  Apple_Scanner *s = handle;
  SANE_Bool cct=SANE_FALSE;
  SANE_Bool UseThreshold=SANE_FALSE;

  DISABLE(OPT_COLOR_SENSOR);

  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART))
    {
      if (s->hw->ScannerModel == APPLESCANNER)
	ENABLE (OPT_AUTOBACKGROUND);
      else
	DISABLE (OPT_AUTOBACKGROUND);
      DISABLE (OPT_HALFTONE_PATTERN);

      UseThreshold=SANE_TRUE;
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_HALFTONE))
    {
      DISABLE (OPT_AUTOBACKGROUND);
      ENABLE (OPT_HALFTONE_PATTERN);
    }
  else if (!strcmp (val, "Gray16") || !strcmp (val, SANE_VALUE_SCAN_MODE_GRAY))
    {
      DISABLE (OPT_AUTOBACKGROUND);
      DISABLE (OPT_HALFTONE_PATTERN);
      if (s->hw->ScannerModel == COLORONESCANNER)
	ENABLE(OPT_COLOR_SENSOR);
        
    }				/* End of Gray */
  else if (!strcmp (val, "BiColor"))
    {
      DISABLE (OPT_AUTOBACKGROUND);
      DISABLE (OPT_HALFTONE_PATTERN);
      UseThreshold=SANE_TRUE;
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_COLOR))
    {
      DISABLE (OPT_AUTOBACKGROUND);
      DISABLE (OPT_HALFTONE_PATTERN);
      cct=SANE_TRUE;
    }
  else
    {
      DBG (ERROR_MESSAGE, "Invalid mode %s\n", (char *) val);
      return SANE_STATUS_INVAL;
    }

/* Second hand dependancies of mode option */
/* Looks like code doubling */


  if (UseThreshold)
    {
      DISABLE (OPT_BRIGHTNESS);
      DISABLE (OPT_CONTRAST);
      DISABLE (OPT_VOLT_REF);
      DISABLE (OPT_VOLT_REF_TOP);
      DISABLE (OPT_VOLT_REF_BOTTOM);

     if (IS_ACTIVE (OPT_AUTOBACKGROUND) && s->val[OPT_AUTOBACKGROUND].w)
      {
      DISABLE (OPT_THRESHOLD);
      ENABLE (OPT_AUTOBACKGROUND_THRESHOLD);
      }
    else
      {
      ENABLE (OPT_THRESHOLD);
      DISABLE (OPT_AUTOBACKGROUND_THRESHOLD);
      }
    }
  else
    {
      DISABLE (OPT_THRESHOLD);
      DISABLE (OPT_AUTOBACKGROUND_THRESHOLD);
      
      if (s->hw->ScannerModel == COLORONESCANNER)
	{
	ENABLE (OPT_VOLT_REF);
	if (s->val[OPT_VOLT_REF].w)
	  {
	  ENABLE (OPT_VOLT_REF_TOP);
	  ENABLE (OPT_VOLT_REF_BOTTOM);
	  DISABLE (OPT_BRIGHTNESS);
	  DISABLE (OPT_CONTRAST);
	  }
	else
	  {
	  DISABLE (OPT_VOLT_REF_TOP);
	  DISABLE (OPT_VOLT_REF_BOTTOM);
	  ENABLE (OPT_BRIGHTNESS);
	  ENABLE (OPT_CONTRAST);
	  }
	}
      else
        {
	ENABLE (OPT_BRIGHTNESS);
	ENABLE (OPT_CONTRAST);
        }
    }


  if (IS_ACTIVE (OPT_HALFTONE_PATTERN) &&
      !strcmp (s->val[OPT_HALFTONE_PATTERN].s, "download"))
    ENABLE (OPT_HALFTONE_FILE);
  else
    DISABLE (OPT_HALFTONE_FILE);

  if (cct)
    ENABLE (OPT_CUSTOM_CCT);
  else
    DISABLE (OPT_CUSTOM_CCT);

  if (cct && s->val[OPT_CUSTOM_CCT].w)
    {
    ENABLE(OPT_CCT);
    ENABLE(OPT_DOWNLOAD_CCT);
    }
  else
    {
    DISABLE(OPT_CCT);
    DISABLE(OPT_DOWNLOAD_CCT);
    }


  gamma_update (s);
  calc_parameters (s);

  return SANE_STATUS_GOOD;

}




static SANE_Status
init_options (Apple_Scanner * s)
{
  int i;

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

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

  /* Hardware detect Information  group: */

  s->opt[OPT_HWDETECT_GROUP].title = "Hardware";
  s->opt[OPT_HWDETECT_GROUP].desc = "Detected during hardware probing";
  s->opt[OPT_HWDETECT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_HWDETECT_GROUP].cap = 0;
  s->opt[OPT_HWDETECT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
        
  s->opt[OPT_MODEL].name = "model";
  s->opt[OPT_MODEL].title = "Model";
  s->opt[OPT_MODEL].desc = "Model and capabilities";
  s->opt[OPT_MODEL].type = SANE_TYPE_STRING;
  s->opt[OPT_MODEL].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_MODEL].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_MODEL].size = max_string_size (SupportedModel);
  s->val[OPT_MODEL].s = strdup (SupportedModel[s->hw->ScannerModel]);


  /* "Mode" group: */

  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  halftone_pattern_list[0]="spiral4x4";
  halftone_pattern_list[1]="bayer4x4";
  halftone_pattern_list[2]="download";
  halftone_pattern_list[3]=NULL;
  

  switch (s->hw->ScannerModel)
    {
    case APPLESCANNER:
      mode_list[0]=SANE_VALUE_SCAN_MODE_LINEART;
      mode_list[1]=SANE_VALUE_SCAN_MODE_HALFTONE;
      mode_list[2]="Gray16";
      mode_list[3]=NULL;
      break;
    case ONESCANNER:
      mode_list[0]=SANE_VALUE_SCAN_MODE_LINEART;
      mode_list[1]=SANE_VALUE_SCAN_MODE_HALFTONE;
      mode_list[2]="Gray16";
      mode_list[3]=SANE_VALUE_SCAN_MODE_GRAY;
      mode_list[4]=NULL;
      halftone_pattern_list[3]="spiral8x8";
      halftone_pattern_list[4]="bayer8x8";
      halftone_pattern_list[5]=NULL;
      break;
    case COLORONESCANNER:
      mode_list[0]=SANE_VALUE_SCAN_MODE_LINEART;
      mode_list[1]="Gray16";
      mode_list[2]=SANE_VALUE_SCAN_MODE_GRAY;
      mode_list[3]="BiColor";
      mode_list[4]=SANE_VALUE_SCAN_MODE_COLOR;
      mode_list[5]=NULL;
      break;
    default:
      break;
    }


  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[0]);


  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
/* TODO: Build the constraints on resolution in a smart way */
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = resbit_list;
  s->val[OPT_RESOLUTION].w = resbit_list[1];

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

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
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X].w = s->hw->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y].w = s->hw->y_range.max;


  /* "Enhancement" group: */

  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
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
  s->opt[OPT_BRIGHTNESS].constraint.range = &byte_range;
  s->val[OPT_BRIGHTNESS].w = 128;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST
    " This option is active for halftone/Grayscale modes only.";
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &byte_range;
  s->val[OPT_CONTRAST].w = 1;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &byte_range;
  s->val[OPT_THRESHOLD].w = 128;

  /* AppleScanner Only options */

  /* GrayMap Enhance */
  s->opt[OPT_GRAYMAP].name = "graymap";
  s->opt[OPT_GRAYMAP].title = "GrayMap";
  s->opt[OPT_GRAYMAP].desc = "Fixed Gamma Enhancing";
  s->opt[OPT_GRAYMAP].type = SANE_TYPE_STRING;
  s->opt[OPT_GRAYMAP].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  if (s->hw->ScannerModel != APPLESCANNER)
    s->opt[OPT_GRAYMAP].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GRAYMAP].constraint.string_list = graymap_list;
  s->opt[OPT_GRAYMAP].size = max_string_size (graymap_list);
  s->val[OPT_GRAYMAP].s = strdup (graymap_list[1]);

  /* Enable auto background adjustment */
  s->opt[OPT_AUTOBACKGROUND].name = "abj";
  s->opt[OPT_AUTOBACKGROUND].title = "Use Auto Background Adjustment";
  s->opt[OPT_AUTOBACKGROUND].desc =
      "Enables/Disables the Auto Background Adjustment feature";
  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART)
      || (s->hw->ScannerModel != APPLESCANNER))
    DISABLE (OPT_AUTOBACKGROUND);
  s->opt[OPT_AUTOBACKGROUND].type = SANE_TYPE_BOOL;
  s->val[OPT_AUTOBACKGROUND].w = SANE_FALSE;

  /* auto background adjustment threshold */
  s->opt[OPT_AUTOBACKGROUND_THRESHOLD].name = "abjthreshold";
  s->opt[OPT_AUTOBACKGROUND_THRESHOLD].title = "Auto Background Adjustment Threshold";
  s->opt[OPT_AUTOBACKGROUND_THRESHOLD].desc = "Selects the automatically adjustable threshold";
  s->opt[OPT_AUTOBACKGROUND_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_AUTOBACKGROUND_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_AUTOBACKGROUND_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;

  if (!IS_ACTIVE (OPT_AUTOBACKGROUND) ||
      s->val[OPT_AUTOBACKGROUND].w == SANE_FALSE)
    s->opt[OPT_AUTOBACKGROUND_THRESHOLD].cap |= SANE_CAP_INACTIVE;

  s->opt[OPT_AUTOBACKGROUND_THRESHOLD].constraint.range = &byte_range;
  s->val[OPT_AUTOBACKGROUND_THRESHOLD].w = 64;

 
  /* AppleScanner & OneScanner options  */

  /* Select HalfTone Pattern  */
  s->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].size = max_string_size (halftone_pattern_list);
  s->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_AUTOMATIC;
  s->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE_PATTERN].constraint.string_list = halftone_pattern_list;
  s->val[OPT_HALFTONE_PATTERN].s = strdup (halftone_pattern_list[0]);

  if (s->hw->ScannerModel!=APPLESCANNER && s->hw->ScannerModel!=ONESCANNER)
    s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;


  /* halftone pattern file */
  s->opt[OPT_HALFTONE_FILE].name = "halftone-pattern-file";
  s->opt[OPT_HALFTONE_FILE].title = "Halftone Pattern File";
  s->opt[OPT_HALFTONE_FILE].desc =
    "Download and use the specified file as halftone pattern";
  s->opt[OPT_HALFTONE_FILE].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_FILE].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_HALFTONE_FILE].size = 256;
  s->val[OPT_HALFTONE_FILE].s = "halftone.pgm";

  /* Use volt_ref */
  s->opt[OPT_VOLT_REF].name = "volt-ref";
  s->opt[OPT_VOLT_REF].title = "Volt Reference";
  s->opt[OPT_VOLT_REF].desc ="It's brightness equivalant.";
  s->opt[OPT_VOLT_REF].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_VOLT_REF].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_VOLT_REF].w = SANE_FALSE;

  s->opt[OPT_VOLT_REF_TOP].name = "volt-ref-top";
  s->opt[OPT_VOLT_REF_TOP].title = "Top Voltage Reference";
  s->opt[OPT_VOLT_REF_TOP].desc = "I really do not know.";
  s->opt[OPT_VOLT_REF_TOP].type = SANE_TYPE_INT;
  s->opt[OPT_VOLT_REF_TOP].unit = SANE_UNIT_NONE;
  if (s->hw->ScannerModel!=COLORONESCANNER || s->val[OPT_VOLT_REF].w==SANE_FALSE)
    s->opt[OPT_VOLT_REF_TOP].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_VOLT_REF_TOP].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_VOLT_REF_TOP].constraint.range = &byte_range;
  s->val[OPT_VOLT_REF_TOP].w = 255;

  s->opt[OPT_VOLT_REF_BOTTOM].name = "volt-ref-bottom";
  s->opt[OPT_VOLT_REF_BOTTOM].title = "Bottom Voltage Reference";
  s->opt[OPT_VOLT_REF_BOTTOM].desc = "I really do not know.";
  s->opt[OPT_VOLT_REF_BOTTOM].type = SANE_TYPE_INT;
  s->opt[OPT_VOLT_REF_BOTTOM].unit = SANE_UNIT_NONE;
  if (s->hw->ScannerModel!=COLORONESCANNER || s->val[OPT_VOLT_REF].w==SANE_FALSE)
    s->opt[OPT_VOLT_REF_BOTTOM].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_VOLT_REF_BOTTOM].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_VOLT_REF_BOTTOM].constraint.range = &byte_range;
  s->val[OPT_VOLT_REF_BOTTOM].w = 1;

/* Misc Functions: Advanced */

  s->opt[OPT_MISC_GROUP].title = "Miscallaneous";
  s->opt[OPT_MISC_GROUP].desc = "";
  s->opt[OPT_MISC_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MISC_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_MISC_GROUP].constraint_type = SANE_CONSTRAINT_NONE;


  /* Turn On lamp  during scan: All scanners */
  s->opt[OPT_LAMP].name = "lamp";
  s->opt[OPT_LAMP].title = "Lamp";
  s->opt[OPT_LAMP].desc = "Hold the lamp on during scans.";
  s->opt[OPT_LAMP].type = SANE_TYPE_BOOL;
  s->val[OPT_LAMP].w = SANE_FALSE;

  /* AppleScanner Only options */

  /* Wait for button to be pressed before scanning */
  s->opt[OPT_WAIT].name = "wait";
  s->opt[OPT_WAIT].title = "Wait";
  s->opt[OPT_WAIT].desc = "You may issue the scan command but the actual "
  "scan will not start unless you press the button in the front of the "
  "scanner. It is a useful feature when you want to make a network scan (?) "
  "In the mean time you may halt your computer waiting for the SCSI bus "
  "to be free. If this happens just press the scanner button.";
  s->opt[OPT_WAIT].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel != APPLESCANNER)
    s->opt[OPT_WAIT].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_WAIT].w = SANE_FALSE;


  /* OneScanner Only options */

  /* Calibrate before scanning ? */
  s->opt[OPT_CALIBRATE].name = "calibrate";
  s->opt[OPT_CALIBRATE].title = "Calibrate";
  s->opt[OPT_CALIBRATE].desc = "You may avoid the calibration before "
      "scanning but this will lead you to lower image quality.";
  s->opt[OPT_CALIBRATE].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel != ONESCANNER)
    s->opt[OPT_CALIBRATE].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CALIBRATE].w = SANE_TRUE;

  /* speed */
  s->opt[OPT_SPEED].name = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
  s->opt[OPT_SPEED].desc = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SPEED].type = SANE_TYPE_STRING;
  if (s->hw->ScannerModel != ONESCANNER)
    s->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_SPEED].size = max_string_size (speed_list);
  s->opt[OPT_SPEED].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SPEED].constraint.string_list = speed_list;
  s->val[OPT_SPEED].s = strdup (speed_list[0]);

  /* OneScanner & ColorOneScanner (LED && CCD) */

  /* LED ? */
  s->opt[OPT_LED].name = "led";
  s->opt[OPT_LED].title = "LED";
  s->opt[OPT_LED].desc ="This option controls the setting of the ambler LED.";
  s->opt[OPT_LED].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel!=ONESCANNER && s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_LED].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_LED].w = SANE_TRUE;

  /* CCD Power ? */
  s->opt[OPT_CCD].name = "ccd";
  s->opt[OPT_CCD].title = "CCD Power";
  s->opt[OPT_CCD].desc ="This option controls the power to the CCD array.";
  s->opt[OPT_CCD].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel!=ONESCANNER && s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_CCD].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CCD].w = SANE_TRUE;

  /*  Use MTF Circuit */
  s->opt[OPT_MTF_CIRCUIT].name = "mtf";
  s->opt[OPT_MTF_CIRCUIT].title = "MTF Circuit";
  s->opt[OPT_MTF_CIRCUIT].desc ="Turns the MTF (Modulation Transfer Function) "
						"peaking circuit on or off.";
  s->opt[OPT_MTF_CIRCUIT].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_MTF_CIRCUIT].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_MTF_CIRCUIT].w = SANE_TRUE;


  /* Use ICP */
  s->opt[OPT_ICP].name = "icp";
  s->opt[OPT_ICP].title = "ICP";
  s->opt[OPT_ICP].desc ="What is an ICP anyway?";
  s->opt[OPT_ICP].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_ICP].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_ICP].w = SANE_TRUE;


  /* Data Polarity */
  s->opt[OPT_POLARITY].name = "polarity";
  s->opt[OPT_POLARITY].title = "Data Polarity";
  s->opt[OPT_POLARITY].desc = "Reverse black and white.";
  s->opt[OPT_POLARITY].type = SANE_TYPE_BOOL;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_POLARITY].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_POLARITY].w = SANE_FALSE;


/* Color Functions: Advanced */

  s->opt[OPT_COLOR_GROUP].title = SANE_VALUE_SCAN_MODE_COLOR;
  s->opt[OPT_COLOR_GROUP].desc = "";
  s->opt[OPT_COLOR_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_COLOR_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_COLOR_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

#ifdef CALIBRATION_FUNCTIONALITY
  /* OneScanner calibration vector */
  s->opt[OPT_CALIBRATION_VECTOR].name = "calibration-vector";
  s->opt[OPT_CALIBRATION_VECTOR].title = "Calibration Vector";
  s->opt[OPT_CALIBRATION_VECTOR].desc = "Calibration vector for the CCD array.";
  s->opt[OPT_CALIBRATION_VECTOR].type = SANE_TYPE_INT;
  if (s->hw->ScannerModel!=ONESCANNER)
    s->opt[OPT_CALIBRATION_VECTOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CALIBRATION_VECTOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATION_VECTOR].size = 2550 * sizeof (SANE_Word);
  s->opt[OPT_CALIBRATION_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CALIBRATION_VECTOR].constraint.range = &u8_range;
  s->val[OPT_CALIBRATION_VECTOR].wa = s->calibration_vector;

  /* ColorOneScanner calibration vector per band */
  s->opt[OPT_CALIBRATION_VECTOR_RED].name = "calibration-vector-red";
  s->opt[OPT_CALIBRATION_VECTOR_RED].title = "Calibration Vector for Red";
  s->opt[OPT_CALIBRATION_VECTOR_RED].desc = "Calibration vector for the CCD array.";
  s->opt[OPT_CALIBRATION_VECTOR_RED].type = SANE_TYPE_INT;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_CALIBRATION_VECTOR_RED].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CALIBRATION_VECTOR_RED].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATION_VECTOR_RED].size = 2700 * sizeof (SANE_Word);
  s->opt[OPT_CALIBRATION_VECTOR_RED].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CALIBRATION_VECTOR_RED].constraint.range = &u8_range;
  s->val[OPT_CALIBRATION_VECTOR_RED].wa = s->calibration_vector_red;

  /* ColorOneScanner calibration vector per band */
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].name = "calibration-vector-green";
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].title = "Calibration Vector for Green";
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].desc = "Calibration vector for the CCD array.";
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].type = SANE_TYPE_INT;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_CALIBRATION_VECTOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].size = 2700 * sizeof (SANE_Word);
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CALIBRATION_VECTOR_GREEN].constraint.range = &u8_range;
  s->val[OPT_CALIBRATION_VECTOR_GREEN].wa = s->calibration_vector_green;

  /* ColorOneScanner calibration vector per band */
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].name = "calibration-vector-blue";
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].title = "Calibration Vector for Blue";
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].desc = "Calibration vector for the CCD array.";
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].type = SANE_TYPE_INT;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_CALIBRATION_VECTOR_BLUE].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].size = 2700 * sizeof (SANE_Word);
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CALIBRATION_VECTOR_BLUE].constraint.range = &u8_range;
  s->val[OPT_CALIBRATION_VECTOR_BLUE].wa = s->calibration_vector_blue;
#endif /* CALIBRATION_FUNCTIONALITY */

  /* Action: Download calibration vector */
  s->opt[OPT_DOWNLOAD_CALIBRATION_VECTOR].name = "download-calibration";
  s->opt[OPT_DOWNLOAD_CALIBRATION_VECTOR].title = "Download Calibration Vector";
  s->opt[OPT_DOWNLOAD_CALIBRATION_VECTOR].desc = "Download calibration vector to scanner";
  s->opt[OPT_DOWNLOAD_CALIBRATION_VECTOR].type = SANE_TYPE_BUTTON;
  if (s->hw->ScannerModel!=ONESCANNER && s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_DOWNLOAD_CALIBRATION_VECTOR].cap |= SANE_CAP_INACTIVE;

  /* custom-cct table */
  s->opt[OPT_CUSTOM_CCT].name = "custom-cct";
  s->opt[OPT_CUSTOM_CCT].title = "Use Custom CCT";
  s->opt[OPT_CUSTOM_CCT].desc ="Determines whether a builtin "
	"or a custom 3x3 Color Correction Table (CCT) should be used.";
  s->opt[OPT_CUSTOM_CCT].type = SANE_TYPE_BOOL;
  s->opt[OPT_CUSTOM_CCT].cap |= SANE_CAP_INACTIVE;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_CUSTOM_CCT].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CUSTOM_CCT].w = SANE_FALSE;


  /* CCT */
  s->opt[OPT_CCT].name = "cct";
  s->opt[OPT_CCT].title = "3x3 Color Correction Table";
  s->opt[OPT_CCT].desc = "TODO: Color Correction is currently unsupported";
  s->opt[OPT_CCT].type = SANE_TYPE_FIXED;
  s->opt[OPT_CCT].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CCT].unit = SANE_UNIT_NONE;
  s->opt[OPT_CCT].size = 9 * sizeof (SANE_Word);
  s->opt[OPT_CCT].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CCT].constraint.range = &u8_range;
  s->val[OPT_CCT].wa = s->cct3x3;


  /* Action: custom 3x3 color correction table */
  s->opt[OPT_DOWNLOAD_CCT].name = "download-3x3";
  s->opt[OPT_DOWNLOAD_CCT].title = "Download 3x3 CCT";
  s->opt[OPT_DOWNLOAD_CCT].desc = "Download 3x3 color correction table";
  s->opt[OPT_DOWNLOAD_CCT].type = SANE_TYPE_BUTTON;
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_DOWNLOAD_CCT].cap |= SANE_CAP_INACTIVE;


  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

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
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[0][0];

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
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[1][0];

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
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[2][0];

  /* Action: download gamma vectors table */
  s->opt[OPT_DOWNLOAD_GAMMA].name = "download-gamma";
  s->opt[OPT_DOWNLOAD_GAMMA].title = "Download Gamma Vector(s)";
  s->opt[OPT_DOWNLOAD_GAMMA].desc = "Download Gamma Vector(s).";
  s->opt[OPT_DOWNLOAD_GAMMA].type = SANE_TYPE_BUTTON;
  s->opt[OPT_DOWNLOAD_GAMMA].cap |= SANE_CAP_INACTIVE;

  s->opt[OPT_COLOR_SENSOR].name = "color-sensor";
  s->opt[OPT_COLOR_SENSOR].title = "Gray scan with";
  s->opt[OPT_COLOR_SENSOR].desc = "Select the color sensor to scan in gray mode.";
  s->opt[OPT_COLOR_SENSOR].type = SANE_TYPE_STRING;
  s->opt[OPT_COLOR_SENSOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_COLOR_SENSOR].size = max_string_size (color_sensor_list);
  if (s->hw->ScannerModel!=COLORONESCANNER)
    s->opt[OPT_COLOR_SENSOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_COLOR_SENSOR].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_COLOR_SENSOR].constraint.string_list = color_sensor_list;
  s->val[OPT_COLOR_SENSOR].s = strdup(color_sensor_list[2]);


  mode_update (s, s->val[OPT_MODE].s);

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one (const char *dev)
{
  attach (dev, 0, SANE_FALSE);
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  authorize = authorize;	/* silence gcc */

  DBG_INIT ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (APPLE_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      attach ("/dev/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;

      len = strlen (dev_name);

      if (!len)
	continue;		/* ignore empty lines */

      if (strncmp (dev_name, "option", 6) == 0
	  && isspace (dev_name[6]))
	{
	  const char *str = dev_name + 7;

	  while (isspace (*str))
	    ++str;

	  continue;
	}

      sanei_config_attach_matching_devices (dev_name, attach_one);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Apple_Device *dev, *next;

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
      free (dev);
    }
  if (devlist)
    free (devlist);
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Apple_Device *dev;
  int i;

  local_only = local_only;		/* silence gcc */

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Apple_Device *dev;
  SANE_Status status;
  Apple_Scanner *s;
  int i, j;

  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;

      if (!dev)
	{
	  status = attach (devicename, &dev, SANE_TRUE);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}
    }
  else
    /* empty devicname -> use first device */
    dev = first_dev;

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->hw = dev;
  for (i = 0; i < 3; ++i)
    for (j = 0; j < 256; ++j)
      s->gamma_table[i][j] = j;

  init_options (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Apple_Scanner *prev, *s;

  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (ERROR_MESSAGE, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  free (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Apple_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Apple_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;


  DBG (FLOW_CONTROL, "(%s): Entering on control_option for option %s (%d).\n",
       (action == SANE_ACTION_GET_VALUE) ? "get" : "set",
       s->opt[option].name, option);

  if (val || action == SANE_ACTION_GET_VALUE)
    switch (s->opt[option].type)
      {
      case SANE_TYPE_STRING:
	DBG (FLOW_CONTROL, "Value %s\n", (action == SANE_ACTION_GET_VALUE) ?
	  s->val[option].s : (char *) val);
	break;
      case SANE_TYPE_FIXED:
	{
	double v1, v2;
	SANE_Fixed f;
	v1 = SANE_UNFIX (s->val[option].w);
	f = *(SANE_Fixed *) val;
	v2 = SANE_UNFIX (f);
	DBG (FLOW_CONTROL, "Value %g (Fixed)\n",
	     (action == SANE_ACTION_GET_VALUE) ? v1 : v2);
        }
      default:
	DBG (FLOW_CONTROL, "Value %u (Int).\n",
		(action == SANE_ACTION_GET_VALUE)
			? s->val[option].w : *(SANE_Int *) val);
	break;
      }


  if (info)
    *info = 0;

  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;


  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	case OPT_AUTOBACKGROUND:
	case OPT_AUTOBACKGROUND_THRESHOLD:
	case OPT_VOLT_REF:
	case OPT_VOLT_REF_TOP:
	case OPT_VOLT_REF_BOTTOM:

	case OPT_LAMP:
	case OPT_WAIT:
	case OPT_CALIBRATE:
	case OPT_LED:
	case OPT_CCD:
	case OPT_MTF_CIRCUIT:
	case OPT_ICP:
	case OPT_POLARITY:

	case OPT_CUSTOM_CCT:
	case OPT_CUSTOM_GAMMA:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* word-array options: */

	case OPT_CCT:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* string options: */

	case OPT_MODE:
/*
TODO: This is to protect the mode string to be ruined from the dll?
backend. I do not know why. It's definitely an overkill and should be
eliminated.
	  status = sanei_constrain_value (s->opt + option, s->val[option].s,
					  info);
*/
	case OPT_MODEL:
	case OPT_GRAYMAP:
	case OPT_HALFTONE_PATTERN:
	case OPT_HALFTONE_FILE:
	case OPT_SPEED:
	case OPT_COLOR_SENSOR:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;

/* Some Buttons */
	case OPT_DOWNLOAD_CALIBRATION_VECTOR:
	case OPT_DOWNLOAD_CCT:
	case OPT_DOWNLOAD_GAMMA:
	  return SANE_STATUS_INVAL;

	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      status = sanei_constrain_value (s->opt + option, val, info);

      if (status != SANE_STATUS_GOOD)
	return status;


      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:

	  s->val[option].w = *(SANE_Word *) val;
	  calc_parameters (s);

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS
	      | SANE_INFO_RELOAD_OPTIONS
	      | SANE_INFO_INEXACT;

	  return SANE_STATUS_GOOD;

	  /* fall through */
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	case OPT_AUTOBACKGROUND_THRESHOLD:
	case OPT_VOLT_REF_TOP:
	case OPT_VOLT_REF_BOTTOM:
	case OPT_LAMP:
	case OPT_WAIT:
	case OPT_CALIBRATE:
	case OPT_LED:
	case OPT_CCD:
	case OPT_MTF_CIRCUIT:
	case OPT_ICP:
	case OPT_POLARITY:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* Simple Strings */
	case OPT_GRAYMAP:
	case OPT_HALFTONE_FILE:
	case OPT_SPEED:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;

	  /* Boolean */
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Bool *) val;
	  return SANE_STATUS_GOOD;


	  /* side-effect-free word-array options: */
	case OPT_CCT:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;


	  /* options with light side-effects: */

	case OPT_HALFTONE_PATTERN:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (!strcmp (val, "download"))
	    {
	      return SANE_STATUS_UNSUPPORTED;
	      /* TODO: ENABLE(OPT_HALFTONE_FILE); */
	    }
	  else
	    DISABLE (OPT_HALFTONE_FILE);
	  return SANE_STATUS_GOOD;

	case OPT_AUTOBACKGROUND:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  s->val[option].w = *(SANE_Bool *) val;
	  if (*(SANE_Bool *) val)
	    {
	      DISABLE (OPT_THRESHOLD);
	      ENABLE (OPT_AUTOBACKGROUND_THRESHOLD);
	    }
	  else
	    {
	      ENABLE (OPT_THRESHOLD);
	      DISABLE (OPT_AUTOBACKGROUND_THRESHOLD);
	    }
	  return SANE_STATUS_GOOD;
	case OPT_VOLT_REF:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  s->val[option].w = *(SANE_Bool *) val;
	  if (*(SANE_Bool *) val)
	    {
	    DISABLE(OPT_BRIGHTNESS);
	    DISABLE(OPT_CONTRAST);
	    ENABLE(OPT_VOLT_REF_TOP);
	    ENABLE(OPT_VOLT_REF_BOTTOM);
	    }
	  else
	    {
	    ENABLE(OPT_BRIGHTNESS);
	    ENABLE(OPT_CONTRAST);
	    DISABLE(OPT_VOLT_REF_TOP);
	    DISABLE(OPT_VOLT_REF_BOTTOM);
	    }
	  return SANE_STATUS_GOOD;

/* Actions: Buttons */

	case OPT_DOWNLOAD_CALIBRATION_VECTOR:
	case OPT_DOWNLOAD_CCT:
	case OPT_DOWNLOAD_GAMMA:
	  /* TODO: fix {down/up}loads */
	  return SANE_STATUS_UNSUPPORTED;

	case OPT_CUSTOM_CCT:
	  s->val[OPT_CUSTOM_CCT].w=*(SANE_Word *) val;
	  if (s->val[OPT_CUSTOM_CCT].w)
	    {
		ENABLE(OPT_CCT);
		ENABLE(OPT_DOWNLOAD_CCT);
	    }
	  else
	    {
		DISABLE(OPT_CCT);
		DISABLE(OPT_DOWNLOAD_CCT);
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;

	case OPT_CUSTOM_GAMMA:
	  s->val[OPT_CUSTOM_GAMMA].w = *(SANE_Word *) val;
	  gamma_update(s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;

	case OPT_COLOR_SENSOR:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  gamma_update(s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	  /* HEAVY (RADIOACTIVE) SIDE EFFECTS: CHECKME */
	case OPT_MODE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);

	  status = mode_update (s, val);
	  if (status != SANE_STATUS_GOOD)
	    return status;

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	}			/* End of switch */
    }				/* End of SET_VALUE */
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Apple_Scanner *s = handle;

  DBG (FLOW_CONTROL, "Entering sane_get_parameters\n");
  calc_parameters (s);


  if (params)
    *params = s->params;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Apple_Scanner *s = handle;
  SANE_Status status;

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */

  calc_parameters (s);

  if (s->fd < 0)
    {
      /* this is the first (and maybe only) pass... */

      status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (ERROR_MESSAGE, "open: open of %s failed: %s\n",
	       s->hw->sane.name, sane_strstatus (status));
	  return status;
	}
    }

  status = wait_ready (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "open: wait_ready() failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = mode_select (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "sane_start: mode_select command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = scan_area_and_windows (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "open: set scan area command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = request_sense (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "sane_start: request_sense revealed error: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  s->scanning = SANE_TRUE;
  s->AbortedByUser = SANE_FALSE;

  status = start_scan (s);
  if (status != SANE_STATUS_GOOD)
    goto stop_scanner_and_return;

  return SANE_STATUS_GOOD;

stop_scanner_and_return:
  s->scanning = SANE_FALSE;
  s->AbortedByUser = SANE_FALSE;
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Apple_Scanner *s = handle;
  SANE_Status status;

  uint8_t get_data_status[10];
  uint8_t read[10];

#ifdef RESERVE_RELEASE_HACK
  uint8_t reserve[6];
  uint8_t release[6];
#endif  

  uint8_t result[12];
  size_t size;
  SANE_Int data_length = 0;
  SANE_Int data_av = 0;
  SANE_Int offset = 0;
  SANE_Int rread = 0;
  SANE_Bool Pseudo8bit = SANE_FALSE;

#ifdef NEUTRALIZE_BACKEND
  *len=max_len;
  return SANE_STATUS_GOOD;

#else
  *len = 0;
  if (!s->scanning) return SANE_STATUS_EOF;
  

  if (!strcmp (s->val[OPT_MODE].s, "Gray16"))
    Pseudo8bit = SANE_TRUE;

  /* TODO: The current function only implements for APPLESCANNER In
     order to use the COLORONESCANNER you have to study the docs to
     see how it the parameters get modified before scan. From this
     starting point it should be trivial to use a ONESCANNER int the
     gray256 mode but I don't have one from these pets in home.  MF */


  memset (get_data_status, 0, sizeof (get_data_status));
  get_data_status[0] = APPLE_SCSI_GET_DATA_STATUS;
  get_data_status[1] = 1;	/* Wait */
  STORE24 (get_data_status + 6, sizeof (result));

  memset (read, 0, sizeof (read));
  read[0] = APPLE_SCSI_READ_SCANNED_DATA;


#ifdef RESERVE_RELEASE_HACK
  memset (reserve, 0, sizeof (reserve));
  reserve[0] = APPLE_SCSI_RESERVE;

  reserve[1]=CONTROLLER_SCSI_ID;
  reserve[1]=reserve[1] << 1;
  reserve[1]|=SETTHIRDPARTY;

  memset (release, 0, sizeof (release));
  release[0] = APPLE_SCSI_RELEASE;
  release[1]=CONTROLLER_SCSI_ID;
  release[1]=reserve[1] << 1;
  release[1]|=SETTHIRDPARTY;

#endif
 
  do
    {
      size = sizeof (result);
      status = sanei_scsi_cmd (s->fd, get_data_status,
			       sizeof (get_data_status), result, &size);

      if (status != SANE_STATUS_GOOD)
	return status;
      if (!size)
	{
	  DBG (ERROR_MESSAGE, "sane_read: cannot get_data_status.\n");
	  return SANE_STATUS_IO_ERROR;
	}

      data_length = READ24 (result);
      data_av = READ24 (result + 9);

      if (data_length)
	{
	  /* if (result[3] & 1)	Scanner Blocked: Retrieve data */
	  if ((result[3] & 1) || data_av)
	    {
	      DBG (IO_MESSAGE,
		   "sane_read: (status) Available in scanner buffer %u.\n",
		   data_av);

	      if (Pseudo8bit)
		if ((data_av << 1) + offset > max_len)
		  rread = (max_len - offset) >> 1;
		else
		  rread = data_av;
	      else if (data_av + offset > max_len)
		rread = max_len - offset;
	      else
		rread = data_av;

	      DBG (IO_MESSAGE,
		   "sane_read: (action) Actual read request for %u bytes.\n",
		   rread);

	      size = rread;

	      STORE24 (read + 6, rread);

#ifdef RESERVE_RELEASE_HACK
	      {
	      SANE_Status status;
	      DBG(IO_MESSAGE,"Reserving the SCSI bus.\n");
	      status=sanei_scsi_cmd (s->fd,reserve,sizeof(reserve),0,0);
	      DBG(IO_MESSAGE,"Reserving... status:= %d\n",status);
	      }
#endif /* RESERVE_RELEASE_HACK */

	      status = sanei_scsi_cmd (s->fd, read, sizeof (read),
				       buf + offset, &size);

#ifdef RESERVE_RELEASE_HACK
	      {
	      SANE_Status status;
	      DBG(IO_MESSAGE,"Releasing the SCSI bus.\n");
	      status=sanei_scsi_cmd (s->fd,release,sizeof(release),0,0);
	      DBG(IO_MESSAGE,"Releasing... status:= %d\n",status);
	      }
#endif /* RESERVE_RELEASE_HACK */


	      if (Pseudo8bit)
		{
		  SANE_Int byte;
		  SANE_Int pos = offset + (rread << 1) - 1;
		  SANE_Byte B;
		  for (byte = offset + rread - 1; byte >= offset; byte--)
		    {
		      B = buf[byte];
		      buf[pos--] = 255 - (B << 4);   /* low (right) nibble */
		      buf[pos--] = 255 - (B & 0xF0); /* high (left) nibble */
		    }
		  offset += size << 1;
		}
	      else
		offset += size;

	      DBG (IO_MESSAGE, "sane_read: Buffer %u of %u full %g%%\n",
		   offset, max_len, (double) (offset * 100. / max_len));
	    }
	}
    }
  while (offset < max_len && data_length != 0 && !s->AbortedByUser);


  if (s->AbortedByUser)
    {
      s->scanning = SANE_FALSE;
      status = sanei_scsi_cmd (s->fd, test_unit_ready,
			       sizeof (test_unit_ready), 0, 0);
      if (status != SANE_STATUS_GOOD)
	return status;
      return SANE_STATUS_CANCELLED;
    }

  if (!data_length)		/* If not blocked */
    {
      s->scanning = SANE_FALSE;

      DBG (IO_MESSAGE, "sane_read: (status) Oups! No more data...");
      if (!offset)
	{
	  *len = 0;
	  DBG (IO_MESSAGE, "EOF\n");
	  return SANE_STATUS_EOF;
	}
      else
	{
	  *len = offset;
	  DBG (IO_MESSAGE, "GOOD\n");
	  return SANE_STATUS_GOOD;
	}
    }


  DBG (FLOW_CONTROL,
       "sane_read: Normal Exiting (?), Aborted=%u, data_length=%u\n",
       s->AbortedByUser, data_length);
  *len = offset;

  return SANE_STATUS_GOOD;

#endif /* NEUTRALIZE_BACKEND */
}

void
sane_cancel (SANE_Handle handle)
{
  Apple_Scanner *s = handle;

  if (s->scanning)
    {
      if (s->AbortedByUser)
	{
	  DBG (FLOW_CONTROL,
	       "sane_cancel: Allready Aborted. Please Wait...\n");
	}
      else
	{
	  s->scanning=SANE_FALSE;
	  s->AbortedByUser = SANE_TRUE;
	  DBG (FLOW_CONTROL, "sane_cancel: Signal Caught! Aborting...\n");
	}
    }
  else
    {
      if (s->AbortedByUser)
	{
	  DBG (FLOW_CONTROL, "sane_cancel: Scan has not been Initiated yet, "
	       "or it is allready aborted.\n");
	  s->AbortedByUser = SANE_FALSE;
	  sanei_scsi_cmd (s->fd, test_unit_ready,
				sizeof (test_unit_ready), 0, 0);
	}
      else
	{
	  DBG (FLOW_CONTROL, "sane_cancel: Scan has not been Initiated "
	       "yet (or it's over).\n");
	}
    }

  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
DBG (FLOW_CONTROL,"sane_set_io_mode: Entering.\n");

 handle = handle;				/* silence gcc */

if (non_blocking)
  {
  DBG (FLOW_CONTROL, "sane_set_io_mode: Don't call me please. "
       "Unimplemented function\n");
  return SANE_STATUS_UNSUPPORTED;
  }

return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  handle = handle;				/* silence gcc */
  fd = fd;						/* silence gcc */

  DBG (FLOW_CONTROL, "sane_get_select_fd: Don't call me please. "
       "Unimplemented function\n");
  return SANE_STATUS_UNSUPPORTED;
}
