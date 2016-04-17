/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   AFE offset/gain setting by David Stevenson <david.stevenson@zoom.co.uk>
   Copyright (C) 2002 - 2007 Henning Geinitz <sane@geinitz.org>
   Copyright (C) 2009 Stéphane Voltz <stef.dev@free.fr> for sheetfed
                      calibration code.

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

#include "gt68xx_high.h"
#include "gt68xx_mid.c"

#include <unistd.h>
#include <math.h>

static SANE_Status
gt68xx_afe_ccd_auto (GT68xx_Scanner * scanner, GT68xx_Scan_Request * request);
static SANE_Status gt68xx_afe_cis_auto (GT68xx_Scanner * scanner);

SANE_Status
gt68xx_calibrator_new (SANE_Int width,
		       SANE_Int white_level, GT68xx_Calibrator ** cal_return)
{
  GT68xx_Calibrator *cal;
  SANE_Int i;

  DBG (4, "gt68xx_calibrator_new: enter: width=%d, white_level=%d\n",
       width, white_level);

  *cal_return = 0;

  if (width <= 0)
    {
      DBG (5, "gt68xx_calibrator_new: invalid width=%d\n", width);
      return SANE_STATUS_INVAL;
    }

  cal = (GT68xx_Calibrator *) malloc (sizeof (GT68xx_Calibrator));
  if (!cal)
    {
      DBG (5, "gt68xx_calibrator_new: no memory for GT68xx_Calibrator\n");
      return SANE_STATUS_NO_MEM;
    }

  cal->k_white = NULL;
  cal->k_black = NULL;
  cal->white_line = NULL;
  cal->black_line = NULL;
  cal->width = width;
  cal->white_level = white_level;
  cal->white_count = 0;
  cal->black_count = 0;
#ifdef TUNE_CALIBRATOR
  cal->min_clip_count = cal->max_clip_count = 0;
#endif /* TUNE_CALIBRATOR */

  cal->k_white = (unsigned int *) malloc (width * sizeof (unsigned int));
  cal->k_black = (unsigned int *) malloc (width * sizeof (unsigned int));
  cal->white_line = (double *) malloc (width * sizeof (double));
  cal->black_line = (double *) malloc (width * sizeof (double));

  if (!cal->k_white || !cal->k_black | !cal->white_line || !cal->black_line)
    {
      DBG (5, "gt68xx_calibrator_new: no memory for calibration data\n");
      gt68xx_calibrator_free (cal);
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0; i < width; ++i)
    {
      cal->k_white[i] = 0;
      cal->k_black[i] = 0;
      cal->white_line[i] = 0.0;
      cal->black_line[i] = 0.0;
    }

  *cal_return = cal;
  DBG (5, "gt68xx_calibrator_new: leave: ok\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_free (GT68xx_Calibrator * cal)
{
  DBG (5, "gt68xx_calibrator_free: enter\n");

  if (!cal)
    {
      DBG (5, "gt68xx_calibrator_free: cal==NULL\n");
      return SANE_STATUS_INVAL;
    }

#ifdef TUNE_CALIBRATOR
  DBG (4, "gt68xx_calibrator_free: min_clip_count=%d, max_clip_count=%d\n",
       cal->min_clip_count, cal->max_clip_count);
#endif /* TUNE_CALIBRATOR */

  if (cal->k_white)
    {
      free (cal->k_white);
      cal->k_white = NULL;
    }

  if (cal->k_black)
    {
      free (cal->k_black);
      cal->k_black = NULL;
    }

  if (cal->white_line)
    {
      free (cal->white_line);
      cal->white_line = NULL;
    }

  if (cal->black_line)
    {
      free (cal->black_line);
      cal->black_line = NULL;
    }

  free (cal);

  DBG (5, "gt68xx_calibrator_free: leave: ok\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_add_white_line (GT68xx_Calibrator * cal, unsigned int *line)
{
  SANE_Int i;
  SANE_Int width = cal->width;

  SANE_Int sum = 0;

  cal->white_count++;

  for (i = 0; i < width; ++i)
    {
      cal->white_line[i] += line[i];
      sum += line[i];
#ifdef SAVE_WHITE_CALIBRATION
      printf ("%c", line[i] >> 8);
#endif
    }
  if (sum / width / 256 < 0x50)
    DBG (1,
	 "gt68xx_calibrator_add_white_line: WARNING: dark calibration line: "
	 "%2d medium white: 0x%02x\n", cal->white_count - 1,
	 sum / width / 256);
  else
    DBG (5,
	 "gt68xx_calibrator_add_white_line: line: %2d medium white: 0x%02x\n",
	 cal->white_count - 1, sum / width / 256);

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_eval_white (GT68xx_Calibrator * cal, double factor)
{
  SANE_Int i;
  SANE_Int width = cal->width;

  for (i = 0; i < width; ++i)
    {
      cal->white_line[i] = cal->white_line[i] / cal->white_count * factor;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_add_black_line (GT68xx_Calibrator * cal, unsigned int *line)
{
  SANE_Int i;
  SANE_Int width = cal->width;

  SANE_Int sum = 0;

  cal->black_count++;

  for (i = 0; i < width; ++i)
    {
      cal->black_line[i] += line[i];
      sum += line[i];
#ifdef SAVE_BLACK_CALIBRATION
      printf ("%c", line[i] >> 8);
#endif
    }

  DBG (5,
       "gt68xx_calibrator_add_black_line: line: %2d medium black: 0x%02x\n",
       cal->black_count - 1, sum / width / 256);
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_eval_black (GT68xx_Calibrator * cal, double factor)
{
  SANE_Int i;
  SANE_Int width = cal->width;

  for (i = 0; i < width; ++i)
    {
      cal->black_line[i] = cal->black_line[i] / cal->black_count - factor;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_finish_setup (GT68xx_Calibrator * cal)
{
#ifdef TUNE_CALIBRATOR
  double ave_black = 0.0;
  double ave_diff = 0.0;
#endif /* TUNE_CALIBRATOR */
  int i;
  int width = cal->width;
  unsigned int max_value = 65535;

  for (i = 0; i < width; ++i)
    {
      unsigned int white = cal->white_line[i];
      unsigned int black = cal->black_line[i];
      unsigned int diff = (white > black) ? white - black : 1;
      if (diff > max_value)
	diff = max_value;
      cal->k_white[i] = diff;
      cal->k_black[i] = black;
#ifdef TUNE_CALIBRATOR
      ave_black += black;
      ave_diff += diff;
#endif /* TUNE_CALIBRATOR */
    }

#ifdef TUNE_CALIBRATOR
  ave_black /= width;
  ave_diff /= width;
  DBG (4, "gt68xx_calibrator_finish_setup: ave_black=%f, ave_diff=%f\n",
       ave_black, ave_diff);
#endif /* TUNE_CALIBRATOR */

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_calibrator_process_line (GT68xx_Calibrator * cal, unsigned int *line)
{
  int i;
  int width = cal->width;
  unsigned int white_level = cal->white_level;

  for (i = 0; i < width; ++i)
    {
      unsigned int src_value = line[i];
      unsigned int black = cal->k_black[i];
      unsigned int value;

      if (src_value > black)
	{
	  value = (src_value - black) * white_level / cal->k_white[i];
	  if (value > 0xffff)
	    {
	      value = 0xffff;
#ifdef TUNE_CALIBRATOR
	      cal->max_clip_count++;
#endif /* TUNE_CALIBRATOR */
	    }
	}
      else
	{
	  value = 0;
#ifdef TUNE_CALIBRATOR
	  if (src_value < black)
	    cal->min_clip_count++;
#endif /* TUNE_CALIBRATOR */
	}

      line[i] = value;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_new (GT68xx_Device * dev, GT68xx_Scanner ** scanner_return)
{
  GT68xx_Scanner *scanner;
  int i;

  *scanner_return = NULL;

  scanner = (GT68xx_Scanner *) malloc (sizeof (GT68xx_Scanner));
  if (!scanner)
    {
      DBG (5, "gt68xx_scanner_new: no memory for GT68xx_Scanner\n");
      return SANE_STATUS_NO_MEM;
    }

  scanner->dev = dev;
  scanner->reader = NULL;
  scanner->cal_gray = NULL;
  scanner->cal_r = NULL;
  scanner->cal_g = NULL;
  scanner->cal_b = NULL;

  for(i=0;i<MAX_RESOLUTIONS;i++)
    {
      scanner->calibrations[i].dpi = 0;
      scanner->calibrations[i].red = NULL;
      scanner->calibrations[i].green = NULL;
      scanner->calibrations[i].blue = NULL;
      scanner->calibrations[i].gray = NULL;
    }

  *scanner_return = scanner;
  return SANE_STATUS_GOOD;
}

static void
gt68xx_scanner_free_calibrators (GT68xx_Scanner * scanner)
{
  if (scanner->cal_gray)
    {
      gt68xx_calibrator_free (scanner->cal_gray);
      scanner->cal_gray = NULL;
    }

  if (scanner->cal_r)
    {
      gt68xx_calibrator_free (scanner->cal_r);
      scanner->cal_r = NULL;
    }

  if (scanner->cal_g)
    {
      gt68xx_calibrator_free (scanner->cal_g);
      scanner->cal_g = NULL;
    }

  if (scanner->cal_b)
    {
      gt68xx_calibrator_free (scanner->cal_b);
      scanner->cal_b = NULL;
    }
}

SANE_Status
gt68xx_scanner_free (GT68xx_Scanner * scanner)
{
  int i;

  if (!scanner)
    {
      DBG (5, "gt68xx_scanner_free: scanner==NULL\n");
      return SANE_STATUS_INVAL;
    }

  if (scanner->reader)
    {
      gt68xx_line_reader_free (scanner->reader);
      scanner->reader = NULL;
    }

  gt68xx_scanner_free_calibrators (scanner);

  /* free in memory calibration data */
  for (i = 0; i < MAX_RESOLUTIONS; i++)
    {
      scanner->calibrations[i].dpi = 0;
      if (scanner->calibrations[i].red != NULL)
	{
	  gt68xx_calibrator_free (scanner->calibrations[i].red);
	}
      if (scanner->calibrations[i].green != NULL)
	{
	  gt68xx_calibrator_free (scanner->calibrations[i].green);
	}
      if (scanner->calibrations[i].blue != NULL)
	{
	  gt68xx_calibrator_free (scanner->calibrations[i].blue);
	}
      if (scanner->calibrations[i].gray != NULL)
	{
	  gt68xx_calibrator_free (scanner->calibrations[i].gray);
	}
    }

  free (scanner);

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_wait_for_positioning (GT68xx_Scanner * scanner)
{
  SANE_Status status;
  SANE_Bool moving;
  SANE_Int status_count = 0;

  usleep (100000);		/* needed by the BP 2400 CU Plus? */
  while (SANE_TRUE)
    {
      status = gt68xx_device_is_moving (scanner->dev, &moving);
      if (status == SANE_STATUS_GOOD)
	{
	  if (!moving)
	    break;
	}
      else
	{
	  if (status_count > 9)
	    {
	      DBG (1, "gt68xx_scanner_wait_for_positioning: error count too high!\n");
	      return status;
	    }
	  status_count++;
	  DBG(3, "gt68xx_scanner_wait_for_positioning: ignored error\n");
	}
      usleep (100000);
    }

  return SANE_STATUS_GOOD;
}


static SANE_Status
gt68xx_scanner_internal_start_scan (GT68xx_Scanner * scanner)
{
  SANE_Status status;
  SANE_Bool ready;
  SANE_Int repeat_count;

  status = gt68xx_scanner_wait_for_positioning (scanner);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_internal_start_scan: gt68xx_scanner_wait_for_positioning error: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gt68xx_device_start_scan (scanner->dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_internal_start_scan: gt68xx_device_start_scan error: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for (repeat_count = 0; repeat_count < 30 * 100; ++repeat_count)
    {
      status = gt68xx_device_read_scanned_data (scanner->dev, &ready);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (5,
	       "gt68xx_scanner_internal_start_scan: gt68xx_device_read_scanned_data error: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (ready)
	break;
      usleep (10000);
    }
  if (!ready)
    {
      DBG (5,
	   "gt68xx_scanner_internal_start_scan: scanner still not ready - giving up\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  status = gt68xx_device_read_start (scanner->dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_internal_start_scan: gt68xx_device_read_start error: %s\n",
	   sane_strstatus (status));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_start_scan_extended (GT68xx_Scanner * scanner,
				    GT68xx_Scan_Request * request,
				    GT68xx_Scan_Action action,
				    GT68xx_Scan_Parameters * params)
{
  SANE_Status status;
  GT68xx_AFE_Parameters afe = *scanner->dev->afe;

  status = gt68xx_scanner_wait_for_positioning (scanner);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_start_scan_extended: gt68xx_scanner_wait_for_positioning error: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gt68xx_device_setup_scan (scanner->dev, request, action, params);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_start_scan_extended: gt68xx_device_setup_scan failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gt68xx_line_reader_new (scanner->dev, params,
				   action == SA_SCAN ? SANE_TRUE : SANE_FALSE,
				   &scanner->reader);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_start_scan_extended: gt68xx_line_reader_new failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (scanner->dev->model->is_cis 
      && !((scanner->dev->model->flags & GT68XX_FLAG_SHEET_FED) && scanner->calibrated == SANE_FALSE))
    {
      status =
	gt68xx_device_set_exposure_time (scanner->dev,
					 scanner->dev->exposure);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (5,
	       "gt68xx_scanner_start_scan_extended: gt68xx_device_set_exposure_time failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  status = gt68xx_device_set_afe (scanner->dev, &afe);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_start_scan_extended: gt68xx_device_set_afe failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gt68xx_scanner_internal_start_scan (scanner);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_start_scan_extended: gt68xx_scanner_internal_start_scan failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_create_calibrator (GT68xx_Scan_Parameters * params,
				  GT68xx_Calibrator ** cal_return)
{
  return gt68xx_calibrator_new (params->pixel_xs, 65535, cal_return);
}

static SANE_Status
gt68xx_scanner_create_color_calibrators (GT68xx_Scanner * scanner,
					 GT68xx_Scan_Parameters * params)
{
  SANE_Status status;

  if (!scanner->cal_r)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_r);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  if (!scanner->cal_g)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_g);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  if (!scanner->cal_b)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_b);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_create_gray_calibrators (GT68xx_Scanner * scanner,
					GT68xx_Scan_Parameters * params)
{
  SANE_Status status;

  if (!scanner->cal_gray)
    {
      status = gt68xx_scanner_create_calibrator (params, &scanner->cal_gray);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_color_white_line (GT68xx_Scanner * scanner,
					   unsigned int **buffer_pointers)
{

  gt68xx_calibrator_add_white_line (scanner->cal_r, buffer_pointers[0]);
  gt68xx_calibrator_add_white_line (scanner->cal_g, buffer_pointers[1]);
  gt68xx_calibrator_add_white_line (scanner->cal_b, buffer_pointers[2]);

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_gray_white_line (GT68xx_Scanner * scanner,
					  unsigned int **buffer_pointers)
{
  gt68xx_calibrator_add_white_line (scanner->cal_gray, buffer_pointers[0]);
  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_color_black_line (GT68xx_Scanner * scanner,
					   unsigned int **buffer_pointers)
{
  gt68xx_calibrator_add_black_line (scanner->cal_r, buffer_pointers[0]);
  gt68xx_calibrator_add_black_line (scanner->cal_g, buffer_pointers[1]);
  gt68xx_calibrator_add_black_line (scanner->cal_b, buffer_pointers[2]);

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_scanner_calibrate_gray_black_line (GT68xx_Scanner * scanner,
					  unsigned int **buffer_pointers)
{
  gt68xx_calibrator_add_black_line (scanner->cal_gray, buffer_pointers[0]);
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_calibrate (GT68xx_Scanner * scanner,
			  GT68xx_Scan_Request * request)
{
  SANE_Status status;
  GT68xx_Scan_Parameters params;
  GT68xx_Scan_Request req;
  SANE_Int i;
  unsigned int *buffer_pointers[3];
  GT68xx_AFE_Parameters *afe = scanner->dev->afe;
  GT68xx_Exposure_Parameters *exposure = scanner->dev->exposure;

  memcpy (&req, request, sizeof (req));

  gt68xx_scanner_free_calibrators (scanner);

  if (scanner->auto_afe)
    {
      if (scanner->dev->model->is_cis)
	status = gt68xx_afe_cis_auto (scanner);
      else
	status = gt68xx_afe_ccd_auto (scanner, request);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (5, "gt68xx_scanner_calibrate: gt68xx_afe_*_auto failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      req.mbs = SANE_FALSE;
    }
  else
    req.mbs = SANE_TRUE;

  DBG (3, "afe 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", afe->r_offset,
       afe->r_pga, afe->g_offset, afe->g_pga, afe->b_offset, afe->b_pga);
  DBG (3, "exposure 0x%02x 0x%02x 0x%02x\n", exposure->r_time,
       exposure->g_time, exposure->b_time);

  if (!scanner->calib)
    return SANE_STATUS_GOOD;

  req.mds = SANE_TRUE;
  req.mas = SANE_FALSE;

  if (scanner->dev->model->is_cis && !(scanner->dev->model->flags & GT68XX_FLAG_CIS_LAMP))
    req.color = SANE_TRUE;

  if (req.use_ta)
    {
      req.lamp = SANE_FALSE;
      status =
	gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
    }
  else
    {
      req.lamp = SANE_TRUE;
      status =
	gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);
    }

  status = gt68xx_scanner_start_scan_extended (scanner, &req, SA_CALIBRATE,
					       &params);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_calibrate: gt68xx_scanner_start_scan_extended failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (params.color)
    {
      status = gt68xx_scanner_create_color_calibrators (scanner, &params);
    }
  else
    {
      status = gt68xx_scanner_create_gray_calibrators (scanner, &params);
    }

#if defined(SAVE_WHITE_CALIBRATION) || defined(SAVE_BLACK_CALIBRATION)
  printf ("P5\n%d %d\n255\n", params.pixel_xs, params.pixel_ys * 3);
#endif
  for (i = 0; i < params.pixel_ys; ++i)
    {
      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (5,
	       "gt68xx_scanner_calibrate: gt68xx_line_reader_read failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (params.color)
	status = gt68xx_scanner_calibrate_color_white_line (scanner,
							    buffer_pointers);
      else
	status = gt68xx_scanner_calibrate_gray_white_line (scanner,
							   buffer_pointers);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (5, "gt68xx_scanner_calibrate: calibration failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  gt68xx_scanner_stop_scan (scanner);

  if (params.color)
    {
      gt68xx_calibrator_eval_white (scanner->cal_r, 1);
      gt68xx_calibrator_eval_white (scanner->cal_g, 1);
      gt68xx_calibrator_eval_white (scanner->cal_b, 1);
    }
  else
    {
      gt68xx_calibrator_eval_white (scanner->cal_gray, 1);
    }

  req.mbs = SANE_FALSE;
  req.mds = SANE_FALSE;
  req.mas = SANE_FALSE;
  req.lamp = SANE_FALSE;

  status = gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_calibrate: gt68xx_device_lamp_control failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (!scanner->dev->model->is_cis
      || (scanner->dev->model->flags & GT68XX_FLAG_CIS_LAMP))
    usleep (500000);
  status = gt68xx_scanner_start_scan_extended (scanner, &req, SA_CALIBRATE,
					       &params);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_calibrate: gt68xx_scanner_start_scan_extended failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for (i = 0; i < params.pixel_ys; ++i)
    {
      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (5,
	       "gt68xx_scanner_calibrate: gt68xx_line_reader_read failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (params.color)
	status = gt68xx_scanner_calibrate_color_black_line (scanner,
							    buffer_pointers);
      else
	status = gt68xx_scanner_calibrate_gray_black_line (scanner,
							   buffer_pointers);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (5, "gt68xx_scanner_calibrate: calibration failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  gt68xx_scanner_stop_scan (scanner);

  if (req.use_ta)
    status = gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
  else
    status = gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_calibrate: gt68xx_device_lamp_control failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (!scanner->dev->model->is_cis)
    usleep (500000);

  if (params.color)
    {
      gt68xx_calibrator_eval_black (scanner->cal_r, 0.0);
      gt68xx_calibrator_eval_black (scanner->cal_g, 0.0);
      gt68xx_calibrator_eval_black (scanner->cal_b, 0.0);

      gt68xx_calibrator_finish_setup (scanner->cal_r);
      gt68xx_calibrator_finish_setup (scanner->cal_g);
      gt68xx_calibrator_finish_setup (scanner->cal_b);
    }
  else
    {
      gt68xx_calibrator_eval_black (scanner->cal_gray, 0.0);
      gt68xx_calibrator_finish_setup (scanner->cal_gray);
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_start_scan (GT68xx_Scanner * scanner,
			   GT68xx_Scan_Request * request,
			   GT68xx_Scan_Parameters * params)
{
  request->mbs = SANE_FALSE;	/* don't go home before real scan */
  request->mds = SANE_TRUE;
  request->mas = SANE_FALSE;
  if (request->use_ta)
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
      request->lamp = SANE_FALSE;
    }
  else
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);
      request->lamp = SANE_TRUE;
    }
  if (!scanner->dev->model->is_cis)
    sleep (2);

  return gt68xx_scanner_start_scan_extended (scanner, request, SA_SCAN,
					     params);
}

SANE_Status
gt68xx_scanner_read_line (GT68xx_Scanner * scanner,
			  unsigned int **buffer_pointers)
{
  SANE_Status status;

  status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_scanner_read_line: gt68xx_line_reader_read failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (scanner->calib)
    {
      if (scanner->reader->params.color)
	{
	  gt68xx_calibrator_process_line (scanner->cal_r, buffer_pointers[0]);
	  gt68xx_calibrator_process_line (scanner->cal_g, buffer_pointers[1]);
	  gt68xx_calibrator_process_line (scanner->cal_b, buffer_pointers[2]);
	}
      else
	{
	  if (scanner->dev->model->is_cis && !(scanner->dev->model->flags & GT68XX_FLAG_CIS_LAMP))
	    {
	      if (strcmp
		  (scanner->val[OPT_GRAY_MODE_COLOR].s,
		   GT68XX_COLOR_BLUE) == 0)
		gt68xx_calibrator_process_line (scanner->cal_b,
						buffer_pointers[0]);
	      else
		if (strcmp
		    (scanner->val[OPT_GRAY_MODE_COLOR].s,
		     GT68XX_COLOR_GREEN) == 0)
		gt68xx_calibrator_process_line (scanner->cal_g,
						buffer_pointers[0]);
	      else
		gt68xx_calibrator_process_line (scanner->cal_r,
						buffer_pointers[0]);
	    }
	  else
	    {
	      gt68xx_calibrator_process_line (scanner->cal_gray,
					      buffer_pointers[0]);
	    }
	}
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_scanner_stop_scan (GT68xx_Scanner * scanner)
{
  gt68xx_line_reader_free (scanner->reader);
  scanner->reader = NULL;

  return gt68xx_device_stop_scan (scanner->dev);
}

/************************************************************************/
/*                                                                      */
/* AFE offset/gain automatic configuration                              */
/*                                                                      */
/************************************************************************/

typedef struct GT68xx_Afe_Values GT68xx_Afe_Values;

struct GT68xx_Afe_Values
{
  SANE_Int black;		/* minimum black (0-255) */
  SANE_Int white;		/* maximum white (0-255) */
  SANE_Int total_white;		/* average white of the complete line (0-65536) */
  SANE_Int calwidth;
  SANE_Int callines;
  SANE_Int max_width;
  SANE_Int scan_dpi;
  SANE_Fixed start_black;
  SANE_Int offset_direction;
  SANE_Int coarse_black;
  SANE_Int coarse_white;
};


/************************************************************************/
/* CCD scanners                                                         */
/************************************************************************/

/** Calculate average black and maximum white
 *
 * This function is used for CCD scanners. The black mark to the left ist used
 * for the calculation of average black. The remaining calibration strip    
 * is used for searching the segment whose white average is the highest.
 *
 * @param values AFE values
 * @param buffer scanned line
 */
static void
gt68xx_afe_ccd_calc (GT68xx_Afe_Values * values, unsigned int *buffer)
{
  SANE_Int start_black;
  SANE_Int end_black;
  SANE_Int start_white;
  SANE_Int end_white;
  SANE_Int i;
  SANE_Int max_black = 0;
  SANE_Int min_black = 255;
  SANE_Int max_white = 0;
  SANE_Int total_white = 0;

  /* set size of black mark and white segments */
  start_black =
    SANE_UNFIX (values->start_black) * values->scan_dpi / MM_PER_INCH;
  end_black = start_black + 1.0 * values->scan_dpi / MM_PER_INCH;	/* 1 mm */

  /* 5mm after mark */
  start_white = end_black + 5.0 * values->scan_dpi / MM_PER_INCH;
  end_white = values->calwidth;

  DBG (5,
       "gt68xx_afe_ccd_calc: dpi=%d, start_black=%d, end_black=%d, start_white=%d, end_white=%d\n",
       values->scan_dpi, start_black, end_black, start_white, end_white);

  /* calc min and max black value */
  for (i = start_black; i < end_black; i++)
    {
      if ((SANE_Int) (buffer[i] >> 8) < min_black)
	min_black = (buffer[i] >> 8);
      if ((SANE_Int) (buffer[i] >> 8) > max_black)
	max_black = (buffer[i] >> 8);
#ifdef DEBUG_BLACK
      if ((buffer[i] >> 8) > 15)
	fprintf (stderr, "+");
      else if ((buffer[i] >> 8) < 5)
	fprintf (stderr, "-");
      else
	fprintf (stderr, "_");
#endif
    }
#ifdef DEBUG_BLACK
  fprintf (stderr, "\n");
#endif

  for (i = start_white; i < end_white; ++i)
    {
      if ((SANE_Int) (buffer[i] >> 8) > max_white)
	max_white = (buffer[i] >> 8);
      total_white += buffer[i];
    }
  values->total_white = total_white / (end_white - start_white);

  values->black = min_black;
  values->white = max_white;
  if (values->white < 50 || values->black > 150
      || values->white - values->black < 30 || max_black - min_black > 15)
    DBG (1,
	 "gt68xx_afe_ccd_calc: WARNING: max_white %3d   min_black %3d  max_black %3d\n",
	 values->white, values->black, max_black);
  else
    DBG (5,
	 "gt68xx_afe_ccd_calc: max_white %3d   min_black %3d  max_black %3d\n",
	 values->white, values->black, max_black);
}

static SANE_Bool
gt68xx_afe_ccd_adjust_offset_gain (SANE_String_Const color_name,
				   GT68xx_Afe_Values * values,
				   unsigned int *buffer, SANE_Byte * offset,
				   SANE_Byte * pga, SANE_Byte * old_offset,
				   SANE_Byte * old_pga)
{
  SANE_Int black_low = values->coarse_black, black_high = black_low + 10;
  SANE_Int white_high = values->coarse_white, white_low = white_high - 10;
  SANE_Bool done = SANE_TRUE;
  SANE_Byte local_pga = *pga;
  SANE_Byte local_offset = *offset;

  gt68xx_afe_ccd_calc (values, buffer);

#if 0
  /* test all offset values */
  local_offset++;
  done = SANE_FALSE;
  goto finish;
#endif

  if (values->white > white_high)
    {
      if (values->black > black_high)
	local_offset += values->offset_direction;
      else if (values->black < black_low)
	local_pga--;
      else
	{
	  local_offset += values->offset_direction;
	  local_pga--;
	}
      done = SANE_FALSE;
      goto finish;
    }
  else if (values->white < white_low)
    {
      if (values->black < black_low)
	local_offset -= values->offset_direction;
      else if (values->black > black_high)
	local_pga++;
      else
	{
	  local_offset -= values->offset_direction;
	  local_pga++;
	}
      done = SANE_FALSE;
      goto finish;
    }
  if (values->black > black_high)
    {
      if (values->white > white_high)
	local_offset += values->offset_direction;
      else if (values->white < white_low)
	local_pga++;
      else
	{
	  local_offset += values->offset_direction;
	  local_pga++;
	}
      done = SANE_FALSE;
      goto finish;
    }
  else if (values->black < black_low)
    {
      if (values->white < white_low)
	local_offset -= values->offset_direction;
      else if (values->white > white_high)
	local_pga--;
      else
	{
	  local_offset -= values->offset_direction;
	  local_pga--;
	}
      done = SANE_FALSE;
      goto finish;
    }
finish:
  if ((local_pga == *pga) && (local_offset == *offset))
    done = SANE_TRUE;
  if ((local_pga == *old_pga) && (local_offset == *old_offset))
    done = SANE_TRUE;

  *old_pga = *pga;
  *old_offset = *offset;

  DBG (4, "%5s white=%3d, black=%3d, offset=%2d, gain=%2d, old offs=%2d, "
       "old gain=%2d, total_white=%5d %s\n", color_name, values->white,
       values->black, local_offset, local_pga, *offset, *pga,
       values->total_white, done ? "DONE " : "");

  *pga = local_pga;
  *offset = local_offset;

  return done;
}

/* Wait for lamp to give stable brightness */
static SANE_Status
gt68xx_wait_lamp_stable (GT68xx_Scanner * scanner, 
			 GT68xx_Scan_Parameters * params,
			 GT68xx_Scan_Request *request,
			 unsigned int *buffer_pointers[3],
			 GT68xx_Afe_Values *values,
			 SANE_Bool dont_move)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int last_white = 0;
  SANE_Bool first = SANE_TRUE;
  SANE_Bool message_printed = SANE_FALSE;
  struct timeval now, start_time;
  int secs_lamp_on, secs_start;
  int increase = -5;

  gettimeofday (&start_time, 0);
  do
    {
      usleep (200000);

      if (!first && dont_move)
	{
	  request->mbs = SANE_FALSE;
	  request->mds = SANE_FALSE;
	}
      first = SANE_FALSE;

      /* read line */
      status = gt68xx_scanner_start_scan_extended (scanner, request,
						   SA_CALIBRATE_ONE_LINE,
						   params);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3,
	       "gt68xx_wait_lamp_stable: gt68xx_scanner_start_scan_extended "
	       "failed: %s\n", sane_strstatus (status));
	  return status;
	}
      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3, "gt68xx_wait_lamp_stable: gt68xx_line_reader_read failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      gt68xx_scanner_stop_scan (scanner);

      gt68xx_afe_ccd_calc (values, buffer_pointers[0]);

      DBG (4,
	   "gt68xx_wait_lamp_stable: this white = %d, last white = %d\n",
	   values->total_white, last_white);

      gettimeofday (&now, 0);
      secs_lamp_on = now.tv_sec - scanner->lamp_on_time.tv_sec;
      secs_start = now.tv_sec - start_time.tv_sec;

      if (!message_printed && secs_start > 5)
	{
	  DBG (0, "Please wait for lamp warm-up\n");
	  message_printed = SANE_TRUE;
	}

      if (scanner->val[OPT_AUTO_WARMUP].w == SANE_TRUE)
	{
	  if (scanner->dev->model->flags & GT68XX_FLAG_CIS_LAMP)
	    {
	      if (values->total_white <= (last_white - 20))
		  increase--;
	      if (values->total_white >= last_white)
		  increase++;
	      if (increase > 0 && (values->total_white <= (last_white + 20))
		  && values->total_white != 0)
		break;
	    }
	  else
	    {
	      if ((values->total_white <= (last_white + 20))
		  && values->total_white != 0)
		break;		/* lamp is warmed up */
	    }
	}
      last_white = values->total_white;
    }
  while (secs_lamp_on <= WARMUP_TIME);

  DBG (3, "gt68xx_wait_lamp_stable: Lamp is stable after %d secs (%d secs total)\n",
       secs_start, secs_lamp_on);
  return status;
}

/** Select best AFE gain and offset parameters.
 *
 * This function must be called before the main scan to choose the best values
 * for the AFE gains and offsets.  It performs several one-line scans of the
 * calibration strip.
 *
 * @param scanner Scanner object.
 * @param orig_request Scan parameters.
 *
 * @returns
 * - #SANE_STATUS_GOOD - gain and offset setting completed successfully
 * - other error value - failure of some internal function
 */
static SANE_Status
gt68xx_afe_ccd_auto (GT68xx_Scanner * scanner,
		     GT68xx_Scan_Request * orig_request)
{
  SANE_Status status;
  GT68xx_Scan_Parameters params;
  GT68xx_Scan_Request request;
  int i;
  GT68xx_Afe_Values values;
  unsigned int *buffer_pointers[3];
  GT68xx_AFE_Parameters *afe = scanner->dev->afe, old_afe;
  SANE_Bool gray_done = SANE_FALSE;
  SANE_Bool red_done = SANE_FALSE, green_done = SANE_FALSE, blue_done =
    SANE_FALSE;

  values.offset_direction = 1;
  if (scanner->dev->model->flags & GT68XX_FLAG_OFFSET_INV)
    values.offset_direction = -1;

  memset (&old_afe, 255, sizeof (old_afe));

  request.x0 = SANE_FIX (0.0);
  request.xs = scanner->dev->model->x_size;
  request.xdpi = 300;
  request.ydpi = 300;
  request.depth = 8;
  request.color = orig_request->color;
  /*  request.color = SANE_TRUE; */
  request.mas = SANE_FALSE;
  request.mbs = SANE_FALSE;
  request.mds = SANE_TRUE;
  request.calculate = SANE_FALSE;
  request.use_ta = orig_request->use_ta;

  if (orig_request->use_ta)
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
      request.lamp = SANE_FALSE;
    }
  else
    {
      gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);
      request.lamp = SANE_TRUE;
    }

  /* read line */
  status = gt68xx_scanner_start_scan_extended (scanner, &request,
					       SA_CALIBRATE_ONE_LINE,
					       &params);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3,
	   "gt68xx_afe_ccd_auto: gt68xx_scanner_start_scan_extended failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
  values.scan_dpi = params.xdpi;
  values.calwidth = params.pixel_xs;
  values.max_width =
    (params.pixel_xs * scanner->dev->model->optical_xdpi) / params.xdpi;
  if (orig_request->use_ta)
    values.start_black = SANE_FIX (20.0);
  else
    values.start_black = scanner->dev->model->x_offset_mark;
  values.coarse_black = 1;
  values.coarse_white = 254;

  request.mds = SANE_FALSE;
  DBG (5, "gt68xx_afe_ccd_auto: scan_dpi=%d, calwidth=%d, max_width=%d, "
       "start_black=%.1f mm\n", values.scan_dpi,
       values.calwidth, values.max_width, SANE_UNFIX (values.start_black));

  status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3, "gt68xx_afe_ccd_auto: gt68xx_line_reader_read failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
  gt68xx_scanner_stop_scan (scanner);

  status = gt68xx_wait_lamp_stable (scanner, &params, &request, buffer_pointers,
				    &values, SANE_FALSE);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "gt68xx_afe_ccd_auto: gt68xx_wait_lamp_stable failed %s\n",
	   sane_strstatus (status));
      return status;
    }

  i = 0;
  do
    {
      i++;
      /* read line */
      status = gt68xx_scanner_start_scan_extended (scanner, &request,
						   SA_CALIBRATE_ONE_LINE,
						   &params);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3,
	       "gt68xx_afe_ccd_auto: gt68xx_scanner_start_scan_extended failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3, "gt68xx_afe_ccd_auto: gt68xx_line_reader_read failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (params.color)
	{
	  /* red */
	  if (!red_done)
	    red_done =
	      gt68xx_afe_ccd_adjust_offset_gain ("red", &values,
						 buffer_pointers[0],
						 &afe->r_offset, &afe->r_pga,
						 &old_afe.r_offset,
						 &old_afe.r_pga);
	  /* green */
	  if (!green_done)
	    green_done =
	      gt68xx_afe_ccd_adjust_offset_gain ("green", &values,
						 buffer_pointers[1],
						 &afe->g_offset, &afe->g_pga,
						 &old_afe.g_offset,
						 &old_afe.g_pga);

	  /* blue */
	  if (!blue_done)
	    blue_done =
	      gt68xx_afe_ccd_adjust_offset_gain ("blue", &values,
						 buffer_pointers[2],
						 &afe->b_offset, &afe->b_pga,
						 &old_afe.b_offset,
						 &old_afe.b_pga);
	}
      else
	{
	  if (strcmp (scanner->val[OPT_GRAY_MODE_COLOR].s, GT68XX_COLOR_BLUE)
	      == 0)
	    {
	      gray_done =
		gt68xx_afe_ccd_adjust_offset_gain ("gray", &values,
						   buffer_pointers[0],
						   &afe->b_offset,
						   &afe->b_pga,
						   &old_afe.b_offset,
						   &old_afe.b_pga);
	    }
	  else
	    if (strcmp
		(scanner->val[OPT_GRAY_MODE_COLOR].s,
		 GT68XX_COLOR_GREEN) == 0)
	    {
	      gray_done =
		gt68xx_afe_ccd_adjust_offset_gain ("gray", &values,
						   buffer_pointers[0],
						   &afe->g_offset,
						   &afe->g_pga,
						   &old_afe.g_offset,
						   &old_afe.g_pga);
	      afe->r_offset = afe->b_offset = 0x20;
	      afe->r_pga = afe->b_pga = 0x18;
	    }
	  else
	    {
	      gray_done =
		gt68xx_afe_ccd_adjust_offset_gain ("gray", &values,
						   buffer_pointers[0],
						   &afe->r_offset,
						   &afe->r_pga,
						   &old_afe.r_offset,
						   &old_afe.r_pga);
	    }
	}
      gt68xx_scanner_stop_scan (scanner);
    }
  while (((params.color && (!red_done || !green_done || !blue_done))
	  || (!params.color && !gray_done)) && i < 100);

  return status;
}

/************************************************************************/
/* CIS scanners                                                         */
/************************************************************************/


static void
gt68xx_afe_cis_calc_black (GT68xx_Afe_Values * values,
			   unsigned int *black_buffer)
{
  SANE_Int start_black;
  SANE_Int end_black;
  SANE_Int i, j;
  SANE_Int min_black = 255;
  SANE_Int average = 0;

  start_black = 0;
  end_black = values->calwidth;

  /* find min average black value */
  for (i = start_black; i < end_black; ++i)
    {
      SANE_Int avg_black = 0;
      for (j = 0; j < values->callines; j++)
	avg_black += (*(black_buffer + i + j * values->calwidth) >> 8);
      avg_black /= values->callines;
      average += avg_black;
      if (avg_black < min_black)
	min_black = avg_black;
    }
  values->black = min_black;
  average /= (end_black - start_black);
  DBG (5,
       "gt68xx_afe_cis_calc_black: min_black=0x%02x, average_black=0x%02x\n",
       values->black, average);
}

static void
gt68xx_afe_cis_calc_white (GT68xx_Afe_Values * values,
			   unsigned int *white_buffer)
{
  SANE_Int start_white;
  SANE_Int end_white;
  SANE_Int i, j;
  SANE_Int max_white = 0;

  start_white = 0;
  end_white = values->calwidth;
  values->total_white = 0;

  /* find max average white value */
  for (i = start_white; i < end_white; ++i)
    {
      SANE_Int avg_white = 0;
      for (j = 0; j < values->callines; j++)
	{
	  avg_white += (*(white_buffer + i + j * values->calwidth) >> 8);
	  values->total_white += (*(white_buffer + i + j * values->calwidth));
	}
      avg_white /= values->callines;
      if (avg_white > max_white)
	max_white = avg_white;
    }
  values->white = max_white;
  values->total_white /= (values->callines * (end_white - start_white));
  DBG (5,
       "gt68xx_afe_cis_calc_white: max_white=0x%02x, average_white=0x%02x\n",
       values->white, values->total_white >> 8);
}

static SANE_Bool
gt68xx_afe_cis_adjust_gain_offset (SANE_String_Const color_name,
				   GT68xx_Afe_Values * values,
				   unsigned int *black_buffer,
				   unsigned int *white_buffer,
				   GT68xx_AFE_Parameters * afe,
				   GT68xx_AFE_Parameters * old_afe)
{
  SANE_Byte *offset, *old_offset, *gain, *old_gain;
  SANE_Int o, g;
  SANE_Int black_low = values->coarse_black, black_high = black_low + 10;
  SANE_Int white_high = values->coarse_white, white_low = white_high - 10;
  SANE_Bool done = SANE_TRUE;

  gt68xx_afe_cis_calc_black (values, black_buffer);
  gt68xx_afe_cis_calc_white (values, white_buffer);

  if (strcmp (color_name, "red") == 0)
    {
      offset = &(afe->r_offset);
      old_offset = &old_afe->r_offset;
      gain = &afe->r_pga;
      old_gain = &old_afe->r_pga;
    }
  else if (strcmp (color_name, "green") == 0)
    {
      offset = &afe->g_offset;
      old_offset = &old_afe->g_offset;
      gain = &afe->g_pga;
      old_gain = &old_afe->g_pga;
    }
  else
    {
      offset = &afe->b_offset;
      old_offset = &old_afe->b_offset;
      gain = &afe->b_pga;
      old_gain = &old_afe->b_pga;
    }

  o = *offset;
  g = *gain;

  if (values->white > white_high)
    {
      if (values->black > black_high)
	o -= values->offset_direction;
      else if (values->black < black_low)
	g--;
      else
	{
	  o -= values->offset_direction;
	  g--;
	}
      done = SANE_FALSE;
      goto finish;
    }
  else if (values->white < white_low)
    {
      if (values->black < black_low)
	o += values->offset_direction;
      else if (values->black > black_high)
	g++;
      else
	{
	  o += values->offset_direction;
	  g++;
	}
      done = SANE_FALSE;
      goto finish;
    }
  if (values->black > black_high)
    {
      if (values->white > white_high)
	o -= values->offset_direction;
      else if (values->white < white_low)
	g++;
      else
	{
	  o -= values->offset_direction;
	  g++;
	}
      done = SANE_FALSE;
      goto finish;
    }
  else if (values->black < black_low)
    {
      if (values->white < white_low)
	o += values->offset_direction;
      else if (values->white > white_high)
	g--;
      else
	{
	  o += values->offset_direction;
	  g--;
	}
      done = SANE_FALSE;
      goto finish;
    }
finish:
  if (g < 0)
    g = 0;
  if (g > 48)
    g = 48;
  if (o < 0)
    o = 0;
  if (o > 64)
    o = 64;

  if ((g == *gain) && (o == *offset))
    done = SANE_TRUE;
  if ((g == *old_gain) && (o == *old_offset))
    done = SANE_TRUE;

  *old_gain = *gain;
  *old_offset = *offset;

  DBG (4, "%5s white=%3d, black=%3d, offset=0x%02X, gain=0x%02X, old offs=0x%02X, "
       "old gain=0x%02X, total_white=%5d %s\n", color_name, values->white,
       values->black, o, g, *offset, *gain, values->total_white,
       done ? "DONE " : "");

  *gain = g;
  *offset = o;

  return done;
}


static SANE_Bool
gt68xx_afe_cis_adjust_exposure (SANE_String_Const color_name,
				GT68xx_Afe_Values * values,
				unsigned int *white_buffer, SANE_Int border,
				SANE_Int * exposure_time)
{
  SANE_Int exposure_change = 0;

  gt68xx_afe_cis_calc_white (values, white_buffer);

  if (values->white < border)
    {
      exposure_change = ((border - values->white) * 1);
      (*exposure_time) += exposure_change;
      DBG (4,
	   "%5s: white = %3d, total_white=%5d (exposure too low) --> exposure += %d (=0x%03x)\n",
	   color_name, values->white, values->total_white, exposure_change, *exposure_time);
      return SANE_FALSE;
    }
  else if (values->white > border + 5)
    {
      exposure_change = -((values->white - (border + 5)) * 1);
      (*exposure_time) += exposure_change;
      DBG (4,
	   "%5s: white = %3d, total_white=%5d (exposure too high) --> exposure -= %d (=0x%03x)\n",
	   color_name, values->white, values->total_white, exposure_change, *exposure_time);
      return SANE_FALSE;
    }
  else
    {
      DBG (4, "%5s: white = %3d, total_white=%5d (exposure ok=0x%03x)\n",
	   color_name, values->white, values->total_white, *exposure_time);
    }
  return SANE_TRUE;
}

static SANE_Status
gt68xx_afe_cis_read_lines (GT68xx_Afe_Values * values,
			   GT68xx_Scanner * scanner, SANE_Bool lamp,
			   SANE_Bool first, unsigned int *r_buffer,
			   unsigned int *g_buffer, unsigned int *b_buffer)
{
  SANE_Status status;
  int line;
  unsigned int *buffer_pointers[3];
  GT68xx_Scan_Request request;
  GT68xx_Scan_Parameters params;

  request.x0 = SANE_FIX (0.0);
  request.xs = scanner->dev->model->x_size;
  request.xdpi = 300;
  request.ydpi = 300;
  request.depth = 8;
  request.color = SANE_TRUE;
  request.mas = SANE_FALSE;
  request.calculate = SANE_FALSE;
  request.use_ta = SANE_FALSE;

  if (first)			/* go to start position */
    {
      request.mbs = SANE_TRUE;
      request.mds = SANE_TRUE;
    }
  else
    {
      request.mbs = SANE_FALSE;
      request.mds = SANE_FALSE;
    }
  request.lamp = lamp;

  if (!r_buffer)		/* First, set the size parameters */
    {
      request.calculate = SANE_TRUE;
      RIE (gt68xx_device_setup_scan
	   (scanner->dev, &request, SA_CALIBRATE_ONE_LINE, &params));
      values->scan_dpi = params.xdpi;
      values->calwidth = params.pixel_xs;
      values->callines = params.pixel_ys;
      values->start_black = scanner->dev->model->x_offset_mark;
      return SANE_STATUS_GOOD;
    }

  if (first && (scanner->dev->model->flags & GT68XX_FLAG_CIS_LAMP))
    {
      if (request.use_ta)
	{
	  gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
	  request.lamp = SANE_FALSE;
	}
      else
	{
	  gt68xx_device_lamp_control (scanner->dev, SANE_TRUE, SANE_FALSE);
	  request.lamp = SANE_TRUE;
	}
      status = gt68xx_wait_lamp_stable (scanner, &params, &request,
					buffer_pointers, values, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "gt68xx_afe_cis_read_lines: gt68xx_wait_lamp_stable failed %s\n",
	       sane_strstatus (status));
	  return status;
	}
      request.mbs = SANE_FALSE;
      request.mds = SANE_FALSE;
    }

  status =
    gt68xx_scanner_start_scan_extended (scanner, &request,
					SA_CALIBRATE_ONE_LINE, &params);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_afe_cis_read_lines: gt68xx_scanner_start_scan_extended failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
  values->scan_dpi = params.xdpi;
  values->calwidth = params.pixel_xs;
  values->callines = params.pixel_ys;
  values->coarse_black = 2;
  values->coarse_white = 253;

  if (r_buffer && g_buffer && b_buffer)
    for (line = 0; line < values->callines; line++)
      {
	status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (5,
		 "gt68xx_afe_cis_read_lines: gt68xx_line_reader_read failed: %s\n",
		 sane_strstatus (status));
	    return status;
	  }
	memcpy (r_buffer + values->calwidth * line, buffer_pointers[0],
		values->calwidth * sizeof (unsigned int));
	memcpy (g_buffer + values->calwidth * line, buffer_pointers[1],
		values->calwidth * sizeof (unsigned int));
	memcpy (b_buffer + values->calwidth * line, buffer_pointers[2],
		values->calwidth * sizeof (unsigned int));
      }

  status = gt68xx_scanner_stop_scan (scanner);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "gt68xx_afe_cis_read_lines: gt68xx_scanner_stop_scan failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_afe_cis_auto (GT68xx_Scanner * scanner)
{
  SANE_Status status;
  int total_count, exposure_count;
  GT68xx_Afe_Values values;
  GT68xx_AFE_Parameters *afe = scanner->dev->afe, old_afe;
  GT68xx_Exposure_Parameters *exposure = scanner->dev->exposure;
  SANE_Int red_done, green_done, blue_done;
  SANE_Bool first = SANE_TRUE;
  unsigned int *r_gbuffer = 0, *g_gbuffer = 0, *b_gbuffer = 0;
  unsigned int *r_obuffer = 0, *g_obuffer = 0, *b_obuffer = 0;

  DBG (5, "gt68xx_afe_cis_auto: start\n");

  memset (&old_afe, 255, sizeof (old_afe));

  /* Start with the preset exposure settings */
  memcpy (scanner->dev->exposure, &scanner->dev->model->exposure,
	  sizeof (*scanner->dev->exposure));

  RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_FALSE, SANE_FALSE,
				  r_gbuffer, g_gbuffer, b_gbuffer));

  r_gbuffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  g_gbuffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  b_gbuffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  r_obuffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  g_obuffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  b_obuffer =
    malloc (values.calwidth * values.callines * sizeof (unsigned int));
  if (!r_gbuffer || !g_gbuffer || !b_gbuffer || !r_obuffer || !g_obuffer
      || !b_obuffer)
    return SANE_STATUS_NO_MEM;

  total_count = 0;
  red_done = green_done = blue_done = SANE_FALSE;
  old_afe.r_offset = old_afe.g_offset = old_afe.b_offset = 255;
  old_afe.r_pga = old_afe.g_pga = old_afe.b_pga = 255;
  do
    {
      values.offset_direction = 1;
      if (scanner->dev->model->flags & GT68XX_FLAG_OFFSET_INV)
	values.offset_direction = -1;

      RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_FALSE, first,
				      r_obuffer, g_obuffer, b_obuffer));
      RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_TRUE, SANE_FALSE,
				      r_gbuffer, g_gbuffer, b_gbuffer));

      if (!red_done)
	red_done =
	  gt68xx_afe_cis_adjust_gain_offset ("red", &values, r_obuffer,
					     r_gbuffer, afe, &old_afe);
      if (!green_done)
	green_done =
	  gt68xx_afe_cis_adjust_gain_offset ("green", &values, g_obuffer,
					     g_gbuffer, afe, &old_afe);
      if (!blue_done)
	blue_done =
	  gt68xx_afe_cis_adjust_gain_offset ("blue", &values, b_obuffer,
					     b_gbuffer, afe, &old_afe);
      total_count++;
      first = SANE_FALSE;

    }
  while (total_count < 100 && (!red_done || !green_done || !blue_done));

  if (!red_done || !green_done || !blue_done)
    DBG (0, "gt68xx_afe_cis_auto: setting AFE reached limit\n");

  /* Exposure time */
  exposure_count = 0;
  red_done = green_done = blue_done = SANE_FALSE;
  do
    {
      /* read white line */
      RIE (gt68xx_afe_cis_read_lines (&values, scanner, SANE_TRUE, SANE_FALSE,
				      r_gbuffer, g_gbuffer, b_gbuffer));
      if (!red_done)
	red_done =
	  gt68xx_afe_cis_adjust_exposure ("red", &values, r_gbuffer, 245,
					  &exposure->r_time);
      if (!green_done)
	green_done =
	  gt68xx_afe_cis_adjust_exposure ("green", &values, g_gbuffer, 245,
					  &exposure->g_time);
      if (!blue_done)
	blue_done =
	  gt68xx_afe_cis_adjust_exposure ("blue", &values, b_gbuffer, 245,
					  &exposure->b_time);
      exposure_count++;
      total_count++;
    }
  while ((!red_done || !green_done || !blue_done) && exposure_count < 50);

  if (!red_done || !green_done || !blue_done)
    DBG (0, "gt68xx_afe_cis_auto: setting exposure reached limit\n");

  /* store afe calibration when needed */
  if(scanner->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE)
    {
      memcpy(&(scanner->afe_params), afe, sizeof(GT68xx_AFE_Parameters));
      scanner->exposure_params.r_time=exposure->r_time;
      scanner->exposure_params.g_time=exposure->g_time;
      scanner->exposure_params.b_time=exposure->b_time;
    }

  free (r_gbuffer);
  free (g_gbuffer);
  free (b_gbuffer);
  free (r_obuffer);
  free (g_obuffer);
  free (b_obuffer);
  DBG (4, "gt68xx_afe_cis_auto: total_count: %d\n", total_count);

  return SANE_STATUS_GOOD;
}

/** @brief create and copy calibrator
 * Creates a calibrator of the given width and copy data from reference
 * to initialize it
 * @param calibator pointer to the calibrator to create
 * @param reference calibrator with reference data to copy
 * @param width the width in pixels of the calibrator
 * @param offset offset in pixels when copying data from reference
 * @return SANE_STATUS_GOOD and a filled calibrator if enough memory
 */
static SANE_Status
gt68xx_calibrator_create_copy (GT68xx_Calibrator ** calibrator,
			       GT68xx_Calibrator * reference, int width,
			       int offset)
{
  SANE_Status status;
  int i;

  if (reference == NULL)
    {
      DBG (1, "gt68xx_calibrator_create_copy: NULL reference, skipping...\n");
      *calibrator = NULL;
      return SANE_STATUS_GOOD;
    }
  /* check for reference overflow */
  if(width+offset>reference->width)
    {
      DBG (1, "gt68xx_calibrator_create_copy: required with and offset exceed reference width\n");
      return SANE_STATUS_INVAL;
    }

  status = gt68xx_calibrator_new (width, 65535, calibrator);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1,
	   "gt68xx_calibrator_create_copy: failed to create calibrator: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for(i=0;i<width;i++)
    {
      (*calibrator)->k_white[i]=reference->k_white[i+offset];
      (*calibrator)->k_black[i]=reference->k_black[i+offset];
      (*calibrator)->white_line[i]=reference->white_line[i+offset];
      (*calibrator)->black_line[i]=reference->black_line[i+offset];
    }

  return status;
}
 
static SANE_Status
gt68xx_sheetfed_move_to_scan_area (GT68xx_Scanner * scanner,
				  GT68xx_Scan_Request * request)
{
  SANE_Status status;

  if (!(scanner->dev->model->flags & GT68XX_FLAG_SHEET_FED)
      || scanner->dev->model->command_set->move_paper == NULL)
    return SANE_STATUS_GOOD;

  /* send move paper command */
  RIE (scanner->dev->model->command_set->move_paper (scanner->dev, request));

  /* wait until paper is set to the desired position */
  return gt68xx_scanner_wait_for_positioning (scanner);
}

/**< number of consecutive white line to detect a white area */
#define WHITE_LINES 2

/** @brief calibrate sheet fed scanner
 * This function calibrates sheet fed scanner by scanning a calibration
 * target (which may be a blank page). It first move to a white area then
 * does afe and exposure calibration. Then it scans white lines to get data
 * for shading correction.
 * @param scanner structure describing the frontend session and the device 
 * @return SANE_STATUS_GOOD is everything goes right, SANE_STATUS_INVAL 
 * otherwise.
 */
static SANE_Status
gt68xx_sheetfed_scanner_calibrate (GT68xx_Scanner * scanner)
{
  SANE_Status status;
  GT68xx_Scan_Request request;
  GT68xx_Scan_Parameters params;
  int count, i, x, y, white;
  unsigned int *buffer_pointers[3];
#ifdef DEBUG_CALIBRATION
  FILE *fcal;
  char title[50];
#endif

  DBG (3, "gt68xx_sheetfed_scanner_calibrate: start.\n");

  /* clear calibration if needed */
  gt68xx_scanner_free_calibrators (scanner);
  for (i = 0; i < MAX_RESOLUTIONS; i++)
    {
      if(scanner->calibrations[i].red!=NULL)
        {
          gt68xx_calibrator_free (scanner->calibrations[i].red);
        }
      if(scanner->calibrations[i].green!=NULL)
        {
          gt68xx_calibrator_free (scanner->calibrations[i].green);
        }
      if(scanner->calibrations[i].blue!=NULL)
        {
          gt68xx_calibrator_free (scanner->calibrations[i].blue);
        }
      if(scanner->calibrations[i].gray!=NULL)
        {
          gt68xx_calibrator_free (scanner->calibrations[i].gray);
        }
    }
  scanner->calibrated = SANE_FALSE;

  /* find minimum horizontal resolution */
  request.xdpi = 9600;
  for (i = 0; scanner->dev->model->xdpi_values[i] != 0; i++)
    {
      if (scanner->dev->model->xdpi_values[i] < request.xdpi)
	{
	  request.xdpi = scanner->dev->model->xdpi_values[i];
	  request.ydpi = scanner->dev->model->xdpi_values[i];
	}
    }

  /* move to white area SA_CALIBRATE uses its own y0/ys fixed values */
  request.x0 = 0;
  request.y0 = scanner->dev->model->y_offset_calib;
  request.xs = scanner->dev->model->x_size;
  request.depth = 8;

  request.color = SANE_FALSE;
  request.mbs = SANE_TRUE;
  request.mds = SANE_TRUE;
  request.mas = SANE_FALSE;
  request.lamp = SANE_TRUE;
  request.calculate = SANE_FALSE;
  request.use_ta = SANE_FALSE;
  request.backtrack = SANE_FALSE;
  request.backtrack_lines = 0;

  /* skip start of calibration sheet */
  status = gt68xx_sheetfed_move_to_scan_area (scanner, &request);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1,
	   "gt68xx_sheetfed_scanner_calibrate: failed to skip start of calibration sheet %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gt68xx_device_lamp_control (scanner->dev, SANE_FALSE, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1,
	   "gt68xx_sheetfed_scanner_calibrate: gt68xx_device_lamp_control returned %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* loop until we find a white area to calibrate on */
  i = 0;
  request.y0 = 0;
  do
    {
      /* start scan */
      status =
	gt68xx_scanner_start_scan_extended (scanner, &request, SA_CALIBRATE,
					    &params);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_sheetfed_scanner_calibrate: gt68xx_scanner_start_scan_extended returned %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* loop until we find WHITE_LINES consecutive white lines or we reach and of area */
      white = 0;
      y = 0;
      do
	{
	  status = gt68xx_line_reader_read (scanner->reader, buffer_pointers);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1,
		   "gt68xx_sheetfed_scanner_calibrate: gt68xx_line_reader_read returned %s\n",
		   sane_strstatus (status));
	      gt68xx_scanner_stop_scan (scanner);
	      return status;
	    }

	  /* check for white line */
	  count = 0;
	  for (x = 0; x < params.pixel_xs; x++)
	    {
	      if (((buffer_pointers[0][x] >> 8) & 0xff) > 50)
		{
		  count++;
		}
	    }

	  /* line is white if 93% is above black level */
	  if ((100 * count) / params.pixel_xs < 93)
	    {
	      white = 0;
	    }
	  else
	    {
	      white++;
	    }
	  y++;
	}
      while ((white < WHITE_LINES) && (y < params.pixel_ys));

      /* end scan */
      gt68xx_scanner_stop_scan (scanner);

      i++;
    }
  while (i < 20 && white < WHITE_LINES);

  /* check if we found a white area */
  if (white != WHITE_LINES)
    {
      DBG (1,
	   "gt68xx_sheetfed_scanner_calibrate: didn't find a white area\n");
      return SANE_STATUS_INVAL;
    }

  /* now do calibration */
  scanner->auto_afe = SANE_TRUE;
  scanner->calib = SANE_TRUE;

  /* loop at each possible xdpi to create calibrators */
  i = 0;
  while (scanner->dev->model->xdpi_values[i] > 0)
    {
      request.xdpi = scanner->dev->model->xdpi_values[i];
      request.ydpi = scanner->dev->model->xdpi_values[i];
      request.x0 = 0;
      request.y0 = 0;
      request.xs = scanner->dev->model->x_size;
      request.color = SANE_FALSE;
      request.mbs = SANE_FALSE;
      request.mds = SANE_TRUE;
      request.mas = SANE_FALSE;
      request.lamp = SANE_TRUE;
      request.calculate = SANE_FALSE;
      request.use_ta = SANE_FALSE;
      request.backtrack = SANE_FALSE;
      request.backtrack_lines = 0;

      /* calibrate in color */
      request.color = SANE_TRUE;
      status = gt68xx_scanner_calibrate (scanner, &request);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_sheetfed_scanner_calibrate: gt68xx_scanner_calibrate returned %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* since auto afe is done at a fixed resolution, we don't need to
       * do each each time, once is enough */
      scanner->auto_afe = SANE_FALSE;

      /* allocate and save per dpi calibrators */
      scanner->calibrations[i].dpi = request.xdpi;

      /* recompute params */
      request.calculate = SANE_TRUE;
      gt68xx_device_setup_scan (scanner->dev, &request, SA_SCAN, &params);

      scanner->calibrations[i].pixel_x0 = params.pixel_x0;
      status =
	gt68xx_calibrator_create_copy (&(scanner->calibrations[i].red),
				       scanner->cal_r, scanner->cal_r->width,
				       0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_sheetfed_scanner_calibrate: failed to create red calibrator: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status =
	gt68xx_calibrator_create_copy (&(scanner->calibrations[i].green),
				       scanner->cal_g, scanner->cal_g->width,
				       0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_sheetfed_scanner_calibrate: failed to create green calibrator: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status =
	gt68xx_calibrator_create_copy (&(scanner->calibrations[i].blue),
				       scanner->cal_b, scanner->cal_b->width,
				       0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_sheetfed_scanner_calibrate: failed to create blue calibrator: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* calibrate in gray */
      request.color = SANE_FALSE;
      status = gt68xx_scanner_calibrate (scanner, &request);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_sheetfed_scanner_calibrate: gt68xx_scanner_calibrate returned %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (scanner->cal_gray)
	{
	  status =
	    gt68xx_calibrator_create_copy (&(scanner->calibrations[i].gray),
					   scanner->cal_gray,
					   scanner->cal_gray->width, 0);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1,
		   "gt68xx_sheetfed_scanner_calibrate: failed to create gray calibrator: %s\n",
		   sane_strstatus (status));
	      return status;
	    }
	}

#ifdef DEBUG_CALIBRATION
      sprintf (title, "cal-%03d-red.pnm", scanner->calibrations[i].dpi);
      fcal = fopen (title, "wb");
      if (fcal != NULL)
	{
	  fprintf (fcal, "P5\n%d 1\n255\n", params.pixel_xs);
	  for (x = 0; x < params.pixel_xs; x++)
	    fputc ((scanner->calibrations[i].red->k_white[x] >> 8) & 0xff,
		   fcal);
	  fclose (fcal);
	}
      sprintf (title, "cal-%03d-green.pnm", scanner->calibrations[i].dpi);
      fcal = fopen (title, "wb");
      if (fcal != NULL)
	{
	  fprintf (fcal, "P5\n%d 1\n255\n", params.pixel_xs);
	  for (x = 0; x < params.pixel_xs; x++)
	    fputc ((scanner->calibrations[i].green->k_white[x] >> 8) & 0xff,
		   fcal);
	  fclose (fcal);
	}
      sprintf (title, "cal-%03d-blue.pnm", scanner->calibrations[i].dpi);
      fcal = fopen (title, "wb");
      if (fcal != NULL)
	{
	  fprintf (fcal, "P5\n%d 1\n255\n", params.pixel_xs);
	  for (x = 0; x < params.pixel_xs; x++)
	    fputc ((scanner->calibrations[i].blue->k_white[x] >> 8) & 0xff,
		   fcal);
	  fclose (fcal);
	}
#endif

      /* next resolution */
      i++;
    }

  scanner->calibrated = SANE_TRUE;

  /* eject calibration target from feeder */
  gt68xx_device_paperfeed (scanner->dev);

  /* save calibration to file */
  gt68xx_write_calibration (scanner);

  DBG (3, "gt68xx_sheetfed_scanner_calibrate: end.\n");
  return SANE_STATUS_GOOD;
}

/** @brief assign calibration for scan
 * This function creates the calibrators and set up afe for the requested
 * scan. It uses calibration data that has been created by 
 * gt68xx_sheetfed_scanner_calibrate.
 * @param scanner structure describing the frontend session and the device 
 * @return SANE_STATUS_GOOD is everything goes right, SANE_STATUS_INVAL 
 * otherwise.
 */
static SANE_Status
gt68xx_assign_calibration (GT68xx_Scanner * scanner,
			   GT68xx_Scan_Parameters params)
{
  int i, dpi, offset;
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (3, "gt68xx_assign_calibration: start.\n");

  dpi = params.xdpi;
  DBG (4, "gt68xx_assign_calibration: searching calibration for %d dpi\n",
       dpi);

  /* search matching dpi */
  i = 0;
  while (scanner->calibrations[i].dpi > 0
	 && scanner->calibrations[i].dpi != dpi)
    {
      i++;
    }

  /* check if found a match */
  if (scanner->calibrations[i].dpi == 0)
    {
      DBG (4,
	   "gt68xx_assign_calibration: failed to find calibration for %d dpi\n",
	   dpi);
      return SANE_STATUS_INVAL;
    }
  DBG (4, "gt68xx_assign_calibration: using entry %d for %d dpi\n", i, dpi);

  DBG (5,
       "gt68xx_assign_calibration: using scan_parameters: pixel_x0=%d, pixel_xs=%d \n",
       params.pixel_x0, params.pixel_xs);

  /* AFE/exposure data copy */
  memcpy (scanner->dev->afe, &(scanner->afe_params),
	  sizeof (GT68xx_AFE_Parameters));
  scanner->dev->exposure->r_time = scanner->exposure_params.r_time;
  scanner->dev->exposure->g_time = scanner->exposure_params.g_time;
  scanner->dev->exposure->b_time = scanner->exposure_params.b_time;

  /* free calibrators if needed */
  gt68xx_scanner_free_calibrators (scanner);

  /* TODO compute offset based on the x0 value from scan_request */
  offset = params.pixel_x0 - scanner->calibrations[i].pixel_x0;

  /* calibrator allocation and copy */
  if (scanner->calibrations[i].red!=NULL)
    {
      status =
	gt68xx_calibrator_create_copy (&(scanner->cal_r),
				       scanner->calibrations[i].red,
				       params.pixel_xs,
                                       offset);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_assign_calibration: failed to create calibrator: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  if (scanner->calibrations[i].green!=NULL)
    {
      status =
	gt68xx_calibrator_create_copy (&(scanner->cal_g),
				       scanner->calibrations[i].green,
				       params.pixel_xs,
				       offset);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_assign_calibration: failed to create calibrator: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  if (scanner->calibrations[i].blue!=NULL)
    {
      status =
	gt68xx_calibrator_create_copy (&(scanner->cal_b),
				       scanner->calibrations[i].blue,
				       params.pixel_xs,
				       offset);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_assign_calibration: failed to create calibrator: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  if (scanner->calibrations[i].gray!=NULL)
    {
      status =
	gt68xx_calibrator_create_copy (&(scanner->cal_gray),
				       scanner->calibrations[i].gray,
				       params.pixel_xs,
				       offset);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "gt68xx_assign_calibration: failed to create calibrator: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  DBG (3, "gt68xx_assign_calibration: end.\n");
  return status;
}

static char *gt68xx_calibration_file(GT68xx_Scanner * scanner)
{
  char *ptr=NULL;
  char tmp_str[PATH_MAX];

  ptr=getenv("HOME");
  if(ptr!=NULL)
    {
      sprintf (tmp_str, "%s/.sane/gt68xx-%s.cal", ptr, scanner->dev->model->name);
    }
  else
    {
      ptr=getenv("TMPDIR");
      if(ptr!=NULL)
        {
          sprintf (tmp_str, "%s/gt68xx-%s.cal", ptr, scanner->dev->model->name);
        }
      else
        {
          sprintf (tmp_str, "/tmp/gt68xx-%s.cal", scanner->dev->model->name);
        }
    }
  DBG(5,"gt68xx_calibration_file: using >%s< for calibration file name\n",tmp_str);
  return strdup(tmp_str);
}

static SANE_Status
gt68xx_clear_calibration (GT68xx_Scanner * scanner)
{
  char *fname;
  int i;

  if (scanner->calibrated == SANE_FALSE)
    return SANE_STATUS_GOOD;

  /* clear file */
  fname = gt68xx_calibration_file (scanner);
  unlink (fname);
  free (fname);

  /* free calibrators */
  for (i = 0; i < MAX_RESOLUTIONS && scanner->calibrations[i].dpi > 0; i++)
    {
      scanner->calibrations[i].dpi = 0;
      if (scanner->calibrations[i].red)
	gt68xx_calibrator_free (scanner->calibrations[i].red);
      if (scanner->calibrations[i].green)
	gt68xx_calibrator_free (scanner->calibrations[i].green);
      if (scanner->calibrations[i].blue)
	gt68xx_calibrator_free (scanner->calibrations[i].blue);
      if (scanner->calibrations[i].gray)
	gt68xx_calibrator_free (scanner->calibrations[i].gray);
    }

  /* reset flags */
  scanner->calibrated = SANE_FALSE;
  scanner->val[OPT_QUALITY_CAL].w = SANE_FALSE;
  scanner->val[OPT_NEED_CALIBRATION_SW].w = SANE_TRUE;
  DBG (5, "gt68xx_clear_calibration: done\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_write_calibration (GT68xx_Scanner * scanner)
{
  char *fname;
  FILE *fcal;
  int i;
  SANE_Int nullwidth = 0;

  if (scanner->calibrated == SANE_FALSE)
    {
      return SANE_STATUS_GOOD;
    }

  /* open file */
  fname = gt68xx_calibration_file (scanner);
  fcal = fopen (fname, "wb");
  free (fname);
  if (fcal == NULL)
    {
      DBG (1,
	   "gt68xx_write_calibration: failed to open calibration file for writing %s\n",
	   strerror (errno));
      return SANE_STATUS_IO_ERROR;
    }

  /* TODO we save check endianness and word alignment in case of a home
   * directory used trough different archs */
  fwrite (&(scanner->afe_params), sizeof (GT68xx_AFE_Parameters), 1, fcal);
  fwrite (&(scanner->exposure_params), sizeof (GT68xx_Exposure_Parameters), 1,
	  fcal);
  for (i = 0; i < MAX_RESOLUTIONS && scanner->calibrations[i].dpi > 0; i++)
    {
      DBG (1, "gt68xx_write_calibration: saving %d dpi calibration\n",
	   scanner->calibrations[i].dpi);
      fwrite (&(scanner->calibrations[i].dpi), sizeof (SANE_Int), 1, fcal);
      fwrite (&(scanner->calibrations[i].pixel_x0), sizeof (SANE_Int), 1,
	      fcal);

      fwrite (&(scanner->calibrations[i].red->width), sizeof (SANE_Int), 1,
	      fcal);
      fwrite (&(scanner->calibrations[i].red->white_level), sizeof (SANE_Int),
	      1, fcal);
      fwrite (scanner->calibrations[i].red->k_white, sizeof (unsigned int),
	      scanner->calibrations[i].red->width, fcal);
      fwrite (scanner->calibrations[i].red->k_black, sizeof (unsigned int),
	      scanner->calibrations[i].red->width, fcal);
      fwrite (scanner->calibrations[i].red->white_line, sizeof (double),
	      scanner->calibrations[i].red->width, fcal);
      fwrite (scanner->calibrations[i].red->black_line, sizeof (double),
	      scanner->calibrations[i].red->width, fcal);

      fwrite (&(scanner->calibrations[i].green->width), sizeof (SANE_Int), 1,
	      fcal);
      fwrite (&(scanner->calibrations[i].green->white_level),
	      sizeof (SANE_Int), 1, fcal);
      fwrite (scanner->calibrations[i].green->k_white, sizeof (unsigned int),
	      scanner->calibrations[i].green->width, fcal);
      fwrite (scanner->calibrations[i].green->k_black, sizeof (unsigned int),
	      scanner->calibrations[i].green->width, fcal);
      fwrite (scanner->calibrations[i].green->white_line, sizeof (double),
	      scanner->calibrations[i].green->width, fcal);
      fwrite (scanner->calibrations[i].green->black_line, sizeof (double),
	      scanner->calibrations[i].green->width, fcal);

      fwrite (&(scanner->calibrations[i].blue->width), sizeof (SANE_Int), 1,
	      fcal);
      fwrite (&(scanner->calibrations[i].blue->white_level),
	      sizeof (SANE_Int), 1, fcal);
      fwrite (scanner->calibrations[i].blue->k_white, sizeof (unsigned int),
	      scanner->calibrations[i].blue->width, fcal);
      fwrite (scanner->calibrations[i].blue->k_black, sizeof (unsigned int),
	      scanner->calibrations[i].blue->width, fcal);
      fwrite (scanner->calibrations[i].blue->white_line, sizeof (double),
	      scanner->calibrations[i].blue->width, fcal);
      fwrite (scanner->calibrations[i].blue->black_line, sizeof (double),
	      scanner->calibrations[i].blue->width, fcal);

      if (scanner->calibrations[i].gray != NULL)
	{
	  fwrite (&(scanner->calibrations[i].gray->width), sizeof (SANE_Int),
		  1, fcal);
	  fwrite (&(scanner->calibrations[i].gray->white_level),
		  sizeof (SANE_Int), 1, fcal);
	  fwrite (scanner->calibrations[i].gray->k_white,
		  sizeof (unsigned int), scanner->calibrations[i].gray->width,
		  fcal);
	  fwrite (scanner->calibrations[i].gray->k_black,
		  sizeof (unsigned int), scanner->calibrations[i].gray->width,
		  fcal);
	  fwrite (scanner->calibrations[i].gray->white_line, sizeof (double),
		  scanner->calibrations[i].gray->width, fcal);
	  fwrite (scanner->calibrations[i].gray->black_line, sizeof (double),
		  scanner->calibrations[i].gray->width, fcal);
	}
      else
	{
	  fwrite (&nullwidth, sizeof (SANE_Int), 1, fcal);
	}
    }
  DBG (5, "gt68xx_write_calibration: wrote %d calibrations\n", i);

  fclose (fcal);
  return SANE_STATUS_GOOD;
}

static SANE_Status
gt68xx_read_calibration (GT68xx_Scanner * scanner)
{
  char *fname;
  FILE *fcal;
  int i;
  SANE_Int width, level;

  scanner->calibrated = SANE_FALSE;
  fname = gt68xx_calibration_file (scanner);
  fcal = fopen (fname, "rb");
  free (fname);
  if (fcal == NULL)
    {
      DBG (1,
	   "gt68xx_read_calibration: failed to open calibration file for reading %s\n",
	   strerror (errno));
      return SANE_STATUS_IO_ERROR;
    }

  /* TODO we should check endiannes and word alignment in case of a home
   * directory used trough different archs */

  /* TODO check for errors */
  fread (&(scanner->afe_params), sizeof (GT68xx_AFE_Parameters), 1, fcal);
  fread (&(scanner->exposure_params), sizeof (GT68xx_Exposure_Parameters), 1,
	 fcal);

  /* loop on calibrators */
  i = 0;
  fread (&(scanner->calibrations[i].dpi), sizeof (SANE_Int), 1, fcal);
  while (!feof (fcal) && scanner->calibrations[i].dpi > 0)
    {
      fread (&(scanner->calibrations[i].pixel_x0), sizeof (SANE_Int), 1,
	     fcal);

      fread (&width, sizeof (SANE_Int), 1, fcal);
      fread (&level, sizeof (SANE_Int), 1, fcal);
      gt68xx_calibrator_new (width, level, &(scanner->calibrations[i].red));
      fread (scanner->calibrations[i].red->k_white, sizeof (unsigned int),
	     width, fcal);
      fread (scanner->calibrations[i].red->k_black, sizeof (unsigned int),
	     width, fcal);
      fread (scanner->calibrations[i].red->white_line, sizeof (double), width,
	     fcal);
      fread (scanner->calibrations[i].red->black_line, sizeof (double), width,
	     fcal);

      fread (&width, sizeof (SANE_Int), 1, fcal);
      fread (&level, sizeof (SANE_Int), 1, fcal);
      gt68xx_calibrator_new (width, level, &(scanner->calibrations[i].green));
      fread (scanner->calibrations[i].green->k_white, sizeof (unsigned int),
	     width, fcal);
      fread (scanner->calibrations[i].green->k_black, sizeof (unsigned int),
	     width, fcal);
      fread (scanner->calibrations[i].green->white_line, sizeof (double),
	     width, fcal);
      fread (scanner->calibrations[i].green->black_line, sizeof (double),
	     width, fcal);

      fread (&width, sizeof (SANE_Int), 1, fcal);
      fread (&level, sizeof (SANE_Int), 1, fcal);
      gt68xx_calibrator_new (width, level, &(scanner->calibrations[i].blue));
      fread (scanner->calibrations[i].blue->k_white, sizeof (unsigned int),
	     width, fcal);
      fread (scanner->calibrations[i].blue->k_black, sizeof (unsigned int),
	     width, fcal);
      fread (scanner->calibrations[i].blue->white_line, sizeof (double),
	     width, fcal);
      fread (scanner->calibrations[i].blue->black_line, sizeof (double),
	     width, fcal);

      fread (&width, sizeof (SANE_Int), 1, fcal);
      if (width > 0)
	{
	  fread (&level, sizeof (SANE_Int), 1, fcal);
	  gt68xx_calibrator_new (width, level,
				 &(scanner->calibrations[i].gray));
	  fread (scanner->calibrations[i].gray->k_white,
		 sizeof (unsigned int), width, fcal);
	  fread (scanner->calibrations[i].gray->k_black,
		 sizeof (unsigned int), width, fcal);
	  fread (scanner->calibrations[i].gray->white_line, sizeof (double),
		 width, fcal);
	  fread (scanner->calibrations[i].gray->black_line, sizeof (double),
		 width, fcal);
	}
      /* prepare for nex resolution */
      i++;
      fread (&(scanner->calibrations[i].dpi), sizeof (SANE_Int), 1, fcal);
    }

  DBG (5, "gt68xx_read_calibration: read %d calibrations\n", i);
  fclose (fcal);

  scanner->val[OPT_QUALITY_CAL].w = SANE_TRUE;
  scanner->val[OPT_NEED_CALIBRATION_SW].w = SANE_FALSE;
  scanner->calibrated = SANE_TRUE;
  return SANE_STATUS_GOOD;
}


/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
