/* sane - Scanner Access Now Easy.

   This file (C) 1997 Ingo Schneider
             (C) 1998 Karl Anders Øygard

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This file implements a SANE backend for AGFA Focus flatbed scanners.  */

#include "../include/sane/config.h"

#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_thread.h"

#define BACKEND_NAME	agfafocus
#include "../include/sane/sanei_backend.h"

#include "agfafocus.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif


#undef Byte
#define Byte SANE_Byte

static const SANE_Device **devlist = 0;
static int num_devices;
static AgfaFocus_Device *agfafocus_devices;

static const SANE_String_Const focus_mode_list[] =
{
  "Lineart", "Gray (6 bit)",
  0
};

static const SANE_String_Const focusii_mode_list[] =
{
  "Lineart", "Gray (6 bit)", "Gray (8 bit)",
  0
};

static const SANE_String_Const focuscolor_mode_list[] =
{
  "Lineart", "Gray (6 bit)", "Gray (8 bit)", "Color (18 bit)", "Color (24 bit)",
  0
};

static const SANE_String_Const halftone_list[] =
{
  "None", "Dispersed dot 4x4", "Round (Clustered dot 4x4)", "Diamond (Clustered dot 4x4)",
  0
};

static const SANE_String_Const halftone_upload_list[] =
{
  "None", "Dispersed dot 4x4", "Round (Clustered dot 4x4)", "Diamond (Clustered dot 4x4)",
  0
};

static const SANE_String_Const source_list[] =
{
  "Opaque/Normal", "Transparency",
  0
};

static const SANE_String_Const quality_list[] =
{
  "Low", "Normal", "High",
  0
};

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

/* sets loc_s bytes long value at offset loc in scsi command to value size  */
static void 
set_size (Byte * loc, int loc_s, size_t size)
{
  int i;
  
  for (i = 0; i < loc_s; i++)
    {
      loc[loc_s - i - 1] = (size >> (i * 8)) & 0xff;
    }
}

/* gets loc_s bytes long value from loc in scsi command */
static int
get_size (Byte * loc, int loc_s)
{
  int i;
  int j = 0;

  for (i = 0; i < loc_s; i++)
    {
      j = (j << 8) + (loc[i] & 0xff);
    }

  return j;
}

static long 
reserve_unit (int fd)
{
  struct 
  {
    /* Command */
    Byte cmd;
    Byte lun;
    Byte res[2];
    Byte tr_len;
    Byte ctrl;
  }
  scsi_reserve;

  memset (&scsi_reserve, 0, sizeof (scsi_reserve));

  scsi_reserve.cmd = 0x16; /* RELEASE */

  DBG (3, "reserve_unit()\n");
  return sanei_scsi_cmd (fd, &scsi_reserve, sizeof (scsi_reserve), 0, 0);
}

static long 
release_unit (int fd)
{
  struct 
  {
    /* Command */
    Byte cmd;
    Byte lun;
    Byte res[2];
    Byte tr_len;
    Byte ctrl;
  }
  scsi_release;

  memset (&scsi_release, 0, sizeof (scsi_release));

  scsi_release.cmd = 0x17; /* RELEASE */

  DBG (3, "release_unit()\n");
  return sanei_scsi_cmd (fd, &scsi_release, sizeof (scsi_release), 0, 0);
}

static SANE_Status
test_ready (int fd)
{
  SANE_Status status;
  int try;

  struct 
  {
    /* Command */
    Byte cmd;
    Byte lun;
    Byte res[2];
    Byte tr_len;
    Byte ctrl;
  }
  scsi_test_ready;

  memset (&scsi_test_ready, 0, sizeof (scsi_test_ready));

  scsi_test_ready.cmd = 0x00; /* TEST UNIT READY */

  for (try = 0; try < 1000; ++try)
    {
      DBG (3, "test_ready: sending TEST_UNIT_READY\n");
      status = sanei_scsi_cmd (fd, &scsi_test_ready, sizeof (scsi_test_ready),
			       0, 0);

      switch (status)
	{
	case SANE_STATUS_DEVICE_BUSY:
	  usleep (100000);	/* retry after 100ms */
	  break;

	case SANE_STATUS_GOOD:
	  return status;

	default:
	  DBG (1, "test_ready: test unit ready failed (%s)\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  DBG (1, "test_ready: timed out after %d attempts\n", try);
  return SANE_STATUS_IO_ERROR;
}

static SANE_Status
sense_handler (int scsi_fd, u_char *result, void *arg)
{
  scsi_fd = scsi_fd;			/* silence gcc */
  arg = arg;					/* silence gcc */

  if (result[0])
    {
      DBG (0, "sense_handler() : sense code = %02x\n", result[0]);
      return SANE_STATUS_IO_ERROR;
    }
  else
    {
      return SANE_STATUS_GOOD;
    }
}

static SANE_Status
stop_scan (int fd)
{
  fd = fd;						/* silence gcc */

  /* XXX don't know how to stop the scanner. To be tested ! */
#if 0
  const Byte scsi_rewind[] =
  {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  DBG (1, "Trying to stop scanner...\n");
  return sanei_scsi_cmd (fd, scsi_rewind, sizeof (scsi_rewind), 0, 0);
#else
  return SANE_STATUS_GOOD;
#endif
}


static SANE_Status
start_scan (int fd, SANE_Bool cont)
{
  struct
  {
    /* Command */
    Byte cmd;
    Byte lun;
    Byte res[2];
    Byte tr_len;
    Byte ctrl;

    /* Data */
    Byte wid;
  }
  scsi_start_scan;

  memset (&scsi_start_scan, 0, sizeof (scsi_start_scan));

  scsi_start_scan.cmd = 0x1b; /* SCAN */
  scsi_start_scan.tr_len = 1;
  scsi_start_scan.wid = 0;
  scsi_start_scan.ctrl = (cont == SANE_TRUE) ? 0x80 : 0x00;

  DBG (1, "Starting scanner ...\n");
  return sanei_scsi_cmd (fd, &scsi_start_scan, sizeof (scsi_start_scan), 0, 0);
}

static void
wait_ready (int fd)
{
  struct
  {
    Byte bytes[2];		/* Total # of bytes */
    Byte scan[2];		/* ms to complete - driver sleep time */
  } result;

  size_t size = sizeof (result);
  SANE_Status status;

  struct {
    Byte cmd;
    Byte lun;
    Byte data_type;
    Byte re1[3];
    Byte tr_len[3];
    Byte ctrl;
  } cmd;
  
  memset (&cmd, 0, sizeof (cmd));

  cmd.cmd = 0x28;       /* READ */
  cmd.data_type = 0x80; /* get scan time */

  set_size (cmd.tr_len, 3, sizeof (result));

  while (1)
    {
      status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd),
			       &result, &size);

      if (status != SANE_STATUS_GOOD || size != sizeof (result))
	{
	  /*
	     Command failed, the assembler code of the windows scan library
	     ignores this condition, and so do I
	   */
	  break;
	}
      else
	{
	  /* left is the amount of seconds left till the scanner is
             ready * 100 */
	  int left = get_size (result.scan, 2);

	  DBG (1, "wait_ready() : %d left...\n", left);

	  if (!left)
	    break;
	  /* We delay only for half the given time */
	  else if (left < 200)
	    usleep (left * 5000);
	  else
	    sleep (left / 200);
	}
    }

  return;
}

static SANE_Status
get_read_sizes (int fd, int *lines_available, int *bpl, int *total_lines)
{
  struct {
    Byte reserved1[8];
    Byte line_width[2];
    Byte total_lines[2];
    Byte cur_line[2];
    Byte lines_this_block[2];
    Byte reserved[8];
  } read_sizes;

  const Byte scsi_read[] =
  {
    0x28, 0x00,				/* opcode, lun */
    0x81,				/* data type 81 == read time left */
    0x00, 0x00, 0x00,			/* reserved */
    0x00, 0x00, sizeof (read_sizes),	/* transfer length */
    0x00,				/* control byte */
  };

  size_t size = sizeof (read_sizes);
  SANE_Status status;

  status = sanei_scsi_cmd (fd, scsi_read, sizeof (scsi_read), &read_sizes, &size);

  if (status != SANE_STATUS_GOOD || size != sizeof (read_sizes))
    {
      /* Command failed */
      return SANE_STATUS_IO_ERROR;
    }
  else
    {
      *lines_available = get_size (read_sizes.lines_this_block, 2);
      *bpl = get_size (read_sizes.cur_line, 2);
      if (total_lines)
	*total_lines = get_size (read_sizes.total_lines, 2);
    }

  DBG (1, "get_read_sizes() : %d of %d, %d\n",
       *lines_available, total_lines ? *total_lines : -1, *bpl);

  return SANE_STATUS_GOOD;
}

static SANE_Status
set_window (AgfaFocus_Scanner * s)
/* This function sets and sends the window for scanning */
{
  double pixels_per_mm = (double) s->val[OPT_RESOLUTION].w / MM_PER_INCH;

  SANE_Bool auto_bright = s->val[OPT_AUTO_BRIGHTNESS].b;
  SANE_Bool auto_contr = s->val[OPT_AUTO_CONTRAST].b;

  /* ranges down 255 (dark) down to 1(bright) */
  int brightness = auto_bright ? 0 : (SANE_UNFIX (s->val[OPT_BRIGHTNESS].w)
				      * -1.27 + 128.5);
  /* ranges from 1 (little contrast) up to 255 (much contrast) */
  int contrast = auto_contr ? 0 : (SANE_UNFIX (s->val[OPT_CONTRAST].w)
				   * 1.27 + 128.5);

  int width;

  /* ranges from 40 (dark) down to 0 (bright) */
  int bright_adjust = (SANE_UNFIX (s->val[OPT_BRIGHTNESS].w) * -20.0) / 100.0 + 20.0;

  /* ranges from 20 (little contrast) down to -20 = 235 (much contrast) */
  int contr_adjust = (SANE_UNFIX (s->val[OPT_CONTRAST].w) * -20.0) / 100.0;

  /* Warning ! The following structur SEEMS to be a valid SCSI-2 SET_WINDOW
     command.  But e.g. the limits for the window are only 2 Bytes instead
     of 4.  The scanner was built at about 1990, so SCSI-2 wasn't available
     for development...  */

  struct
    {
      Byte cmd;
      Byte lun;
      Byte re1[4];
      Byte tr_len[3];
      Byte ctrl;

      Byte re2[6];
      Byte wd_len[2];

      struct
	{
	  Byte wid;                      /* Window ID */
	  Byte autobit;                  /* Window creation */

	  Byte x_axis_res[2];            /* X resolution */
	  Byte y_axis_res[2];            /* X resolution */

	  Byte x_axis_ul[2];             /* X upper left */
	  Byte y_axis_ul[2];             /* Y upper left */

	  Byte wwidth[2];                /* Width */
	  Byte wlength[2];               /* Length */

	  Byte contrast;                 /* Contrast */
	  Byte dummy1;
	  Byte intensity;                /* Intensity */

	  Byte image_comp;               /* Image composition (0, 2, 5) */
	  Byte bpp;                      /* Bits per pixel */

          Byte tonecurve;                /* Tone curve (0 - 8) */
	  Byte ht_pattern;               /* Halftone pattern */
	  Byte paddingtype;              /* Padding type */
			           
          Byte bitordering[2];           /* Bit ordering (0 = left to right) */
          Byte comprtype;                /* Compression type */
          Byte comprarg;                 /* Compression argument */

	  Byte dummy2[6];
	  Byte edge;                     /* Sharpening (0 - 7) */
	  Byte dummy3;

	  Byte bright_adjust;            /*  */
	  Byte contr_adjust;             /*  */

	  Byte imagewidthtruncation;     /* */

	  Byte dummy4;
          Byte quality_type;             /* 0 normal, 1 high, 255 low */
          Byte red_att;
          Byte green_att;
          Byte blue_att;

	  Byte dummy5[5];
	  Byte color_planes;
	  Byte orig_type;
	  Byte fixturetype;
	  Byte exposure[2];
	  Byte defocus[2];
	  Byte dummy6[4];
          Byte descreen_factor;

	  Byte packing_word_length;
	  Byte packing_number_of_pixels;
	  Byte packing_color_mode;
	  Byte strokenab;
	  Byte rotatenab;
	  Byte autostrokenab;
	  Byte dummy7;
	}
      wd;

    }
  cmd;

  memset (&cmd, 0, sizeof (cmd));

  cmd.cmd = 0x24; /* SET WINDOW PARAMETERS */

  switch (s->hw->type)
    {
    case AGFAGRAY64:
    case AGFALINEART:
    case AGFAGRAY256:
      set_size (cmd.tr_len, 3, 36 + 8);
      set_size (cmd.wd_len, 2, 36);
      break;
      
    case AGFACOLOR:
      set_size (cmd.tr_len, 3, 65 + 8);
      set_size (cmd.wd_len, 2, 65);
      break;
    }

  /* Resolution.  Original comment in German: Aufloesung */
  set_size (cmd.wd.x_axis_res, 2, s->val[OPT_RESOLUTION].w);
  set_size (cmd.wd.y_axis_res, 2, s->val[OPT_RESOLUTION].w);

  /* Scan window position/size.  Original comment in German:
     Fensterposition / Groesse */
  set_size (cmd.wd.x_axis_ul, 2,
	    SANE_UNFIX (s->val[OPT_TL_X].w) * pixels_per_mm + 0.5);
  set_size (cmd.wd.y_axis_ul, 2,
	    SANE_UNFIX (s->val[OPT_TL_Y].w) * pixels_per_mm + 0.5);

  width = (SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w) * pixels_per_mm) + 0.5;

  if (s->bpp == 1 && width % 8)
    width += 8 - width % 8;

  set_size (cmd.wd.wwidth, 2, width);
  set_size (cmd.wd.wlength, 2, SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)
	    * pixels_per_mm + 0.5);

  cmd.wd.bpp = s->bpp;

  if (s->mode == COLOR18BIT ||
      s->mode == COLOR24BIT)
    {
      cmd.wd.paddingtype = 3;
      cmd.wd.ht_pattern = 3;

      cmd.wd.red_att = s->r_att;
      cmd.wd.blue_att = s->g_att;
      cmd.wd.green_att = s->b_att;
      cmd.wd.color_planes = 0x0e;

      set_size (cmd.wd.exposure, 2, s->exposure);

      cmd.wd.packing_word_length = 1;
      cmd.wd.packing_number_of_pixels = 1;
      cmd.wd.packing_color_mode = 2;

      if (s->bpp == 6)
	cmd.wd.edge = s->edge;

      DBG (3,
	   "Setting parameters: imc %d, bpp %d, res %d, exp %d, attenuation [%d, %d, %d], edge %d\n",
	   s->image_composition, s->bpp, s->val[OPT_RESOLUTION].w,
	   s->exposure, cmd.wd.red_att, cmd.wd.blue_att, cmd.wd.green_att, s->edge);
    }
  else
    {
      if (s->bpp == 1)
	cmd.wd.ht_pattern = s->halftone;
      else
	cmd.wd.ht_pattern = 3;
      
      cmd.wd.intensity = brightness;
      cmd.wd.contrast = contrast;

      cmd.wd.contr_adjust = contr_adjust;
      cmd.wd.bright_adjust = bright_adjust;

      cmd.wd.tonecurve = s->tonecurve;
      cmd.wd.paddingtype = 3;
      cmd.wd.edge = s->edge;

      if (s->lin_log)
	cmd.wd.dummy3 = 0x02;

      DBG (3,
	   "Setting parameters: imc %d, bpp %d, res %d, bri %d, con %d, bad %d, cad %d, ht %d, edge %d\n",
	   s->image_composition, s->bpp, s->val[OPT_RESOLUTION].w,
	   brightness, contrast, bright_adjust, contr_adjust, s->halftone, s->edge);
    }

  cmd.wd.image_comp = s->image_composition;
  cmd.wd.quality_type = s->quality;
  cmd.wd.orig_type = s->original;

  return sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
}

/* Tell scanner to scan more data. */

static SANE_Status
request_more_data (AgfaFocus_Scanner * s)
{
  SANE_Status status;
  int lines_available;
  int bytes_per_line;

  status = start_scan (s->fd, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (!s->hw->disconnect)
    wait_ready (s->fd);

  status = get_read_sizes (s->fd, &lines_available, &bytes_per_line, 0);

  if (!lines_available)
    return SANE_STATUS_INVAL;

  s->lines_available = lines_available;

  return SANE_STATUS_GOOD;
}

static SANE_Status
upload_dither_matrix (AgfaFocus_Scanner * s, int rows, int cols, int *dither_matrix)
{
  struct {
    Byte cmd;
    Byte lun;
    Byte data_type;
    Byte re1[3];
    Byte tr_len[3];
    Byte ctrl;
    
    struct {
      Byte nrrows[2];
      Byte nrcols[2];
      
      struct {
	Byte data[2];
      } element[256];
    } wd;
  } cmd;
  
  SANE_Status status;
  int i;
  
  memset (&cmd, 0, sizeof (cmd));

  cmd.cmd = 0x2a;       /* WRITE */
  cmd.data_type = 0x81; /* upload dither matrix */

  set_size (cmd.tr_len, 3, 4 + (2 * rows * cols));
  set_size (cmd.wd.nrrows, 2, rows);
  set_size (cmd.wd.nrcols, 2, cols);

  for (i = 0; i < cols * rows; ++i)
    set_size (cmd.wd.element[i].data, 2, dither_matrix[i]);
      
  status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
  
  if (status != SANE_STATUS_GOOD)
    /* Command failed */
    return SANE_STATUS_IO_ERROR;

  DBG (1, "upload_dither_matrix(): uploaded dither matrix: %d, %d\n", rows, cols);

  return SANE_STATUS_GOOD;
}

#if 0
static SANE_Status
upload_tonecurve (AgfaFocus_Scanner * s, int color_type, int input, int output, int dither_matrix[256])
{
  struct {
   Byte cmd;
   Byte lun;
   Byte re1[4];
   Byte tr_len[3];
   Byte ctrl;
   
   Byte re2[6];
   Byte wd_len[2];

    struct {
      Byte color_type[2];
      Byte nrinput[2];
      Byte nroutput[2];
      
      struct {
	Byte data[2];
      } outputval[256];
    } wd;
  } cmd;

  SANE_Status status;
  int i, j;

  memset (&cmd, 0, sizeof (cmd));

  cmd.cmd = 0x80;

  set_size (cmd.tr_len, 3, sizeof (cmd.wd));
  set_size (cmd.wd.nrrows, 2, rows);
  set_size (cmd.wd.nrrows, 2, cols);

  for (i = 0; i < cols; ++i)
    for (j = 0; j < rows; ++j)
      set_size (cmd.wd.element[j + i * rows].data, 2, dither_matrix[j + i * rows]);
      
  status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
  
  if (status != SANE_STATUS_GOOD)
  /*    * Command failed * */
    return SANE_STATUS_IO_ERROR;

  DBG (1, "upload_dither_matrix(): uploaded dither matrix\n");

  return SANE_STATUS_GOOD;
}
#endif

/* May only be called when there is at least one row of data to
   be read.

   Original comment in German: Darf nur aufgerufen werden, wenn
   wirklich noch Zeilen zu scannen/lesen sind !  */
static SANE_Status
read_data (AgfaFocus_Scanner * s, SANE_Byte *buf, int lines, int bpl)
{
  struct {
   Byte cmd;
   Byte lun;
   Byte re1[4];
   Byte tr_len[3];
   Byte ctrl;
  } cmd;

  SANE_Status status;
  size_t size;
  unsigned int i;

  memset (&cmd, 0, sizeof (cmd));

  cmd.cmd = 0x28; /* READ */

  set_size (cmd.tr_len, 3, lines);
  size = lines * bpl;
  
  status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), buf, &size);
  
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sanei_scsi_cmd() = %d\n", status);
      return SANE_STATUS_IO_ERROR;
    }
  
  if (size != ((unsigned int) lines * bpl))
    {
      DBG (1, "sanei_scsi_cmd(): got %lu bytes, expected %d\n",
	   (u_long) size, lines * bpl);
      return SANE_STATUS_INVAL;
    }
  
  DBG (1, "Got %lu bytes\n", (u_long) size);

  /* Reverse: */
  if (s->bpp != 1)
    {
      if (s->bpp != 6)
	for (i = 0; i < size; i++)
	  buf[i] = 255 - buf[i];
      else
	for (i = 0; i < size; i++)
	  buf[i] = 255 - ((buf[i] * 256.0f) / 64.0f);
    }
		  
  s->lines_available -= lines;

  return SANE_STATUS_GOOD;
}


static SANE_Status
attach (const char *devname, AgfaFocus_Device ** devp)
{
#define ATTACH_SCSI_INQ_LEN 55
  const Byte scsi_inquiry[] =
  {
    0x12, 0x00, 0x00, 0x00, ATTACH_SCSI_INQ_LEN, 0x00
  };
  Byte result[ATTACH_SCSI_INQ_LEN];

  int fd;
  AgfaFocus_Device *dev;
  SANE_Status status;
  size_t size;
  int i;

  for (dev = agfafocus_devices; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	return SANE_STATUS_GOOD;
      }

  DBG (3, "attach: opening %s\n", devname);
  status = sanei_scsi_open (devname, &fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: open failed (%s)\n", sane_strstatus (status));
      return SANE_STATUS_INVAL;
    }

  DBG (4, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = sanei_scsi_cmd (fd, scsi_inquiry, sizeof (scsi_inquiry),
			   result, &size);
  if (status != SANE_STATUS_GOOD || size != ATTACH_SCSI_INQ_LEN)
    {
      DBG (1, "attach: inquiry failed (%s)\n", sane_strstatus (status));
      sanei_scsi_close (fd);
      return status;
    }

  status = test_ready (fd);
  sanei_scsi_close (fd);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* The structure send by the scanner after inquiry is not SCSI-2
     compatible.  The standard manufacturer/model fields are no ASCII
     strings, but ?  At offset 36 my SIEMENS scanner identifies as an
     AGFA one ?!   */

  if (result[0] != 6 || strncmp ((char *)result + 36, "AGFA0", 5))
    {
      DBG (1, "attach: device doesn't look like a Siemens 9036 scanner\n");
      return SANE_STATUS_INVAL;
    }

  DBG (4, "Inquiry data:\n");
  DBG (4, "-----------\n");
  for (i = 5; i < 55; i += 10)
    DBG (4, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
      result[i], result[i + 1], result[i + 2], result[i + 3], result[i + 4],
	 result[i + 5], result[i + 6], result[i + 7], result[i + 8],
	 result[i + 9]);

  dev = malloc (sizeof (*dev));

  if (!dev)
    return SANE_STATUS_NO_MEM;

  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devname);
  if (!strncmp ((char *)result + 36, "AGFA01", 6)) {
    dev->sane.vendor = "AGFA";
    dev->sane.model = "Focus GS Scanner (6 bit)";
    dev->upload_user_defines = SANE_TRUE;
    dev->type = AGFAGRAY64;
  } else if (!strncmp ((char *)result + 36, "AGFA02", 6)) {
    dev->sane.vendor = "AGFA";
    dev->sane.model = "Focus Lineart Scanner";
    dev->upload_user_defines = SANE_FALSE;
    dev->type = AGFALINEART;
  } else if (!strncmp ((char *)result + 36, "AGFA03", 6)) {
    dev->sane.vendor = "AGFA";
    dev->sane.model = "Focus II";
    dev->upload_user_defines = SANE_TRUE;
    dev->type = AGFAGRAY256;
  } else if (!strncmp ((char *)result + 36, "AGFA04", 6)) {
    dev->sane.vendor = "AGFA";
    dev->sane.model = "Focus Color";
    dev->upload_user_defines = SANE_TRUE;
    dev->type = AGFACOLOR;
  } else {
    free (dev);
    DBG (1, "attach: device looks like an AGFA scanner, but wasn't recognised\n");
    return SANE_STATUS_INVAL;
  }
  dev->sane.type = "flatbed scanner";

  dev->transparent = result[45] & 0x80 ? SANE_TRUE : SANE_FALSE;
  dev->analoglog =   result[46] & 0x80 ? SANE_TRUE : SANE_FALSE;
  dev->tos5 =        result[46] & 0x05 ? SANE_TRUE : SANE_FALSE;
  dev->quality =     result[47] & 0x40 ? SANE_TRUE : SANE_FALSE;
  dev->disconnect =  result[47] & 0x80 ? SANE_TRUE : SANE_FALSE;

  DBG (4, "\n");
  DBG (4, "scan modes:\n");
  DBG (4, "-----------\n");
  DBG (4, "three pass color mode: %s\n", dev->type >= AGFACOLOR ? "yes" : "no");
  DBG (4, "8 bit gray mode: %s\n", dev->type >= AGFAGRAY64 ? "yes" : "no");
  DBG (4, "uploadable matrices: %s\n", dev->upload_user_defines ? "yes" : "no");
  DBG (4, "transparency: %s\n", dev->transparent ? "yes" : "no");
  DBG (4, "disconnect: %s\n", dev->disconnect ? "yes" : "no");
  DBG (4, "quality calibration: %s\n", dev->quality ? "yes" : "no");

  dev->handle = 0;

  DBG (3, "attach: found AgfaFocus scanner model\n");

  ++num_devices;
  dev->next = agfafocus_devices;
  agfafocus_devices = dev;

  if (devp)
    *devp = dev;

  return SANE_STATUS_GOOD;
}

static SANE_Status
do_eof (AgfaFocus_Scanner *s)
{
  if (s->pipe >= 0)
    {
      close (s->pipe);
      s->pipe = -1;
    }
  return SANE_STATUS_EOF;
}


static SANE_Status
do_cancel (AgfaFocus_Scanner * s)
{
  s->scanning = SANE_FALSE;
  s->pass = 0;

  do_eof (s);

  if (s->reader_pid != -1)
    {
      int exit_status;

      /* ensure child knows it's time to stop: */
      sanei_thread_kill (s->reader_pid);
      sanei_thread_waitpid (s->reader_pid, &exit_status);
      s->reader_pid = -1;
    }

  if (s->fd >= 0)
    {
      stop_scan (s->fd);
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }

  return SANE_STATUS_CANCELLED;
}


static SANE_Status
init_options (AgfaFocus_Scanner * s)
{
  int i;

  /* Hardware Limitations: must be static ! */
  static const SANE_Int dpi_list[] =
  {8, 100, 200, 300, 400, 500, 600, 700, 800};

  static const SANE_Range percentage_range =
  {
    -100 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
    100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
    1 << SANE_FIXED_SCALE_SHIFT	/* quantization */
  };

  static const SANE_Range sharpen_range =
  {0, 7, 1};
  static const SANE_Range exposure_range =
  {0, 100, 0};
  static const SANE_Range attenuation_range =
  {
    0 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
    100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
    1 << SANE_FIXED_SCALE_SHIFT	        /* quantization */
  };


  static const SANE_Range x_range =
  {0, SANE_FIX (8.27 * MM_PER_INCH), 0};
  static const SANE_Range y_range =
  {0, SANE_FIX (12.72 * MM_PER_INCH), 0};

  /* ------ */

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

  /* "Mode" group: */

  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;

  switch (s->hw->type)
    {
    case AGFACOLOR:
      s->opt[OPT_MODE].size = max_string_size (focuscolor_mode_list);
      s->opt[OPT_MODE].constraint.string_list = focuscolor_mode_list;
      s->val[OPT_MODE].s = strdup (focuscolor_mode_list[0]);
      break;
    case AGFAGRAY256:
      s->opt[OPT_MODE].size = max_string_size (focusii_mode_list);
      s->opt[OPT_MODE].constraint.string_list = focusii_mode_list;
      s->val[OPT_MODE].s = strdup (focusii_mode_list[0]);
      break;
    default:
      s->opt[OPT_MODE].size = max_string_size (focus_mode_list);
      s->opt[OPT_MODE].constraint.string_list = focus_mode_list;
      s->val[OPT_MODE].s = strdup (focus_mode_list[0]);
      break;
    }

  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = dpi_list;
  s->val[OPT_RESOLUTION].w = 100;

  s->opt[OPT_SOURCE].name  = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc  = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type  = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].unit  = SANE_UNIT_NONE;
  if (!s->hw->transparent)
    s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
  else
    s->opt[OPT_SOURCE].cap &= ~SANE_CAP_INACTIVE;
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].constraint.string_list = source_list;
  s->opt[OPT_SOURCE].size  = max_string_size (source_list);
  s->val[OPT_SOURCE].s     = strdup (source_list[0]);

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
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &x_range;
  s->val[OPT_BR_X].w = x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &y_range;
  s->val[OPT_BR_Y].w = y_range.max;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* exposure */
  s->opt[OPT_EXPOSURE].name  = "exposure";
  s->opt[OPT_EXPOSURE].title = "Exposure";
  s->opt[OPT_EXPOSURE].desc  = "Analog exposure control.";
  s->opt[OPT_EXPOSURE].type  = SANE_TYPE_INT;
  s->opt[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_EXPOSURE].unit  = SANE_UNIT_PERCENT;
  s->opt[OPT_EXPOSURE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_EXPOSURE].constraint.range = &exposure_range;
  s->val[OPT_EXPOSURE].w     = 23;

  /* brightness automatic correct */
  s->opt[OPT_AUTO_BRIGHTNESS].name = "adjust-bright";
  s->opt[OPT_AUTO_BRIGHTNESS].title = "Automatic brightness correction";
  s->opt[OPT_AUTO_BRIGHTNESS].desc = "Turns on automatic brightness correction of "
    "the acquired image. This makes the scanner do a two pass scan to analyse the "
    "brightness of the image before it's scanned.";
  s->opt[OPT_AUTO_BRIGHTNESS].type = SANE_TYPE_BOOL;
  s->val[OPT_AUTO_BRIGHTNESS].b = SANE_FALSE;

  /* contrast automatic correct */
  s->opt[OPT_AUTO_CONTRAST].name = "adjust-contr";
  s->opt[OPT_AUTO_CONTRAST].title = "Automatic contrast correction";
  s->opt[OPT_AUTO_CONTRAST].desc = "Turns on automatic contrast correction of "
    "the acquired image. This makes the scanner do a two pass scan to analyse "
    "the contrast of the image to be scanned.";
  s->opt[OPT_AUTO_CONTRAST].type = SANE_TYPE_BOOL;
  s->val[OPT_AUTO_CONTRAST].b = SANE_FALSE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = "Controls the brightness of the acquired image. "
    "When automatic brightness is enabled, this can be used to adjust the selected brightness.";
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
  s->val[OPT_BRIGHTNESS].w = 0;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = "Controls the contrast of the acquired image. "
    "When automatic contrast is enabled, this can be used to adjust the selected contrast.";
  s->opt[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
  s->val[OPT_CONTRAST].w = 0;
  
  /* halftone patterns */
  s->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_PATTERN].size = 32;
  s->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  if (s->hw->upload_user_defines)
    {
      s->opt[OPT_HALFTONE_PATTERN].constraint.string_list = halftone_upload_list;
      s->val[OPT_HALFTONE_PATTERN].s = strdup (halftone_upload_list[0]);
    }
  else
    {
      s->opt[OPT_HALFTONE_PATTERN].constraint.string_list = halftone_list;
      s->val[OPT_HALFTONE_PATTERN].s = strdup (halftone_list[0]);
    }

  /* red-attenuation */
  s->opt[OPT_ATTENUATION_RED].name  = "red-attenuation";
  s->opt[OPT_ATTENUATION_RED].title = "Red attenuation";
  s->opt[OPT_ATTENUATION_RED].desc  = "Controls the red attenuation of the acquired image. "
    "Higher values mean less impact on scanned image.";
  s->opt[OPT_ATTENUATION_RED].type = SANE_TYPE_FIXED;
  s->opt[OPT_ATTENUATION_RED].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_ATTENUATION_RED].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_ATTENUATION_RED].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_ATTENUATION_RED].constraint.range = &attenuation_range;
  s->val[OPT_ATTENUATION_RED].w = SANE_FIX (50.0);

  /* green-attenuation */
  s->opt[OPT_ATTENUATION_GREEN].name  = "green-attenuation";
  s->opt[OPT_ATTENUATION_GREEN].title = "Green attenuation";
  s->opt[OPT_ATTENUATION_GREEN].desc  = "Controls the green attenuation of the acquired image. "
    "Higher values mean less impact on scanned image.";
  s->opt[OPT_ATTENUATION_GREEN].type = SANE_TYPE_FIXED;
  s->opt[OPT_ATTENUATION_GREEN].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_ATTENUATION_GREEN].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_ATTENUATION_GREEN].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_ATTENUATION_GREEN].constraint.range = &attenuation_range;
  s->val[OPT_ATTENUATION_GREEN].w = SANE_FIX (50.0);

  /* blue-attenuation */
  s->opt[OPT_ATTENUATION_BLUE].name  = "blue-attenuation";
  s->opt[OPT_ATTENUATION_BLUE].title = "Blue attenuation";
  s->opt[OPT_ATTENUATION_BLUE].desc  = "Controls the blue attenuation of the acquired image. "
    "Higher values mean less impact on scanned image.";
  s->opt[OPT_ATTENUATION_BLUE].type = SANE_TYPE_FIXED;
  s->opt[OPT_ATTENUATION_BLUE].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_ATTENUATION_BLUE].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_ATTENUATION_BLUE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_ATTENUATION_BLUE].constraint.range = &attenuation_range;
  s->val[OPT_ATTENUATION_BLUE].w = SANE_FIX (50.0);

  /* quality-calibration */
  s->opt[OPT_QUALITY].name  = SANE_NAME_QUALITY_CAL;
  s->opt[OPT_QUALITY].title = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QUALITY].desc  = "Controls the calibration that will be done in the "
        "scanner.  Less calibration result in faster scanner times.";
  s->opt[OPT_QUALITY].type = SANE_TYPE_STRING;
  s->opt[OPT_QUALITY].size = 32;
  if (!s->hw->quality)
    s->opt[OPT_QUALITY].cap |= SANE_CAP_INACTIVE;
  else
    s->opt[OPT_QUALITY].cap &= ~SANE_CAP_INACTIVE;
  s->opt[OPT_QUALITY].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_QUALITY].constraint.string_list = quality_list;
  s->val[OPT_QUALITY].s = strdup (quality_list[1]);

  /* sharpening */
  s->opt[OPT_SHARPEN].name = "sharpen";
  s->opt[OPT_SHARPEN].title = "Sharpening";
  s->opt[OPT_SHARPEN].desc = "Controls the sharpening that will be "
    "done by the video processor in the scanner.";
  s->opt[OPT_SHARPEN].type = SANE_TYPE_INT;
  s->opt[OPT_SHARPEN].unit = SANE_UNIT_NONE;
  s->opt[OPT_SHARPEN].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SHARPEN].constraint.range = &sharpen_range;
  s->val[OPT_SHARPEN].w = 1;

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one (const char *dev)
{
  attach (dev, 0);
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  authorize = authorize;		/* silence gcc */

  DBG_INIT ();

  sanei_thread_init ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open ("agfafocus.conf");
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      attach ("/dev/scanner", 0);
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;
      len = strlen (dev_name);

      if (!len)
	continue;		/* ignore empty lines */

      sanei_config_attach_matching_devices (dev_name, attach_one);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  AgfaFocus_Device *dev, *next;

  for (dev = agfafocus_devices; dev; dev = next)
    {
      next = dev->next;
      if (dev->handle)
	sane_close (dev->handle);
      free (dev);
    }
  
  if (devlist)
    free (devlist);
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  AgfaFocus_Device *dev;
  int i;

  local_only = local_only;		/* silence gcc */

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  for (dev = agfafocus_devices, i = 0; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  AgfaFocus_Device *dev;
  SANE_Status status;
  AgfaFocus_Scanner *s;

  if (devicename[0])
    {
      status = attach (devicename, &dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  else
    {
      /* empty devicname -> use first device */
      dev = agfafocus_devices;
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  if (dev->handle)
    return SANE_STATUS_DEVICE_BUSY;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;

  memset (s, 0, sizeof (*s));
  s->scanning = SANE_FALSE;

  s->fd = -1;
  s->hw = dev;
  s->hw->handle = s;

  init_options (s);

  *handle = s;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  AgfaFocus_Scanner *s = handle;

  if (s->scanning)
    do_cancel (handle);

  s->hw->handle = 0;

  free (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  AgfaFocus_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  AgfaFocus_Scanner *s = handle;
  SANE_Status status;

  if (info)
    *info = 0;

  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  if (option >= NUM_OPTIONS || !SANE_OPTION_IS_ACTIVE (s->opt[option].cap))
    return SANE_STATUS_UNSUPPORTED;

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_SHARPEN:
	case OPT_EXPOSURE:
	case OPT_ATTENUATION_RED:
	case OPT_ATTENUATION_GREEN:
	case OPT_ATTENUATION_BLUE:
	  *(SANE_Word *) val = s->val[option].w;
	  break;
	case OPT_AUTO_BRIGHTNESS:
	case OPT_AUTO_CONTRAST:
	  *(SANE_Bool *) val = s->val[option].b;
	  break;
        case OPT_MODE:
	case OPT_HALFTONE_PATTERN:
	case OPT_QUALITY:
	case OPT_SOURCE:
          strcpy (val, s->val[option].s);
          return (SANE_STATUS_GOOD);
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}

    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (s->opt[option].cap))
	return SANE_STATUS_UNSUPPORTED;

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	case OPT_SHARPEN:
	case OPT_EXPOSURE:
	case OPT_ATTENUATION_RED:
	case OPT_ATTENUATION_GREEN:
	case OPT_ATTENUATION_BLUE:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	case OPT_AUTO_BRIGHTNESS:
	case OPT_AUTO_CONTRAST:
	  s->val[option].b = *(SANE_Bool *) val;
	  break;
        case OPT_MODE:
	  if (strcmp (s->val[option].s, (SANE_String) val))
	    {
	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	      if (s->val[option].s)
		free (s->val[option].s);

	      s->val[option].s = strdup (val);

	      if (strcmp (s->val[option].s, "Gray (6 bit)") == 0)
		s->mode = GRAY6BIT;	
	      else if (strcmp (s->val[option].s, "Gray (8 bit)") == 0)
		s->mode = GRAY8BIT;	
	      else if (strcmp (s->val[option].s, "Color (18 bit)") == 0)
		s->mode = COLOR18BIT;	
	      else if (strcmp (s->val[option].s, "Color (24 bit)") == 0)
		s->mode = COLOR24BIT;	
	      else
		s->mode = LINEART;
    
	      switch (s->mode)
		{
		case LINEART:
		  s->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_SHARPEN].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_RED].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_GREEN].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_BLUE].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		  break;

		case GRAY6BIT:
		  s->opt[OPT_SHARPEN].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_RED].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_GREEN].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_BLUE].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
		  break;

		case GRAY8BIT:
		  s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_RED].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_GREEN].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_BLUE].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_SHARPEN].cap |= SANE_CAP_INACTIVE;
		  break;

		case COLOR18BIT:
		  s->opt[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_RED].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_GREEN].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_BLUE].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_BRIGHTNESS].cap |=  SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap |=  SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_CONTRAST].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_SHARPEN].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
		  break;

		case COLOR24BIT:
		  s->opt[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_RED].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_GREEN].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_ATTENUATION_BLUE].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_BRIGHTNESS].cap |=  SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap |=  SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_AUTO_CONTRAST].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_SHARPEN].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
		  break;
		}
	    }
	  break;
	case OPT_SOURCE:
	case OPT_QUALITY:
	case OPT_HALFTONE_PATTERN:
          if (info && strcmp (s->val[option].s, (SANE_String) val))
            *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
          if (s->val[option].s)
            free (s->val[option].s);
          s->val[option].s = strdup (val);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}

    }
  else
    {
      return SANE_STATUS_UNSUPPORTED;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  AgfaFocus_Scanner *s = handle;

  if (!s->scanning)
    {
      double width, height, dpi;
      const char *quality;
      const char *original;

      memset (&s->params, 0, sizeof (s->params));

      width = SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w);
      height = SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w);
      dpi = s->val[OPT_RESOLUTION].w;

      /* make best-effort guess at what parameters will look like once
         scanning starts.  */
      if (dpi > 0.0 && width > 0.0 && height > 0.0)
	{
	  double dots_per_mm = dpi / MM_PER_INCH;

	  s->params.pixels_per_line = width * dots_per_mm + 0.5;
	  s->params.lines = height * dots_per_mm + 0.5;
	}

      /* Should we specify calibration quality? */

      if (SANE_OPTION_IS_ACTIVE (s->opt[OPT_QUALITY].cap))
        {
          DBG(3, " -------------- setting quality\n");
          quality = s->val[OPT_QUALITY].s;
          if (strcmp (quality, "Low") == 0 )
	    s->quality = 255;
          else if (strcmp (quality, "High") == 0)
	    s->quality = 1;
          else
	    s->quality = 0;
        }
      else
        s->quality = 0;

      /* Should we select source type? */

      if (SANE_OPTION_IS_ACTIVE (s->opt[OPT_SOURCE].cap))
        {
          DBG(3, " -------------- setting source\n");
          original = s->val[OPT_SOURCE].s;
          if (strcmp (original, "Transparency") == 0)
	    s->original = 0;
          else
	    s->original = 1;
        }
      else
        s->original = 0;

      s->exposure = ((s->val[OPT_EXPOSURE].w * (255.0f - 80.0f)) / 100.0f) + 80.0f;
      s->r_att = (SANE_UNFIX (s->val[OPT_ATTENUATION_RED].w) * 20.0f) / 100.0f;
      s->g_att = (SANE_UNFIX (s->val[OPT_ATTENUATION_GREEN].w) * 20.0f) / 100.0f;
      s->b_att = (SANE_UNFIX (s->val[OPT_ATTENUATION_BLUE].w) * 20.0f) / 100.0f;
      s->tonecurve = 0;

      switch (s->mode)
	{
	case LINEART:
	  {
	    const char *halftone;
	    
	    s->image_composition = 0;
	    
	    /* in 1 bpp mode, lines need to be 8 pixel length */
	    
	    if (s->params.pixels_per_line % 8)
	      s->params.pixels_per_line += 8 - (s->params.pixels_per_line % 8);
	    
	    s->params.format = SANE_FRAME_GRAY;
	    s->params.bytes_per_line = s->params.pixels_per_line / 8;
	    s->bpp = s->params.depth = 1;
	    
	    halftone = s->val[OPT_HALFTONE_PATTERN].s;
	    if (strcmp (halftone, "1") == 0 )
	      s->halftone = 1;
	    else if (strcmp (halftone, "Dispersed dot 4x4") == 0)
	      s->halftone = 2;
	    else if (strcmp (halftone, "Round (Clustered dot 4x4)") == 0)
	      s->halftone = 3;
	    else if (strcmp (halftone, "Diamond (Clustered dot 4x4)") == 0)
	      s->halftone = 4;
	    else if (strcmp (halftone, "User defined") == 0)
	      s->halftone = 5;
	    else
	      s->halftone = 0;
	    
	    s->edge = s->val[OPT_SHARPEN].w;
	  }
	  break;

	case GRAY6BIT:
	  s->image_composition = 2;
	  
          s->params.format = SANE_FRAME_GRAY;
          s->params.bytes_per_line = s->params.pixels_per_line;
	  s->bpp = 6;
	  s->params.depth = 8;
	  s->edge = s->val[OPT_SHARPEN].w;

	  break;

	case GRAY8BIT:
	  s->image_composition = 2;
	  
          s->params.format = SANE_FRAME_GRAY;
          s->params.bytes_per_line = s->params.pixels_per_line;
	  s->bpp = s->params.depth = 8;

	  break;

	case COLOR18BIT:
	  s->image_composition = 5;

          s->params.format = SANE_FRAME_RED;
          s->params.bytes_per_line = s->params.pixels_per_line;
	  s->bpp = 6;
	  s->params.depth = 8;
	  s->edge = s->val[OPT_SHARPEN].w;

	  break;

	case COLOR24BIT:
	  s->image_composition = 5;

          s->params.format = SANE_FRAME_RED;
          s->params.bytes_per_line = s->params.pixels_per_line;
          s->bpp = s->params.depth = 8;

	  break;
        }

      s->pass = 0;

      /*s->params.bytes_per_line =
	(s->params.pixels_per_line + (8 - s->params.depth))
	/ (8 / s->params.depth);*/
    }
  else
    if (s->mode == COLOR18BIT ||
	s->mode == COLOR24BIT)
      s->params.format = SANE_FRAME_RED + s->pass;
  
  s->params.last_frame = (s->params.format != SANE_FRAME_RED && s->params.format != SANE_FRAME_GREEN);
  
  if (params)
    *params = s->params;
  return SANE_STATUS_GOOD;
}

/* This function is executed as a child process.  The reason this is
   executed as a subprocess is because some (most?) generic SCSI
   interfaces block a SCSI request until it has completed.  With a
   subprocess, we can let it block waiting for the request to finish
   while the main process can go about to do more important things
   (such as recognizing when the user presses a cancel button).


   WARNING: Since this is executed as a subprocess, it's NOT possible
   to update any of the variables in the main process (in particular
   the scanner state cannot be updated).  */
static int
reader_process (void *scanner)
{
  AgfaFocus_Scanner *s = (AgfaFocus_Scanner *) scanner;
  int fd = s->reader_pipe;

  SANE_Status status;
  SANE_Byte *data;
  int lines_read = 0;
  int lines_per_buffer;
  int bytes_per_line = 0, total_lines = 0;
  int i;
  sigset_t sigterm_set;
  sigset_t ignore_set;
  struct SIGACTION act;

  if (sanei_thread_is_forked()) close (s->pipe);

  sigfillset (&ignore_set);
  sigdelset (&ignore_set, SIGTERM);
#if defined (__APPLE__) && defined (__MACH__)
  sigdelset (&ignore_set, SIGUSR2);
#endif
  sigprocmask (SIG_SETMASK, &ignore_set, 0);

  memset (&act, 0, sizeof (act));
  sigaction (SIGTERM, &act, 0);

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  if (!s->hw->disconnect)
    wait_ready (s->fd);

  status = get_read_sizes (s->fd, &s->lines_available, &bytes_per_line, &total_lines);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "open: get_read_sizes() failed: %s\n",
	   sane_strstatus (status));
      do_cancel (s);
      close (fd);
      return 1;
    }

  if (!s->lines_available || !bytes_per_line || !total_lines || bytes_per_line < s->params.bytes_per_line)
    {
      DBG (1, "open: invalid sizes: %d, %d, %d\n",
	   s->lines_available, bytes_per_line, total_lines);
      do_cancel (s);
      close (fd);
      return 1;
    }

  lines_per_buffer = sanei_scsi_max_request_size / bytes_per_line;
  if (!lines_per_buffer)
    {
      close (fd);
      return 2;			/* resolution is too high */
    }

  data = malloc (lines_per_buffer * bytes_per_line);
  if (!data)
    {
      DBG (1, "open  malloc(%lu) failed.\n", (u_long) lines_per_buffer * bytes_per_line);
      do_cancel (s);
      close (fd);
      return 1;
    }

  while (lines_read < s->params.lines)
    {
      int lines = lines_per_buffer;

      if (s->lines_available == 0)
	{
	  /* No lines in scanner?  Scan some more */
	  status = request_more_data (s);
	  
	  if (status != SANE_STATUS_GOOD)
	    {
	      close (fd);
	      return 1;
	    }
	}
      
      /* We only request as many lines as there are already scanned */
      if (lines > s->lines_available)
	lines = s->lines_available;

      DBG (1, "Requesting %d lines, in scanner: %d, total: %d\n", lines,
	   s->lines_available, s->params.lines);
  
      status = read_data (s, data, lines, bytes_per_line);
      
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_read: read_data() failed (%s)\n",
	       sane_strstatus (status));
	  do_cancel (s);
	  close (fd);
	  return 1;
	}
      
      /* Sometimes the scanner will return more bytes per line than
         requested, so we copy only what we wanted. */
      
      for (i = 0; i < lines; i++)
	if (write (fd, data + i * bytes_per_line, s->params.bytes_per_line) != s->params.bytes_per_line)
	  {
	    do_cancel (s);
	    close (fd);
	    return 1;
	  }

      lines_read += lines;
    }
  
  close (fd);
  return 0;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  AgfaFocus_Scanner *s = handle;
  SANE_Status status;
  int fds[2];
  
  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  /* don't initialise scanner if we're doing a three-pass scan */

  if (s->pass == 0)
    {
      if (s->fd < 0)
	{
	  status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1, "open: open of %s failed: %s\n",
		   s->hw->sane.name, sane_strstatus (status));
	      s->fd = -1;
	      return status;
	    }
	}

      status = test_ready (s->fd);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "open: test_ready() failed: %s\n", sane_strstatus (status));
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return status;
	}

      status = reserve_unit (s->fd);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "open: reserve_unit() failed: %s\n", sane_strstatus (status));
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return status;
	}

      status = set_window (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "open: set_window() failed: %s\n", sane_strstatus (status));
	  release_unit (s->fd);
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return status;
	}

      {
	int matrix[256] = {
	   2, 60, 16, 56,  3, 57, 13, 53,
	  34, 18, 48, 32, 35, 19, 45, 29,
	  10, 50,  6, 63, 11, 51,  7, 61,
	  42, 26, 38, 22, 43, 27, 39, 23,
	   4, 58, 14, 54,  1, 59, 15, 55,
	  36, 20, 46, 30, 33, 17, 47, 31,
	  12, 52,  8, 62,  9, 49,  5, 63, 
	  44, 28, 40, 24, 41, 25, 37, 21
	};
	
	status = upload_dither_matrix (s, 8, 8, matrix);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (1, "open: upload_dither_matrix() failed: %s\n", sane_strstatus (status));
	    release_unit (s->fd);
	    sanei_scsi_close (s->fd);
	    s->fd = -1;
	    return status;
	  }
      }

      s->scanning = SANE_TRUE;

      status = start_scan (s->fd, SANE_FALSE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "open: start_scan() failed: %s\n", sane_strstatus (status));
	  do_cancel (s);
	  return status;
	}
    }
  else
    {
      /* continue three-pass scan */

      status = start_scan (s->fd, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "open: start_scan() failed: %s\n", sane_strstatus (status));
	  do_cancel (s);
	  return status;
	}
    }

  if (pipe (fds) < 0)
    return SANE_STATUS_IO_ERROR;

  s->pipe = fds[0];
  s->reader_pipe = fds[1];
  s->reader_pid = sanei_thread_begin (reader_process, (void *) s);

  if (sanei_thread_is_forked()) close (s->reader_pipe);

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  AgfaFocus_Scanner *s = handle;
  ssize_t nread;

  *len = 0;

  nread = read (s->pipe, buf, max_len);
  DBG (3, "read %ld bytes\n", (long) nread);

  if (!s->scanning)
    return do_cancel (s);
  
  if (nread < 0) {
    if (errno == EAGAIN) {
      return SANE_STATUS_GOOD;
    } else {
      do_cancel (s);
      return SANE_STATUS_IO_ERROR;
    }
  }

  *len = nread;

  if (nread == 0) {
    s->pass++;
    return do_eof (s);
  }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  AgfaFocus_Scanner *s = handle;

  if (s->reader_pid != -1)
    sanei_thread_kill (s->reader_pid);
  s->scanning = SANE_FALSE;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  AgfaFocus_Scanner *s = handle;

  if (!s->scanning)
    return SANE_STATUS_INVAL;

  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  AgfaFocus_Scanner *s = handle;

  if (!s->scanning)
    return SANE_STATUS_INVAL;

  *fd = s->pipe;
  return SANE_STATUS_GOOD;
}
