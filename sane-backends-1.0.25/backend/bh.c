/* sane - Scanner Access Now Easy.
   Copyright (C) 1999,2000 Tom Martone
   This file is part of a SANE backend for Bell and Howell Copiscan II
   Scanners using the Remote SCSI Controller(RSC).

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
#include "../include/sane/config.h"
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_config.h"

#define BACKEND_NAME bh
#include "../include/sane/sanei_backend.h"
#define BUILD 4

#include "bh.h"

#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define MAX(x,y) ((x)>(y) ? (x) : (y))

static const SANE_Device **devlist = 0;
static int num_devices = 0;
static BH_Device *first_dev = NULL;
static BH_Scanner *first_handle = NULL;
static SANE_Char inquiry_data[255] = "Bell+Howell scanner";
static SANE_Int disable_optional_frames = 0;
static SANE_Int fake_inquiry = 0;

static int allblank(const char *s)
{
  while (s && *s) 
    if (!isspace(*s++))
      return 0;

  return 1;
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

static void
trim_spaces(char *s, size_t n)
{
  for (s += (n-1); n > 0; n--, s--)
    {
      if (*s && !isspace(*s))
	break;
      *s = '\0';
    }
}

static SANE_String_Const
print_devtype (SANE_Byte devtype) 
{
  static SANE_String devtypes[] =
  { 
    "disk",
    "tape",
    "printer",
    "processor",
    "CD-writer",
    "CD-drive",
    "scanner",
    "optical-drive",
    "jukebox",
    "communicator"
  };

  return (devtype > 0 && devtype < NELEMS(devtypes)) ? 
    devtypes[devtype] : 
    "unknown-device";
}

static SANE_String_Const
print_barcodetype (SANE_Int i) 
{
  return (i > 0 && i < NELEMS(barcode_search_bar_list)) ? 
    barcode_search_bar_list[i] : 
    (SANE_String_Const) "unknown";
}

static SANE_String_Const
print_orientation (SANE_Int i) 
{
  switch(i)
    {
    case 0:
    case 7:
      return "vertical upwards";
    case 1:
    case 2:
      return "horizontal right";
    case 3:
    case 4:
      return "vertical downwards";
    case 5:
    case 6:
      return "horizontal left";
    default:
      return "unknown";
    }
}

static SANE_String_Const
print_read_type (SANE_Int i) 
{
  static char buf[32];
  SANE_Int n;

  /* translate BH_SCSI_READ_TYPE_ codes to a human-readable string */
  if (i == BH_SCSI_READ_TYPE_FRONT)
    {
      strcpy(buf, "front page");
    }
  else if (i == BH_SCSI_READ_TYPE_BACK)
    {
      strcpy(buf, "back page");
    }
  else if (i > BH_SCSI_READ_TYPE_FRONT &&
	   i <= BH_SCSI_READ_TYPE_FRONT + NUM_SECTIONS)
    {
      n = i - BH_SCSI_READ_TYPE_FRONT;
      sprintf(buf, "front section %d", n);
    }
  else if (i > BH_SCSI_READ_TYPE_BACK &&
	   i <= BH_SCSI_READ_TYPE_BACK + NUM_SECTIONS)
    {
      n = i - BH_SCSI_READ_TYPE_BACK;
      sprintf(buf, "back section %d", n);
    }
  else if (i == BH_SCSI_READ_TYPE_FRONT_BARCODE)
    {
      strcpy(buf, "front page barcode");
    }
  else if (i == BH_SCSI_READ_TYPE_BACK_BARCODE)
    {
      strcpy(buf, "back page barcode");
    }
  else if (i > BH_SCSI_READ_TYPE_FRONT_BARCODE &&
	   i <= BH_SCSI_READ_TYPE_FRONT_BARCODE + NUM_SECTIONS)
    {
      n = i - BH_SCSI_READ_TYPE_FRONT_BARCODE;
      sprintf(buf, "front barcode section %d", n);
    }
  else if (i > BH_SCSI_READ_TYPE_BACK_BARCODE &&
	   i <= BH_SCSI_READ_TYPE_BACK_BARCODE + NUM_SECTIONS)
    {
      n = i - BH_SCSI_READ_TYPE_BACK_BARCODE;
      sprintf(buf, "back barcode section %d", n);
    }
  else if (i == BH_SCSI_READ_TYPE_FRONT_PATCHCODE)
    {
      strcpy(buf, "front page patchcode");
    }
  else if (i == BH_SCSI_READ_TYPE_BACK_PATCHCODE)
    {
      strcpy(buf, "back page patchcode");
    }
  else if (i > BH_SCSI_READ_TYPE_FRONT_PATCHCODE &&
	   i <= BH_SCSI_READ_TYPE_FRONT_PATCHCODE + NUM_SECTIONS)
    {
      n = i - BH_SCSI_READ_TYPE_FRONT_PATCHCODE;
      sprintf(buf, "front patchcode section %d", n);
    }
  else if (i > BH_SCSI_READ_TYPE_BACK_PATCHCODE &&
	   i <= BH_SCSI_READ_TYPE_BACK_PATCHCODE + NUM_SECTIONS)
    {
      n = i - BH_SCSI_READ_TYPE_BACK_PATCHCODE;
      sprintf(buf, "back patchcode section %d", n);
    }
  else if (i == BH_SCSI_READ_TYPE_FRONT_ICON)
    {
      strcpy(buf, "front page icon");
    }
  else if (i == BH_SCSI_READ_TYPE_BACK_ICON)
    {
      strcpy(buf, "back page icon");
    }
  else if (i == BH_SCSI_READ_TYPE_SENDBARFILE)
    {
      strcpy(buf, "transmit bar/patch codes");
    }
  else
    {
      strcpy(buf, "unknown");
    }

  return buf;
}

static SANE_Int 
get_rotation_id(char *s)
{
  SANE_Int i;

  for (i = 0; rotation_list[i]; i++) 
    if (strcmp(s, rotation_list[i]) == 0) 
      break;

  /* unknown strings are treated as '0' */
  return rotation_list[i] ? i : 0;
}

static SANE_Int 
get_compression_id(char *s)
{
  SANE_Int i;

  for (i = 0; compression_list[i]; i++) 
    if (strcmp(s, compression_list[i]) == 0) 
      break;

  /* unknown strings are treated as 'none' */
  return compression_list[i] ?  i : 0;
}

static SANE_Int 
get_barcode_id(char *s)
{
  SANE_Int i;

  for (i = 0; barcode_search_bar_list[i]; i++) 
    if (strcmp(s, barcode_search_bar_list[i]) == 0) 
      break;

  /* unknown strings are treated as 'none' */
  return barcode_search_bar_list[i] ?  i : 0;
}

static SANE_Int 
get_scan_mode_id(char *s)
{
  SANE_Int i;

  for (i = 0; scan_mode_list[i]; i++) 
    if (strcmp(s, scan_mode_list[i]) == 0) 
      break;

  /* unknown strings are treated as 'lineart' */
  return scan_mode_list[i] ?  i : 0;
}

static SANE_Int 
get_paper_id(char *s)
{
  SANE_Int i;

  for (i = 0; paper_list[i]; i++) 
    if (strcmp(s, paper_list[i]) == 0) 
      break;

  /* unknown strings are treated as 'custom' */
  return paper_list[i] ?  i : 0;
}

static SANE_Int 
get_barcode_search_mode(char *s)
{
  SANE_Int i;

  if (strcmp(s, "horizontal") == 0)
    {
      i = 1;
    }
  else if (strcmp(s, "vertical") == 0)
    {
      i = 2;
    }
  else if (strcmp(s, "vert-horiz") == 0)
    {
      i = 6;
    }
  else if (strcmp(s, "horiz-vert") == 0)
    {
      i = 9;
    }
  else 
    {
      /* unknown strings are treated as 'horiz-vert' */
      DBG(1, "get_barcode_search_mode: unrecognized string `%s'\n", s);
      i = 9;
    }

  return i;
}

static void 
appendStdList(BH_Info *sc, SANE_Int res)
{
  /*  append entry to resolution list - a SANE_WORD_LIST */
  sc->resStdList[sc->resStdList[0]+1] = res;
  sc->resStdList[0]++;
}

static void 
ScannerDump(BH_Scanner *s)
{
  int i;
  BH_Info *info;
  SANE_Device *sdev;

  info = &s->hw->info;
  sdev = &s->hw->sane;

  DBG (1, "SANE Device: '%s' Vendor: '%s' Model: '%s' Type: '%s'\n",
	 sdev->name,
	 sdev->vendor,
	 sdev->model,
	 sdev->type);

  DBG (1, "Type: '%s' Vendor: '%s' Product: '%s' Revision: '%s'\n",
	 print_devtype(info->devtype), 
	 info->vendor, 
	 info->product, 
	 info->revision);

  DBG (1, "Automatic Document Feeder:%s\n",
	 info->canADF ? " <Installed>" : " <Not Installed>");

  DBG (1, "Colors:%s%s\n", info->colorBandW ? " <Black and White>" : "",
	 info->colorHalftone ? " <Halftone>" : "");

  DBG (1, "Data processing:%s%s%s%s%s%s\n",
	 info->canWhiteFrame ? " <White Frame>" : "",
	 info->canBlackFrame ? " <Black Frame>" : "",
	 info->canEdgeExtract ? " <Edge Extraction>" : "",
	 info->canNoiseFilter ? " <Noise Filter>" : "",
	 info->canSmooth ? " <Smooth>" : "",
	 info->canLineBold ? " <Line Bolding>" : "");

  DBG (1, "Compression:%s%s%s\n",
	 info->comprG3_1D ? " <Group 3, 1D>" : "",
	 info->comprG3_2D ? " <Group 3, 2D>" : "",
	 info->comprG4 ? " <Group 4>" : "");

  DBG (1, "Optional Features:%s%s%s%s\n",
	 info->canBorderRecog ? " <Border Recognition>" : "",
	 info->canBarCode ? " <BarCode Decoding>" : "",
	 info->canIcon ? " <Icon Generation>" : "",
	 info->canSection ? " <Section Support>" : "");
 
  DBG (1, "Max bytes per scan-line: %d (%d pixels)\n", 
	 info->lineMaxBytes,
	 info->lineMaxBytes * 8);

  DBG (1, "Basic resolution (X/Y): %d/%d\n",
	 info->resBasicX,
	 info->resBasicY);

  DBG (1, "Maximum resolution (X/Y): %d/%d\n", 
	 info->resMaxX,
	 info->resMaxY);

  DBG (1, "Minimum resolution (X/Y): %d/%d\n", 
	 info->resMinX,
	 info->resMinY);

  DBG (1, "Standard Resolutions:\n");
  for (i = 0; i < info->resStdList[0]; i++)
    DBG (1, " %d\n", info->resStdList[i+1]);

  DBG (1, "Window Width/Height (in basic res) %d/%d (%.2f/%.2f inches)\n",
	 info->winWidth, 
	 info->winHeight, 
	 (info->resBasicX != 0) ? ((float) info->winWidth) / info->resBasicX : 0.0,
	 (info->resBasicY) ? ((float) info->winHeight) / info->resBasicY : 0.0);

  DBG (1, "Summary:%s%s%s\n",
	 info->canDuplex ? "Duplex Scanner" : "Simplex Scanner",
	 info->canACE ? " (ACE capable)" : "",
	 info->canCheckADF ? " (ADF Paper Sensor capable)" : "");
 
  sprintf(inquiry_data, "Vendor: %s Product: %s Rev: %s %s%s%s\n",
	 info->vendor, 
	 info->product, 
	 info->revision,
	 info->canDuplex ? "Duplex Scanner" : "Simplex Scanner",
	 info->canACE ? " (ACE capable)" : "",
	 info->canCheckADF ? " (ADF Paper Sensor capable)" : "");
 
  DBG (5, "autoborder_default=%d\n", info->autoborder_default);
  DBG (5, "batch_default=%d\n", info->batch_default);
  DBG (5, "deskew_default=%d\n", info->deskew_default);
  DBG (5, "check_adf_default=%d\n", info->check_adf_default);
  DBG (5, "duplex_default=%d\n", info->duplex_default);
  DBG (5, "timeout_adf_default=%d\n", info->timeout_adf_default);
  DBG (5, "timeout_manual_default=%d\n", info->timeout_manual_default);
  DBG (5, "control_panel_default=%d\n", info->control_panel_default);

}

static SANE_Status
test_unit_ready (int fd)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (3, "test_unit_ready called\n");

  cmd[0] = BH_SCSI_TEST_UNIT_READY;
  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  return status;
}

static SANE_Status
object_position (BH_Scanner *s)
{
  static SANE_Byte cmd[10];
  SANE_Status status;
  DBG (3, "object_position called\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = BH_SCSI_OBJECT_POSITION;
  cmd[1] = 0x01;
  status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);

  return status;
}

static SANE_Status
read_barcode_data (BH_Scanner *s, FILE *fp)
{
  static SANE_Byte cmd[10];
  SANE_Status status;
  SANE_Int num_found = 0;
  double w, l, x, y, res;
  struct barcode_data buf;
  size_t buf_size = sizeof(buf);
  DBG (3, "read_barcode_data called\n");

  memset (&cmd, 0, sizeof (cmd));
  cmd[0] = BH_SCSI_READ_SCANNED_DATA;
  cmd[2] = s->readlist[s->readptr];
  _lto3b(buf_size, &cmd[6]); /* transfer length */

  s->barcode_not_found = SANE_FALSE;
  do {
    memset (&buf, 0, sizeof(buf));
    status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), &buf, &buf_size);
    if (status != SANE_STATUS_GOOD)
      break;
    if (s->barcode_not_found == SANE_TRUE)
      break;

    num_found++;

    buf.barcodedata[sizeof(buf.barcodedata)-1] = '\0';

    /* calculate the bounding rectangle */
    x = MIN((int) _2btol(buf.posxb), (int) _2btol(buf.posxa));
    y = MIN((int) _2btol(buf.posyb), (int) _2btol(buf.posyd));
    w = MAX((int) _2btol(buf.posxd), (int) _2btol(buf.posxd)) - x;
    l = MAX((int) _2btol(buf.posya), (int) _2btol(buf.posyc)) - y;
    /* convert from pixels to mm */
    res = _OPT_VAL_WORD(s, OPT_RESOLUTION);
    if (res <= 0.0)
      {
	/* avoid divide by zero */
	DBG(1, "read_barcode_data: warning: "
	    "encountered bad resolution value '%f', replacing with '%f'\n",
	    res, 200.0);
	res = 200.0;
      }
    x = x * MM_PER_INCH / res;
    y = y * MM_PER_INCH / res;
    w = w * MM_PER_INCH / res;
    l = l * MM_PER_INCH / res;
    /* add a bit of a border around the edges */
    x = MAX(0.0, x - BH_DECODE_FUDGE);
    y = MAX(0.0, y - BH_DECODE_FUDGE);
    w += (BH_DECODE_FUDGE * 4);
    l += (BH_DECODE_FUDGE * 4);

    /* write the decoded barcode data into the file */
    fprintf(fp, "<barcode>\n <section>%s</section>\n", 
	    print_read_type((int) s->readlist[s->readptr]));
    fprintf(fp, " <type>%s</type>\n <status-flag>%d</status-flag>\n",
	    print_barcodetype((int) _2btol(buf.barcodetype)),
	    (int) _2btol(buf.statusflag));
    fprintf(fp, " <orientation>%s</orientation>\n",
	    print_orientation((int) _2btol(buf.barcodeorientation)));
    fprintf(fp, " <location>\n  <tl><x>%d</x><y>%d</y></tl>\n",
	    (int) _2btol(buf.posxb), (int) _2btol(buf.posyb));
    fprintf(fp, "  <tr><x>%d</x><y>%d</y></tr>\n",
	    (int) _2btol(buf.posxd), (int) _2btol(buf.posyd));
    fprintf(fp, "  <bl><x>%d</x><y>%d</y></bl>\n",
	    (int) _2btol(buf.posxa), (int) _2btol(buf.posya));
    fprintf(fp, "  <br><x>%d</x><y>%d</y></br>\n </location>\n",
	    (int) _2btol(buf.posxc), (int) _2btol(buf.posyc));
    fprintf(fp, " <rectangle>%.2fx%.2f+%.2f+%.2f</rectangle>\n",
	    w, l, x, y);
    fprintf(fp, " <search-time>%d</search-time>\n <length>%d</length>\n",
	    (int) _2btol(buf.barcodesearchtime),
	    (int) buf.barcodelen);
    fprintf(fp, " <data>%s</data>\n</barcode>\n",
	    buf.barcodedata);
  } while (num_found <= BH_DECODE_TRIES);

  DBG (3, "read_barcode_data: found %d barcodes, returning %s\n", 
       num_found, sane_strstatus(status));

  return status;
}

static SANE_Status
read_icon_data (BH_Scanner *s)
{
  static SANE_Byte cmd[10];
  SANE_Status status;
  struct icon_data buf;
  size_t buf_size = sizeof(buf);
  DBG (3, "read_icon_data called\n");

  memset (&cmd, 0, sizeof (cmd));
  cmd[0] = BH_SCSI_READ_SCANNED_DATA;
  cmd[2] = s->readlist[s->readptr];
  _lto3b(buf_size, &cmd[6]); /* transfer length */

  memset (&buf, 0, sizeof(buf));

  status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), &buf, &buf_size);
  
  /* set the fields in the scanner handle for later reference */
  s->iconwidth = _4btol(buf.iconwidth);
  s->iconlength = _4btol(buf.iconlength);

  DBG(3, "read_icon_data: windowwidth:%lu, windowlength:%lu\n",
      _4btol(buf.windowwidth),
      _4btol(buf.windowlength));
  DBG(3, "read_icon_data: iconwidth:%lu, iconlength:%lu, iconwidth(bytes):%lu\n",
      _4btol(buf.iconwidth),
      _4btol(buf.iconlength),
      _4btol(buf.iconwidthbytes));
  DBG(3, "read_icon_data: bitordering:%02x, icondatalen:%lu\n", 
      buf.bitordering,
      _4btol(buf.icondatalen));

  DBG (3, "read_icon_data returning %d\n", status);

  return status;
}

static SANE_Status
read_barfile (BH_Scanner *s, void *buf, size_t *buf_size)
{
  SANE_Status status = SANE_STATUS_GOOD;
  size_t nread;
  DBG (3, "read_barfile called (%lu bytes)\n", (u_long) *buf_size);

  if (s->barf != NULL)
    {
      /* this function needs to set InvalidBytes so it looks 
       * like a B&H scsi EOF
       */
      if ((nread = fread(buf, 1, *buf_size, s->barf)) < *buf_size)
	{
	  /* set InvalidBytes */
	  s->InvalidBytes = *buf_size - nread;

	  if (ferror(s->barf))
	    {
	      status = SANE_STATUS_IO_ERROR;
	      fclose(s->barf);
	      s->barf = NULL;
	      unlink(s->barfname);
	    }
	  else if (feof(s->barf))
	    {
	      /* it also needs to close the file and delete it when EOF is
	       * reached.
	       */
	      fclose(s->barf);
	      s->barf = NULL;
	      unlink(s->barfname);
	    }
	}
    }
  else
    {
      /* set InvalidBytes */
      s->InvalidBytes = *buf_size;
    }

  return status;
}

static SANE_Status
read_data (BH_Scanner *s, void *buf, size_t *buf_size)
{
  static SANE_Byte cmd[10];
  SANE_Status status;
  DBG (3, "read_data called (%lu bytes)\n", (u_long) *buf_size);

  if (s->readlist[s->readptr] == BH_SCSI_READ_TYPE_SENDBARFILE)
    {
      /* call special barcode data read function. */
      status = read_barfile(s, buf, buf_size);
    }
  else
    {
      memset (&cmd, 0, sizeof (cmd));
      cmd[0] = BH_SCSI_READ_SCANNED_DATA;
      cmd[2] = s->readlist[s->readptr];
      _lto3b(*buf_size, &cmd[6]); /* transfer length */

      status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), buf, buf_size);
    }

  return status;
}

static SANE_Status
mode_select_measurement (BH_Scanner *s)
{
  static struct {
    SANE_Byte cmd[6];
    struct mode_page_03 mp;
  } select_cmd;
  SANE_Status status;

  DBG (3, "mode_select_measurement called (bmu:%d mud:%d)\n",
       s->bmu, s->mud);

  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd[0] = BH_SCSI_MODE_SELECT;
  select_cmd.cmd[1] = 0x10;
  select_cmd.cmd[4] = sizeof(select_cmd.mp);

  select_cmd.mp.pagecode = BH_MODE_MEASUREMENT_PAGE_CODE;
  select_cmd.mp.paramlen = 0x06;
  select_cmd.mp.bmu = s->bmu;
  _lto2b(s->mud, select_cmd.mp.mud);

  status = sanei_scsi_cmd (s->fd, &select_cmd, sizeof (select_cmd), 0, 0);

  return status;
}

static SANE_Status
mode_select_timeout (BH_Scanner *s)
{
  static struct {
    SANE_Byte cmd[6];
    struct mode_page_20 mp;
  } select_cmd;
  SANE_Status status;

  DBG (3, "mode_select_timeout called\n");

  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd[0] = BH_SCSI_MODE_SELECT;
  select_cmd.cmd[1] = 0x10;
  select_cmd.cmd[4] = sizeof(select_cmd.mp);

  select_cmd.mp.pagecode = BH_MODE_TIMEOUT_PAGE_CODE;
  select_cmd.mp.paramlen = 0x06;
  select_cmd.mp.timeoutmanual = _OPT_VAL_WORD(s, OPT_TIMEOUT_MANUAL);
  select_cmd.mp.timeoutadf = _OPT_VAL_WORD(s, OPT_TIMEOUT_ADF);

  status = sanei_scsi_cmd (s->fd, &select_cmd, sizeof (select_cmd), 0, 0);

  return status;
}

static SANE_Status
mode_select_icon (BH_Scanner *s)
{
  static struct {
    SANE_Byte cmd[6];
    struct mode_page_21 mp;
  } select_cmd;
  SANE_Status status;

  DBG (3, "mode_select_icon called\n");

  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd[0] = BH_SCSI_MODE_SELECT;
  select_cmd.cmd[1] = 0x10;
  select_cmd.cmd[4] = sizeof(select_cmd.mp);

  select_cmd.mp.pagecode = BH_MODE_ICON_PAGE_CODE;
  select_cmd.mp.paramlen = 0x06;
  _lto2b(_OPT_VAL_WORD(s, OPT_ICON_WIDTH), select_cmd.mp.iconwidth);
  _lto2b(_OPT_VAL_WORD(s, OPT_ICON_LENGTH), select_cmd.mp.iconlength);

  status = sanei_scsi_cmd (s->fd, &select_cmd, sizeof (select_cmd), 0, 0);

  return status;
}

static SANE_Status
mode_select_barcode_priority (BH_Scanner *s)
{
  static struct {
    SANE_Byte cmd[6];
    struct mode_page_30 mp;
  } select_cmd;
  SANE_Status status;
  int i;

  DBG (3, "mode_select_barcode_priority called\n");

  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd[0] = BH_SCSI_MODE_SELECT;
  select_cmd.cmd[1] = 0x10;
  select_cmd.cmd[4] = sizeof(select_cmd.mp);

  select_cmd.mp.pagecode = BH_MODE_BARCODE_PRIORITY_PAGE_CODE;
  select_cmd.mp.paramlen = 0x06;

  for (i = 0; i < NUM_SEARCH_BARS; i++)
    {
      /* anything after a 'none' is ignored */
      if ((select_cmd.mp.priority[i] = s->search_bars[i]) == 0) break;
    }

  status = sanei_scsi_cmd (s->fd, &select_cmd, sizeof (select_cmd), 0, 0);

  return status;
}

static SANE_Status
mode_select_barcode_param1 (BH_Scanner *s)
{
  static struct {
    SANE_Byte cmd[6];
    struct mode_page_31 mp;
  } select_cmd;
  SANE_Status status;

  DBG (3, "mode_select_barcode_param1 called\n");

  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd[0] = BH_SCSI_MODE_SELECT;
  select_cmd.cmd[1] = 0x10;
  select_cmd.cmd[4] = sizeof(select_cmd.mp);

  select_cmd.mp.pagecode = BH_MODE_BARCODE_PARAM1_PAGE_CODE;
  select_cmd.mp.paramlen = 0x06;

  _lto2b((SANE_Int)_OPT_VAL_WORD_THOUSANDTHS(s, OPT_BARCODE_HMIN), select_cmd.mp.minbarheight);
  select_cmd.mp.searchcount = _OPT_VAL_WORD(s, OPT_BARCODE_SEARCH_COUNT);
  select_cmd.mp.searchmode = 
    get_barcode_search_mode(_OPT_VAL_STRING(s, OPT_BARCODE_SEARCH_MODE));
  _lto2b(_OPT_VAL_WORD(s, OPT_BARCODE_SEARCH_TIMEOUT), select_cmd.mp.searchtimeout);

  status = sanei_scsi_cmd (s->fd, &select_cmd, sizeof (select_cmd), 0, 0);

  return status;
}

static SANE_Status
mode_select_barcode_param2 (BH_Scanner *s)
{
  static struct {
    SANE_Byte cmd[6];
    struct mode_page_32 mp;
  } select_cmd;
  SANE_Status status;
  size_t len;

  DBG (3, "mode_select_barcode_param2 called\n");

  /* first we'll do a mode sense, then we'll overwrite with
   * our new values, and then do a mode select
   */
  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd[0] = BH_SCSI_MODE_SENSE;
  select_cmd.cmd[2] = BH_MODE_BARCODE_PARAM2_PAGE_CODE;
  select_cmd.cmd[4] = sizeof(select_cmd.mp);

  len = sizeof(select_cmd.mp);
  status = sanei_scsi_cmd (s->fd, &select_cmd.cmd, sizeof (select_cmd.cmd), 
			   &select_cmd.mp, &len);

  if (status == SANE_STATUS_GOOD) 
    {
      DBG(8, "mode_select_barcode_param2: sensed values: relmax:%d barmin:%d barmax:%d\n",
	  (int) _2btol(select_cmd.mp.relmax),
	  (int) _2btol(select_cmd.mp.barmin),
	  (int) _2btol(select_cmd.mp.barmax));

      memset (&select_cmd.cmd, 0, sizeof (select_cmd.cmd));
      select_cmd.cmd[0] = BH_SCSI_MODE_SELECT;
      select_cmd.cmd[1] = 0x10;
      select_cmd.cmd[4] = sizeof(select_cmd.mp);

      select_cmd.mp.modedatalen = 0x00;
      select_cmd.mp.mediumtype = 0x00;
      select_cmd.mp.devicespecificparam = 0x00;
      select_cmd.mp.blockdescriptorlen = 0x00;

      select_cmd.mp.pagecode = BH_MODE_BARCODE_PARAM2_PAGE_CODE;
      select_cmd.mp.paramlen = 0x06;

      /* only overwrite the default values if the option is non-zero */
      if (_OPT_VAL_WORD(s, OPT_BARCODE_RELMAX) != 0)
	{
	  _lto2b(_OPT_VAL_WORD(s, OPT_BARCODE_RELMAX), select_cmd.mp.relmax);
	}
      if (_OPT_VAL_WORD(s, OPT_BARCODE_BARMIN) != 0)
	{
	  _lto2b(_OPT_VAL_WORD(s, OPT_BARCODE_BARMIN), select_cmd.mp.barmin);
	}
      if (_OPT_VAL_WORD(s, OPT_BARCODE_BARMAX) != 0)
	{
	  _lto2b(_OPT_VAL_WORD(s, OPT_BARCODE_BARMAX), select_cmd.mp.barmax);
	}

      DBG(8, "mode_select_barcode_param2: param values: relmax:%d barmin:%d barmax:%d\n",
	  (int) _OPT_VAL_WORD(s, OPT_BARCODE_RELMAX),
	  (int) _OPT_VAL_WORD(s, OPT_BARCODE_BARMIN),
	  (int) _OPT_VAL_WORD(s, OPT_BARCODE_BARMAX));

      DBG(8, "mode_select_barcode_param2: select values: relmax:%d barmin:%d barmax:%d\n",
	  (int) _2btol(select_cmd.mp.relmax),
	  (int) _2btol(select_cmd.mp.barmin),
	  (int) _2btol(select_cmd.mp.barmax));

      status = sanei_scsi_cmd (s->fd, &select_cmd, sizeof (select_cmd), 0, 0);
    }

  return status;
}

static SANE_Status
mode_select_barcode_param3 (BH_Scanner *s)
{
  static struct {
    SANE_Byte cmd[6];
    struct mode_page_33 mp;
  } select_cmd;
  SANE_Status status;
  size_t len;

  DBG (3, "mode_select_barcode_param3 called\n");

  /* first we'll do a mode sense, then we'll overwrite with
   * our new values, and then do a mode select
   */
  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd[0] = BH_SCSI_MODE_SENSE;
  select_cmd.cmd[2] = BH_MODE_BARCODE_PARAM3_PAGE_CODE;
  select_cmd.cmd[4] = sizeof(select_cmd.mp);

  len = sizeof(select_cmd.mp);
  status = sanei_scsi_cmd (s->fd, &select_cmd.cmd, sizeof (select_cmd.cmd), 
			   &select_cmd.mp, &len);

  if (status == SANE_STATUS_GOOD) 
    {
      DBG(8, "mode_select_barcode_param3: sensed values: contrast:%d patchmode:%d\n",
	  (int) _2btol(select_cmd.mp.barcodecontrast),
	  (int) _2btol(select_cmd.mp.patchmode));

      memset (&select_cmd.cmd, 0, sizeof (select_cmd.cmd));
      select_cmd.cmd[0] = BH_SCSI_MODE_SELECT;
      select_cmd.cmd[1] = 0x10;
      select_cmd.cmd[4] = sizeof(select_cmd.mp);

      select_cmd.mp.modedatalen = 0x00;
      select_cmd.mp.mediumtype = 0x00;
      select_cmd.mp.devicespecificparam = 0x00;
      select_cmd.mp.blockdescriptorlen = 0x00;

      select_cmd.mp.pagecode = BH_MODE_BARCODE_PARAM3_PAGE_CODE;
      select_cmd.mp.paramlen = 0x06;

      /* only overwrite the default values if the option is non-zero */
      if (_OPT_VAL_WORD(s, OPT_BARCODE_CONTRAST) != 0)
	{
	  _lto2b(_OPT_VAL_WORD(s, OPT_BARCODE_CONTRAST), select_cmd.mp.barcodecontrast);
	}
      if (_OPT_VAL_WORD(s, OPT_BARCODE_PATCHMODE) != 0)
	{
	  _lto2b(_OPT_VAL_WORD(s, OPT_BARCODE_PATCHMODE), select_cmd.mp.patchmode);
	}

      DBG(8, "mode_select_barcode_param3: param values: contrast:%d patchmode:%d\n",
	  (int) _OPT_VAL_WORD(s, OPT_BARCODE_CONTRAST),
	  (int) _OPT_VAL_WORD(s, OPT_BARCODE_PATCHMODE));

      DBG(8, "mode_select_barcode_param3: select values: contrast:%d patchmode:%d\n",
	  (int) _2btol(select_cmd.mp.barcodecontrast),
	  (int) _2btol(select_cmd.mp.patchmode));

      status = sanei_scsi_cmd (s->fd, &select_cmd, sizeof (select_cmd), 0, 0);
    }

  return status;
}

static SANE_Status
inquiry (int fd, void *buf, size_t *buf_size, SANE_Byte evpd, SANE_Byte page_code)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (3, "inquiry called\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = BH_SCSI_INQUIRY;
  cmd[1] = evpd;
  cmd[2] = page_code;
  cmd[4] = *buf_size;

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  return status;
}

static SANE_Status
set_window (BH_Scanner *s, SANE_Byte batchmode)
{
  static struct {
    SANE_Byte cmd[10];
    SANE_Byte hdr[8];
    struct window_data window;
  } set_window_cmd;
  SANE_Status status;
  SANE_Int width, length, i, format, rotation, deskew ;

  DBG (3, "set_window called\n");

  /* set to thousandths for set_window */
  s->bmu = BH_UNIT_INCH;
  s->mud = 1000;
  status = mode_select_measurement(s);
  if (status != SANE_STATUS_GOOD)
    return status;

  memset (&set_window_cmd, 0, sizeof (set_window_cmd));
  set_window_cmd.cmd[0] = BH_SCSI_SET_WINDOW;
  DBG(3, "set_window: sizeof(hdr) %d, sizeof(window): %d\n", 
      (int)sizeof(set_window_cmd.hdr), (int)sizeof(set_window_cmd.window));

  _lto3b(sizeof(set_window_cmd.hdr) + sizeof(set_window_cmd.window), 
	 &set_window_cmd.cmd[6]);

  _lto2b(256, &set_window_cmd.hdr[6]);

  set_window_cmd.window.windowid = 0;
  set_window_cmd.window.autoborder = _OPT_VAL_WORD(s, OPT_AUTOBORDER);
  DBG (5, "autoborder set to=%d\n", set_window_cmd.window.autoborder);
  _lto2b(_OPT_VAL_WORD(s, OPT_RESOLUTION), set_window_cmd.window.xres);
  _lto2b(_OPT_VAL_WORD(s, OPT_RESOLUTION), set_window_cmd.window.yres);
  _lto4b((int) _OPT_VAL_WORD_THOUSANDTHS(s, OPT_TL_X), set_window_cmd.window.ulx);
  _lto4b((int) _OPT_VAL_WORD_THOUSANDTHS(s, OPT_TL_Y), set_window_cmd.window.uly);

  width = (SANE_Int) (_OPT_VAL_WORD_THOUSANDTHS(s, OPT_BR_X) - 
	 _OPT_VAL_WORD_THOUSANDTHS(s, OPT_TL_X));
  length = (SANE_Int) (_OPT_VAL_WORD_THOUSANDTHS(s, OPT_BR_Y) - 
	 _OPT_VAL_WORD_THOUSANDTHS(s, OPT_TL_Y));

  _lto4b(width, set_window_cmd.window.windowwidth);
  _lto4b(length, set_window_cmd.window.windowlength);

  /* brightness (1-255) 0 is default, aka 128.  Ignored with ACE scanners */
  set_window_cmd.window.brightness = _OPT_VAL_WORD(s, OPT_BRIGHTNESS);
  /* threshold (1-255) 0 is default, aka 128.  Ignored with ACE scanners */
  set_window_cmd.window.threshold = _OPT_VAL_WORD(s, OPT_THRESHOLD);
  /*!!! contrast (not used) */
  /*!!! set_window_cmd.window.contrast = _OPT_VAL_WORD(s, OPT_CONTRAST); */
  /* imagecomposition 0x00 lineart, 0x01 dithered/halftone, 0x02 grayscale*/
  set_window_cmd.window.imagecomposition = 
    get_scan_mode_id(_OPT_VAL_STRING(s, OPT_SCAN_MODE));
  
  set_window_cmd.window.bitsperpixel = 0x01;
  /*!!! halftone code (not used) */
  /*!!! halftone id (not used) */

  set_window_cmd.window.paddingtype = 0x03; /* truncate byte */
  if (_OPT_VAL_WORD(s, OPT_NEGATIVE) == SANE_TRUE) {
    /* reverse image format (valid when bitsperpixel=1) 
     * 0x00 normal, 0x01 reversed.  This is bit 7 of paddingtype.
     */
    set_window_cmd.window.paddingtype |= 0x80;
  }

  set_window_cmd.window.bitordering[0] = 0x00;

  /* we must always sent plain gray data in preview mode */
  format = (_OPT_VAL_WORD(s, OPT_PREVIEW)) ?
    BH_COMP_NONE :
    get_compression_id(_OPT_VAL_STRING(s, OPT_COMPRESSION));

  switch (format)
    {
    case BH_COMP_G31D:
      set_window_cmd.window.compressiontype = 0x01;
      set_window_cmd.window.compressionarg = 0x00;
      set_window_cmd.window.bitordering[1] = 0x01; /* Bit ordering LSB */
      break;
    case BH_COMP_G32D:
      set_window_cmd.window.compressiontype = 0x02;
      set_window_cmd.window.compressionarg = 0x04;
      set_window_cmd.window.bitordering[1] = 0x01; /* Bit ordering LSB */
      break;
    case BH_COMP_G42D:
      set_window_cmd.window.compressiontype = 0x03;
      set_window_cmd.window.compressionarg = 0x00;
      set_window_cmd.window.bitordering[1] = 0x01; /* Bit ordering LSB */
      break;
    case BH_COMP_NONE:
    default:
      set_window_cmd.window.compressiontype = 0x00;
      set_window_cmd.window.compressionarg = 0x00;
      set_window_cmd.window.bitordering[1] = 0x00; /* n/a */
      break;
    }

  /* rotation and deskew settings, if autoborder is turned on */
  if(set_window_cmd.window.autoborder){ /*--- setting byte 46 of the window descriptor block only works with autoborder */
    rotation = get_rotation_id(_OPT_VAL_STRING(s, OPT_ROTATION));
    if (_OPT_VAL_WORD(s, OPT_DESKEW) == SANE_TRUE) deskew = BH_DESKEW_ENABLE;
    else deskew = BH_DESKEW_DISABLE;
    set_window_cmd.window.border_rotation = ( rotation | deskew );  /*--- deskew assumes autoborder */
  }

  /* remote - 0x00 ACE set in window; 0x01 ACE set by control panel */
  set_window_cmd.window.remote = _OPT_VAL_WORD(s, OPT_CONTROL_PANEL);
  if (set_window_cmd.window.remote == 0x00) {
    /* acefunction (ignored on non-ACE scanners) */
    set_window_cmd.window.acefunction = _OPT_VAL_WORD(s, OPT_ACE_FUNCTION);
    /* acesensitivity (ignored on non-ACE scanners) */
    set_window_cmd.window.acesensitivity = _OPT_VAL_WORD(s, OPT_ACE_SENSITIVITY);
  }

  set_window_cmd.window.batchmode = batchmode;

  /* fill in the section descriptor blocks */
  for (i = 0; i < s->num_sections; i++)
    {
      BH_SectionBlock *b;

      b = &set_window_cmd.window.sectionblock[i];

      _lto4b(s->sections[i].left, b->ul_x);
      _lto4b(s->sections[i].top, b->ul_y);
      _lto4b(s->sections[i].width, b->width);
      _lto4b(s->sections[i].length, b->length);
      b->compressiontype = s->sections[i].compressiontype;
      b->compressionarg = s->sections[i].compressionarg;
    }

  status = sanei_scsi_cmd (s->fd, &set_window_cmd, sizeof (set_window_cmd), 0, 0);
  DBG (5, "sanei_scsi_cmd executed, status=%d\n", status );
  if (status != SANE_STATUS_GOOD)
    return status;

  /* set to points for reading */
  s->bmu = BH_UNIT_POINT;
  s->mud = 1;
  status = mode_select_measurement(s);

  return status;
}

static SANE_Status
get_window (BH_Scanner *s, SANE_Int *w, SANE_Int *h, SANE_Bool backpage)
{
  SANE_Byte cmd[10];
  static struct {
    SANE_Byte hdr[8];
    struct window_data window;
  } get_window_data;
  SANE_Status status;
  SANE_Int x, y, i = 0, get_window_delay = 1;
  SANE_Bool autoborder;
  size_t len;

  DBG (3, "get_window called\n");

  autoborder = _OPT_VAL_WORD(s, OPT_AUTOBORDER) == 1;

  while (1) 
    {
      i++;
      memset (&cmd, 0, sizeof (cmd));
      memset (&get_window_data, 0, sizeof (get_window_data));

      cmd[0] = BH_SCSI_GET_WINDOW;
      _lto3b(sizeof(get_window_data), &cmd[6]);

      _lto2b(256, &get_window_data.hdr[6]);

      get_window_data.window.windowid = (backpage == SANE_TRUE) ? 1 : 0;

      len = sizeof(get_window_data);
      status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 
			       &get_window_data, &len);
      if (status == SANE_STATUS_GOOD)
	{
	  x =_4btol(get_window_data.window.ulx);
	  y =_4btol(get_window_data.window.uly);
	  *w =_4btol(get_window_data.window.windowwidth);
	  *h =_4btol(get_window_data.window.windowlength);

	  if (autoborder)
	    {
	      /* we try repeatedly until we get the autoborder bit set */
	      if (get_window_data.window.autoborder != 1 &&
		  i < BH_AUTOBORDER_TRIES)
		{
	          DBG (5, "waiting %d second[s], try: %d\n",get_window_delay,i);
		  sleep(get_window_delay);  /*--- page 4-5 of B&H Copiscan 8000 ESC OEM Tech Manual */
                                            /*--- requires at least 50ms wait between each GET WINDOW command */
                                            /*--- experience shows that this can take 3 to 4 seconds */
		  continue;
		}
	      if (get_window_data.window.autoborder != 1)
		{
		  DBG(1, "Automatic Border Detection not done within %d tries\n",
		      BH_AUTOBORDER_TRIES);
		  status = SANE_STATUS_IO_ERROR;
		}
             DBG (0, "page dimension: wide:%d high:%d \n",*w,*h);
	    }
	  DBG (3, "*** Window size: %dx%d+%d+%d\n", *w, *h, x, y);
	  DBG (5, "*** get_window found autoborder=%02xh\n", get_window_data.window.autoborder);
	  DBG (5, "*** get_window found border_rotation=%02xh\n", get_window_data.window.border_rotation);
	}

      /* we are 'outta here' */
      break;
    }

  return status;
}

static SANE_Status
get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  BH_Scanner *s = handle;
  SANE_Int width, length, res, comp;
  double br_x, tl_x, br_y, tl_y;
  SANE_Frame format;

  DBG(3, "get_parameters called\n");
  
  memset (&s->params, 0, sizeof (s->params));
    
  res = _OPT_VAL_WORD(s, OPT_RESOLUTION);

  /* make best-effort guess at what parameters will look like once
     the scan starts.  */

  br_x = _OPT_VAL_WORD_THOUSANDTHS(s, OPT_BR_X);
  br_y = _OPT_VAL_WORD_THOUSANDTHS(s, OPT_BR_Y);
  tl_x = _OPT_VAL_WORD_THOUSANDTHS(s, OPT_TL_X);
  tl_y = _OPT_VAL_WORD_THOUSANDTHS(s, OPT_TL_Y);

  width = (br_x - tl_x + 1) * res / 1000.0;
  length = (br_y - tl_y + 1) * res / 1000.0;

  /* figure out the default image format for front/back pages */
  comp = get_compression_id(_OPT_VAL_STRING(s, OPT_COMPRESSION));
  switch (comp)
    {
    case BH_COMP_G31D:
      format = SANE_FRAME_G31D;
      break;
    case BH_COMP_G32D:
      format = SANE_FRAME_G32D;
      break;
    case BH_COMP_G42D:
      format = SANE_FRAME_G42D;
      break;
    case BH_COMP_NONE:
    default:
      format = SANE_FRAME_GRAY;
      break;
    }

  if (s->scanning)
    {
      SANE_Int w, l, status;
      SANE_Byte itemtype;

      itemtype = s->readlist[s->readptr];
      /* update parameters based on the current item */

      status = SANE_STATUS_GOOD;
      if (itemtype == BH_SCSI_READ_TYPE_FRONT) 
	{
	  DBG (3, "get_parameters: sending GET WINDOW (front)\n");
	  status = get_window (s, &w, &l, SANE_FALSE);
	  if (status == SANE_STATUS_GOOD)
	    {
	      width = w;
	      length = l;
	    }
	}
      else if (itemtype == BH_SCSI_READ_TYPE_BACK)
	{
	  DBG (3, "get_parameters: sending GET WINDOW (back)\n");
	  status = get_window (s, &w, &l, SANE_TRUE);
	  if (status == SANE_STATUS_GOOD)
	    {
	      width = w;
	      length = l;
	    }
	}
      else if (itemtype == BH_SCSI_READ_TYPE_FRONT_ICON ||
	       itemtype == BH_SCSI_READ_TYPE_BACK_ICON)
	{
	  /* the icon is never compressed */
	  format = SANE_FRAME_GRAY;
	  width = s->iconwidth;
	  length = s->iconlength;
	}
      else if (itemtype > BH_SCSI_READ_TYPE_FRONT &&
	       itemtype <= (BH_SCSI_READ_TYPE_FRONT + NUM_SECTIONS))
	{
	  /* a front section */
	  SANE_Int sectnum = itemtype - BH_SCSI_READ_TYPE_FRONT;

	  format = s->sections[sectnum - 1].format;
	  /* convert from thousandths to pixels */
	  width = s->sections[sectnum - 1].width * res / 1000.0;
	  length = s->sections[sectnum - 1].length * res / 1000.0;
	}
      else if (itemtype > BH_SCSI_READ_TYPE_BACK &&
	       itemtype <= (BH_SCSI_READ_TYPE_BACK + NUM_SECTIONS))
	{
	  /* a back section */
	  SANE_Int sectnum = itemtype - BH_SCSI_READ_TYPE_BACK;

	  format = s->sections[sectnum - 1].format;
	  /* convert from thousandths to pixels */
	  width = s->sections[sectnum - 1].width * res / 1000.0;
	  length = s->sections[sectnum - 1].length * res / 1000.0;
	}
      else if ( (itemtype >= BH_SCSI_READ_TYPE_BACK_BARCODE &&
		 itemtype <= (BH_SCSI_READ_TYPE_BACK_BARCODE + NUM_SECTIONS)) || 
		(itemtype >= BH_SCSI_READ_TYPE_FRONT_BARCODE &&
		 itemtype <= (BH_SCSI_READ_TYPE_FRONT_BARCODE + NUM_SECTIONS)) )
	{
	  /* decoded barcode data */
	  format = SANE_FRAME_TEXT;
	  width = 8;
	  length = -1;
	}
      else if (itemtype == BH_SCSI_READ_TYPE_SENDBARFILE)
	{
	  /* decoded barcode data file */
	  format = SANE_FRAME_TEXT;
	  width = 8;
	  length = -1;
	}
      else
	{
	  format = SANE_FRAME_GRAY;
	  width = 8;
	  length = -1;
	  DBG(1, "get_parameters: unrecognized read itemtype: %d\n",
	      itemtype);
	}

      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "get_parameters: failed\n");
	  return status;
	}
    }

  if (res <= 0 || width <= 0)
    {
      DBG(1, "get_parameters:illegal parameters res=%d, width=%d, length=%d\n",
	  res, width, length);
      return SANE_STATUS_INVAL;
    }
  
  /* we disable our compression/barcode formats in preview as well
   * as with the disable_optional_frames configuration option.  NOTE:
   * we may still be delivering 'wierd' data and lying about it being _GRAY!
   */
  if (format != SANE_FRAME_GRAY &&
      (_OPT_VAL_WORD(s, OPT_PREVIEW) || disable_optional_frames))
    {
      DBG(1, "get_parameters: warning: delivering %s data as gray",
	  sane_strframe(format));
      format = SANE_FRAME_GRAY;
    }

  s->params.format = format;
  s->params.depth = 1;
  s->params.last_frame = SANE_TRUE;
  s->params.pixels_per_line = width;
  s->params.lines = length;
  s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
  /* The Bell and Howell truncates to the byte */
  s->params.pixels_per_line = s->params.bytes_per_line * 8;

  if (params)
    *params = s->params;

  DBG (1, "get_parameters: format=%d, pixels/line=%d, bytes/line=%d, "
       "lines=%d, dpi=%d\n", 
       (int) s->params.format, 
       s->params.pixels_per_line, 
       s->params.bytes_per_line,
       s->params.lines, 
       res);

  return SANE_STATUS_GOOD;
}

static SANE_Status
section_parse(const char *val, BH_Section *sect, SANE_Int res, SANE_Int comp)
{
  SANE_Status status = SANE_STATUS_INVAL;
  char buf[255+1], *x, *y, *w, *l, *f, *ep;
  const char *seps = "x+:";
  double mm, fpixels;
  u_long pixels;

  DBG(3, "section_parse called\n");

  /* a section option looks something like this:
   * <width>x<length>+<tl-x>+<tl-y>:<functioncodes>
   * Example:
   * 76.2x25.4+50.8+0:frontbar:back:front
   * the width, length, tl-x, and tl-y are in mm.
   * the function codes are one or more of:
   * front, back, frontbar, backbar, frontpatch, backpatch
   */
  if (strlen(val) > sizeof(buf) - 1)
    {
      DBG(1, "section_parse: option string too long\n");
      status = SANE_STATUS_INVAL;
    }
  else
    {
      do {
	strcpy(buf, val);

	x = y = w = l = f = NULL;
	w = strtok(buf, seps);
	if (w) l = strtok(NULL, seps);
	if (l) x = strtok(NULL, seps);
	if (x) y = strtok(NULL, seps);
	if (y) f = strtok(NULL, seps);
	if (!x || !y || !w || !l) break;

	mm = strtod(x, &ep); 
	if (*ep != '\0' || errno == ERANGE || mm < 0.0) break;
	sect->left = mm * 1000.0 / MM_PER_INCH;

	mm = strtod(y, &ep);
	if (*ep != '\0' || errno == ERANGE || mm < 0.0) break;
	sect->top = mm * 1000.0 / MM_PER_INCH;

	mm = strtod(w, &ep);
	if (*ep != '\0' || errno == ERANGE || mm < 0.0) break;
	sect->width = mm * 1000.0 / MM_PER_INCH;
	/* the window width must be truncated to 16 bit points */
	fpixels = sect->width * res / 1000.0;
	pixels = fpixels / 16;
	sect->width = pixels * 16 * 1000 / res;

	mm = strtod(l, &ep);
	if (*ep != '\0' || errno == ERANGE || mm < 0.0) break;
	sect->length = mm * 1000.0 / MM_PER_INCH;

	status = SANE_STATUS_GOOD;
	while (f)
	  {
	    /* parse the function modifiers and set flags */
	    if (strcmp(f, "front") == 0)
	      sect->flags |= BH_SECTION_FRONT_IMAGE;
	    else if (strcmp(f, "frontbar") == 0)
	      sect->flags |= BH_SECTION_FRONT_BAR;
	    else if (strcmp(f, "frontpatch") == 0)
	      sect->flags |= BH_SECTION_FRONT_PATCH;
	    else if (strcmp(f, "back") == 0)
		sect->flags |= BH_SECTION_BACK_IMAGE;
	    else if (strcmp(f, "backbar") == 0)
		sect->flags |= BH_SECTION_BACK_BAR;
	    else if (strcmp(f, "backpatch") == 0)
		sect->flags |= BH_SECTION_BACK_PATCH;
	    else if (strcmp(f, "g42d") == 0)
		comp = BH_COMP_G42D;
	    else if (strcmp(f, "g32d") == 0)
		comp = BH_COMP_G32D;
	    else if (strcmp(f, "g31d") == 0)
		comp = BH_COMP_G31D;
	    else if (strcmp(f, "none") == 0)
		comp = BH_COMP_NONE;
	    else
	      DBG(1, "section_parse: ignoring unrecognized function "
		  "code '%s'\n", f);

	    f = strtok(NULL, seps);
	  }

	switch (comp)
	  {
	  case BH_COMP_G31D:
	    sect->compressiontype = 0x01;
	    sect->compressionarg = 0x00;
	    sect->format = SANE_FRAME_G31D;
	    break;
	  case BH_COMP_G32D:
	    sect->compressiontype = 0x02;
	    sect->compressionarg = 0x04;
	    sect->format = SANE_FRAME_G32D;
	    break;
	  case BH_COMP_G42D:
	    sect->compressiontype = 0x03;
	    sect->compressionarg = 0x00;
	    sect->format = SANE_FRAME_G42D;
	    break;
	  case BH_COMP_NONE:
	  default:
	    sect->compressiontype = 0x00;
	    sect->compressionarg = 0x00;
	    sect->format = SANE_FRAME_GRAY;
	    break;
	  }

	DBG(3, "section_parse: converted '%s' (mm) to "
	    "%ldx%ld+%ld+%ld (thousandths) "
	    "flags=%02x compression=[%d,%d] frame=%s\n", 
	    val, 
	    sect->width, sect->length, sect->left, sect->top,
	    sect->flags, 
	    sect->compressiontype, sect->compressionarg,
	    sane_strframe(sect->format));

      } while (0); /* perform 'loop' once */
    }

  return status;
}

static SANE_Status
setup_sections (BH_Scanner *s, const char *val)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int sectnum = 0;
  char buf[255+1], *section;

  DBG(3, "setup_sections called\n");

  memset(s->sections, '\0', sizeof(s->sections));
  if (strlen(val) > sizeof(buf) - 1)
    {
      DBG(1, "setup_sections: option string too long\n");
      status = SANE_STATUS_INVAL;
    }
  else
    {
      strcpy(buf, val);

      section = strtok(buf, ",");
      while (section != NULL && sectnum < NUM_SECTIONS) 
	{
	  if (!allblank(section)) 
	    {
	      SANE_Int res = _OPT_VAL_WORD(s, OPT_RESOLUTION);
	      SANE_Int format = 
		get_compression_id(_OPT_VAL_STRING(s, OPT_COMPRESSION));

	      status = section_parse(section, &s->sections[sectnum], 
				     res, format);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG(1, 
		      "setup_sections: error parsing section `%s'\n", 
		      section);
		  break;
		}

	      sectnum++;
	    }
	  section += strlen(section) + 1;
	  if (section > buf + strlen(val)) break;

	  section = strtok(section, ",");
	}
    }
  s->num_sections = sectnum;

  return status;
}

static SANE_Status
start_setup (BH_Scanner *s)
{
  SANE_Status status;
  SANE_Bool duplex;
  SANE_Int i, imagecnt;
  SANE_Byte batchmode;

  DBG(3, "start_setup called\n");

  duplex = _OPT_VAL_WORD(s, OPT_DUPLEX);

  /* get the _SECTION option, parse it and fill in the sections */
  status = setup_sections(s, _OPT_VAL_STRING(s, OPT_SECTION));
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "start_setup: setup_sections failed: %s\n", 
	  sane_strstatus(status));
      return status;
    }

  /* see whether we'll be decoding barcodes and 
   * set the barcodes flag appropriately
   */
  if (s->search_bars[0] == 0)
    {
      s->barcodes = SANE_FALSE;
    }
  else
    {
      s->barcodes = SANE_TRUE;
    }

  /* see whether we'll be handling icons (thumbnails)
   * set the icons flag appropriately
   */
  if (_OPT_VAL_WORD(s, OPT_ICON_WIDTH) >= 8 &&
      _OPT_VAL_WORD(s, OPT_ICON_LENGTH) >= 8)
    {
      s->icons = SANE_TRUE;
    }
  else
    {
      s->icons = SANE_FALSE;
    }


  /* calculate a new readlist for this 'batch' */
  s->readptr = s->readcnt = 0;

  /* always read the front image */
  s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_FRONT;

  /* read back page only if duplex is true */
  if (duplex == SANE_TRUE)
    {
      s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_BACK;
    }

  /* add image section reads to the readlist */
  for (i = 0; i < s->num_sections; i++)
    {
      SANE_Word flags = s->sections[i].flags;

      if (flags & BH_SECTION_FRONT_IMAGE)
	s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_FRONT + i + 1;
      if (flags & BH_SECTION_BACK_IMAGE)
	s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_BACK + i + 1;
    }


  /* icons (thumbnails) */
  if (s->icons)
    {
      s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_FRONT_ICON;
      /* read back icon only if duplex is true */
      if (duplex == SANE_TRUE)
	{
	  s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_BACK_ICON;
	}
    }

  /* NOTE: It is important that all of the image data comes before
   * the barcode/patchcode data.
   */
  /* barcodes */
  imagecnt = s->readcnt;
  if (s->barcodes)
    {
      if (s->num_sections == 0)
	{
	  /* we only decode the entire page(s) if there are no
	   * sections defined
	   */
	  s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_FRONT_BARCODE;
	  /* read back barcode only if duplex is true */
	  if (duplex == SANE_TRUE)
	    {
	      s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_BACK_BARCODE;
	    }
	}
      else
	{
	  /* add barcode section reads to the readlist */
	  for (i = 0; i < s->num_sections; i++)
	    {
	      SANE_Word flags = s->sections[i].flags;

	      if (flags & BH_SECTION_FRONT_BAR)
		s->readlist[s->readcnt++] = 
		  BH_SCSI_READ_TYPE_FRONT_BARCODE + i + 1;
	      if (flags & BH_SECTION_BACK_BAR)
		s->readlist[s->readcnt++] = 
		  BH_SCSI_READ_TYPE_BACK_BARCODE + i + 1;
	    }
	}
    }

  /* patchcodes */
  if (s->patchcodes)
    {
      if (s->num_sections == 0)
	{
	  /* we only decode the entire page(s) if there are no
	   * sections defined
	   */
	  s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_FRONT_PATCHCODE;
	  /* read back patchcode only if duplex is true */
	  if (duplex == SANE_TRUE)
	    {
	      s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_BACK_PATCHCODE;
	    }
	}
      else
	{
	  /* add patchcode section reads to the readlist */
	  for (i = 0; i < s->num_sections; i++)
	    {
	      SANE_Word flags = s->sections[i].flags;

	      if (flags & BH_SECTION_FRONT_PATCH)
		s->readlist[s->readcnt++] = 
		  BH_SCSI_READ_TYPE_FRONT_PATCHCODE + i + 1;
	      if (flags & BH_SECTION_BACK_PATCH)
		s->readlist[s->readcnt++] = 
		  BH_SCSI_READ_TYPE_BACK_PATCHCODE + i + 1;
	    }
	}
    }

  /* add the special item to the read list which transfers the barcode
   * file that's built as a result of processing barcode and patchcode
   * readitems.  NOTE: this one must be last!
   */
  if (s->readcnt > imagecnt)
    {
      s->readlist[s->readcnt++] = BH_SCSI_READ_TYPE_SENDBARFILE;
    }

  if (_OPT_VAL_WORD(s, OPT_BATCH) == SANE_TRUE)
    {
      /* if batchmode is enabled, then call set_window to 
       * abort the batch (even though there might not (and probably
       * isn't) a batch in progress).  This avoids a batch start error
       * in the case where a previous batch was not aborted.
       */
      DBG(5, "start_setup: calling set_window to abort batch\n");
      set_window(s, BH_BATCH_ABORT);

      batchmode = BH_BATCH_ENABLE;
    }  
  else
    {
      batchmode = BH_BATCH_DISABLE;
    }

  DBG(5, "start_setup: duplex=%s, barcodes=%s, patchcodes=%s, "
      "icons=%s, batch=%s\n",
      (duplex == SANE_TRUE) ? "yes" : "no",
      (s->barcodes == SANE_TRUE) ? "yes" : "no",
      (s->patchcodes == SANE_TRUE) ? "yes" : "no",
      (s->icons == SANE_TRUE) ? "yes" : "no",
      (batchmode == BH_BATCH_ENABLE) ? "yes" : "no");
  DBG(5, "start_setup: sections=%d\n", s->num_sections);
  for (i = 0; i < s->num_sections; i++)
    {
      DBG(5, "start_setup:  "
	  "[%d] %lux%lu+%lu+%lu flags=%02x compression=[%d,%d]\n",
	  i+1,
	  s->sections[i].width, s->sections[i].length,
	  s->sections[i].left, s->sections[i].top,
	  s->sections[i].flags,
	  s->sections[i].compressiontype, s->sections[i].compressionarg);
    }
  DBG(5, "start_setup: read list length=%d\n", s->readcnt);
  for (i = 0; i < s->readcnt; i++)
    {
      DBG(5, "start_setup:  [%d] %s\n", i+1, print_read_type(s->readlist[i]));
    }

  DBG(5, "start_setup: sending SET WINDOW\n");
  status = set_window(s, batchmode);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "start_setup: SET WINDOW failed: %s\n", 
	   sane_strstatus(status));
      return status;
    }

  DBG(5, "start_setup: sending mode_select_timeout\n");
  status = mode_select_timeout(s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG(1, "start_setup: mode_select_timeout failed: %s\n", 
	   sane_strstatus(status));
      return status;
    }

  if (s->icons == SANE_TRUE)
    {
      DBG(5, "start_setup: sending mode_select_icon\n");
      status = mode_select_icon(s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "start_setup: mode_select_icon failed: %s\n", 
	       sane_strstatus(status));
	  return status;
	}
    }

  if (s->barcodes == SANE_TRUE)
    {
      DBG(5, "start_setup: sending mode_select_barcode_priority\n");
      status = mode_select_barcode_priority(s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "start_setup: mode_select_barcode_priority failed: %s\n", 
	       sane_strstatus(status));
	  return status;
	}

      DBG(5, "start_setup: sending mode_select_barcode_param1\n");
      status = mode_select_barcode_param1(s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "start_setup: mode_select_barcode_param1 failed: %s\n", 
	       sane_strstatus(status));
	  return status;
	}

      DBG(5, "start_setup: sending mode_select_barcode_param2\n");
      status = mode_select_barcode_param2(s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "start_setup: mode_select_barcode_param2 failed: %s\n", 
	       sane_strstatus(status));
	  return status;
	}

      DBG(5, "start_setup: sending mode_select_barcode_param3\n");
      status = mode_select_barcode_param3(s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "start_setup: mode_select_barcode_param3 failed: %s\n", 
	       sane_strstatus(status));
	  return status;
	}
    }

  return status;
}

static SANE_Status
start_scan (BH_Scanner *s)
{
  static SANE_Byte cmd[8];
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool check_adf, duplex;
  DBG (3, "start_scan called\n");

  /* SANE front ends will call this function between 'FRAMES'.
   * A single scan on the B&H may result in up to 56 different
   * things to read (20 are SANE image frames, 36 are non-SANE
   * data - decoded bar/patch codes).  
   */

  if (s->readcnt > 1 && s->scanning == SANE_TRUE)
    {
      DBG(3, "start_scan: any more items in the readlist?\n");
      /* we've been reading data from this scan, so we just 
       * move on to the next item in the readlist without
       * starting a new scan.
       */
      s->readptr++;
      if (s->readptr < s->readcnt)
	{
	  SANE_Byte itemtype;

	  for (; s->readptr < s->readcnt; s->readptr++)
	    {

	      itemtype = s->readlist[s->readptr];

	      DBG(3, "start_scan: advance readlist(%d, %d)\n",
		  s->readptr, 
		  (int) itemtype);

	      /* 'dance' by the non-SANE data streams
	       * like bar/patch code data
	       */
	      if (!BH_HAS_IMAGE_DATA(itemtype))
		{
		  int fd;
		  FILE *fp;

		  strncpy(s->barfname, "/tmp/bhXXXXXX", sizeof(s->barfname));
		  s->barfname[sizeof(s->barfname)-1] = '\0';
		  fd = mkstemp(s->barfname);

		  if (fd !=-1 && (fp = fdopen(fd, "w")) != NULL)
		    {
		      fprintf(fp, "<xml-stream>\n");

		      for (; 
			   s->readptr < s->readcnt && 
			     status == SANE_STATUS_GOOD; 
			   s->readptr++)
			{
			  if (s->readlist[s->readptr] == 
			      BH_SCSI_READ_TYPE_SENDBARFILE) {
			    break;
			  }
			  status = read_barcode_data(s, fp);
			  if (status != SANE_STATUS_GOOD) break;
			}

		      fprintf(fp, "</xml-stream>\n");

		      /* close file; re-open for read(setting s->barfd) */
		      fclose(fp);
		      if ((s->barf = fopen(s->barfname, "r")) == NULL)
			{
			  DBG(1, "sane_start: error opening barfile `%s'\n", 
			      s->barfname);
			  status = SANE_STATUS_IO_ERROR;
			}
		    }
		  else
		    {
		      DBG(1, "sane_start: error opening barfile `%s'\n", 
			  s->barfname);
		      status = SANE_STATUS_IO_ERROR;
		    }
		}
	      else if (itemtype == BH_SCSI_READ_TYPE_FRONT_ICON ||
		       itemtype == BH_SCSI_READ_TYPE_BACK_ICON)
		{
		  /* read the icon header setting the iconwidth and iconlength
		   * to the actual values so get_parameters will have them.
		   * Subsequent calls to sane_read will get pure image data 
		   * since the icon header has been consumed.
		   */

		  status = read_icon_data(s);
		}

	      if (status == SANE_STATUS_GOOD)
		{
		  /* update our parameters to reflect the new item */
		  status = get_parameters (s, 0);
		}

	      if (status != SANE_STATUS_GOOD) s->scanning = SANE_FALSE;

	      return status;
	    }
	  /* if we reach here, we're finished with the readlist and 
	   * will drop through to start a new scan
	   */
	}
    }

  s->readptr = 0;

  check_adf = _OPT_VAL_WORD(s, OPT_CHECK_ADF);
  duplex = _OPT_VAL_WORD(s, OPT_DUPLEX);

  memset (&cmd, 0, sizeof (cmd));
  cmd[0] = BH_SCSI_START_SCAN;
  cmd[4] = (duplex == SANE_TRUE) ? 2 : 1;

  cmd[6] = 0;
  cmd[7] = 1;

  if (check_adf)
    {
      status = object_position(s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(3, "object_position: returned %d\n", status);
	  return status;
	}
    }

  status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
  if (status == SANE_STATUS_GOOD)
    {
      s->scanning = SANE_TRUE;

      /* update our parameters,
       * now that we're scanning we'll do a GET_WINDOW
       */
      status = get_parameters (s, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  s->scanning = SANE_FALSE;
	}
    }

  return status;
}

/* a sensible sense handler, courtesy of Franck;
   arg is a pointer to the associated BH_Scanner structure */
static SANE_Status
sense_handler (int scsi_fd, u_char *result, void *arg)
{
  BH_Scanner *s = (BH_Scanner *) arg;
  u_char sense, asc, ascq, EOM, ILI, ErrorCode, ValidData;
  u_long InvalidBytes;
  char *sense_str = "", *as_str = "";
  SANE_Int i;
  SANE_Status status = SANE_STATUS_INVAL;
  SANE_Char print_sense[(16 * 3) + 1];

  scsi_fd = scsi_fd; /* get rid of compiler warning */
  ErrorCode = result[0] & 0x7F;
  ValidData = (result[0] & 0x80) != 0;
  sense = result[2] & 0x0f; /* Key */
  asc = result[12]; /* Code */
  ascq = result[13]; /* Qual */
  EOM = (result[2] & 0x40) != 0; /* End Of Media */
  ILI = (result[2] & 0x20) != 0; /* Invalid Length Indicator */
  InvalidBytes = ValidData ? _4btol(&result[3]) : 0;
  
  DBG(3, "sense_handler: result=%x, sense=%x, asc=%x, ascq=%x\n",
      result[0], sense, asc, ascq);
  DBG(3, "sense_handler: ErrorCode %02x ValidData: %d "
      "EOM: %d ILI: %d InvalidBytes: %lu\n",
      ErrorCode, ValidData, EOM, ILI, InvalidBytes);

  memset(print_sense, '\0', sizeof(print_sense));
  for (i = 0; i < 16; i++)
    {
      sprintf(print_sense + strlen(print_sense), "%02x ", result[i]);
    }
  DBG(5, "sense_handler: sense=%s\n", print_sense);

  if (ErrorCode != 0x70 && ErrorCode != 0x71)
    {
      DBG (3, "sense_handler: error code is invalid.\n");
      return SANE_STATUS_IO_ERROR;	/* error code is invalid */
    }

  /* handle each sense key; 
   * RSC supports 0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0B 
   */
  switch (sense)
    {
    case 0x00:
      /* no sense */
      sense_str = "No sense.";
      status = SANE_STATUS_GOOD;
      if (ILI && asc == 0x00 && ascq == 0x05)
	{
	  /* from read_data function */
	  as_str = "ILI bit is set.";
	  if (s != NULL)
	    {
	      s->InvalidBytes = InvalidBytes;
	    }
	  status = SANE_STATUS_GOOD;
	}
      else if (EOM && asc == 0x00 && ascq == 0x02)
	{
	  /* from adfStatus or startScan function */
	  as_str = "Out of paper in the hopper.";
	  status = SANE_STATUS_NO_DOCS;
	}
      else if (EOM)
	{
	  /* from adfStatus or startScan function */
	  as_str = "Out of paper in the hopper.";
	  status = SANE_STATUS_NO_DOCS;
	}
      break;
    case 0x01:
      /* recovered error */
      sense_str = "Recovered error.";
      status = SANE_STATUS_GOOD;
      break;
    case 0x02:
      /* not ready */
      sense_str = "Not ready.";
      status = SANE_STATUS_DEVICE_BUSY;
      if (asc == 0x40 && ascq == 0x01)
	{
	  as_str = "P.O.D. error: Scanner not found.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x40 && ascq == 0x02)
	{
	  as_str = "P.O.D. error: Scanner not ready(paper in transport).";
	  status = SANE_STATUS_DEVICE_BUSY;
	}
      else if (asc == 0x40 && ascq == 0x03)
	{
	  as_str = "P.O.D. error: Unknown scanner.";
	  status = SANE_STATUS_INVAL;
	}
      break;
    case 0x03:
      /* medium error */
      sense_str = "Medium error.";
      status = SANE_STATUS_IO_ERROR;
      if (asc == 0x00 && ascq == 0x00)
	{
	  as_str = "Scanner error: paper jam detected.";
	  status = SANE_STATUS_JAMMED;
	}
      break;
    case 0x04:
      /* hardware error */
      sense_str = "Hardware error.";
      status = SANE_STATUS_IO_ERROR;
      if (asc == 0x60 && ascq == 0x00)
	{
	  as_str = "Scanner error: illumination lamps failure.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x03)
	{
	  as_str = "Communication error between RSC and scanner.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x06)
	{
	  as_str = "Scanner error: page detected but lamps are off.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x07)
	{
	  as_str = "Scanner error: camera white level problem.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x08)
	{
	  /* could be caught from start_scan or read_data */
	  /* stop button pressed */
	  as_str = "Scanner error: operator pressed the Stop key.";
	  status = SANE_STATUS_NO_DOCS;
	}
      else if (asc == 0x80 && ascq == 0x12)
	{
	  as_str = "Scanner error: transport motor failure.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x15)
	{
	  as_str = "Scanner error: device / page sensor(s) bouncing.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x16)
	{
	  as_str = "Scanner error: feeder is not attached.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x18)
	{
	  as_str = "Scanner error: logic system general failure.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x34)
	{
	  as_str = "Scanner error: no dual logic communication.";
	  status = SANE_STATUS_IO_ERROR;
	}
      break;
    case 0x05:
      /* illegal request */
      sense_str = "Illegal request.";
      status = SANE_STATUS_INVAL;
      if (asc == 0x1a && ascq == 0x00)
	{
	  as_str = "Parameter list length error.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x20 && ascq == 0x00)
	{
	  as_str = "Invalid command operation code.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x24 && ascq == 0x00)
	{
	  /* caught from object_position (via reverse engineering) */
	  /* Not supported? */
	  as_str = "Invalid field in CDB.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x25 && ascq == 0x00)
	{
	  as_str = "Unsupported LUN.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x26 && ascq == 0x00)
	{
	  /* caught from mode_select (as well as others) */
	  /* Bar/Patch code detection support not installed */
	  /* See Appendix A, Section A.5 */
	  as_str = "Invalid field in parameter list.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x2c && ascq == 0x00)
	{
	  /* we were getting this in read_data during the time
	     that the ADF was misbehaving.  Hopefully we will
	     not see it anymore.
	  */
	  as_str = "Command out of sequence.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x2c && ascq == 0x01)
	{
	  as_str = "Too many windows defined.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x2c && ascq == 0x02)
	{
	  as_str = "Batch start error.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x2c && ascq == 0x03)
	{
	  as_str = "Batch abort error.";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x3d && ascq == 0x00)
	{
	  as_str = "Invalid bits in IDENTIFY message.";
	  status = SANE_STATUS_INVAL;
	}
      break;
    case 0x06:
      /* unit attention */
      sense_str = "Unit attention.";
      status = SANE_STATUS_IO_ERROR;
      if (asc == 0x04 && ascq == 0x01)
	{
	  as_str = "Reset detected, LUN is becoming ready.";
	  status = SANE_STATUS_DEVICE_BUSY;
	}
      break;
    case 0x07:
      /* data protect */
      sense_str = "Data protect.";
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x08:
      /* blank check */
      sense_str = "Blank check.";
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x09:
      /* vendor specific */
      sense_str = "Vendor specific.";
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x0A:
      /* copy aborted */
      sense_str = "Copy aborted.";
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x0B:
      /* aborted command */
      sense_str = "Aborted command.";
      status = SANE_STATUS_IO_ERROR;
      if (asc == 0x00 && ascq == 0x00)
	{
	  as_str = "Aborted command (unspecified error).";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x08 && ascq == 0x01)
	{
	  /* caught from start_scan */
	  /* manual feed timeout */
	  as_str = "SCSI Time-out, paper Time-out (SCAN command).";
	  status = SANE_STATUS_NO_DOCS;
	}
      else if (asc == 0x47 && ascq == 0x00)
	{
	  as_str = "SCSI parity error.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x00)
	{
	  as_str = "Aborted command due to memory error.";
	  status = SANE_STATUS_IO_ERROR;
	}
      else if (asc == 0x80 && ascq == 0x01)
	{
	  /* caught from read_data */
	  /* section border error; border is outside the main window */
	  /* See Appendix A, Section A.4 */
	  as_str = "Section Read error (out of border).";
	  status = SANE_STATUS_INVAL;
	}
      else if (asc == 0x80 && ascq == 0x02)
	{
	  /* caught from read_data */
	  /* No code found; no barcode data is found */
	  /* See Appendix A, Section A.5 */
	  s->barcode_not_found = SANE_TRUE;
	  as_str = "No Bar/Patch Code found.";
	  status = SANE_STATUS_GOOD;
	}
      else if (asc == 0x80 && ascq == 0x03)
	{
	  as_str = "Icon Read error (out of border).";
	  status = SANE_STATUS_INVAL;
	}
      break;
    case 0x0C:
      /* equal */
      sense_str = "Equal.";
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x0D:
      /* volume overflow */
      sense_str = "Volume overflow.";
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x0E:
      /* miscompare */
      sense_str = "Miscompare.";
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x0F:
      /* reserved */
      sense_str = "Reserved.";
      status = SANE_STATUS_IO_ERROR;
      break;
    default:
      sense_str = "Unhandled case.";
      status = SANE_STATUS_IO_ERROR;
      break;
    }

  DBG(3, "sense_handler: '%s' '%s' return:%d\n", 
      sense_str, as_str, status);

  return status;
}

static SANE_Status
init_options (BH_Scanner * s)
{
  int i;
  DBG (3, "init_options called\n");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Scan Mode" group: */
  s->opt[OPT_MODE_GROUP].name = "";
  s->opt[OPT_MODE_GROUP].title = SANE_TITLE_SCAN_MODE_GROUP;
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Preview: */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_PREVIEW].w = 0;

  /* Inquiry */
  s->opt[OPT_INQUIRY].name = SANE_NAME_INQUIRY;
  s->opt[OPT_INQUIRY].title = SANE_TITLE_INQUIRY;
  s->opt[OPT_INQUIRY].desc = SANE_DESC_INQUIRY;
  s->opt[OPT_INQUIRY].type = SANE_TYPE_STRING;
  s->opt[OPT_INQUIRY].size = sizeof(inquiry_data);
  s->opt[OPT_INQUIRY].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_INQUIRY].s = strdup(inquiry_data);
  s->opt[OPT_INQUIRY].cap = SANE_CAP_SOFT_DETECT;

  /* scan mode */
  s->opt[OPT_SCAN_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_SCAN_MODE].size = max_string_size (scan_mode_list);
  s->opt[OPT_SCAN_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCAN_MODE].constraint.string_list = scan_mode_list;
  s->val[OPT_SCAN_MODE].s = strdup (scan_mode_list[0]);

  /* Standard resolutions */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = s->hw->info.resStdList;
  s->val[OPT_RESOLUTION].w = s->hw->info.res_default;

  /* compression */
  s->opt[OPT_COMPRESSION].name = SANE_NAME_COMPRESSION;
  s->opt[OPT_COMPRESSION].title = SANE_TITLE_COMPRESSION;
  s->opt[OPT_COMPRESSION].desc = SANE_DESC_COMPRESSION;
  s->opt[OPT_COMPRESSION].type = SANE_TYPE_STRING;
  s->opt[OPT_COMPRESSION].size = max_string_size (compression_list);
  s->opt[OPT_COMPRESSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_COMPRESSION].constraint.string_list = compression_list;
  s->val[OPT_COMPRESSION].s = strdup (compression_list[0]);

  if (s->hw->info.colorHalftone == SANE_FALSE)
    {
      s->opt[OPT_SCAN_MODE].size = max_string_size (scan_mode_min_list);
      s->opt[OPT_SCAN_MODE].constraint.string_list = scan_mode_min_list;
    }

  if (s->hw->info.comprG3_1D == SANE_FALSE ||
      s->hw->info.comprG3_2D == SANE_FALSE ||
      s->hw->info.comprG4 == SANE_FALSE)
    {
      s->opt[OPT_COMPRESSION].cap |= SANE_CAP_INACTIVE;
    }

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].name = "";
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_TITLE_GEOMETRY_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Autoborder: */
  s->opt[OPT_AUTOBORDER].name = SANE_NAME_AUTOBORDER;
  s->opt[OPT_AUTOBORDER].title = SANE_TITLE_AUTOBORDER;
  s->opt[OPT_AUTOBORDER].desc = SANE_DESC_AUTOBORDER;
  s->opt[OPT_AUTOBORDER].type = SANE_TYPE_BOOL;
  s->opt[OPT_AUTOBORDER].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_AUTOBORDER].w = s->hw->info.autoborder_default;

  /* Paper Size */
  s->opt[OPT_PAPER_SIZE].name = SANE_NAME_PAPER_SIZE;
  s->opt[OPT_PAPER_SIZE].title = SANE_TITLE_PAPER_SIZE;
  s->opt[OPT_PAPER_SIZE].desc = SANE_DESC_PAPER_SIZE;
  s->opt[OPT_PAPER_SIZE].type = SANE_TYPE_STRING;
  s->opt[OPT_PAPER_SIZE].size = max_string_size (paper_list);
  s->opt[OPT_PAPER_SIZE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_PAPER_SIZE].constraint.string_list = paper_list;
  s->val[OPT_PAPER_SIZE].s = strdup (paper_list[0]);

  /* rotation */
  s->opt[OPT_ROTATION].name = SANE_NAME_ROTATION;
  s->opt[OPT_ROTATION].title = SANE_TITLE_ROTATION;
  s->opt[OPT_ROTATION].desc = SANE_DESC_ROTATION;
  s->opt[OPT_ROTATION].type = SANE_TYPE_STRING;
  s->opt[OPT_ROTATION].size = max_string_size (rotation_list);
  s->opt[OPT_ROTATION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_ROTATION].constraint.string_list = rotation_list;
  s->val[OPT_ROTATION].s = strdup (rotation_list[0]);

  /* Deskew: */
  s->opt[OPT_DESKEW].name = SANE_NAME_DESKEW;
  s->opt[OPT_DESKEW].title = SANE_TITLE_DESKEW;
  s->opt[OPT_DESKEW].desc = SANE_DESC_DESKEW;
  s->opt[OPT_DESKEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_DESKEW].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_DESKEW].w =  s->hw->info.deskew_default;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &(s->hw->info.x_range);
  s->val[OPT_TL_X].w = SANE_FIX(0.0);

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &(s->hw->info.y_range);
  s->val[OPT_TL_Y].w = SANE_FIX(0.0);

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &(s->hw->info.x_range);
  s->val[OPT_BR_X].w = s->hw->info.x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &(s->hw->info.y_range);
  s->val[OPT_BR_Y].w = s->hw->info.y_range.max;

  if (s->hw->info.canBorderRecog == SANE_FALSE)
    {
      s->opt[OPT_AUTOBORDER].cap |= SANE_CAP_INACTIVE;
    }

  /* "Feeder" group: */
  s->opt[OPT_FEEDER_GROUP].name = "";
  s->opt[OPT_FEEDER_GROUP].title = SANE_TITLE_FEEDER_GROUP;
  s->opt[OPT_FEEDER_GROUP].desc = "";
  s->opt[OPT_FEEDER_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_FEEDER_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_FEEDER_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan source */
  s->opt[OPT_SCAN_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SCAN_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SCAN_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SCAN_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SCAN_SOURCE].size = max_string_size (scan_source_list);
  s->opt[OPT_SCAN_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCAN_SOURCE].constraint.string_list = scan_source_list;
  s->val[OPT_SCAN_SOURCE].s = strdup (scan_source_list[0]);

  /* Batch: */
  s->opt[OPT_BATCH].name = SANE_NAME_BATCH;
  s->opt[OPT_BATCH].title = SANE_TITLE_BATCH;
  s->opt[OPT_BATCH].desc = SANE_DESC_BATCH;
  s->opt[OPT_BATCH].type = SANE_TYPE_BOOL;
  s->opt[OPT_BATCH].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_BATCH].w =  s->hw->info.batch_default;

  /* Check ADF: */
  s->opt[OPT_CHECK_ADF].name = SANE_NAME_CHECK_ADF;
  s->opt[OPT_CHECK_ADF].title = SANE_TITLE_CHECK_ADF;
  s->opt[OPT_CHECK_ADF].desc = SANE_DESC_CHECK_ADF;
  s->opt[OPT_CHECK_ADF].type = SANE_TYPE_BOOL;
  s->opt[OPT_CHECK_ADF].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_CHECK_ADF].w =  s->hw->info.check_adf_default;

  /* Duplex: */
  s->opt[OPT_DUPLEX].name = SANE_NAME_DUPLEX;
  s->opt[OPT_DUPLEX].title = SANE_TITLE_DUPLEX;
  s->opt[OPT_DUPLEX].desc = SANE_DESC_DUPLEX;
  s->opt[OPT_DUPLEX].type = SANE_TYPE_BOOL;
  s->opt[OPT_DUPLEX].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_DUPLEX].w = s->hw->info.duplex_default;

  /* timeout adf */
  s->opt[OPT_TIMEOUT_ADF].name = SANE_NAME_TIMEOUT_ADF;
  s->opt[OPT_TIMEOUT_ADF].title = SANE_TITLE_TIMEOUT_ADF;
  s->opt[OPT_TIMEOUT_ADF].desc = SANE_DESC_TIMEOUT_ADF;
  s->opt[OPT_TIMEOUT_ADF].type = SANE_TYPE_INT;
  s->opt[OPT_TIMEOUT_ADF].unit = SANE_UNIT_NONE;
  s->opt[OPT_TIMEOUT_ADF].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TIMEOUT_ADF].constraint.range = &u8_range;
  s->val[OPT_TIMEOUT_ADF].w =  s->hw->info.timeout_adf_default;

  /* timeout manual */
  s->opt[OPT_TIMEOUT_MANUAL].name = SANE_NAME_TIMEOUT_MANUAL;
  s->opt[OPT_TIMEOUT_MANUAL].title = SANE_TITLE_TIMEOUT_MANUAL;
  s->opt[OPT_TIMEOUT_MANUAL].desc = SANE_DESC_TIMEOUT_MANUAL;
  s->opt[OPT_TIMEOUT_MANUAL].type = SANE_TYPE_INT;
  s->opt[OPT_TIMEOUT_MANUAL].unit = SANE_UNIT_NONE;
  s->opt[OPT_TIMEOUT_MANUAL].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TIMEOUT_MANUAL].constraint.range = &u8_range;
  s->val[OPT_TIMEOUT_MANUAL].w =  s->hw->info.timeout_manual_default;

  if (s->hw->info.canCheckADF == SANE_FALSE)
    {
      s->opt[OPT_CHECK_ADF].cap |= SANE_CAP_INACTIVE;
    }

  if (s->hw->info.canDuplex == SANE_FALSE)
    {
      s->opt[OPT_DUPLEX].cap |= SANE_CAP_INACTIVE;
    }

  if (s->hw->info.canADF == SANE_FALSE)
    {
      s->opt[OPT_TIMEOUT_ADF].cap |= SANE_CAP_INACTIVE;
    }

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].name = "";
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_TITLE_ENHANCEMENT_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Control Panel: */
  s->opt[OPT_CONTROL_PANEL].name = SANE_NAME_CONTROL_PANEL;
  s->opt[OPT_CONTROL_PANEL].title = SANE_TITLE_CONTROL_PANEL;
  s->opt[OPT_CONTROL_PANEL].desc = SANE_DESC_CONTROL_PANEL;
  s->opt[OPT_CONTROL_PANEL].type = SANE_TYPE_BOOL;
  s->opt[OPT_CONTROL_PANEL].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_CONTROL_PANEL].w =  s->hw->info.control_panel_default;

  /* Ace_Function */
  s->opt[OPT_ACE_FUNCTION].name = SANE_NAME_ACE_FUNCTION;
  s->opt[OPT_ACE_FUNCTION].title = SANE_TITLE_ACE_FUNCTION;
  s->opt[OPT_ACE_FUNCTION].desc = SANE_DESC_ACE_FUNCTION;
  s->opt[OPT_ACE_FUNCTION].type = SANE_TYPE_INT;
  s->opt[OPT_ACE_FUNCTION].unit = SANE_UNIT_NONE;
  s->opt[OPT_ACE_FUNCTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_ACE_FUNCTION].constraint.range = &ace_function_range;
  s->val[OPT_ACE_FUNCTION].w = 0;

  /* Ace_Sensitivity */
  s->opt[OPT_ACE_SENSITIVITY].name = SANE_NAME_ACE_SENSITIVITY;
  s->opt[OPT_ACE_SENSITIVITY].title = SANE_TITLE_ACE_SENSITIVITY;
  s->opt[OPT_ACE_SENSITIVITY].desc = SANE_DESC_ACE_SENSITIVITY;
  s->opt[OPT_ACE_SENSITIVITY].type = SANE_TYPE_INT;
  s->opt[OPT_ACE_SENSITIVITY].unit = SANE_UNIT_NONE;
  s->opt[OPT_ACE_SENSITIVITY].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_ACE_SENSITIVITY].constraint.range = &ace_sensitivity_range;
  s->val[OPT_ACE_SENSITIVITY].w = 4;

  /* Brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &u8_range;
  s->val[OPT_BRIGHTNESS].w = 0;

  /* Threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &u8_range;
  s->val[OPT_THRESHOLD].w = 0;

  /* Contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &u8_range;
  s->val[OPT_CONTRAST].w = 0;

  /* Negative: */
  s->opt[OPT_NEGATIVE].name = SANE_NAME_NEGATIVE;
  s->opt[OPT_NEGATIVE].title = SANE_TITLE_NEGATIVE;
  s->opt[OPT_NEGATIVE].desc = SANE_DESC_NEGATIVE;
  s->opt[OPT_NEGATIVE].type = SANE_TYPE_BOOL;
  s->opt[OPT_NEGATIVE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_NEGATIVE].w = SANE_FALSE;

  /* Contrast is not used in any case; why did we add it? */
  s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
  if (s->hw->info.control_panel_default == SANE_TRUE)
    {
      s->opt[OPT_ACE_FUNCTION].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_ACE_SENSITIVITY].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
    }
  else if (s->hw->info.canACE == SANE_FALSE)
    {
      s->opt[OPT_ACE_FUNCTION].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_ACE_SENSITIVITY].cap |= SANE_CAP_INACTIVE;
    }
  else
    {
      s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
    }

  /* "ICON" group: */
  s->opt[OPT_ICON_GROUP].name = "";
  s->opt[OPT_ICON_GROUP].title = SANE_TITLE_ICON_GROUP;
  s->opt[OPT_ICON_GROUP].desc = "";
  s->opt[OPT_ICON_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ICON_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_ICON_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Icon_Width */
  s->opt[OPT_ICON_WIDTH].name = SANE_NAME_ICON_WIDTH;
  s->opt[OPT_ICON_WIDTH].title = SANE_TITLE_ICON_WIDTH;
  s->opt[OPT_ICON_WIDTH].desc = SANE_DESC_ICON_WIDTH;
  s->opt[OPT_ICON_WIDTH].type = SANE_TYPE_INT;
  s->opt[OPT_ICON_WIDTH].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_ICON_WIDTH].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_ICON_WIDTH].constraint.range = &icon_range;
  s->val[OPT_ICON_WIDTH].w = 0;

  /* Icon_Length */
  s->opt[OPT_ICON_LENGTH].name = SANE_NAME_ICON_LENGTH;
  s->opt[OPT_ICON_LENGTH].title = SANE_TITLE_ICON_LENGTH;
  s->opt[OPT_ICON_LENGTH].desc = SANE_DESC_ICON_LENGTH;
  s->opt[OPT_ICON_LENGTH].type = SANE_TYPE_INT;
  s->opt[OPT_ICON_LENGTH].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_ICON_LENGTH].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_ICON_LENGTH].constraint.range = &icon_range;
  s->val[OPT_ICON_LENGTH].w = 0;

  if (s->hw->info.canIcon == SANE_FALSE)
    {
      s->opt[OPT_ICON_GROUP].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_ICON_WIDTH].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_ICON_LENGTH].cap |= SANE_CAP_INACTIVE;
    }

  /* "Barcode" group: */
  s->opt[OPT_BARCODE_GROUP].name = "";
  s->opt[OPT_BARCODE_GROUP].title = SANE_TITLE_BARCODE_GROUP;
  s->opt[OPT_BARCODE_GROUP].desc = "";
  s->opt[OPT_BARCODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_BARCODE_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_BARCODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Add <name> to barcode search priority. */
  s->opt[OPT_BARCODE_SEARCH_BAR].name = SANE_NAME_BARCODE_SEARCH_BAR;
  s->opt[OPT_BARCODE_SEARCH_BAR].title = SANE_TITLE_BARCODE_SEARCH_BAR;
  s->opt[OPT_BARCODE_SEARCH_BAR].desc = SANE_DESC_BARCODE_SEARCH_BAR;
  s->opt[OPT_BARCODE_SEARCH_BAR].type = SANE_TYPE_STRING;
  s->opt[OPT_BARCODE_SEARCH_BAR].unit = SANE_UNIT_NONE;
  s->opt[OPT_BARCODE_SEARCH_BAR].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_BARCODE_SEARCH_BAR].constraint.string_list = barcode_search_bar_list;
  s->opt[OPT_BARCODE_SEARCH_BAR].size = max_string_size (barcode_search_bar_list);
  s->val[OPT_BARCODE_SEARCH_BAR].s = strdup (barcode_search_bar_list[0]);
  
  /* Barcode search count (1-7, default 1). */
  s->opt[OPT_BARCODE_SEARCH_COUNT].name = SANE_NAME_BARCODE_SEARCH_COUNT;
  s->opt[OPT_BARCODE_SEARCH_COUNT].title = SANE_TITLE_BARCODE_SEARCH_COUNT;
  s->opt[OPT_BARCODE_SEARCH_COUNT].desc = SANE_DESC_BARCODE_SEARCH_COUNT;
  s->opt[OPT_BARCODE_SEARCH_COUNT].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_SEARCH_COUNT].unit = SANE_UNIT_NONE;
  s->opt[OPT_BARCODE_SEARCH_COUNT].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_SEARCH_COUNT].constraint.range = &barcode_search_count_range;
  s->val[OPT_BARCODE_SEARCH_COUNT].w =  3;

  /* Barcode search mode. horiz-vert, horizontal, vertical, vert-horiz */
  s->opt[OPT_BARCODE_SEARCH_MODE].name = SANE_NAME_BARCODE_SEARCH_MODE;
  s->opt[OPT_BARCODE_SEARCH_MODE].title = SANE_TITLE_BARCODE_SEARCH_MODE;
  s->opt[OPT_BARCODE_SEARCH_MODE].desc = SANE_DESC_BARCODE_SEARCH_MODE;
  s->opt[OPT_BARCODE_SEARCH_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_BARCODE_SEARCH_MODE].size = max_string_size (barcode_search_mode_list);
  s->opt[OPT_BARCODE_SEARCH_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_BARCODE_SEARCH_MODE].constraint.string_list = barcode_search_mode_list;
  s->val[OPT_BARCODE_SEARCH_MODE].s = strdup(barcode_search_mode_list[0]);

  /* Patch code min height (def=5mm) */
  s->opt[OPT_BARCODE_HMIN].name = SANE_NAME_BARCODE_HMIN;
  s->opt[OPT_BARCODE_HMIN].title = SANE_TITLE_BARCODE_HMIN;
  s->opt[OPT_BARCODE_HMIN].desc = SANE_DESC_BARCODE_HMIN;
  s->opt[OPT_BARCODE_HMIN].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_HMIN].unit = SANE_UNIT_MM;
  s->opt[OPT_BARCODE_HMIN].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_HMIN].constraint.range = &barcode_hmin_range;
  s->val[OPT_BARCODE_HMIN].w =  5;

  /* Barcode search timeout in ms (20-65535,default is 10000). */
  s->opt[OPT_BARCODE_SEARCH_TIMEOUT].name = SANE_NAME_BARCODE_SEARCH_TIMEOUT;
  s->opt[OPT_BARCODE_SEARCH_TIMEOUT].title = SANE_TITLE_BARCODE_SEARCH_TIMEOUT;
  s->opt[OPT_BARCODE_SEARCH_TIMEOUT].desc = SANE_DESC_BARCODE_SEARCH_TIMEOUT;
  s->opt[OPT_BARCODE_SEARCH_TIMEOUT].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_SEARCH_TIMEOUT].unit = SANE_UNIT_MICROSECOND;
  s->opt[OPT_BARCODE_SEARCH_TIMEOUT].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_SEARCH_TIMEOUT].constraint.range = &barcode_search_timeout_range;
  s->val[OPT_BARCODE_SEARCH_TIMEOUT].w =  10000;

  /* Specify image sections and functions */
  s->opt[OPT_SECTION].name = SANE_NAME_SECTION;
  s->opt[OPT_SECTION].title = SANE_TITLE_SECTION;
  s->opt[OPT_SECTION].desc = SANE_DESC_SECTION;
  s->opt[OPT_SECTION].type = SANE_TYPE_STRING;
  s->opt[OPT_SECTION].unit = SANE_UNIT_NONE;
  s->opt[OPT_SECTION].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_SECTION].size = 255;
  s->val[OPT_SECTION].s = strdup ("");
  
  /* Barcode_Relmax */
  s->opt[OPT_BARCODE_RELMAX].name = SANE_NAME_BARCODE_RELMAX;
  s->opt[OPT_BARCODE_RELMAX].title = SANE_TITLE_BARCODE_RELMAX;
  s->opt[OPT_BARCODE_RELMAX].desc = SANE_DESC_BARCODE_RELMAX;
  s->opt[OPT_BARCODE_RELMAX].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_RELMAX].unit = SANE_UNIT_NONE;
  s->opt[OPT_BARCODE_RELMAX].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_RELMAX].constraint.range = &u8_range;
  s->val[OPT_BARCODE_RELMAX].w = 0;

  /* Barcode_Barmin */
  s->opt[OPT_BARCODE_BARMIN].name = SANE_NAME_BARCODE_BARMIN;
  s->opt[OPT_BARCODE_BARMIN].title = SANE_TITLE_BARCODE_BARMIN;
  s->opt[OPT_BARCODE_BARMIN].desc = SANE_DESC_BARCODE_BARMIN;
  s->opt[OPT_BARCODE_BARMIN].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_BARMIN].unit = SANE_UNIT_NONE;
  s->opt[OPT_BARCODE_BARMIN].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_BARMIN].constraint.range = &u8_range;
  s->val[OPT_BARCODE_BARMIN].w = 0;

  /* Barcode_Barmax */
  s->opt[OPT_BARCODE_BARMAX].name = SANE_NAME_BARCODE_BARMAX;
  s->opt[OPT_BARCODE_BARMAX].title = SANE_TITLE_BARCODE_BARMAX;
  s->opt[OPT_BARCODE_BARMAX].desc = SANE_DESC_BARCODE_BARMAX;
  s->opt[OPT_BARCODE_BARMAX].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_BARMAX].unit = SANE_UNIT_NONE;
  s->opt[OPT_BARCODE_BARMAX].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_BARMAX].constraint.range = &u8_range;
  s->val[OPT_BARCODE_BARMAX].w = 0;

  /* Barcode_Contrast */
  s->opt[OPT_BARCODE_CONTRAST].name = SANE_NAME_BARCODE_CONTRAST;
  s->opt[OPT_BARCODE_CONTRAST].title = SANE_TITLE_BARCODE_CONTRAST;
  s->opt[OPT_BARCODE_CONTRAST].desc = SANE_DESC_BARCODE_CONTRAST;
  s->opt[OPT_BARCODE_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_BARCODE_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_CONTRAST].constraint.range = &barcode_contrast_range;
  s->val[OPT_BARCODE_CONTRAST].w = 3;

  /* Barcode_Patchmode */
  s->opt[OPT_BARCODE_PATCHMODE].name = SANE_NAME_BARCODE_PATCHMODE;
  s->opt[OPT_BARCODE_PATCHMODE].title = SANE_TITLE_BARCODE_PATCHMODE;
  s->opt[OPT_BARCODE_PATCHMODE].desc = SANE_DESC_BARCODE_PATCHMODE;
  s->opt[OPT_BARCODE_PATCHMODE].type = SANE_TYPE_INT;
  s->opt[OPT_BARCODE_PATCHMODE].unit = SANE_UNIT_NONE;
  s->opt[OPT_BARCODE_PATCHMODE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BARCODE_PATCHMODE].constraint.range = &barcode_patchmode_range;
  s->val[OPT_BARCODE_PATCHMODE].w = 0;

  if (s->hw->info.canSection == SANE_FALSE)
    {
      s->opt[OPT_SECTION].cap |= SANE_CAP_INACTIVE;
    }

  if (s->hw->info.canBarCode == SANE_FALSE)
    {
      s->opt[OPT_BARCODE_GROUP].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_SEARCH_BAR].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_SEARCH_COUNT].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_SEARCH_MODE].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_HMIN].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_SEARCH_TIMEOUT].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_RELMAX].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_BARMIN].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_BARMAX].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_CONTRAST].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BARCODE_PATCHMODE].cap |= SANE_CAP_INACTIVE;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (const char *devnam, BH_Device ** devp)
{
  SANE_Status status;
  BH_Device *dev;
  struct inquiry_standard_data ibuf;
  struct inquiry_vpd_data vbuf;
  struct inquiry_jis_data jbuf;
  size_t buf_size;
  int fd = -1;
  double mm;

  DBG (3, "attach called\n");

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devnam) == 0)
        {
          if (devp)
            *devp = dev;
          return SANE_STATUS_GOOD;
        }
    }

#ifdef FAKE_INQUIRY
  if (fake_inquiry)
    {
      DBG (3, "attach: faking inquiry of %s\n", devnam);

      memset (&ibuf, 0, sizeof (ibuf));
      ibuf.devtype = 6;
      memcpy(ibuf.vendor, "**FAKE**", 8);
      memcpy(ibuf.product, "COPISCAN II 6338", 16);
      memcpy(ibuf.revision, "0016", 4);

      DBG (1, "attach: reported devtype='%d', vendor='%.8s', "
	   "product='%.16s', revision='%.4s'\n",
	   ibuf.devtype, ibuf.vendor,
	   ibuf.product, ibuf.revision);

      memset (&vbuf, 0, sizeof (vbuf));
      memset (&jbuf, 0, sizeof (jbuf));
    }
  else
#endif
    {
      DBG (3, "attach: opening %s\n", devnam);
      status = sanei_scsi_open (devnam, &fd, sense_handler, NULL);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "attach: open failed: %s\n", sane_strstatus (status));
	  return status;
	}

      DBG (3, "attach: sending TEST_UNIT_READY\n");
      status = test_unit_ready (fd);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "attach: test unit ready failed (%s)\n",
	       sane_strstatus (status));
	  sanei_scsi_close (fd);
	  return status;
	}

      DBG (3, "attach: sending INQUIRY (standard data)\n");
      memset (&ibuf, 0, sizeof (ibuf));
      buf_size = sizeof(ibuf);
      status = inquiry (fd, &ibuf, &buf_size, 0,
			BH_INQUIRY_STANDARD_PAGE_CODE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "attach: inquiry (standard data) failed: %s\n", 
	       sane_strstatus (status));
	  sanei_scsi_close (fd);
	  return status;
	}

      DBG (1, "attach: reported devtype='%d', vendor='%.8s', "
	   "product='%.16s', revision='%.4s'\n",
	   ibuf.devtype, ibuf.vendor,
	   ibuf.product, ibuf.revision);

      if (ibuf.devtype != 6
	  || strncmp ((char *)ibuf.vendor, "B&H SCSI", 8) != 0
	  || strncmp ((char *)ibuf.product, "COPISCAN ", 9) != 0)
	{
	  DBG (1, 
	       "attach: device is not a recognized Bell and Howell scanner\n");
	  sanei_scsi_close (fd);
	  return SANE_STATUS_INVAL;
	}

      DBG (3, "attach: sending INQUIRY (vpd data)\n");
      memset (&vbuf, 0, sizeof (vbuf));
      buf_size = sizeof(vbuf);
      status = inquiry (fd, &vbuf, &buf_size, 1, 
			BH_INQUIRY_VPD_PAGE_CODE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "attach: inquiry (vpd data) failed: %s\n", 
	       sane_strstatus (status));
	  sanei_scsi_close (fd);
	  return status;
	}

      DBG (3, "attach: sending INQUIRY (jis data)\n");
      memset (&jbuf, 0, sizeof (jbuf));
      buf_size = sizeof(jbuf);
      status = inquiry (fd, &jbuf, &buf_size, 1, 
			BH_INQUIRY_JIS_PAGE_CODE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "attach: inquiry (jis data) failed: %s\n",
	       sane_strstatus (status));
	  sanei_scsi_close (fd);
	  return status;
	}

      sanei_scsi_close (fd);
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;
  memset (dev, 0, sizeof (*dev));


  dev->info.devtype = ibuf.devtype;
  sprintf(dev->info.vendor, "%.8s", ibuf.vendor);
  trim_spaces(dev->info.vendor, sizeof(dev->info.vendor));
  sprintf(dev->info.product, "%.16s", ibuf.product);
  trim_spaces(dev->info.product, sizeof(dev->info.product));
  sprintf(dev->info.revision, "%.4s", ibuf.revision);
  trim_spaces(dev->info.revision, sizeof(dev->info.revision));

  dev->sane.name = strdup (devnam);
  dev->sane.vendor = strdup(dev->info.vendor);
  dev->sane.model = strdup(dev->info.product);;
  dev->sane.type = strdup(print_devtype(dev->info.devtype));

  /* set capabilities from vpd */
  dev->info.canADF = vbuf.adf & 0x01;
  dev->info.colorBandW = vbuf.imagecomposition & 0x01;
  dev->info.colorHalftone = vbuf.imagecomposition & 0x02;
  dev->info.canWhiteFrame = vbuf.imagedataprocessing[1] & 0x01;
  dev->info.canBlackFrame = vbuf.imagedataprocessing[1] & 0x02;
  dev->info.canEdgeExtract = vbuf.imagedataprocessing[1] & 0x04;
  dev->info.canNoiseFilter = vbuf.imagedataprocessing[1] & 0x08;
  dev->info.canSmooth = vbuf.imagedataprocessing[1] & 0x10;
  dev->info.canLineBold = vbuf.imagedataprocessing[1] & 0x20;
  dev->info.comprG3_1D = vbuf.compression & 0x01;
  dev->info.comprG3_2D = vbuf.compression & 0x02;
  dev->info.comprG4 = vbuf.compression & 0x04;
  dev->info.canBorderRecog = vbuf.sizerecognition & 0x01;
  dev->info.canBarCode = vbuf.optionalfeatures & 0x01;
  dev->info.canIcon = vbuf.optionalfeatures & 0x02;
  dev->info.canSection = vbuf.optionalfeatures & 0x04;
  dev->info.lineMaxBytes = _2btol(vbuf.xmaxoutputbytes);

#ifdef FAKE_INQUIRY
  if (fake_inquiry)
    {
      dev->info.canADF = SANE_FALSE;
      dev->info.colorBandW = SANE_TRUE;
      dev->info.colorHalftone = SANE_TRUE;
      dev->info.canWhiteFrame = SANE_TRUE;
      dev->info.canBlackFrame = SANE_TRUE;
      dev->info.canEdgeExtract = SANE_TRUE;
      dev->info.canNoiseFilter = SANE_TRUE;
      dev->info.canSmooth = SANE_TRUE;
      dev->info.canLineBold = SANE_TRUE;
      dev->info.comprG3_1D = SANE_TRUE;
      dev->info.comprG3_2D = SANE_TRUE;
      dev->info.comprG4 = SANE_TRUE;
      dev->info.canBorderRecog = SANE_TRUE;
      dev->info.canBarCode = SANE_TRUE;
      dev->info.canIcon = SANE_TRUE;
      dev->info.canSection = SANE_TRUE;
      dev->info.lineMaxBytes = 450;
    }
#endif

  /* set capabilities from jis */
  dev->info.resBasicX = _2btol(jbuf.basicxres);
  dev->info.resBasicY = _2btol(jbuf.basicyres);
  dev->info.resMaxX = _2btol(jbuf.maxxres);
  dev->info.resMaxY = _2btol(jbuf.maxyres);
  dev->info.resMinX = _2btol(jbuf.minxres);
  dev->info.resMinY = _2btol(jbuf.minyres);

  /* set the length of the list to zero first, then append standard resolutions */
  dev->info.resStdList[0] = 0;
  if (jbuf.standardres[0] & 0x80) appendStdList(&dev->info, 60);
  if (jbuf.standardres[0] & 0x40) appendStdList(&dev->info, 75);
  if (jbuf.standardres[0] & 0x20) appendStdList(&dev->info, 100);
  if (jbuf.standardres[0] & 0x10) appendStdList(&dev->info, 120);
  if (jbuf.standardres[0] & 0x08) appendStdList(&dev->info, 150);
  if (jbuf.standardres[0] & 0x04) appendStdList(&dev->info, 160);
  if (jbuf.standardres[0] & 0x02) appendStdList(&dev->info, 180);
  if (jbuf.standardres[0] & 0x01) appendStdList(&dev->info, 200);
  if (jbuf.standardres[1] & 0x80) appendStdList(&dev->info, 240);
  if (jbuf.standardres[1] & 0x40) appendStdList(&dev->info, 300);
  if (jbuf.standardres[1] & 0x20) appendStdList(&dev->info, 320);
  if (jbuf.standardres[1] & 0x10) appendStdList(&dev->info, 400);
  if (jbuf.standardres[1] & 0x08) appendStdList(&dev->info, 480);
  if (jbuf.standardres[1] & 0x04) appendStdList(&dev->info, 600);
  if (jbuf.standardres[1] & 0x02) appendStdList(&dev->info, 800);
  if (jbuf.standardres[1] & 0x01) appendStdList(&dev->info, 1200);
  if (dev->info.resStdList[0] == 0) 
    {
      /* make a default standard resolutions for 200 and 300dpi */
      DBG(1, "attach: no standard resolutions reported\n");
      dev->info.resStdList[0] = 2;
      dev->info.resStdList[1] = 200;
      dev->info.resStdList[2] = 300;
      dev->info.resBasicX = dev->info.resBasicY = 300;
    }
 
  dev->info.winWidth = _4btol(jbuf.windowwidth);
  dev->info.winHeight = _4btol(jbuf.windowlength);

  if (dev->info.winWidth <= 0) 
    {
      dev->info.winWidth = (SANE_Int) (dev->info.resBasicX * 8.5);
      DBG(1, "attach: invalid window width reported, using %d\n", dev->info.winWidth);
    }
  if (dev->info.winHeight <= 0) 
    {
      dev->info.winHeight = dev->info.resBasicY * 14;
      DBG(1, "attach: invalid window height reported, using %d\n", dev->info.winHeight);
    }

  mm = (dev->info.resBasicX > 0) ? 
    ((double) dev->info.winWidth / (double) dev->info.resBasicX * MM_PER_INCH) :
    0.0;
  dev->info.x_range.min = SANE_FIX(0.0);
  dev->info.x_range.max = SANE_FIX(mm);
  dev->info.x_range.quant = SANE_FIX(0.0);

  mm = (dev->info.resBasicY > 0) ? 
    ((double) dev->info.winHeight / (double) dev->info.resBasicY * MM_PER_INCH) :
    0.0;
  dev->info.y_range.min = SANE_FIX(0.0);
  dev->info.y_range.max = SANE_FIX(mm);
  dev->info.y_range.quant = SANE_FIX(0.0);

  /* set additional discovered/guessed capabilities */

  /* if all of the ACE capabilities are present, declare it ACE capable */
  dev->info.canACE = dev->info.canEdgeExtract &&
    dev->info.canNoiseFilter &&
    dev->info.canSmooth &&
    dev->info.canLineBold;

  /* if the model is known to be a duplex, declare it duplex capable */
  if (strcmp(dev->info.product, "COPISCAN II 6338") == 0)
    {
      dev->info.canDuplex = SANE_TRUE;
    }
  else
    {
      dev->info.canDuplex = SANE_FALSE;
    }

  /* the paper sensor requires RSC revision 1.4 or higher and an
   * installed feeder.  NOTE: It also requires SW-4 on and the
   * AccufeedPlus feeder, but we cannot discover that.
   */
  if (strcmp(dev->info.revision, "0014") >= 0)
    {
      dev->info.canCheckADF = dev->info.canADF;
    }
  else
    {
      dev->info.canCheckADF = SANE_FALSE;
    }

  /* set option defaults based on inquiry information */
  dev->info.res_default = dev->info.resBasicX;
  dev->info.autoborder_default = dev->info.canBorderRecog;
  dev->info.batch_default = SANE_FALSE;
  dev->info.deskew_default = SANE_FALSE;
  dev->info.check_adf_default = SANE_FALSE;
  dev->info.duplex_default = SANE_FALSE;
  dev->info.timeout_adf_default = 0;
  dev->info.timeout_manual_default = 0;
  dev->info.control_panel_default = dev->info.canACE;

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one(const char *devnam)
{
  attach (devnam, NULL);
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback authorize)
{
    char devnam[PATH_MAX] = "/dev/scanner";
    FILE *fp;

    authorize = authorize; /* get rid of compiler warning */

    DBG_INIT();
    DBG(3, "sane_init called\n");
    DBG(1, "Bell+Howell SANE backend %d.%d build %d %s endian\n",
	SANE_CURRENT_MAJOR, V_MINOR, BUILD,
	_is_host_little_endian() ? "little" : "big");

    if (version_code)
	*version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);
    
    fp = sanei_config_open(BH_CONFIG_FILE);
    if (fp)
	{
	    char line[PATH_MAX];
	    const char *lp;
	    size_t len;
	    
	    /* read config file */
	    while (sanei_config_read (line, sizeof (line), fp))
		{
		  if (line[0] == '#')           /* ignore line comments */
		    continue;
		  len = strlen (line);
		    
		  if (!len)
		    continue;                   /* ignore empty lines */
		    
		  lp = sanei_config_skip_whitespace (line);

		  DBG(16, 
		      "sane_init: processing config file line '%s'\n",
		      line);
		  if (strncmp(lp, "option", 6) == 0 && 
		      (isspace (lp[6]) || lp[6] == '\0'))
		    {
		      lp += 6;
		      lp = sanei_config_skip_whitespace (lp);
		      
		      if (strncmp(lp, "disable-optional-frames", 23) == 0)
			{
			  DBG(1, "sane_init: configuration option "
			      "'disable-optional-frames' set\n");
			  disable_optional_frames = 1;
			}
		      else if (strncmp(lp, "fake-inquiry", 12) == 0)
			{
			  DBG(1, "sane_init: configuration option "
			      "'fake-inquiry' set\n");
			  fake_inquiry = 1;
			}
		      else
			{
			  DBG(1, "sane_init: ignoring unknown "
			      "configuration option '%s'\n",
			      lp);
			}
		    }
		  else 
		    {
		      DBG(16, 
			  "sane_init: found a device: line '%s'\n",
			  lp);
		      strncpy (devnam, lp, sizeof(devnam));
		      devnam[sizeof(devnam)-1] = '\0';
		      
		      sanei_config_attach_matching_devices(devnam, 
							   attach_one);
		    }
		}
	    fclose (fp);
	}
    else
	{
	    /* configure the /dev/scanner device in the absence of config file */
	    sanei_config_attach_matching_devices ("/dev/scanner", attach_one);
	}

    return SANE_STATUS_GOOD;
}

SANE_Status 
sane_get_devices (const SANE_Device ***device_list, SANE_Bool local)
{
    BH_Device *dev;
    int i;
    DBG(3, "sane_get_devices called\n");

    local = local; /* get rid of compiler warning */
    if (devlist)
	free (devlist);
    devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
    if (!devlist)
	return SANE_STATUS_NO_MEM;

    i = 0;
    for (dev = first_dev; dev; dev = dev->next)
	devlist[i++] = &dev->sane;
    devlist[i++] = 0;

    *device_list = devlist;

    return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devnam, SANE_Handle *handle)
{
    SANE_Status status;
    BH_Device *dev;
    BH_Scanner *s;
    DBG(3, "sane_open called\n");
    
    if (devnam[0] != '\0')
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
			return status;
		}
	}
    else
	{
	    dev = first_dev;
	}
    
    if (!dev)
	return SANE_STATUS_INVAL;

    s = malloc (sizeof (*s));
    if (!s)
	return SANE_STATUS_NO_MEM;
    memset (s, 0, sizeof (*s));

    s->fd = -1;
    s->hw = dev;

    s->bmu = BH_UNIT_POINT;
    s->mud = 1;

    ScannerDump(s);

    init_options (s);

    s->next = first_handle;
    first_handle = s;

    /* initialize our parameters */
    get_parameters(s, 0);

    *handle = s;

#ifdef FAKE_INQUIRY
    if (fake_inquiry)
      {
	DBG (1, "sane_open: faking open of %s\n",
	     s->hw->sane.name);
      }
    else
#endif
      {
	status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (1, "sane_open: open of %s failed: %s\n",
		 s->hw->sane.name, sane_strstatus (status));
	    return status;
	  }
      }

    return SANE_STATUS_GOOD;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  BH_Scanner *s = handle;
  DBG(3, "sane_get_option_descriptor called (option:%d)\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
      return 0;

  return (s->opt + option);
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option, SANE_Action action,
		     void *val, SANE_Word *info)
{
  BH_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_String_Const name;

  DBG(3, "sane_control_option called\n");

  name = s->opt[option].name ? s->opt[option].name : "(nil)";

  if (info)
    *info = 0;

  if (s->scanning && action == SANE_ACTION_SET_VALUE)
    return SANE_STATUS_DEVICE_BUSY;
  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG(16, "sane_control_option: get_value %s [#%d]\n", name, option);
      switch (option)
	{
	  /* word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_TIMEOUT_ADF:
	case OPT_TIMEOUT_MANUAL:
	case OPT_ACE_FUNCTION:
	case OPT_ACE_SENSITIVITY:
	case OPT_BRIGHTNESS:
	case OPT_THRESHOLD:
	case OPT_CONTRAST:
	case OPT_ICON_WIDTH:
	case OPT_ICON_LENGTH:
	case OPT_BARCODE_SEARCH_COUNT:
	case OPT_BARCODE_HMIN:
	case OPT_BARCODE_SEARCH_TIMEOUT:
	case OPT_BARCODE_RELMAX:
	case OPT_BARCODE_BARMIN:
	case OPT_BARCODE_BARMAX:
	case OPT_BARCODE_CONTRAST:
	case OPT_BARCODE_PATCHMODE:
	case OPT_NUM_OPTS:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_INQUIRY:
	case OPT_SCAN_SOURCE:
	case OPT_SCAN_MODE:
	case OPT_COMPRESSION:
	case OPT_PAPER_SIZE:
	case OPT_ROTATION:
	case OPT_BARCODE_SEARCH_BAR:
	case OPT_BARCODE_SEARCH_MODE:
	case OPT_SECTION:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;

	  /* boolean options: */
	case OPT_PREVIEW:
	case OPT_AUTOBORDER:
	case OPT_DESKEW:
	case OPT_BATCH:
	case OPT_CHECK_ADF:
	case OPT_DUPLEX:
	case OPT_CONTROL_PANEL:
	case OPT_NEGATIVE:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	default:
	  DBG(1, "sane_control_option:invalid option number %d\n", option);
	  return SANE_STATUS_INVAL;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      switch (s->opt[option].type)
	{
	case SANE_TYPE_BOOL:
	case SANE_TYPE_INT:
	  DBG(16, "sane_control_option: set_value %s [#%d] to %d\n", 
	      name, option, *(SANE_Word *) val);
	  break;
	  
	case SANE_TYPE_FIXED:
	  DBG(16, "sane_control_option: set_value %s [#%d] to %f\n", 
	      name, option, SANE_UNFIX(*(SANE_Word *) val));
	  break;
	  
	case SANE_TYPE_STRING:
	  DBG(16, "sane_control_option: set_value %s [#%d] to %s\n", 
	      name, option, (char *) val);
	  break;
	  
	default:
	  DBG(16, "sane_control_option: set_value %s [#%d]\n", 
	      name, option);
	}

      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  /* make sure that paper-size is set to custom */
	  if (s->val[option].w != *(SANE_Word *) val)
	    {
	      if (info) *info |= SANE_INFO_RELOAD_PARAMS;

	      if (get_paper_id(_OPT_VAL_STRING(s, OPT_PAPER_SIZE)) != 0)
		{
		  if (info) *info |= SANE_INFO_RELOAD_OPTIONS;

		  /* set paper size to 'custom' */
		  free (s->val[OPT_PAPER_SIZE].s);
		  s->val[OPT_PAPER_SIZE].s = strdup(paper_list[0]);
		}
	    }
	  /* fall through */
	case OPT_RESOLUTION:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_TIMEOUT_ADF:
	case OPT_TIMEOUT_MANUAL:
	case OPT_ACE_FUNCTION:
	case OPT_ACE_SENSITIVITY:
	case OPT_BRIGHTNESS:
	case OPT_THRESHOLD:
	case OPT_CONTRAST:
	case OPT_ICON_WIDTH:
	case OPT_ICON_LENGTH:
	case OPT_BARCODE_SEARCH_COUNT:
	case OPT_BARCODE_HMIN:
	case OPT_BARCODE_SEARCH_TIMEOUT:
	case OPT_BARCODE_RELMAX:
	case OPT_BARCODE_BARMIN:
	case OPT_BARCODE_BARMAX:
	case OPT_BARCODE_CONTRAST:
	case OPT_BARCODE_PATCHMODE:
	case OPT_NUM_OPTS:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;
	  
	  /* string options */
	case OPT_BARCODE_SEARCH_BAR:
	  /*!!! we're supporting only a single barcode type via the option */
	  s->search_bars[0] = get_barcode_id(val);
	  /* fall through */
	case OPT_SCAN_SOURCE:
	case OPT_COMPRESSION:
	case OPT_ROTATION:
	case OPT_BARCODE_SEARCH_MODE:
	case OPT_SECTION:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;

	  /* boolean options: */
	case OPT_AUTOBORDER:
	  /*!!! autoborder true disables geometry controls
	   * and sets them to defaults?
	   */
	  /* fall through */
	case OPT_PREVIEW:
	case OPT_BATCH:
	case OPT_DESKEW:
	case OPT_CHECK_ADF:
	case OPT_DUPLEX:
	case OPT_NEGATIVE:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* options with side effects */
	case OPT_CONTROL_PANEL:
	  /* a boolean option */
	  /* control-panel true enables/disables some enhancement controls */
	  if (s->val[option].w != *(SANE_Word *) val)
	    {
	      if (info) *info |= SANE_INFO_RELOAD_OPTIONS;

	      s->val[option].w = *(SANE_Word *) val;

	      if (*(SANE_Word *) val == SANE_TRUE)
		{
		  if (s->hw->info.canACE == SANE_TRUE)
		    {
		      s->opt[OPT_ACE_FUNCTION].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_ACE_SENSITIVITY].cap |= SANE_CAP_INACTIVE;
		    }
		  else
		    {
		      s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
		    }
		}
	      else
		{
		  if (s->hw->info.canACE == SANE_TRUE)
		    {
		      s->opt[OPT_ACE_FUNCTION].cap &= ~SANE_CAP_INACTIVE;
		      s->opt[OPT_ACE_SENSITIVITY].cap &= ~SANE_CAP_INACTIVE;
		    }
		  else
		    {
		      s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		      s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
		    }
		}
	    }
	  return SANE_STATUS_GOOD;

	case OPT_SCAN_MODE:
	  /* a string option */
	  /* scan mode != lineart disables compression, setting it to
	   * 'none'
	   */
	  if (strcmp (s->val[option].s, (SANE_String) val))
	    {
	      if (info) *info |= SANE_INFO_RELOAD_OPTIONS;
	      if (get_scan_mode_id((SANE_String) val) != 0)
		{
		  /* scan mode is not lineart, disable compression 
		   * and set compression to 'none'
		   */
		  s->opt[OPT_COMPRESSION].cap |= SANE_CAP_INACTIVE;
		  if (s->val[OPT_COMPRESSION].s &&
		      get_compression_id(s->val[OPT_COMPRESSION].s) != 0)
		    {
		      free (s->val[OPT_COMPRESSION].s);
		      s->val[OPT_COMPRESSION].s = strdup(compression_list[0]);
		    }
		}
	      else
		{
		  /* scan mode is lineart, enable compression */
		  s->opt[OPT_COMPRESSION].cap &= ~SANE_CAP_INACTIVE;
		}
	      free (s->val[option].s);
	      s->val[option].s = strdup (val);
	    }
	  return SANE_STATUS_GOOD;

	case OPT_PAPER_SIZE:
	  /* a string option */
	  /* changes geometry options, therefore _RELOAD_PARAMS and _RELOAD_OPTIONS */
	  if (strcmp (s->val[option].s, (SANE_String) val))
	    {
	      SANE_Int paper_id = get_paper_id((SANE_String) val);

	      /* paper_id 0 is a special case (custom) that
	       * disables the paper size control of geometry
	       */
	      if (paper_id != 0)
		{
		  double left, x_max, y_max, x, y;

		  x_max = SANE_UNFIX(s->hw->info.x_range.max);
		  y_max = SANE_UNFIX(s->hw->info.y_range.max);
		  /* a dimension of 0.0 (or less) is replaced with the max value */
		  x = (paper_sizes[paper_id].width <= 0.0) ? x_max : 
		    paper_sizes[paper_id].width;
		  y = (paper_sizes[paper_id].length <= 0.0) ? y_max : 
		    paper_sizes[paper_id].length;

		  if (info) *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

		  /* set geometry options based on paper size */
		  /* set geometry options based on paper size */
		  if (s->hw->info.canADF)
		    {
		      /* when the feeder is used the paper is centered in the
		       * hopper; with the manual feed it is aligned left.
		       */
		      left = (x_max - x) / 2.0;
		      if (left < 0.0) left = 0.0;
		    }
		  else
		    {
		      left = 0.0;
		    }

		  s->val[OPT_TL_X].w = SANE_FIX(left); 
		  s->val[OPT_TL_Y].w = SANE_FIX(0.0); 
		  s->val[OPT_BR_X].w = SANE_FIX(MIN(x + left, x_max));
		  s->val[OPT_BR_Y].w = SANE_FIX(MIN(y, y_max));
		}
	      free (s->val[option].s);
	      s->val[option].s = strdup (val);
	    }
	  return SANE_STATUS_GOOD;

	default:
	  DBG(1, "sane_control_option:invalid option number %d\n", option);
	  return SANE_STATUS_INVAL;
	}
    }

  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  BH_Scanner *s = handle;
  SANE_Int status = SANE_STATUS_GOOD;

  DBG(3, "sane_get_parameters called\n");
  
  if (params)
    {
      SANE_Int res;

      if (!s->scanning)
	{
	  /* update our parameters ONLY if we're not scanning */
	  status = get_parameters(s, 0);
	}

      *params = s->params;

      res = _OPT_VAL_WORD(s, OPT_RESOLUTION);

      DBG (1, "get_parameters: format=%d, pixels/line=%d, bytes/line=%d, "
	   "lines=%d, dpi=%d\n", 
	   (int) s->params.format, 
	   s->params.pixels_per_line, 
	   s->params.bytes_per_line,
	   s->params.lines, 
	   res);
    }

  return status;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  BH_Scanner *s = handle;
  SANE_Status status;

  DBG(3, "sane_start called\n");
  s->cancelled = SANE_FALSE;

  if (s->scanning == SANE_FALSE)
    {
      /* get preliminary parameters */
      status = get_parameters (s, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_start: get_parameters failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* Do the setup once per 'batch'.  The SANE standard requires the
       * frontend to call sane_cancel once all desired frames have been 
       * acquired.  That is when scanning is set back to SANE_FALSE and
       * the 'batch' is considered done.
       */
      status = start_setup (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_start: start_setup failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  status = start_scan (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: start_scan failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  return SANE_STATUS_GOOD; 
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int maxlen, SANE_Int *len)
{
  BH_Scanner *s = handle;
  SANE_Status status;
  size_t nread;

  DBG(3, "sane_read called\n");

  *len = 0;

  if (s->cancelled) {
    DBG (3, "sane_read: cancelled!\n");
    return SANE_STATUS_CANCELLED;
  }

  if (!s->scanning) {
    DBG (3, "sane_read: scanning is false!\n");
    sane_cancel(s);
    return SANE_STATUS_CANCELLED;
  }

  nread = maxlen;

  DBG (3, "sane_read: request %lu bytes\n", (u_long) nread);
  /* set InvalidBytes to 0 before read; sense_handler will set it
   * to non-zero if we do the last partial read.
   */
  s->InvalidBytes = 0;
  status = read_data (s, buf, &nread);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_read: read_data failed %s\n",
	   sane_strstatus(status));
      sane_cancel (s);
      return status;
    }
  nread = maxlen - s->InvalidBytes;
  DBG (3, "sane_read: got %lu bytes\n", (u_long) nread);
  *len = nread;

  return (maxlen != 0 && nread == 0) ? SANE_STATUS_EOF : SANE_STATUS_GOOD;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
#ifdef NONBLOCKSUPPORTED
  BH_Scanner *s = handle;
#endif
  DBG(3, "sane_set_io_mode called: non_blocking=%d\n", non_blocking);

#ifdef NONBLOCKSUPPORTED
  if (s->fd < 0) 
    { 
      return SANE_STATUS_INVAL; 
    }

  if (fcntl (s->fd, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    { 
      DBG(1, "sane_set_io_mode: error setting io mode\n");
      return SANE_STATUS_IO_ERROR;
    }

 return SANE_STATUS_GOOD;
#else
 handle = handle; /* get rid of compiler warning */
 return (non_blocking == 1) ? SANE_STATUS_UNSUPPORTED : SANE_STATUS_GOOD;
#endif
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
#ifdef NONBLOCKSUPPORTED
  BH_Scanner *s = handle;
#endif
  DBG(3, "sane_get_select_fd called\n");

#ifdef NONBLOCKSUPPORTED
  if (s->fd < 0)
    { 
      return SANE_STATUS_INVAL; 
    }
  *fd = s->fd;

  return SANE_STATUS_GOOD;
#else
  handle = handle; fd = fd; /* get rid of compiler warning */
  return SANE_STATUS_UNSUPPORTED;
#endif
}

void
sane_cancel (SANE_Handle handle)
{
  BH_Scanner *s = (BH_Scanner *) handle;
  DBG(3, "sane_cancel called\n");
  if (s->scanning)
    {
      /* if batchmode is enabled, then call set_window to 
       * abort the batch
       */
      if (_OPT_VAL_WORD(s, OPT_BATCH) == SANE_TRUE)
	{
	  DBG(5, "sane_cancel: calling set_window to abort batch\n");
	  set_window(s, BH_BATCH_ABORT);
	}  
    }
  s->scanning = SANE_FALSE;
  s->cancelled = SANE_TRUE;
}

void
sane_close (SANE_Handle handle)
{
  BH_Scanner *s = (BH_Scanner *) handle;
  DBG(3, "sane_close called\n");

  if (s->fd != -1)
    sanei_scsi_close (s->fd);
  s->fd = -1;
  free (s);
}

void
sane_exit (void)
{
  BH_Device *dev, *next;
  DBG(3, "sane_exit called\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev);
    }
  
  if (devlist)
    free (devlist);
}

