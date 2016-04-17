/* ------------------------------------------------------------------------- */
/* sane - Scanner Access Now Easy.
   coolscan.c , version  0.4.4

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

   This file implements a SANE backend for COOLSCAN flatbed scanners.  */

/* ------------------------------------------------------------------------- */


/* SANE-FLOW-DIAGRAMM

   - sane_init() : initialize backend, attach scanners
   . - sane_get_devices() : query list of scanner-devices
   . - sane_open() : open a particular scanner-device
   . . - sane_set_io_mode : set blocking-mode
   . . - sane_get_select_fd : get scanner-fd
   . . - sane_get_option_descriptor() : get option informations
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image aquisition
   . .   - sane_get_parameters() : returns actual scan-parameters
   . .   - sane_read() : read image-data (from pipe)
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner-device
   - sane_exit() : terminate use of backend
 */

#ifdef _AIX
# include "lalloca.h"		/* MUST come first for AIX! */
#endif

#include "../include/sane/config.h"
#include "lalloca.h"

#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_thread.h"

#include "../include/sane/sanei_config.h"
#define COOLSCAN_CONFIG_FILE "coolscan.conf"
#include "../include/sane/sanei_backend.h"

#include "coolscan.h"
#include "coolscan-scsidef.h"


#ifndef PATH_MAX
#define PATH_MAX       1024
#endif

/* ------------------------------------------------------------------------- */
static const SANE_Int resolution_list[] =
{
  25,
  2700, 1350, 900, 675, 540, 450, 385, 337, 300, 270, 245, 225, 207,
  192, 180, 168, 158, 150, 142, 135, 128, 122, 117, 112, 108
};


#define coolscan_do_scsi_open(dev, fd, handler) sanei_scsi_open(dev, fd, handler)
#define coolscan_do_scsi_close(fd)              sanei_scsi_close(fd)

#define	COOLSCAN_MAX_RETRY	25


static SANE_Status sense_handler (int scsi_fd, unsigned char * result, void *arg);
static int coolscan_check_values (Coolscan_t * s);
static int get_internal_info (Coolscan_t *);
static void coolscan_get_inquiry_values (Coolscan_t *);
static void hexdump (int level, char *comment, unsigned char *p, int l);
static int swap_res (Coolscan_t * s);
/* --------------------------- COOLSCAN_DO_SCSI_CMD  ----------------------- */
static int
do_scsi_cmd (int fd, unsigned char *cmd, int cmd_len, unsigned char *out, size_t out_len)
{
  int ret;
  size_t ol = out_len;

  hexdump (20, "", cmd, cmd_len);

  ret = sanei_scsi_cmd (fd, cmd, cmd_len, out, &ol);
  if ((out_len != 0) && (out_len != ol))
    {
      DBG (1, "sanei_scsi_cmd: asked %lu bytes, got %lu\n",
	   (u_long) out_len, (u_long) ol);
    }
  if (ret)
    {
      DBG (1, "sanei_scsi_cmd: returning 0x%08x\n", ret);
    }
  DBG (10, "sanei_scsi_cmd: returning %lu bytes:\n", (u_long) ol);
  if (out != NULL && out_len != 0)
    hexdump (15, "", out, (out_len > 0x60) ? 0x60 : out_len);

  return ret;
}


static int
request_sense_parse (unsigned char *sensed_data)
{
  int ret, sense, asc, ascq;
  sense = get_RS_sense_key (sensed_data);
  asc = get_RS_ASC (sensed_data);
  ascq = get_RS_ASCQ (sensed_data);

  ret = SANE_STATUS_IO_ERROR;

  switch (sense)
    {
    case 0x0:
      DBG (5, "\t%d/%d/%d: Scanner ready\n", sense, asc, ascq);
      return SANE_STATUS_GOOD;

    case 0x1:
      if ((0x37 == asc) && (0x00 == ascq)) {
	DBG (1, "\t%d/%d/%d: Rounded Parameter\n", sense, asc, ascq);
        ret = SANE_STATUS_GOOD;
      }
      else if ((0x61 == asc) && (0x02 == ascq))
	DBG (1, "\t%d/%d/%d: Out Of Focus\n", sense, asc, ascq);
      else
	DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
      break;

    case 0x2:
      if ((0x4 == asc) && (0x1 == ascq)) {
	DBG (10, "\t%d/%d/%d: Logical unit is in process of becoming ready\n",
	     sense, asc, ascq);
	ret = SANE_STATUS_DEVICE_BUSY;
      }
      else if ((0x3A == asc) && (0x00 == ascq))
	{
	  DBG (1, "\t%d/%d/%d: No Diapo inserted\n", sense, asc, ascq);
          ret = SANE_STATUS_GOOD;
	}
      else if ((0x60 == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Lamp Failure\n", sense, asc, ascq);
      else
	{
	  DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
          ret = SANE_STATUS_GOOD;
	}
      break;

    case 0x3:
      if ((0x3b == asc) && (0xe == ascq))
	DBG (1, "\t%d/%d/%d: Medium source element empty\n", sense, asc, ascq);
      else if ((0x53 == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Media Load of Eject Failed\n", sense, asc, ascq);
      else
	DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
      break;

    case 0x4:
      if ((0x15 == asc) && (0x1 == ascq))
	DBG (1, "\t%d/%d/%d: Mechanical Positioning Error\n", sense, asc, ascq);
      else
	DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
      break;

    case 0x5:
      if ((0x00 == asc) && (0x5 == ascq))
	DBG (1, "\t%d/%d/%d: End-Of-Data Detected\n", sense, asc, ascq);
      else if ((0x1a == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Parameter List Length Error\n", sense, asc, ascq);
      else if ((0x20 == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Invalid Command Operation Code\n", sense, asc, ascq);
      else if ((0x24 == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Invalid Field In CDB\n", sense, asc, ascq);
      else if ((0x25 == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Logical Unit Not Supported\n", sense, asc, ascq);
      else if ((0x26 == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Invalid Field in Parameter List\n", sense, asc, ascq);
      else if ((0x2c == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Command Sequence Error\n", sense, asc, ascq);
      else if ((0x39 == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Saving Parameters Not Supported\n", sense, asc, ascq);
      else if ((0x3d == asc) && (0x00 == ascq))
	DBG (1, "\t%d/%d/%d: Invalid Bits In Identify Message\n", sense, asc, ascq);
      else
	DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
      break;

    case 0x6:
      if ((0x29 == asc) && (0x0 == ascq))
	DBG (1, "\t%d/%d/%d: Power On, Reset, or Bus Device Reset Occured\n", sense, asc, ascq);
      else if ((0x2a == asc) && (0x1 == ascq))
	DBG (1, "\t%d/%d/%d: Mode Parameters Changed\n", sense, asc, ascq);
      else
	DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
      break;

    case 0xb:
      if ((0x43 == asc) && (0x0 == ascq))
	DBG (1, "\t%d/%d/%d: Message Error\n", sense, asc, ascq);
      else if ((0x47 == asc) && (0x0 == ascq))
	DBG (1, "\t%d/%d/%d: SCSI Parity Error\n", sense, asc, ascq);
      else if ((0x48 == asc) && (0x0 == ascq))
	DBG (1, "\t%d/%d/%d: Initiator Detected Error Message Received\n", sense, asc, ascq);
      else if ((0x49 == asc) && (0x0 == ascq))
	DBG (1, "\t%d/%d/%d: Invalid Message Error\n", sense, asc, ascq);
      else if ((0x4e == asc) && (0x0 == ascq))
	DBG (1, "\t%d/%d/%d: Overlapped Commands Attempted\n", sense, asc, ascq);
      else
	DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
      break;

    default:
      DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
      break;
    }				/* switch */
  return ret;
}

/* 
 *  wait_scanner should spin until TEST_UNIT_READY returns 0 (GOOD)
 *  returns 0 on success,
 *  returns -1 on error.  
 */
static int
wait_scanner (Coolscan_t * s)
{
  int ret = -1;
  int cnt = 0;
  DBG (10, "wait_scanner: Testing if scanner is ready\n");

  while (ret != 0)
    {
      ret = do_scsi_cmd (s->sfd, test_unit_ready.cmd,
			 test_unit_ready.size, 0, 0);

      if (ret == SANE_STATUS_DEVICE_BUSY)
	{
	  usleep (500000);	/* wait 0.5 seconds */
	  if (cnt++ > 40)
	    {			/* 20 sec. max (prescan takes up to 15 sec. */
	      DBG (1, "wait_scanner: scanner does NOT get ready\n");
	      return -1;
	    }
	}
      else if (ret == SANE_STATUS_GOOD)
	{
	  DBG (10, "wait_scanner: scanner is ready\n");
	  return ret;
	}
      else
	{
	  DBG (1, "wait_scanner: test unit ready failed (%s)\n",
	       sane_strstatus (ret));
	}
    }
  return 0;
}

/* ------------------------- COOLSCAN GRAB SCANNER ----------------------------- */


/* coolscan_grab_scanner should go through the following command sequence:
 * TEST UNIT READY
 *     CHECK CONDITION  \
 * REQUEST SENSE         > These should be handled automagically by
 *     UNIT ATTENTION   /  the kernel if they happen (powerup/reset)
 * TEST UNIT READY
 *     GOOD
 * RESERVE UNIT
 *     GOOD
 * 
 * It is then responsible for installing appropriate signal handlers
 * to call emergency_give_scanner() if user aborts.
 */

static int
coolscan_grab_scanner (Coolscan_t * s)
{
  int ret;

  DBG (10, "grabbing scanner\n");

  wait_scanner (s);		/* wait for scanner ready, if not print 
				   sense and return 1 */
  ret = do_scsi_cmd (s->sfd, reserve_unit.cmd, reserve_unit.size, NULL, 0);
  if (ret)
    return ret;

  DBG (10, "scanner reserved\n");
  return 0;
}

/* 
 * Convert a size in ilu to the units expected by the scanner
 */

static int
resDivToVal (int res_div)
{ 
  if (res_div < 1 || res_div > resolution_list[0])
    { 
      DBG (1, "Invalid resolution divisor %d \n", res_div);
      return 2700;
    }
  else
    {
      return resolution_list[res_div];
    }
}

static int
resValToDiv (int res_val)
{
  int res_div;
  int max_res = resolution_list[0];
  for (res_div = 1; res_div <= max_res; res_div++)
    {
      if (resolution_list[res_div] == res_val)
	break;
    }
  if (res_div > max_res)
    {
      DBG (1, "Invalid resolution value\n");
      return 1;
    }
  else
    {
      return res_div;
    }
}
/* 
 * use mode select to force a mesurement divisor of 2700
 */
static unsigned char mode_select[] =
{
  MODE_SELECT, 0x10, 0, 0, 20, 0,
  0, 0, 0, 8,
  0, 0, 0, 0, 0, 0, 0, 1,
  3, 6, 0, 0, 0xA, 0x8C, 0, 0};

static int
select_MUD (Coolscan_t * s)
{
  return do_scsi_cmd (s->sfd, mode_select, 26, NULL, 0);
}

static int
coolscan_autofocus_LS30 (Coolscan_t * s)
{
  int x, y;

  wait_scanner(s);
  memcpy(s->buffer, autofocusLS30.cmd, autofocusLS30.size);
  memcpy(s->buffer+ autofocusLS30.size, autofocuspos, 9);

  x = s->xmaxpix - (s->brx + s->tlx) / 2;
  y = (s->bry + s->tly) / 2;

  DBG (10, "Attempting AutoFocus at x=%d, y=%d\n", x, y);

  do_scsi_cmd (s->sfd, s->buffer,
	       autofocusLS30.size  + 9, NULL, 0);
  /* Trashes when used in combination with scsi-driver AM53C974.o  */
  do_scsi_cmd (s->sfd, command_c1.cmd,
	       command_c1.size, NULL, 0);
  
  DBG (10, "\tWaiting end of Autofocus\n");
  wait_scanner (s);
  DBG (10, "AutoFocused.\n");
  return 0;
}

static int
coolscan_autofocus (Coolscan_t * s)
{
  int x, y;

  if(s->LS>=2)
    { return coolscan_autofocus_LS30(s);
    }

  wait_scanner(s);
  memcpy(s->buffer, autofocus.cmd, autofocus.size);

  x = s->xmaxpix - (s->brx + s->tlx) / 2;
  y = (s->bry + s->tly) / 2;

  DBG (10, "Attempting AutoFocus at x=%d, y=%d\n", x, y);

  set_AF_XPoint (s->buffer, x);
  set_AF_YPoint (s->buffer, y);

  set_AF_transferlength (s->buffer, 0); /* should be 8 !*/
  do_scsi_cmd (s->sfd, s->buffer,
	       autofocus.size  + AF_Point_length, NULL, 0);

  sleep(5);		 	/* autofocus takes a minimum of 5 sec. */

  DBG (10, "\tWaiting end of Autofocus\n");
  wait_scanner (s);
  DBG (10, "AutoFocused.\n");
  return 0;
}

/*
   static int
   coolscan_abort_scan (Coolscan_t * s)
   {
   int ret;

   DBG (5, "Aborting scan...\n");
   ret = do_scsi_cmd (s->sfd, sabort.cmd, sabort.size, NULL, 0);
   if (ret)
   DBG (5, "Scan Aborted\n");
   else
   DBG (5, "Not scanning\n");
   return 0;
   }
 */
static int
coolscan_mode_sense (Coolscan_t * s)
{
  int ret, len;

  DBG (10, "Mode Sense...\n");
  len = 12;
  set_MS_DBD (mode_sense.cmd, 1);
  set_MS_len (mode_sense.cmd, len);
  ret = do_scsi_cmd (s->sfd, mode_sense.cmd, mode_sense.size,
		     s->buffer, len);

  if (ret == 0)
    {
      s->MUD = get_MS_MUD (s->buffer);
      DBG (10, "\tMode Sensed (MUD is %d)\n", s->MUD);
    }
  return ret;
}

static int
coolscan_object_discharge (Coolscan_t * s)
{
  int ret;

  DBG (10, "Trying to discharge object...\n");

  memcpy (s->buffer, object_position.cmd, object_position.size);
  set_OP_autofeed (s->buffer, OP_Discharge);
  ret = do_scsi_cmd (s->sfd, s->buffer,
		     object_position.size, NULL, 0);
  wait_scanner (s);
  DBG (10, "Object discharged.\n");
  return ret;
}

static int
coolscan_object_feed (Coolscan_t * s)
{
  int ret;
  DBG (10, "Trying to feed object...\n");
  if (!s->asf)
    {
      DBG (10, "\tAutofeeder not present.\n");
      return 0;
    }
  memcpy (s->buffer, object_position.cmd, object_position.size);
  set_OP_autofeed (s->buffer, OP_Feed);
  ret = do_scsi_cmd (s->sfd, s->buffer,
		     object_position.size, NULL, 0);
  wait_scanner (s);
  DBG (10, "Object fed.\n");
  return ret;
}

/* coolscan_give_scanner should go through the following sequence:
 * OBJECT POSITION DISCHARGE
 *     GOOD
 * RELEASE UNIT
 *     GOOD
 */
static int
coolscan_give_scanner (Coolscan_t * s)
{
  DBG (10, "trying to release scanner ...\n");
  coolscan_object_discharge (s);
  wait_scanner (s);
  do_scsi_cmd (s->sfd, release_unit.cmd, release_unit.size, NULL, 0);
  DBG (10, "scanner released\n");
  return 0;
}


static int
coolscan_set_window_param_LS20 (Coolscan_t * s, int prescan)
{
  unsigned char buffer_r[max_WDB_size];
  int ret;

  wait_scanner (s);
  memset (buffer_r, '\0', max_WDB_size);	/* clear buffer */
  memcpy (buffer_r, window_descriptor_block.cmd,
	  window_descriptor_block.size);	/* copy preset data */

  set_WD_wid (buffer_r, WD_wid_all);	/* window identifier */
  set_WD_auto (buffer_r, s->set_auto);	/* 0 or 1: don't know what it is */

  set_WD_negative (buffer_r, s->negative);	/* Negative/positive slide */

  if (prescan)
    {
      set_WD_scanmode (buffer_r, WD_Prescan);
    }
  else
    {
      set_WD_scanmode (buffer_r, WD_Scan);

      /* geometry */
      set_WD_Xres (buffer_r, resDivToVal (s->x_nres));	/* x resolution in dpi */
      set_WD_Yres (buffer_r, resDivToVal (s->y_nres));	/* y resolution in dpi */

      /* the coolscan  uses the upper right corner as the origin of coordinates */
      /* xmax and ymax are given in 1200 dpi */
      set_WD_ULX (buffer_r, (s->xmaxpix - s->brx));
      set_WD_ULY (buffer_r, s->tly);	/* upper_edge y */
      set_WD_width (buffer_r, (s->brx - s->tlx + 1));
      set_WD_length (buffer_r, (s->bry - s->tly + 1));

      /* BTC */
      if (s->brightness == 128)
	{
	  set_WD_brightness (buffer_r, 0);
	}
      else
	{
	  set_WD_brightness (buffer_r, s->brightness);	/* brightness */
	}

      if (s->contrast == 128)
	{
	  set_WD_contrast (buffer_r, 0);
	}
      else
	{
	  set_WD_contrast (buffer_r, s->contrast);	/* contrast */
	}

      /* scanmode */
      if (s->colormode == GREYSCALE)
	set_WD_composition (buffer_r, WD_comp_grey);	/* GRAY composition */
      else
	set_WD_composition (buffer_r, WD_comp_rgb_full);	/* RGB composition */

      set_WD_dropoutcolor (buffer_r, s->dropoutcolor);	/* Which color to scan with when grayscale scan */
      set_WD_transfermode (buffer_r, WD_LineSequence);
      set_WD_gammaselection (buffer_r, s->gammaselection);	/* monitor/linear */

      set_WD_shading (buffer_r, WD_Shading_ON);		/* default for non-manufacturing */

      if (1 == s->LS)
	{			/* Analog gamma reserved on LS-1000 */
	  set_WD_analog_gamma_R (buffer_r, 0);
	  set_WD_analog_gamma_G (buffer_r, 0);
	  set_WD_analog_gamma_R (buffer_r, 0);
	}
      else
	{
	  /* Quote spec: "It is recomended that analog gamma bits 5, 4 and 3 be
	   * set to 1 (OFF) when the object type of byte 48 is positive and the
	   * gamma specificateion of byte 51 is linear, and to 0 (ON) in all
	   * other cases." */
	  /*
	  int foo;
	  if ((buffer_r[48] == WD_Positive) && (buffer_r[51] == WD_Linear))
	    foo = WD_Analog_Gamma_OFF;
	  else
	    foo = WD_Analog_Gamma_ON;
	  set_WD_analog_gamma_R (buffer_r, foo);
	  set_WD_analog_gamma_G (buffer_r, foo);
	  set_WD_analog_gamma_B (buffer_r, foo);
	  */
	  set_WD_analog_gamma_R (buffer_r, s->analog_gamma_r);
	  set_WD_analog_gamma_G (buffer_r, s->analog_gamma_g);
	  set_WD_analog_gamma_B (buffer_r, s->analog_gamma_b);
	  if (s->gamma_bind)
	    {
	      set_WD_LUT_R (buffer_r, 1);
	      set_WD_LUT_G (buffer_r, 1);
	      set_WD_LUT_B (buffer_r, 1);
	    }
	  else
	    {
	      set_WD_LUT_R (buffer_r, 1);
	      set_WD_LUT_G (buffer_r, 2);
	      set_WD_LUT_B (buffer_r, 3);
	    }
	}
      set_WD_averaging (buffer_r, s->averaging);

      set_WD_brightness_R (buffer_r, s->brightness_R);
      set_WD_brightness_G (buffer_r, s->brightness_G);
      set_WD_brightness_B (buffer_r, s->brightness_B);

      set_WD_contrast_R (buffer_r, s->contrast_R);
      set_WD_contrast_G (buffer_r, s->contrast_G);
      set_WD_contrast_B (buffer_r, s->contrast_B);

      set_WD_exposure_R (buffer_r, s->exposure_R);
      set_WD_exposure_G (buffer_r, s->exposure_G);
      set_WD_exposure_B (buffer_r, s->exposure_B);
      set_WD_shift_R (buffer_r, s->shift_R);
      set_WD_shift_G (buffer_r, s->shift_G);
      set_WD_shift_B (buffer_r, s->shift_B);

 
      /* FIXME: LUT-[RGB] */
      /* FIXME: stop on/off */
    }

  DBG (10, "\tx_nres=%d, y_nres=%d, upper left-x=%d, upper left-y=%d\n",
       s->x_nres, s->y_nres, s->tlx, s->tly);
  DBG (10, "\twindow width=%d, MUD=%d, brx=%d\n",
       s->brx - s->tlx, s->MUD, s->brx);
  DBG (10, "\tcolormode=%d, bits per pixel=%d\n",
       s->colormode, s->bits_per_color);
  DBG (10, "\tnegative=%d, dropoutcolor=%d, preview=%d, transfermode=%d, gammasel=%d\n",
       s->negative, s->dropoutcolor, s->preview, s->transfermode,
       s->gammaselection);

  /* prepare SCSI-BUFFER */
  memcpy (s->buffer, set_window.cmd, set_window.size);	/* SET-WINDOW cmd */
  memcpy ((s->buffer + set_window.size),	/* add WPDB */
	  window_parameter_data_block.cmd,
	  window_parameter_data_block.size);
  set_WPDB_wdblen ((s->buffer + set_window.size), used_WDB_size);	/* set WD_len */
  memcpy (s->buffer + set_window.size + window_parameter_data_block.size,
	  buffer_r, window_descriptor_block.size);

  hexdump (15, "Window set", buffer_r, s->wdb_len);

  set_SW_xferlen (s->buffer, (window_parameter_data_block.size +
			      window_descriptor_block.size));

  ret = do_scsi_cmd (s->sfd, s->buffer, set_window.size +
		     window_parameter_data_block.size +
		     window_descriptor_block.size,
		     NULL, 0);
  DBG (10, "window set.\n");
  return ret;
}

static int
coolscan_set_window_param_LS30 (Coolscan_t * s, int wid, int prescan)
{
  unsigned char buffer_r[max_WDB_size];
  int ret;

  wait_scanner (s);
  memset (buffer_r, '\0', max_WDB_size);	/* clear buffer */
  memcpy (buffer_r, window_descriptor_block_LS30.cmd,
	  window_descriptor_block_LS30.size);	/* copy preset data */

  set_WD_wid (buffer_r, wid);          	/* window identifier */
  set_WD_auto (buffer_r, s->set_auto);	/* 0 or 1: don't know what it is */

  /* geometry */
  set_WD_Xres (buffer_r, resDivToVal (s->x_nres));	/* x resolution in dpi */
  set_WD_Yres (buffer_r, resDivToVal (s->y_nres));	/* y resolution in dpi */

  if (prescan)
    {
      set_WD_scanmode_LS30 (buffer_r, WD_Prescan);
      set_WD_Xres (buffer_r, resDivToVal (1));	/* x res. in dpi */
      set_WD_Yres (buffer_r, resDivToVal (1));	/* y res. in dpi */
      buffer_r[0x29]=0x81;
      buffer_r[0x2a]=0x04;
      buffer_r[0x2b]=0x02;
      buffer_r[0x2c]=0x01;
      buffer_r[0x2d]=0xff;
      buffer_r[0x30]=0x00;
      buffer_r[0x31]=0x00;
      buffer_r[0x32]=0x00;
      buffer_r[0x33]=0x00;
      set_WD_width (buffer_r,(2592));
      set_WD_length (buffer_r,(3894));
    }
  else
    {
      set_WD_scanmode_LS30 (buffer_r, WD_Scan);

      /* the coolscan LS-30 uses the upper left corner 
	 as the origin of coordinates */
      /* xmax and ymax are given in 1200 dpi */
      set_WD_ULX (buffer_r, s->tlx);
      set_WD_ULY (buffer_r, s->tly);	/* upper_edge y */
      set_WD_width (buffer_r, (s->brx - s->tlx+1));
      set_WD_length (buffer_r, (s->bry - s->tly+1));

      /* BTC */
      if (s->brightness == 128)
	{
	  buffer_r[0x32]=0x00;
	}
      else
	{
	  buffer_r[0x32]=s->brightness;	/* brightness */
	}

      if (s->contrast == 128)
	{
	  buffer_r[0x33]=0x00;
	}
      else
	{
	  buffer_r[0x33]=s->contrast;	/* contrast */
	}

      /* scanmode */
      if (s->colormode == GREYSCALE)
	set_WD_composition (buffer_r, WD_comp_grey);	/* GRAY composition */
      else
	set_WD_composition (buffer_r, WD_comp_rgb_full);	/* RGB composition */

      set_WD_composition (buffer_r, WD_comp_rgb_full);  /* allways RGB composition */

      /* Bits per pixel */
      set_WD_bitsperpixel(buffer_r, s->bits_per_color);

      buffer_r[0x29]=0x81;
      buffer_r[0x2a]=0x01;
      buffer_r[0x2b]=0x02;
      buffer_r[0x2c]=0x01;
      buffer_r[0x2d]=0xff;
      buffer_r[0x30]=0x00;

    }
    set_WD_negative_LS30(buffer_r, s->negative);	/* Negative/positive slide */

    switch(wid)
    { case 1:  set_gain_LS30(buffer_r,(s->exposure_R*s->pretv_r)/50);	
                 break;
      case 2:  set_gain_LS30(buffer_r,(s->exposure_G*s->pretv_g)/50);	
                 break;
      case 3:  set_gain_LS30(buffer_r,(s->exposure_B*s->pretv_b)/50);	
                 break;
    }

  DBG (10, "\texpo_r=%d, expo_g=%d, expob=%d\n",
       s->exposure_R, s->exposure_G, s->exposure_B);
  DBG (10, "\tpre_r=%d, pre_g=%d, preb=%d\n",
       s->pretv_r, s->pretv_g, s->pretv_b);
  DBG (10, "\tx_nres=%d, y_nres=%d, upper left-x=%d, upper left-y=%d\n",
       s->x_nres, s->y_nres, s->tlx, s->tly);
  DBG (10, "\twindow width=%d, MUD=%d, brx=%d\n",
       s->brx - s->tlx, s->MUD, s->brx);
  DBG (10, "\tcolormode=%d, bits per pixel=%d\n",
       s->colormode, s->bits_per_color);
  DBG (10, "\tnegative=%d, dropoutcolor=%d, preview=%d, transfermode=%d, gammasel=%d\n",
       s->negative, s->dropoutcolor, s->preview, s->transfermode,
       s->gammaselection);

  /* prepare SCSI-BUFFER */
  memcpy (s->buffer, set_window.cmd, set_window.size);	/* SET-WINDOW cmd */
  memcpy ((s->buffer + set_window.size),	/* add WPDB */
	  window_parameter_data_block.cmd,
	  window_parameter_data_block.size);
  set_WPDB_wdblen ((s->buffer + set_window.size), used_WDB_size_LS30);	/* set WD_len */
  memcpy (s->buffer + set_window.size + window_parameter_data_block.size,
	  buffer_r, window_descriptor_block_LS30.size);

  hexdump (15, "Window set", buffer_r, s->wdb_len);

  set_SW_xferlen (s->buffer, (window_parameter_data_block.size +
			      window_descriptor_block_LS30.size));

  ret = do_scsi_cmd (s->sfd, s->buffer, set_window.size +
		     window_parameter_data_block.size +
		     window_descriptor_block_LS30.size,
		     NULL, 0);
  DBG (10, "window set.\n");
  return ret;
}

static int
coolscan_set_window_param (Coolscan_t * s, int prescan)
{
  int ret;
  ret=0;
  DBG (10, "set_window_param\n");
  
  if(s->LS<2)                   /* distinquish between old and new scanners */
  { ret=coolscan_set_window_param_LS20 (s,prescan);
  }
  else
  {  do_scsi_cmd (s->sfd,commande1.cmd,commande1.size,s->buffer,0x0d);
     wait_scanner (s);
     wait_scanner (s);
     coolscan_set_window_param_LS30(s,1,prescan);
     ret=coolscan_set_window_param_LS30(s,2,prescan);
     ret=coolscan_set_window_param_LS30(s,3,prescan);     
     if(s->colormode&0x08)
     { ret=coolscan_set_window_param_LS30(s,9,prescan);
     }
  }
  return ret;
}


/* 
 * The only purpose of get_window is debugging. None of the return parameters
 * is currently used. 
 */
static int
coolscan_get_window_param_LS30 (Coolscan_t * s, int wid,int prescanok)
{
  int translen;
  unsigned char *buf;

  DBG (10, "GET_WINDOW_PARAM\n");
  /*  wait_scanner (s); */

  translen = window_parameter_data_block.size + window_descriptor_block_LS30.size;

  /* prepare SCSI-BUFFER */
  memset (s->buffer, '\0', max_WDB_size);	/* clear buffer */

  set_SW_xferlen (get_window.cmd, translen);	/* Transfer length */
  get_window.cmd[5]= wid;                     	/* window identifier */

  hexdump (15, "Get window cmd", get_window.cmd, get_window.size);
  do_scsi_cmd (s->sfd, get_window.cmd, get_window.size,
		     s->buffer, translen);

  buf = s->buffer + window_parameter_data_block.size;
  hexdump (10, "Window get", buf, 117);

  s->brightness = buf[0x32];	/* brightness */
  s->contrast = buf[0x33];	/* contrast */
  DBG (10, "\tbrightness=%d, contrast=%d\n", s->brightness, s->contrast);

  /* Useful? */
  s->bits_per_color = get_WD_bitsperpixel (buf);	/* bits/pixel (8) */

  DBG (10, "\tcolormode=%d, bits per pixel=%d\n",
       s->colormode, s->bits_per_color);

  if(prescanok)
  { switch(wid)
    { case 1: s->pretv_r = get_gain_LS30(buf);	
              break;
      case 2: s->pretv_g = get_gain_LS30(buf);	
              break;
      case 3: s->pretv_b = get_gain_LS30(buf);	
            break;
    }
  }

  /* Should this one be set at all, here? */
  s->transfermode = get_WD_transfermode (buf);

  s->gammaselection = get_WD_gammaselection (buf);	/* monitor/linear */
  DBG (10, "\tpre_r=%d, pre_g=%d, preb=%d\n",
       s->pretv_r, s->pretv_g, s->pretv_b);

  DBG (5, "\tnegative=%d, dropoutcolor=%d, preview=%d, transfermode=%d, gammasel=%d\n",
       s->negative, s->dropoutcolor, s->preview, s->transfermode,
       s->gammaselection);

  DBG (10, "get_window_param - return\n");
  return 0;
}

/* 
 * The only purpose of get_window is debugging. None of the return parameters
 * is currently used. 
 */
static int
coolscan_get_window_param_LS20 (Coolscan_t * s)
{
  int translen;
  unsigned char *buf;

  DBG (10, "GET_WINDOW_PARAM\n");
  wait_scanner (s);

  translen = window_parameter_data_block.size + window_descriptor_block.size;

  /* prepare SCSI-BUFFER */
  memset (s->buffer, '\0', max_WDB_size);	/* clear buffer */

  set_SW_xferlen (get_window.cmd, translen);	/* Transfer length */

  hexdump (15, "Get window cmd", get_window.cmd, get_window.size);
  do_scsi_cmd (s->sfd, get_window.cmd, get_window.size,
		     s->buffer, translen);

  buf = s->buffer + window_parameter_data_block.size;
  hexdump (10, "Window get", buf, 117);

  /* BTC */
  s->brightness = get_WD_brightness (buf);	/* brightness */
  s->contrast = get_WD_contrast (buf);	/* contrast */
  DBG (10, "\tbrightness=%d, contrast=%d\n", s->brightness, s->contrast);

  if (WD_comp_gray == get_WD_composition (buf))
    s->colormode = GREYSCALE;
  else
    s->colormode = RGB;

  /* Useful? */
  s->bits_per_color = get_WD_bitsperpixel (buf);	/* bits/pixel (8) */

  DBG (10, "\tcolormode=%d, bits per pixel=%d\n",
       s->colormode, s->bits_per_color);


  s->dropoutcolor = get_WD_dropoutcolor (buf);	/* Which color to scan with when grayscale scan */

  /* Should this one be set at all, here? */
  s->transfermode = get_WD_transfermode (buf);

  s->gammaselection = get_WD_gammaselection (buf);	/* monitor/linear */

  DBG (5, "\tnegative=%d, dropoutcolor=%d, preview=%d, transfermode=%d, gammasel=%d\n",
       s->negative, s->dropoutcolor, s->preview, s->transfermode,
       s->gammaselection);

  /* Should this one be set at all? */
  s->shading = get_WD_shading (buf);
  s->averaging = get_WD_averaging (buf);
  DBG (10, "get_window_param - return\n");
  return 0;
}

/* 
 * The only purpose of get_window is debugging. None of the return parameters
 * is currently used. 
 */
static int
coolscan_get_window_param (Coolscan_t * s, int prescanok)
{
  int ret;
  DBG (10, "get_window_param\n");

  ret=0;  
  if(s->LS<2)                   /* distinquish between old and new scanners */
  { ret=coolscan_get_window_param_LS20 (s);
  }
  else
  {  
     ret=coolscan_get_window_param_LS30(s,1,prescanok);
     ret=coolscan_get_window_param_LS30(s,2,prescanok);
     ret=coolscan_get_window_param_LS30(s,3,prescanok);
     if(s->colormode&0x08)
     { ret=coolscan_get_window_param_LS30(s,9,prescanok);
     }
  }
  return ret;
}

static int
coolscan_start_scanLS30 (Coolscan_t * s)
{ int channels;
  DBG (10, "starting scan\n");

  channels=1;
  memcpy (s->buffer, scan.cmd, scan.size);
  switch(s->colormode)
    {  case RGB:
       case GREYSCALE:	 
	       channels=s->buffer[4]=0x03; /* window 1 */
               s->buffer[6]=0x01; /* window 1 */
	       s->buffer[7]=0x02; /* window 2 */
	       s->buffer[8]=0x03; /* window 3 */  
	       
              break;      
       case RGBI:
	       channels=s->buffer[4]=0x04; /* window 1 */
               s->buffer[6]=0x01; /* window 1 */
	       s->buffer[7]=0x02; /* window 2 */
	       s->buffer[8]=0x03; /* window 3 */  
	       s->buffer[9]=0x09; /* window 3 */  
              break; 
       case IRED:
	       channels=s->buffer[4]=0x01; /* window 1 */
	       s->buffer[8]=0x09; /* window 3 */  
              break; 
    }

  return do_scsi_cmd (s->sfd, s->buffer, scan.size+channels, NULL, 0);
}

static int
coolscan_start_scan (Coolscan_t * s)
{
  DBG (10, "starting scan\n");
  if(s->LS>=2)
    { return coolscan_start_scanLS30(s);
    }
  return do_scsi_cmd (s->sfd, scan.cmd, scan.size, NULL, 0);
}


static int
prescan (Coolscan_t * s)
{
  int ret;

  DBG (10, "Starting prescan...\n");
  if(s->LS<2)
  {  coolscan_set_window_param (s, 1);
  }
  else
  { 
     do_scsi_cmd (s->sfd,commande1.cmd,commande1.size,s->buffer,0x0d);
     wait_scanner (s);
     wait_scanner (s);
     coolscan_set_window_param_LS30 (s,1,1);
     coolscan_set_window_param_LS30 (s,2,1);
     coolscan_set_window_param_LS30 (s,3,1);
  
  }
  ret = coolscan_start_scan(s);

  sleep(8);			/* prescan takes a minimum of 10 sec. */
  wait_scanner (s);
  DBG (10, "Prescan done\n");
  return ret;
}

static SANE_Status
do_prescan_now (Coolscan_t * scanner)
{

  DBG (10, "do_prescan_now \n");
  if (scanner->scanning == SANE_TRUE)
    return SANE_STATUS_DEVICE_BUSY;

  if (scanner->sfd < 0)
    {				/* first call */
      if (sanei_scsi_open (scanner->sane.name,
			   &(scanner->sfd),
			   sense_handler, 0) != SANE_STATUS_GOOD)
	{
	  DBG (1, "do_prescan_now: open of %s failed:\n",
	       scanner->sane.name);
	  return SANE_STATUS_INVAL;
	}
    }
  scanner->scanning = SANE_TRUE;


  if (coolscan_check_values (scanner) != 0)
    {				/* Verify values */
      DBG (1, "ERROR: invalid scan-values\n");
      scanner->scanning = SANE_FALSE;
      coolscan_give_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      return SANE_STATUS_INVAL;
    }

  if (coolscan_grab_scanner (scanner))
    {
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      DBG (5, "WARNING: unable to reserve scanner: device busy\n");
      scanner->scanning = SANE_FALSE;
      return SANE_STATUS_DEVICE_BUSY;
    }

  prescan (scanner);  
  if(scanner->LS<2)
    {	get_internal_info(scanner);
    }
  coolscan_get_window_param (scanner,1);
  scanner->scanning = SANE_FALSE;
  coolscan_give_scanner (scanner);
  return SANE_STATUS_GOOD;
}


static int
send_one_LUT (Coolscan_t * s, SANE_Word * LUT, int reg)
{
  int i;
  short lutval;
  short bytesperval;
  unsigned char *gamma, *gamma_p;
  unsigned short *gamma_s;

  DBG (10, "send LUT\n");

  if(s->LS<2)
  { set_S_datatype_code (send.cmd, R_user_reg_gamma);
    bytesperval=1;
  }
  else
  {
    send.cmd[0x02]=3;
    send.cmd[0x05]=1;
    bytesperval=2;
  }

  set_S_xfer_length (send.cmd, s->lutlength*bytesperval);
  set_S_datatype_qual_upper (send.cmd, reg);

  gamma = alloca (send.size + s->lutlength*2);
  memcpy (gamma, send.cmd, send.size);
  if(s->LS<2)  
  { gamma_p = &gamma[send.size];
    for (i = 0; i < s->lutlength; i++)
    {
      if (LUT[i] > 255)
	LUT[i] = 255;		/* broken gtk */
      *gamma_p++ = (unsigned char) LUT[i];
    }
  }
  else if(s->LS==2)  
  { gamma_s = (unsigned short*)( &gamma[send.size]);
    for (i = 0; i < s->lutlength; i++)
    {
       if(s->negative)
       {
         lutval=(unsigned short)(LUT[(s->lutlength-i)]);  
       }
       else    
       {
	 lutval=(unsigned short)(LUT[i]);      
       }     
       if (LUT[i] >= s->max_lut_val)
       LUT[i] = s->max_lut_val-1;	          	/* broken gtk */
       if(s->low_byte_first)                                /* if on little endian machine: */
       {
         lutval=((lutval&0x00ff)<<8)+((lutval&0xff00)>>8); /* inverse byteorder */       
       }
       *gamma_s++ = lutval;   
    }
  }
  else if(s->LS==3)  
  { gamma_s = (unsigned short*)( &gamma[send.size]);
    for (i = 0; i < s->lutlength; i++)
    {
       if(s->negative)
       {
         lutval=(unsigned short)(LUT[s->lutlength-i]);  
       }
       else    
       {
	 lutval=(unsigned short)(LUT[i]);      
       }     
       if (LUT[i] >= s->max_lut_val)
       LUT[i] = s->max_lut_val-1;	          	    /* broken gtk */
       if(s->low_byte_first)                                /* if on little endian machine: */
       {  lutval=((lutval&0x00ff)<<8)+((lutval&0xff00)>>8); /* inverse byteorder */ 
       }
       *gamma_s++ = lutval;   
    }
  }
  return do_scsi_cmd (s->sfd, gamma, send.size + s->lutlength*bytesperval, NULL, 0);
}


static int
send_LUT (Coolscan_t * s)
{
  wait_scanner (s);
  if (s->gamma_bind)
    {
      send_one_LUT (s, s->gamma, S_DQ_Reg1);
      if(s->LS>=2)
	{      send_one_LUT (s, s->gamma, S_DQ_Reg2);
	       send_one_LUT (s, s->gamma, S_DQ_Reg3);
               if(s->colormode&0x08)
	       { send_one_LUT (s, s->gamma, S_DQ_Reg9);
	       }   

	}
    }
  else
    {
      send_one_LUT (s, s->gamma_r, S_DQ_Reg1);
      send_one_LUT (s, s->gamma_g, S_DQ_Reg2);
      send_one_LUT (s, s->gamma_b, S_DQ_Reg3);
      if(s->colormode&0x08)
      { send_one_LUT (s, s->gamma_r, S_DQ_Reg9);
      }   
    }
  return 0;
}


static int
coolscan_read_data_block (Coolscan_t * s, unsigned int datatype, unsigned int length)
{
  int r;

  DBG (10, "read_data_block (type= %x length = %d)\n",datatype,length);
  /*wait_scanner(s); */

  set_R_datatype_code (sread.cmd, datatype);
  sread.cmd[4]=00;
  sread.cmd[5]=00;
  set_R_xfer_length (sread.cmd, length);

  r = do_scsi_cmd (s->sfd, sread.cmd, sread.size, s->buffer, length);
  return ((r != 0) ? -1 : (int) length);
}


static void
coolscan_do_inquiry (Coolscan_t * s)
{
  int size;

  DBG (10, "do_inquiry\n");
  memset (s->buffer, '\0', 256);	/* clear buffer */
  size = 36;			/* Hardcoded, and as specified by Nikon */
  /* then get inquiry with actual size */
  set_inquiry_return_size (inquiry.cmd, size);
  do_scsi_cmd (s->sfd, inquiry.cmd, inquiry.size, s->buffer, size);
}

static int
coolscan_identify_scanner (Coolscan_t * s)
{
  unsigned char vendor[9];
  unsigned char product[0x11];
  unsigned char version[5];
  unsigned char *pp;
  int i;

  vendor[8] = product[0x10] = version[4] = 0;
  DBG (10, "identify_scanner\n");
  coolscan_do_inquiry (s);	/* get inquiry */
  if (get_inquiry_periph_devtype (s->buffer) != IN_periph_devtype_scanner)
    {
      DBG (5, "identify_scanner: not a scanner\n");
      return 1;
    }				/* no, continue searching */

  coolscan_get_inquiry_values (s);

  get_inquiry_vendor ((char *)s->buffer, (char *)vendor);
  get_inquiry_product ((char *)s->buffer, (char *)product);
  get_inquiry_version ((char *)s->buffer, (char *)version);

  if (strncmp ("Nikon   ", (char *)vendor, 8))
    {
      DBG (5, "identify_scanner: \"%s\" isn't a Nikon product\n", vendor);
      return 1;
    }				/* Not a Nikon product */

  pp = &vendor[8];
  vendor[8] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  pp = &product[0x10];
  product[0x10] = ' ';
  while (*(pp - 1) == ' ')
    {
      *pp-- = '\0';
    }				/* leave one blank at the end! */

  pp = &version[4];
  version[4] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  DBG (10, "Found Nikon scanner %sversion %s on device %s\n",
       product, version, s->devicename);

  /* look for scanners that do not give all inquiry-informations */
  /* and if possible use driver-known inquiry-data  */
  if (get_inquiry_additional_length (s->buffer) >= 0x1f)
    {
      /* Now identify full supported scanners */
      for (i = 0; i < known_scanners; i++)
	{
	  if (!strncmp ((char *)product, scanner_str[i], strlen (scanner_str[i])))
	    {
	      s->LS = i;
	      return 0;
	    }
	}
      if (s->cont)
	return 0;
      else
	return 1;
    }
  else
    return 1;
}

static int
pixels_per_line (Coolscan_t * s)
{
  int pic_dot;
  if(s->LS<2)
  {  pic_dot = (s->brx - s->tlx + s->x_nres) / s->x_nres; 
  }
  else
  { pic_dot = (s->brx - s->tlx + 1) / s->x_nres; 
  }
  DBG (10, "pic_dot=%d\n", pic_dot);
  return pic_dot;
}

static int
lines_per_scan (Coolscan_t * s)
{
  int pic_line;
  if(s->LS<2)
  { pic_line = (s->bry - s->tly + s->y_nres) / s->y_nres; 
  }
  else
  { pic_line = (( s->bry - s->tly + 1.0 )  / s->y_nres); 
  }
  DBG (10, "pic_line=%d\n", pic_line);
  return pic_line;
}

static int
scan_bytes_per_line (Coolscan_t * s)
{ int bpl;
  switch(s->colormode)
    {  case RGB:
       case GREYSCALE:
              bpl=pixels_per_line (s) * 3;
              if(s->bits_per_color>8) bpl=bpl*2;
              return bpl;
              break;      
       case RGBI:
       case IRED:
              bpl=pixels_per_line (s) * 4;
              if(s->bits_per_color>8) bpl=bpl*2;
              return bpl;
              break; 
    }
    return 0;
}

static int
write_bytes_per_line (Coolscan_t * s)
{ int bpl;
  switch(s->colormode)
    {  case RGB:
              bpl=pixels_per_line (s) * 3;
              if(s->bits_per_color>8) bpl=bpl*2;
              return bpl;
              break;      
       case RGBI:
              bpl=pixels_per_line (s) * 4;
              if(s->bits_per_color>8) bpl=bpl*2;
              return bpl;
              break; 
       case IRED:
       case GREYSCALE:
              bpl= pixels_per_line (s) ;
              if(s->bits_per_color>8) bpl=bpl*2;
              return bpl;
              break; 
    }
    return 0;
}


static void
coolscan_trim_rowbufsize (Coolscan_t * s)
{
  unsigned int row_len;
  row_len = scan_bytes_per_line (s);
  s->row_bufsize = (s->row_bufsize < row_len) ? s->row_bufsize
    : s->row_bufsize - (s->row_bufsize % row_len);
  DBG (10, "trim_bufsize to %d\n", s->row_bufsize);
}

static int
coolscan_check_values (Coolscan_t * s)
{
  DBG (10, "check_values\n");
  /* -------------------------- asf --------------------------------- */
  if (s->asf != 0)
    {
      if (s->autofeeder == 0)
	{
	  DBG (1, "ERROR: ASF-MODE NOT SUPPORTED BY SCANNER, ABORTING\n");
	  return (1);
	}
    }

  return (0);
}

/* test_little_endian */

static SANE_Bool 
coolscan_test_little_endian(void)
{
  SANE_Int testvalue = 255;
  unsigned char *firstbyte = (unsigned char *) &testvalue;

  if (*firstbyte == 255)
  { return SANE_TRUE;
  }
  return SANE_FALSE;
}

static int
get_inquiery_part_LS30 (Coolscan_t * s, unsigned char part)
{ 
  int size;
  int ret;

  /* Get length of reponse */
  inquiry.cmd[1]=0x01;
  inquiry.cmd[2]=part;
  size=4;
  set_inquiry_return_size (inquiry.cmd, size);
  ret = do_scsi_cmd (s->sfd, inquiry.cmd, inquiry.size,
		     s->buffer, size);
  size=get_inquiry_length(s->buffer); 
  size+=4;
  /* then get inquiry with actual size */
  set_inquiry_return_size (inquiry.cmd, size);
  ret = do_scsi_cmd (s->sfd, inquiry.cmd, inquiry.size,
		     s->buffer, size);
  return size;
}

static int
coolscan_read_var_data_block (Coolscan_t * s,int datatype)
{
  int r;
  int size;

  DBG (10, "read_data_block (type= %x)\n",datatype);
  /*wait_scanner(s); */

  sread.cmd[2]=datatype;
  sread.cmd[4]=00;
  sread.cmd[5]=03;
  size=6;
  set_R_xfer_length (sread.cmd, size);
  r = do_scsi_cmd (s->sfd, sread.cmd, sread.size,
		     s->buffer, size);
  size=s->buffer[5]; 
  set_R_xfer_length (sread.cmd, size);
  r = do_scsi_cmd (s->sfd, sread.cmd, sread.size,
		     s->buffer, size);
  return ((r != 0) ? -1 : size);
}

static int
get_inquiery_LS30 (Coolscan_t * s)
{ 
  unsigned char part;
  unsigned char parts[5];
  int size;
  int i;

  /* Get vector of inquiery parts */
  size=get_inquiery_part_LS30(s, (unsigned char) 0);
  /* Get the parts of inquiery */  
  for(i=0;i<5;i++)
  { parts[i]=((unsigned char *)s->buffer)[4+11+i];
  }
  for(i=0;i<5;i++)
  { part=parts[i];
    size=get_inquiery_part_LS30 (s, part);
    switch(part)
    {  case 0x0c1:/* max size and resolution */                   
                    s->adbits = 8;
                    s->outputbits = 8;
		    s->maxres = getnbyte(s->buffer+0x12,2)-1;
		    s->xmaxpix = getnbyte(s->buffer+0x53,2)-1;
		    s->ymaxpix = getnbyte(s->buffer+0x3c,2)-1;
                  break;
       case 0x0d1:
                  break;
       case 0x0e1:
                  break;
       case 0x0f0:
                  break;
       case 0x0f8:
                  break;
    }  
  }

  /* get windows */
  coolscan_get_window_param_LS30 (s,0,0);
   s->xmax = get_WD_width(s->buffer);
   s->ymax = get_WD_length(s->buffer);
  coolscan_get_window_param_LS30 (s,1,0);
  coolscan_get_window_param_LS30 (s,2,0);
  coolscan_get_window_param_LS30 (s,3,0);
  coolscan_get_window_param_LS30 (s,4,0);
  coolscan_get_window_param_LS30 (s,9,0);

  s->analoggamma = 0;
  return 1;
}

static int
get_feeder_type_LS30 (Coolscan_t * s)
{ 
  int size;
  unsigned char *ptr;
  int ima;

  /* find out about Film-strip-feeder or Mount-Feeder */
  size=get_inquiery_part_LS30(s, (unsigned char) 1);
  if(strncmp((char *)s->buffer+5,"Strip",5)==0)
  { s->feeder=STRIP_FEEDER;
    s->autofeeder = 1;
  }
  if(strncmp((char *)s->buffer+5,"Mount",5)==0)
  { s->feeder=MOUNT_FEEDER;
  }
  /* find out about Film-strip-feeder positions*/
  if(s->feeder==STRIP_FEEDER)
  { size=coolscan_read_var_data_block (s,(int)0x88);
    if(size>=4)
    { s->numima=s->buffer[3];
      if(s->numima>6) s->numima=6; /* limit to 6 images for now */
      if(s->numima>(size-4)/16) s->numima=(size-4)/16;
      ptr=s->buffer+4;
      for(ima=0;ima<s->numima;ima++)
      {  s->ipos[ima].start=getnbyte(ptr,4);
         s->ipos[ima].offset=getnbyte(ptr+4,4);
         s->ipos[ima].end=getnbyte(ptr+8,4);
         s->ipos[ima].height=getnbyte(ptr+12,4);
	 ptr+=16;
      }
    }
    s->posima=0;
  }
  return 1;
}


static int
get_internal_info_LS20 (Coolscan_t * s)
{
  int ret;

  DBG (10, "get_internal_info\n");
  wait_scanner (s);
  memset (s->buffer, '\0', DI_length);	/* clear buffer */

  set_R_datatype_code (sread.cmd, R_device_internal_info);
  set_R_datatype_qual_upper (sread.cmd, R_DQ_none);
  set_R_xfer_length (sread.cmd, DI_length);
  /* then get inquiry with actual size */
  ret = do_scsi_cmd (s->sfd, sread.cmd, sread.size,
		     s->buffer, DI_length);

  s->adbits = get_DI_ADbits (s->buffer);
  s->outputbits = get_DI_Outputbits (s->buffer);
  s->maxres = get_DI_MaxResolution (s->buffer);
  s->xmax = get_DI_Xmax (s->buffer);
  s->ymax = get_DI_Ymax (s->buffer);
  s->xmaxpix = get_DI_Xmaxpixel (s->buffer);
  s->ymaxpix = get_DI_Ymaxpixel (s->buffer);
  s->ycurrent = get_DI_currentY (s->buffer);
  s->currentfocus = get_DI_currentFocus (s->buffer);
  s->currentscanpitch = get_DI_currentscanpitch (s->buffer);
  s->autofeeder = get_DI_autofeeder (s->buffer);
  s->analoggamma = get_DI_analoggamma (s->buffer);
  s->derr[0] = get_DI_deviceerror0 (s->buffer);
  s->derr[1] = get_DI_deviceerror1 (s->buffer);
  s->derr[2] = get_DI_deviceerror2 (s->buffer);
  s->derr[3] = get_DI_deviceerror3 (s->buffer);
  s->derr[4] = get_DI_deviceerror4 (s->buffer);
  s->derr[5] = get_DI_deviceerror5 (s->buffer);
  s->derr[6] = get_DI_deviceerror6 (s->buffer);
  s->derr[7] = get_DI_deviceerror7 (s->buffer);
  s->wbetr_r = get_DI_WBETR_R (s->buffer);
  s->webtr_g = get_DI_WBETR_G (s->buffer);
  s->webtr_b = get_DI_WBETR_B (s->buffer);
  s->pretv_r = get_DI_PRETV_R (s->buffer);
  s->pretv_g = get_DI_PRETV_G (s->buffer);
  s->pretv_r = get_DI_PRETV_R (s->buffer);
  s->cetv_r = get_DI_CETV_R (s->buffer);
  s->cetv_g = get_DI_CETV_G (s->buffer);
  s->cetv_b = get_DI_CETV_B (s->buffer);
  s->ietu_r = get_DI_IETU_R (s->buffer);
  s->ietu_g = get_DI_IETU_G (s->buffer);
  s->ietu_b = get_DI_IETU_B (s->buffer);
  s->limitcondition = get_DI_limitcondition (s->buffer);
  s->offsetdata_r = get_DI_offsetdata_R (s->buffer);
  s->offsetdata_g = get_DI_offsetdata_G (s->buffer);
  s->offsetdata_b = get_DI_offsetdata_B (s->buffer);
  get_DI_poweron_errors (s->buffer, s->power_on_errors);

  DBG (10,
       "\tadbits=%d\toutputbits=%d\tmaxres=%d\txmax=%d\tymax=%d\n"
       "\txmaxpix=%d\tymaxpix=%d\tycurrent=%d\tcurrentfocus=%d\n"
       "\tautofeeder=%s\tanaloggamma=%s\tcurrentscanpitch=%d\n",
       s->adbits, s->outputbits, s->maxres, s->xmax, s->ymax,
       s->xmaxpix, s->ymaxpix, s->ycurrent, s->currentfocus,
       s->autofeeder ? "Yes" : "No", s->analoggamma ? "Yes" : "No",
       s->currentscanpitch);
  DBG (10,
       "\tWhite balance exposure time var [RGB]=\t%d %d %d\n"
       "\tPrescan result exposure time var [RGB]=\t%d %d %d\n"
       "\tCurrent exposure time var.[RGB]=\t%d %d %d\n"
       "\tInternal exposure time unit[RGB]=\t%d %d %d\n",
       s->wbetr_r, s->webtr_g, s->webtr_b, s->pretv_r, s->pretv_g,
       s->pretv_r, s->cetv_r, s->cetv_g, s->cetv_b, s->ietu_r,
       s->ietu_g, s->ietu_b);
  DBG (10,
       "\toffsetdata_[rgb]=\t0x%x 0x%x 0x%x\n"
       "\tlimitcondition=0x%x\n"
       "\tdevice error code = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n"
       "\tpower-on errors = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
       s->offsetdata_r, s->offsetdata_g, s->offsetdata_b,
       s->limitcondition, 
       s->derr[0], s->derr[1], s->derr[2], s->derr[3], s->derr[4],
       s->derr[5], s->derr[6], s->derr[7],
       s->power_on_errors[0], s->power_on_errors[1],
       s->power_on_errors[2], s->power_on_errors[3],
       s->power_on_errors[4], s->power_on_errors[5],
       s->power_on_errors[6], s->power_on_errors[7]);

  return ret;
}

static int
get_internal_info (Coolscan_t * s)
{
  int ret;

  DBG (10, "get_internal_info\n");
  
  if(s->LS<2)                   /* distinquish between old and new scanners */
  { ret=get_internal_info_LS20 (s);
  }
  else
  { ret=get_inquiery_LS30 (s);
  }
  return ret;
}

static void
coolscan_get_inquiry_values (Coolscan_t * s)
{
  unsigned char *inquiry_block;

  DBG (10, "get_inquiry_values\n");

  inquiry_block = (unsigned char *) s->buffer;
  s->inquiry_len = 36;

  get_inquiry_vendor ((char *)inquiry_block, (char *)s->vendor);
  s->vendor[8] = '\0';
  get_inquiry_product ((char *)inquiry_block, (char *)s->product);
  s->product[16] = '\0';
  get_inquiry_version ((char *)inquiry_block, (char *)s->version);
  s->version[4] = '\0';

  if (s->inquiry_len < 36)
    {
      DBG (1, "WARNING: inquiry return block is unexpected short (%d instead of 36).\n", s->inquiry_len);
    }
  s->inquiry_wdb_len = 117;
  return;
}

static void
coolscan_initialize_values (Coolscan_t * s)
{
  int i;
  DBG (10, "initialize_values\n");
  /* Initialize us structure */
  if(s->LS<2)                   /* LS-20 or LS-10000 */
  {  select_MUD (s);		/* must be before mode_sense - not for LS-30*/
     coolscan_mode_sense (s);	/* Obtain MUD (Measurement Unit Divisor) */
     get_internal_info (s);	/* MUST be called first. */
     s->wdb_len = 117;
  }
  if(s->LS>=2)                  /* LS-30 */
  {
    get_inquiery_LS30(s);	/* Info about scanner*/ 
    select_MUD (s);		/* must be before mode_sense */
    get_feeder_type_LS30(s);
    s->wdb_len = 117;
  }

  s->cont = 0;			/* do not continue if scanner is unknown */
  s->verbose = 2;		/* 1=verbose,2=very verbose */


  s->x_nres = s->y_nres = 2;	/* 2 => 1350 dpi */
  s->x_p_nres = s->y_p_nres = 9;	/* 9 => 300 dpi */
  s->tlx = 0;
  s->tly = 0;
  s->brx = s->xmaxpix;		/* 2700 / 1200; */
  s->bry = s->ymaxpix;		/* 2700 / 1200; */


  s->set_auto = 0;		/* Always 0 on Nikon LS-{100|2}0 */
  s->preview = 0;		/* 1 for preview */
  s->colormode = RGB;		/* GREYSCALE or RGB */
  s->colormode_p = RGB;		/* GREYSCALE or RGB for preview*/
  s->asf = 0;			/* 1 if asf shall be used */
  s->gammaselection = WD_Linear;

  s->brightness = 128;
  s->brightness_R = 128;
  s->brightness_G = 128;
  s->brightness_B = 128;
  s->contrast = 128;
  s->contrast_R = 128;
  s->contrast_G = 128;
  s->contrast_B = 128;

  s->exposure_R = 50;
  s->exposure_G = 50;
  s->exposure_B = 50;

  s->pretv_r=40000;
  s->pretv_g=40000;  
  s->pretv_b=40000;

  s->shift_R = 128;
  s->shift_G = 128;
  s->shift_B = 128;

  s->ired_red=60;
  s->ired_green=1;
  s->ired_blue=1;

  s->prescan = 1;
  s->bits_per_color = 8;
  s->rgb_control = 0;
  s->gamma_bind = 1;
  switch(s->LS)
  {  case 0:s->lutlength=2048;
            s->max_lut_val=256;
            break;
     case 1:s->lutlength=512;
            s->max_lut_val=512;
            break;
     case 2:s->lutlength=1024;
            s->max_lut_val=1024;
            break;
     case 3:s->lutlength=4096;
            s->max_lut_val=4096;
            break;
  }
  for (i = 0; i < s->lutlength; i++)
  {
     s->gamma[i] =((short)((((double)i)/s->lutlength)*s->max_lut_val));
     s->gamma_r[i] = s->gamma[i];
     s->gamma_g[i] = s->gamma[i];
     s->gamma_b[i] = s->gamma[i];
  } 

  if (coolscan_test_little_endian() == SANE_TRUE)
  {
    s->low_byte_first = 1;					        /* in 2 byte mode send lowbyte first */
    DBG(10,"backend runs on little endian machine\n");
  }
  else
  {
    s->low_byte_first = 0;					       /* in 2 byte mode send highbyte first */
    DBG(10,"backend runs on big endian machine\n");
  }
}

static void
hexdump (int level, char *comment, unsigned char *p, int l)
{
  int i;
  char line[128];
  char *ptr;

  DBG (level, "%s\n", comment);
  ptr = line;
  for (i = 0; i < l; i++, p++)
    {
      if ((i % 16) == 0)
	{
	  if (ptr != line)
	    {
	      *ptr = '\0';
	      DBG (level, "%s\n", line);
	      ptr = line;
	    }
	  sprintf (ptr, "%3.3d:", i);
	  ptr += 4;
	}
      sprintf (ptr, " %2.2x", *p);
      ptr += 3;
    }
  *ptr = '\0';
  DBG (level, "%s\n", line);
}


static SANE_Status
sense_handler (int scsi_fd, unsigned char * result, void *arg)
{
  scsi_fd = scsi_fd;
  arg = arg;

  if (result[0] != 0x70)
    {
      return SANE_STATUS_IO_ERROR;	/* we only know about this one  */
    }
  return request_sense_parse(result);
 
}


/* ------------------------------------------------------------------------- */


/* ilu per mm */

#define length_quant SANE_UNFIX(SANE_FIX(MM_PER_INCH / 2700.0))
#define mmToIlu(mm) ((mm) / length_quant)
#define iluToMm(ilu) ((ilu) * length_quant)

#define P_200_TO_255(per) SANE_UNFIX((per + 100) * 255/200 )
#define P_100_TO_255(per) SANE_UNFIX(per * 255/100 )

static const char negativeStr[] = "Negative";
static const char positiveStr[] = "Positive";
static SANE_String_Const type_list[] =
{
  positiveStr,
  negativeStr,
  0
};

static const char colorStr[] = SANE_VALUE_SCAN_MODE_COLOR;
static const char grayStr[] = SANE_VALUE_SCAN_MODE_GRAY;
static const char rgbiStr[] = "RGBI";
static const char iredStr[] = "Infrared";

static SANE_String_Const scan_mode_list_LS20[] =
{
  colorStr,
  grayStr,
  NULL
};

static SANE_String_Const scan_mode_list_LS30[] =
{
  colorStr,
  grayStr,
#ifdef HAS_IRED	     
  rgbiStr,
#endif /* HAS_IRED */
  NULL
};

static SANE_Int bit_depth_list[9];

static const char neverStr[] = "never";
static const char previewStr[] = "before preview";
static const char scanStr[] = "before scan";
static const char preandscanStr[] = "before preview and scan";
static SANE_String_Const autofocus_mode_list[] =
{
  neverStr,
  previewStr,
  scanStr,
  preandscanStr,
  NULL
};

static SANE_String_Const source_list[4] =
{NULL, NULL, NULL, NULL};

static const SANE_Range gamma_range_8 = 
{
  0,				/* minimum */
  255,				/* maximum */
  1				/* quantization */
};


static const SANE_Range gamma_range_9 = 
{
  0,				/* minimum */
  511,				/* maximum */
  1				/* quantization */
};

static const SANE_Range gamma_range_10 = 
{
  0,				/* minimum */
  1023,				/* maximum */
  1				/* quantization */
};

static const SANE_Range gamma_range_12 = 
{
  0,				/* minimum */
  4096,				/* maximum */
  1				/* quantization */
};

static const SANE_Range brightness_range =
{
  -5,
  +5,
  1
};

static const SANE_Range contrast_range =
{
  -5,
  +5,
  0
};

static const SANE_Range exposure_range =
{
  24,
  400,
  2
};

static const SANE_Range shift_range =
{
  -15,
  +15,
  0
};

static const SANE_Device **devlist = 0;
static int num_devices;
static Coolscan_t *first_dev;


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
do_eof (Coolscan_t * scanner)
{
  DBG (10, "do_eof\n");

  if (scanner->pipe >= 0)
    {
      close (scanner->pipe);
      scanner->pipe = -1;
    }
  return SANE_STATUS_EOF;
}

static SANE_Status
do_cancel (Coolscan_t * scanner)
{
  DBG (10, "do_cancel\n");
  swap_res (scanner);
  scanner->scanning = SANE_FALSE;

  do_eof (scanner);		/* close pipe and reposition scanner */

  if (scanner->reader_pid != -1)
    {
      int exit_status;

      DBG (10, "do_cancel: kill reader_process\n");

      /* ensure child knows it's time to stop: */
      sanei_thread_kill (scanner->reader_pid);
      while (sanei_thread_waitpid(scanner->reader_pid, &exit_status) !=
                                                        scanner->reader_pid );
      scanner->reader_pid = -1;
    }

  if (scanner->sfd >= 0)
    {
      coolscan_give_scanner (scanner);
      DBG (10, "do_cancel: close filedescriptor\n");
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
    }

  return SANE_STATUS_CANCELLED;
}

static SANE_Status
attach_scanner (const char *devicename, Coolscan_t ** devp)
{
  Coolscan_t *dev;
  int sfd;

  DBG (10, "attach_scanner: %s\n", devicename);

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
	{
	  if (devp)
	    {
	      *devp = dev;
	    }
	  DBG (5, "attach_scanner: scanner already attached (is ok)!\n");
	  return SANE_STATUS_GOOD;
	}
    }

  DBG (10, "attach_scanner: opening %s\n", devicename);
  if (sanei_scsi_open (devicename, &sfd, sense_handler, 0) != 0)
    {
      DBG (1, "attach_scanner: open failed\n");
      return SANE_STATUS_INVAL;
    }

  if (NULL == (dev = malloc (sizeof (*dev))))
    return SANE_STATUS_NO_MEM;

	
  dev->row_bufsize = (sanei_scsi_max_request_size < (64 * 1024)) ?
  	sanei_scsi_max_request_size : 64 * 1024;
 
  if ((dev->buffer = malloc (dev->row_bufsize)) == NULL)
/*  if ((dev->buffer = malloc (sanei_scsi_max_request_size)) == NULL)*/
    return SANE_STATUS_NO_MEM;

  if ((dev->obuffer = malloc (dev->row_bufsize)) == NULL)
    return SANE_STATUS_NO_MEM;

  dev->devicename = strdup (devicename);
  dev->sfd = sfd;

  /* Nikon manual: Step 1 */
  if (coolscan_identify_scanner (dev) != 0)
    {
      DBG (1, "attach_scanner: scanner-identification failed\n");
      sanei_scsi_close (dev->sfd);
      free (dev->buffer);
      free (dev);
      return SANE_STATUS_INVAL;
    }

  /* Get MUD (via mode_sense), internal info (via get_internal_info), and
   * initialize values */
  coolscan_initialize_values (dev);

  /* Why? */
  sanei_scsi_close (dev->sfd);
  dev->sfd = -1;

  dev->sane.name = dev->devicename;
  dev->sane.vendor = dev->vendor;
  dev->sane.model = dev->product;
  dev->sane.type = "slide scanner";

  dev->x_range.min = SANE_FIX (0);
  dev->x_range.quant = SANE_FIX (length_quant);
  dev->x_range.max = SANE_FIX ((double) ((dev->xmaxpix) * length_quant));

  dev->y_range.min = SANE_FIX (0.0);
  dev->y_range.quant = SANE_FIX (length_quant);
  dev->y_range.max = SANE_FIX ((double) ((dev->ymaxpix) * length_quant));

  /* ...and this?? */
  dev->dpi_range.min = SANE_FIX (108);
  dev->dpi_range.quant = SANE_FIX (0);
  dev->dpi_range.max = SANE_FIX (dev->maxres);
  DBG (10, "attach: dev->dpi_range.max = %f\n",
       SANE_UNFIX (dev->dpi_range.max));

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    {
      *devp = dev;
    }

  DBG (10, "attach_scanner done\n");

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one (const char *devName)
{
  return attach_scanner(devName, 0);
}

static RETSIGTYPE
sigterm_handler (int signal)
{
  signal = signal;
  sanei_scsi_req_flush_all ();	/* flush SCSI queue */
  _exit (SANE_STATUS_GOOD);
}


typedef struct Color_correct_s
{ int sum;           /* number of pixels summed so far */
  double sumr;          /* sum of red pixel values*/
  double sumi;          /* sum of infrared pixel values*/
  double sumri;         /* sum of red*ired pixel values*/
  double sumii;         /* sum of ired*ired pixel values*/
  double sumrr;         /* sum of ired*ired pixel values*/
  int  mr;         /* factor between red and ired values (*256) */ 
  int  br;         /* offset of ired values */ 
} ColorCorrect;

/* --------------------------------------------------------------- 

  function:   RGBIfix

  taks:       Correct the infrared channel
      
  import:     unsigned char * rgbimat - RGBI - matrix from scanner
              int size - number of pixels to correct
	      int *lutr - lookup table for red correction
	      int *lutg - lookup table for red correction
	      int *lutb - lookup table for red correction
	      int *lutr - lookup table for red correction

  export:     unsigned char * orgbimat - RGBI - corrected matrix
                  
  written by: Andreas RICK   19.6.1999
                           
  ----------------------------------------------------------------*/

static int Calc_fix_LUT(Coolscan_t * s)
{ int uselutr,uselutg,uselutb,useluti;    
/*  static int irmulr= -34*25; */
   int irmulr= -64*25;
   int irmulg= -1*25;
   int irmulb= -0*25;
   int irmuli= 256*25;
   int div;
   int i;

    irmulr=s->ired_red*(25);
    irmulg=s->ired_green*(25);
    irmulb=s->ired_blue*(25);
    irmuli=25*256;
  
  if(s->LS==2) /* TODO: right conversion factors for 10 and 12 bit */
    { div=4;
    }
  else  if(s->LS==3) 
    { div=16;
    }
  else
    { return 0;
    }

  memset(s->lutr, 0,256*4);
  memset(s->lutg, 0,256*4);
  memset(s->lutb, 0,256*4);
  memset(s->luti, 0,256*4);

  for(i=0;i<s->lutlength;i++)
  {  if(s->gamma_bind)
     { uselutr=uselutg=uselutb=useluti=s->gamma[i]/div;
     }
     else
     { uselutr=s->gamma_r[i]/div;
       uselutg=s->gamma_g[i]/div;
       uselutb=s->gamma_b[i]/div;
       useluti=s->gamma_r[i]/div;
     }
     s->lutr[uselutr]=(int)(irmulr*pow((double)i,(double)0.333333)); 
     s->lutg[uselutg]=(int)(irmulg*pow((double)i,(double)0.333333)); 
     s->lutb[uselutb]=(int)(irmulb*pow((double)i,(double)0.333333)); 
     s->luti[useluti]=(int)(irmuli*pow((double)i,(double)0.333333)); 
     if(uselutr<255)
     { if(s->lutr[uselutr+1]==0) s->lutr[uselutr+1]=s->lutr[uselutr];
     }
     if(uselutg<255)
     { if(s->lutg[uselutg+1]==0) s->lutg[uselutg+1]=s->lutg[uselutg];
     }
     if(uselutb<255)
     { if(s->lutb[uselutb+1]==0) s->lutb[uselutb+1]=s->lutb[uselutb];
     }
     if(useluti<255)
     { if(s->luti[useluti+1]==0) s->luti[useluti+1]=s->luti[useluti];
     }
  }
  /* DEBUG
  for(i=0;i<255;i++)
  { fprintf(stderr,"%d %d %d %d\n"
	    ,s->lutr[i],s->lutg[i],s->lutb[i],s->luti[i]);
  }
  */
  return 1;
}



/* --------------------------------------------------------------- 

  function:   RGBIfix

  taks:       Correct the infrared channel
      
  import:     unsigned char * rgbimat - RGBI - matrix from scanner
              int size - number of pixels to correct
	      int *lutr - lookup table for red correction
	      int *lutg - lookup table for red correction
	      int *lutb - lookup table for red correction
	      int *lutr - lookup table for red correction

  export:     unsigned char * orgbimat - RGBI - corrected matrix
                  
  written by: Andreas RICK   19.6.1999
                           
  ----------------------------------------------------------------*/

static int RGBIfix(Coolscan_t * scanner,
	   unsigned char* rgbimat,
	   unsigned char* orgbimat, 	   
	    int size,
	    int *lutr,
	    int *lutg,
	    int *lutb,
	    int *luti)
	    
{
  unsigned char *pr,*pg,*pb,*pi;
  unsigned char *opr,*opg,*opb,*opi;

   int r,g,b,i;
   int ii;
   int x;
   for(x=0;x<size;x++)
   {
        pr=rgbimat+x*4;
	pg=pr+1; 
	pb=pg+1;
	pi=pb+1;	
        opr=orgbimat+x*4;
	opg=opr+1; 
	opb=opg+1;
	opi=opb+1;
	r=lutr[(*pr)];
	g=lutg[(*pg)];
	b=lutb[(*pb)];	
	i=luti[(*pi)];	
	ii= i-r-g-b;
	(*opr)=(*pr);
	(*opg)=(*pg);
	(*opb)=(*pb);	
	if(ii<0)ii=0;
	if(ii>255*256)ii=255*256;
	if(scanner->negative)
	{
	    (*opi)=(unsigned char)(255-(ii>>8));	
	}
	else
	{
	  (*opi)=(unsigned char)(ii>>8);	
	}
   }
   return 1;
}

/* --------------------------------------------------------------- 

  function:   RGBIfix16

  taks:       Correct the infrared channel for 16 bit images
              (doesn't do anything for now)
      
  import:     unsigned char * rgbimat - RGBI - matrix from scanner
              int size - number of pixels to correct
	      int *lutr - lookup table for red correction
	      int *lutg - lookup table for red correction
	      int *lutb - lookup table for red correction
	      int *lutr - lookup table for red correction

  export:     unsigned char * orgbimat - RGBI - corrected matrix
                  
  written by: Andreas RICK   19.6.1999
                           
  ----------------------------------------------------------------*/

static int RGBIfix16(Coolscan_t * scanner,
	      unsigned short* rgbimat,
	      unsigned short* orgbimat, 	   
	      int size,
	      int *lutr,
	      int *lutg,
	      int *lutb,
	      int *luti)
	    
{  
  unsigned short *pr,*pg,*pb,*pi;
  unsigned short *opr,*opg,*opb,*opi;
  int x;

  scanner = scanner; lutr = lutr; lutg = lutg; lutb = lutb; luti = luti;

   for(x=0;x<size;x++)
   {
        pr=rgbimat+x*4;
	pg=pr+1; 
	pb=pg+1;
	pi=pb+1;	
        opr=orgbimat+x*4;
	opg=opr+1; 
	opb=opg+1;
	opi=opb+1;
	(*opr)=(((*pr)&0x00ff)<<8)+(((*pr)&0xff00)>>8); 
      	(*opg)=(((*pg)&0x00ff)<<8)+(((*pg)&0xff00)>>8); 
	(*opb)=(((*pb)&0x00ff)<<8)+(((*pb)&0xff00)>>8); 	
	(*opi)=(((*pi)&0x00ff)<<8)+(((*pi)&0xff00)>>8); 
   }
   return 1;
}


/* --------------------------------------------------------------- 

  function:   rgb2g

  taks:       Convert RGB data to grey
      
  import:     unsigned char * rgbimat - RGB - matrix from scanner
              int size - size of input data (num pixel)

  export:     unsigned char * gomat - Grey matrix
                  
  written by: Andreas RICK   13.7.1999
                           
  ----------------------------------------------------------------*/
#define RtoG ((int)(0.27*256))
#define GtoG ((int)(0.54*256))
#define BtoG ((int)(0.19*256))

static int rgb2g(unsigned char* rgbimat,unsigned char* gomat, 	   
	  int size)
	    
{  unsigned char *pr,*pg,*pb;
   unsigned char *opg;

   int g;
   int x;
   for(x=0;x<size;x++)
   {
        pr=rgbimat+x*3;
	pg=pr+1; 
	pb=pg+1;
        opg=gomat+x;
	g= RtoG*(*pr) + GtoG*(*pg) + BtoG*(*pb);
	(*opg)=(unsigned char)(g>>8);	
   }
   return 1;
}


/* --------------------------------------------------------------- 

  function:   RGBIfix1

  taks:       Correct the infrared channel.
              The input image data is the output of scaning
	      with LUT. To calculate the original values
	      the lutr and luti is applied.
	      The infrared values is corrected by:

	      Ir=mr*lutr(r)+luti(i)
      
  import:     unsigned char * rgbimat - RGBI - matrix from scanner
              int size - number of pixels to correct
 	      ColorCorrect *cc,
	      int *lutr - lookup table for red correction
	      int *luti - lookup table for ired correction

  export:     unsigned char * orgbimat - RGBI - corrected matrix
                  
  written by: Andreas RICK   3.7.1999
                           
  ----------------------------------------------------------------*/
#if 0
static int RGBIfix1(unsigned char* rgbimat,unsigned char* orgbimat, 	   
	    int size,
	    int *lutr,
	    int *lutg,
	    int *lutb,
	    int *luti)
	    
{  unsigned char *pr,*pg,*pb,*pi;
   unsigned char *opr,*opg,*opb,*opi;
   ColorCorrect cc;
   int r,i;
   static int thresi=100;
   int ii;
   int x;

   lutg = lutg; lutb = lutb;

   /* calculate regression between r and ir */
   cc.sum=0;
   cc.sumr=cc.sumii=cc.sumrr=cc.sumi=cc.sumri=0.0;
   for(x=0;x<size;x++)
   {    pr=rgbimat+x*4;
	pi=pr+3;	
	r=lutr[(*pr)];
	i=luti[(*pi)];
	/*	r=(*pr);
		i=(*pi); */
	if((*pi)>thresi)
        { cc.sum++;
          cc.sumr+=r;
          cc.sumii+=(i*i);
          cc.sumrr+=(r*r);
          cc.sumi+=i;
          cc.sumri+=(i*r);
	}
   }
   if((cc.sumii!=0)&&(cc.sum!=0))
   { double dn,dz,dm;
     dz=(cc.sumri-cc.sumr*cc.sumi/cc.sum);
     dn=(cc.sumrr-cc.sumr*cc.sumr/cc.sum);
     DBG (2, "Reg:dz:%e dn:%e\n",dz,dn);
     if(dn!=0)
     {  dm=(dz/dn);
        cc.mr=(int)(dm*1024);
     }
     else
     { cc.mr=0;
       dm=0;
     }
     cc.br=(int)((cc.sumi-dm*cc.sumr)/cc.sum);
   }
   else
   { cc.mr=0;
   }
   DBG (2, "Regression: size:%d I=%d/1024*R b:%d s:%d sr:%e si:%e sii:%e sri:%e  srr:%e\n",
	size,cc.mr,cc.br,cc.sum,cc.sumr,cc.sumi,cc.sumii,cc.sumri,cc.sumrr);
   for(x=0;x<size;x++)
   {

        pr=rgbimat+x*4;
	pg=pr+1; 
	pb=pg+1;
	pi=pb+1;	
        opr=orgbimat+x*4;
	opg=opr+1; 
	opb=opg+1;
	opi=opb+1;
	r=lutr[(*pr)];
	i=luti[(*pi)];
	/*	r=(*pr);
		i=(*pi); */
	ii= ((i-((r*cc.mr)>>10)-cc.br)>>2) +128;
	(*opr)=(*pr);
	(*opg)=(*pg);
	(*opb)=(*pb);	
	if(ii<0) ii=0;
	if(ii>255) ii=255;
	(*opi)=(unsigned char)(ii);	
   }
   return 1;
}

#endif
/* This function is executed as a child process. */
static int
reader_process (void *data )
{
  int status;
  unsigned int i;
  unsigned char h;
  unsigned int data_left;
  unsigned int data_to_read;
  unsigned int data_to_write;
  FILE *fp;
  sigset_t sigterm_set, ignore_set;
  struct SIGACTION act;
  unsigned int bpl, linesPerBuf, lineOffset;
  unsigned char r_data, g_data, b_data;
  unsigned int j, line;
  Coolscan_t * scanner = (Coolscan_t*)data;

  if (sanei_thread_is_forked ())
    {
      DBG (10, "reader_process started (forked)\n");
      close (scanner->pipe);
      scanner->pipe = -1;

      sigfillset ( &ignore_set );
      sigdelset  ( &ignore_set, SIGTERM );
#if defined (__APPLE__) && defined (__MACH__)
      sigdelset  ( &ignore_set, SIGUSR2 );
#endif
      sigprocmask( SIG_SETMASK, &ignore_set, 0 );

      memset (&act, 0, sizeof (act));
      sigaction (SIGTERM, &act, 0);
    }
  else
    {
      DBG (10, "reader_process started (as thread)\n");
    }

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  fp = fdopen ( scanner->reader_fds, "w");
  if (!fp)
    {
      DBG (1, "reader_process: couldn't open pipe!\n");
      return 1;
    }

  DBG (10, "reader_process: starting to READ data\n");

  data_left = scan_bytes_per_line (scanner) *
    lines_per_scan (scanner);
  
  /*scanner->row_bufsize = sanei_scsi_max_request_size;*/
  coolscan_trim_rowbufsize (scanner);	/* trim bufsize */

  DBG (10, "reader_process: reading %u bytes in blocks of %u bytes\n",
       data_left, scanner->row_bufsize);

  memset (&act, 0, sizeof (act));
  act.sa_handler = sigterm_handler;
  sigaction (SIGTERM, &act, 0);
  /* wait_scanner(scanner); */
  do
    {
      data_to_read = (data_left < scanner->row_bufsize) ?
	data_left : scanner->row_bufsize;

      data_to_write=data_to_read;

      status = coolscan_read_data_block (scanner
					 ,R_datatype_imagedata,data_to_read);
      if (status == 0)
	{
	  continue;
	}
      if (status == -1)
	{
	  DBG (1, "reader_process: unable to get image data from scanner!\n");
	  fclose (fp);
	  return (-1);
	}
    
      if (scanner->LS == 1) {	/* mirror image for LS-1000 */
	  bpl = scan_bytes_per_line(scanner);
	  linesPerBuf = data_to_read / bpl;
	  
	  for (line = 0, lineOffset = 0; line < linesPerBuf; 
	       line++, lineOffset += bpl ) {
	      
	      if (scanner->colormode == RGB) {
		  for (j = 0; j < bpl/2 ; j += 3) {
		      r_data=scanner->buffer[lineOffset + j];
		      g_data=scanner->buffer[lineOffset + j + 1];
		      b_data=scanner->buffer[lineOffset + j + 2];
		      
		      scanner->buffer[lineOffset + j] = 
			  scanner->buffer[lineOffset + bpl -1 - j - 2 ];
		      scanner->buffer[lineOffset + j + 1] = 
			  scanner->buffer[lineOffset + bpl -1 - j - 1 ];
		      scanner->buffer[lineOffset + j + 2] = 
			  scanner->buffer[lineOffset + bpl -1 - j ];
		  
		      scanner->buffer[lineOffset + bpl -1 - j - 2 ] = r_data;
		      scanner->buffer[lineOffset + bpl -1 - j - 1] = g_data;
		      scanner->buffer[lineOffset + bpl -1 - j] = b_data;
		  }
	      }
	      else {
		  for (j = 0; j < bpl/2; j++) {
		      r_data=scanner->buffer[lineOffset + j];
		      scanner->buffer[lineOffset + j] = 
			  scanner->buffer[lineOffset + bpl - 1 - j];
		      scanner->buffer[lineOffset + bpl - 1 - j] = r_data;
		  }
	      }
	  }	  
      }    
      if(scanner->colormode==RGBI) 
      {  /* Correct Infrared Channel */
	if(scanner->bits_per_color>8)
	{
	  RGBIfix16(scanner, (unsigned short * ) scanner->buffer, 
		    (unsigned short * )scanner->obuffer,
		 data_to_read/8,scanner->lutr,
		 scanner->lutg,scanner->lutb,scanner->luti);
	}
	else
	{
	  RGBIfix(scanner,scanner->buffer,scanner->obuffer,
		 data_to_read/4,scanner->lutr,
		 scanner->lutg,scanner->lutb,scanner->luti);
	}
      }
      else if((scanner->colormode==GREYSCALE)&&(scanner->LS>=2))
      {  /* Convert to Grey */
	 data_to_write/=3;
	 rgb2g(scanner->buffer,scanner->obuffer,data_to_write);
      }
      else
      { /* or just copy */
	memcpy (scanner->obuffer, scanner->buffer,data_to_read);
      }     
      if((!scanner->low_byte_first)&&(scanner->bits_per_color>8))
	{  for(i=0;i<data_to_write;i++) /* inverse byteorder */        
           { h=scanner->obuffer[i];
             scanner->obuffer[i]=scanner->obuffer[i+1];
	     i++;
  	     scanner->obuffer[i]=h;
	   }
	}
      fwrite (scanner->obuffer, 1, data_to_write, fp);
      fflush (fp);      
      data_left -= data_to_read;
      DBG (10, "reader_process: buffer of %d bytes read; %d bytes to go\n",
	   data_to_read, data_left);
    }
  while (data_left);

  fclose (fp);

  DBG (10, "reader_process: finished reading data\n");

  return 0;
}

static SANE_Status
init_options (Coolscan_t * scanner)
{
  int i;
  int bit_depths;

  DBG (10, "init_options\n");

  memset (scanner->opt, 0, sizeof (scanner->opt));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      scanner->opt[i].size = sizeof (SANE_Word);
      scanner->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  scanner->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  scanner->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

  /* "Mode" group: */
  scanner->opt[OPT_MODE_GROUP].title = "Scan Mode";
  scanner->opt[OPT_MODE_GROUP].desc = "";
  scanner->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_MODE_GROUP].cap = 0;
  scanner->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  scanner->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  scanner->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  scanner->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  scanner->opt[OPT_MODE].type = SANE_TYPE_STRING;
  if(scanner->LS<2)
  { scanner->opt[OPT_MODE].size = max_string_size (scan_mode_list_LS20);
    scanner->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    scanner->opt[OPT_MODE].constraint.string_list = scan_mode_list_LS20;
  }
  else
  { scanner->opt[OPT_MODE].size = max_string_size (scan_mode_list_LS30);
    scanner->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    scanner->opt[OPT_MODE].constraint.string_list = scan_mode_list_LS30;
  }

  /* source */
  source_list[0] = "Slide";
  source_list[1] = "Automatic Slide Feeder";
  source_list[2] = NULL;
  if (!scanner->autofeeder)
    {
      scanner->opt[OPT_SOURCE].cap = SANE_CAP_INACTIVE;
    }

  scanner->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_SOURCE].size = max_string_size (source_list);
  scanner->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_SOURCE].constraint.string_list = source_list;

  /* negative */
  scanner->opt[OPT_TYPE].name = "type";
  scanner->opt[OPT_TYPE].title = "Film type";
  scanner->opt[OPT_TYPE].desc =
    "Select the film type (positive (slide) or negative)";
  scanner->opt[OPT_TYPE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_TYPE].size = max_string_size (type_list);
  scanner->opt[OPT_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_TYPE].constraint.string_list = type_list;

  scanner->opt[OPT_PRESCAN].name = "prescan";
  scanner->opt[OPT_PRESCAN].title = "Prescan";
  scanner->opt[OPT_PRESCAN].desc =
    "Perform a prescan during preview";
  scanner->opt[OPT_PRESCAN].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_PRESCAN].unit = SANE_UNIT_NONE;

  scanner->opt[OPT_PRESCAN_NOW].name = "prescan now";
  scanner->opt[OPT_PRESCAN_NOW].title = "Prescan now";
  scanner->opt[OPT_PRESCAN_NOW].desc =
    "Perform a prescan now";
  scanner->opt[OPT_PRESCAN_NOW].type = SANE_TYPE_BUTTON;
  scanner->opt[OPT_PRESCAN_NOW].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_PRESCAN_NOW].size = 0;
  scanner->opt[OPT_PRESCAN_NOW].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  scanner->opt[OPT_PRESCAN_NOW].constraint_type = SANE_CONSTRAINT_NONE;
  scanner->opt[OPT_PRESCAN_NOW].constraint.string_list = 0;

 /* bit depth */
  
  bit_depths=0;
  bit_depth_list[++bit_depths] = 8;
  if (scanner->LS==2)
  {
    bit_depth_list[++bit_depths] = 10;
  }
  if (scanner->LS==3)
  {
    bit_depth_list[++bit_depths] = 12;
  }

  bit_depth_list[0] = bit_depths;

  scanner->opt[OPT_BIT_DEPTH].name  = SANE_NAME_BIT_DEPTH;
  scanner->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  scanner->opt[OPT_BIT_DEPTH].desc  = SANE_DESC_BIT_DEPTH;
  scanner->opt[OPT_BIT_DEPTH].type  = SANE_TYPE_INT;
  scanner->opt[OPT_BIT_DEPTH].unit  = SANE_UNIT_BIT;
  scanner->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_BIT_DEPTH].constraint.word_list = bit_depth_list;

  /* resolution */
  scanner->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  scanner->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  scanner->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  scanner->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  scanner->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_RESOLUTION].constraint.word_list = resolution_list;

  scanner->opt[OPT_PREVIEW_RESOLUTION].name = "preview-resolution";
  scanner->opt[OPT_PREVIEW_RESOLUTION].title = "Preview resolution";
  scanner->opt[OPT_PREVIEW_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  scanner->opt[OPT_PREVIEW_RESOLUTION].type = SANE_TYPE_INT;
  scanner->opt[OPT_PREVIEW_RESOLUTION].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_PREVIEW_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_PREVIEW_RESOLUTION].constraint.word_list = resolution_list;

  /* "Geometry" group: */
  scanner->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  scanner->opt[OPT_GEOMETRY_GROUP].desc = "";
  scanner->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  scanner->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  scanner->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  scanner->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  scanner->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  scanner->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_X].constraint.range = &(scanner->x_range);

  /* top-left y */
  scanner->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_Y].constraint.range = &(scanner->y_range);

  /* bottom-right x */
  scanner->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  scanner->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  scanner->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  scanner->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_X].constraint.range = &(scanner->x_range);

  /* bottom-right y */
  scanner->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_Y].constraint.range = &(scanner->y_range);


  /* ------------------------------ */

  /* "Enhancement" group: */
  scanner->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  scanner->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  scanner->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  scanner->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;


  scanner->opt[OPT_GAMMA_BIND].name = "gamma-bind";
  scanner->opt[OPT_GAMMA_BIND].title = "Gamma bind";
  scanner->opt[OPT_GAMMA_BIND].desc =
    "Use same gamma correction for all colours";
  scanner->opt[OPT_GAMMA_BIND].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_GAMMA_BIND].unit = SANE_UNIT_NONE;

  
  scanner->opt[OPT_ANALOG_GAMMA].name = "analog_gamma";
  scanner->opt[OPT_ANALOG_GAMMA].title = "Analog Gamma";
  scanner->opt[OPT_ANALOG_GAMMA].desc = "Analog Gamma";
  scanner->opt[OPT_ANALOG_GAMMA].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_ANALOG_GAMMA].unit = SANE_UNIT_NONE;
  if (!scanner->analoggamma)
    {
      scanner->opt[OPT_ANALOG_GAMMA].cap = SANE_CAP_INACTIVE;
    }

  scanner->opt[OPT_AVERAGING].name = "averaging";
  scanner->opt[OPT_AVERAGING].title = "Averaging";
  scanner->opt[OPT_AVERAGING].desc = "Averaging";
  scanner->opt[OPT_AVERAGING].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_AVERAGING].unit = SANE_UNIT_NONE;


  scanner->opt[OPT_RGB_CONTROL].name = "rgb-control";
  scanner->opt[OPT_RGB_CONTROL].title = "RGB control";
  scanner->opt[OPT_RGB_CONTROL].desc =
    "toggles brightness/contrast control over individual colours";
  scanner->opt[OPT_RGB_CONTROL].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_RGB_CONTROL].unit = SANE_UNIT_NONE;
  if(scanner->LS>=2)
  {  scanner->opt[OPT_RGB_CONTROL].cap |= SANE_CAP_INACTIVE;
  }



  /* brightness */
  scanner->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  scanner->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BRIGHTNESS].constraint.range = &brightness_range;
  if(scanner->LS>=2)
  {  scanner->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
  }


  scanner->opt[OPT_R_BRIGHTNESS].name = "red-brightness";
  scanner->opt[OPT_R_BRIGHTNESS].title = "Red brightness";
  scanner->opt[OPT_R_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  scanner->opt[OPT_R_BRIGHTNESS].type = SANE_TYPE_INT;
  scanner->opt[OPT_R_BRIGHTNESS].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_R_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_R_BRIGHTNESS].constraint.range = &brightness_range;
  scanner->opt[OPT_R_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;

  scanner->opt[OPT_G_BRIGHTNESS].name = "green-brightness";
  scanner->opt[OPT_G_BRIGHTNESS].title = "Green brightness";
  scanner->opt[OPT_G_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  scanner->opt[OPT_G_BRIGHTNESS].type = SANE_TYPE_INT;
  scanner->opt[OPT_G_BRIGHTNESS].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_G_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_G_BRIGHTNESS].constraint.range = &brightness_range;
  scanner->opt[OPT_G_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;

  scanner->opt[OPT_B_BRIGHTNESS].name = "blue-brightness";
  scanner->opt[OPT_B_BRIGHTNESS].title = "Blue brightness";
  scanner->opt[OPT_B_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  scanner->opt[OPT_B_BRIGHTNESS].type = SANE_TYPE_INT;
  scanner->opt[OPT_B_BRIGHTNESS].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_B_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_B_BRIGHTNESS].constraint.range = &brightness_range;
  scanner->opt[OPT_B_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;

  /* contrast */
  scanner->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  scanner->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  scanner->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  scanner->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  scanner->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CONTRAST].constraint.range = &contrast_range;
  if(scanner->LS>=2)
  {  scanner->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
  }


  scanner->opt[OPT_R_CONTRAST].name = "red-contrast";
  scanner->opt[OPT_R_CONTRAST].title = "Red contrast";
  scanner->opt[OPT_R_CONTRAST].desc = SANE_DESC_CONTRAST;
  scanner->opt[OPT_R_CONTRAST].type = SANE_TYPE_INT;
  scanner->opt[OPT_R_CONTRAST].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_R_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_R_CONTRAST].constraint.range = &contrast_range;
  scanner->opt[OPT_R_CONTRAST].cap |= SANE_CAP_INACTIVE;

  scanner->opt[OPT_G_CONTRAST].name = "green-contrast";
  scanner->opt[OPT_G_CONTRAST].title = "Green contrast";
  scanner->opt[OPT_G_CONTRAST].desc = SANE_DESC_CONTRAST;
  scanner->opt[OPT_G_CONTRAST].type = SANE_TYPE_INT;
  scanner->opt[OPT_G_CONTRAST].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_G_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_G_CONTRAST].constraint.range = &contrast_range;
  scanner->opt[OPT_G_CONTRAST].cap |= SANE_CAP_INACTIVE;

  scanner->opt[OPT_B_CONTRAST].name = "blue-contrast";
  scanner->opt[OPT_B_CONTRAST].title = "Blue contrast";
  scanner->opt[OPT_B_CONTRAST].desc = SANE_DESC_CONTRAST;
  scanner->opt[OPT_B_CONTRAST].type = SANE_TYPE_INT;
  scanner->opt[OPT_B_CONTRAST].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_B_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_B_CONTRAST].constraint.range = &contrast_range;
  scanner->opt[OPT_B_CONTRAST].cap |= SANE_CAP_INACTIVE;

  scanner->opt[OPT_EXPOSURE].name = "exposure";
  scanner->opt[OPT_EXPOSURE].title = "Exposure";
  scanner->opt[OPT_EXPOSURE].desc = "";
  scanner->opt[OPT_EXPOSURE].type = SANE_TYPE_INT;
  scanner->opt[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
  scanner->opt[OPT_EXPOSURE].unit = SANE_UNIT_PERCENT;
  scanner->opt[OPT_EXPOSURE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_EXPOSURE].constraint.range = &exposure_range;

  scanner->opt[OPT_R_EXPOSURE].name = "red-exposure";
  scanner->opt[OPT_R_EXPOSURE].title = "Red exposure";
  scanner->opt[OPT_R_EXPOSURE].desc = "";
  scanner->opt[OPT_R_EXPOSURE].type = SANE_TYPE_INT;
  scanner->opt[OPT_R_EXPOSURE].cap |= SANE_CAP_INACTIVE;
  scanner->opt[OPT_R_EXPOSURE].unit = SANE_UNIT_PERCENT;
  scanner->opt[OPT_R_EXPOSURE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_R_EXPOSURE].constraint.range = &exposure_range;

  scanner->opt[OPT_G_EXPOSURE].name = "green-exposure";
  scanner->opt[OPT_G_EXPOSURE].title = "Green exposure";
  scanner->opt[OPT_G_EXPOSURE].desc = "";
  scanner->opt[OPT_G_EXPOSURE].type = SANE_TYPE_INT;
  scanner->opt[OPT_G_EXPOSURE].unit = SANE_UNIT_PERCENT;
  scanner->opt[OPT_G_EXPOSURE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_G_EXPOSURE].constraint.range = &exposure_range;
  scanner->opt[OPT_G_EXPOSURE].cap |= SANE_CAP_INACTIVE;

  scanner->opt[OPT_B_EXPOSURE].name = "blue-exposure";
  scanner->opt[OPT_B_EXPOSURE].title = "Blue exposre";
  scanner->opt[OPT_B_EXPOSURE].desc = "";
  scanner->opt[OPT_B_EXPOSURE].type = SANE_TYPE_INT;
  scanner->opt[OPT_B_EXPOSURE].unit = SANE_UNIT_PERCENT;
  scanner->opt[OPT_B_EXPOSURE].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_B_EXPOSURE].constraint.range = &exposure_range;
  scanner->opt[OPT_B_EXPOSURE].cap |= SANE_CAP_INACTIVE;
  if(scanner->LS>=2)
  {  scanner->opt[OPT_R_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
     scanner->opt[OPT_G_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
     scanner->opt[OPT_B_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
     scanner->opt[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
  }

  scanner->opt[OPT_R_SHIFT].name = "red-shift";
  scanner->opt[OPT_R_SHIFT].title = "Red shift";
  scanner->opt[OPT_R_SHIFT].desc = "";
  scanner->opt[OPT_R_SHIFT].type = SANE_TYPE_INT;
  scanner->opt[OPT_R_SHIFT].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_R_SHIFT].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_R_SHIFT].constraint.range = &shift_range;
  if(scanner->LS>=2)
  {  scanner->opt[OPT_R_SHIFT].cap |= SANE_CAP_INACTIVE;
  }


  scanner->opt[OPT_G_SHIFT].name = "green-shift";
  scanner->opt[OPT_G_SHIFT].title = "Green shift";
  scanner->opt[OPT_G_SHIFT].desc = "";
  scanner->opt[OPT_G_SHIFT].type = SANE_TYPE_INT;
  scanner->opt[OPT_G_SHIFT].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_G_SHIFT].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_G_SHIFT].constraint.range = &shift_range;
  if(scanner->LS>=2)
  {  scanner->opt[OPT_G_SHIFT].cap |= SANE_CAP_INACTIVE;
  }


  scanner->opt[OPT_B_SHIFT].name = "blue-shift";
  scanner->opt[OPT_B_SHIFT].title = "Blue shift";
  scanner->opt[OPT_B_SHIFT].desc = "";
  scanner->opt[OPT_B_SHIFT].type = SANE_TYPE_INT;
  scanner->opt[OPT_B_SHIFT].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_B_SHIFT].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_B_SHIFT].constraint.range = &shift_range;
  if(scanner->LS>=2)
  {  scanner->opt[OPT_B_SHIFT].cap |= SANE_CAP_INACTIVE;
  }

  /* R+G+B gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  if (scanner->LS == 1)
    {
      scanner->opt[OPT_GAMMA_VECTOR].cap = SANE_CAP_INACTIVE;
    }
  scanner->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  switch(scanner->LS)
  {  case 0:
           scanner->opt[OPT_GAMMA_VECTOR].constraint.range = &gamma_range_8;
           scanner->lutlength=2048;
           break;
     case 1:
           scanner->opt[OPT_GAMMA_VECTOR].constraint.range = &gamma_range_9;
           scanner->lutlength=512;
           break;
     case 2:
           scanner->opt[OPT_GAMMA_VECTOR].constraint.range = &gamma_range_10;
           scanner->lutlength=1024;
           break;
     case 3:
           scanner->opt[OPT_GAMMA_VECTOR].constraint.range = &gamma_range_12;
           scanner->lutlength=4096;
           break;
  }
  scanner->opt[OPT_GAMMA_VECTOR].size = scanner->lutlength * sizeof (SANE_Word);
  scanner->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;

  /* red gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  scanner->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  switch(scanner->LS)
  {  case 0:
           scanner->opt[OPT_GAMMA_VECTOR_R].constraint.range = &gamma_range_8;
           scanner->lutlength=2048;
           break;
     case 1:
           scanner->opt[OPT_GAMMA_VECTOR_R].constraint.range = &gamma_range_9;
           scanner->lutlength=512;
           break;
     case 2:
           scanner->opt[OPT_GAMMA_VECTOR_R].constraint.range = &gamma_range_10;
           scanner->lutlength=1024;
           break;
     case 3:
           scanner->opt[OPT_GAMMA_VECTOR_R].constraint.range = &gamma_range_12;
           scanner->lutlength=4096;
           break;
  }
  scanner->opt[OPT_GAMMA_VECTOR_R].size = scanner->lutlength * sizeof (SANE_Word);
  scanner->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;

  /* green gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  scanner->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  switch(scanner->LS)
  {  case 0:
           scanner->opt[OPT_GAMMA_VECTOR_G].constraint.range = &gamma_range_8;
           scanner->lutlength=2048;
           break;
     case 1:
           scanner->opt[OPT_GAMMA_VECTOR_G].constraint.range = &gamma_range_9;
           scanner->lutlength=512;
           break;
     case 2:
           scanner->opt[OPT_GAMMA_VECTOR_G].constraint.range = &gamma_range_10;
           scanner->lutlength=1024;
           break;
     case 3:
           scanner->opt[OPT_GAMMA_VECTOR_G].constraint.range = &gamma_range_12;
           scanner->lutlength=4096;
           break;
  }
  scanner->opt[OPT_GAMMA_VECTOR_G].size = scanner->lutlength * sizeof (SANE_Word);
  scanner->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;

  /* blue gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  scanner->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  switch(scanner->LS)
  {  case 0:
           scanner->opt[OPT_GAMMA_VECTOR_B].constraint.range = &gamma_range_8;
           scanner->lutlength=2048;
           break;
     case 1:
           scanner->opt[OPT_GAMMA_VECTOR_B].constraint.range = &gamma_range_9;
           scanner->lutlength=512;
           break;
     case 2:
           scanner->opt[OPT_GAMMA_VECTOR_B].constraint.range = &gamma_range_10;
           scanner->lutlength=1024;
           break;
     case 3:
           scanner->opt[OPT_GAMMA_VECTOR_B].constraint.range = &gamma_range_12;
           scanner->lutlength=4096;
           break;
  }
  scanner->opt[OPT_GAMMA_VECTOR_B].size = scanner->lutlength * sizeof (SANE_Word);
  scanner->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;


  /* ------------------------------ */

  /* "Advanced" group: */
  scanner->opt[OPT_ADVANCED_GROUP].title = "Advanced";
  scanner->opt[OPT_ADVANCED_GROUP].desc = "";
  scanner->opt[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ADVANCED_GROUP].cap = SANE_CAP_ADVANCED;
  scanner->opt[OPT_ADVANCED_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* preview */
  scanner->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  scanner->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  scanner->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  scanner->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;

  /* Autofocus */
  scanner->opt[OPT_AUTOFOCUS].name = "Autofocus";
  scanner->opt[OPT_AUTOFOCUS].title ="Autofocus";
  scanner->opt[OPT_AUTOFOCUS].desc = "When to do autofocussing";
  scanner->opt[OPT_AUTOFOCUS].type = SANE_TYPE_STRING;
  scanner->opt[OPT_AUTOFOCUS].size = max_string_size (autofocus_mode_list);
  scanner->opt[OPT_AUTOFOCUS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_AUTOFOCUS].constraint.string_list = autofocus_mode_list;

  scanner->opt[OPT_IRED_RED].name = "IRED cor. red";
  scanner->opt[OPT_IRED_RED].title ="IRED cor. red";
  scanner->opt[OPT_IRED_RED].desc = "Correction of infrared from red";
  scanner->opt[OPT_IRED_RED].type = SANE_TYPE_INT;
  scanner->opt[OPT_IRED_RED].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_IRED_RED].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_IRED_RED].constraint.range = &gamma_range_8;
  scanner->opt[OPT_IRED_RED].cap |= SANE_CAP_ADVANCED;
  if(scanner->LS<2)
  {  scanner->opt[OPT_IRED_RED].cap |= SANE_CAP_INACTIVE;
  }



  /*  scanner->opt[OPT_PREVIEW].cap   = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT; */
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  authorize = authorize;

  DBG_INIT ();
  sanei_thread_init ();
  
  DBG (10, "sane_init\n");
  if (version_code)
      *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (COOLSCAN_CONFIG_FILE);
  if (!fp)
    {
      attach_scanner ("/dev/scanner", 0); /* no config-file: /dev/scanner */
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')
	continue;		/* ignore line comments */
      len = strlen (dev_name);

      if (!len)
	continue;		/* ignore empty lines */

      sanei_config_attach_matching_devices (dev_name, attach_one);
      /*attach_scanner (dev_name, 0);*/
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Coolscan_t *dev, *next;

  DBG (10, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->devicename);
      free (dev->buffer);
      free (dev->obuffer);
      free (dev);
    }
  
  if (devlist)
    free (devlist);
}

/* ----------------------------- SANE GET DEVICES -------------------------- */
SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool local_only)
{
  Coolscan_t *dev;
  int i;

  local_only = local_only;

  DBG (10, "sane_get_devices\n");

  if (devlist)
    free (devlist);

  devlist = calloc (num_devices + 1, sizeof (devlist[0]));
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
  Coolscan_t *dev;
  SANE_Status status;

  DBG (10, "sane_open\n");

  if (devicename[0])
    {				/* search for devicename */
      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devicename) == 0)
	    {
	      break;
	    }
	}

      if (!dev)
	{
	  status = attach_scanner (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    {
	      return status;
	    }
	}
    }
  else
    {
      dev = first_dev;		/* empty devicname -> use first device */
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  dev->sfd = -1;
  dev->pipe = -1;
  dev->scanning = SANE_FALSE;

  init_options (dev);
  *handle = dev;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  DBG (10, "sane_close\n");
  if (((Coolscan_t *) handle)->scanning)
    do_cancel (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Coolscan_t *scanner = handle;

  DBG (10, "sane_get_option_descriptor %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return &scanner->opt[option];
}

/*
   static void
   worddump(char *comment, SANE_Word * p, int l)
   {
   int i;
   char line[128];
   char *ptr;

   DBG (5, "%s\n", comment);
   ptr = line;
   for (i = 0; i < l; i++, p++)
   {
   if ((i % 8) == 0)
   {
   if (ptr != line)
   {
   *ptr = '\0';
   DBG (5, "%s\n", line);
   ptr = line;
   }
   sprintf (ptr, "%3.3d:", i);
   ptr += 4;
   }
   sprintf (ptr, " %4.4d", *p);
   ptr += 5;
   }
   *ptr = '\0';
   DBG (5, "%s\n", line);
   }
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val,
		     SANE_Int * info)
{
  Coolscan_t *scanner = handle;
  SANE_Status status;
  SANE_Word cap;

  if (info)
    *info = 0;

  if (scanner->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = scanner->opt[option].cap;


  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (10, "sane_control_option %d, get value\n", option);
      switch (option)
	{
	  /* word options: */
	case OPT_TL_X:
	  *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tlx));
	  return SANE_STATUS_GOOD;

	case OPT_TL_Y:
	  *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tly));
	  return SANE_STATUS_GOOD;

	case OPT_BR_X:
	  *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->brx));
	  return SANE_STATUS_GOOD;

	case OPT_BR_Y:
	  *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->bry));
	  return SANE_STATUS_GOOD;

	case OPT_PREVIEW:
	  *(SANE_Word *) val = scanner->preview;
	  return SANE_STATUS_GOOD;

	case OPT_AUTOFOCUS:
	  switch(scanner->autofocus)
	    {  case AF_NEVER: strcpy (val,neverStr);
  	                 break;
	       case AF_PREVIEW:strcpy (val,previewStr);
  	                 break;             
               case AF_SCAN:if(scanner->LS>=2) strcpy (val,scanStr);
  	                 break;             
	       case AF_PREANDSCAN:if(scanner->LS>=2) strcpy (val,preandscanStr);
  	                 break;             
	    }
	  return SANE_STATUS_GOOD;

	case OPT_NUM_OPTS:
	  *(SANE_Word *) val = NUM_OPTIONS;
	  return SANE_STATUS_GOOD;

	case OPT_RESOLUTION:
	  *(SANE_Word *) val = resDivToVal (scanner->x_nres);
	  return SANE_STATUS_GOOD;

	case OPT_PREVIEW_RESOLUTION:
	  *(SANE_Word *) val = resDivToVal (scanner->x_p_nres);
	  return SANE_STATUS_GOOD;

	case OPT_BIT_DEPTH:
	  *(SANE_Word *) val = scanner->bits_per_color;
	  return SANE_STATUS_GOOD;

	case OPT_CONTRAST:
	  *(SANE_Word *) val = scanner->contrast - 128;
	  return SANE_STATUS_GOOD;

	case OPT_R_CONTRAST:
	  *(SANE_Word *) val = scanner->contrast_R - 128;
	  return SANE_STATUS_GOOD;

	case OPT_G_CONTRAST:
	  *(SANE_Word *) val = scanner->contrast_G - 128;
	  return SANE_STATUS_GOOD;

	case OPT_B_CONTRAST:
	  *(SANE_Word *) val = scanner->contrast_B - 128;
	  return SANE_STATUS_GOOD;

	case OPT_BRIGHTNESS:
	  *(SANE_Word *) val = scanner->brightness - 128;
	  return SANE_STATUS_GOOD;

	case OPT_R_BRIGHTNESS:
	  *(SANE_Word *) val = scanner->brightness_R - 128;
	  return SANE_STATUS_GOOD;

	case OPT_G_BRIGHTNESS:
	  *(SANE_Word *) val = scanner->brightness_G - 128;
	  return SANE_STATUS_GOOD;

	case OPT_B_BRIGHTNESS:
	  *(SANE_Word *) val = scanner->brightness_B - 128;
	  return SANE_STATUS_GOOD;

	case OPT_EXPOSURE:
	  *(SANE_Word *) val = scanner->exposure_R * 2;
	  return SANE_STATUS_GOOD;

	case OPT_R_EXPOSURE:
	  *(SANE_Word *) val = scanner->exposure_R * 2;
	  return SANE_STATUS_GOOD;

	case OPT_G_EXPOSURE:
	  *(SANE_Word *) val = scanner->exposure_G * 2;
	  return SANE_STATUS_GOOD;

	case OPT_B_EXPOSURE:
	  *(SANE_Word *) val = scanner->exposure_B * 2;
	  return SANE_STATUS_GOOD;

	case OPT_R_SHIFT:
	  *(SANE_Word *) val = scanner->shift_R - 128;
	  return SANE_STATUS_GOOD;

	case OPT_G_SHIFT:
	  *(SANE_Word *) val = scanner->shift_G - 128;
	  return SANE_STATUS_GOOD;

	case OPT_B_SHIFT:
	  *(SANE_Word *) val = scanner->shift_B - 128;
	  return SANE_STATUS_GOOD;


	case OPT_IRED_RED:
	  *(SANE_Word *) val = scanner->ired_red;
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_TYPE:
	  strcpy (val, ((scanner->negative) ? negativeStr : positiveStr));
	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  switch(scanner->colormode)
	    {  case RGB: strcpy (val,colorStr);
  	                 break;
	       case GREYSCALE:strcpy (val,grayStr);
  	                 break;             
               case RGBI:if(scanner->LS>=2) strcpy (val,rgbiStr);
		         else strcpy (val,colorStr);
  	                 break;             
	       case IRED:if(scanner->LS>=2) strcpy (val,iredStr); 
	                 else strcpy (val,grayStr);
  	                 break;             
	    }
 	    if (info)
	    {
	      *info |= SANE_INFO_RELOAD_PARAMS;
	    }

	  return SANE_STATUS_GOOD;

	case OPT_PRESCAN:
	  *(SANE_Word *) val = (scanner->prescan) ? SANE_TRUE : SANE_FALSE;
	  return SANE_STATUS_GOOD;

	case OPT_PRESCAN_NOW:	 
	  return SANE_STATUS_GOOD;

	case OPT_RGB_CONTROL:
	  *(SANE_Word *) val = (scanner->rgb_control) ? SANE_TRUE : SANE_FALSE;
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_BIND:
	  *(SANE_Word *) val = (scanner->gamma_bind) ? SANE_TRUE : SANE_FALSE;
	  return SANE_STATUS_GOOD;

	case OPT_ANALOG_GAMMA:
	  *(SANE_Word *) val = 
	    (scanner->analog_gamma_r) ? SANE_TRUE : SANE_FALSE;
	  return SANE_STATUS_GOOD;

	case OPT_AVERAGING:
	  *(SANE_Word *) val = (scanner->averaging) ? SANE_TRUE : SANE_FALSE;
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR:
	  memcpy (val, scanner->gamma, scanner->opt[option].size);
	  return SANE_STATUS_GOOD;
	case OPT_GAMMA_VECTOR_R:
	  memcpy (val, scanner->gamma_r, scanner->opt[option].size);
	  return SANE_STATUS_GOOD;
	case OPT_GAMMA_VECTOR_G:
	  memcpy (val, scanner->gamma_g, scanner->opt[option].size);
	  return SANE_STATUS_GOOD;
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, scanner->gamma_b, scanner->opt[option].size);
	  return SANE_STATUS_GOOD;

	case OPT_SOURCE:
	  if (strcmp (val, "Automatic Slide Feeder") == 0)
	    {
	      /* Feed/Discharge/update filename/etc */
	    }
	  else
	    {
	      /* Reset above */
	    }
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_PARAMS;
	    }
	}

    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (10, "sane_control_option %d, set value\n", option);

      if (!SANE_OPTION_IS_ACTIVE (cap))
	return SANE_STATUS_INVAL;

      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      status = sanei_constrain_value (scanner->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	case OPT_GAMMA_BIND:
	  scanner->gamma_bind = (*(SANE_Word *) val == SANE_TRUE);
	  if (scanner->LS != 1)
	    {
	      if (scanner->gamma_bind)
		{
		  scanner->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		  scanner->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		  scanner->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		  scanner->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

		}
	      else
		{
		  scanner->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
		  scanner->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  scanner->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  scanner->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;

		}
	    }
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	    }
	  return SANE_STATUS_GOOD;

	case OPT_ANALOG_GAMMA:
	  scanner->analog_gamma_r = scanner->analog_gamma_g =
	    scanner->analog_gamma_b = (*(SANE_Word *) val == SANE_TRUE);
	  return SANE_STATUS_GOOD;

	case OPT_AVERAGING:
	  scanner->averaging = (*(SANE_Word *) val == SANE_TRUE);
	  return SANE_STATUS_GOOD;

	case OPT_PRESCAN:
	  scanner->prescan = (*(SANE_Word *) val == SANE_TRUE);
	  return SANE_STATUS_GOOD;

	case OPT_PRESCAN_NOW:
	   do_prescan_now(scanner);  
	  return SANE_STATUS_GOOD;

	case OPT_BIT_DEPTH:
	   scanner->bits_per_color=(*(SANE_Word *)val);
 	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;


	case OPT_RGB_CONTROL:
	  scanner->rgb_control = (*(SANE_Word *) val == SANE_TRUE);
	  if (scanner->rgb_control)
	    {
	      scanner->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;

	      scanner->opt[OPT_R_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_G_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_B_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;

	      scanner->opt[OPT_R_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_G_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_B_CONTRAST].cap &= ~SANE_CAP_INACTIVE;

	      scanner->opt[OPT_R_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_G_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_B_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;

	      scanner->contrast_R = 128;
	      scanner->contrast_G = 128;
	      scanner->contrast_B = 128;
	      scanner->brightness_R = 128;
	      scanner->brightness_G = 128;
	      scanner->brightness_B = 128;
	      scanner->exposure_R = 50;
	      scanner->exposure_G = 50;
	      scanner->exposure_B = 50;
	    }
	  else
	    {
	      scanner->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
	      scanner->opt[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;

	      scanner->contrast = 128;
	      scanner->brightness = 128;
	      scanner->exposure_R = 50;
	      scanner->exposure_G = 50;
	      scanner->exposure_B = 50;

	      scanner->opt[OPT_R_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_G_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_B_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;

	      scanner->opt[OPT_R_CONTRAST].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_G_CONTRAST].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_B_CONTRAST].cap |= SANE_CAP_INACTIVE;

	      scanner->opt[OPT_R_EXPOSURE].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_G_EXPOSURE].cap |= SANE_CAP_INACTIVE;
	      scanner->opt[OPT_B_EXPOSURE].cap |= SANE_CAP_INACTIVE;
	    }
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	    }
	  return SANE_STATUS_GOOD;

	case OPT_RESOLUTION:
	  scanner->y_nres = scanner->x_nres =
	    resValToDiv (*(SANE_Word *) val);

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_PREVIEW_RESOLUTION:
	  scanner->y_p_nres = scanner->x_p_nres =
	    resValToDiv (*(SANE_Word *) val);

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_TL_X:
	  scanner->tlx = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;

	  return SANE_STATUS_GOOD;

	case OPT_TL_Y:
	  scanner->tly = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_BR_X:
	  scanner->brx = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_BR_Y:
	  scanner->bry = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_NUM_OPTS:
	  return SANE_STATUS_GOOD;

	case OPT_PREVIEW:
	  scanner->preview = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	case OPT_AUTOFOCUS:
	  if(strcmp(val,neverStr)==0) 
	    {    scanner->autofocus=AF_NEVER;
	    }
	  if(strcmp(val,previewStr)==0)  
	    {    scanner->autofocus=AF_PREVIEW;
	    }
	  if(strcmp(val,scanStr)==0)  
	    {    scanner->autofocus=AF_SCAN;
	    }
	  if(strcmp(val,preandscanStr)==0)  
	    {    scanner->autofocus=AF_PREANDSCAN;;
	    }
	  return SANE_STATUS_GOOD;

	case OPT_CONTRAST:
	  scanner->contrast = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_R_CONTRAST:
	  scanner->contrast_R = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_G_CONTRAST:
	  scanner->contrast_G = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_B_CONTRAST:
	  scanner->contrast_B = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;

	case OPT_BRIGHTNESS:
	  scanner->brightness = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_R_BRIGHTNESS:
	  scanner->brightness_R = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_G_BRIGHTNESS:
	  scanner->brightness_G = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_B_BRIGHTNESS:
	  scanner->brightness_B = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;

	case OPT_EXPOSURE:
	  scanner->exposure_R = *(SANE_Word *) val / 2;
	  scanner->exposure_G = *(SANE_Word *) val / 2;
	  scanner->exposure_B = *(SANE_Word *) val / 2;
	  return SANE_STATUS_GOOD;
	case OPT_R_EXPOSURE:
	  scanner->exposure_R = *(SANE_Word *) val / 2;
	  return SANE_STATUS_GOOD;
	case OPT_G_EXPOSURE:
	  scanner->exposure_G = *(SANE_Word *) val / 2;
	  return SANE_STATUS_GOOD;
	case OPT_B_EXPOSURE:
	  scanner->exposure_B = *(SANE_Word *) val / 2;
	  return SANE_STATUS_GOOD;

	case OPT_R_SHIFT:
	  scanner->shift_R = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_G_SHIFT:
	  scanner->shift_G = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;
	case OPT_B_SHIFT:
	  scanner->shift_B = *(SANE_Word *) val + 128;
	  return SANE_STATUS_GOOD;

	case OPT_IRED_RED:
	  scanner->ired_red= *(SANE_Word *) val; 
	  return SANE_STATUS_GOOD;

	case OPT_SOURCE:
	  scanner->asf = (strcmp (val, "Automatic...") == 0);
	  return SANE_STATUS_GOOD;

	case OPT_TYPE:
	  scanner->negative = (strcmp (val, negativeStr) == 0);
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  return SANE_STATUS_GOOD;
	case OPT_MODE:
	  if(strcmp(val,colorStr)==0) 
	    {    scanner->colormode=RGB;
	         scanner->colormode_p=RGB;
	    }
	  if(strcmp(val,grayStr)==0)  
	    {    scanner->colormode=GREYSCALE;
	         scanner->colormode_p=GREYSCALE;
	    }
	  if(strcmp(val,rgbiStr)==0)
	    {    scanner->colormode=RGBI;
	         scanner->colormode_p=RGB;
	    }
	  if(strcmp(val,iredStr)==0) 
	    {    scanner->colormode=IRED;
	         scanner->colormode_p=GREYSCALE;
	    }
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	    }
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR:
	  memcpy (scanner->gamma, val, scanner->opt[option].size);
	  if(scanner->LS>2)  Calc_fix_LUT(scanner);
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR_R:
	  memcpy (scanner->gamma_r, val, scanner->opt[option].size);
	  if(scanner->LS>2)  Calc_fix_LUT(scanner);
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR_G:
	  memcpy (scanner->gamma_g, val, scanner->opt[option].size);
	  if(scanner->LS>2)  Calc_fix_LUT(scanner);
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR_B:
	  memcpy (scanner->gamma_b, val, scanner->opt[option].size);
	  if(scanner->LS>2)  Calc_fix_LUT(scanner);
	  return SANE_STATUS_GOOD;

	}			/* switch */
    }				/* else */
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Coolscan_t *scanner = handle;

  DBG (10, "sane_get_parameters");
  switch(scanner->colormode)
    {  case RGB:
              params->format =  SANE_FRAME_RGB;
              break;
#ifdef HAS_IRED
       case RGBI:
              params->format =  SANE_FRAME_RGBA; 
              break; 
#endif /* HAS_RGBI */
       case GREYSCALE:
              params->format =  SANE_FRAME_GRAY;
              break;
    }

  params->depth = scanner->bits_per_color>8?16:8;
  params->pixels_per_line = pixels_per_line (scanner);
  params->lines = lines_per_scan (scanner);
  params->bytes_per_line = write_bytes_per_line (scanner);
  params->last_frame = 1;
  return SANE_STATUS_GOOD;
}
static int
swap_res (Coolscan_t * s)
{
  if (s->preview)
    {
      /* swap preview/scan resolutions */
      int xres, yres, cmode;
      xres = s->x_nres;
      yres = s->y_nres;
      s->x_nres = s->x_p_nres;
      s->y_nres = s->y_p_nres;

      s->x_p_nres = xres;
      s->y_p_nres = yres;
      cmode=s->colormode;
      s->colormode=s->colormode_p;
      s->colormode_p=cmode;
    }
  return 0;
}
SANE_Status
sane_start (SANE_Handle handle)
{
  Coolscan_t *scanner = handle;
  int fds[2];

  DBG (10, "sane_start\n");
  if (scanner->scanning == SANE_TRUE)
    return SANE_STATUS_DEVICE_BUSY;

  if (scanner->sfd < 0)
    {				/* first call */
      if (sanei_scsi_open (scanner->sane.name,
			   &(scanner->sfd),
			   sense_handler, 0) != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_start: open of %s failed:\n",
	       scanner->sane.name);
	  return SANE_STATUS_INVAL;
	}
    }
  scanner->scanning = SANE_TRUE;


  if (coolscan_check_values (scanner) != 0)
    {				/* Verify values */
      DBG (1, "ERROR: invalid scan-values\n");
      scanner->scanning = SANE_FALSE;
      coolscan_give_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      return SANE_STATUS_INVAL;
    }

  if (coolscan_grab_scanner (scanner))
    {
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      DBG (5, "WARNING: unable to reserve scanner: device busy\n");
      scanner->scanning = SANE_FALSE;
      return SANE_STATUS_DEVICE_BUSY;
    }

  /* hoho, step 2c, -perm */
  coolscan_object_feed (scanner);

  swap_res (scanner);
  if (!scanner->preview)
  { if(scanner->autofocus & 0x02)
    {  coolscan_autofocus (scanner);
    }
  }
  else
    { 
      if(scanner->autofocus & 0x01)
      {  coolscan_autofocus (scanner);
      }
      if (scanner->prescan) {
	prescan (scanner);  
	if(scanner->LS<2)
        {	get_internal_info(scanner);
	}
        coolscan_get_window_param (scanner,1);
      }
    }
  /*read_LUT(scanner); */
  if(scanner->LS<2)
  {  send_LUT (scanner);
     coolscan_set_window_param (scanner, 0);
     coolscan_get_window_param (scanner,0);
     coolscan_start_scan (scanner);
  }
  else
  {  coolscan_set_window_param (scanner, 0);
     send_LUT (scanner);
     Calc_fix_LUT(scanner);
     coolscan_start_scan (scanner);
     wait_scanner (scanner);
     coolscan_get_window_param (scanner,0);
  }

  DBG (10, "bytes per line        = %d\n", scan_bytes_per_line (scanner));
  DBG (10, "pixels_per_line       = %d\n", pixels_per_line (scanner));
  DBG (10, "lines                 = %d\n", lines_per_scan (scanner));
  DBG (10, "negative              = %d\n", scanner->negative);
  DBG (10, "brightness (halftone) = %d\n", scanner->brightness);
  DBG (10, "contrast   (halftone) = %d\n", scanner->contrast);
  DBG (10, "fast preview function = %d\n", scanner->preview);

  /* create a pipe, fds[0]=read-fd, fds[1]=write-fd */
  if (pipe (fds) < 0)
    {
      DBG (1, "ERROR: could not create pipe\n");

      swap_res (scanner);
      scanner->scanning = SANE_FALSE;
      coolscan_give_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      return SANE_STATUS_IO_ERROR;
    }

  scanner->pipe       = fds[0];
  scanner->reader_fds = fds[1];
  scanner->reader_pid = sanei_thread_begin( reader_process, (void*)scanner );
  if (scanner->reader_pid == -1)
    {
      DBG (1, "sane_start: sanei_thread_begin failed (%s)\n",
             strerror (errno));
      return SANE_STATUS_NO_MEM;
    }

  if (sanei_thread_is_forked ())
    {
      close (scanner->reader_fds);
      scanner->reader_fds = -1;
    }

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
	   SANE_Int max_len, SANE_Int * len)
{
  Coolscan_t *scanner = handle;
  ssize_t nread;

  *len = 0;

  nread = read (scanner->pipe, buf, max_len);
  DBG (10, "sane_read: read %ld bytes\n", (long) nread);

  if (!(scanner->scanning))
    {
      return do_cancel (scanner);
    }

  if (nread < 0)
    {
      if (errno == EAGAIN)
	{
	  return SANE_STATUS_GOOD;
	}
      else
	{
	  do_cancel (scanner);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  *len = nread;

  if (nread == 0)
    return do_eof (scanner);	/* close pipe */

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Coolscan_t *s = handle;

  if (s->reader_pid != -1)
    {
      sanei_thread_kill   ( s->reader_pid );
      sanei_thread_waitpid( s->reader_pid, NULL );
      s->reader_pid = -1;
    }
  swap_res (s);
  s->scanning = SANE_FALSE;
}


SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Coolscan_t *scanner = handle;

  DBG (10, "sane_set_io_mode: non_blocking=%d\n", non_blocking);

  if (!scanner->scanning)
    return SANE_STATUS_INVAL;

  if (fcntl (scanner->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Coolscan_t *scanner = handle;

  DBG (10, "sane_get_select_fd\n");

  if (!scanner->scanning)
    {
      return SANE_STATUS_INVAL;
    }
  *fd = scanner->pipe;

  return SANE_STATUS_GOOD;
}
