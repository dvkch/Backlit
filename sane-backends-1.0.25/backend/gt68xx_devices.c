/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2002 - 2007 Henning Geinitz <sane@geinitz.org>
   
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

/* Scanner-specific data */

#include "gt68xx_low.h"
#include "gt68xx_generic.c"
#include "gt68xx_gt6801.c"
#include "gt68xx_gt6816.c"

static GT68xx_Command_Set mustek_gt6816_command_set = {
  "mustek-gt6816",		/* Name of this command set */

  0x40,				/* Request type */
  0x01,				/* Request */

  0x200c,			/* Memory read - wValue */
  0x200b,			/* Memory write - wValue */

  0x2010,			/* Send normal command - wValue */
  0x3f40,			/* Send normal command - wIndex */
  0x2011,			/* Receive normal result - wValue */
  0x3f00,			/* Receive normal result - wIndex */

  0x2012,			/* Send small command - wValue */
  0x3f40,			/* Send small command - wIndex */
  0x2013,			/* Receive small result - wValue */
  0x3f00,			/* Receive small result - wIndex */

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6816_check_firmware,
  gt6816_download_firmware,
  gt6816_get_power_status,
  gt6816_get_ta_status,
  gt6816_lamp_control,
  gt6816_is_moving,
  gt68xx_generic_move_relative,
  gt6816_carriage_home,
  /* gt68xx_generic_paperfeed */ NULL,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6816_stop_scan,
  gt68xx_generic_setup_scan,
  gt68xx_generic_set_afe,
  gt68xx_generic_set_exposure_time,
  gt68xx_generic_get_id,
  /* gt68xx_generic_move_paper */ NULL,
  /* gt6816_document_present */ NULL
};

static GT68xx_Command_Set mustek_gt6816_sheetfed_command_set = {
  "mustek-gt6816",		/* Name of this command set */

  0x40,				/* Request type */
  0x01,				/* Request */

  0x200c,			/* Memory read - wValue */
  0x200b,			/* Memory write - wValue */

  0x2010,			/* Send normal command - wValue */
  0x3f40,			/* Send normal command - wIndex */
  0x2011,			/* Receive normal result - wValue */
  0x3f00,			/* Receive normal result - wIndex */

  0x2012,			/* Send small command - wValue */
  0x3f40,			/* Send small command - wIndex */
  0x2013,			/* Receive small result - wValue */
  0x3f00,			/* Receive small result - wIndex */

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6816_check_firmware,
  gt6816_download_firmware,
  gt6816_get_power_status,
  gt6816_get_ta_status,
  gt6816_lamp_control,
  gt6816_is_moving,
  gt68xx_generic_move_relative,
  /* gt6816_carriage_home */ NULL,
  gt68xx_generic_paperfeed,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6801_stop_scan,
  gt68xx_generic_setup_scan,
  gt68xx_generic_set_afe,
  gt68xx_generic_set_exposure_time,
  gt68xx_generic_get_id,
  gt68xx_generic_move_paper,
  gt6816_document_present
};

static GT68xx_Command_Set mustek_gt6801_command_set = {
  "mustek-gt6801",


  0x40,				/* Request type */
  0x01,				/* Request */

  0x200a,			/* Memory read - wValue */
  0x2009,			/* Memory write - wValue */

  0x2010,			/* Send normal command - wValue */
  0x3f40,			/* Send normal command - wIndex */
  0x2011,			/* Receive normal result - wValue */
  0x3f00,			/* Receive normal result - wIndex */

  0x2012,			/* Send small command - wValue */
  0x3f40,			/* Send small command - wIndex */
  0x2013,			/* Receive small result - wValue */
  0x3f00,			/* Receive small result - wIndex */

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6801_check_firmware,
  gt6801_download_firmware,
  gt6801_get_power_status,
  /* get_ta_status (FIXME: implement this) */ NULL,
  gt6801_lamp_control,
  gt6801_is_moving,
  /* gt68xx_generic_move_relative *** to be tested */ NULL,
  gt6801_carriage_home,
  /* gt68xx_generic_paperfeed */ NULL,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6801_stop_scan,
  gt68xx_generic_setup_scan,
  gt68xx_generic_set_afe,
  gt68xx_generic_set_exposure_time,
  gt68xx_generic_get_id,
  /* gt68xx_generic_move_paper */ NULL,
  /* gt6816_document_present */ NULL
};

static GT68xx_Command_Set plustek_gt6801_command_set = {
  "plustek-gt6801",

  0x40,				/* Request type */
  0x04,				/* Request */

  0x200a,			/* Memory read - wValue */
  0x2009,			/* Memory write - wValue */

  0x2010,			/* Send normal command - wValue */
  0x3f40,			/* Send normal command - wIndex */
  0x2011,			/* Receive normal result - wValue */
  0x3f00,			/* Receive normal result - wIndex */

  0x2012,			/* Send small command - wValue */
  0x3f40,			/* Send small command - wIndex */
  0x2013,			/* Receive small result - wValue */
  0x3f00,			/* Receive small result - wIndex */

  /* activate */ NULL,
  /* deactivate */ NULL,
  gt6801_check_plustek_firmware,
  gt6801_download_firmware,
  gt6801_get_power_status,
  /* get_ta_status (FIXME: implement this) */ NULL,
  gt6801_lamp_control,
  gt6801_is_moving,
  /* gt68xx_generic_move_relative *** to be tested */ NULL,
  gt6801_carriage_home,
  /* gt68xx_generic_paperfeed */ NULL,
  gt68xx_generic_start_scan,
  gt68xx_generic_read_scanned_data,
  gt6801_stop_scan,
  gt68xx_generic_setup_scan,
  gt68xx_generic_set_afe,
  /* set_exposure_time */ NULL,
  gt68xx_generic_get_id,
  /* gt68xx_generic_move_paper */ NULL,
  /* gt6816_document_present */ NULL
};

static GT68xx_Model unknown_model = {
  "unknown-scanner",		/* Name */
  "unknown manufacturer",	/* Device vendor string */
  "unknown device -- use override to select",	/* Device model name */
  "unknown",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {8, 0},			/* possible depths in gray mode */
  {8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (200.0),		/* Size of scan area in mm (x) */
  SANE_FIX (280.0),		/* Size of scan area in mm (y) */

  SANE_FIX (9.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_STOP		/* Which flags are needed for this scanner? */
    /* Standard values for unknown scanner */
};

static GT68xx_Model mustek_2400ta_model = {
  "mustek-bearpaw-2400-ta",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 TA",		/* Device model name */
  "A2fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 200, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 200, 100, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (3.67),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.4),		/* Start of scan area in mm (y) */
  SANE_FIX (219.0),		/* Size of scan area in mm (x) */
  SANE_FIX (298.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.635),		/* Start of black mark in mm (x) */

  SANE_FIX (94.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (107.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (37.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (165.0),		/* Size of scan area in TA mode in mm (y) */
  SANE_FIX (95.0),		/* Start of white strip in TA mode in mm (y) */

  0, 24, 48,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_SCAN_FROM_HOME	/* Which flags are needed for this scanner? */
    /* flatbed values tested */
};

static GT68xx_Model mustek_2400taplus_model = {
  "mustek-bearpaw-2400-ta-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 TA Plus",	/* Device model name */
  "A2Dfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 100, 50, 0},	/* possible y-resolutions */
  {8, 0},			/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (7.41),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.4),		/* Start of scan area in mm (y) */
  SANE_FIX (217.5),		/* Size of scan area in mm (x) */
  SANE_FIX (298.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (5.0),		/* Start of black mark in mm (x) */

  SANE_FIX (94.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (107.0) /* Start of scan area in TA mode in mm (y) */ ,
  SANE_FIX (37.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (165.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (95.0),		/* Start of white strip in TA mode in mm (y) */

  0, 48, 96,			/* RGB CCD Line-distance correction in pixel */
  8,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_USE_OPTICAL_X | GT68XX_FLAG_SCAN_FROM_HOME	/* Which flags are needed for this scanner? */
    /* Setup and tested */
};

static GT68xx_Model mustek_2448taplus_model = {
  "mustek-bearpaw-2448-ta-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2448 TA Plus",	/* Device model name */
  "A2Nfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 200, 100, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 200, 100, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (7.41),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.4),		/* Start of scan area in mm (y) */
  SANE_FIX (216.3),		/* Size of scan area in mm (x) */
  SANE_FIX (297.5),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (4.0),		/* Start of black mark in mm (x) */

  SANE_FIX (94.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (105.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (36.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (165.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (95.0),		/* Start of white strip in TA mode in mm (y) */

  0, 48, 96,			/* RGB CCD Line-distance correction in pixel */
  8,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_STOP		/* Which flags are needed for this scanner? */
    /* Based on data from Jakub Dvo?ák <xdvorak@chello.cz>. */
};

static GT68xx_Model mustek_1200ta_model = {
  "mustek-bearpaw-1200-ta",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 1200 TA",		/* Device model name */
  "A1fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (8.4),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (6.9),		/* Start of black mark in mm (x) */

  SANE_FIX (98.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (108.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (37.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (163.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (95.0),		/* Start of white strip in TA mode in mm (y) */

  32, 16, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x2a, 0x0c, 0x2e, 0x06, 0x2d, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup for 1200 TA */
};

static GT68xx_Model mustek_1200cuplus_model = {
  "mustek-bearpaw-1200-cu-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "Bearpaw 1200 CU Plus",	/* Device model name */
  "PS1Dfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (9.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_STOP		/* Which flags are needed for this scanner? */
    /* Tested by Hamersky Robert r.hamersky at utanet.at */
};

static GT68xx_Model mustek_1200cuplus2_model = {
  "mustek-bearpaw-1200-cu-plus-2",	/* Name */
  "Mustek",			/* Device vendor string */
  "Bearpaw 1200 CU Plus",	/* Device model name */
  "PS1Gfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (4.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (5.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x19, 0x03, 0x1b, 0x05, 0x19, 0x03},	/* Default offset/gain */
  {0xb0, 0xb0, 0xb0},	/* Default exposure parameters */
  SANE_FIX (1.5),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_STOP		/* Which flags are needed for this scanner? */
    /* Tested by hmg */
};

static GT68xx_Model mustek_2400cuplus_model = {
  "mustek-bearpaw-2400-cu-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 CU Plus",	/* Device model name */
  "PS2Dfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 200, 150, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 200, 150, 100, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (2.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (4.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x15, 0x08, 0x12, 0x05, 0x12, 0x05},	/* Default offset/gain */
  {0x2a0, 0x1ab, 0x10d},	/* Default exposure parameters */
  SANE_FIX (1.5),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_STOP		/* Which flags are needed for this scanner? */
    /* Setup and tested */
};

/* Seems that Mustek ScanExpress 1200 UB Plus, the Mustek BearPaw 1200 CU
 * and lots of other scanners have the same USB identifier.
 */
static GT68xx_Model mustek_1200cu_model = {
  "mustek-bearpaw-1200-cu",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 1200 CU",		/* Device model name */
  "PS1fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  600,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.0),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (4.2),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x13, 0x04, 0x15, 0x06, 0x0f, 0x02},	/* Default offset/gain */
  {0x150, 0x150, 0x150},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup and tested */
};

static GT68xx_Model mustek_scanexpress1200ubplus_model = {
  "mustek-scanexpress-1200-ub-plus",	/* Name */
  "Mustek",			/* Device vendor string */
  "ScanExpress 1200 UB Plus",	/* Device model name */
  "SBfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (15.5),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (6.5),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* Setup and tested */
};

static GT68xx_Model mustek_scanexpress1248ub_model = {
  "mustek-scanexpress-1248-ub",	/* Name */
  "Mustek",			/* Device vendor string */
  "ScanExpress 1248 UB",	/* Device model name */
  "SBSfw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (4.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (14.5),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (9.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1e, 0x08, 0x21, 0x0d, 0x1b, 0x05},	/* Default offset/gain */
  {0x50, 0x50, 0x50},	/* Default exposure parameters */
  SANE_FIX (1.5),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_STOP		/* Which flags are needed for this scanner? */
    /* tested by hmg */
};


static GT68xx_Model artec_ultima2000_model = {
  "artec-ultima-2000",		/* Name */
  "Artec",			/* Device vendor string */
  "Ultima 2000",		/* Device model name */
  "Gt680xfw.usb",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 200, 150, 100, 75, 50, 0},	/* possible x-resolutions */
  {600, 300, 200, 150, 100, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (2.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.0),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_MIRROR_X | GT68XX_FLAG_MOTOR_HOME | GT68XX_FLAG_OFFSET_INV	/* Which flags are needed for this scanner? */
    /* Setup for Cytron TCM MD 9385 */
};

static GT68xx_Model mustek_2400cu_model = {
  "mustek-bearpaw-2400-cu",	/* Name */
  "Mustek",			/* Device vendor string */
  "BearPaw 2400 CU",		/* Device model name */
  "PS2fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 150, 100, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.5),		/* Start of scan area in mm (y) */
  SANE_FIX (215.0),		/* Size of scan area in mm (x) */
  SANE_FIX (296.9),		/* Size of scan area in mm (y) */

  SANE_FIX (5.5),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.9),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x0f, 0x01, 0x15, 0x06, 0x13, 0x04},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* basically tested, details may need tweaking */
};

static GT68xx_Model mustek_scanexpress2400usb_model = {
  "mustek-scanexpress-2400-usb",	/* Name */
  "Mustek",			/* Device vendor string */
  "ScanExpress 2400 USB",	/* Device model name */
  "P9fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6801_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (5.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (12.0),		/* Start of scan area in mm (y) */
  SANE_FIX (224.0),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (7.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  24, 12, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x12, 0x06, 0x0e, 0x03, 0x19, 0x25},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_UNTESTED | GT68XX_FLAG_SE_2400	/* Which flags are needed for this scanner? */
    /* only partly tested, from "Fan Dan" <dan_fancn@hotmail.com> */
};

static GT68xx_Model mustek_a3usb_model = {
  "mustek-scanexpress-a3-usb",	/* Name */
  "Mustek",			/* Device vendor string */
  "ScanExpress A3 USB",		/* Device model name */
  "A32fw.usb",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  300,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  300,				/* base x-res used to calculate geometry */
  300,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {300, 150, 75, 50, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (7.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (11.5),		/* Start of scan area in mm (y) */
  SANE_FIX (297.0),		/* Size of scan area in mm (x) */
  SANE_FIX (433.0),		/* Size of scan area in mm (y) */

  SANE_FIX (2.4),		/* Start of white strip in mm (y) */
  SANE_FIX (0),			/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 5, 5,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x14, 0x05, 0x12, 0x05, 0x17, 0x0c},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (1.5),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_CIS_LAMP 		/* Which flags are needed for this scanner? */
    /* Tested by hmg. This scanner is a bit strange as it uses a CIS sensor but
       it also has a lamp. So the lamp needs to be heated but CIS mode must be
       used for scanning and calibration. There is no TA for that scanner */
};

static GT68xx_Model lexmark_x73_model = {
  "lexmark-x73",		/* Name */
  "Lexmark",			/* Device vendor string */
  "X73",			/* Device model name */
  "OSLO3071b2.usb",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 12, 8, 0},		/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (6.519),		/* Start of scan area in mm  (x) */
  SANE_FIX (12.615),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (297.0),		/* Size of scan area in mm (y) */

  SANE_FIX (1.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  32, 16, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x14, 0x07, 0x14, 0x07, 0x14, 0x07},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  0				/* Which flags are needed for this scanner? */
    /* When using automatic gain pictures are too dark. Only some ad hoc tests for
       lexmark x70 were done so far. WARNING: Don't use the Full scan option
       with the above settings, otherwise the sensor may bump at the end of
       the sledge and the scanner may be damaged!  */
};

static GT68xx_Model plustek_op1248u_model = {
  "plustek-op1248u",		/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro 1248U",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &plustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (216.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1e, 0x24, 0x1d, 0x1f, 0x1d, 0x20},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE	/* Which flags are needed for this scanner? */
    /* tested */
};

static GT68xx_Model plustek_u16b_model = {
  "plustek-u16b",		/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro U16B",		/* Device model name */
  "ccd68861.fw",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (5.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.5),		/* Start of scan area in mm (y) */
  SANE_FIX (216.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x18, 0x16, 0x16, 0x0f, 0x17, 0x11},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_NO_POWER_STATUS |
    GT68XX_FLAG_NO_LINEMODE
    /* Which flags are needed for this scanner? */
    /* Tested with a U16B by Henning Meier-Geinitz. 600 dpi is maximum
       vertically. Line mode does not work. That's a hardware/firmware
       issue. */
};

static GT68xx_Model plustek_ops12_model = {
  "plustek-opticpro-s12",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro S12",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE	/* Which flags are needed for this scanner? */
    /* Seems to work */
};

static GT68xx_Model plustek_ops24_model = {
  "plustek-opticpro-s24",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro S24",	/* Device model name */
  "ccd569.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 200, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 200, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (4.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  48, 24, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x18, 0x1c, 0x16, 0x12, 0x18, 0x1c},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE
  				/* Which flags are needed for this scanner? */
    /* Works (tested by Filip Kaluza). Based on genius Colorpage Vivid 1200 X. */
};


static GT68xx_Model genius_vivid4_model = {
  "genius-colorpage-vivid4",	/* Name */
  "Genius",			/* Device vendor string */
  "ColorPage Vivid 4",		/* Device model name */
  "ccd68861.fw",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 12, 8, 0},		/* possible depths in color mode */

  SANE_FIX (5.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.5),		/* Start of scan area in mm (y) */
  SANE_FIX (216.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x18, 0x16, 0x16, 0x0f, 0x17, 0x11},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_NO_POWER_STATUS |
    GT68XX_FLAG_NO_LINEMODE
    /* Which flags are needed for this scanner? */
    /* This scanner seems to be very similar to Plustelk U16B and is reported to work. */
};


static GT68xx_Model genius_vivid3x_model = {
  "genius-colorpage-vivid3x",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid3x",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &plustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE	/* Which flags are needed for this scanner? */
    /* Tested to some degree, based on the Plustek OpticPro 1248U */
};

static GT68xx_Model genius_vivid4x_model = {
  "genius-colorpage-vivid4x",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid4x",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE	/* Which flags are needed for this scanner? */
    /* Is reported to work, copied from 3x, some values from Claudio Filho <filhocf@openoffice.org> */
};

static GT68xx_Model genius_vivid4xe_model = {
  "genius-colorpage-vivid4xe",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid4xe",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE	/* Which flags are needed for this scanner? */
    /* tested a bit */
};

static GT68xx_Model genius_vivid3xe_model = {
  "genius-colorpage-vivid3xe",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid3xe",		/* Device model name */
  "ccd548.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &plustek_gt6801_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  600,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x1c, 0x29, 0x1c, 0x2c, 0x1c, 0x2b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE	/* Which flags are needed for this scanner? */
    /* mostly untested, based on the Genius Vivid3x */
};

static GT68xx_Model genius_vivid1200x_model = {
  "genius-colorpage-vivid-1200-x",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid 1200 X",	/* Device model name */
  "ccd569.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 200, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 200, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (4.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  48, 24, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x18, 0x1c, 0x16, 0x12, 0x18, 0x1c},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE
  				/* Which flags are needed for this scanner? */
    /* Tested. */
};


static GT68xx_Model genius_vivid1200xe_model = {
  "genius-colorpage-vivid-1200-xe",	/* Name */
  "Genius",			/* Device vendor string */
  "Colorpage Vivid 1200 XE",	/* Device model name */
  "ccd569.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_TRUE,			/* Use base_ydpi for all resolutions */

  {600, 300, 200, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 200, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (4.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  48, 24, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x18, 0x1c, 0x16, 0x12, 0x18, 0x1c},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_FALSE,			/* Is this a CIS scanner? */
  GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_ALWAYS_LINEMODE	/* Which flags are needed for this scanner? */
    /* Tested by hmg */
};

static GT68xx_Model iriscan_express_2_model = {
  "iriscan-express-2",	/* Name */
  "Iris",				/* Device vendor string */
  "Iriscan Express 2",			/* Device model name */
  "cism216.fw",				/* Name of the firmware file */
  SANE_FALSE,				/* Dynamic allocation flag */

  &mustek_gt6816_sheetfed_command_set,	/* Command set used by this scanner */

  600,					/* maximum optical sensor resolution */
  1200,					/* maximum motor resolution */
  600,					/* base x-res used to calculate geometry */
  600,					/* base y-res used to calculate geometry */
  1200,   				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  /* values based on analyze of the firmware */
  {600, 400, 300, 200, 150, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 400, 300, 200, 150, 100, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (1.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (18.0),		/* Start of white strip in mm (y) */
  SANE_FIX (21.72),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,				/* RGB CCD Line-distance correction in pixel */
  0,					/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x22, 0x0c, 0x22, 0x0a, 0x24, 0x09},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),			/* Default gamma value */

  SANE_TRUE,				/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_POWER_STATUS | GT68XX_FLAG_SHEET_FED | GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_HAS_CALIBRATE
};


static GT68xx_Model plustek_opticslim_m12_model = {
  "plustek-opticslim-m12",	/* Name */
  "Plustek",				/* Device vendor string */
  "OpticSlim M12",			/* Device model name */
  "cism216.fw",				/* Name of the firmware file */
  SANE_FALSE,				/* Dynamic allocation flag */

  &mustek_gt6816_sheetfed_command_set,	/* Command set used by this scanner */

  600,					/* maximum optical sensor resolution */
  1200,					/* maximum motor resolution */
  600,					/* base x-res used to calculate geometry */
  600,					/* base y-res used to calculate geometry */
  1200,   				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 400, 300, 200, 150, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 400, 300, 200, 150, 100, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (1.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (18.0),		/* Start of white strip in mm (y) */
  SANE_FIX (21.72),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,				/* RGB CCD Line-distance correction in pixel */
  0,					/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x24, 0x0a, 0x23, 0x0f, 0x23, 0x0b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),			/* Default gamma value */

  SANE_TRUE,				/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_POWER_STATUS | GT68XX_FLAG_SHEET_FED | GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_HAS_CALIBRATE
};

static GT68xx_Model genius_sf600_model = {
  "genius-SF600",	/* Name */
  "Genius",				/* Device vendor string */
  "ColorPage SF600",			/* Device model name */
  "cism216.fw",				/* Name of the firmware file */
  SANE_FALSE,				/* Dynamic allocation flag */

  &mustek_gt6816_sheetfed_command_set,	/* Command set used by this scanner */

  600,					/* maximum optical sensor resolution */
  1200,					/* maximum motor resolution */
  600,					/* base x-res used to calculate geometry */
  600,					/* base y-res used to calculate geometry */
  1200,   				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 300, 200, 150, 100, 0},	/* possible x-resolutions */
  {600, 300, 200, 150, 100, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (1.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (18.0),		/* Start of white strip in mm (y) */
  SANE_FIX (21.72),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,				/* RGB CCD Line-distance correction in pixel */
  0,					/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */
  {0x24, 0x0a, 0x23, 0x0f, 0x23, 0x0b},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),			/* Default gamma value */

  SANE_TRUE,				/* Is this a CIS scanner? */
  GT68XX_FLAG_NO_POWER_STATUS | GT68XX_FLAG_UNTESTED | GT68XX_FLAG_SHEET_FED | GT68XX_FLAG_OFFSET_INV | GT68XX_FLAG_HAS_CALIBRATE | GT68XX_FLAG_NO_STOP
};

/* Untested but should work according to Ryan Reading <ryanr23@gmail.com>. Based on Plustek M12 */

static GT68xx_Model plustek_opticslim1200_model = {
  "plustek-opticslim-1200",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticSlim 1200",		/* Device model name */
  "cism216.fw",			/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  600,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  600,				/* base x-res used to calculate geometry */
  600,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {600, 300, 150, 75, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (1.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (9.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (140.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x15, 0x09, 0x18, 0x11, 0x16, 0x0c},	/* Default offset/gain */
  {0x157, 0x157, 0x157},	/* Default exposure parameters */
  SANE_FIX (2.0),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0
};

static GT68xx_Model plustek_opticslim2400_model = {
  "plustek-opticslim-2400",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticSlim 2400",		/* Device model name */
  "cis3R5B1.fw",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  2400,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 150, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (1.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (9.5),		/* Start of scan area in mm (y) */
  SANE_FIX (217.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x15, 0x09, 0x18, 0x11, 0x16, 0x0c},	/* Default offset/gain */
  {0x300, 0x300, 0x300},	/* Default exposure parameters */
  SANE_FIX (1.5),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0
/* By Detlef Gausepohl <detlef@psych.rwth-aachen.de>. Fixed and tested by hmg. */
};

static GT68xx_Model visioneer_onetouch_7300_model = {
  "visioneer-onetouch-7300",	/* Name */
  "Visioneer",			/* Device vendor string */
  "OneTouch 7300",		/* Device model name */
  "Cis3r5b1.fw",		/* Name of the firmware file */
  SANE_FALSE,			/* Dynamic allocation flag */

  &mustek_gt6816_command_set,	/* Command set used by this scanner */

  1200,				/* maximum optical sensor resolution */
  1200,				/* maximum motor resolution */
  1200,				/* base x-res used to calculate geometry */
  1200,				/* base y-res used to calculate geometry */
  1200,				/* if ydpi is equal or higher, disable backtracking */
  SANE_FALSE,			/* Use base_ydpi for all resolutions */

  {1200, 600, 300, 150, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 50, 0},	/* possible y-resolutions */
  {12, 8, 0},			/* possible depths in gray mode */
  {12, 8, 0},			/* possible depths in color mode */

  SANE_FIX (1.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (9.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (140.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */
  0,				/* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */
  {0x15, 0x09, 0x18, 0x11, 0x16, 0x0c},	/* Default offset/gain */
  {0x80, 0x80, 0x80},	/* Default exposure parameters */
  SANE_FIX (1.5),		/* Default gamma value */

  SANE_TRUE,			/* Is this a CIS scanner? */
  0
};

/* Tested by Jason Novek. Based on Plustek OpticSlim 2400. */


static GT68xx_Model genius_colorpageslim_1200_model = {
  "genius-colorpageslim-1200",  /* Name */
  "Genius",                      /* Device vendor string */
  "ColorPage Slim 1200",                /* Device model name */
  "Cis3r5b1.fw",                /* Name of the firmware file */
  SANE_FALSE,                   /* Dynamic allocation flag */

  &mustek_gt6816_command_set,   /* Command set used by this scanner */

   1200,                         /* maximum optical sensor resolution */
   2400,                         /* maximum motor resolution */
   1200,                          /* base x-res used to calculate geometry */
   1200,                         /* base y-res used to calculate geometry */
   1200,                         /* if ydpi is equal or higher, disable backtracking */
   SANE_FALSE,                   /* Use base_ydpi for all resolutions */



  {1200, 600, 300, 150, 100, 50, 0},    /* possible x-resolutions */
  {2400,1200, 600, 300, 150, 100, 50, 0},       /* possible y-resolutions */
  {16, 12, 8, 0},                       /* possible depths in gray mode */
  {16, 12, 8, 0},                       /* possible depths in color mode */

  SANE_FIX (0.5),               /* Start of scan area in mm  (x) */
  SANE_FIX (8.0),               /* Start of scan area in mm (y) */
  SANE_FIX (218.0),             /* Size of scan area in mm (x) */
  SANE_FIX (299.0),             /* Size of scan area in mm (y) */

  SANE_FIX (0.0),               /* Start of white strip in mm (y) */
  SANE_FIX (9.5),               /* Start of black mark in mm (x) */

  SANE_FIX (0.0),               /* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),               /* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.0),               /* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),               /* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),               /* Start of white strip in TA mode in mm (y) */

  0, 0, 0,                      /* RGB CCD Line-distance correction in pixel */
  0,                            /* CCD distcance for CCD with 6 lines) */

  COLOR_ORDER_RGB,              /* Order of the CCD/CIS colors */
  {0x19, 0x1a, 0x18, 0x14, 0x18, 0x12}, /* Default offset/gain */
  {0x548, 0x513, 0x48d},        /* Default exposure parameters */
  SANE_FIX (1.5),               /* Default gamma value */

  SANE_TRUE,                    /* Is this a CIS scanner? */
  GT68XX_FLAG_ALWAYS_LINEMODE | GT68XX_FLAG_SE_2400
};
/* tested by  Aleksey Nedorezov <aleksey at nedorezov.com> */




static GT68xx_USB_Device_Entry gt68xx_usb_device_list[] = {
  {0x10000, 0x10000, &unknown_model},	/* used for yet unknown scanners */
  {0x055f, 0x0218, &mustek_2400ta_model},
  {0x055f, 0x0219, &mustek_2400taplus_model},
  {0x055f, 0x021c, &mustek_1200cuplus_model},
  {0x055f, 0x021b, &mustek_1200cuplus2_model},
  {0x055f, 0x021d, &mustek_2400cuplus_model},
  {0x055f, 0x021e, &mustek_1200ta_model},
  {0x055f, 0x021f, &mustek_scanexpress1248ub_model},
  {0x05d8, 0x4002, &mustek_1200cu_model},
  {0x05d8, 0x4002, &mustek_scanexpress1200ubplus_model},	/* manual override */
  {0x05d8, 0x4002, &artec_ultima2000_model},	/* manual override */
  {0x05d8, 0x4002, &mustek_2400cu_model},	/* manual override */
  {0x05d8, 0x4002, &mustek_scanexpress2400usb_model},	/* manual override */
  {0x055f, 0x0210, &mustek_a3usb_model},
  {0x055f, 0x021a, &mustek_2448taplus_model},
  {0x043d, 0x002d, &lexmark_x73_model},
  {0x07b3, 0x0400, &plustek_op1248u_model},
  {0x07b3, 0x0401, &plustek_op1248u_model},	/* Same scanner, different id? */
  {0x07b3, 0x0402, &plustek_u16b_model},
  {0x07b3, 0x0403, &plustek_u16b_model},	/* two ids? 403 seems to be more common */
  {0x07b3, 0x040b, &plustek_ops12_model},
  {0x07b3, 0x040e, &plustek_ops24_model},
  {0x07b3, 0x0412, &plustek_opticslim_m12_model},
  {0x07b3, 0x0413, &plustek_opticslim1200_model},
  {0x07b3, 0x0422, &plustek_opticslim2400_model},
  {0x07b3, 0x045f, &iriscan_express_2_model},
  {0x0458, 0x2011, &genius_vivid3x_model},
  {0x0458, 0x2014, &genius_vivid4_model},
  {0x0458, 0x2017, &genius_vivid3xe_model},
  {0x0458, 0x201a, &genius_vivid4xe_model},
  {0x0458, 0x201b, &genius_vivid4x_model},
  {0x0458, 0x201d, &genius_vivid1200x_model},
  {0x0458, 0x201f, &genius_vivid1200xe_model},
  {0x0458, 0x2021, &genius_sf600_model},
  {0x04a7, 0x0444, &visioneer_onetouch_7300_model},
  {0x0458, 0x201E, &genius_colorpageslim_1200_model},
  {0, 0, NULL}
};
