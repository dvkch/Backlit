/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2005-2007 Henning Geinitz <sane@geinitz.org>
   
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

/** @file
 * @brief GT68xx commands common for most GT68xx-based scanners.
 */

#include "gt68xx_generic.h"


SANE_Status
gt68xx_generic_move_relative (GT68xx_Device * dev, SANE_Int distance)
{
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  if (distance >= 0)
    req[0] = 0x14;
  else
    {
      req[0] = 0x15;
      distance = -distance;
    }
  req[1] = 0x01;
  req[2] = LOBYTE (distance);
  req[3] = HIBYTE (distance);

  return gt68xx_device_req (dev, req, req);
}

SANE_Status
gt68xx_generic_start_scan (GT68xx_Device * dev)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));
  req[0] = 0x43;
  req[1] = 0x01;
  RIE (gt68xx_device_req (dev, req, req));
  RIE (gt68xx_device_check_result (req, 0x43));

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_generic_read_scanned_data (GT68xx_Device * dev, SANE_Bool * ready)
{
  SANE_Status status;
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x35;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));

  *ready = SANE_FALSE;
  if (req[0] == 0)
    *ready = SANE_TRUE;

  return SANE_STATUS_GOOD;
}

static SANE_Byte
gt68xx_generic_fix_gain (SANE_Int gain)
{
  if (gain < 0)
    gain = 0;
  else if (gain > 31)
    gain += 12;
  else if (gain > 51)
    gain = 63;
    
  return gain;
}

static SANE_Byte
gt68xx_generic_fix_offset (SANE_Int offset)
{
  if (offset < 0)
    offset = 0;
  else if (offset > 63)
    offset = 63;
  return offset;
}

SANE_Status
gt68xx_generic_set_afe (GT68xx_Device * dev, GT68xx_AFE_Parameters * params)
{
  GT68xx_Packet req;

  memset (req, 0, sizeof (req));
  req[0] = 0x22;
  req[1] = 0x01;
  req[2] = gt68xx_generic_fix_offset (params->r_offset);
  req[3] = gt68xx_generic_fix_gain (params->r_pga);
  req[4] = gt68xx_generic_fix_offset (params->g_offset);
  req[5] = gt68xx_generic_fix_gain (params->g_pga);
  req[6] = gt68xx_generic_fix_offset (params->b_offset);
  req[7] = gt68xx_generic_fix_gain (params->b_pga);

  DBG (6,
       "gt68xx_generic_set_afe: real AFE: 0x%02x 0x%02x  0x%02x 0x%02x  0x%02x 0x%02x\n",
       req[2], req[3], req[4], req[5], req[6], req[7]);
  return gt68xx_device_req (dev, req, req);
}

SANE_Status
gt68xx_generic_set_exposure_time (GT68xx_Device * dev,
				  GT68xx_Exposure_Parameters * params)
{
  GT68xx_Packet req;
  SANE_Status status;

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

  DBG (6, "gt68xx_generic_set_exposure_time: 0x%03x 0x%03x 0x%03x\n",
       params->r_time, params->g_time, params->b_time);

  RIE (gt68xx_device_req (dev, req, req));
  RIE (gt68xx_device_check_result (req, 0x76));
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_generic_get_id (GT68xx_Device * dev)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));
  req[0] = 0x2e;
  req[1] = 0x01;
  RIE (gt68xx_device_req (dev, req, req));
  RIE (gt68xx_device_check_result (req, 0x2e));

  DBG (2,
       "get_id: vendor id=0x%04X, product id=0x%04X, DID=0x%08X, FID=0x%04X\n",
       req[2] + (req[3] << 8), req[4] + (req[5] << 8),
       req[6] + (req[7] << 8) + (req[8] << 16) + (req[9] << 24),
       req[10] + (req[11] << 8));
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_generic_paperfeed (GT68xx_Device * dev)
{
  GT68xx_Packet req;
  SANE_Status status;

  memset (req, 0, sizeof (req));
  req[0] = 0x83;
  req[1] = 0x01;

  RIE (gt68xx_device_req (dev, req, req));
  return SANE_STATUS_GOOD;
}

#define MAX_PIXEL_MODE 15600

SANE_Status
gt68xx_generic_setup_scan (GT68xx_Device * dev,
			   GT68xx_Scan_Request * request,
			   GT68xx_Scan_Action action,
			   GT68xx_Scan_Parameters * params)
{
  SANE_Status status;
  GT68xx_Model *model;
  SANE_Int xdpi, ydpi;
  SANE_Bool color;
  SANE_Int depth;
  SANE_Int pixel_x0, pixel_y0, pixel_xs, pixel_ys;
  SANE_Int pixel_align;

  SANE_Int abs_x0, abs_y0, abs_xs, abs_ys, base_xdpi, base_ydpi;
  SANE_Int scan_xs, scan_ys, scan_bpl;
  SANE_Int bits_per_line;
  SANE_Byte color_mode_code;
  SANE_Bool line_mode;
  SANE_Int overscan_lines;
  SANE_Fixed x0, y0, xs, ys;
  SANE_Bool backtrack = SANE_FALSE;

  DBG (6, "gt6816_setup_scan: enter (action=%s)\n",
       action == SA_CALIBRATE ? "calibrate" :
       action == SA_CALIBRATE_ONE_LINE ? "calibrate one line" :
       action == SA_SCAN ? "scan" : "calculate only");

  model = dev->model;

  xdpi = request->xdpi;
  ydpi = request->ydpi;
  color = request->color;
  depth = request->depth;

  base_xdpi = model->base_xdpi;
  base_ydpi = model->base_ydpi;

  if (xdpi > model->base_xdpi)
    base_xdpi = model->optical_xdpi;

  /* Special fixes */
  if ((dev->model->flags & GT68XX_FLAG_USE_OPTICAL_X) && xdpi <= 50)
    base_xdpi = model->optical_xdpi;

  if ((dev->model->flags & GT68XX_FLAG_SCAN_FROM_HOME) &&
      !request->use_ta && action == SA_SCAN)
    request->mbs = SANE_TRUE;

  if (!model->constant_ydpi)
    {
      if (ydpi > model->base_ydpi)
	base_ydpi = model->optical_ydpi;
    }

  DBG (6, "gt68xx_generic_setup_scan: base_xdpi=%d, base_ydpi=%d\n",
       base_xdpi, base_ydpi);

  switch (action)
    {
    case SA_CALIBRATE_ONE_LINE:
      {
	x0 = request->x0;
	if (request->use_ta)
	  y0 = model->y_offset_calib_ta;
	else
	  y0 = model->y_offset_calib;
	ys = SANE_FIX (1.0 * MM_PER_INCH / ydpi);	/* one line */
	xs = request->xs;
	depth = 8;
	break;
      }
    case SA_CALIBRATE:
      {
	if (request->use_ta)
	  {
	    if (dev->model->flags & GT68XX_FLAG_MIRROR_X)
	      x0 = request->x0 - model->x_offset_ta;
	    else
	      x0 = request->x0 + model->x_offset_ta;
	    if (request->mbs)
	      y0 = model->y_offset_calib_ta;
	    else
	      y0 = 0;
	  }
	else
	  {
	    if (dev->model->flags & GT68XX_FLAG_MIRROR_X)
	      x0 = request->x0 - model->x_offset;
	    else
	      x0 = request->x0 + model->x_offset;
	    if (request->mbs)
	      y0 = model->y_offset_calib;
	    else
	      y0 = 0;
	  }
	ys = SANE_FIX (CALIBRATION_HEIGHT);
	xs = request->xs;
	break;
      }
    case SA_SCAN:
      {
	SANE_Fixed x_offset, y_offset;

	if (strcmp (dev->model->command_set->name, "mustek-gt6816") != 0)
	  request->mbs = SANE_TRUE;	/* always go home for gt6801 scanners */
	if (request->use_ta)
	  {
	    x_offset = model->x_offset_ta;
	    if (request->mbs)
	      y_offset = model->y_offset_ta;
	    else
	      {
		y_offset = model->y_offset_ta - model->y_offset_calib_ta
		  - SANE_FIX (CALIBRATION_HEIGHT);
		if ((request->y0 + y_offset) < 0)
		  {
		    y_offset = model->y_offset_ta;
		    request->mbs = SANE_TRUE;
		  }
	      }
		  
	  }
	else
	  {
	    x_offset = model->x_offset;
	    if (request->mbs)
	      y_offset = model->y_offset;
	    else
	      {
		y_offset = model->y_offset - model->y_offset_calib
		  - SANE_FIX (CALIBRATION_HEIGHT);
		if ((request->y0 + y_offset) < 0)
		  {
		    y_offset = model->y_offset;
		    request->mbs = SANE_TRUE;
		  }
	      }

	  }
	if (dev->model->flags & GT68XX_FLAG_MIRROR_X)
	  x0 = request->x0 - x_offset;
	else
	  x0 = request->x0 + x_offset;
	y0 = request->y0 + y_offset;
	if (y0 < 0)
	  y0 = 0;
	ys = request->ys;
	xs = request->xs;
	backtrack = request->backtrack;
	break;
      }

    default:
      DBG (1, "gt68xx_generic_setup_scan: invalid action=%d\n", (int) action);
      return SANE_STATUS_INVAL;
    }

  pixel_x0 = SANE_UNFIX (x0) * xdpi / MM_PER_INCH + 0.5;
  pixel_y0 = SANE_UNFIX (y0) * ydpi / MM_PER_INCH + 0.5;
  pixel_ys = SANE_UNFIX (ys) * ydpi / MM_PER_INCH + 0.5;
  pixel_xs = SANE_UNFIX (xs) * xdpi / MM_PER_INCH + 0.5;


  DBG (6, "gt68xx_generic_setup_scan: xdpi=%d, ydpi=%d\n", xdpi, ydpi);
  DBG (6, "gt68xx_generic_setup_scan: color=%s, depth=%d\n",
       color ? "TRUE" : "FALSE", depth);
  DBG (6, "gt68xx_generic_setup_scan: pixel_x0=%d, pixel_y0=%d\n",
       pixel_x0, pixel_y0);
  DBG (6, "gt68xx_generic_setup_scan: pixel_xs=%d, pixel_ys=%d\n",
       pixel_xs, pixel_ys);


  color_mode_code = 0x80;
  if (color)
    color_mode_code |= (1 << 2);
  else
    color_mode_code |= dev->gray_mode_color;

  if (depth > 12)
    color_mode_code |= (1 << 5);
  else if (depth > 8)
    {
      color_mode_code &= 0x7f;
      color_mode_code |= (1 << 4);
    }

  DBG (6, "gt68xx_generic_setup_scan: color_mode_code = 0x%02X\n",
       color_mode_code);

  overscan_lines = 0;
  params->ld_shift_r = params->ld_shift_g = params->ld_shift_b = 0;
  params->ld_shift_double = 0;

  /* Line distance correction is required for color scans. */
  if (action == SA_SCAN && color)
    {
      SANE_Int optical_ydpi = model->optical_ydpi;
      SANE_Int ld_shift_r = model->ld_shift_r;
      SANE_Int ld_shift_g = model->ld_shift_g;
      SANE_Int ld_shift_b = model->ld_shift_b;
      SANE_Int max_ld = MAX (MAX (ld_shift_r, ld_shift_g), ld_shift_b);

      overscan_lines = max_ld * ydpi / optical_ydpi;
      params->ld_shift_r = ld_shift_r * ydpi / optical_ydpi;
      params->ld_shift_g = ld_shift_g * ydpi / optical_ydpi;
      params->ld_shift_b = ld_shift_b * ydpi / optical_ydpi;
      params->ld_shift_double = 0;
      DBG (6, "gt68xx_generic_setup_scan: overscan=%d, ld=%d/%d/%d\n",
	   overscan_lines, params->ld_shift_r, params->ld_shift_g,
	   params->ld_shift_b);
    }

  /* Used for CCD scanners with 6 instead of 3 CCD lines */
  if (action == SA_SCAN && xdpi >= model->optical_xdpi
      && model->ld_shift_double > 0)
    {
      params->ld_shift_double =
	model->ld_shift_double * ydpi / model->optical_ydpi;
      if (color)
	overscan_lines += (params->ld_shift_double * 3);
      else
	overscan_lines += params->ld_shift_double;

      DBG (6, "gt68xx_generic_setup_scan: overscan=%d, ld double=%d\n",
	   overscan_lines, params->ld_shift_double);
    }

  abs_x0 = pixel_x0 * base_xdpi / xdpi;
  abs_y0 = pixel_y0 * base_ydpi / ydpi;
  DBG (6, "gt68xx_generic_setup_scan: abs_x0=%d, abs_y0=%d\n", abs_x0,
       abs_y0);

  params->double_column = abs_x0 & 1;

  /* Calculate minimum number of pixels which span an integral multiple of 64
   * bytes. */
  pixel_align = 32;		/* best case for depth = 16 */
  while ((depth * pixel_align) % (64 * 8) != 0)
    pixel_align *= 2;
  DBG (6, "gt68xx_generic_setup_scan: pixel_align=%d\n", pixel_align);

  if (pixel_xs % pixel_align == 0)
    scan_xs = pixel_xs;
  else
    scan_xs = (pixel_xs / pixel_align + 1) * pixel_align;
  scan_ys = pixel_ys + overscan_lines;

  if ((xdpi != base_xdpi)
      && (strcmp (dev->model->command_set->name, "mustek-gt6816") != 0))
    abs_xs = (scan_xs - 1) * base_xdpi / xdpi;	/* gt6801 */
  else
    abs_xs = scan_xs * base_xdpi / xdpi;	/* gt6816 */

  if (action == SA_CALIBRATE_ONE_LINE)
    abs_ys = 2;
  else
    abs_ys = scan_ys * base_ydpi / ydpi;
  DBG (6, "gt68xx_generic_setup_scan: abs_xs=%d, abs_ys=%d\n", abs_xs,
       abs_ys);

  if (model->flags & GT68XX_FLAG_NO_LINEMODE)
    {
      line_mode = SANE_FALSE;
      DBG (6,
	   "gt68xx_generic_setup_scan: using pixel mode (GT68XX_FLAG_NO_LINEMODE)\n");
    }
  else if (model->is_cis && !(model->flags & GT68XX_FLAG_CIS_LAMP))
    {
      line_mode = SANE_TRUE;
      DBG (6, "gt68xx_generic_setup_scan: using line mode (CIS)\n");
    }
  else if (model->flags & GT68XX_FLAG_ALWAYS_LINEMODE)
    {
      line_mode = SANE_TRUE;
      DBG (6,
	   "gt68xx_generic_setup_scan: using line mode (GT68XX_FLAG_ALWAYS_LINEMODE)\n");
    }
  else
    {
      SANE_Int max_bpl = xdpi * 3 * depth *
	(SANE_UNFIX (model->x_size) -
	 SANE_UNFIX (model->x_offset)) / MM_PER_INCH / 8;

      line_mode = SANE_FALSE;
      if (!color)
	{
	  DBG (6,
	       "gt68xx_generic_setup_scan: using line mode for monochrome scan\n");
	  line_mode = SANE_TRUE;
	}
      else if (max_bpl > MAX_PIXEL_MODE)
	{
	  DBG (6,
	       "gt68xx_generic_setup_scan: max_bpl = %d > %d: forcing line mode\n",
	       max_bpl, MAX_PIXEL_MODE);
	  line_mode = SANE_TRUE;
	}
      else
	DBG (6,
	     "gt68xx_generic_setup_scan: max_bpl = %d <= %d: using pixel mode\n",
	     max_bpl, MAX_PIXEL_MODE);
    }

  bits_per_line = depth * scan_xs;

  if (color && !line_mode)
    bits_per_line *= 3;

  if (bits_per_line % 8)	/* impossible */
    {
      DBG (0, "gt68xx_generic_setup_scan: BUG: unaligned bits_per_line=%d\n",
	   bits_per_line);
      return SANE_STATUS_INVAL;
    }
  scan_bpl = bits_per_line / 8;

  if (scan_bpl % 64)		/* impossible */
    {
      DBG (0, "gt68xx_generic_setup_scan: BUG: unaligned scan_bpl=%d\n",
	   scan_bpl);
      return SANE_STATUS_INVAL;
    }

  if (color)
    if (line_mode || dev->model->flags & GT68XX_FLAG_SE_2400)
      scan_ys *= 3;

  DBG (6, "gt68xx_generic_setup_scan: scan_xs=%d, scan_ys=%d\n", scan_xs,
       scan_ys);

  DBG (6, "gt68xx_generic_setup_scan: scan_bpl=%d\n", scan_bpl);

  if (!request->calculate)
    {
      GT68xx_Packet req;
      SANE_Byte motor_mode_1, motor_mode_2;

      if (scan_bpl > (16 * 1024))
	{
	  DBG (0, "gt68xx_generic_setup_scan: scan_bpl=%d, too large\n",
	       scan_bpl);
	  return SANE_STATUS_NO_MEM;
	}

      if ((dev->model->flags & GT68XX_FLAG_NO_LINEMODE) && line_mode && color)
	{
	  DBG (0,
	       "gt68xx_generic_setup_scan: the scanner's memory is too small for "
	       "that combination of resolution, dpi and width\n");
	  return SANE_STATUS_NO_MEM;
	}
      DBG (6, "gt68xx_generic_setup_scan: backtrack=%d\n", backtrack);

      motor_mode_1 = (request->mbs ? 0 : 1) << 1;
      motor_mode_1 |= (request->mds ? 0 : 1) << 2;
      motor_mode_1 |= (request->mas ? 0 : 1) << 0;
      motor_mode_1 |= (backtrack ? 1 : 0) << 3;

      motor_mode_2 = (request->lamp ? 0 : 1) << 0;
      motor_mode_2 |= (line_mode ? 0 : 1) << 2;

      if ((action != SA_SCAN) 
	  && (strcmp (dev->model->command_set->name, "mustek-gt6816") == 0))
	motor_mode_2 |= 1 << 3;

      DBG (6,
	   "gt68xx_generic_setup_scan: motor_mode_1 = 0x%02X, motor_mode_2 = 0x%02X\n",
	   motor_mode_1, motor_mode_2);

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
      if (model->is_cis && !(model->flags & GT68XX_FLAG_CIS_LAMP))
	req[0x0b] = 0x60;
      else
	req[0x0b] = 0x20;
      req[0x0c] = LOBYTE (xdpi);
      req[0x0d] = HIBYTE (xdpi);
      req[0x0e] = 0x12;		/* ??? 0x12 */
      req[0x0f] = 0x00;		/* ??? 0x00 */
      req[0x10] = LOBYTE (scan_bpl);
      req[0x11] = HIBYTE (scan_bpl);
      req[0x12] = LOBYTE (scan_ys);
      req[0x13] = HIBYTE (scan_ys);
      req[0x14] = motor_mode_1;
      req[0x15] = motor_mode_2;
      req[0x16] = LOBYTE (ydpi);
      req[0x17] = HIBYTE (ydpi);
      if (backtrack)
	req[0x18] = request->backtrack_lines;
      else
	req[0x18] = 0x00;

      status = gt68xx_device_req (dev, req, req);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3, "gt68xx_generic_setup_scan: setup request failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      RIE (gt68xx_device_check_result (req, 0x20));
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
  params->line_mode = line_mode;
  params->overscan_lines = overscan_lines;
  params->pixel_x0 = pixel_x0;

  DBG (6, "gt68xx_generic_setup_scan: leave: ok\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_generic_move_paper (GT68xx_Device * dev,
			   GT68xx_Scan_Request * request)
{
  GT68xx_Packet req;
  SANE_Status status;
  SANE_Int ydpi;
  SANE_Int pixel_y0;
  SANE_Int abs_y0, base_ydpi;
  GT68xx_Model *model = dev->model;

  ydpi = request->ydpi;
  base_ydpi = model->base_ydpi;

  if (ydpi > model->base_ydpi)
    ydpi = base_ydpi;
    
  pixel_y0 =
    SANE_UNFIX ((request->y0 + model->y_offset)) * ydpi / MM_PER_INCH + 0.5;
  abs_y0 = pixel_y0 * base_ydpi / ydpi;
  
  DBG (6, "gt68xx_generic_move_paper: base_ydpi=%d\n", base_ydpi);
  DBG (6, "gt68xx_generic_move_paper: ydpi=%d\n", ydpi);
  DBG (6, "gt68xx_generic_move_paper: abs_y0=%d\n", abs_y0);

  /* paper move request */
  memset (req, 0, sizeof (req));
  req[0] = 0x82;
  req[1] = 0x01;
  req[2] = LOBYTE (abs_y0);
  req[3] = HIBYTE (abs_y0);
  RIE (gt68xx_device_req (dev, req, req));

  DBG (6, "gt68xx_generic_move_paper: leave: ok\n");
  return SANE_STATUS_GOOD;
}
