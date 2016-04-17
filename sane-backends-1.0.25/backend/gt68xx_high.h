/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2002-2007 Henning Geinitz <sane@geinitz.org>
   
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

#ifndef GT68XX_HIGH_H
#define GT68XX_HIGH_H

#include "gt68xx_mid.h"

typedef struct GT68xx_Calibrator GT68xx_Calibrator;
typedef struct GT68xx_Calibration GT68xx_Calibration;
typedef struct GT68xx_Scanner GT68xx_Scanner;

/** Calibration data for one channel.
 */
struct GT68xx_Calibrator
{
  unsigned int *k_white;	/**< White point vector */
  unsigned int *k_black;	/**< Black point vector */

  double *white_line;		/**< White average */
  double *black_line;		/**< Black average */

  SANE_Int width;		/**< Image width */
  SANE_Int white_level;		/**< Desired white level */

  SANE_Int white_count;		/**< Number of white lines scanned */
  SANE_Int black_count;		/**< Number of black lines scanned */

#ifdef TUNE_CALIBRATOR
  SANE_Int min_clip_count;	 /**< Count of too low values */
  SANE_Int max_clip_count;	 /**< Count of too high values */
#endif				/* TUNE_CALIBRATOR */
};


/** Calibration data for a given resolution
 */
struct GT68xx_Calibration
{ 
  SANE_Int dpi;                 /**< optical horizontal dpi used to
                                  build the calibration data */
  SANE_Int pixel_x0;            /**< x start position used at calibration time */
  
  GT68xx_Calibrator *gray;	    /**< Calibrator for grayscale data */
  GT68xx_Calibrator *red;	    /**< Calibrator for the red channel */
  GT68xx_Calibrator *green;	    /**< Calibrator for the green channel */
  GT68xx_Calibrator *blue;	    /**< Calibrator for the blue channel */
};

/** Create a new calibrator for one (color or mono) channel.
 *
 * @param width       Image width in pixels.
 * @param white_level Desired white level (65535 max).
 * @param cal_return  Returned pointer to the created calibrator object.
 *
 * @return
 * - SANE_STATUS_GOOD   - the calibrator object was created.
 * - SANE_STATUS_INVAL  - invalid parameters.
 * - SANE_STATUS_NO_MEM - not enough memory to create the object.
 */
static SANE_Status
gt68xx_calibrator_new (SANE_Int width,
		       SANE_Int white_level, GT68xx_Calibrator ** cal_return);

/** Destroy the channel calibrator object.
 *
 * @param cal Calibrator object.
 */
static SANE_Status gt68xx_calibrator_free (GT68xx_Calibrator * cal);

/** Add a white calibration line to the calibrator.
 *
 * This function should be called after scanning each white calibration line.
 * The line width must be equal to the value passed to gt68xx_calibrator_new().
 *
 * @param cal  Calibrator object.
 * @param line Pointer to the line data.
 *
 * @return
 * - #SANE_STATUS_GOOD - the line data was processed successfully.
 */
static SANE_Status
gt68xx_calibrator_add_white_line (GT68xx_Calibrator * cal,
				  unsigned int *line);

/** Calculate the white point for the calibrator.
 *
 * This function should be called when all white calibration lines have been
 * scanned.  After doing this, gt68xx_calibrator_add_white_line() should not be
 * called again for this calibrator.
 *
 * @param cal    Calibrator object.
 * @param factor White point correction factor.
 *
 * @return
 * - #SANE_STATUS_GOOD - the white point was calculated successfully.
 */
static SANE_Status
gt68xx_calibrator_eval_white (GT68xx_Calibrator * cal, double factor);

/** Add a black calibration line to the calibrator.
 *
 * This function should be called after scanning each black calibration line.
 * The line width must be equal to the value passed to gt68xx_calibrator_new().
 *
 * @param cal  Calibrator object.
 * @param line Pointer to the line data.
 *
 * @return
 * - #SANE_STATUS_GOOD - the line data was processed successfully.
 */
static SANE_Status
gt68xx_calibrator_add_black_line (GT68xx_Calibrator * cal,
				  unsigned int *line);

/** Calculate the black point for the calibrator.
 *
 * This function should be called when all black calibration lines have been
 * scanned.  After doing this, gt68xx_calibrator_add_black_line() should not be
 * called again for this calibrator.
 *
 * @param cal    Calibrator object.
 * @param factor Black point correction factor.
 *
 * @return
 * - #SANE_STATUS_GOOD - the white point was calculated successfully.
 */
static SANE_Status
gt68xx_calibrator_eval_black (GT68xx_Calibrator * cal, double factor);

/** Finish the calibrator setup and prepare for real scanning.
 *
 * This function must be called after gt68xx_calibrator_eval_white() and
 * gt68xx_calibrator_eval_black().
 *
 * @param cal Calibrator object.
 *
 * @return
 * - #SANE_STATUS_GOOD - the calibrator setup completed successfully.
 */
static SANE_Status gt68xx_calibrator_finish_setup (GT68xx_Calibrator * cal);

/** Process the image line through the calibrator.
 *
 * This function must be called only after gt68xx_calibrator_finish_setup().
 * The image line is modified in place.
 *
 * @param cal  Calibrator object.
 * @param line Pointer to the image line data.
 *
 * @return
 * - #SANE_STATUS_GOOD - the image line was processed successfully.
 */
static SANE_Status
gt68xx_calibrator_process_line (GT68xx_Calibrator * cal, unsigned int *line);

/** List of SANE options
 */
enum GT68xx_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_GRAY_MODE_COLOR,
  OPT_SOURCE,
  OPT_PREVIEW,
  OPT_BIT_DEPTH,
  OPT_RESOLUTION,
  OPT_LAMP_OFF_AT_EXIT,
  OPT_BACKTRACK,

  OPT_DEBUG_GROUP,
  OPT_AUTO_WARMUP,
  OPT_FULL_SCAN,
  OPT_COARSE_CAL,
  OPT_COARSE_CAL_ONCE,
  OPT_QUALITY_CAL,
  OPT_BACKTRACK_LINES,

  OPT_ENHANCEMENT_GROUP,
  OPT_GAMMA_VALUE,
  OPT_THRESHOLD,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_SENSOR_GROUP,
  OPT_NEED_CALIBRATION_SW,      /* signals calibration is needed */
  OPT_PAGE_LOADED_SW,           /* signals that a document is inserted in feeder */

  OPT_BUTTON_GROUP,
  OPT_CALIBRATE,                /* button option to trigger call
                                   to sheetfed calibration */
  OPT_CLEAR_CALIBRATION,        /* clear calibration */

  /* must come last: */
  NUM_OPTIONS
};

/** Scanner object.
 */
struct GT68xx_Scanner
{
  struct GT68xx_Scanner *next;	    /**< Next scanner in list */
  GT68xx_Device *dev;		    /**< Low-level device object */

  GT68xx_Line_Reader *reader;	    /**< Line reader object */

  GT68xx_Calibrator *cal_gray;	    /**< Calibrator for grayscale data */
  GT68xx_Calibrator *cal_r;	    /**< Calibrator for the red channel */
  GT68xx_Calibrator *cal_g;	    /**< Calibrator for the green channel */
  GT68xx_Calibrator *cal_b;	    /**< Calibrator for the blue channel */

  /* SANE data */
  SANE_Bool scanning;			   /**< We are currently scanning */
  SANE_Option_Descriptor opt[NUM_OPTIONS]; /**< Option descriptors */
  Option_Value val[NUM_OPTIONS];	   /**< Option values */
  SANE_Parameters params;		   /**< SANE Parameters */
  SANE_Int line;			   /**< Current line */
  SANE_Int total_bytes;			   /**< Bytes already transmitted */
  SANE_Int byte_count;			   /**< Bytes transmitted in this line */
  SANE_Bool calib;			   /**< Apply calibration data */
  SANE_Bool auto_afe;			   /**< Use automatic gain/offset */
  SANE_Bool first_scan;			   /**< Is this the first scan? */
  struct timeval lamp_on_time;		   /**< Time when the lamp was turned on */
  struct timeval start_time;		   /**< Time when the scan was started */
  SANE_Int bpp_list[5];			   /**< */
  SANE_Int *gamma_table;		   /**< Gray gamma table */
#ifdef DEBUG_BRIGHTNESS
  SANE_Int average_white;	    /**< For debugging brightness problems */
  SANE_Int max_white;
  SANE_Int min_black;
#endif

  /** SANE_TRUE when the scanner has been calibrated */
  SANE_Bool calibrated;

  /** per horizontal resolution calibration data */
  GT68xx_Calibration calibrations[MAX_RESOLUTIONS];

  /* AFE and exposure settings */
  GT68xx_AFE_Parameters afe_params;
  GT68xx_Exposure_Parameters exposure_params;
};


/** Create a new scanner object.
 *
 * @param dev            Low-level device object.
 * @param scanner_return Returned pointer to the created scanner object.
 */
static SANE_Status
gt68xx_scanner_new (GT68xx_Device * dev, GT68xx_Scanner ** scanner_return);

/** Destroy the scanner object.
 *
 * The low-level device object is not destroyed.
 *
 * @param scanner Scanner object.
 */
static SANE_Status gt68xx_scanner_free (GT68xx_Scanner * scanner);

/** Calibrate the scanner before the main scan.
 *
 * @param scanner Scanner object.
 * @param request Scan request data.
 * @param use_autogain Enable automatic offset/gain control
 */
static SANE_Status
gt68xx_scanner_calibrate (GT68xx_Scanner * scanner,
			  GT68xx_Scan_Request * request);

/** Start scanning the image.
 *
 * This function does not perform calibration - it needs to be performed before
 * by calling gt68xx_scanner_calibrate().
 *
 * @param scanner Scanner object.
 * @param request Scan request data.
 * @param params  Returned scan parameters (calculated from the request).
 */
static SANE_Status
gt68xx_scanner_start_scan (GT68xx_Scanner * scanner,
			   GT68xx_Scan_Request * request,
			   GT68xx_Scan_Parameters * params);

/** Read one image line from the scanner.
 *
 * This function can be called only during the scan - after calling
 * gt68xx_scanner_start_scan() and before calling gt68xx_scanner_stop_scan().
 *
 * @param scanner Scanner object.
 * @param buffer_pointers Array of pointers to the image lines.
 */
static SANE_Status
gt68xx_scanner_read_line (GT68xx_Scanner * scanner,
			  unsigned int **buffer_pointers);

/** Stop scanning the image.
 *
 * This function must be called to finish the scan started by
 * gt68xx_scanner_start_scan().  It may be called before all lines are read to
 * cancel the scan prematurely.
 *
 * @param scanner Scanner object.
 */
static SANE_Status gt68xx_scanner_stop_scan (GT68xx_Scanner * scanner);

/** Save calibration data to file
 *
 * This function stores in memory calibration data created at calibration
 * time into file
 * @param scanner Scanner object.
 * @return SANE_STATUS_GOOD when succesfull
 */
static SANE_Status gt68xx_write_calibration (GT68xx_Scanner * scanner);

/** Read calibration data from file
 *
 * This function sets in memory calibration data from data saved into file.
 *
 * @param scanner Scanner object.
 * @return SANE_STATUS_GOOD when succesfull
 */
static SANE_Status gt68xx_read_calibration (GT68xx_Scanner * scanner);

#endif /* not GT68XX_HIGH_H */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
