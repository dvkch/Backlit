/* sane - Scanner Access Now Easy.
   Copyright (C) 2002 Michael Herder <crapsite@gmx.net>

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This backend is based on the "gt68xxtest" program written by the following
persons:
  Sergey Vlasov <vsu@mivlgu.murom.ru>
    - Main backend code.

  Andreas Nowack <nowack.andreas@gmx.de>
    - Support for GT6801 (Mustek ScanExpress 1200 UB Plus).

  David Stevenson <david.stevenson@zoom.co.uk>
    - Automatic AFE gain and offset setting.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Please note:
The calibration code from the gt68xxtest program isn't used here, since I
couldn't get it working. I'm using my own calibration code, which is based
on wild assumptions based on the USB logs from the windoze driver.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
It also contains code from the plustek backend

Copyright (C) 2000-2002 Gerhard Jaeger <g.jaeger@earthling.net>

and from the mustek_usb backend

Copyright (C) 2000 Mustek.
Maintained by Tom Wang <tom.wang@mustek.com.tw>
Updates (C) 2001 by Henning Meier-Geinitz.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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
   If you do not wish that, delete this exception notice.  */

#define BUILD 12

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>


#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME artec_eplus48u
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"

#include "artec_eplus48u.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define _DEFAULT_DEVICE "/dev/usbscanner"
#define ARTEC48U_CONFIG_FILE "artec_eplus48u.conf"

#define _SHADING_FILE_BLACK "artec48ushading_black"
#define _SHADING_FILE_WHITE "artec48ushading_white"
#define _EXPOSURE_FILE      "artec48uexposure"
#define _OFFSET_FILE        "artec48uoffset"

#define _BYTE      3
#define _STRING    2
#define _FLOAT     1
#define _INT       0

/*for calibration*/
#define WHITE_MIN 243*257
#define WHITE_MAX 253*257
#define BLACK_MIN 8*257
#define BLACK_MAX 18*257
#define EXPOSURE_STEP 280

static Artec48U_Device *first_dev = 0;
static Artec48U_Scanner *first_handle = 0;
static SANE_Int num_devices = 0;
static char devName[PATH_MAX];
static char firmwarePath[PATH_MAX];
static char vendor_string[PATH_MAX];
static char model_string[PATH_MAX];

static SANE_Bool cancelRead;
static int isEPro;
static int eProMult;
static SANE_Auth_Callback auth = NULL;
static double gamma_master_default = 1.7;
static double gamma_r_default = 1.0;
static double gamma_g_default = 1.0;
static double gamma_b_default = 1.0;

static SANE_Word memory_read_value = 0x200c;	/**< Memory read - wValue */
static SANE_Word memory_write_value = 0x200b;	/**< Memory write - wValue */
static SANE_Word send_cmd_value = 0x2010;	/**< Send normal command - wValue */
static SANE_Word send_cmd_index = 0x3f40;	/**< Send normal command - wIndex */
static SANE_Word recv_res_value = 0x2011;	/**< Receive normal result - wValue */
static SANE_Word recv_res_index = 0x3f00;	/**< Receive normal result - wIndex */
static SANE_Word send_small_cmd_value = 0x2012;	/**< Send small command - wValue */
static SANE_Word send_small_cmd_index = 0x3f40;	/**< Send small command - wIndex */
static SANE_Word recv_small_res_value = 0x2013;	/**< Receive small result - wValue */
static SANE_Word recv_small_res_index = 0x3f00;	/**< Receive small result - wIndex */

static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  NULL
};

static SANE_Word resbit_list[] = {
  6,
  50, 100, 200, 300, 600, 1200
};

static SANE_Range brightness_contrast_range = {
  -127,
  127,
  0
};

static SANE_Range blacklevel_range = {
  20,
  240,
  1
};

static SANE_Range gamma_range = {
  0,				/* minimum */
  SANE_FIX (4.0),		/* maximum */
  0				/* quantization */
};

static SANE_Range scan_range_x = {
  0,				/* minimum */
  SANE_FIX (216.0),		/* maximum */
  0				/* quantization */
};

static SANE_Range scan_range_y = {
  0,				/* minimum */
  SANE_FIX (297.0),		/* maximum */
  0				/* quantization */
};


static SANE_Word bitdepth_list[] = {
  2, 8, 16
};

static SANE_Word bitdepth_list2[] = {
  1, 8
};

static Artec48U_Exposure_Parameters exp_params;
static Artec48U_Exposure_Parameters default_exp_params =
  { 0x009f, 0x0109, 0x00cb };
static Artec48U_AFE_Parameters afe_params;
static Artec48U_AFE_Parameters default_afe_params =
  { 0x28, 0x0a, 0x2e, 0x03, 0x2e, 0x03 };

static SANE_Status
download_firmware_file (Artec48U_Device * chip)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte *buf = NULL;
  int size = -1;
  FILE *f;

  XDBG ((2, "Try to open firmware file: \"%s\"\n", chip->firmware_path));
  f = fopen (chip->firmware_path, "rb");
  if (!f)
    {
      XDBG ((2, "Cannot open firmware file \"%s\"\n", firmwarePath));
      status = SANE_STATUS_INVAL;
    }

  if (status == SANE_STATUS_GOOD)
    {
      fseek (f, 0, SEEK_END);
      size = ftell (f);
      fseek (f, 0, SEEK_SET);
      if (size == -1)
	{
	  XDBG ((2, "Error getting size of firmware file \"%s\"\n",
	       chip->firmware_path));
	  status = SANE_STATUS_INVAL;
	}
    }

  if (status == SANE_STATUS_GOOD)
    {
      XDBG ((3, "firmware size: %d\n", size));
      buf = (SANE_Byte *) malloc (size);
      if (!buf)
	{
	  XDBG ((2, "Cannot allocate %d bytes for firmware\n", size));
	  status = SANE_STATUS_NO_MEM;
	}
    }

  if (status == SANE_STATUS_GOOD)
    {
      int bytes_read = fread (buf, 1, size, f);
      if (bytes_read != size)
	{
	  XDBG ((2, "Problem reading firmware file \"%s\"\n",
	       chip->firmware_path));
	  status = SANE_STATUS_INVAL;
	}
    }

  if (f)
    fclose (f);

  if (status == SANE_STATUS_GOOD)
    {
      status = artec48u_download_firmware (chip, buf, size);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((2, "Firmware download failed\n"));
	}
    }

  if (buf)
    free (buf);
  return status;
}

static SANE_Status
init_calibrator (Artec48U_Scanner * s)
{
  XDBG ((2, "Init calibrator size %d\n",30720 * s->dev->epro_mult));
  s->shading_buffer_w = (unsigned char *) malloc (30720 * s->dev->epro_mult); /*epro*/
  s->shading_buffer_b = (unsigned char *) malloc (30720 * s->dev->epro_mult); /*epro*/
  s->shading_buffer_white[0] =
    (unsigned int *) malloc (5120 * s->dev->epro_mult * sizeof(unsigned int));/*epro*/
  s->shading_buffer_black[0] =
    (unsigned int *) malloc (5120 * s->dev->epro_mult * sizeof (unsigned int));/*epro*/
  s->shading_buffer_white[1] =
    (unsigned int *) malloc (5120 * s->dev->epro_mult * sizeof (unsigned int));/*epro*/
  s->shading_buffer_black[1] =
    (unsigned int *) malloc (5120 * s->dev->epro_mult *  sizeof (unsigned int));/*epro*/
  s->shading_buffer_white[2] =
    (unsigned int *) malloc (5120 * s->dev->epro_mult *  sizeof (unsigned int));/*epro*/
  s->shading_buffer_black[2] =
    (unsigned int *) malloc (5120 * s->dev->epro_mult *  sizeof (unsigned int));/*epro*/

  if (!s->shading_buffer_w || !s->shading_buffer_b
      || !s->shading_buffer_white[0] || !s->shading_buffer_black[0]
      || !s->shading_buffer_white[1] || !s->shading_buffer_black[1]
      || !s->shading_buffer_white[2] || !s->shading_buffer_black[2])
    {
      if (s->shading_buffer_w)
	free (s->shading_buffer_w);
      if (s->shading_buffer_b)
	free (s->shading_buffer_b);
      if (s->shading_buffer_white[0])
	free (s->shading_buffer_white[0]);
      if (s->shading_buffer_black[0])
	free (s->shading_buffer_black[0]);
      if (s->shading_buffer_white[1])
	free (s->shading_buffer_white[1]);
      if (s->shading_buffer_black[1])
	free (s->shading_buffer_black[1]);
      if (s->shading_buffer_white[2])
	free (s->shading_buffer_white[2]);
      if (s->shading_buffer_black[2])
	free (s->shading_buffer_black[2]);
      return SANE_STATUS_NO_MEM;
    }
  return SANE_STATUS_GOOD;
}

static void
init_shading_buffer (Artec48U_Scanner * s)
{
  unsigned int i, j;

  for (i = 0; i < 5120 * s->dev->epro_mult; i++) /*epro*/
    {
      for (j = 0; j < 3; j++)
	{
	  s->temp_shading_buffer[j][i] = 0;
	}
    }
}

static void
add_to_shading_buffer (Artec48U_Scanner * s, unsigned int **buffer_pointers)
{
  unsigned int i, j;

  for (i = 0; i < 5120 * s->dev->epro_mult; i++)  /*epro*/
    {
      for (j = 0; j < 3; j++)
	{
	  s->temp_shading_buffer[j][i] += buffer_pointers[j][i];
	}
    }
}

static void
finish_shading_buffer (Artec48U_Scanner * s, SANE_Bool white)
{
  unsigned int i, j, cnt, c, div;
  unsigned long max_r;
  unsigned long max_g;
  unsigned long max_b;
  unsigned char *shading_buffer;
  cnt = 0;

  if (white)
    {
      shading_buffer = s->shading_buffer_w;
      div = s->dev->shading_lines_w;
    }
  else
    {
      shading_buffer = s->shading_buffer_b;
      div = s->dev->shading_lines_b;
    }

  for (i = 0; i < 5120 * s->dev->epro_mult; i++) /*epro*/
    {
      for (j = 0; j < 3; j++)
	{
	  int value = s->temp_shading_buffer[j][i] / (div);
	  shading_buffer[cnt] = (SANE_Byte) (value & 0xff);
	  ++cnt;
	  shading_buffer[cnt] = (SANE_Byte) ((value >> 8) & 0xff);
	  ++cnt;
	}
    }
  max_r = 0;
  max_g = 0;
  max_b = 0;

  for (c = 0; c < (30720 * s->dev->epro_mult) - 5; c += 6) /*epro*/
    {
      i = (int) shading_buffer[c] + ((int) shading_buffer[c + 1] << 8);
      max_r += i;
      i = (int) shading_buffer[c + 2] + ((int) shading_buffer[c + 3] << 8);
      max_g += i;
      i = (int) shading_buffer[c + 4] + ((int) shading_buffer[c + 5] << 8);
      max_b += i;
    }
}

static void
finish_exposure_buffer (Artec48U_Scanner * s, int *avg_r, int *avg_g,
			int *avg_b)
{
  unsigned int i, j, cnt, c, div;
  unsigned int max_r;
  unsigned int max_g;
  unsigned int max_b;
  unsigned char *shading_buffer;
  cnt = 0;

  shading_buffer = s->shading_buffer_w;
  div = s->dev->shading_lines_w;

  for (i = 0; i < 5120 * s->dev->epro_mult; i++) /*epro*/
    {
      for (j = 0; j < 3; j++)
	{
	  int value = s->temp_shading_buffer[j][i] / (div);
	  shading_buffer[cnt] = (SANE_Byte) (value & 0xff);
	  ++cnt;
	  shading_buffer[cnt] = (SANE_Byte) ((value >> 8) & 0xff);
	  ++cnt;
	}
    }
  max_r = 0;
  max_g = 0;
  max_b = 0;
  for (c = 0; c < (30720 * s->dev->epro_mult) - 5; c += 6) /*epro*/
    {
      i = (int) shading_buffer[c] + ((int) shading_buffer[c + 1] << 8);
      if (i > max_r)
	max_r = i;
      i = (int) shading_buffer[c + 2] + ((int) shading_buffer[c + 3] << 8);
      if (i > max_g)
	max_g = i;
      i = (int) shading_buffer[c + 4] + ((int) shading_buffer[c + 5] << 8);
      if (i > max_b)
	max_b = i;
    }
  *avg_r = max_r;
  *avg_g = max_g;
  *avg_b = max_b;
}

static void
finish_offset_buffer (Artec48U_Scanner * s, int *avg_r, int *avg_g,
		      int *avg_b)
{
  unsigned int i, j, cnt, c, div;
  unsigned int min_r;
  unsigned int min_g;
  unsigned int min_b;
  unsigned char *shading_buffer;
  cnt = 0;

  shading_buffer = s->shading_buffer_b;
  div = s->dev->shading_lines_b;

  for (i = 0; i < 5120 * s->dev->epro_mult; i++) /*epro*/
    {
      for (j = 0; j < 3; j++)
	{
	  int value = s->temp_shading_buffer[j][i] / (div);
	  shading_buffer[cnt] = (SANE_Byte) (value & 0xff);
	  ++cnt;
	  shading_buffer[cnt] = (SANE_Byte) ((value >> 8) & 0xff);
	  ++cnt;
	}
    }
  min_r = 65535;
  min_g = 65535;
  min_b = 65535;
  for (c = 0; c < (30720 * s->dev->epro_mult) - 5; c += 6) /*epro*/
    {
      i = (int) shading_buffer[c] + ((int) shading_buffer[c + 1] << 8);
      if (i < min_r)
	min_r = i;
      i = (int) shading_buffer[c + 2] + ((int) shading_buffer[c + 3] << 8);
      if (i < min_g)
	min_g = i;
      i = (int) shading_buffer[c + 4] + ((int) shading_buffer[c + 5] << 8);
      if (i < min_b)
	min_b = i;
    }
  *avg_r = min_r;
  *avg_g = min_g;
  *avg_b = min_b;
}

static SANE_Status
artec48u_wait_for_positioning (Artec48U_Device * chip)
{
  SANE_Status status;
  SANE_Bool moving;

  while (SANE_TRUE)
    {
      status = artec48u_is_moving (chip, &moving);
      if (status != SANE_STATUS_GOOD)
	return status;
      if (!moving)
	break;
      usleep (100000);
    }

  return SANE_STATUS_GOOD;
}

static void
copy_scan_line (Artec48U_Scanner * s)
{
  /*For resolution of 1200 dpi we have to interpolate
     horizontally, because the optical horizontal resolution is
     limited to 600 dpi. We simply use the avarage value of two pixels. */
  int cnt, i, j;
  int xs = s->params.pixel_xs;
  int interpolate = 0;
  int value;
  int value1;
  int value2;
  if ((s->reader->params.ydpi == 1200) && (s->dev->is_epro == 0)) /*epro*/
    interpolate = 1;
  cnt = 0;
  if (s->params.color)
    {
      if (s->params.depth > 8)
	{
	  for (i = xs - 1; i >= 0; i--)
	    {
	      for (j = 0; j < 3; j++)
		{
		  value = s->buffer_pointers[j][i];
		  s->line_buffer[cnt] = LOBYTE (value);
		  ++cnt;
		  s->line_buffer[cnt] = HIBYTE (value);
		  ++cnt;
		}
	      if (interpolate == 1)	/*1200 dpi */
		cnt += 6;
	    }
	  if (interpolate == 1)
	    {
	      for (i = 0; i < (xs * 12) - 12; i += 12)
		{
		  value1 = (int) s->line_buffer[i];
		  value1 += (int) (s->line_buffer[i + 1] << 8);
		  value2 = (int) s->line_buffer[i + 12];
		  value2 += (int) (s->line_buffer[i + 13] << 8);
		  value = (value1 + value2) / 2;
		  if (value < 0)
		    value = 0;
		  if (value > 65535)
		    value = 65535;
		  s->line_buffer[i + 6] = LOBYTE (value);
		  s->line_buffer[i + 7] = HIBYTE (value);

		  value1 = (int) s->line_buffer[i + 2];
		  value1 += (int) (s->line_buffer[i + 3] << 8);
		  value2 = (int) s->line_buffer[i + 14];
		  value2 += (int) (s->line_buffer[i + 15] << 8);
		  value = (value1 + value2) / 2;
		  if (value < 0)
		    value = 0;
		  if (value > 65535)
		    value = 65535;
		  s->line_buffer[i + 8] = LOBYTE (value);
		  s->line_buffer[i + 9] = HIBYTE (value);

		  value1 = (int) s->line_buffer[i + 4];
		  value1 += (int) (s->line_buffer[i + 5] << 8);
		  value2 = (int) s->line_buffer[i + 16];
		  value2 += (int) (s->line_buffer[i + 17] << 8);
		  value = (value1 + value2) / 2;
		  if (value < 0)
		    value = 0;
		  if (value > 65535)
		    value = 65535;
		  s->line_buffer[i + 10] = LOBYTE (value);
		  s->line_buffer[i + 11] = HIBYTE (value);
		}
	    }
	}
      else
	{
	  for (i = xs - 1; i >= 0; i--)
	    {
	      for (j = 0; j < 3; j++)
		{
		  value = s->buffer_pointers[j][i];
		  s->line_buffer[cnt] = (SANE_Byte) (value / 257);
		  cnt += 1;
		}
	      if (interpolate == 1)	/*1200 dpi */
		cnt += 3;
	    }
	  if (interpolate == 1)
	    {
	      for (i = 0; i < (xs * 6) - 6; i += 6)
		{
		  value1 = (int) s->line_buffer[i];
		  value2 = (int) s->line_buffer[i + 6];
		  value = (value1 + value2) / 2;
		  if (value < 0)
		    value = 0;
		  if (value > 255)
		    value = 255;
		  s->line_buffer[i + 3] = (SANE_Byte) (value);

		  value1 = (int) s->line_buffer[i + 1];
		  value2 = (int) s->line_buffer[i + 7];
		  value = (value1 + value2) / 2;
		  if (value < 0)
		    value = 0;
		  if (value > 255)
		    value = 255;
		  s->line_buffer[i + 4] = (SANE_Byte) (value);

		  value1 = (int) s->line_buffer[i + 2];
		  value2 = (int) s->line_buffer[i + 8];
		  value = (value1 + value2) / 2;
		  if (value < 0)
		    value = 0;
		  if (value > 255)
		    value = 255;
		  s->line_buffer[i + 5] = (SANE_Byte) (value);
		}
	    }
	}
    }
  else
    {
      if (s->params.depth > 8)
	{
	  for (i = xs - 1; i >= 0; --i)
	    {
	      value = s->buffer_pointers[0][i];
	      s->line_buffer[cnt] = LOBYTE (value);
	      ++cnt;
	      s->line_buffer[cnt] = HIBYTE (value);
	      ++cnt;
	      if (interpolate == 1)	/*1200 dpi */
		cnt += 2;
	    }
	  if (interpolate == 1)
	    {
	      for (i = 0; i < (xs * 4) - 4; i += 4)
		{
		  value1 = (int) s->line_buffer[i];
		  value1 += (int) (s->line_buffer[i + 1] << 8);
		  value2 = (int) s->line_buffer[i + 4];
		  value2 += (int) (s->line_buffer[i + 5] << 8);
		  value = (value1 + value2) / 2;
		  if (value < 0)
		    value = 0;
		  if (value > 65535)
		    value = 65535;
		  s->line_buffer[i + 2] = LOBYTE (value);
		  s->line_buffer[i + 3] = HIBYTE (value);
		}
	    }
	}
      else
	{
	  if (s->params.lineart == SANE_FALSE)
	    {
	      for (i = xs - 1; i >= 0; --i)
		{
		  value = s->buffer_pointers[0][i];
		  s->line_buffer[cnt] = (SANE_Byte) (value / 257);
		  ++cnt;
		  if (interpolate == 1)	/*1200 dpi */
		    ++cnt;
		}
	      if (interpolate == 1)
		{
		  for (i = 0; i < (xs * 2) - 2; i += 2)
		    {
		      value1 = (int) s->line_buffer[i];
		      value2 = (int) s->line_buffer[i + 2];
		      value = (value1 + value2) / 2;
		      if (value < 0)
			value = 0;
		      if (value > 255)
			value = 255;
		      s->line_buffer[i + 1] = (SANE_Byte) (value);
		    }
		}
	    }
	  else
	    {
	      int cnt2;
	      int bit_cnt = 0;
	      int black_level = s->val[OPT_BLACK_LEVEL].w;
	      /*copy to lineart_buffer */
	      for (i = xs - 1; i >= 0; --i)
		{
		  s->lineart_buffer[cnt] =
		    (SANE_Byte) (s->buffer_pointers[0][i] / 257);
		  ++cnt;
		  if (interpolate == 1)	/*1200 dpi */
		    ++cnt;
		}
	      cnt2 = cnt - 1;
	      cnt = 0;
	      if (interpolate == 1)
		{
		  for (i = 0; i < cnt2 - 2; i += 2)
		    {
		      value1 = (int) s->lineart_buffer[i];
		      value2 = (int) s->lineart_buffer[i + 2];
		      value = (value1 + value2) / 2;
		      if (value < 0)
			value = 0;
		      if (value > 255)
			value = 255;
		      s->lineart_buffer[i + 1] = (SANE_Byte) (value);
		    }
		}
	      /* in this case, every value in buffer_pointers represents a bit */
	      for (i = 0; i < cnt2; i++)
		{
		  SANE_Byte temp;
		  if (bit_cnt == 0)
		    s->line_buffer[cnt] = 0;	/*clear */
		  temp = s->lineart_buffer[i];
		  if (temp <= black_level)
		    s->line_buffer[cnt] |= 1 << (7 - bit_cnt);
		  ++bit_cnt;
		  if (bit_cnt > 7)
		    {
		      bit_cnt = 0;
		      ++cnt;
		    }
		}

	    }
	}
    }
}

/*.............................................................................
 * attach a device to the backend
 */
static SANE_Status
attach (const char *dev_name, Artec48U_Device ** devp)
{
  SANE_Status status;
  Artec48U_Device *dev;

  XDBG ((1, "attach (%s, %p)\n", dev_name, (void *) devp));

  if (!dev_name)
    {
      XDBG ((1, "attach: devname == NULL\n"));
      return SANE_STATUS_INVAL;
    }
  /* already attached ? */
  for (dev = first_dev; dev; dev = dev->next)
    {
      if (0 == strcmp (dev->name, dev_name))
	{
	  if (devp)
	    *devp = dev;
	  XDBG ((3, "attach: device %s already attached\n", dev_name));
	  return SANE_STATUS_GOOD;
	}
    }
  XDBG ((3, "attach: device %s NOT attached\n", dev_name));
  /* allocate some memory for the device */
  artec48u_device_new (&dev);
  if (NULL == dev)
    return SANE_STATUS_NO_MEM;

  dev->fd = -1;
  dev->name = strdup (dev_name);
  dev->sane.name = strdup (dev_name);
/*
 * go ahead and open the scanner device
 */
  status = artec48u_device_open (dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "Could not open device!!\n"));
      artec48u_device_free (dev);
      return status;
    }
  /*limit the size of vendor and model string to 40 */
  vendor_string[40] = 0;
  model_string[40] = 0;

  /* assign all the stuff we need fo this device... */
  dev->sane.vendor = strdup (vendor_string);
  XDBG ((3, "attach: setting vendor string: %s\n", vendor_string));
  dev->sane.model = strdup (model_string);
  XDBG ((3, "attach: setting model string: %s\n", model_string));
  dev->sane.type = "flatbed scanner";
  dev->firmware_path = strdup (firmwarePath);

  dev->epro_mult = eProMult;
  dev->is_epro = isEPro;
  XDBG ((1, "attach eProMult %d\n", eProMult));
  XDBG ((1, "attach isEPro %d\n", isEPro));
  dev->optical_xdpi = 600 * dev->epro_mult; /*epro*/
  dev->optical_ydpi = 1200 * dev->epro_mult; /*epro*/
  dev->base_ydpi = 600 * dev->epro_mult; /*epro*/
  dev->xdpi_offset = 0;		/* in optical_xdpi units */
  dev->ydpi_offset = 280 * dev->epro_mult;	/* in optical_ydpi units */
  dev->x_size = 5120 * dev->epro_mult; /*epro*/  /* in optical_xdpi units */
  dev->y_size = 14100 * dev->epro_mult; /*epro*/  /* in optical_ydpi units */
  dev->shading_offset = 10 * dev->epro_mult;
  dev->shading_lines_b = 70 * dev->epro_mult;
  dev->shading_lines_w = 70 * dev->epro_mult;

  dev->gamma_master = gamma_master_default;
  dev->gamma_r = gamma_r_default;
  dev->gamma_g = gamma_g_default;
  dev->gamma_b = gamma_b_default;

  dev->afe_params.r_offset = afe_params.r_offset;
  dev->afe_params.g_offset = afe_params.g_offset;
  dev->afe_params.b_offset = afe_params.b_offset;

  dev->afe_params.r_pga = default_afe_params.r_pga;
  dev->afe_params.g_pga = default_afe_params.g_pga;
  dev->afe_params.b_pga = default_afe_params.b_pga;

  dev->exp_params.r_time = exp_params.r_time;
  dev->exp_params.g_time = exp_params.g_time;
  dev->exp_params.b_time = exp_params.b_time;


  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = first_dev;
  status = artec48u_device_close (dev);
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  Artec48U_Device *dev;
  SANE_Status status;

  status = attach (devname, &dev);
  if (SANE_STATUS_GOOD != status)
    return status;
  return SANE_STATUS_GOOD;
}

/**
 * function to decode an value and give it back to the caller.
 * @param src    -  pointer to the source string to check
 * @param opt    -  string that keeps the option name to check src for
 * @param what   - _FLOAT or _INT
 * @param result -  pointer to the var that should receive our result
 * @param def    - default value that result should be in case of any error
 * @return The function returns SANE_TRUE if the option has been found,
 *         if not, it returns SANE_FALSE
 */
static SANE_Bool
decodeVal (char *src, char *opt, int what, void *result, void *def)
{
  char *tmp, *tmp2;
  const char *name;

/* skip the option string */
  name = (const char *) &src[strlen ("option")];

/* get the name of the option */
  name = sanei_config_get_string (name, &tmp);

  if (tmp)
    {
      /* on success, compare wiht the given one */
      if (0 == strcmp (tmp, opt))
	{
	  XDBG ((1, "Decoding option >%s<\n", opt));
	  if (_INT == what)
	    {
	      /* assign the default value for this option... */
	      *((int *) result) = *((int *) def);
	      if (*name)
		{
		  /* get the configuration value and decode it */
		  name = sanei_config_get_string (name, &tmp2);
		  if (tmp2)
		    {
		      *((int *) result) = strtol (tmp2, 0, 0);
		      free (tmp2);
		    }
		}
	      free (tmp);
	      return SANE_TRUE;
	    }
	  else if (_FLOAT == what)
	    {
	      /* assign the default value for this option... */
	      *((double *) result) = *((double *) def);
	      if (*name)
		{
		  /* get the configuration value and decode it */
		  name = sanei_config_get_string (name, &tmp2);
		  if (tmp2)
		    {
		      *((double *) result) = strtod (tmp2, 0);
		      free (tmp2);
		    }
		}
	      free (tmp);
	      return SANE_TRUE;
	    }
	  else if (_BYTE == what)
	    {
	      /* assign the default value for this option... */
	      *((SANE_Byte *) result) = *((SANE_Byte *) def);
	      if (*name)
		{
		  /* get the configuration value and decode it */
		  name = sanei_config_get_string (name, &tmp2);
		  if (tmp2)
		    {
		      *((SANE_Byte *) result) =
			(SANE_Byte) strtol (tmp2, 0, 0);
		      free (tmp2);
		    }
		}
	      free (tmp);
	      return SANE_TRUE;
	    }
	  else if (_STRING == what)
	    {
	      if (*name)
		{
		  /* get the configuration value and decode it */
		  sanei_config_get_string (name, &tmp2);
		  if (tmp2)
		    {
		      strcpy ((char *) result, (char *) tmp2);
		      free (tmp2);
		    }
		}
	      free (tmp);
	      return SANE_TRUE;
	    }
	}
      free (tmp);
    }
  return SANE_FALSE;
}

/**
 * function to retrive the device name of a given string
 * @param src  -  string that keeps the option name to check src for
 * @param dest -  pointer to the string, that should receive the detected
 *                devicename
 * @return The function returns SANE_TRUE if the devicename has been found,
 *         if not, it returns SANE_FALSE
 */
static SANE_Bool
decodeDevName (char *src, char *dest)
{
  char *tmp;
  const char *name;

  if (0 == strncmp ("device", src, 6))
    {
      name = (const char *) &src[strlen ("device")];
      name = sanei_config_skip_whitespace (name);

      XDBG ((1, "Decoding device name >%s<\n", name));

      if (*name)
	{
	  name = sanei_config_get_string (name, &tmp);
	  if (tmp)
	    {
	      strcpy (dest, tmp);
	      free (tmp);
	      return SANE_TRUE;
	    }
	}
    }
  return SANE_FALSE;
}

#ifdef ARTEC48U_USE_BUTTONS
static SANE_Status
artec48u_check_buttons (Artec48U_Device * dev, SANE_Int * value)
{
  SANE_Status status;
  Artec48U_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x74;
  req[1] = 0x01;

  status = artec48u_device_small_req (dev, req, req);
  if (status != SANE_STATUS_GOOD)
    return status;

  *value = (SANE_Int) req[2];
  return SANE_STATUS_GOOD;
}
#endif

#define MAX_DOWNLOAD_BLOCK_SIZE 64
static SANE_Status
artec48u_generic_start_scan (Artec48U_Device * dev)
{
  Artec48U_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x43;
  req[1] = 0x01;

  return artec48u_device_req (dev, req, req);

}

static SANE_Status
artec48u_generic_read_scanned_data (Artec48U_Device * dev, SANE_Bool * ready)
{
  SANE_Status status;
  Artec48U_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x35;
  req[1] = 0x01;

  status = artec48u_device_req (dev, req, req);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (req[1] == 0x35)
    {
      if (req[0] == 0)
	*ready = SANE_TRUE;
      else
	*ready = SANE_FALSE;
    }
  else
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_download_firmware (Artec48U_Device * dev,
			    SANE_Byte * data, SANE_Word size)
{
  SANE_Status status;
  SANE_Byte download_buf[MAX_DOWNLOAD_BLOCK_SIZE];
  SANE_Byte check_buf[MAX_DOWNLOAD_BLOCK_SIZE];
  SANE_Byte *block;
  SANE_Word addr, bytes_left;
  Artec48U_Packet boot_req;
  SANE_Word block_size = MAX_DOWNLOAD_BLOCK_SIZE;

  CHECK_DEV_ACTIVE ((Artec48U_Device *) dev,
		    (char *) "artec48u_device_download_firmware");

  for (addr = 0; addr < size; addr += block_size)
    {
      bytes_left = size - addr;
      if (bytes_left > block_size)
	block = data + addr;
      else
	{
	  memset (download_buf, 0, block_size);
	  memcpy (download_buf, data + addr, bytes_left);
	  block = download_buf;
	}
      status = artec48u_device_memory_write (dev, addr, block_size, block);
      if (status != SANE_STATUS_GOOD)
	return status;
      status = artec48u_device_memory_read (dev, addr, block_size, check_buf);
      if (status != SANE_STATUS_GOOD)
	return status;
      if (memcmp (block, check_buf, block_size) != 0)
	{
	  XDBG ((3,
	       "artec48u_device_download_firmware: mismatch at block 0x%0x\n",
	       addr));
	  return SANE_STATUS_IO_ERROR;
	}
    }

  memset (boot_req, 0, sizeof (boot_req));
  boot_req[0] = 0x69;
  boot_req[1] = 0x01;
  boot_req[2] = LOBYTE (addr);
  boot_req[3] = HIBYTE (addr);
  status = artec48u_device_req (dev, boot_req, boot_req);
  if (status != SANE_STATUS_GOOD)
    return status;
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_is_moving (Artec48U_Device * dev, SANE_Bool * moving)
{
  SANE_Status status;
  Artec48U_Packet req;
  memset (req, 0, sizeof (req));
  req[0] = 0x17;
  req[1] = 0x01;

  status = artec48u_device_req (dev, req, req);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (req[0] == 0x00 && req[1] == 0x17)
    {
      if (req[2] == 0 && (req[3] == 0 || req[3] == 2))
	*moving = SANE_FALSE;
      else
	*moving = SANE_TRUE;
    }
  else
    return SANE_STATUS_IO_ERROR;
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_carriage_home (Artec48U_Device * dev)
{
  Artec48U_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x24;
  req[1] = 0x01;

  return artec48u_device_req (dev, req, req);
}


static SANE_Status
artec48u_stop_scan (Artec48U_Device * dev)
{
  Artec48U_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x41;
  req[1] = 0x01;
  return artec48u_device_small_req (dev, req, req);
}


static SANE_Status
artec48u_setup_scan (Artec48U_Scanner * s,
		     Artec48U_Scan_Request * request,
		     Artec48U_Scan_Action action,
		     SANE_Bool calculate_only,
		     Artec48U_Scan_Parameters * params)
{
  DECLARE_FUNCTION_NAME ("artec48u_setup_scan") SANE_Status status;
  SANE_Int xdpi, ydpi;
  SANE_Bool color;
  SANE_Int depth;
  SANE_Int pixel_x0, pixel_y0, pixel_xs, pixel_ys;
  SANE_Int pixel_align;

  SANE_Int abs_x0, abs_y0, abs_xs, abs_ys, base_xdpi, base_ydpi;
  SANE_Int scan_xs, scan_ys, scan_bpl;
  SANE_Int bits_per_line;
  SANE_Byte color_mode_code;

  /*If we scan a black line, we use these exposure values */
  Artec48U_Exposure_Parameters exp_params_black = { 4, 4, 4 };

  XDBG ((6, "%s: enter\n", function_name));
  XDBG ((1,"setup scan is_epro %d\n",s->dev->is_epro));
  XDBG ((1,"setup scan epro_mult %d\n",s->dev->epro_mult));

  xdpi = request->xdpi;
  ydpi = request->ydpi;
  color = request->color;
  depth = request->depth;

  switch (action)
    {
    case SA_CALIBRATE_SCAN_WHITE:
      {
	/*move a bit inside scan mark -
	   the value for the offset was found by trial and error */
	pixel_y0 = s->dev->shading_offset;
	pixel_ys = s->dev->shading_lines_w;
	pixel_x0 = 0;
	pixel_xs = 5120 * s->dev->epro_mult; /*epro*/
	xdpi = ydpi = 600 * s->dev->epro_mult; /*epro*/
	color = SANE_TRUE;
	depth = 8;
	break;
      }
    case SA_CALIBRATE_SCAN_OFFSET_1:
    case SA_CALIBRATE_SCAN_OFFSET_2:
      {
	pixel_y0 = s->dev->shading_offset;
	pixel_ys = s->dev->shading_lines_b;
	pixel_x0 = 0;
	pixel_xs = 5120 * s->dev->epro_mult; /*epro*/
	xdpi = ydpi = 600 * s->dev->epro_mult; /*epro*/
	color = SANE_TRUE;
	depth = 8;
	break;
      }
    case SA_CALIBRATE_SCAN_EXPOSURE_1:
    case SA_CALIBRATE_SCAN_EXPOSURE_2:
      {
	pixel_y0 = s->dev->shading_offset;
	pixel_ys = s->dev->shading_lines_w;
	pixel_x0 = 0;
	pixel_xs = 5120 * s->dev->epro_mult; /*epro*/
	xdpi = ydpi = 600 * s->dev->epro_mult; /*epro*/
	color = SANE_TRUE;
	depth = 8;
	break;
      }
    case SA_CALIBRATE_SCAN_BLACK:
      {
	pixel_y0 = s->dev->shading_offset;
	pixel_ys = s->dev->shading_lines_w;
	pixel_x0 = 0;
	pixel_xs = 5120 * s->dev->epro_mult; /*epro*/
	xdpi = ydpi = 600 * s->dev->epro_mult; /*epro*/
	color = SANE_TRUE;
	depth = 8;
	break;
      }
    case SA_SCAN:
      {
	SANE_Fixed x0 = request->x0 + s->dev->xdpi_offset;
	SANE_Fixed y0;
	/*epro*/
	if ((ydpi == 1200) && (s->dev->is_epro == 0))
	  xdpi = 600;
	y0 = request->y0 + s->dev->ydpi_offset;
	pixel_ys = SANE_UNFIX (request->ys) * ydpi / MM_PER_INCH + 0.5;
	pixel_x0 = SANE_UNFIX (x0) * xdpi / MM_PER_INCH + 0.5;
	pixel_y0 = SANE_UNFIX (y0) * ydpi / MM_PER_INCH + 0.5;
	pixel_xs = SANE_UNFIX (request->xs) * xdpi / MM_PER_INCH + 0.5;
	break;
      }

    default:
      XDBG ((6, "%s: invalid action=%d\n", function_name, (int) action));
      return SANE_STATUS_INVAL;
    }

  XDBG ((6, "%s: xdpi=%d, ydpi=%d\n", function_name, xdpi, ydpi));
  XDBG ((6, "%s: color=%s, depth=%d\n", function_name,
       color ? "TRUE" : "FALSE", depth));
  XDBG ((6, "%s: pixel_x0=%d, pixel_y0=%d\n", function_name,
       pixel_x0, pixel_y0));
  XDBG ((6, "%s: pixel_xs=%d, pixel_ys=%d\n", function_name,
       pixel_xs, pixel_ys));

  switch (depth)
    {
    case 8:
      color_mode_code = color ? 0x84 : 0x82;
      break;

    case 16:
      color_mode_code = color ? 0xa4 : 0xa2;
      break;

    default:
      XDBG ((6, "%s: unsupported depth=%d\n", function_name, depth));
      return SANE_STATUS_UNSUPPORTED;
    }

  base_xdpi = s->dev->optical_xdpi;
  base_ydpi = s->dev->base_ydpi;

  XDBG ((6, "%s: base_xdpi=%d, base_ydpi=%d\n", function_name,
       base_xdpi, base_ydpi));

  abs_x0 = pixel_x0 * base_xdpi / xdpi;
  abs_y0 = pixel_y0 * base_ydpi / ydpi;

  /* Calculate minimum number of pixels which span an integral multiple of 64
   * bytes. */
  pixel_align = 32;		/* best case for depth = 16 */
  while ((depth * pixel_align) % (64 * 8) != 0)
    pixel_align *= 2;
  XDBG ((6, "%s: pixel_align=%d\n", function_name, pixel_align));

  if (pixel_xs % pixel_align == 0)
    scan_xs = pixel_xs;
  else
    scan_xs = (pixel_xs / pixel_align + 1) * pixel_align;
  scan_ys = pixel_ys;
  XDBG ((6, "%s: scan_xs=%d, scan_ys=%d\n", function_name, scan_xs, scan_ys));

  abs_xs = scan_xs * base_xdpi / xdpi;
  abs_ys = scan_ys * base_ydpi / ydpi;
  XDBG ((6, "%s: abs_xs=%d, abs_ys=%d\n", function_name, abs_xs, abs_ys));

  bits_per_line = depth * scan_xs;
  if (bits_per_line % 8)	/* impossible */
    {
      XDBG ((1, "%s: BUG: unaligned bits_per_line=%d\n", function_name,
	   bits_per_line));
      return SANE_STATUS_INVAL;
    }
  scan_bpl = bits_per_line / 8;

  if (scan_bpl % 64)		/* impossible */
    {
      XDBG ((1, "%s: BUG: unaligned scan_bpl=%d\n", function_name, scan_bpl));
      return SANE_STATUS_INVAL;
    }

  if (scan_bpl > 15600)
    {
      XDBG ((6, "%s: scan_bpl=%d, too large\n", function_name, scan_bpl));
      return SANE_STATUS_INVAL;
    }

  XDBG ((6, "%s: scan_bpl=%d\n", function_name, scan_bpl));

  if (!calculate_only)
    {
      Artec48U_Packet req;
      char motor_mode_1, motor_mode_2;
      switch (action)
	{
	case SA_CALIBRATE_SCAN_WHITE:
	  motor_mode_1 = 0x01;
	  motor_mode_2 = 0x00;
	  break;

	case SA_CALIBRATE_SCAN_BLACK:
	  motor_mode_1 = 0x04;
	  motor_mode_2 = 0x00;
	  break;

	case SA_SCAN:
	  motor_mode_1 = 0x01;
	  motor_mode_2 = 0x00;
	  break;

	default:
	  XDBG ((6, "%s: invalid action=%d\n", function_name, (int) action));
	  return SANE_STATUS_INVAL;
	}

      /* Fill in the setup command */
      memset (req, 0, sizeof (req));
      req[0x00] = 0x20;
      req[0x01] = 0x01;
      req[0x02] = LOBYTE (abs_y0);
      req[0x03] = HIBYTE (abs_y0);
      req[0x04] = LOBYTE (abs_ys);
      req[0x05] = HIBYTE (abs_ys);
      req[0x06] = LOBYTE (abs_x0);
      req[0x07] = HIBYTE (abs_x0);
      req[0x08] = LOBYTE (abs_xs);
      req[0x09] = HIBYTE (abs_xs);
      req[0x0a] = color_mode_code;
      req[0x0b] = 0x60;
      req[0x0c] = LOBYTE (xdpi);
      req[0x0d] = HIBYTE (xdpi);
      req[0x0e] = 0x12;
      req[0x0f] = 0x00;
      req[0x10] = LOBYTE (scan_bpl);
      req[0x11] = HIBYTE (scan_bpl);
      req[0x12] = LOBYTE (scan_ys);
      req[0x13] = HIBYTE (scan_ys);
      req[0x14] = motor_mode_1;
      req[0x15] = motor_mode_2;
      req[0x16] = LOBYTE (ydpi);
      req[0x17] = HIBYTE (ydpi);
      req[0x18] = 0x00;

      status = artec48u_device_req (s->dev, req, req);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((3, "%s: setup request failed: %s\n", function_name,
	       sane_strstatus (status)));
	  return status;
	}

      if (action == SA_SCAN)
	{
	  artec48u_calculate_shading_buffer (s, pixel_x0, pixel_xs + pixel_x0,
					     xdpi, color);
	  artec48u_generic_set_exposure_time (s->dev,
					      &(s->dev->
						artec_48u_exposure_params));
	  artec48u_generic_set_afe (s->dev, &(s->dev->artec_48u_afe_params));
	}
      else if (action == SA_CALIBRATE_SCAN_BLACK)
	{
	  artec48u_generic_set_exposure_time (s->dev, &exp_params_black);
	  artec48u_generic_set_afe (s->dev, &(s->dev->afe_params));
	}
      else if (action == SA_CALIBRATE_SCAN_WHITE)
	{
	  artec48u_generic_set_exposure_time (s->dev, &(s->dev->exp_params));
	  artec48u_generic_set_afe (s->dev, &(s->dev->afe_params));
	}
    }
  /* Fill in calculated values */
  params->xdpi = xdpi;
  params->ydpi = ydpi;
  params->depth = depth;
  params->color = color;
  params->pixel_xs = pixel_xs;
  params->pixel_ys = pixel_ys;
  params->scan_xs = scan_xs;
  params->scan_ys = scan_ys;
  params->scan_bpl = scan_bpl;

  XDBG ((6, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_generic_set_afe (Artec48U_Device * dev,
			  Artec48U_AFE_Parameters * params)
{
  Artec48U_Packet req;
  memset (req, 0, sizeof (req));
  req[0] = 0x22;
  req[1] = 0x01;
  req[2] = params->r_offset;
  req[3] = params->r_pga;
  req[4] = params->g_offset;
  req[5] = params->g_pga;
  req[6] = params->b_offset;
  req[7] = params->b_pga;

  return artec48u_device_req (dev, req, req);
}


static SANE_Status
artec48u_generic_set_exposure_time (Artec48U_Device * dev,
				    Artec48U_Exposure_Parameters * params)
{
  Artec48U_Packet req;
  memset (req, 0, sizeof (req));
  req[0] = 0x76;
  req[1] = 0x01;
  req[2] = req[6] = req[10] = 0x04;
  req[4] = LOBYTE (params->r_time);
  req[5] = HIBYTE (params->r_time);
  req[8] = LOBYTE (params->g_time);
  req[9] = HIBYTE (params->g_time);
  req[12] = LOBYTE (params->b_time);
  req[13] = HIBYTE (params->b_time);
  return artec48u_device_req (dev, req, req);
}

static SANE_Status
artec48u_device_new (Artec48U_Device ** dev_return)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_new") Artec48U_Device *dev;

  XDBG ((7, "%s: enter\n", function_name));
  if (!dev_return)
    return SANE_STATUS_INVAL;

  dev = (Artec48U_Device *) malloc (sizeof (Artec48U_Device));

  if (!dev)
    {
      XDBG ((3, "%s: couldn't malloc %lu bytes for device\n",
	     function_name, (u_long) sizeof (Artec48U_Device)));
      *dev_return = 0;
      return SANE_STATUS_NO_MEM;
    }
  *dev_return = dev;

  memset (dev, 0, sizeof (Artec48U_Device));

  dev->fd = -1;
  dev->active = SANE_FALSE;

  dev->read_buffer = NULL;
  dev->requested_buffer_size = 32768;

  XDBG ((7, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_free (Artec48U_Device * dev)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_free")
    XDBG ((7, "%s: enter: dev=%p\n", function_name, (void *) dev));
  if (dev)
    {
      if (dev->active)
	artec48u_device_deactivate (dev);

      if (dev->fd != -1)
	artec48u_device_close (dev);

      XDBG ((7, "%s: freeing dev\n", function_name));
      free (dev);
    }
  XDBG ((7, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_open (Artec48U_Device * dev)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_open")
  SANE_Status status;
  SANE_Int fd;

  XDBG ((7, "%s: enter: dev=%p\n", function_name, (void *) dev));

  CHECK_DEV_NOT_NULL (dev, function_name);

  if (dev->fd != -1)
    {
      XDBG ((3, "%s: device already open\n", function_name));
      return SANE_STATUS_INVAL;
    }

  status = sanei_usb_open (dev->sane.name, &fd);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: sanei_usb_open failed: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  dev->fd = fd;

  XDBG ((7, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_close (Artec48U_Device * dev)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_close")
    XDBG ((7, "%s: enter: dev=%p\n", function_name, (void *) dev));

  CHECK_DEV_OPEN (dev, function_name);

  if (dev->active)
    artec48u_device_deactivate (dev);

  sanei_usb_close (dev->fd);
  dev->fd = -1;

  XDBG ((7, "%s: leave: ok\n", function_name));
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_activate (Artec48U_Device * dev)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_activate")
    CHECK_DEV_OPEN (dev, function_name);

  if (dev->active)
    {
      XDBG ((3, "%s: device already active\n", function_name));
      return SANE_STATUS_INVAL;
    }

  XDBG ((7, "%s: model \"%s\"\n", function_name, dev->sane.model));

  dev->xdpi_offset = SANE_FIX (dev->xdpi_offset *
			       MM_PER_INCH / dev->optical_xdpi);
  dev->ydpi_offset = SANE_FIX (dev->ydpi_offset *
			       MM_PER_INCH / dev->optical_ydpi);

  dev->active = SANE_TRUE;

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_deactivate (Artec48U_Device * dev)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_deactivate")
    SANE_Status status = SANE_STATUS_GOOD;

  CHECK_DEV_ACTIVE (dev, function_name);

  if (dev->read_active)
    artec48u_device_read_finish (dev);

  dev->active = SANE_FALSE;

  return status;
}

static SANE_Status
artec48u_device_memory_write (Artec48U_Device * dev,
			      SANE_Word addr,
			      SANE_Word size, SANE_Byte * data)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_memory_write")
  SANE_Status status;

  XDBG ((8, "%s: dev=%p, addr=0x%x, size=0x%x, data=%p\n",
       function_name, (void *) dev, addr, size, (void *) data));
  CHECK_DEV_ACTIVE (dev, function_name);

  status = sanei_usb_control_msg (dev->fd, 0x40, 0x01,
				  memory_write_value, addr, size, data);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: sanei_usb_control_msg failed: %s\n",
	   function_name, sane_strstatus (status)));
    }

  return status;
}

static SANE_Status
artec48u_device_memory_read (Artec48U_Device * dev,
			     SANE_Word addr, SANE_Word size, SANE_Byte * data)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_memory_read")
  SANE_Status status;

  XDBG ((8, "%s: dev=%p, addr=0x%x, size=0x%x, data=%p\n",
       function_name, (void *) dev, addr, size, data));
  CHECK_DEV_ACTIVE (dev, function_name);

  status = sanei_usb_control_msg (dev->fd, 0xc0, 0x01,
				  memory_read_value, addr, size, data);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: sanei_usb_control_msg failed: %s\n",
	   function_name, sane_strstatus (status)));
    }

  return status;
}

static SANE_Status
artec48u_device_generic_req (Artec48U_Device * dev,
			     SANE_Word cmd_value, SANE_Word cmd_index,
			     SANE_Word res_value, SANE_Word res_index,
			     Artec48U_Packet cmd, Artec48U_Packet res)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_generic_req")
  SANE_Status status;

  XDBG ((7, "%s: command=0x%02x\n", function_name, cmd[0]));
  CHECK_DEV_ACTIVE (dev, function_name);

  status = sanei_usb_control_msg (dev->fd,
				  0x40, 0x01, cmd_value, cmd_index,
				  ARTEC48U_PACKET_SIZE, cmd);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: writing command failed: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  memset (res, 0, sizeof (Artec48U_Packet));

  status = sanei_usb_control_msg (dev->fd,
				  0xc0, 0x01, res_value, res_index,
				  ARTEC48U_PACKET_SIZE, res);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: reading response failed: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }
  return status;
}

static SANE_Status
artec48u_device_req (Artec48U_Device * dev, Artec48U_Packet cmd,
		     Artec48U_Packet res)
{
  return artec48u_device_generic_req (dev,
				      send_cmd_value,
				      send_cmd_index,
				      recv_res_value,
				      recv_res_index, cmd, res);
}

static SANE_Status
artec48u_device_small_req (Artec48U_Device * dev, Artec48U_Packet cmd,
			   Artec48U_Packet res)
{
  Artec48U_Packet fixed_cmd;
  int i;

  for (i = 0; i < 8; ++i)
    memcpy (fixed_cmd + i * 8, cmd, 8);

  return artec48u_device_generic_req (dev,
				      send_small_cmd_value,
				      send_small_cmd_index,
				      recv_small_res_value,
				      recv_small_res_index, fixed_cmd, res);
}

static SANE_Status
artec48u_device_read_raw (Artec48U_Device * dev, SANE_Byte * buffer,
			  size_t * size)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_read_raw")
  SANE_Status status;

  CHECK_DEV_ACTIVE (dev, function_name);

  XDBG ((7, "%s: enter: size=0x%lx\n", function_name, (unsigned long) *size));

  status = sanei_usb_read_bulk (dev->fd, buffer, size);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: bulk read failed: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  XDBG ((7, "%s: leave: size=0x%lx\n", function_name, (unsigned long) *size));

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_set_read_buffer_size (Artec48U_Device * dev,
				      size_t buffer_size)
{
  DECLARE_FUNCTION_NAME ("gt68xx_device_set_read_buffer_size")
    CHECK_DEV_NOT_NULL (dev, function_name);

  if (dev->read_active)
    {
      XDBG ((3, "%s: BUG: read already active\n", function_name));
      return SANE_STATUS_INVAL;
    }

  buffer_size = (buffer_size + 63UL) & ~63UL;
  if (buffer_size > 0)
    {
      dev->requested_buffer_size = buffer_size;
      return SANE_STATUS_GOOD;
    }

  XDBG ((3, "%s: bad buffer size\n", function_name));
  return SANE_STATUS_INVAL;
}

static SANE_Status
artec48u_device_read_prepare (Artec48U_Device * dev, size_t expected_count)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_read_prepare")
    CHECK_DEV_ACTIVE (dev, function_name);

  if (dev->read_active)
    {
      XDBG ((3, "%s: read already active\n", function_name));
      return SANE_STATUS_INVAL;
    }

  dev->read_buffer = (SANE_Byte *) malloc (dev->requested_buffer_size);
  if (!dev->read_buffer)
    {
      XDBG ((3, "%s: not enough memory for the read buffer (%lu bytes)\n",
	   function_name, (unsigned long) dev->requested_buffer_size));
      return SANE_STATUS_NO_MEM;
    }

  dev->read_active = SANE_TRUE;
  dev->read_pos = dev->read_bytes_in_buffer = 0;
  dev->read_bytes_left = expected_count;

  return SANE_STATUS_GOOD;
}

static RETSIGTYPE
reader_process_sigterm_handler (int signal)
{
  XDBG ((1, "reader_process: terminated by signal %d\n", signal));
  _exit (SANE_STATUS_GOOD);
}

static RETSIGTYPE
usb_reader_process_sigterm_handler (int signal)
{
  XDBG ((1, "reader_process (usb): terminated by signal %d\n", signal));
  cancelRead = SANE_TRUE;
}

static SANE_Status
artec48u_device_read_start (Artec48U_Device * dev)
{
  CHECK_DEV_ACTIVE (dev, "artec48u_device_read_start");

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_read (Artec48U_Device * dev, SANE_Byte * buffer,
		      size_t * size)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_read") SANE_Status status;
  size_t byte_count = 0;
  size_t left_to_read = *size;
  size_t transfer_size, block_size, raw_block_size;

  CHECK_DEV_ACTIVE (dev, function_name);

  if (!dev->read_active)
    {
      XDBG ((3, "%s: read not active\n", function_name));
      return SANE_STATUS_INVAL;
    }

  while (left_to_read > 0)
    {
      if (dev->read_bytes_in_buffer == 0)
	{
	  block_size = dev->requested_buffer_size;
	  if (block_size > dev->read_bytes_left)
	    block_size = dev->read_bytes_left;
	  if (block_size == 0)
	    break;
	  raw_block_size = (block_size + 63UL) & ~63UL;
	  status = artec48u_device_read_raw (dev, dev->read_buffer,
					     &raw_block_size);
	  if (status != SANE_STATUS_GOOD)
	    {
	      XDBG ((3, "%s: read failed\n", function_name));
	      return status;
	    }
	  dev->read_pos = 0;
	  dev->read_bytes_in_buffer = block_size;
	  dev->read_bytes_left -= block_size;
	}

      transfer_size = left_to_read;
      if (transfer_size > dev->read_bytes_in_buffer)
	transfer_size = dev->read_bytes_in_buffer;
      if (transfer_size > 0)
	{
	  memcpy (buffer, dev->read_buffer + dev->read_pos, transfer_size);
	  dev->read_pos += transfer_size;
	  dev->read_bytes_in_buffer -= transfer_size;
	  byte_count += transfer_size;
	  left_to_read -= transfer_size;
	  buffer += transfer_size;
	}
    }

  *size = byte_count;

  if (byte_count == 0)
    return SANE_STATUS_EOF;
  else
    return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_device_read_finish (Artec48U_Device * dev)
{
  DECLARE_FUNCTION_NAME ("artec48u_device_read_finish")
    CHECK_DEV_ACTIVE (dev, function_name);

  if (!dev->read_active)
    {
      XDBG ((3, "%s: read not active\n", function_name));
      return SANE_STATUS_INVAL;
    }

  XDBG ((7, "%s: read_bytes_left = %ld\n",
       function_name, (long) dev->read_bytes_left));

  free (dev->read_buffer);
  dev->read_buffer = NULL;

  dev->read_active = SANE_FALSE;

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_delay_buffer_init (Artec48U_Delay_Buffer * delay,
			    SANE_Int pixels_per_line)
{
  DECLARE_FUNCTION_NAME ("artec48u_delay_buffer_init")
    SANE_Int bytes_per_line;
  SANE_Int line_count, i;

  if (pixels_per_line <= 0)
    {
      XDBG ((3, "%s: BUG: pixels_per_line=%d\n",
	   function_name, pixels_per_line));
      return SANE_STATUS_INVAL;
    }

  bytes_per_line = pixels_per_line * sizeof (unsigned int);

  delay->line_count = line_count = 1;
  delay->read_index = 0;
  delay->write_index = 0;

  delay->mem_block = (SANE_Byte *) malloc (bytes_per_line * line_count);
  if (!delay->mem_block)
    {
      XDBG ((3, "%s: no memory for delay block\n", function_name));
      return SANE_STATUS_NO_MEM;
    }

  delay->lines =
    (unsigned int **) malloc (sizeof (unsigned int *) * line_count);
  if (!delay->lines)
    {
      free (delay->mem_block);
      XDBG ((3, "%s: no memory for delay line pointers\n", function_name));
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0; i < line_count; ++i)
    delay->lines[i] =
      (unsigned int *) (delay->mem_block + i * bytes_per_line);

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_delay_buffer_done (Artec48U_Delay_Buffer * delay)
{
  if (delay->lines)
    {
      free (delay->lines);
      delay->lines = NULL;
    }

  if (delay->mem_block)
    {
      free (delay->mem_block);
      delay->mem_block = NULL;
    }

  return SANE_STATUS_GOOD;
}

#define DELAY_BUFFER_WRITE_PTR(delay) ( (delay)->lines[(delay)->write_index] )

#define DELAY_BUFFER_READ_PTR(delay)  ( (delay)->lines[(delay)->read_index ] )

#define DELAY_BUFFER_STEP(delay)                                             \
  do {                                                                       \
    (delay)->read_index  = ((delay)->read_index  + 1) % (delay)->line_count; \
    (delay)->write_index = ((delay)->write_index + 1) % (delay)->line_count; \
  } while (SANE_FALSE)


static inline void
unpack_8_mono (SANE_Byte * src, unsigned int *dst, SANE_Int pixels_per_line)
{
  XDBG ((3, "unpack_8_mono\n"));
  for (; pixels_per_line > 0; ++src, ++dst, --pixels_per_line)
    {
      *dst = (((unsigned int) *src) << 8) | *src;
    }
}

static inline void
unpack_16_le_mono (SANE_Byte * src, unsigned int *dst,
		   SANE_Int pixels_per_line)
{
  XDBG ((3, "unpack_16_le_mono\n"));
  for (; pixels_per_line > 0; src += 2, dst++, --pixels_per_line)
    {
      *dst = (((unsigned int) src[1]) << 8) | src[0];
    }
}

static SANE_Status
line_read_gray_8 (Artec48U_Line_Reader * reader,
		  unsigned int **buffer_pointers_return)
{
  SANE_Status status;
  size_t size;
  unsigned int *buffer;
  XDBG ((3, "line_read_gray_8\n"));

  size = reader->params.scan_bpl;
  status = artec48u_device_read (reader->dev, reader->pixel_buffer, &size);
  if (status != SANE_STATUS_GOOD)
    return status;

  buffer = DELAY_BUFFER_READ_PTR (&reader->g_delay);
  buffer_pointers_return[0] = buffer;
  unpack_8_mono (reader->pixel_buffer, buffer, reader->pixels_per_line);

  return SANE_STATUS_GOOD;
}

static SANE_Status
line_read_gray_16 (Artec48U_Line_Reader * reader,
		   unsigned int **buffer_pointers_return)
{
  SANE_Status status;
  size_t size;
  unsigned int *buffer;

  XDBG ((3, "line_read_gray_16\n"));
  size = reader->params.scan_bpl;
  status = artec48u_device_read (reader->dev, reader->pixel_buffer, &size);
  if (status != SANE_STATUS_GOOD)
    return status;

  buffer = DELAY_BUFFER_READ_PTR (&reader->g_delay);
  buffer_pointers_return[0] = buffer;
  unpack_16_le_mono (reader->pixel_buffer, buffer, reader->pixels_per_line);

  return SANE_STATUS_GOOD;
}

static SANE_Status
line_read_bgr_8_line_mode (Artec48U_Line_Reader * reader,
			   unsigned int **buffer_pointers_return)
{
  SANE_Status status;
  size_t size;
  SANE_Int pixels_per_line;
  SANE_Byte *pixel_buffer = reader->pixel_buffer;
  XDBG ((3, "line_read_bgr_8_line_mode\n"));

  size = reader->params.scan_bpl * 3;
  status = artec48u_device_read (reader->dev, pixel_buffer, &size);
  if (status != SANE_STATUS_GOOD)
    return status;

  pixels_per_line = reader->pixels_per_line;
  unpack_8_mono (pixel_buffer,
		 DELAY_BUFFER_WRITE_PTR (&reader->b_delay), pixels_per_line);
  pixel_buffer += reader->params.scan_bpl;
  unpack_8_mono (pixel_buffer,
		 DELAY_BUFFER_WRITE_PTR (&reader->g_delay), pixels_per_line);
  pixel_buffer += reader->params.scan_bpl;
  unpack_8_mono (pixel_buffer,
		 DELAY_BUFFER_WRITE_PTR (&reader->r_delay), pixels_per_line);

  buffer_pointers_return[0] = DELAY_BUFFER_READ_PTR (&reader->r_delay);
  buffer_pointers_return[1] = DELAY_BUFFER_READ_PTR (&reader->g_delay);
  buffer_pointers_return[2] = DELAY_BUFFER_READ_PTR (&reader->b_delay);

  DELAY_BUFFER_STEP (&reader->r_delay);
  DELAY_BUFFER_STEP (&reader->g_delay);
  DELAY_BUFFER_STEP (&reader->b_delay);

  return SANE_STATUS_GOOD;
}

static SANE_Status
line_read_bgr_16_line_mode (Artec48U_Line_Reader * reader,
			    unsigned int **buffer_pointers_return)
{
  SANE_Status status;
  size_t size;
  SANE_Int pixels_per_line;
  SANE_Byte *pixel_buffer = reader->pixel_buffer;

  XDBG ((3, "line_read_bgr_16_line_mode\n"));
  size = reader->params.scan_bpl * 3;
  status = artec48u_device_read (reader->dev, pixel_buffer, &size);
  if (status != SANE_STATUS_GOOD)
    return status;

  pixels_per_line = reader->pixels_per_line;
  unpack_16_le_mono (pixel_buffer,
		     DELAY_BUFFER_WRITE_PTR (&reader->b_delay),
		     pixels_per_line);
  pixel_buffer += reader->params.scan_bpl;
  unpack_16_le_mono (pixel_buffer,
		     DELAY_BUFFER_WRITE_PTR (&reader->g_delay),
		     pixels_per_line);
  pixel_buffer += reader->params.scan_bpl;
  unpack_16_le_mono (pixel_buffer,
		     DELAY_BUFFER_WRITE_PTR (&reader->r_delay),
		     pixels_per_line);

  buffer_pointers_return[0] = DELAY_BUFFER_READ_PTR (&reader->r_delay);
  buffer_pointers_return[1] = DELAY_BUFFER_READ_PTR (&reader->g_delay);
  buffer_pointers_return[2] = DELAY_BUFFER_READ_PTR (&reader->b_delay);

  DELAY_BUFFER_STEP (&reader->r_delay);
  DELAY_BUFFER_STEP (&reader->g_delay);
  DELAY_BUFFER_STEP (&reader->b_delay);

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_line_reader_init_delays (Artec48U_Line_Reader * reader)
{
  SANE_Status status;

  if (reader->params.color)
    {
      status = artec48u_delay_buffer_init (&reader->r_delay,
					   reader->params.pixel_xs);
      if (status != SANE_STATUS_GOOD)
	return status;

      status = artec48u_delay_buffer_init (&reader->g_delay,
					   reader->params.pixel_xs);
      if (status != SANE_STATUS_GOOD)
	{
	  artec48u_delay_buffer_done (&reader->r_delay);
	  return status;
	}

      status = artec48u_delay_buffer_init (&reader->b_delay,
					   reader->params.pixel_xs);
      if (status != SANE_STATUS_GOOD)
	{
	  artec48u_delay_buffer_done (&reader->g_delay);
	  artec48u_delay_buffer_done (&reader->r_delay);
	  return status;
	}
    }
  else
    {
      status = artec48u_delay_buffer_init (&reader->g_delay,
					   reader->params.pixel_xs);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  reader->delays_initialized = SANE_TRUE;

  return SANE_STATUS_GOOD;
}

static void
artec48u_line_reader_free_delays (Artec48U_Line_Reader * reader)
{
  if (!reader)
    {
      return;
    }
  if (reader->delays_initialized)
    {
      if (reader->params.color)
	{
	  artec48u_delay_buffer_done (&reader->b_delay);
	  artec48u_delay_buffer_done (&reader->g_delay);
	  artec48u_delay_buffer_done (&reader->r_delay);
	}
      else
	{
	  artec48u_delay_buffer_done (&reader->g_delay);
	}
      reader->delays_initialized = SANE_FALSE;
    }
}

static SANE_Status
artec48u_line_reader_new (Artec48U_Device * dev,
			  Artec48U_Scan_Parameters * params,
			  Artec48U_Line_Reader ** reader_return)
{
  DECLARE_FUNCTION_NAME ("artec48u_line_reader_new") SANE_Status status;
  Artec48U_Line_Reader *reader;
  SANE_Int image_size;
  SANE_Int scan_bpl_full;

  XDBG ((6, "%s: enter\n", function_name));
  XDBG ((6, "%s: enter params xdpi: %i\n", function_name, params->xdpi));
  XDBG ((6, "%s: enter params ydpi: %i\n", function_name, params->ydpi));
  XDBG ((6, "%s: enter params depth: %i\n", function_name, params->depth));
  XDBG ((6, "%s: enter params color: %i\n", function_name, params->color));
  XDBG ((6, "%s: enter params pixel_xs: %i\n", function_name, params->pixel_xs));
  XDBG ((6, "%s: enter params pixel_ys: %i\n", function_name, params->pixel_ys));
  XDBG ((6, "%s: enter params scan_xs: %i\n", function_name, params->scan_xs));
  XDBG ((6, "%s: enter params scan_ys: %i\n", function_name, params->scan_ys));
  XDBG ((6, "%s: enter params scan_bpl: %i\n", function_name, params->scan_bpl));
  *reader_return = NULL;

  reader = (Artec48U_Line_Reader *) malloc (sizeof (Artec48U_Line_Reader));
  if (!reader)
    {
      XDBG ((3, "%s: cannot allocate Artec48U_Line_Reader\n", function_name));
      return SANE_STATUS_NO_MEM;
    }
  memset (reader, 0, sizeof (Artec48U_Line_Reader));

  reader->dev = dev;
  memcpy (&reader->params, params, sizeof (Artec48U_Scan_Parameters));
  reader->pixel_buffer = 0;
  reader->delays_initialized = SANE_FALSE;

  reader->read = NULL;

  status = artec48u_line_reader_init_delays (reader);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: cannot allocate line buffers: %s\n",
	   function_name, sane_strstatus (status)));
      free (reader);
      return status;
    }

  reader->pixels_per_line = reader->params.pixel_xs;

  if (!reader->params.color)
    {
      XDBG ((2, "!reader->params.color\n"));
      if (reader->params.depth == 8)
	reader->read = line_read_gray_8;
      else if (reader->params.depth == 16)
	reader->read = line_read_gray_16;
    }
  else
    {
      XDBG ((2, "reader line mode\n"));
      if (reader->params.depth == 8)
	{
	  XDBG ((2, "depth 8\n"));
	  reader->read = line_read_bgr_8_line_mode;
	}
      else if (reader->params.depth == 16)
	{
	  XDBG ((2, "depth 16\n"));
	  reader->read = line_read_bgr_16_line_mode;
	}
    }

  if (reader->read == NULL)
    {
      XDBG ((3, "%s: unsupported bit depth (%d)\n",
	   function_name, reader->params.depth));
      artec48u_line_reader_free_delays (reader);
      free (reader);
      return SANE_STATUS_UNSUPPORTED;
    }

  scan_bpl_full = reader->params.scan_bpl;
  if (reader->params.color)
    scan_bpl_full *= 3;

  reader->pixel_buffer = malloc (scan_bpl_full);
  if (!reader->pixel_buffer)
    {
      XDBG ((3, "%s: cannot allocate pixel buffer\n", function_name));
      artec48u_line_reader_free_delays (reader);
      free (reader);
      return SANE_STATUS_NO_MEM;
    }

  artec48u_device_set_read_buffer_size (reader->dev,
					scan_bpl_full /* 200 */ );

  image_size = scan_bpl_full * reader->params.scan_ys;
  status = artec48u_device_read_prepare (reader->dev, image_size);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: artec48u_device_read_prepare failed: %s\n",
	   function_name, sane_strstatus (status)));
      free (reader->pixel_buffer);
      artec48u_line_reader_free_delays (reader);
      free (reader);
      return status;
    }

  XDBG ((6, "%s: leave: ok\n", function_name));
  *reader_return = reader;
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_line_reader_free (Artec48U_Line_Reader * reader)
{
  DECLARE_FUNCTION_NAME ("artec48u_line_reader_free") SANE_Status status;

  XDBG ((6, "%s: enter\n", function_name));

  if (!reader)
    {
      return SANE_STATUS_GOOD;
    }
  artec48u_line_reader_free_delays (reader);

  if (reader->pixel_buffer)
    {
      free (reader->pixel_buffer);
      reader->pixel_buffer = NULL;
    }

  status = artec48u_device_read_finish (reader->dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "%s: artec48u_device_read_finish failed: %s\n",
	   function_name, sane_strstatus (status)));
    }

  if (reader)
    free (reader);

  XDBG ((6, "%s: leave\n", function_name));
  return status;
}

static SANE_Status
artec48u_line_reader_read (Artec48U_Line_Reader * reader,
			   unsigned int **buffer_pointers_return)
{
  return (*reader->read) (reader, buffer_pointers_return);
}

static SANE_Status
artec48u_scanner_new (Artec48U_Device * dev,
		      Artec48U_Scanner ** scanner_return)
{
  DECLARE_FUNCTION_NAME ("artec48u_scanner_new") Artec48U_Scanner *s;

  *scanner_return = NULL;

  s = (Artec48U_Scanner *) malloc (sizeof (Artec48U_Scanner));
  if (!s)
    {
      XDBG ((5, "%s: no memory for Artec48U_Scanner\n", function_name));
      return SANE_STATUS_NO_MEM;
    }
  s->dev = dev;
  s->reader = NULL;
  s->scanning = SANE_FALSE;
  s->line_buffer = NULL;
  s->lineart_buffer = NULL;
  s->next = NULL;
  s->pipe_handle = NULL;
  s->buffer_pointers[0] = NULL;
  s->buffer_pointers[1] = NULL;
  s->buffer_pointers[2] = NULL;
  s->shading_buffer_w = NULL;
  s->shading_buffer_b = NULL;
  s->shading_buffer_white[0] = NULL;
  s->shading_buffer_white[1] = NULL;
  s->shading_buffer_white[2] = NULL;
  s->shading_buffer_black[0] = NULL;
  s->shading_buffer_black[1] = NULL;
  s->shading_buffer_black[2] = NULL;
  *scanner_return = s;
  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_scanner_read_line (Artec48U_Scanner * s,
			    unsigned int **buffer_pointers, SANE_Bool shading)
{
  DECLARE_FUNCTION_NAME ("artec48u_scanner_read_line") SANE_Status status;
  int i, j, c;

  status = artec48u_line_reader_read (s->reader, buffer_pointers);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((5, "%s: artec48u_line_reader_read failed: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }
  if (shading != SANE_TRUE)
    return status;

  c = s->reader->pixels_per_line;
  if (s->reader->params.color == SANE_TRUE)
    {
      for (i = c - 1; i >= 0; i--)
	{
	  for (j = 0; j < 3; j++)
	    {
	      int new_value;
	      unsigned int value = buffer_pointers[j][i];
	      if (value < s->shading_buffer_black[j][i])
		value = s->shading_buffer_black[j][i];
	      if (value > s->shading_buffer_white[j][i])
		value = s->shading_buffer_white[j][i];
	      new_value =
		(double) (value -
			  s->shading_buffer_black[j][i]) * 65535.0 /
		(double) (s->shading_buffer_white[j][i] -
			  s->shading_buffer_black[j][i]);
	      if (new_value < 0)
		new_value = 0;
	      if (new_value > 65535)
		new_value = 65535;
	      new_value =
		s->gamma_array[j +
			       1][s->contrast_array[s->
						    brightness_array
						    [new_value]]];
	      new_value = s->gamma_array[0][new_value];
	      buffer_pointers[j][i] = new_value;
	    }
	}
    }
  else
    {
      for (i = c - 1; i >= 0; i--)
	{
	  int new_value;
	  unsigned int value = buffer_pointers[0][i];
	  new_value =
	    (double) (value -
		      s->shading_buffer_black[1][i]) * 65535.0 /
	    (double) (s->shading_buffer_white[1][i] -
		      s->shading_buffer_black[1][i]);
	  if (new_value < 0)
	    new_value = 0;
	  if (new_value > 65535)
	    new_value = 65535;
	  new_value =
	    s->gamma_array[0][s->
			      contrast_array[s->brightness_array[new_value]]];
	  buffer_pointers[0][i] = new_value;
	}
    }
  return status;
}

static SANE_Status
artec48u_scanner_free (Artec48U_Scanner * s)
{
  DECLARE_FUNCTION_NAME ("artec48u_scanner_free") if (!s)
    {
      XDBG ((5, "%s: scanner==NULL\n", function_name));
      return SANE_STATUS_INVAL;
    }

  if (s->reader)
    {
      artec48u_line_reader_free (s->reader);
      s->reader = NULL;
    }

  free (s->shading_buffer_w);
  free (s->shading_buffer_b);
  free (s->shading_buffer_white[0]);
  free (s->shading_buffer_black[0]);
  free (s->shading_buffer_white[1]);
  free (s->shading_buffer_black[1]);
  free (s->shading_buffer_white[2]);
  free (s->shading_buffer_black[2]);

  if (s->line_buffer)
    free (s->line_buffer);
  if (s->lineart_buffer)
    free (s->lineart_buffer);

  free (s);

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_scanner_internal_start_scan (Artec48U_Scanner * s)
{
  DECLARE_FUNCTION_NAME ("artec48u_scanner_internal_start_scan")
    SANE_Status status;
  SANE_Bool ready;
  SANE_Int repeat_count;

  status = artec48u_wait_for_positioning (s->dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "%s: artec48u_scanner_wait_for_positioning error: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  status = artec48u_generic_start_scan (s->dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "%s: artec48u_device_start_scan error: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  for (repeat_count = 0; repeat_count < 30 * 10; ++repeat_count)
    {
      status = artec48u_generic_read_scanned_data (s->dev, &ready);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((2, "%s: artec48u_device_read_scanned_data error: %s\n",
	       function_name, sane_strstatus (status)));
	  return status;
	}
      if (ready)
	break;
      usleep (100000);
    }

  if (!ready)
    {
      XDBG ((2, "%s: scanner still not ready - giving up\n", function_name));
      return SANE_STATUS_DEVICE_BUSY;
    }

  status = artec48u_device_read_start (s->dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "%s: artec48u_device_read_start error: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_scanner_start_scan_extended (Artec48U_Scanner * s,
				      Artec48U_Scan_Request * request,
				      Artec48U_Scan_Action action,
				      Artec48U_Scan_Parameters * params)
{
  DECLARE_FUNCTION_NAME ("artec48u_scanner_start_scan_extended")
    SANE_Status status;

  status = artec48u_wait_for_positioning (s->dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "%s: artec48u_scanner_wait_for_positioning error: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  if (action == SA_SCAN)
    status = artec48u_setup_scan (s, request, action, SANE_FALSE, params);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "%s: artec48u_device_setup_scan failed: %s\n", function_name,
	   sane_strstatus (status)));
      return status;
    }
  status = artec48u_line_reader_new (s->dev, params, &s->reader);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "%s: artec48u_line_reader_new failed: %s\n", function_name,
	   sane_strstatus (status)));
      return status;
    }

  status = artec48u_scanner_internal_start_scan (s);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "%s: artec48u_scanner_internal_start_scan failed: %s\n",
	   function_name, sane_strstatus (status)));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
artec48u_scanner_start_scan (Artec48U_Scanner * s,
			     Artec48U_Scan_Request * request,
			     Artec48U_Scan_Parameters * params)
{
  return artec48u_scanner_start_scan_extended (s, request, SA_SCAN, params);
}


static SANE_Status
artec48u_scanner_stop_scan (Artec48U_Scanner * s)
{
  XDBG ((1, "artec48u_scanner_stop_scan begin: \n"));
  artec48u_line_reader_free (s->reader);
  s->reader = NULL;

  return artec48u_stop_scan (s->dev);
}

static void
calculateGamma (Artec48U_Scanner * s)
{
  double d;
  int gval;
  unsigned int i;

  double gamma = SANE_UNFIX (s->val[OPT_GAMMA].w);

  d = 65536.0 / pow (65536.0, 1.0 / gamma);
  for (i = 0; i < 65536; i++)
    {
      gval = (int) (pow ((double) i, 1.0 / gamma) * d);
      s->gamma_array[0][i] = gval;
    }
}

static void
calculateGammaRed (Artec48U_Scanner * s)
{
  double d;
  int gval;
  unsigned int i;

  double gamma = SANE_UNFIX (s->val[OPT_GAMMA_R].w);

  d = 65536.0 / pow (65536.0, 1.0 / gamma);
  for (i = 0; i < 65536; i++)
    {
      gval = (int) (pow ((double) i, 1.0 / gamma) * d);
      s->gamma_array[1][i] = gval;
    }
}

static void
calculateGammaGreen (Artec48U_Scanner * s)
{
  double d;
  int gval;
  unsigned int i;

  double gamma = SANE_UNFIX (s->val[OPT_GAMMA_G].w);

  d = 65536.0 / pow (65536.0, 1.0 / gamma);
  for (i = 0; i < 65536; i++)
    {
      gval = (int) (pow ((double) i, 1.0 / gamma) * d);
      s->gamma_array[2][i] = gval;
    }
}

static void
calculateGammaBlue (Artec48U_Scanner * s)
{
  double d;
  int gval;
  unsigned int i;

  double gamma = SANE_UNFIX (s->val[OPT_GAMMA_B].w);

  d = 65536.0 / pow (65536.0, 1.0 / gamma);
  for (i = 0; i < 65536; i++)
    {
      gval = (int) (pow ((double) i, 1.0 / gamma) * d);
      s->gamma_array[3][i] = gval;
    }
}

static SANE_Status
artec48u_calculate_shading_buffer (Artec48U_Scanner * s, int start, int end,
				   int resolution, SANE_Bool color)
{
  int i;
  int c;
  int bpp;
  c = 0;
  bpp = 6;
  switch (resolution)
    {
    case 50:
      bpp = 72;
      break;
    case 100:
      bpp = 36;
      break;
    case 200:
      bpp = 18;
      break;
    case 300:
      bpp = 12;
      break;
    case 600:
      bpp = 6;
      break;
    case 1200:
      if(s->dev->is_epro == 0)
        bpp = 6;
      else
        bpp = 3;
    }

  for (i = start * bpp; i < end * bpp; i += bpp)
    {
      if (color)
	{
	  s->shading_buffer_white[0][c] =
	    (unsigned int) s->shading_buffer_w[i] +
	    ((((unsigned int) s->shading_buffer_w[i + 1]) << 8));
	  s->shading_buffer_white[2][c] =
	    (unsigned int) s->shading_buffer_w[i + 4] +
	    ((((unsigned int) s->shading_buffer_w[i + 5]) << 8));
	  s->shading_buffer_black[0][c] =
	    (unsigned int) s->shading_buffer_b[i] +
	    ((((unsigned int) s->shading_buffer_b[i + 1]) << 8));
	  s->shading_buffer_black[2][c] =
	    (unsigned int) s->shading_buffer_b[i + 4] +
	    ((((unsigned int) s->shading_buffer_b[i + 5]) << 8));
	}
      s->shading_buffer_white[1][c] =
	(unsigned int) s->shading_buffer_w[i + 2] +
	((((unsigned int) s->shading_buffer_w[i + 3]) << 8));
      s->shading_buffer_black[1][c] =
	(unsigned int) s->shading_buffer_b[i + 2] +
	((((unsigned int) s->shading_buffer_b[i + 3]) << 8));
      ++c;
    }
  return SANE_STATUS_GOOD;
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  SANE_Int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

static SANE_Status
init_options (Artec48U_Scanner * s)
{
  int i;

  XDBG ((5, "init_options: scanner %p\n", (void *) s));
  XDBG ((5, "init_options: start\n"));
  XDBG ((5, "init_options: num options %i\n", NUM_OPTIONS));

  memset (s->val, 0, sizeof (s->val));
  memset (s->opt, 0, sizeof (s->opt));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].unit = SANE_UNIT_NONE;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  s->opt[OPT_MODE_GROUP].name = "scanmode-group";
  s->opt[OPT_MODE_GROUP].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].size = 0;
  s->opt[OPT_MODE_GROUP].unit = SANE_UNIT_NONE;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_MODE_GROUP].cap = 0;

  s->opt[OPT_SCAN_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_SCAN_MODE].size = max_string_size (mode_list);
  s->opt[OPT_SCAN_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCAN_MODE].constraint.string_list = mode_list;
  s->val[OPT_SCAN_MODE].s = strdup (mode_list[1]);

  s->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
  s->opt[OPT_BIT_DEPTH].unit = SANE_UNIT_NONE;
  s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_BIT_DEPTH].constraint.word_list = bitdepth_list;
  s->val[OPT_BIT_DEPTH].w = bitdepth_list[1];

  /* black level (lineart only) */
  s->opt[OPT_BLACK_LEVEL].name = SANE_NAME_BLACK_LEVEL;
  s->opt[OPT_BLACK_LEVEL].title = SANE_TITLE_BLACK_LEVEL;
  s->opt[OPT_BLACK_LEVEL].desc = SANE_DESC_BLACK_LEVEL;
  s->opt[OPT_BLACK_LEVEL].type = SANE_TYPE_INT;
  s->opt[OPT_BLACK_LEVEL].unit = SANE_UNIT_NONE;
  s->opt[OPT_BLACK_LEVEL].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BLACK_LEVEL].constraint.range = &blacklevel_range;
  s->opt[OPT_BLACK_LEVEL].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_BLACK_LEVEL].w = 127;

  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = resbit_list;
  s->val[OPT_RESOLUTION].w = resbit_list[1];

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].name = "enhancement-group";
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N ("Enhancement");
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &brightness_contrast_range;
  s->val[OPT_BRIGHTNESS].w = 0;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &brightness_contrast_range;
  s->val[OPT_CONTRAST].w = 0;

  /* master analog gamma */
  s->opt[OPT_GAMMA].name = SANE_NAME_ANALOG_GAMMA;
  s->opt[OPT_GAMMA].title = SANE_TITLE_ANALOG_GAMMA;
  s->opt[OPT_GAMMA].desc = SANE_DESC_ANALOG_GAMMA;
  s->opt[OPT_GAMMA].type = SANE_TYPE_FIXED;
  s->opt[OPT_GAMMA].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA].constraint.range = &gamma_range;
  s->val[OPT_GAMMA].w = SANE_FIX (s->dev->gamma_master);
  s->opt[OPT_GAMMA].size = sizeof (SANE_Word);

  /* red analog gamma */
  s->opt[OPT_GAMMA_R].name = SANE_NAME_ANALOG_GAMMA_R;
  s->opt[OPT_GAMMA_R].title = SANE_TITLE_ANALOG_GAMMA_R;
  s->opt[OPT_GAMMA_R].desc = SANE_DESC_ANALOG_GAMMA_R;
  s->opt[OPT_GAMMA_R].type = SANE_TYPE_FIXED;
  s->opt[OPT_GAMMA_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_R].constraint.range = &gamma_range;
  s->val[OPT_GAMMA_R].w = SANE_FIX (s->dev->gamma_r);

  /* green analog gamma */
  s->opt[OPT_GAMMA_G].name = SANE_NAME_ANALOG_GAMMA_G;
  s->opt[OPT_GAMMA_G].title = SANE_TITLE_ANALOG_GAMMA_G;
  s->opt[OPT_GAMMA_G].desc = SANE_DESC_ANALOG_GAMMA_G;
  s->opt[OPT_GAMMA_G].type = SANE_TYPE_FIXED;
  s->opt[OPT_GAMMA_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_G].constraint.range = &gamma_range;
  s->val[OPT_GAMMA_G].w = SANE_FIX (s->dev->gamma_g);

  /* blue analog gamma */
  s->opt[OPT_GAMMA_B].name = SANE_NAME_ANALOG_GAMMA_B;
  s->opt[OPT_GAMMA_B].title = SANE_TITLE_ANALOG_GAMMA_B;
  s->opt[OPT_GAMMA_B].desc = SANE_DESC_ANALOG_GAMMA_B;
  s->opt[OPT_GAMMA_B].type = SANE_TYPE_FIXED;
  s->opt[OPT_GAMMA_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_B].constraint.range = &gamma_range;
  s->val[OPT_GAMMA_B].w = SANE_FIX (s->dev->gamma_b);

  s->opt[OPT_DEFAULT_ENHANCEMENTS].name = "default-enhancements";
  s->opt[OPT_DEFAULT_ENHANCEMENTS].title = SANE_I18N ("Defaults");
  s->opt[OPT_DEFAULT_ENHANCEMENTS].desc =
    SANE_I18N ("Set default values for enhancement controls.");
  s->opt[OPT_DEFAULT_ENHANCEMENTS].size = 0;
  s->opt[OPT_DEFAULT_ENHANCEMENTS].type = SANE_TYPE_BUTTON;
  s->opt[OPT_DEFAULT_ENHANCEMENTS].unit = SANE_UNIT_NONE;
  s->opt[OPT_DEFAULT_ENHANCEMENTS].constraint_type = SANE_CONSTRAINT_NONE;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].name = "geometry-group";
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = 0;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &scan_range_x;
  s->val[OPT_TL_X].w = SANE_FIX (0.0);

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &scan_range_y;
  s->val[OPT_TL_Y].w = SANE_FIX (0.0);

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &scan_range_x;
  s->val[OPT_BR_X].w = SANE_FIX (50.0);

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &scan_range_y;
  s->val[OPT_BR_Y].w = SANE_FIX (50.0);

  /* "Calibration" group: */
  s->opt[OPT_CALIBRATION_GROUP].name = "calibration-group";
  s->opt[OPT_CALIBRATION_GROUP].title = SANE_I18N ("Calibration");
  s->opt[OPT_CALIBRATION_GROUP].desc = "";
  s->opt[OPT_CALIBRATION_GROUP].size = 0;
  s->opt[OPT_CALIBRATION_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_CALIBRATION_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_CALIBRATION_GROUP].cap = 0;

  /* calibrate */
  s->opt[OPT_CALIBRATE].name = "calibration";
  s->opt[OPT_CALIBRATE].title = SANE_I18N ("Calibrate before next scan");
  s->opt[OPT_CALIBRATE].desc =
    SANE_I18N ("If enabled, the device will be calibrated before the "
	       "next scan. Otherwise, calibration is performed "
	       "only before the first start.");
  s->opt[OPT_CALIBRATE].type = SANE_TYPE_BOOL;
  s->opt[OPT_CALIBRATE].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_CALIBRATE].w = SANE_FALSE;

  /* calibrate */
  s->opt[OPT_CALIBRATE_SHADING].name = "calibration-shading";
  s->opt[OPT_CALIBRATE_SHADING].title =
    SANE_I18N ("Only perform shading-correction");
  s->opt[OPT_CALIBRATE_SHADING].desc =
    SANE_I18N ("If enabled, only the shading correction is "
	       "performed during calibration. The default values "
	       "for gain, offset and exposure time, "
	       "either build-in or from the configuration file, "
	       "are used.");
  s->opt[OPT_CALIBRATE_SHADING].type = SANE_TYPE_BOOL;
  s->opt[OPT_CALIBRATE_SHADING].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATE_SHADING].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_CALIBRATE_SHADING].w = SANE_FALSE;
#ifdef ARTEC48U_USE_BUTTONS
  s->opt[OPT_BUTTON_STATE].name = "button-state";
  s->opt[OPT_BUTTON_STATE].title = SANE_I18N ("Button state");
  s->opt[OPT_BUTTON_STATE].type = SANE_TYPE_INT;
  s->opt[OPT_BUTTON_STATE].unit = SANE_UNIT_NONE;
  s->opt[OPT_BUTTON_STATE].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_BUTTON_STATE].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_BUTTON_STATE].w = 0;
#endif
  return SANE_STATUS_GOOD;
}

static void
calculate_brightness (Artec48U_Scanner * s)
{
  long cnt;
  double bright;

  bright = (double) s->val[OPT_BRIGHTNESS].w;

  bright *= 257.0;
  for (cnt = 0; cnt < 65536; cnt++)
    {
      if (bright < 0.0)
	s->brightness_array[cnt] =
	  (int) (((double) cnt * (65535.0 + bright)) / 65535.0);
      else
	s->brightness_array[cnt] =
	  (int) ((double) cnt +
		 ((65535.0 - (double) cnt) * bright) / 65535.0);
      if (s->brightness_array[cnt] > 65535)
	s->brightness_array[cnt] = 65535;
      if (s->brightness_array[cnt] < 0)
	s->brightness_array[cnt] = 0;
    }
}

static void
calculate_contrast (Artec48U_Scanner * s)
{
  int val;
  double p;
  int cnt;
  double contr;

  contr = (double) s->val[OPT_CONTRAST].w;

  contr *= 257.0;

  for (cnt = 0; cnt < 65536; cnt++)
    {
      if (contr < 0.0)
	{
	  val = (int) (cnt > 32769) ? (65535 - cnt) : cnt;
	  val = (int) (32769.0 * pow ((double) (val ? val : 1) / 32769.0,
				      (32769.0 + contr) / 32769.0));
	  s->contrast_array[cnt] = (cnt > 32769) ? (65535 - val) : val;
	  if (s->contrast_array[cnt] > 65535)
	    s->contrast_array[cnt] = 65535;
	  if (s->contrast_array[cnt] < 0)
	    s->contrast_array[cnt] = 0;
	}
      else
	{
	  val = (cnt > 32769) ? (65535 - cnt) : cnt;
	  p = ((int) contr == 32769) ? 32769.0 : 32769.0 / (32769.0 - contr);
	  val = (int) (32769.0 * pow ((double) val / 32769.0, p));
	  s->contrast_array[cnt] = (cnt > 32639) ? (65535 - val) : val;
	  if (s->contrast_array[cnt] > 65535)
	    s->contrast_array[cnt] = 65535;
	  if (s->contrast_array[cnt] < 0)
	    s->contrast_array[cnt] = 0;
	}
    }
}

/*
  The calibration function
  Disclaimer: the following might be complete crap :-)
  -Gain, offset, exposure time
   It seems, that the gain values are actually constants. The windows driver always
   uses the values 0x0a,0x03,0x03, during calibration as well as during a normal
   scan. The exposure values are set to 0x04 for black calibration. It's not necessary to
   move the scan head during this stage.
   Calibration starts with default values for offset/exposure. These values are
   increased/decreased until the white and black values are within a specific range, defined
   by WHITE_MIN, WHITE_MAX, BLACK_MIN and BLACK_MAX.

  -White shading correction
   The scanning head is moved some lines over the calibration strip. Some lines
   are scanned at 600dpi/16bit over the full width. The average values are used for the
   shading buffer. The normal exposure values are used.
  -Black shading correction
   Works like the white shading correction, with the difference, that the red-, green-
   and blue exposure time is set to 0x04 (the value is taken from the windoze driver).
  -Since we do this over the whole width of the image with the maximal optical resolution,
   we can use the shading data for every scan, independend of the size, position or resolution,
   because we have the shading values for every sensor/LED.

  Note:
  For a CIS device, it's sufficient to determine those values once. It's not necessary, to
  repeat the calibration sequence before every new scan. The windoze driver even saves the values
  to various files to avoid the quite lengthy calibration sequence. This backend can also save
  the values to files. For this purpose, the user has to create a hidden directory called
  .artec-eplus48u in his/her home directory. If the user insists on calibration
  before every new scan, he/she can enable a specific option in the backend.
*/
static SANE_Status
calibrate_scanner (SANE_Handle handle)
{
  Artec48U_Scanner *s = handle;
  unsigned int *buffer_pointers[3];
  int avg_black[3];
  int avg_white[3];
  int exp_off;
  int c;
  int finish = 0;
  int noloop = 0;


  if ((s->val[OPT_CALIBRATE].w == SANE_TRUE) &&
      (s->val[OPT_CALIBRATE_SHADING].w == SANE_FALSE))
    {
      while (finish == 0)
	{
	  finish = 1;
	  /*get black values */
	  artec48u_carriage_home (s->dev);

	  artec48u_wait_for_positioning (s->dev);
	  s->reader = NULL;

	  s->scanning = SANE_TRUE;

	  init_shading_buffer (s);

	  artec48u_setup_scan (s, &(s->request), SA_CALIBRATE_SCAN_BLACK,
			       SANE_FALSE, &(s->params));
	  artec48u_scanner_start_scan_extended (s, &(s->request),
						SA_CALIBRATE_SCAN_OFFSET_1,
						&(s->params));

	  for (c = 0; c < s->dev->shading_lines_b; c++)
	    {
	      artec48u_scanner_read_line (s, buffer_pointers, SANE_FALSE);
	      /* we abuse the shading buffer for the offset calculation */
	      add_to_shading_buffer (s, buffer_pointers);
	    }
	  artec48u_scanner_stop_scan (s);
	  finish_offset_buffer (s, &avg_black[0], &avg_black[1],
				&avg_black[2]);
	  s->scanning = SANE_FALSE;
	  XDBG ((1, "avg_r: %i, avg_g: %i, avg_b: %i\n", avg_black[0],
	       avg_black[1], avg_black[2]));
	  /*adjust offset */
	  for (c = 0; c < 3; c++)
	    {
	      if (c == 0)
		{
		  if (avg_black[c] < BLACK_MIN)
		    {
		      s->dev->afe_params.r_offset -= 1;
		      finish = 0;
		      XDBG ((1, "adjust offset r: -1\n"));
		    }
		  else if (avg_black[c] > BLACK_MAX)
		    {
		      s->dev->afe_params.r_offset += 1;
		      finish = 0;
		      XDBG ((1, "adjust offset r: +1\n"));
		    }
		}
	      if (c == 1)
		{
		  if (avg_black[c] < BLACK_MIN)
		    {
		      s->dev->afe_params.g_offset -= 1;
		      finish = 0;
		      XDBG ((1, "adjust offset g: -1\n"));
		    }
		  else if (avg_black[c] > BLACK_MAX)
		    {
		      s->dev->afe_params.g_offset += 1;
		      finish = 0;
		      XDBG ((1, "adjust offset g: +1\n"));
		    }
		}
	      if (c == 2)
		{
		  if (avg_black[c] < BLACK_MIN)
		    {
		      s->dev->afe_params.b_offset -= 1;
		      finish = 0;
		      XDBG ((1, "adjust offset b: -1\n"));
		    }
		  else if (avg_black[c] > BLACK_MAX)
		    {
		      s->dev->afe_params.b_offset += 1;
		      finish = 0;
		      XDBG ((1, "adjust offset b: +1\n"));
		    }
		}
	    }

	  /*adjust exposure */
	  /*get white values */

	  artec48u_carriage_home (s->dev);

	  artec48u_wait_for_positioning (s->dev);
	  s->reader = NULL;

	  s->scanning = SANE_TRUE;

	  init_shading_buffer (s);

	  artec48u_setup_scan (s, &(s->request), SA_CALIBRATE_SCAN_WHITE,
			       SANE_FALSE, &(s->params));
	  artec48u_scanner_start_scan_extended (s, &(s->request),
						SA_CALIBRATE_SCAN_EXPOSURE_1,
						&(s->params));

	  for (c = 0; c < s->dev->shading_lines_w; c++)
	    {
	      artec48u_scanner_read_line (s, buffer_pointers, SANE_FALSE);
	      /* we abuse the shading buffer for the exposure calculation */
	      add_to_shading_buffer (s, buffer_pointers);
	    }
	  artec48u_scanner_stop_scan (s);
	  finish_exposure_buffer (s, &avg_white[0], &avg_white[1],
				  &avg_white[2]);
	  s->scanning = SANE_FALSE;
	  XDBG ((1, "avg_r: %i, avg_g: %i, avg_b: %i\n", avg_white[0],
	       avg_white[1], avg_white[2]));
	  for (c = 0; c < 3; c++)
	    {
	      if (c == 0)
		{
		  if (avg_white[c] < WHITE_MIN)
		    {
		      exp_off =
			((WHITE_MAX + WHITE_MIN) / 2 -
			 avg_white[c]) / EXPOSURE_STEP;
		      if (exp_off < 1)
			exp_off = 1;
		      s->dev->exp_params.r_time += exp_off;
		      finish = 0;
		      XDBG ((1, "adjust exposure r: ++\n"));
		    }
		  else if (avg_white[c] > WHITE_MAX)
		    {
		      exp_off =
			(avg_white[c] -
			 (WHITE_MAX + WHITE_MIN) / 2) / EXPOSURE_STEP;
		      if (exp_off < 1)
			exp_off = 1;
		      s->dev->exp_params.r_time -= exp_off;
		      finish = 0;
		      XDBG ((1, "adjust exposure r: --\n"));
		    }
		}
	      else if (c == 1)
		{
		  if (avg_white[c] < WHITE_MIN)
		    {
		      exp_off =
			((WHITE_MAX + WHITE_MIN) / 2 -
			 avg_white[c]) / EXPOSURE_STEP;
		      if (exp_off < 1)
			exp_off = 1;
		      s->dev->exp_params.g_time += exp_off;
		      finish = 0;
		      XDBG ((1, "adjust exposure g: ++\n"));
		    }
		  else if (avg_white[c] > WHITE_MAX)
		    {
		      exp_off =
			(avg_white[c] -
			 (WHITE_MAX + WHITE_MIN) / 2) / EXPOSURE_STEP;
		      if (exp_off < 1)
			exp_off = 1;
		      s->dev->exp_params.g_time -= exp_off;
		      finish = 0;
		      XDBG ((1, "adjust exposure g: --\n"));
		    }
		}
	      else if (c == 2)
		{
		  if (avg_white[c] < WHITE_MIN)
		    {
		      exp_off =
			((WHITE_MAX + WHITE_MIN) / 2 -
			 avg_white[c]) / EXPOSURE_STEP;
		      if (exp_off < 1)
			exp_off = 1;
		      s->dev->exp_params.b_time += exp_off;
		      finish = 0;
		      XDBG ((1, "adjust exposure b: ++\n"));
		    }
		  else if (avg_white[c] > WHITE_MAX)
		    {
		      exp_off =
			(avg_white[c] -
			 (WHITE_MAX + WHITE_MIN) / 2) / EXPOSURE_STEP;
		      if (exp_off < 1)
			exp_off = 1;
		      s->dev->exp_params.b_time -= exp_off;
		      finish = 0;
		      XDBG ((1, "adjust exposure b: --\n"));
		    }
		}
	    }

	  XDBG ((1, "time_r: %x, time_g: %x, time_b: %x\n",
	       s->dev->exp_params.r_time, s->dev->exp_params.g_time,
	       s->dev->exp_params.b_time));
	  XDBG ((1, "offset_r: %x, offset_g: %x, offset_b: %x\n",
	       s->dev->afe_params.r_offset, s->dev->afe_params.g_offset,
	       s->dev->afe_params.b_offset));
	  ++noloop;
	  if (noloop > 10)
	    break;
	}
    }

  XDBG ((1, "option redOffset 0x%x\n", s->dev->afe_params.r_offset));
  XDBG ((1, "option greenOffset 0x%x\n", s->dev->afe_params.g_offset));
  XDBG ((1, "option blueOffset 0x%x\n", s->dev->afe_params.b_offset));
  XDBG ((1, "option redExposure 0x%x\n", s->dev->exp_params.r_time));
  XDBG ((1, "option greenExposure 0x%x\n", s->dev->exp_params.g_time));
  XDBG ((1, "option blueExposure 0x%x\n", s->dev->exp_params.b_time));

  s->dev->artec_48u_afe_params.r_offset = s->dev->afe_params.r_offset;
  s->dev->artec_48u_afe_params.g_offset = s->dev->afe_params.g_offset;
  s->dev->artec_48u_afe_params.b_offset = s->dev->afe_params.b_offset;
  /*don't forget the gain */
  s->dev->artec_48u_afe_params.r_pga = s->dev->afe_params.r_pga;
  s->dev->artec_48u_afe_params.g_pga = s->dev->afe_params.g_pga;
  s->dev->artec_48u_afe_params.b_pga = s->dev->afe_params.b_pga;

  s->dev->artec_48u_exposure_params.r_time = s->dev->exp_params.r_time;
  s->dev->artec_48u_exposure_params.g_time = s->dev->exp_params.g_time;
  s->dev->artec_48u_exposure_params.b_time = s->dev->exp_params.b_time;

  /*******************************
   *get the black shading values *
   *******************************/
  artec48u_carriage_home (s->dev);

  artec48u_wait_for_positioning (s->dev);
  s->reader = NULL;

  s->scanning = SANE_TRUE;

  init_shading_buffer (s);

  artec48u_setup_scan (s, &(s->request), SA_CALIBRATE_SCAN_BLACK, SANE_FALSE,
		       &(s->params));
  artec48u_scanner_start_scan_extended (s, &(s->request),
					SA_CALIBRATE_SCAN_BLACK,
					&(s->params));

  for (c = 0; c < s->dev->shading_lines_b; c++)
    {
      artec48u_scanner_read_line (s, buffer_pointers, SANE_FALSE);
      add_to_shading_buffer (s, buffer_pointers);
    }
  artec48u_scanner_stop_scan (s);
  finish_shading_buffer (s, SANE_FALSE);
  s->scanning = SANE_FALSE;

  /*******************************
   *get the white shading values *
   *******************************/
  artec48u_carriage_home (s->dev);

  artec48u_wait_for_positioning (s->dev);
  s->reader = NULL;
  s->scanning = SANE_TRUE;

  init_shading_buffer (s);

  artec48u_setup_scan (s, &(s->request), SA_CALIBRATE_SCAN_WHITE, SANE_FALSE,
		       &(s->params));
  artec48u_scanner_start_scan_extended (s, &(s->request),
					SA_CALIBRATE_SCAN_WHITE,
					&(s->params));
  for (c = 0; c < s->dev->shading_lines_w; c++)
    {
      artec48u_scanner_read_line (s, buffer_pointers, SANE_FALSE);
      add_to_shading_buffer (s, buffer_pointers);
    }
  artec48u_scanner_stop_scan (s);
  finish_shading_buffer (s, SANE_TRUE);
  s->scanning = SANE_FALSE;
  save_calibration_data (s);
  return SANE_STATUS_GOOD;
}

static SANE_Status
close_pipe (Artec48U_Scanner * s)
{
  if (s->pipe >= 0)
    {
      XDBG ((1, "close_pipe\n"));
      close (s->pipe);
      s->pipe = -1;
    }
  return SANE_STATUS_EOF;
}
static RETSIGTYPE
sigalarm_handler (int signal)
{
  int dummy;			/*Henning doesn't like warnings :-) */
  XDBG ((1, "ALARM!!!\n"));
  dummy = signal;
  cancelRead = SANE_TRUE;
}

static void
sig_chldhandler (int signo)
{
  XDBG ((1, "Child is down (signal=%d)\n", signo));
}

static int
reader_process (void * data)
{
  Artec48U_Scanner * s = (Artec48U_Scanner *) data;
  int fd = s->reader_pipe;

  SANE_Status status;
  struct SIGACTION act;
  sigset_t ignore_set;
  ssize_t bytes_written = 0;

  XDBG ((1, "reader process...\n"));

  if (sanei_thread_is_forked()) close (s->pipe);

  sigfillset (&ignore_set);
  sigdelset (&ignore_set, SIGTERM);
  sigdelset (&ignore_set, SIGUSR1);
#if defined (__APPLE__) && defined (__MACH__)
  sigdelset (&ignore_set, SIGUSR2);
#endif
  sigprocmask (SIG_SETMASK, &ignore_set, 0);

  memset (&act, 0, sizeof (act));
  sigaction (SIGTERM, &act, 0);
  sigaction (SIGUSR1, &act, 0);

  cancelRead = SANE_FALSE;
  if (sigemptyset (&(act.sa_mask)) < 0)
    XDBG ((2, "(child) reader_process: sigemptyset() failed\n"));
  act.sa_flags = 0;

  act.sa_handler = reader_process_sigterm_handler;
  if (sigaction (SIGTERM, &act, 0) < 0)
    XDBG ((2, "(child) reader_process: sigaction(SIGTERM,...) failed\n"));

  act.sa_handler = usb_reader_process_sigterm_handler;
  if (sigaction (SIGUSR1, &act, 0) < 0)
    XDBG ((2, "(child) reader_process: sigaction(SIGUSR1,...) failed\n"));


  XDBG ((2, "(child) reader_process: s=%p, fd=%d\n", (void *) s, fd));

  /*read line by line into buffer */
  /*copy buffer pointers to line_buffer */
  XDBG ((2, "(child) reader_process: byte_cnt %d\n", (int) s->byte_cnt));
  s->eof = SANE_FALSE;
  while (s->lines_to_read > 0)
    {
      if (cancelRead == SANE_TRUE)
	{
	  XDBG ((2, "(child) reader_process: cancelRead == SANE_TRUE\n"));
	  s->scanning = SANE_FALSE;
	  s->eof = SANE_FALSE;
	  return SANE_STATUS_CANCELLED;
	}
      if (s->scanning != SANE_TRUE)
	{
	  XDBG ((2, "(child) reader_process: scanning != SANE_TRUE\n"));
	  return SANE_STATUS_CANCELLED;
	}
      status = artec48u_scanner_read_line (s, s->buffer_pointers, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	{
	  XDBG ((2, "(child) reader_process: scanner_read_line failed\n"));
	  return SANE_STATUS_IO_ERROR;
	}
      copy_scan_line (s);
      s->lines_to_read -= 1;
      bytes_written =
	write (fd, s->line_buffer, s->sane_params.bytes_per_line);

      if (bytes_written < 0)
	{
	  XDBG ((2, "(child) reader_process: write returned %s\n",
	       strerror (errno)));
	  s->eof = SANE_FALSE;
	  return SANE_STATUS_IO_ERROR;
	}

      XDBG ((2, "(child) reader_process: lines to read %i\n", s->lines_to_read));
    }
  s->eof = SANE_TRUE;
  close (fd);
  return SANE_STATUS_GOOD;
}

static SANE_Status
do_cancel (Artec48U_Scanner * s, SANE_Bool closepipe)
{
  struct SIGACTION act;
  SANE_Pid res;
  XDBG ((1, "do_cancel\n"));

  s->scanning = SANE_FALSE;

  if (s->reader_pid != -1)
    {
      /*parent */
      XDBG ((1, "killing reader_process\n"));
      /* tell the driver to stop scanning */
      sigemptyset (&(act.sa_mask));
      act.sa_flags = 0;

      act.sa_handler = sigalarm_handler;

      if (sigaction (SIGALRM, &act, 0) == -1)
	XDBG ((1, "sigaction() failed !\n"));

      /* kill our child process and wait until done */
      alarm (10);
      if (sanei_thread_kill (s->reader_pid) < 0)
	XDBG ((1, "sanei_thread_kill() failed !\n"));
      res = sanei_thread_waitpid (s->reader_pid, 0);
      alarm (0);

      if (res != s->reader_pid)
	{
	  XDBG ((1, "sanei_thread_waitpid() failed !\n"));
	}
      s->reader_pid = -1;
      XDBG ((1, "reader_process killed\n"));
    }
  if (SANE_TRUE == closepipe)
    {
      close_pipe (s);
      XDBG ((1, "pipe closed\n"));
    }
  artec48u_scanner_stop_scan (s);
  artec48u_carriage_home (s->dev);
  if (s->line_buffer)
    {
      XDBG ((2, "freeing line_buffer\n"));
      free (s->line_buffer);
      s->line_buffer = NULL;
    }
  if (s->lineart_buffer)
    {
      XDBG ((2, "freeing lineart_buffer\n"));
      free (s->lineart_buffer);
      s->lineart_buffer = NULL;
    }

  return SANE_STATUS_CANCELLED;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  Artec48U_Device *dev;
  SANE_Int dev_num;

  XDBG ((5, "sane_get_devices: start: local_only = %s\n",
       local_only == SANE_TRUE ? "true" : "false"));

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  dev_num = 0;
  for (dev = first_dev; dev_num < num_devices; dev = dev->next)
    {
      devlist[dev_num] = &dev->sane;
      XDBG ((3, "sane_get_devices: name %s\n", dev->sane.name));
      XDBG ((3, "sane_get_devices: vendor %s\n", dev->sane.vendor));
      XDBG ((3, "sane_get_devices: model %s\n", dev->sane.model));
      ++dev_num;
    }
  devlist[dev_num] = 0;
  ++dev_num;

  *device_list = devlist;

  XDBG ((5, "sane_get_devices: exit\n"));

  return SANE_STATUS_GOOD;
}

static SANE_Status
load_calibration_data (Artec48U_Scanner * s)
{
  SANE_Status status = SANE_STATUS_GOOD;
  FILE *f = 0;
  size_t cnt;
  char path[PATH_MAX];
  char filename[PATH_MAX];

  s->calibrated = SANE_FALSE;
  path[0] = 0;

  /* return SANE_STATUS_INVAL if HOME environment variable is not set */
  if (getenv ("HOME") == NULL)
  {
    XDBG ((1, "Environment variable HOME not set\n"));
    return SANE_STATUS_INVAL;
  }

  if (strlen (getenv ("HOME")) < (PATH_MAX - 1))
    strcat (path, getenv ("HOME"));
  else
    return SANE_STATUS_INVAL;

  if (strlen (path) < (PATH_MAX - 1 - strlen ("/.artec_eplus48u/")))
    strcat (path, "/.artec_eplus48u/");
  else
    return SANE_STATUS_INVAL;

  /*try to load black shading file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48ushading_black")))
    strcat (filename, "artec48ushading_black");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to read black shading file: \"%s\"\n", filename));

  f = fopen (filename, "rb");
  if (!f)
    return SANE_STATUS_INVAL;

  /*read values */
  cnt = fread (s->shading_buffer_b, sizeof (unsigned char), 30720*s->dev->epro_mult, f); /*epro*/
  if (cnt != (30720*s->dev->epro_mult)) /*epro*/
    {
      fclose (f);
      XDBG ((1, "Could not load black shading file\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);

  /*try to load white shading file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48ushading_white")))
    strcat (filename, "artec48ushading_white");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to read white shading file: \"%s\"\n", filename));
  f = fopen (filename, "rb");
  if (!f)
    return SANE_STATUS_INVAL;
  /*read values */
  cnt = fread (s->shading_buffer_w, sizeof (unsigned char), 30720*s->dev->epro_mult, f);/*epro*/
  if (cnt != (30720*s->dev->epro_mult)) /*epro*/
    {
      fclose (f);
      XDBG ((1, "Could not load white shading file\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);

  /*try to load offset file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48uoffset")))
    strcat (filename, "artec48uoffset");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to read offset file: \"%s\"\n", filename));
  f = fopen (filename, "rb");
  if (!f)
    return SANE_STATUS_INVAL;
  /*read values */
  cnt =
    fread (&s->dev->artec_48u_afe_params, sizeof (Artec48U_AFE_Parameters), 1,
	   f);
  if (cnt != 1)
    {
      fclose (f);
      XDBG ((1, "Could not load offset file\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);

  /*load exposure file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48uexposure")))
    strcat (filename, "artec48uexposure");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to read exposure file: \"%s\"\n", filename));
  f = fopen (filename, "rb");
  if (!f)
    return SANE_STATUS_INVAL;
  /*read values */
  cnt =
    fread (&s->dev->artec_48u_exposure_params,
	   sizeof (Artec48U_Exposure_Parameters), 1, f);
  if (cnt != 1)
    {
      fclose (f);
      XDBG ((1, "Could not load exposure file\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);
  s->calibrated = SANE_TRUE;
  return status;
}

static SANE_Status
save_calibration_data (Artec48U_Scanner * s)
{
  SANE_Status status = SANE_STATUS_GOOD;
  FILE *f = 0;
  size_t cnt;
  char path[PATH_MAX];
  char filename[PATH_MAX];
  mode_t mode = S_IRUSR | S_IWUSR;

  path[0] = 0;

  /* return SANE_STATUS_INVAL if HOME environment variable is not set */
  if (getenv ("HOME") == NULL)
  {
    XDBG ((1, "Environment variable HOME not set\n"));
    return SANE_STATUS_INVAL;
  }

  if (strlen (getenv ("HOME")) < (PATH_MAX - 1))
    strcat (path, getenv ("HOME"));
  else
    return SANE_STATUS_INVAL;

  if (strlen (path) < (PATH_MAX - 1 - strlen ("/.artec_eplus48u/")))
    strcat (path, "/.artec_eplus48u/");
  else
    return SANE_STATUS_INVAL;

  /*try to save black shading file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48ushading_black")))
    strcat (filename, "artec48ushading_black");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to save black shading file: \"%s\"\n", filename));
  f = fopen (filename, "w");
  if (!f)
    {
      XDBG ((1, "Could not save artec48ushading_black\n"));
      return SANE_STATUS_INVAL;
    }
  if (chmod (filename, mode) != 0)
    return SANE_STATUS_INVAL;

  /*read values */
  cnt = fwrite (s->shading_buffer_b, sizeof (unsigned char), 30720*s->dev->epro_mult, f); /*epro*/
  XDBG ((1, "Wrote %li bytes to black shading buffer \n", (u_long) cnt));
  if (cnt != (30720*s->dev->epro_mult))/*epro*/
    {
      fclose (f);
      XDBG ((1, "Could not write black shading buffer\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);

  /*try to save white shading file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48ushading_white")))
    strcat (filename, "artec48ushading_white");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to save white shading file: \"%s\"\n", filename));
  f = fopen (filename, "w");
  if (!f)
    return SANE_STATUS_INVAL;
  if (chmod (filename, mode) != 0)
    return SANE_STATUS_INVAL;
  /*read values */
  cnt = fwrite (s->shading_buffer_w, sizeof (unsigned char), 30720*s->dev->epro_mult, f);/*epro*/
  if (cnt != (30720*s->dev->epro_mult)) /*epro*/
    {
      fclose (f);
      XDBG ((1, "Could not write white shading buffer\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);

  /*try to save offset file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48uoffset")))
    strcat (filename, "artec48uoffset");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to write offset file: \"%s\"\n", filename));
  f = fopen (filename, "w");
  if (!f)
    return SANE_STATUS_INVAL;
  if (chmod (filename, mode) != 0)
    return SANE_STATUS_INVAL;
  /*read values */
  cnt =
    fwrite (&s->dev->artec_48u_afe_params, sizeof (Artec48U_AFE_Parameters),
	    1, f);
  if (cnt != 1)
    {
      fclose (f);
      XDBG ((1, "Could not write afe values\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);

  /*try to write exposure file */
  strcpy (filename, path);
  if (strlen (filename) < (PATH_MAX - 1 - strlen ("artec48uexposure")))
    strcat (filename, "artec48uexposure");
  else
    return SANE_STATUS_INVAL;
  XDBG ((1, "Try to write exposure file: \"%s\"\n", filename));
  f = fopen (filename, "w");
  if (!f)
    return SANE_STATUS_INVAL;
  if (chmod (filename, mode) != 0)
    return SANE_STATUS_INVAL;
  /*read values */
  cnt =
    fwrite (&s->dev->artec_48u_exposure_params,
	    sizeof (Artec48U_Exposure_Parameters), 1, f);
  if (cnt != 1)
    {
      fclose (f);
      XDBG ((1, "Could not write exposure values\n"));
      return SANE_STATUS_INVAL;
    }
  fclose (f);
  return status;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  SANE_Status status = SANE_STATUS_INVAL;
  Artec48U_Device *dev = 0;
  Artec48U_Scanner *s = 0;

  if (!devicename)
    return SANE_STATUS_INVAL;
  XDBG ((2, "sane_open: devicename = \"%s\"\n", devicename));


  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devicename) == 0)
	    {
	      XDBG ((2, "sane_open: found matching device %s\n",
		   dev->sane.name));
	      break;
	    }
	}
      if (!dev)
	{
	  status = attach (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    XDBG ((2, "sane_open: attach failed %s\n", devicename));
	}
    }
  else
    {
      /* empty devicename -> use first device */
      XDBG ((2, "sane_open: empty devicename\n"));
      dev = first_dev;
    }
  if (!dev)
    return SANE_STATUS_INVAL;

  status = artec48u_device_open (dev);

  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "could not open device\n"));
      return status;
    }
  XDBG ((2, "sane_open: opening device `%s', handle = %p\n", dev->sane.name,
       (void *) dev));

  XDBG ((1, "sane_open - %s\n", dev->sane.name));
  XDBG ((2, "sane_open: try to open %s\n", dev->sane.name));

  status = artec48u_device_activate (dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "could not activate device\n"));
      return status;
    }
  /* We do not check anymore, whether the firmware is already loaded */
  /* because that caused problems after rebooting; furthermore, loading */
  /* of the firmware is fast, therefore the test doesn't make much sense */
  status = download_firmware_file (dev);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((3, "download_firmware_file failed\n"));
      return status;
    }
  /* If a scan is interrupted without sending stop_scan, bad things happen.
   * Send the stop scan command now just in case. */
  artec48u_stop_scan (dev);

  artec48u_wait_for_positioning (dev);

  artec48u_scanner_new (dev, &s);
  init_calibrator (s);
  s->next = first_handle;
  first_handle = s;
  *handle = s;

  status = init_options (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  /*Try to load the calibration values */
  status = load_calibration_data (s);

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Artec48U_Scanner *prev, *s;

  XDBG ((5, "sane_close: start\n"));

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
      XDBG ((5, "close: invalid handle %p\n", handle));
      return;
    }
  artec48u_device_close (s->dev);
  artec48u_scanner_free (s);
  XDBG ((5, "sane_close: exit\n"));
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Artec48U_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  XDBG ((5, "sane_get_option_descriptor: option = %s (%d)\n",
       s->opt[option].name, option));
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value, SANE_Int * info)
{
  Artec48U_Scanner *s = handle;
#ifdef ARTEC48U_USE_BUTTONS
  SANE_Int button_state;
#endif
  SANE_Status status;
  XDBG ((8, "sane_control_option: handle=%p, opt=%d, act=%d, val=%p, info=%p\n",
       (void *) handle, option, action, (void *) value, (void *) info));

  if (info)
    *info = 0;

  if (option < 0 || option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;	/* Unknown option ... */

  if (!SANE_OPTION_IS_ACTIVE (s->opt[option].cap))
    return SANE_STATUS_INVAL;

  switch (action)
    {
    case SANE_ACTION_SET_VALUE:
      if (s->scanning == SANE_TRUE)
	return SANE_STATUS_INVAL;

      if (!SANE_OPTION_IS_SETTABLE (s->opt[option].cap))
	return SANE_STATUS_INVAL;

      status = sanei_constrain_value (s->opt + option, value, info);

      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	case OPT_RESOLUTION:
          if(s->dev->is_epro != 0)
	  {
            if((s->val[option].w == 1200) && (*(SANE_Word *) value < 1200))
            {
              s->opt[OPT_BIT_DEPTH].constraint.word_list = bitdepth_list;
	      *info |= SANE_INFO_RELOAD_OPTIONS;
            }
            else if((s->val[option].w < 1200) && (*(SANE_Word *) value == 1200))
            {
              s->opt[OPT_BIT_DEPTH].constraint.word_list = bitdepth_list2;
              if(s->val[OPT_BIT_DEPTH].w > 8)
                s->val[OPT_BIT_DEPTH].w = 8;
	      *info |= SANE_INFO_RELOAD_OPTIONS;
            }
	  }
	  s->val[option].w = *(SANE_Word *) value;
	  if (info)
	  {
            *info |= SANE_INFO_RELOAD_PARAMS;
          }
	  break;
        /* fall through */
	case OPT_BIT_DEPTH:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  s->val[option].w = *(SANE_Word *) value;
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	  /* fall through */
	case OPT_BLACK_LEVEL:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_GAMMA:
	case OPT_GAMMA_R:
	case OPT_GAMMA_G:
	case OPT_GAMMA_B:
	case OPT_CALIBRATE:
	case OPT_CALIBRATE_SHADING:
	  s->val[option].w = *(SANE_Word *) value;
	  return SANE_STATUS_GOOD;
	case OPT_DEFAULT_ENHANCEMENTS:
	  s->val[OPT_GAMMA].w = SANE_FIX (s->dev->gamma_master);
	  if (strcmp (s->val[OPT_SCAN_MODE].s, mode_list[2]) == 0)
	    {
	      s->val[OPT_GAMMA_R].w = SANE_FIX (s->dev->gamma_r);
	      s->val[OPT_GAMMA_G].w = SANE_FIX (s->dev->gamma_g);
	      s->val[OPT_GAMMA_B].w = SANE_FIX (s->dev->gamma_b);
	    }
	  s->val[OPT_BRIGHTNESS].w = 0;
	  s->val[OPT_CONTRAST].w = 0;
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case OPT_SCAN_MODE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (value);
	  if (strcmp (s->val[OPT_SCAN_MODE].s, mode_list[0]) == 0)
	    {
	      s->opt[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_BLACK_LEVEL].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
	    }
	  else if (strcmp (s->val[OPT_SCAN_MODE].s, mode_list[1]) == 0)
	    {
	      s->opt[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_BLACK_LEVEL].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_BIT_DEPTH].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[OPT_GAMMA_R].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_G].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_B].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_BLACK_LEVEL].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_BIT_DEPTH].cap &= ~SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	}
      break;
    case SANE_ACTION_GET_VALUE:
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_BIT_DEPTH:
	case OPT_BLACK_LEVEL:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_GAMMA:
	case OPT_GAMMA_R:
	case OPT_GAMMA_G:
	case OPT_GAMMA_B:
	case OPT_CALIBRATE:
	case OPT_CALIBRATE_SHADING:
	  *(SANE_Word *) value = (SANE_Word) s->val[option].w;
	  return SANE_STATUS_GOOD;
	  /* string options: */
	case OPT_SCAN_MODE:
	  strcpy (value, s->val[option].s);
	  return SANE_STATUS_GOOD;
#ifdef ARTEC48U_USE_BUTTONS
	case OPT_BUTTON_STATE:
	  status = artec48u_check_buttons (s->dev, &button_state);
	  if (status == SANE_STATUS_GOOD)
	    {
	      s->val[option].w = button_state;
	      *(SANE_Int *) value = (SANE_Int) s->val[option].w;
	    }
	  else
	    {
	      s->val[option].w = 0;
	      *(SANE_Int *) value = 0;
	    }
	  return SANE_STATUS_GOOD;
#endif
	}
      break;
    default:
      return SANE_STATUS_INVAL;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Artec48U_Scanner *s = handle;
  SANE_Status status;
  SANE_Word resx;
/*  int scan_mode;*/
  SANE_String str = s->val[OPT_SCAN_MODE].s;
  int tlx;
  int tly;
  int brx;
  int bry;
  int tmp;
  XDBG ((2, "sane_get_params: string %s\n", str));
  XDBG ((2, "sane_get_params: enter\n"));

  tlx = s->val[OPT_TL_X].w;
  tly = s->val[OPT_TL_Y].w;
  brx = s->val[OPT_BR_X].w;
  bry = s->val[OPT_BR_Y].w;

  /*make sure, that tlx < brx and tly < bry
     this will NOT change the options */
  if (tlx > brx)
    {
      tmp = tlx;
      tlx = brx;
      brx = tmp;
    }
  if (tly > bry)
    {
      tmp = tly;
      tly = bry;
      bry = tmp;
    }
  resx = s->val[OPT_RESOLUTION].w;
  str = s->val[OPT_SCAN_MODE].s;

  s->request.color = SANE_TRUE;
  if ((strcmp (str, mode_list[0]) == 0) || (strcmp (str, mode_list[1]) == 0))
    s->request.color = SANE_FALSE;
  else
    s->request.color = SANE_TRUE;
  s->request.depth = s->val[OPT_BIT_DEPTH].w;
  if (strcmp (str, mode_list[0]) == 0)
    s->request.depth = 8;
  s->request.y0 = tly;	      /**< Top boundary */
  s->request.x0 = SANE_FIX (216.0) - brx;	/**< left boundary */
  s->request.xs = brx - tlx;	    /**< Width */
  s->request.ys = bry - tly;	    /**< Height */
  s->request.xdpi = resx;      /**< Horizontal resolution */
  s->request.ydpi = resx;      /**< Vertical resolution */
  /*epro*/
  if ((resx == 1200) && (s->dev->is_epro == 0))
    s->request.xdpi = 600;/**< Vertical resolution */

  status = artec48u_setup_scan (s, &(s->request), SA_SCAN,
				SANE_TRUE, &(s->params));
  if (status != SANE_STATUS_GOOD)
    return SANE_STATUS_INVAL;

/*DBG(1, "sane_get_params: scan_mode %i\n",scan_mode);*/

  params->depth = s->params.depth;
  s->params.lineart = SANE_FALSE;
  if (s->params.color == SANE_TRUE)
    {
      params->format = SANE_FRAME_RGB;
      params->bytes_per_line = s->params.pixel_xs * 3;
    }
  else
    {
      params->format = SANE_FRAME_GRAY;
      params->bytes_per_line = s->params.pixel_xs;
      if (strcmp (str, mode_list[0]) == 0)
	{
	  params->depth = 1;
	  params->bytes_per_line = (s->params.pixel_xs + 7) / 8;
	  s->params.lineart = SANE_TRUE;
	}
    }
  if ((resx == 1200) && (s->dev->is_epro == 0))
    {
      if (params->depth == 1)
	params->bytes_per_line = (s->params.pixel_xs * 2 + 7) / 8;
      else
	params->bytes_per_line *= 2;
    }
  if (params->depth == 16)
    params->bytes_per_line *= 2;
  params->last_frame = SANE_TRUE;
  params->pixels_per_line = s->params.pixel_xs;
  if ((resx == 1200) && (s->dev->is_epro == 0))
    params->pixels_per_line *= 2;
  params->lines = s->params.pixel_ys;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Artec48U_Scanner *s = handle;
  SANE_Status status;
  int fds[2];

  if (s->scanning)
    {
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (sane_get_parameters (handle, &s->sane_params) != SANE_STATUS_GOOD)
    return SANE_STATUS_INVAL;

  if ((s->calibrated != SANE_TRUE) || (s->val[OPT_CALIBRATE].w == SANE_TRUE))
    {
      XDBG ((1, "Must calibrate scanner\n"));
      status = calibrate_scanner (s);
      if (status != SANE_STATUS_GOOD)
	return status;
      s->calibrated = SANE_TRUE;
    }
  if (sane_get_parameters (handle, &s->sane_params) != SANE_STATUS_GOOD)
    return SANE_STATUS_INVAL;

  calculate_brightness (s);
  calculate_contrast (s);
  calculateGamma (s);
  calculateGammaRed (s);
  calculateGammaGreen (s);
  calculateGammaBlue (s);

  artec48u_carriage_home (s->dev);

  artec48u_wait_for_positioning (s->dev);
  s->reader = NULL;

  s->scanning = SANE_TRUE;
  s->byte_cnt = 0;
  s->lines_to_read = s->params.pixel_ys;
  /*allocate a buffer, that can hold a complete scan line */
  /*If resolution is 1200 dpi and we are scanning in lineart mode,
     then we also allocate a lineart_buffer, which can hold a complete scan line
     in 8 bit/gray. This makes interpolation easier. */
  if ((s->params.ydpi == 1200) && (s->dev->is_epro == 0))
    {
      if (s->request.color == SANE_TRUE)
	{
	  s->line_buffer = (SANE_Byte *) malloc (s->params.scan_bpl * 8);
	}
      else
	{
	  s->line_buffer = (SANE_Byte *) malloc (s->params.scan_bpl * 4);
	  /*lineart ? */
	  if (strcmp (s->val[OPT_SCAN_MODE].s, mode_list[0]) == 0)
	    s->lineart_buffer = (SANE_Byte *) malloc (s->params.pixel_xs * 2);
	}
    }
  else
    {
      if (s->request.color == SANE_TRUE)
	s->line_buffer = (SANE_Byte *) malloc (s->params.scan_bpl * 4);
      else
	{
	  s->line_buffer = (SANE_Byte *) malloc (s->params.scan_bpl * 2);
	  /*lineart ? */
	  if (strcmp (s->val[OPT_SCAN_MODE].s, mode_list[0]) == 0)
	    s->lineart_buffer = (SANE_Byte *) malloc (s->params.pixel_xs * 2);
	}
    }
  if (pipe (fds) < 0)
    {
      s->scanning = SANE_FALSE;
      XDBG ((2, "sane_start: pipe failed (%s)\n", strerror (errno)));
      return SANE_STATUS_IO_ERROR;
    }
  status = artec48u_scanner_start_scan (s, &s->request, &s->params);
  if (status != SANE_STATUS_GOOD)
    {
      XDBG ((2, "sane_start: could not start scan\n"));
      return status;
    }
  s->pipe = fds[0];
  s->reader_pipe = fds[1];
  s->reader_pid = sanei_thread_begin (reader_process, s);
  cancelRead = SANE_FALSE;
  if (s->reader_pid == -1)
    {
      s->scanning = SANE_FALSE;
      XDBG ((2, "sane_start: sanei_thread_begin failed (%s)\n", strerror (errno)));
      return SANE_STATUS_NO_MEM;
    }
  signal (SIGCHLD, sig_chldhandler);

  if (sanei_thread_is_forked()) close (s->reader_pipe);

  XDBG ((1, "sane_start done\n"));

  return SANE_STATUS_GOOD;	/* parent */
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  Artec48U_Scanner *s = handle;
  ssize_t nread;

  *length = 0;

  /* here we read all data from the driver... */
  nread = read (s->pipe, data, max_length);
  XDBG ((3, "sane_read - read %ld bytes\n", (long) nread));
  if (cancelRead == SANE_TRUE)
    {
      return do_cancel (s, SANE_TRUE);
    }

  if (nread < 0)
    {
      if (EAGAIN == errno)
	{
	  /* if we already had read the picture, so it's okay and stop */
	  if (s->eof == SANE_TRUE)
	    {
	      sanei_thread_waitpid (s->reader_pid, 0);
	      s->reader_pid = -1;
	      artec48u_scanner_stop_scan (s);
	      artec48u_carriage_home (s->dev);
	      return close_pipe (s);
	    }
	  /* else force the frontend to try again */
	  return SANE_STATUS_GOOD;
	}
      else
	{
	  XDBG ((4, "ERROR: errno=%d\n", errno));
	  do_cancel (s, SANE_TRUE);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  *length = nread;
  s->byte_cnt += nread;

  /* nothing read means that we're finished OR we had a problem... */
  if (0 == nread)
    {
      if (0 == s->byte_cnt)
	{
	  s->exit_code = sanei_thread_get_status (s->reader_pid);

	  if (SANE_STATUS_GOOD != s->exit_code)
	    {
	      close_pipe (s);
	      return s->exit_code;
	    }
	}
      return close_pipe (s);
    }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Artec48U_Scanner *s = handle;
  XDBG ((2, "sane_cancel: handle = %p\n", handle));
  if (s->scanning)
    do_cancel (s, SANE_FALSE);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Artec48U_Scanner *s = (Artec48U_Scanner *) handle;

  XDBG ((1, "sane_set_io_mode: non_blocking=%d\n", non_blocking));

  if (!s->scanning)
    {
      XDBG ((4, "ERROR: not scanning !\n"));
      return SANE_STATUS_INVAL;
    }

  if (-1 == s->pipe)
    {
      XDBG ((4, "ERROR: not supported !\n"));
      return SANE_STATUS_UNSUPPORTED;
    }

  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    {
      XDBG ((4, "ERROR: can?t set to non-blocking mode !\n"));
      return SANE_STATUS_IO_ERROR;
    }

  XDBG ((1, "sane_set_io_mode done\n"));
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Artec48U_Scanner *s = (Artec48U_Scanner *) handle;

  XDBG ((1, "sane_get_select_fd\n"));

  if (!s->scanning)
    {
      XDBG ((4, "ERROR: not scanning !\n"));
      return SANE_STATUS_INVAL;
    }

  *fd = s->pipe;

  XDBG ((1, "sane_get_select_fd done\n"));
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  Artec48U_Device *device = 0;
  SANE_Status status;
  char str[PATH_MAX] = _DEFAULT_DEVICE;
  char temp[PATH_MAX];
  size_t len;
  FILE *fp;
  double gamma_m = 1.9;
  double gamma_r = 1.0;
  double gamma_g = 1.0;
  double gamma_b = 1.0;
  int epro_default = 0;

  DBG_INIT ();
  eProMult = 1;
  isEPro = 0;
  temp[0] = 0;
  strcpy (vendor_string, "Artec");
  strcpy (model_string, "E+ 48U");

  sanei_usb_init ();
  sanei_thread_init ();

  /* do some presettings... */
  auth = authorize;

  if (version_code != NULL)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (ARTEC48U_CONFIG_FILE);

  /* default to _DEFAULT_DEVICE instead of insisting on config file */
  if (NULL == fp)
    {
      status = attach (_DEFAULT_DEVICE, &device);
      return status;
    }

  while (sanei_config_read (str, sizeof (str), fp))
    {
      XDBG ((1, "sane_init, >%s<\n", str));
      /* ignore line comments */
      if (str[0] == '#')
	continue;
      len = strlen (str);
      /* ignore empty lines */
      if (0 == len)
	continue;
      /* check for options */
      if (0 == strncmp (str, "option", 6))
	{
	  if(decodeVal (str,"ePlusPro",_INT, &isEPro,&epro_default) == SANE_TRUE)
	  {
            eProMult = 1;
            if(isEPro != 0)
            {
              eProMult = 2;
              XDBG ((3, "Is Artec E Pro\n"));
            }
            else
              XDBG ((3, "Is Artec E+ 48U\n"));
          }
	  decodeVal (str, "masterGamma", _FLOAT, &gamma_master_default,
		     &gamma_m);
	  decodeVal (str, "redGamma", _FLOAT, &gamma_r_default, &gamma_r);
	  decodeVal (str, "greenGamma", _FLOAT, &gamma_g_default, &gamma_g);
	  decodeVal (str, "blueGamma", _FLOAT, &gamma_b_default, &gamma_b);
	  decodeVal (str, "redOffset", _BYTE, &afe_params.r_offset,
		     &default_afe_params.r_offset);
	  decodeVal (str, "greenOffset", _BYTE, &afe_params.g_offset,
		     &default_afe_params.g_offset);
	  decodeVal (str, "blueOffset", _BYTE, &afe_params.b_offset,
		     &default_afe_params.b_offset);

	  decodeVal (str, "redExposure", _INT, &exp_params.r_time,
		     &default_exp_params.r_time);
	  decodeVal (str, "greenExposure", _INT, &exp_params.g_time,
		     &default_exp_params.g_time);
	  decodeVal (str, "blueExposure", _INT, &exp_params.b_time,
		     &default_exp_params.b_time);

	  decodeVal (str, "modelString", _STRING, model_string, model_string);
	  decodeVal (str, "vendorString", _STRING, vendor_string,
		     vendor_string);

	  decodeVal (str, "artecFirmwareFile", _STRING, firmwarePath,
		     firmwarePath);
	}
      else if (0 == strncmp (str, "usb", 3))
	{
	  if (temp[0] != 0)
	    {
	      XDBG ((3, "trying to attach: %s\n", temp));
	      XDBG ((3, "      vendor: %s\n", vendor_string));
	      XDBG ((3, "      model: %s\n", model_string));
	      sanei_usb_attach_matching_devices (temp, attach_one_device);
	    }
	  /*save config line in temp */
	  strcpy (temp, str);
	}
      else if (0 == strncmp (str, "device", 6))
	{
	  if (SANE_TRUE == decodeDevName (str, devName))
	    {
	      if (devName[0] != 0)
		sanei_usb_attach_matching_devices (devName,
						   attach_one_device);
	      temp[0] = 0;
	    }
	}
      else
	{
	  /* ignore other stuff... */
	  XDBG ((1, "ignoring >%s<\n", str));
	}
    }
  if (temp[0] != 0)
    {
      XDBG ((3, "trying to attach: %s\n", temp));
      XDBG ((3, "      vendor: %s\n", vendor_string));
      XDBG ((3, "      model: %s\n", model_string));
      sanei_usb_attach_matching_devices (temp, attach_one_device);
      temp[0] = 0;
    }

  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Artec48U_Device *dev, *next;

  XDBG ((5, "sane_exit: start\n"));
  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      /*function will check, whether device is really open */
      artec48u_device_close (dev);
      artec48u_device_free (dev);
    }
  XDBG ((5, "sane_exit: exit\n"));
  return;
}
