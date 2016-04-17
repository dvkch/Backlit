/* sane - Scanner Access Now Easy.

   pieusb_specific.c

   Copyright (C) 2012-2015 Jan Vleeshouwers, Michael Rickmann, Klaus Kaempf

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
   If you do not wish that, delete this exception notice.  */

/* =========================================================================
 *
 * Various Pieusb backend specific functions
 *
 * Option handling, configuration file handling, post-processing
 *
 * ========================================================================= */

#define DEBUG_DECLARE_ONLY
#include "pieusb.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"

#include <errno.h>
#include <math.h>
#include <time.h>

#include "pieusb_usb.h"
#include "pieusb_scancmd.h"
#include "pieusb_buffer.h"
#include "pieusb_specific.h"

/* Pieusb specific */

/* sub to sanei_pieusb_find_device_callback() */
static SANE_Status pieusb_initialize_device_definition (Pieusb_Device_Definition* dev, Pieusb_Scanner_Properties* inq, const char* devicename, SANE_Word vendor_id, SANE_Word product_id);
static void pieusb_print_inquiry (Pieusb_Device_Definition * dev);

/* sub to sane_start() */
static void pieusb_calculate_shading(struct Pieusb_Scanner *scanner, SANE_Byte* buffer);

/* MR */
/* sub to sanei_pieusb_post() */
static SANE_Status pieusb_write_pnm_file (char *filename, uint16_t *data, int depth, int channels, int pixels_per_line, int lines);

/* Auxilary */
static size_t max_string_size (SANE_String_Const const strings[]);
static double getGain(int gain);
static int getGainSetting(double gain);
/*
static void updateGain(Pieusb_Scanner *scanner, int color_index);
*/
static void updateGain2(Pieusb_Scanner *scanner, int color_index, double gain_increase);

/* --------------------------------------------------------------------------
 *
 * SPECIFIC PIEUSB
 *
 * --------------------------------------------------------------------------*/

/* Settings for byte order */
#define SCAN_IMG_FMT_OKLINE          0x08
#define SCAN_IMG_FMT_BLK_ONE         0x04
#define SCAN_IMG_FMT_MOTOROLA        0x02
#define SCAN_IMG_FMT_INTEL           0x01

/* Settings for scanner capabilities */
#define SCAN_CAP_PWRSAV              0x80
#define SCAN_CAP_EXT_CAL             0x40
#define SCAN_CAP_FAST_PREVIEW        0x10
#define SCAN_CAP_DISABLE_CAL         0x08
#define SCAN_CAP_SPEEDS              0x07

/* Available scanner options */
#define SCAN_OPT_DEV_MPCL            0x80
#define SCAN_OPT_DEV_TP1             0x04
#define SCAN_OPT_DEV_TP              0x02
#define SCAN_OPT_DEV_ADF             0x01

/* Options */
#define SANE_NAME_EXPOSURE_R         "exposure-time-r"
#define SANE_TITLE_EXPOSURE_R        "Exposure time red"
#define SANE_DESC_EXPOSURE_R         "The time the red color filter of the CCD is exposed"
#define SANE_NAME_EXPOSURE_G         "exposure-time-g"
#define SANE_TITLE_EXPOSURE_G        "Exposure time green"
#define SANE_DESC_EXPOSURE_G         "The time the green color filter of the CCD is exposed"
#define SANE_NAME_EXPOSURE_B         "exposure-time-b"
#define SANE_TITLE_EXPOSURE_B        "Exposure time blue"
#define SANE_DESC_EXPOSURE_B         "The time the blue color filter of the CCD is exposed"
#define SANE_NAME_EXPOSURE_I         "exposure-time-i"
#define SANE_TITLE_EXPOSURE_I        "Exposure time infrared"
#define SANE_DESC_EXPOSURE_I         "The time the infrared color filter of the CCD is exposed"
#define SANE_EXPOSURE_DEFAULT        DEFAULT_EXPOSURE
#if 1
#define SANE_NAME_GAIN_R               "gain-r"
#define SANE_TITLE_GAIN_R              "Gain red"
#define SANE_DESC_GAIN_R               "The gain of the signal processor for red"
#define SANE_NAME_GAIN_G               "gain-g"
#define SANE_TITLE_GAIN_G              "Gain green"
#define SANE_DESC_GAIN_G               "The gain of the signal processor for green"
#define SANE_NAME_GAIN_B               "gain-b"
#define SANE_TITLE_GAIN_B              "Gain blue"
#define SANE_DESC_GAIN_B               "The gain of the signal processor for blue"
#define SANE_NAME_GAIN_I               "gain-i"
#define SANE_TITLE_GAIN_I              "Gain infrared"
#define SANE_DESC_GAIN_I               "The gain of the signal processor for infrared"
#define SANE_GAIN_DEFAULT            DEFAULT_GAIN

#define SANE_NAME_OFFSET_R             "offset-r"
#define SANE_TITLE_OFFSET_R            "Offset red"
#define SANE_DESC_OFFSET_R             "The offset of the signal processor for red"
#define SANE_NAME_OFFSET_G             "offset-g"
#define SANE_TITLE_OFFSET_G            "Offset greed"
#define SANE_DESC_OFFSET_G             "The offset of the signal processor for green"
#define SANE_NAME_OFFSET_B             "offset-b"
#define SANE_TITLE_OFFSET_B            "Offset blue"
#define SANE_DESC_OFFSET_B             "The offset of the signal processor for blue"
#define SANE_NAME_OFFSET_I             "offset-i"
#define SANE_TITLE_OFFSET_I            "Offset infrared"
#define SANE_DESC_OFFSET_I             "The offset of the signal processor for infrared"
#define SANE_OFFSET_DEFAULT          DEFAULT_OFFSET
#else
#define SANE_NAME_GAIN               "gain"
#define SANE_TITLE_GAIN              "Gain"
#define SANE_DESC_GAIN               "The gain of the signal processor for the 4 CCD color filters (R,G,B,I)"
#define SANE_GAIN_DEFAULT            0x13

#define SANE_NAME_OFFSET             "offset"
#define SANE_TITLE_OFFSET            "Offset"
#define SANE_DESC_OFFSET             "The offset of the signal processor for the 4 CCD color filters (R,G,B,I)"
#define SANE_OFFSET_DEFAULT          0
#endif
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

static const SANE_Range percentage_range_100 = {
  0 << SANE_FIXED_SCALE_SHIFT,	  /* minimum */
  100 << SANE_FIXED_SCALE_SHIFT,  /* maximum */
  0 << SANE_FIXED_SCALE_SHIFT	  /* quantization */
};

/* From the firmware disassembly */
static const SANE_Range gain_range = {
  0,	  /* minimum */
  63,     /* maximum */
  0	  /* quantization */
};

/* From the firmware disassembly */
static const SANE_Range offset_range = {
  0,      /* minimum */
  255,    /* maximum */
  0	  /* quantization */
};

static const double gains[] = {
1.000, 1.075, 1.154, 1.251, 1.362, 1.491, 1.653, /*  0,  5, 10, 15, 20, 25, 30 */
1.858, 2.115, 2.458, 2.935, 3.638, 4.627         /* 35, 40, 45, 50, 55, 60 */
};

/**
 * Callback called whenever a connected USB device reports a supported vendor
 * and product id combination.
 * Used by sane_init() and by sane_open().
 *
 * @param name Device name which has required vendor and product id
 * @return SANE_STATUS_GOOD
 */
SANE_Status
sanei_pieusb_find_device_callback (const char *devicename)
{
    struct Pieusb_Command_Status status;
    SANE_Status r;
    Pieusb_Device_Definition *dev;
    int device_number; /* index in usb devices list maintained by sani_usb */
    Pieusb_Scanner_Properties inq;
    int retry;

    DBG (DBG_info_proc, "sanei_pieusb_find_device_callback: %s\n", devicename);

    /* Check if device is present in the Pieusb device list */
    for (dev = pieusb_definition_list_head; dev; dev = dev->next) {
        if (strcmp (dev->sane.name, devicename) == 0) {
	    return SANE_STATUS_GOOD;
        }
    }

    /* If not, create a new device struct */
    dev = malloc (sizeof (*dev));
    if (!dev) {
        return SANE_STATUS_NO_MEM;
    }

    /* Get device number: index of the device in the sanei_usb devices list */
    r = sanei_usb_open (devicename, &device_number);
    if (r != SANE_STATUS_GOOD) {
        free (dev);
        DBG (DBG_error, "sanei_pieusb_find_device_callback: sanei_usb_open failed for device %s: %s\n",devicename,sane_strstatus(r));
        return r;
    }

    /* Get device properties */

    retry = 2;
    while (retry > 0) {
      retry--;
      /* get inquiry data length */
      sanei_pieusb_cmd_inquiry (device_number, &inq, 5, &status);
      if (status.pieusb_status == PIEUSB_STATUS_GOOD) {
	break;
      }
      else if (status.pieusb_status == PIEUSB_STATUS_IO_ERROR) {
	if (retry > 0) {
	  DBG (DBG_info_proc, "inquiry failed, resetting usb\n");
	  if (sanei_pieusb_usb_reset(device_number) == SANE_STATUS_GOOD) {
	    continue; /* retry after IEEE1284 reset */
	  }
	  if (sanei_usb_reset(device_number) == SANE_STATUS_GOOD) {
	    continue; /* retry after USB reset */
	  }
	}
      }
      free (dev);
      DBG (DBG_error, "sanei_pieusb_find_device_callback: get scanner properties (5 bytes) failed with %d\n", status.pieusb_status);
      sanei_usb_close (device_number);
      return status.pieusb_status;
    }
    /* get full inquiry data */
    sanei_pieusb_cmd_inquiry(device_number, &inq, inq.additionalLength+4, &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
        free (dev);
        DBG (DBG_error, "sanei_pieusb_find_device_callback: get scanner properties failed\n");
        sanei_usb_close (device_number);
        return status.pieusb_status;
    }

    /* Close the device again */
    sanei_usb_close(device_number);

    /* Initialize device definition */
    r = pieusb_initialize_device_definition(dev, &inq, devicename, pieusb_supported_usb_device.vendor, pieusb_supported_usb_device.product);
    if (r != SANE_STATUS_GOOD) {
      return r;
    }

    /* Output */
    pieusb_print_inquiry (dev);

    /* Check model number */
    if (inq.model != pieusb_supported_usb_device.model) {
        free (dev);
        DBG (DBG_error, "sanei_pieusb_find_device_callback: wrong model number %d\n", inq.model);
        return SANE_STATUS_INVAL;
    }

    /* Found a supported scanner, put it in the definitions list*/
    DBG (DBG_info_proc, "sanei_pieusb_find_device_callback: success\n");
    dev->next = pieusb_definition_list_head;
    pieusb_definition_list_head = dev;
    return SANE_STATUS_GOOD;
}

/**
 * Full initialization of a Pieusb_Device structure from INQUIRY data.
 * The function is used in find_device_callback(), so when sane_init() or
 * sane_open() is called.
 *
 * @param dev
 */
static SANE_Status
pieusb_initialize_device_definition (Pieusb_Device_Definition* dev, Pieusb_Scanner_Properties* inq, const char* devicename,
        SANE_Word vendor_id, SANE_Word product_id)
{
    char *pp, *buf;

    /* Initialize device definition */
    dev->next = NULL;
    dev->sane.name = strdup(devicename);

    /* Create 0-terminated string without trailing spaces for vendor */
    buf = malloc(9);
    if (buf == NULL)
      return SANE_STATUS_NO_MEM;
    strncpy(buf, inq->vendor, 8);
    pp = buf + 8;
    *pp-- = '\0';
    while (*pp == ' ') *pp-- = '\0';
    dev->sane.vendor = buf;

    /* Create 0-terminated string without trailing spaces for model */
    buf = malloc(17);
    if (buf == NULL)
      return SANE_STATUS_NO_MEM;
    strncpy(buf, inq->product, 16);
    pp = buf + 16;
    *pp-- = '\0';
    while (*pp == ' ') *pp-- = '\0';
    dev->sane.model = buf;

    dev->sane.type = "film scanner";
    dev->vendorId = vendor_id;
    dev->productId = product_id;

    /* Create 0-terminated strings without trailing spaces for revision */
    buf = malloc(5);
    if (buf == NULL)
      return SANE_STATUS_NO_MEM;
    strncpy(buf, inq->productRevision, 4);
    pp = buf + 4;
    *pp-- = '\0';
    while (*pp == ' ') *pp-- = '\0';
    dev->version = buf;

    dev->model = inq->model;

    /* Maximum resolution values */
    dev->maximum_resolution_x = inq->maxResolutionX;
    dev->maximum_resolution_y = inq->maxResolutionY;
    if (dev->maximum_resolution_y < 256) {
        /* y res is a multiplier */
        dev->maximum_resolution = dev->maximum_resolution_x;
        dev->maximum_resolution_x *= dev->maximum_resolution_y;
        dev->maximum_resolution_y = dev->maximum_resolution_x;
    } else {
      /* y res really is resolution */
      dev->maximum_resolution = min (dev->maximum_resolution_x, dev->maximum_resolution_y);
    }

    /* Geometry */
    dev->scan_bed_width = (double) inq->maxScanWidth / dev->maximum_resolution;
    dev->scan_bed_height = (double) inq->maxScanHeight / dev->maximum_resolution;
    dev->slide_top_left_x = inq->x0;
    dev->slide_top_left_y = inq->y0;
    dev->slide_width = (double) (inq->x1 - inq->x0) / dev->maximum_resolution;
    dev->slide_height = (double) (inq->y1 - inq->y0) / dev->maximum_resolution;

    /* Integer and bit-encoded properties */
    dev->halftone_patterns = inq->halftones & 0x0f;
    dev->color_filters = inq->filters;
    dev->color_depths = inq->colorDepths;
    dev->color_formats = inq->colorFormat;
    dev->image_formats = inq->imageFormat;
    dev->scan_capabilities = inq->scanCapability;
    dev->optional_devices = inq->optionalDevices;
    dev->enhancements = inq->enhancements;
    dev->gamma_bits = inq->gammaBits;
    dev->fast_preview_resolution = inq->previewScanResolution;
    dev->minimum_highlight = inq->minumumHighlight;
    dev->maximum_shadow = inq->maximumShadow;
    dev->calibration_equation = inq->calibrationEquation;
    dev->minimum_exposure = inq->minimumExposure;
    dev->maximum_exposure = inq->maximumExposure*4; /* *4 to solve the strange situation that the default value is out of range */

    dev->x0 = inq->x0;
    dev->y0 = inq->y0;
    dev->x1 = inq->x1;
    dev->y1 = inq->y1;
    dev->production = strndup(inq->production, 4);
    dev->timestamp = strndup(inq->timestamp, 20);
    dev->signature = (char *)strndup((char *)inq->signature, 40);

    /* Ranges for various quantities */
    dev->x_range.min = SANE_FIX (0);
    dev->x_range.quant = SANE_FIX (0);
    dev->x_range.max = SANE_FIX (dev->scan_bed_width * MM_PER_INCH);

    dev->y_range.min = SANE_FIX (0);
    dev->y_range.quant = SANE_FIX (0);
    dev->y_range.max = SANE_FIX (dev->scan_bed_height * MM_PER_INCH);

    dev->dpi_range.min = SANE_FIX (25);
    dev->dpi_range.quant = SANE_FIX (1);
    dev->dpi_range.max = SANE_FIX (max (dev->maximum_resolution_x, dev->maximum_resolution_y));

    dev->shadow_range.min = SANE_FIX (0);
    dev->shadow_range.quant = SANE_FIX (1);
    dev->shadow_range.max = SANE_FIX (dev->maximum_shadow);

    dev->highlight_range.min = SANE_FIX (dev->minimum_highlight);
    dev->highlight_range.quant = SANE_FIX (1);
    dev->highlight_range.max = SANE_FIX (100);

    dev->exposure_range.min = dev->minimum_exposure;
    dev->exposure_range.quant = 1;
    dev->exposure_range.max = dev->maximum_exposure;

    dev->dust_range.min = 0;
    dev->dust_range.quant = 1;
    dev->dust_range.max = 100;

    /* Enumerated ranges vor various quantities */
    /*TODO: create from inq->filters */
    dev->scan_mode_list[0] = SANE_VALUE_SCAN_MODE_LINEART;
    dev->scan_mode_list[1] = SANE_VALUE_SCAN_MODE_HALFTONE;
    dev->scan_mode_list[2] = SANE_VALUE_SCAN_MODE_GRAY;
    dev->scan_mode_list[3] = SANE_VALUE_SCAN_MODE_COLOR;
    dev->scan_mode_list[4] = SANE_VALUE_SCAN_MODE_RGBI;
    dev->scan_mode_list[5] = 0;

    dev->calibration_mode_list[0] = SCAN_CALIBRATION_DEFAULT;
    dev->calibration_mode_list[1] = SCAN_CALIBRATION_AUTO;
    dev->calibration_mode_list[2] = SCAN_CALIBRATION_PREVIEW;
    dev->calibration_mode_list[3] = SCAN_CALIBRATION_OPTIONS;
    dev->calibration_mode_list[4] = 0;

    dev->gain_adjust_list[0] = SCAN_GAIN_ADJUST_03;
    dev->gain_adjust_list[1] = SCAN_GAIN_ADJUST_05;
    dev->gain_adjust_list[2] = SCAN_GAIN_ADJUST_08;
    dev->gain_adjust_list[3] = SCAN_GAIN_ADJUST_10;
    dev->gain_adjust_list[4] = SCAN_GAIN_ADJUST_12;
    dev->gain_adjust_list[5] = SCAN_GAIN_ADJUST_16;
    dev->gain_adjust_list[6] = SCAN_GAIN_ADJUST_19;
    dev->gain_adjust_list[7] = SCAN_GAIN_ADJUST_24;
    dev->gain_adjust_list[8] = SCAN_GAIN_ADJUST_30;
    dev->gain_adjust_list[9] = 0;

    /*TODO: create from inq->colorDepths? Maybe not: didn't experiment with
     * 4 and 12 bit depths. Don;t know how they behave. */
    dev->bpp_list[0] = 3; /* count */
    dev->bpp_list[1] = 1;
    dev->bpp_list[2] = 8;
    dev->bpp_list[3] = 16;

    /* Infrared */
    dev->ir_sw_list[0] = "None";
    dev->ir_sw_list[1] = "Reduce red overlap";
    dev->ir_sw_list[2] = "Remove dirt";
    dev->ir_sw_list[3] = 0;

    dev->grain_sw_list[0] = 4;
    dev->grain_sw_list[1] = 0;
    dev->grain_sw_list[2] = 1;
    dev->grain_sw_list[3] = 2;
    dev->grain_sw_list[4] = 3;
    dev->grain_sw_list[5] = 0;

    dev->crop_sw_list[0] = "None";
    dev->crop_sw_list[1] = "Outside";
    dev->crop_sw_list[2] = "Inside";
    dev->crop_sw_list[3] = 0;

    /* halftone_list */
    dev->halftone_list[0] = "53lpi 45d ROUND"; /* 8x8 pattern */
    dev->halftone_list[1] = "70lpi 45d ROUND"; /* 6x6 pattern */
    dev->halftone_list[2] = "75lpi Hori. Line"; /* 4x4 pattern */
    dev->halftone_list[3] = "4X4 BAYER"; /* 4x4 pattern */
    dev->halftone_list[4] = "4X4 SCROLL"; /* 4x4 pattern */
    dev->halftone_list[5] = "5x5 26 Levels"; /* 5x5 pattern */
    dev->halftone_list[6] = "4x4 SQUARE"; /* 4x4 pattern */
    dev->halftone_list[7] = "5x5 TILE"; /* 5x5 pattern */
    dev->halftone_list[8] = 0;

    return SANE_STATUS_GOOD;
}

/**
 * Output device definition.
 * The function is used in find_device_callback(), so when sane_init() or
 * sane_open() is called.
 *
 * @param dev Device to output
 */
static void
pieusb_print_inquiry (Pieusb_Device_Definition * dev)
{
  DBG (DBG_inquiry, "INQUIRY:\n");
  DBG (DBG_inquiry, "========\n");
  DBG (DBG_inquiry, "\n");
  DBG (DBG_inquiry, "vendor........................: '%s'\n", dev->sane.vendor);
  DBG (DBG_inquiry, "product.......................: '%s'\n", dev->sane.model);
  DBG (DBG_inquiry, "model  .......................: 0x%04x\n", dev->model);
  DBG (DBG_inquiry, "version.......................: '%s'\n", dev->version);

  DBG (DBG_inquiry, "X resolution..................: %d dpi\n",
       dev->maximum_resolution_x);
  DBG (DBG_inquiry, "Y resolution..................: %d dpi\n",
       dev->maximum_resolution_y);
  DBG (DBG_inquiry, "pixel resolution..............: %d dpi\n",
       dev->maximum_resolution);
  DBG (DBG_inquiry, "fb width......................: %f in\n",
       dev->scan_bed_width);
  DBG (DBG_inquiry, "fb length.....................: %f in\n",
       dev->scan_bed_height);

  DBG (DBG_inquiry, "transparency width............: %f in\n",
       dev->slide_width);
  DBG (DBG_inquiry, "transparency length...........: %f in\n",
       dev->slide_height);
  DBG (DBG_inquiry, "transparency offset...........: %d,%d\n",
       dev->slide_top_left_x, dev->slide_top_left_y);

  DBG (DBG_inquiry, "# of halftones................: %d\n",
       dev->halftone_patterns);

  DBG (DBG_inquiry, "One pass color................: %s\n",
       dev->color_filters & SCAN_ONE_PASS_COLOR ? "yes" : "no");

  DBG (DBG_inquiry, "Filters.......................: %s%s%s%s%s (%02x)\n",
       dev->color_filters & SCAN_FILTER_INFRARED ? "Infrared " : "",
       dev->color_filters & SCAN_FILTER_RED ? "Red " : "",
       dev->color_filters & SCAN_FILTER_GREEN ? "Green " : "",
       dev->color_filters & SCAN_FILTER_BLUE ? "Blue " : "",
       dev->color_filters & SCAN_FILTER_NEUTRAL ? "Neutral " : "",
       dev->color_filters);

  DBG (DBG_inquiry, "Color depths..................: %s%s%s%s%s%s (%02x)\n",
       dev->color_depths & SCAN_COLOR_DEPTH_16 ? "16 bit " : "",
       dev->color_depths & SCAN_COLOR_DEPTH_12 ? "12 bit " : "",
       dev->color_depths & SCAN_COLOR_DEPTH_10 ? "10 bit " : "",
       dev->color_depths & SCAN_COLOR_DEPTH_8 ? "8 bit " : "",
       dev->color_depths & SCAN_COLOR_DEPTH_4 ? "4 bit " : "",
       dev->color_depths & SCAN_COLOR_DEPTH_1 ? "1 bit " : "",
       dev->color_depths);

  DBG (DBG_inquiry, "Color Format..................: %s%s%s (%02x)\n",
       dev->color_formats & SCAN_COLOR_FORMAT_INDEX ? "Indexed " : "",
       dev->color_formats & SCAN_COLOR_FORMAT_LINE ? "Line " : "",
       dev->color_formats & SCAN_COLOR_FORMAT_PIXEL ? "Pixel " : "",
       dev->color_formats);

  DBG (DBG_inquiry, "Image Format..................: %s%s%s%s (%02x)\n",
       dev->image_formats & SCAN_IMG_FMT_OKLINE ? "OKLine " : "",
       dev->image_formats & SCAN_IMG_FMT_BLK_ONE ? "BlackOne " : "",
       dev->image_formats & SCAN_IMG_FMT_MOTOROLA ? "Motorola " : "",
       dev->image_formats & SCAN_IMG_FMT_INTEL ? "Intel" : "",
       dev->image_formats);

  DBG (DBG_inquiry,
       "Scan Capability...............: %s%s%s%s%d speeds (%02x)\n",
       dev->scan_capabilities & SCAN_CAP_PWRSAV ? "PowerSave " : "",
       dev->scan_capabilities & SCAN_CAP_EXT_CAL ? "ExtCal " : "",
       dev->scan_capabilities & SCAN_CAP_FAST_PREVIEW ? "FastPreview" :
       "",
       dev->scan_capabilities & SCAN_CAP_DISABLE_CAL ? "DisCal " : "",
       dev->scan_capabilities & SCAN_CAP_SPEEDS,
       dev->scan_capabilities);

  DBG (DBG_inquiry, "Optional Devices..............: %s%s%s%s (%02x)\n",
       dev->optional_devices & SCAN_OPT_DEV_MPCL ? "MultiPageLoad " :
       "",
       dev->optional_devices & SCAN_OPT_DEV_TP1 ? "TransModule1 " : "",
       dev->optional_devices & SCAN_OPT_DEV_TP ? "TransModule " : "",
       dev->optional_devices & SCAN_OPT_DEV_ADF ? "ADF " : "",
       dev->optional_devices);

  DBG (DBG_inquiry, "Enhancement...................: %02x\n",
       dev->enhancements);
  DBG (DBG_inquiry, "Gamma bits....................: %d\n",
       dev->gamma_bits);

  DBG (DBG_inquiry, "Fast Preview Resolution.......: %d\n",
       dev->fast_preview_resolution);
  DBG (DBG_inquiry, "Min Highlight.................: %d\n",
       dev->minimum_highlight);
  DBG (DBG_inquiry, "Max Shadow....................: %d\n",
       dev->maximum_shadow);
  DBG (DBG_inquiry, "Cal Eqn.......................: %d\n",
       dev->calibration_equation);
  DBG (DBG_inquiry, "Min Exposure..................: %d\n",
       dev->minimum_exposure);
  DBG (DBG_inquiry, "Max Exposure..................: %d\n",
       dev->maximum_exposure);

  DBG (DBG_inquiry, "x0,y0 x1,y1...................: %d,%d %d,%d\n",
       dev->x0, dev->y0, dev->x1, dev->y1);
  DBG (DBG_inquiry, "production....................: '%s'\n",
       dev->production);
  DBG (DBG_inquiry, "timestamp.....................: '%s'\n",
       dev->timestamp);
  DBG (DBG_inquiry, "signature.....................: '%s'\n",
       dev->signature);

}

/**
 * Initiaize scanner options from the device definition and from exposure,
 * gain and offset defaults. The function is called by sane_open(), when no
 * optimized settings are available yet. The scanner object is fully
 * initialized in sane_start().
 *
 * @param scanner Scanner to initialize
 * @return SANE_STATUS_GOOD
 */
SANE_Status
sanei_pieusb_init_options (Pieusb_Scanner* scanner)
{
    int i;

    DBG (DBG_info_proc, "sanei_pieusb_init_options\n");

    memset (scanner->opt, 0, sizeof (scanner->opt));
    memset (scanner->val, 0, sizeof (scanner->val));

    for (i = 0; i < NUM_OPTIONS; ++i) {
        scanner->opt[i].size = sizeof (SANE_Word);
        scanner->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

    /* Number of options (a pseudo-option) */
    scanner->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
    scanner->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
    scanner->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
    scanner->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
    scanner->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
    scanner->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

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
    scanner->opt[OPT_MODE].size = max_string_size ((SANE_String_Const const *) scanner->device->scan_mode_list);
    scanner->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    scanner->opt[OPT_MODE].constraint.string_list = (SANE_String_Const const *) scanner->device->scan_mode_list;
    scanner->val[OPT_MODE].s = (SANE_Char *) strdup (scanner->device->scan_mode_list[3]); /* default RGB */

    /* bit depth */
    scanner->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
    scanner->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
    scanner->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
    scanner->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
    scanner->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    scanner->opt[OPT_BIT_DEPTH].size = sizeof (SANE_Word);
    scanner->opt[OPT_BIT_DEPTH].constraint.word_list = scanner->device->bpp_list;
    scanner->val[OPT_BIT_DEPTH].w = scanner->device->bpp_list[2];

    /* resolution */
    scanner->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
    scanner->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
    scanner->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
    scanner->opt[OPT_RESOLUTION].type = SANE_TYPE_FIXED;
    scanner->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
    scanner->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
    scanner->opt[OPT_RESOLUTION].constraint.range = &scanner->device->dpi_range;
    scanner->val[OPT_RESOLUTION].w = scanner->device->fast_preview_resolution << SANE_FIXED_SCALE_SHIFT;

    /* halftone pattern */
    scanner->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
    scanner->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
    scanner->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
    scanner->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
    scanner->opt[OPT_HALFTONE_PATTERN].size = max_string_size ((SANE_String_Const const *) scanner->device->halftone_list);
    scanner->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    scanner->opt[OPT_HALFTONE_PATTERN].constraint.string_list = (SANE_String_Const const *) scanner->device->halftone_list;
    scanner->val[OPT_HALFTONE_PATTERN].s = (SANE_Char *) strdup (scanner->device->halftone_list[6]);
    scanner->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE; /* Not implemented, and only meaningful at depth 1 */

    /* lineart threshold */
    scanner->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
    scanner->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
    scanner->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
    scanner->opt[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
    scanner->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
    scanner->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
    scanner->opt[OPT_THRESHOLD].constraint.range = &percentage_range_100;
    scanner->val[OPT_THRESHOLD].w = SANE_FIX (50);
    /* scanner->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE; Not implemented, and only meaningful at depth 1 */

    /* create a sharper scan at the cost of scan time */
    scanner->opt[OPT_SHARPEN].name = "sharpen";
    scanner->opt[OPT_SHARPEN].title = "Sharpen scan";
    scanner->opt[OPT_SHARPEN].desc = "Sharpen scan by taking more time to discharge the CCD.";
    scanner->opt[OPT_SHARPEN].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_SHARPEN].unit = SANE_UNIT_NONE;
    scanner->opt[OPT_SHARPEN].constraint_type = SANE_CONSTRAINT_NONE;
    scanner->val[OPT_SHARPEN].b = SANE_FALSE;
    scanner->opt[OPT_SHARPEN].cap |= SANE_CAP_SOFT_SELECT;

    /* skip the auto-calibration phase before the scan */
    scanner->opt[OPT_SHADING_ANALYSIS].name = "shading-analysis";
    scanner->opt[OPT_SHADING_ANALYSIS].title = "Perform shading analysis";
    scanner->opt[OPT_SHADING_ANALYSIS].desc = "Collect shading reference data before scanning the image. If set to 'no', this option may be overridden by the scanner.";
    scanner->opt[OPT_SHADING_ANALYSIS].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_SHADING_ANALYSIS].unit = SANE_UNIT_NONE;
    scanner->opt[OPT_SHADING_ANALYSIS].constraint_type = SANE_CONSTRAINT_NONE;
    scanner->val[OPT_SHADING_ANALYSIS].b = SANE_FALSE;
    scanner->opt[OPT_SHADING_ANALYSIS].cap |= SANE_CAP_SOFT_SELECT;

    /* use auto-calibration settings for scan */
    scanner->opt[OPT_CALIBRATION_MODE].name = "calibration";
    scanner->opt[OPT_CALIBRATION_MODE].title = "Calibration mode";
    scanner->opt[OPT_CALIBRATION_MODE].desc = "How to calibrate the scanner.";
    scanner->opt[OPT_CALIBRATION_MODE].type = SANE_TYPE_STRING;
    scanner->opt[OPT_CALIBRATION_MODE].size = max_string_size ((SANE_String_Const const *) scanner->device->calibration_mode_list);
    scanner->opt[OPT_CALIBRATION_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    scanner->opt[OPT_CALIBRATION_MODE].constraint.string_list = (SANE_String_Const const *) scanner->device->calibration_mode_list;
    scanner->val[OPT_CALIBRATION_MODE].s = (SANE_Char *) strdup (scanner->device->calibration_mode_list[1]); /* default auto */

    /* OPT_GAIN_ADJUST */
    scanner->opt[OPT_GAIN_ADJUST].name = "gain-adjust";
    scanner->opt[OPT_GAIN_ADJUST].title = "Adjust gain";
    scanner->opt[OPT_GAIN_ADJUST].desc = "Adjust gain determined by calibration procedure.";
    scanner->opt[OPT_GAIN_ADJUST].type = SANE_TYPE_STRING;
    scanner->opt[OPT_GAIN_ADJUST].size = max_string_size ((SANE_String_Const const *) scanner->device->gain_adjust_list);
    scanner->opt[OPT_GAIN_ADJUST].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    scanner->opt[OPT_GAIN_ADJUST].constraint.string_list = (SANE_String_Const const *) scanner->device->gain_adjust_list;
    scanner->val[OPT_GAIN_ADJUST].s = (SANE_Char *) strdup (scanner->device->gain_adjust_list[2]); /* x 1.0 (no change) */

    /* scan infrared channel faster but less accurate */
    scanner->opt[OPT_FAST_INFRARED].name = "fast-infrared";
    scanner->opt[OPT_FAST_INFRARED].title = "Fast infrared scan";
    scanner->opt[OPT_FAST_INFRARED].desc = "Do not reposition scan head before scanning infrared line. Results in an infrared offset which may deteriorate IR dust and scratch removal.";
    scanner->opt[OPT_FAST_INFRARED].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_FAST_INFRARED].unit = SANE_UNIT_NONE;
    scanner->opt[OPT_FAST_INFRARED].constraint_type = SANE_CONSTRAINT_NONE;
    scanner->val[OPT_FAST_INFRARED].b = SANE_FALSE;
    scanner->opt[OPT_FAST_INFRARED].cap |= SANE_CAP_SOFT_SELECT;

    /* automatically advance to next slide after scan */
    scanner->opt[OPT_ADVANCE_SLIDE].name = "advcane";
    scanner->opt[OPT_ADVANCE_SLIDE].title = "Advance slide";
    scanner->opt[OPT_ADVANCE_SLIDE].desc = "Automatically advance to next slide after scan";
    scanner->opt[OPT_ADVANCE_SLIDE].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_ADVANCE_SLIDE].unit = SANE_UNIT_NONE;
    scanner->opt[OPT_ADVANCE_SLIDE].constraint_type = SANE_CONSTRAINT_NONE;
    scanner->val[OPT_ADVANCE_SLIDE].w = SANE_TRUE;
    scanner->opt[OPT_ADVANCE_SLIDE].cap |= SANE_CAP_SOFT_SELECT;

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
    scanner->opt[OPT_TL_X].constraint.range = &(scanner->device->x_range);
    scanner->val[OPT_TL_X].w = 0;

    /* top-left y */
    scanner->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
    scanner->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
    scanner->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
    scanner->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
    scanner->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
    scanner->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
    scanner->opt[OPT_TL_Y].constraint.range = &(scanner->device->y_range);
    scanner->val[OPT_TL_Y].w = 0;

    /* bottom-right x */
    scanner->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
    scanner->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
    scanner->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
    scanner->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
    scanner->opt[OPT_BR_X].unit = SANE_UNIT_MM;
    scanner->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
    scanner->opt[OPT_BR_X].constraint.range = &(scanner->device->x_range);
    scanner->val[OPT_BR_X].w = scanner->device->x_range.max;

    /* bottom-right y */
    scanner->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
    scanner->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
    scanner->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
    scanner->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
    scanner->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
    scanner->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
    scanner->opt[OPT_BR_Y].constraint.range = &(scanner->device->y_range);
    scanner->val[OPT_BR_Y].w = scanner->device->y_range.max;

    /* "Enhancement" group: */
    scanner->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
    scanner->opt[OPT_ENHANCEMENT_GROUP].desc = "";
    scanner->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
    scanner->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
    scanner->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    /* correct data for lamp variations (shading) */
    scanner->opt[OPT_CORRECT_SHADING].name = "correct-shading";
    scanner->opt[OPT_CORRECT_SHADING].title = "Correct shading";
    scanner->opt[OPT_CORRECT_SHADING].desc = "Correct data for lamp variations (shading)";
    scanner->opt[OPT_CORRECT_SHADING].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_CORRECT_SHADING].unit = SANE_UNIT_NONE;
    scanner->val[OPT_CORRECT_SHADING].w = SANE_TRUE;

    /* correct infrared for red crosstalk */
    scanner->opt[OPT_CORRECT_INFRARED].name = "correct-infrared";
    scanner->opt[OPT_CORRECT_INFRARED].title = "Correct infrared";
    scanner->opt[OPT_CORRECT_INFRARED].desc = "Correct infrared for red crosstalk";
    scanner->opt[OPT_CORRECT_INFRARED].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_CORRECT_INFRARED].unit = SANE_UNIT_NONE;
    scanner->val[OPT_CORRECT_INFRARED].w = SANE_FALSE;

    /* detect and remove dust and scratch artifacts */
    scanner->opt[OPT_CLEAN_IMAGE].name = "clean-image";
    scanner->opt[OPT_CLEAN_IMAGE].title = "Clean image";
    scanner->opt[OPT_CLEAN_IMAGE].desc = "Detect and remove dust and scratch artifacts";
    scanner->opt[OPT_CLEAN_IMAGE].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_CLEAN_IMAGE].unit = SANE_UNIT_NONE;
    scanner->val[OPT_CLEAN_IMAGE].w = SANE_FALSE;

    /* strength of grain filtering */
    scanner->opt[OPT_SMOOTH_IMAGE].name = "smooth";
    scanner->opt[OPT_SMOOTH_IMAGE].title = "Attenuate film grain";
    scanner->opt[OPT_SMOOTH_IMAGE].desc = "Amount of smoothening";
    scanner->opt[OPT_SMOOTH_IMAGE].type = SANE_TYPE_INT;
    scanner->opt[OPT_SMOOTH_IMAGE].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    scanner->opt[OPT_SMOOTH_IMAGE].size = sizeof (SANE_Word);
    scanner->opt[OPT_SMOOTH_IMAGE].constraint.word_list = scanner->device->grain_sw_list;
    scanner->val[OPT_SMOOTH_IMAGE].w = scanner->device->grain_sw_list[1];
    if (scanner->opt[OPT_SMOOTH_IMAGE].constraint.word_list[0] < 2) {
        scanner->opt[OPT_SMOOTH_IMAGE].cap |= SANE_CAP_INACTIVE;
    }

    /* gamma correction, to make image sRGB like */
    scanner->opt[OPT_TRANSFORM_TO_SRGB].name = "srgb";
    scanner->opt[OPT_TRANSFORM_TO_SRGB].title = "sRGB colors";
    scanner->opt[OPT_TRANSFORM_TO_SRGB].desc = "Transform image to approximate sRGB color space";
    scanner->opt[OPT_TRANSFORM_TO_SRGB].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_TRANSFORM_TO_SRGB].unit = SANE_UNIT_NONE;
    scanner->val[OPT_TRANSFORM_TO_SRGB].w = SANE_FALSE;
    scanner->opt[OPT_TRANSFORM_TO_SRGB].cap |= SANE_CAP_INACTIVE;

    /* color correction for generic negative film */
    scanner->opt[OPT_INVERT_IMAGE].name = "invert";
    scanner->opt[OPT_INVERT_IMAGE].title = "Invert colors";
    scanner->opt[OPT_INVERT_IMAGE].desc = "Correct for generic negative film";
    scanner->opt[OPT_INVERT_IMAGE].type = SANE_TYPE_BOOL;
    scanner->opt[OPT_INVERT_IMAGE].unit = SANE_UNIT_NONE;
    scanner->val[OPT_INVERT_IMAGE].w = SANE_FALSE;
    scanner->opt[OPT_INVERT_IMAGE].cap |= SANE_CAP_INACTIVE;

    /* crop image */
    scanner->opt[OPT_CROP_IMAGE].name = "crop";
    scanner->opt[OPT_CROP_IMAGE].title = "Cropping";
    scanner->opt[OPT_CROP_IMAGE].desc = "How to crop the image";
    scanner->opt[OPT_CROP_IMAGE].type = SANE_TYPE_STRING;
    scanner->opt[OPT_CROP_IMAGE].size = max_string_size ((SANE_String_Const const *)(void*) scanner->device->crop_sw_list);
    scanner->opt[OPT_CROP_IMAGE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    scanner->opt[OPT_CROP_IMAGE].constraint.string_list = (SANE_String_Const const *)(void*) scanner->device->crop_sw_list;
    scanner->val[OPT_CROP_IMAGE].s = (SANE_Char *) strdup (scanner->device->crop_sw_list[2]);

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
    scanner->val[OPT_PREVIEW].w = SANE_FALSE;

    /* save shading data */
    scanner->opt[OPT_SAVE_SHADINGDATA].name = "save-shading-data";
    scanner->opt[OPT_SAVE_SHADINGDATA].title = "Save shading data";
    scanner->opt[OPT_SAVE_SHADINGDATA].desc = "Save shading data in 'pieusb.shading'";
    scanner->opt[OPT_SAVE_SHADINGDATA].type = SANE_TYPE_BOOL;
    scanner->val[OPT_SAVE_SHADINGDATA].w = SANE_FALSE;

    /* save CCD mask */
    scanner->opt[OPT_SAVE_CCDMASK].name = "save-ccdmask";
    scanner->opt[OPT_SAVE_CCDMASK].title = "Save CCD mask";
    scanner->opt[OPT_SAVE_CCDMASK].desc = "Save CCD mask 'pieusb.ccd'";
    scanner->opt[OPT_SAVE_CCDMASK].type = SANE_TYPE_BOOL;
    scanner->val[OPT_SAVE_CCDMASK].w = SANE_FALSE;

    scanner->opt[OPT_LIGHT].name = "light";
    scanner->opt[OPT_LIGHT].title = "Light";
    scanner->opt[OPT_LIGHT].desc = "Light";
    scanner->opt[OPT_LIGHT].type = SANE_TYPE_INT;
    scanner->opt[OPT_LIGHT].unit = SANE_UNIT_MICROSECOND;
    scanner->opt[OPT_LIGHT].cap |= SANE_CAP_SOFT_SELECT;
    scanner->opt[OPT_LIGHT].size = sizeof(SANE_Word);
    scanner->val[OPT_LIGHT].w = DEFAULT_LIGHT;

    scanner->opt[OPT_DOUBLE_TIMES].name = "double-times";
    scanner->opt[OPT_DOUBLE_TIMES].title = "Double times";
    scanner->opt[OPT_DOUBLE_TIMES].desc = "Double times";
    scanner->opt[OPT_DOUBLE_TIMES].type = SANE_TYPE_INT;
    scanner->opt[OPT_DOUBLE_TIMES].unit = SANE_UNIT_MICROSECOND;
    scanner->opt[OPT_DOUBLE_TIMES].cap |= SANE_CAP_SOFT_SELECT;
    scanner->opt[OPT_DOUBLE_TIMES].size = sizeof(SANE_Word);
    scanner->val[OPT_DOUBLE_TIMES].w = DEFAULT_DOUBLE_TIMES;

    /* exposure times for R, G, B and I */
    scanner->opt[OPT_SET_EXPOSURE_R].name = SANE_NAME_EXPOSURE_R;
    scanner->opt[OPT_SET_EXPOSURE_R].title = SANE_TITLE_EXPOSURE_R;
    scanner->opt[OPT_SET_EXPOSURE_R].desc = SANE_DESC_EXPOSURE_R;
    scanner->opt[OPT_SET_EXPOSURE_G].name = SANE_NAME_EXPOSURE_G;
    scanner->opt[OPT_SET_EXPOSURE_G].title = SANE_TITLE_EXPOSURE_G;
    scanner->opt[OPT_SET_EXPOSURE_G].desc = SANE_DESC_EXPOSURE_G;
    scanner->opt[OPT_SET_EXPOSURE_B].name = SANE_NAME_EXPOSURE_B;
    scanner->opt[OPT_SET_EXPOSURE_B].title = SANE_TITLE_EXPOSURE_B;
    scanner->opt[OPT_SET_EXPOSURE_B].desc = SANE_DESC_EXPOSURE_B;
    scanner->opt[OPT_SET_EXPOSURE_I].name = SANE_NAME_EXPOSURE_I;
    scanner->opt[OPT_SET_EXPOSURE_I].title = SANE_TITLE_EXPOSURE_I;
    scanner->opt[OPT_SET_EXPOSURE_I].desc = SANE_DESC_EXPOSURE_I;
    for (i = OPT_SET_EXPOSURE_R; i <= OPT_SET_EXPOSURE_I; ++i) {
    scanner->opt[i].type = SANE_TYPE_INT;
    scanner->opt[i].unit = SANE_UNIT_MICROSECOND;
    scanner->opt[i].cap |= SANE_CAP_SOFT_SELECT;
    scanner->opt[i].constraint_type = SANE_CONSTRAINT_RANGE;
    scanner->opt[i].constraint.range = &(scanner->device->exposure_range);
    scanner->opt[i].size = sizeof(SANE_Word);
    scanner->val[i].w = SANE_EXPOSURE_DEFAULT;
    }

    /* gain for R, G, B and I */
    scanner->opt[OPT_SET_GAIN_R].name = SANE_NAME_GAIN_R;
    scanner->opt[OPT_SET_GAIN_R].title = SANE_TITLE_GAIN_R;
    scanner->opt[OPT_SET_GAIN_R].desc = SANE_DESC_GAIN_R;
    scanner->opt[OPT_SET_GAIN_G].name = SANE_NAME_GAIN_G;
    scanner->opt[OPT_SET_GAIN_G].title = SANE_TITLE_GAIN_G;
    scanner->opt[OPT_SET_GAIN_G].desc = SANE_DESC_GAIN_G;
    scanner->opt[OPT_SET_GAIN_B].name = SANE_NAME_GAIN_B;
    scanner->opt[OPT_SET_GAIN_B].title = SANE_TITLE_GAIN_B;
    scanner->opt[OPT_SET_GAIN_B].desc = SANE_DESC_GAIN_B;
    scanner->opt[OPT_SET_GAIN_I].name = SANE_NAME_GAIN_I;
    scanner->opt[OPT_SET_GAIN_I].title = SANE_TITLE_GAIN_I;
    scanner->opt[OPT_SET_GAIN_I].desc = SANE_DESC_GAIN_I;
    for (i = OPT_SET_GAIN_R; i <= OPT_SET_GAIN_I; ++i) {
      scanner->opt[i].type = SANE_TYPE_INT;
      scanner->opt[i].unit = SANE_UNIT_NONE;
      scanner->opt[i].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[i].constraint.range = &gain_range;
      scanner->opt[i].size = sizeof(SANE_Word);
      scanner->val[i].w = SANE_GAIN_DEFAULT;
    }
    /* offsets for R, G, B and I */
    scanner->opt[OPT_SET_OFFSET_R].name = SANE_NAME_OFFSET_R;
    scanner->opt[OPT_SET_OFFSET_R].title = SANE_TITLE_OFFSET_R;
    scanner->opt[OPT_SET_OFFSET_R].desc = SANE_DESC_OFFSET_R;
    scanner->opt[OPT_SET_OFFSET_G].name = SANE_NAME_OFFSET_G;
    scanner->opt[OPT_SET_OFFSET_G].title = SANE_TITLE_OFFSET_G;
    scanner->opt[OPT_SET_OFFSET_G].desc = SANE_DESC_OFFSET_G;
    scanner->opt[OPT_SET_OFFSET_B].name = SANE_NAME_OFFSET_B;
    scanner->opt[OPT_SET_OFFSET_B].title = SANE_TITLE_OFFSET_B;
    scanner->opt[OPT_SET_OFFSET_B].desc = SANE_DESC_OFFSET_B;
    scanner->opt[OPT_SET_OFFSET_I].name = SANE_NAME_OFFSET_I;
    scanner->opt[OPT_SET_OFFSET_I].title = SANE_TITLE_OFFSET_I;
    scanner->opt[OPT_SET_OFFSET_I].desc = SANE_DESC_OFFSET_I;
    for (i = OPT_SET_OFFSET_R; i <= OPT_SET_OFFSET_I; ++i) {
      scanner->opt[i].type = SANE_TYPE_INT;
      scanner->opt[i].unit = SANE_UNIT_NONE;
      scanner->opt[i].constraint_type = SANE_CONSTRAINT_RANGE;
      scanner->opt[i].constraint.range = &offset_range;
      scanner->opt[i].size = sizeof(SANE_Word);
      scanner->val[i].w = SANE_OFFSET_DEFAULT;
    }
    return SANE_STATUS_GOOD;
}

/**
 * Parse line from config file into a vendor id, product id and a model number
 *
 * @param config_line Text to parse
 * @param vendor_id
 * @param product_id
 * @param model_number
 * @return SANE_STATUS_GOOD, or SANE_STATUS_INVAL in case of a parse error
 */
SANE_Status
sanei_pieusb_parse_config_line(const char* config_line, SANE_Word* vendor_id, SANE_Word* product_id, SANE_Word* model_number)
{
    char *vendor_id_string, *product_id_string, *model_number_string;

    if (strncmp (config_line, "usb ", 4) != 0) {
        return SANE_STATUS_INVAL;
    }
    /* Detect vendor-id */
    config_line += 4;
    config_line = sanei_config_skip_whitespace (config_line);
    if (*config_line) {
        config_line = sanei_config_get_string (config_line, &vendor_id_string);
        if (vendor_id_string) {
            *vendor_id = strtol (vendor_id_string, 0, 0);
            free (vendor_id_string);
        } else {
            return SANE_STATUS_INVAL;
        }
        config_line = sanei_config_skip_whitespace (config_line);
    } else {
        return SANE_STATUS_INVAL;
    }
    /* Detect product-id */
    config_line = sanei_config_skip_whitespace (config_line);
    if (*config_line) {
        config_line = sanei_config_get_string (config_line, &product_id_string);
        if (product_id_string) {
            *product_id = strtol (product_id_string, 0, 0);
            free (product_id_string);
        } else {
            return SANE_STATUS_INVAL;
        }
        config_line = sanei_config_skip_whitespace (config_line);
    } else {
        return SANE_STATUS_INVAL;
    }
    /* Detect product-id */
    config_line = sanei_config_skip_whitespace (config_line);
    if (*config_line) {
        config_line = sanei_config_get_string (config_line, &model_number_string);
        if (model_number_string) {
            *model_number = strtol (model_number_string, 0, 0);
            free (model_number_string);
        } else {
            return SANE_STATUS_INVAL;
        }
        config_line = sanei_config_skip_whitespace (config_line);
    } else {
        return SANE_STATUS_INVAL;
    }
    return SANE_STATUS_GOOD;
}

/**
 * Check if current list of supported devices contains the given specifications.
 *
 * @param vendor_id
 * @param product_id
 * @param model_number
 * @return
 */
SANE_Bool
sanei_pieusb_supported_device_list_contains(SANE_Word vendor_id, SANE_Word product_id, SANE_Word model_number)
{
    int i = 0;
    while (pieusb_supported_usb_device_list[i].vendor != 0) {
        if (pieusb_supported_usb_device_list[i].vendor == vendor_id
              && pieusb_supported_usb_device_list[i].product == product_id
              && pieusb_supported_usb_device_list[i].model == model_number) {
            return SANE_TRUE;
        }
        i++;
    }
    return SANE_FALSE;
}

/**
 * Add the given specifications to the current list of supported devices
 * @param vendor_id
 * @param product_id
 * @param model_number
 * @return
 */
SANE_Status
sanei_pieusb_supported_device_list_add(SANE_Word vendor_id, SANE_Word product_id, SANE_Word model_number)
{
    int i = 0, k;
    struct Pieusb_USB_Device_Entry* dl;

    while (pieusb_supported_usb_device_list[i].vendor != 0) {
        i++;
    }
    /* i is index of last entry */
    for (k=0; k<=i; k++) {
        DBG(DBG_info_proc,"sanei_pieusb_supported_device_list_add(): current %03d: %04x %04x %02x\n", i,
            pieusb_supported_usb_device_list[k].vendor,
            pieusb_supported_usb_device_list[k].product,
            pieusb_supported_usb_device_list[k].model);
    }

    dl = realloc(pieusb_supported_usb_device_list,(i+2)*sizeof(struct Pieusb_USB_Device_Entry)); /* Add one entry to list */
    if (dl == NULL) {
        return SANE_STATUS_INVAL;
    }
    /* Copy values */
    pieusb_supported_usb_device_list = dl;
    pieusb_supported_usb_device_list[i].vendor = vendor_id;
    pieusb_supported_usb_device_list[i].product = product_id;
    pieusb_supported_usb_device_list[i].model = model_number;
    pieusb_supported_usb_device_list[i+1].vendor = 0;
    pieusb_supported_usb_device_list[i+1].product = 0;
    pieusb_supported_usb_device_list[i+1].model = 0;
    for (k=0; k<=i+1; k++) {
        DBG(DBG_info_proc,"sanei_pieusb_supported_device_list_add() add: %03d: %04x %04x %02x\n", i,
            pieusb_supported_usb_device_list[k].vendor,
            pieusb_supported_usb_device_list[k].product,
            pieusb_supported_usb_device_list[k].model);
    }
    return SANE_STATUS_GOOD;
}

/**
 * Actions to perform when a cancel request has been received.
 *
 * @param scanner scanner to stop scanning
 * @return SANE_STATUS_CANCELLED
 */
SANE_Status
sanei_pieusb_on_cancel (Pieusb_Scanner * scanner)
{
    struct Pieusb_Command_Status status;

    DBG (DBG_info_proc, "sanei_pieusb_on_cancel()\n");

    sanei_pieusb_cmd_stop_scan (scanner->device_number, &status);
    sanei_pieusb_cmd_set_scan_head (scanner->device_number, 1, 0, &status);
    sanei_pieusb_buffer_delete (&scanner->buffer);
    scanner->scanning = SANE_FALSE;
    return SANE_STATUS_CANCELLED;
}

/**
 * Determine maximum lengt of a set of strings.
 *
 * @param strings Set of strings
 * @return maximum length
 */
static size_t
max_string_size (SANE_String_Const const strings[])
{
    size_t size, max_size = 0;
    int i;

    for (i = 0; strings[i]; ++i) {
        size = strlen (strings[i]) + 1;
        if (size > max_size) {
            max_size = size;
        }
    }

    return max_size;
}

/* From MR's pie.c */

/* ------------------------- PIEUSB_CORRECT_SHADING -------------------------- */

/**
 * Correct the given buffer for shading using shading data in scanner.
 * If the loop order is width->color->height, a 7200 dpi scan correction takes
 * 45 minutes. If the loop order is color->height->width, this is less than 3
 * minutes. So it is worthwhile to find the used pixels first (array width_to_loc).
 *
 * @param scanner Scanner
 * @param buffer Buffer to correct
 */
void
sanei_pieusb_correct_shading(struct Pieusb_Scanner *scanner, struct Pieusb_Read_Buffer *buffer)
{

    int i, j, c, k;
    SANE_Uint val, val_org, *p;
    int *width_to_loc;

    DBG (DBG_info_proc, "sanei_pieusb_correct_shading()\n");

    /* Loop through CCD-mask to find used pixels */
    width_to_loc = calloc(buffer->width,sizeof(int));
    j = 0;
    for (i = 0; i < scanner->ccd_mask_size; i++) {
        if (scanner->ccd_mask[i] == 0) {
            width_to_loc[j++] = i;
        }
    }
    /* Correct complete image */
    for (c = 0; c < buffer->colors; c++) {
        DBG(DBG_info,"sanei_pieusb_correct_shading() correct color %d\n",c);
        for (k = 0; k < buffer->height; k++) {
            /* DBG(DBG_info,"Correct line %d\n",k); */
            p = buffer->data + c * buffer->width * buffer->height + k * buffer->width;
            for (j = 0; j < buffer->width; j++) {
                val_org = *p;
                val = lround((double)scanner->shading_mean[c] / scanner->shading_ref[c][width_to_loc[j]] * val_org);
                /* DBG(DBG_info,"Correct [%d,%d,%d] %d -> %d\n",k,j,c,val_org,val); */
                *p++ = val;
            }
        }
    }
    /* Free memory */
    free(width_to_loc);
}

/* === functions copied from MR's code === */

/**
 *
 * @param scanner
 * @param in_img
 * @param planes
 * @param out_planes
 * @return
 */
SANE_Status
sanei_pieusb_post (Pieusb_Scanner *scanner, uint16_t **in_img, int planes)
{
  uint16_t *cplane[PLANES];    /* R, G, B, I gray scale planes */
  SANE_Parameters parameters;   /* describes the image */
  int winsize_smooth;           /* for adapting replaced pixels */
  char filename[64];
  SANE_Status status;
  int smooth, i;

  memcpy (&parameters, &scanner->scan_parameters, sizeof (SANE_Parameters));
  parameters.format = SANE_FRAME_GRAY;
  parameters.bytes_per_line = parameters.pixels_per_line;
  if (parameters.depth > 8)
    parameters.bytes_per_line *= 2;
  parameters.last_frame = 0;

  DBG (DBG_info, "pie_usb_post: %d ppl, %d lines, %d bits, %d planes, %d dpi\n",
       parameters.pixels_per_line, parameters.lines,
       parameters.depth, planes, scanner->mode.resolution);

  if (planes > PLANES) {
    DBG (DBG_error, "pie_usb_post: too many planes: %d (max %d)\n", planes, PLANES);
    return SANE_STATUS_INVAL;
  }

  for (i = 0; i < planes; i++)
    cplane[i] = in_img[i];

  /* dirt is rather resolution invariant, so
   * setup resolution dependent parameters
   */
  /* film grain reduction */
  smooth = scanner->val[OPT_SMOOTH_IMAGE].w;
  winsize_smooth = (scanner->mode.resolution / 540) | 1;
  /* smoothen whole image or only replaced pixels */
  if (smooth)
    {
      winsize_smooth += 2 * (smooth - 3);       /* even */
      if (winsize_smooth < 3)
        smooth = 0;
    }
  if (winsize_smooth < 3)
    winsize_smooth = 3;
  DBG (DBG_info, "pie_usb_sw_post: winsize_smooth %d\n", winsize_smooth);

  /* RGBI post-processing if selected:
   * 1) remove spectral overlay from ired plane,
   * 2) remove dirt, smoothen if, crop if */
  if (scanner->val[OPT_CORRECT_INFRARED].b) /* (scanner->processing & POST_SW_IRED_MASK) */
    {
      /* remove spectral overlay from ired plane */
      status = sanei_ir_spectral_clean (&parameters, scanner->ln_lut, cplane[0], cplane[3]);
      if (status != SANE_STATUS_GOOD)
        return status;
      if (DBG_LEVEL >= 15)
        {
          snprintf (filename, 63, "/tmp/ir-spectral.pnm");
          pieusb_write_pnm_file (filename, cplane[3],
                                  parameters.depth, 1,
                                  parameters.pixels_per_line, parameters.lines);
        }
      if (scanner->cancel_request)          /* asynchronous cancel ? */
        return SANE_STATUS_CANCELLED;
  } /* scanner-> processing & POST_SW_IRED_MASK */

  /* remove dirt, smoothen if, crop if */
  if (scanner->val[OPT_CLEAN_IMAGE].b) /* (scanner->processing & POST_SW_DIRT) */
    {
      double *norm_histo;
      uint16_t *thresh_data;
      int static_thresh, too_thresh;    /* static thresholds */
      int winsize_filter;               /* primary size of filtering window */
      int size_dilate;                  /* the dirt mask */

      /* size of filter detecting dirt */
      winsize_filter = (int) (5.0 * (double) scanner->mode.resolution / 300.0) | 1;
      if (winsize_filter < 3)
        winsize_filter = 3;
      /* dirt usually has smooth edges which also need correction */
      size_dilate = scanner->mode.resolution / 1000 + 1;

      /* first detect large dirt by a static threshold */
      status = sanei_ir_create_norm_histogram (&parameters, cplane[3], &norm_histo);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "pie_usb_sw_post: no buffer\n");
          return SANE_STATUS_NO_MEM;
        }
      /* generate a "bimodal" static threshold */
      status = sanei_ir_threshold_yen (&parameters, norm_histo, &static_thresh);
      if (status != SANE_STATUS_GOOD)
        return status;
      /* generate traditional static threshold */
      status = sanei_ir_threshold_otsu (&parameters, norm_histo, &too_thresh);
      if (status != SANE_STATUS_GOOD)
        return status;
      /* choose lower one */
      if (too_thresh < static_thresh)
        static_thresh = too_thresh;
      free (norm_histo);

      /* then generate dirt mask with adaptive thresholding filter
       * and add the dirt from the static threshold */
      /* last two parameters: 10, 50 detects more, 20, 75 less */
      status = sanei_ir_filter_madmean (&parameters, cplane[3], &thresh_data, winsize_filter, 20, 100);
      if (status != SANE_STATUS_GOOD) {
        free (thresh_data);
        return status;
      }
      sanei_ir_add_threshold (&parameters, cplane[3], thresh_data, static_thresh);
      if (DBG_LEVEL >= 15)
        {
          snprintf (filename, 63, "/tmp/ir-threshold.pnm");
          pieusb_write_pnm_file (filename, thresh_data,
                                  8, 1, parameters.pixels_per_line,
                                  parameters.lines);
        }
      if (scanner->cancel_request) {         /* asynchronous cancel ? */
        free (thresh_data);
        return SANE_STATUS_CANCELLED;
      }
      /* replace the dirt and smoothen film grain and crop if possible */
      status = sanei_ir_dilate_mean (&parameters, cplane, thresh_data,
              500, size_dilate, winsize_smooth, smooth,
              0, NULL);
      if (status != SANE_STATUS_GOOD) {
        free (thresh_data);
        return status;
      }
      smooth = 0;
      free (thresh_data);
    }

  if (DBG_LEVEL >= 15)
    {
      pieusb_write_pnm_file ("/tmp/RGBi-img.pnm", scanner->buffer.data,
        scanner->scan_parameters.depth, 3, scanner->scan_parameters.pixels_per_line,
        scanner->scan_parameters.lines);
    }

  return status;
}

/* ------------------------------ PIE_USB_WRITE_PNM_FILE ------------------------------- */
static SANE_Status
pieusb_write_pnm_file (char *filename, SANE_Uint *data, int depth,
                        int channels, int pixels_per_line, int lines)
{
  FILE *out;
  int r, c, ch;
  SANE_Uint val;
  uint8_t b = 0;

  DBG (DBG_info_proc,
       "pie_usb_write_pnm_file: depth=%d, channels=%d, ppl=%d, lines=%d\n",
       depth, channels, pixels_per_line, lines);

  out = fopen (filename, "w");
  if (!out)
    {
      DBG (DBG_error,
           "pie_usb_write_pnm_file: could not open %s for writing: %s\n",
           filename, strerror (errno));
      return SANE_STATUS_INVAL;
    }

  switch (depth) {
      case 1:
          fprintf (out, "P4\n%d\n%d\n", pixels_per_line, lines);
          for (r = 0; r < lines; r++) {
              int i;
              i = 0;
              b = 0;
              for (c = 0; c < pixels_per_line; c++) {
                  val = *(data + r * pixels_per_line + c);
                  if (val > 0) b |= (0x80 >> i);
                  i++;
                  if (i == 7) {
                      fputc(b, out);
                      i = 0;
                      b = 0;
                  }
              }
              if (i != 0) {
                  fputc(b, out);
              }
          }
          break;
      case 8:
          fprintf (out, "P%c\n%d\n%d\n%d\n", channels == 1 ? '5' : '6', pixels_per_line, lines, 255);
          for (r = 0; r < lines; r++) {
              for (c = 0; c < pixels_per_line; c++) {
                  for (ch = 0; ch < channels; ch++) {
                      val = *(data + ch * lines * pixels_per_line + r * pixels_per_line + c);
                      b = val & 0xFF;
                      fputc(b, out);
                  }
              }
          }
          break;
      case 16:
          fprintf (out, "P%c\n%d\n%d\n%d\n", channels == 1 ? '5' : '6', pixels_per_line, lines, 65535);
          for (r = 0; r < lines; r++) {
              for (c = 0; c < pixels_per_line; c++) {
                  for (ch = 0; ch < channels; ch++) {
                      val = *(data + ch * lines * pixels_per_line + r * pixels_per_line + c);
                      b = (val >> 8) & 0xFF;
                      fputc(b, out);
                      b = val & 0xFF;
                      fputc(b, out);
                  }
              }
          }
          break;
      default:
          DBG (DBG_error, "pie_usb_write_pnm_file: depth %d not implemented\n", depth);
  }
  fclose (out);

  DBG (DBG_info, "pie_usb_write_pnm_file: finished\n");
  return SANE_STATUS_GOOD;
}

/**
 * Check option inconsistencies.
 * In most cases an inconsistency can be solved by ignoring an option setting.
 * Message these situations and return 1 to indicate we can work with the
 * current set op options. If the settings are really inconsistent, return 0.
 */
int
sanei_pieusb_analyse_options(struct Pieusb_Scanner *scanner)
{
    /* Checks*/
    if (scanner->val[OPT_TL_X].w > scanner->val[OPT_BR_X].w) {
        DBG (DBG_error, "sane_start: %s (%.1f mm) is bigger than %s (%.1f mm) -- aborting\n",
	   scanner->opt[OPT_TL_X].title,
	   SANE_UNFIX (scanner->val[OPT_TL_X].w),
	   scanner->opt[OPT_BR_X].title,
	   SANE_UNFIX (scanner->val[OPT_BR_X].w));
        return 0;
    }
    if (scanner->val[OPT_TL_Y].w > scanner->val[OPT_BR_Y].w) {
        DBG (DBG_error, "sane_start: %s (%.1f mm) is bigger than %s (%.1f mm) -- aborting\n",
	   scanner->opt[OPT_TL_Y].title,
	   SANE_UNFIX (scanner->val[OPT_TL_Y].w),
	   scanner->opt[OPT_BR_Y].title,
	   SANE_UNFIX (scanner->val[OPT_BR_Y].w));
        return 0;
    }
    /* Modes sometimes limit other choices */
    if (scanner->val[OPT_PREVIEW].b) {
        /* Preview uses its own specific settings */
        if (scanner->val[OPT_RESOLUTION].w != (scanner->device->fast_preview_resolution << SANE_FIXED_SCALE_SHIFT)) {
            DBG (DBG_info_sane, "Option %s = %f ignored during preview\n", scanner->opt[OPT_RESOLUTION].name, SANE_UNFIX(scanner->val[OPT_RESOLUTION].w));
        }
        if (scanner->val[OPT_SHARPEN].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored during preview\n", scanner->opt[OPT_SHARPEN].name, scanner->val[OPT_SHARPEN].b);
        }
        if (!scanner->val[OPT_FAST_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored during preview\n", scanner->opt[OPT_FAST_INFRARED].name, scanner->val[OPT_FAST_INFRARED].b);
        }
        if (scanner->val[OPT_CORRECT_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored during preview\n", scanner->opt[OPT_CORRECT_INFRARED].name, scanner->val[OPT_CORRECT_INFRARED].b);
        }
        if (scanner->val[OPT_CLEAN_IMAGE].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored during preview\n", scanner->opt[OPT_CLEAN_IMAGE].name, scanner->val[OPT_CLEAN_IMAGE].b);
        }
        if (scanner->val[OPT_SMOOTH_IMAGE].w != 0) {
            DBG (DBG_info_sane, "Option %s = %d ignored during preview\n", scanner->opt[OPT_SMOOTH_IMAGE].name, scanner->val[OPT_SMOOTH_IMAGE].w);
        }
        if (strcmp(scanner->val[OPT_CROP_IMAGE].s, scanner->device->crop_sw_list[0]) != 0) {
            DBG (DBG_info_sane, "Option %s = %s ignored during preview\n", scanner->opt[OPT_CROP_IMAGE].name, scanner->val[OPT_CROP_IMAGE].s);
        }
        if (scanner->val[OPT_TRANSFORM_TO_SRGB].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored during preview\n", scanner->opt[OPT_TRANSFORM_TO_SRGB].name, scanner->val[OPT_TRANSFORM_TO_SRGB].w);
        }
        if (scanner->val[OPT_INVERT_IMAGE].w) {
            DBG (DBG_info_sane, "Option %s = %d ignored during preview\n", scanner->opt[OPT_INVERT_IMAGE].name, scanner->val[OPT_INVERT_IMAGE].w);
        }
    } else if (strcmp(scanner->val[OPT_MODE].s,SANE_VALUE_SCAN_MODE_LINEART)==0) {
        /* Can we do any post processing in lineart? Needs testing to see what's possible */
        if (scanner->val[OPT_BIT_DEPTH].w != 1) {
            DBG (DBG_info_sane, "Option %s = %d ignored in lineart mode (will use 1)\n", scanner->opt[OPT_BIT_DEPTH].name, scanner->val[OPT_BIT_DEPTH].w);
        }
        if (!scanner->val[OPT_FAST_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in lineart mode (irrelevant)\n", scanner->opt[OPT_FAST_INFRARED].name, scanner->val[OPT_FAST_INFRARED].b);
        }
        if (!scanner->val[OPT_CORRECT_SHADING].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in lineart mode (irrelevant)\n", scanner->opt[OPT_CORRECT_SHADING].name, scanner->val[OPT_CORRECT_SHADING].b);
        }
        if (!scanner->val[OPT_CORRECT_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in lineart mode (irrelevant)\n", scanner->opt[OPT_CORRECT_INFRARED].name, scanner->val[OPT_CORRECT_INFRARED].b);
        }
        if (scanner->val[OPT_CLEAN_IMAGE].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in lineart mode (irrelevant)\n", scanner->opt[OPT_CLEAN_IMAGE].name, scanner->val[OPT_CLEAN_IMAGE].b);
        }
        if (scanner->val[OPT_SMOOTH_IMAGE].w != 0) {
            DBG (DBG_info_sane, "Option %s = %d ignored in lineart mode (irrelevant)\n", scanner->opt[OPT_SMOOTH_IMAGE].name, scanner->val[OPT_SMOOTH_IMAGE].w);
        }
        if (strcmp(scanner->val[OPT_CROP_IMAGE].s, scanner->device->crop_sw_list[0]) != 0) {
            DBG (DBG_info_sane, "Option %s = %s ignored in lineart mode (irrelevant)\n", scanner->opt[OPT_CROP_IMAGE].name, scanner->val[OPT_CROP_IMAGE].s);
        }
        if (scanner->val[OPT_TRANSFORM_TO_SRGB].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in lineart mode (irrelevant)\n", scanner->opt[OPT_TRANSFORM_TO_SRGB].name, scanner->val[OPT_TRANSFORM_TO_SRGB].w);
        }
    } else if (strcmp(scanner->val[OPT_MODE].s,SANE_VALUE_SCAN_MODE_HALFTONE)==0) {
        /* Can we do any post processing in halftone? Needs testing to see what's possible */
        if (scanner->val[OPT_BIT_DEPTH].w != 1) {
            DBG (DBG_info_sane, "Option %s = %d ignored in halftone mode (will use 1)\n", scanner->opt[OPT_BIT_DEPTH].name, scanner->val[OPT_BIT_DEPTH].w);
        }
        if (!scanner->val[OPT_FAST_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in halftone mode (irrelevant)\n", scanner->opt[OPT_FAST_INFRARED].name, scanner->val[OPT_FAST_INFRARED].b);
        }
        if (!scanner->val[OPT_CORRECT_SHADING].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in halftone mode (irrelevant)\n", scanner->opt[OPT_CORRECT_SHADING].name, scanner->val[OPT_CORRECT_SHADING].b);
        }
        if (!scanner->val[OPT_CORRECT_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in halftone mode (irrelevant)\n", scanner->opt[OPT_CORRECT_INFRARED].name, scanner->val[OPT_CORRECT_INFRARED].b);
        }
        if (scanner->val[OPT_CLEAN_IMAGE].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in halftone mode (irrelevant)\n", scanner->opt[OPT_CLEAN_IMAGE].name, scanner->val[OPT_CLEAN_IMAGE].b);
        }
        if (scanner->val[OPT_SMOOTH_IMAGE].w != 0) {
            DBG (DBG_info_sane, "Option %s = %d ignored in halftone mode (irrelevant)\n", scanner->opt[OPT_SMOOTH_IMAGE].name, scanner->val[OPT_SMOOTH_IMAGE].w);
        }
        if (strcmp(scanner->val[OPT_CROP_IMAGE].s, scanner->device->crop_sw_list[0]) != 0) {
            DBG (DBG_info_sane, "Option %s = %s ignored in halftone mode (irrelevant)\n", scanner->opt[OPT_CROP_IMAGE].name, scanner->val[OPT_CROP_IMAGE].s);
        }
        if (scanner->val[OPT_TRANSFORM_TO_SRGB].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in halftone mode (irrelevant)\n", scanner->opt[OPT_TRANSFORM_TO_SRGB].name, scanner->val[OPT_TRANSFORM_TO_SRGB].w);
        }
    } else if (strcmp(scanner->val[OPT_MODE].s,SANE_VALUE_SCAN_MODE_GRAY)==0) {
        /* Can we do any post processing in gray mode? */
        /* Can we obtain a single color channel in this mode? How? */
        /* Is this just RGB with luminance trasformation? */
        /* Needs testing to see what's possible */
        /* Only do 8 or 16 bit scans */
        if (scanner->val[OPT_BIT_DEPTH].w == 1) {
            DBG (DBG_info_sane, "Option %s = %d ignored in gray mode (will use 8)\n", scanner->opt[OPT_BIT_DEPTH].name, scanner->val[OPT_BIT_DEPTH].w);
        }
        if (!scanner->val[OPT_FAST_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in gray mode (irrelevant)\n", scanner->opt[OPT_FAST_INFRARED].name, scanner->val[OPT_FAST_INFRARED].b);
        }
        if (!scanner->val[OPT_CORRECT_INFRARED].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in gray mode (irrelevant)\n", scanner->opt[OPT_CORRECT_INFRARED].name, scanner->val[OPT_CORRECT_INFRARED].b);
        }
        if (scanner->val[OPT_CLEAN_IMAGE].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in gray mode (irrelevant)\n", scanner->opt[OPT_CLEAN_IMAGE].name, scanner->val[OPT_CLEAN_IMAGE].b);
        }
        if (scanner->val[OPT_TRANSFORM_TO_SRGB].b) {
            DBG (DBG_info_sane, "Option %s = %d ignored in gray mode (irrelevant)\n", scanner->opt[OPT_TRANSFORM_TO_SRGB].name, scanner->val[OPT_TRANSFORM_TO_SRGB].w);
        }
    } else if (strcmp(scanner->val[OPT_MODE].s,SANE_VALUE_SCAN_MODE_COLOR)==0) {
        /* Some options require infrared data to be obtained, so all infrared options are relevant */
        /* Only do 8 or 16 bit scans */
        if (scanner->val[OPT_BIT_DEPTH].w == 1) {
            DBG (DBG_info_sane, "Option %s = %d ignored in color mode (will use 8)\n", scanner->opt[OPT_BIT_DEPTH].name, scanner->val[OPT_BIT_DEPTH].w);
        }
    } else if (strcmp(scanner->val[OPT_MODE].s,SANE_VALUE_SCAN_MODE_RGBI)==0) {
        /* Only do 8 or 16 bit scans */
        if (scanner->val[OPT_BIT_DEPTH].w == 1) {
            DBG (DBG_info_sane, "Option %s = %d ignored in color mode (will use 8)\n", scanner->opt[OPT_BIT_DEPTH].name, scanner->val[OPT_BIT_DEPTH].w);
        }
    }

    return 1;
}

/**
 * Print options
 *
 * @param scanner
 */
void
sanei_pieusb_print_options(struct Pieusb_Scanner *scanner)
{
    int k;
    /* List current options and values */
    DBG (DBG_info, "Num options = %d\n", scanner->val[OPT_NUM_OPTS].w);
    for (k = 1; k < scanner->val[OPT_NUM_OPTS].w; k++) {
        switch (scanner->opt[k].type) {
            case SANE_TYPE_BOOL:
                DBG(DBG_info,"  Option %d: %s = %d\n", k, scanner->opt[k].name, scanner->val[k].b);
                break;
            case SANE_TYPE_INT:
	        DBG(DBG_info,"  Option %d: %s = %d\n", k, scanner->opt[k].name, scanner->val[k].w);
                break;
            case SANE_TYPE_FIXED:
                DBG(DBG_info,"  Option %d: %s = %f\n", k, scanner->opt[k].name, SANE_UNFIX (scanner->val[k].w));
                break;
            case SANE_TYPE_STRING:
                DBG(DBG_info,"  Option %d: %s = %s\n", k, scanner->opt[k].name, scanner->val[k].s);
                break;
            case SANE_TYPE_GROUP:
                DBG(DBG_info,"  Option %d: %s = %s\n", k, scanner->opt[k].title, scanner->val[k].s);
	        break;
            default:
                DBG(DBG_info,"  Option %d: %s unknown type %d\n", k, scanner->opt[k].name, scanner->opt[k].type);
                break;
        }
    }
}

/**
 * Calculate reference values for each pixel, line means and line maxima.
 * We have got 45 lines for all four colors and for each CCD pixel.
 * The reference value for each pixel is the 45-line average for that
 * pixel, for each color separately.
 *
 * @param scanner
 * @param buffer
 */
static void pieusb_calculate_shading(struct Pieusb_Scanner *scanner, SANE_Byte* buffer)
{
    int k, m;
    SANE_Byte* p;
    SANE_Int ci, val;
    SANE_Int shading_width = scanner->device->shading_parameters[0].pixelsPerLine;
    SANE_Int shading_height = scanner->device->shading_parameters[0].nLines;

    /* Initialze all to 0 */
    for (k = 0; k < SHADING_PARAMETERS_INFO_COUNT; k++) {
        scanner->shading_max[k] = 0;
        scanner->shading_mean[k] = 0;
        memset(scanner->shading_ref[k], 0, shading_width * sizeof (SANE_Int));
    }
    /* Process data from buffer */
    p = buffer;
    switch (scanner->mode.colorFormat) {
        case 0x01: /* Pixel */
            /* Process pixel by pixel */
            for (k = 0; k < shading_height; k++) {
                for (m = 0; m < shading_width; m++) {
                    for (ci = 0; ci < SHADING_PARAMETERS_INFO_COUNT; ci++) {
                        val = *(p) + *(p+1) * 256;
                        scanner->shading_ref[ci][m] += val;
                        scanner->shading_max[ci] = scanner->shading_max[ci] < val ? val : scanner->shading_max[ci];
                        p += 2;
                    }
                }
            }
            break;
        case 0x04: /* Indexed */
            /* Process each line in the sequence found in the buffer */
            for (k = 0; k < shading_height*4; k++) {
                /* Save at right color */
                switch (*p) {
                    case 'R': ci = 0; break;
                    case 'G': ci = 1; break;
                    case 'B': ci = 2; break;
                    case 'I': ci = 3; break;
                    default: ci = -1; break; /* ignore line */
                }
                /* Add scanned data to reference line and keep track of maximum */
                if (ci != -1) {
                    for (m = 0; m < shading_width; m++) {
                        val = *(p+2+2*m) + *(p+2+2*m+1) * 256;
                        scanner->shading_ref[ci][m] += val;
                        scanner->shading_max[ci] = scanner->shading_max[ci] < val ? val : scanner->shading_max[ci];
                        /* DBG(DBG_error,"%02d Shading_ref[%d][%d] = %d\n",k,ci,m,scanner->shading_ref[ci][m]); */
                    }
                }
                /* Next line */
                p += 2*shading_width+2;
            }
            break;
        default:
            DBG (DBG_error,"sane_start(): color format %d not implemented\n",scanner->mode.colorFormat);
            return;
    }
    /* Mean reference value needs division */
    for (k = 0; k < SHADING_PARAMETERS_INFO_COUNT; k++) {
        for (m = 0; m < shading_width; m++) {
            scanner->shading_ref[k][m] = lround((double)scanner->shading_ref[k][m]/shading_height);
            /* DBG(DBG_error,"Shading_ref[%d][%d] = %d\n",k,m,scanner->shading_ref[k][m]); */
        }
    }
    /* Overall means */
    for (k = 0; k < SHADING_PARAMETERS_INFO_COUNT; k++) {
        for (m=0; m<shading_width; m++) {
            scanner->shading_mean[k] += scanner->shading_ref[k][m];
        }
        scanner->shading_mean[k] = lround((double)scanner->shading_mean[k]/shading_width);
        DBG (DBG_error,"Shading_mean[%d] = %d\n",k,scanner->shading_mean[k]);
    }

    /* Set shading data present */
    scanner->shading_data_present = SANE_TRUE;

    /* Export shading data as TIFF */
#ifdef CAN_DO_4_CHANNEL_TIFF
    if (scanner->val[OPT_SAVE_SHADINGDATA].b) {
        struct Pieusb_Read_Buffer shading;
        SANE_Byte* lboff = buffer;
        SANE_Int bpl = shading_width*2;
        SANE_Int n;
        buffer_create(&shading, shading_width, shading_height, 0x0F, 16);
        for (n=0; n<4*shading_height; n++) {
            if (buffer_put_single_color_line(&shading, *lboff, lboff+2, bpl) == 0) {
                break;
            }
            lboff += (bpl + 2);
        }
        FILE* fs = fopen("pieusb.shading", "w");
        /* write_tiff_rgbi_header (fs, shading_width, shading_height, 16, 3600, NULL); */
        fwrite(shading.data, 1, shading.image_size_bytes, fs);
        fclose(fs);
        buffer_delete(&shading);
    }
#endif

}

/*
 * Set frame (from scanner options)
 */

SANE_Status
sanei_pieusb_set_frame_from_options(Pieusb_Scanner * scanner)
{
    double dpmm;
    struct Pieusb_Command_Status status;

    dpmm = (double) scanner->device->maximum_resolution / MM_PER_INCH;
    scanner->frame.x0 = SANE_UNFIX(scanner->val[OPT_TL_X].w) * dpmm;
    scanner->frame.y0 = SANE_UNFIX(scanner->val[OPT_TL_Y].w) * dpmm;
    scanner->frame.x1 = SANE_UNFIX(scanner->val[OPT_BR_X].w) * dpmm;
    scanner->frame.y1 = SANE_UNFIX(scanner->val[OPT_BR_Y].w) * dpmm;
    scanner->frame.index = 0x80; /* 0x80: value from cyberview */
    sanei_pieusb_cmd_set_scan_frame (scanner->device_number, scanner->frame.index, &(scanner->frame), &status);
    DBG (DBG_info_sane, "sanei_pieusb_set_frame_from_options(): sanei_pieusb_cmd_set_scan_frame status %s\n", sane_strstatus (sanei_pieusb_convert_status (status.pieusb_status)));
    return status.pieusb_status;
}

/*
 * Set mode (from scanner options)
 */

SANE_Status
sanei_pieusb_set_mode_from_options(Pieusb_Scanner * scanner)
{
    struct Pieusb_Command_Status status;
    const char *mode;
    SANE_Status res;

    mode = scanner->val[OPT_MODE].s;
    if (strcmp (mode, SANE_VALUE_SCAN_MODE_LINEART) == 0) {
        scanner->mode.passes = SCAN_FILTER_GREEN; /* G */
        scanner->mode.colorFormat = SCAN_COLOR_FORMAT_PIXEL;
    } else if(strcmp (mode, SANE_VALUE_SCAN_MODE_HALFTONE) == 0) {
        scanner->mode.passes = SCAN_FILTER_GREEN; /* G */
        scanner->mode.colorFormat = SCAN_COLOR_FORMAT_PIXEL;
    } else if(strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY) == 0) {
        scanner->mode.passes = SCAN_FILTER_GREEN; /* G=gray; unable to get R & B & I to work */
        scanner->mode.colorFormat = SCAN_COLOR_FORMAT_PIXEL;
    } else if(scanner->val[OPT_PREVIEW].b) {
        /* Catch preview here, otherwise next ifs get complicated */
        scanner->mode.passes = SCAN_ONE_PASS_COLOR;
        scanner->mode.colorFormat = SCAN_COLOR_FORMAT_INDEX; /* pixel format might be an alternative */
    } else if(strcmp (mode, SANE_VALUE_SCAN_MODE_RGBI) == 0) {
        scanner->mode.passes = SCAN_ONE_PASS_RGBI;
        scanner->mode.colorFormat = SCAN_COLOR_FORMAT_INDEX;
    } else if(strcmp (mode, SANE_VALUE_SCAN_MODE_COLOR) == 0 && scanner->val[OPT_CLEAN_IMAGE].b) {
        scanner->mode.passes = SCAN_ONE_PASS_RGBI; /* Need infrared for cleaning */
        scanner->mode.colorFormat = SCAN_COLOR_FORMAT_INDEX;
    } else { /* SANE_VALUE_SCAN_MODE_COLOR */
        scanner->mode.passes = SCAN_ONE_PASS_COLOR;
        scanner->mode.colorFormat = SCAN_COLOR_FORMAT_INDEX; /* pixel format might be an alternative */
    }
    /* Resolution */
    if (scanner->val[OPT_PREVIEW].b) {
        scanner->mode.resolution = scanner->device->fast_preview_resolution;
        DBG (DBG_info_sane, "sanei_pieusb_set_mode_from_options(): resolution fast preview (%d)\n", scanner->mode.resolution);
    } else {
        scanner->mode.resolution = SANE_UNFIX (scanner->val[OPT_RESOLUTION].w);
        DBG (DBG_info_sane, "sanei_pieusb_set_mode_from_options(): resolution from option setting (%d)\n", scanner->mode.resolution);
    }
    /* Bit depth: exit on untested values */
    switch (scanner->val[OPT_BIT_DEPTH].w) {
        case 1: scanner->mode.colorDepth = SCAN_COLOR_DEPTH_1; break;
        case 8: scanner->mode.colorDepth = SCAN_COLOR_DEPTH_8; break;
        case 16: scanner->mode.colorDepth = SCAN_COLOR_DEPTH_16; break;
        default: /* 4, 10 & 12 */
            DBG (DBG_error, "sanei_pieusb_set_mode_from_options(): sanei_pieusb_cmd_set_scan_frame untested bit depth %d\n", scanner->val[OPT_BIT_DEPTH].w);
            return SANE_STATUS_INVAL;
    }
    scanner->mode.byteOrder = 0x01; /* 0x01 = Intel; only bit 0 used */
    scanner->mode.sharpen = scanner->val[OPT_SHARPEN].b && !scanner->val[OPT_PREVIEW].b;
    scanner->mode.skipShadingAnalysis = !scanner->val[OPT_SHADING_ANALYSIS].b;
    scanner->mode.fastInfrared = scanner->val[OPT_FAST_INFRARED].b && !scanner->val[OPT_PREVIEW].b;
    if (strcmp (scanner->val[OPT_HALFTONE_PATTERN].s, "53lpi 45d ROUND") == 0) {
        scanner->mode.halftonePattern = 0;
    } else { /*TODO: the others */
        scanner->mode.halftonePattern = 0;
    }
    scanner->mode.lineThreshold = SANE_UNFIX (scanner->val[OPT_THRESHOLD].w) / 100 * 0xFF; /* 0xFF = 100% */
    sanei_pieusb_cmd_set_mode (scanner->device_number, &(scanner->mode), &status);
    res = sanei_pieusb_convert_status(status.pieusb_status);
    if (res == SANE_STATUS_GOOD) {
      res = sanei_pieusb_wait_ready (scanner, 0);
    }
    DBG (DBG_info_sane, "sanei_pieusb_set_mode_from_options(): sanei_pieusb_cmd_set_mode status %s\n", sane_strstatus(res));
    return res;
}

/**
 * Set gains, exposure and offset, to:
 * - values default (pieusb_set_default_gain_offset)
 * - values set by options
 * - values set by auto-calibration procedure
 * - values determined from preceeding preview
 *
 * @param scanner
 * @return
 */
SANE_Status
sanei_pieusb_set_gain_offset(Pieusb_Scanner * scanner, const char *calibration_mode)
{
    struct Pieusb_Command_Status status;
    SANE_Status ret;
    double gain;

    DBG (DBG_info,"sanei_pieusb_set_gain_offset(): mode = %s\n", calibration_mode);

    if (strcmp (calibration_mode, SCAN_CALIBRATION_DEFAULT) == 0) {
        /* Default values */
        DBG(DBG_info_sane,"sanei_pieusb_set_gain_offset(): get calibration data from defaults\n");
        scanner->settings.exposureTime[0] = DEFAULT_EXPOSURE;
        scanner->settings.exposureTime[1] = DEFAULT_EXPOSURE;
        scanner->settings.exposureTime[2] = DEFAULT_EXPOSURE;
        scanner->settings.exposureTime[3] = DEFAULT_EXPOSURE;
        scanner->settings.offset[0] = DEFAULT_OFFSET;
        scanner->settings.offset[1] = DEFAULT_OFFSET;
        scanner->settings.offset[2] = DEFAULT_OFFSET;
        scanner->settings.offset[3] = DEFAULT_OFFSET;
        scanner->settings.gain[0] = DEFAULT_GAIN;
        scanner->settings.gain[1] = DEFAULT_GAIN;
        scanner->settings.gain[2] = DEFAULT_GAIN;
        scanner->settings.gain[3] = DEFAULT_GAIN;
        scanner->settings.light = DEFAULT_LIGHT;
        scanner->settings.extraEntries = DEFAULT_ADDITIONAL_ENTRIES;
        scanner->settings.doubleTimes = DEFAULT_DOUBLE_TIMES;
        status.pieusb_status = PIEUSB_STATUS_GOOD;
    } else if ((strcmp(calibration_mode, SCAN_CALIBRATION_PREVIEW) == 0)
	       && scanner->preview_done) {
        /* If no preview data availble, do the auto-calibration. */
        double dg, dgi;
        DBG (DBG_info, "sanei_pieusb_set_gain_offset(): get calibration data from preview. scanner->mode.passes %d\n", scanner->mode.passes);
        switch (scanner->mode.passes) {
            case SCAN_ONE_PASS_RGBI:
                dg = 3.00;
                dgi = ((double)scanner->settings.saturationLevel[0] / 65536) / ((double)scanner->preview_upper_bound[0] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                dgi = ((double)scanner->settings.saturationLevel[1] / 65536) / ((double)scanner->preview_upper_bound[1] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                dgi = ((double)scanner->settings.saturationLevel[2] / 65536) / ((double)scanner->preview_upper_bound[2] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                updateGain2(scanner, 0, dg);
                updateGain2(scanner, 1, dg);
                updateGain2(scanner, 2, dg);
	    break;
            case SCAN_ONE_PASS_COLOR:
                dg = 3.00;
                dgi = ((double)scanner->settings.saturationLevel[0] / 65536) / ((double)scanner->preview_upper_bound[0] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                dgi = ((double)scanner->settings.saturationLevel[1] / 65536) / ((double)scanner->preview_upper_bound[1] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                dgi = ((double)scanner->settings.saturationLevel[2] / 65536) / ((double)scanner->preview_upper_bound[2] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                updateGain2(scanner, 0, dg);
                updateGain2(scanner, 1, dg);
                updateGain2(scanner, 2, dg);
                break;
            case SCAN_FILTER_BLUE:
                dg = 3.00;
                dgi = ((double)scanner->settings.saturationLevel[2] / 65536) / ((double)scanner->preview_upper_bound[2] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                updateGain2(scanner, 2, dg);
                break;
            case SCAN_FILTER_GREEN:
                dg = 3.00;
                dgi = ((double)scanner->settings.saturationLevel[1] / 65536) / ((double)scanner->preview_upper_bound[1] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                updateGain2(scanner, 1, dg);
                break;
            case SCAN_FILTER_RED:
                dg = 3.00;
                dgi = ((double)scanner->settings.saturationLevel[0] / 65536) / ((double)scanner->preview_upper_bound[0] / HISTOGRAM_SIZE);
                if (dgi < dg) dg = dgi;
                updateGain2(scanner, 0, dg);
                break;
            case SCAN_FILTER_NEUTRAL:
                break;
        }
        status.pieusb_status = PIEUSB_STATUS_GOOD;
    } else if (strcmp (calibration_mode, SCAN_CALIBRATION_OPTIONS) == 0) {
        DBG (DBG_info_sane, "sanei_pieusb_set_gain_offset(): get calibration data from options\n");
        /* Exposure times */
        scanner->settings.exposureTime[0] = scanner->val[OPT_SET_EXPOSURE_R].w;
        scanner->settings.exposureTime[1] = scanner->val[OPT_SET_EXPOSURE_G].w;
        scanner->settings.exposureTime[2] = scanner->val[OPT_SET_EXPOSURE_B].w;
        scanner->settings.exposureTime[3] = scanner->val[OPT_SET_EXPOSURE_I].w; /* Infrared */
        /* Offsets */
        scanner->settings.offset[0] = scanner->val[OPT_SET_OFFSET_R].w;
        scanner->settings.offset[1] = scanner->val[OPT_SET_OFFSET_G].w;
        scanner->settings.offset[2] = scanner->val[OPT_SET_OFFSET_B].w;
        scanner->settings.offset[3] = scanner->val[OPT_SET_OFFSET_I].w; /* Infrared */
        /* Gains */
        scanner->settings.gain[0] = scanner->val[OPT_SET_GAIN_R].w;
        scanner->settings.gain[1] = scanner->val[OPT_SET_GAIN_G].w;
        scanner->settings.gain[2] = scanner->val[OPT_SET_GAIN_B].w;
        scanner->settings.gain[3] = scanner->val[OPT_SET_GAIN_I].w; /* Infrared */
        /* Light, extra entries and doubling */
        scanner->settings.light = scanner->val[OPT_LIGHT].w;
        scanner->settings.extraEntries = DEFAULT_ADDITIONAL_ENTRIES;
        scanner->settings.doubleTimes = scanner->val[OPT_DOUBLE_TIMES].w;
        status.pieusb_status = PIEUSB_STATUS_GOOD;
    } else { /* SCAN_CALIBRATION_AUTO */
        DBG (DBG_info_sane, "sanei_pieusb_set_gain_offset(): get calibration data from scanner\n");
        sanei_pieusb_cmd_get_gain_offset (scanner->device_number, &scanner->settings, &status);
    }
    /* Check status */
    if (status.pieusb_status == PIEUSB_STATUS_DEVICE_BUSY) {
      ret = sanei_pieusb_wait_ready (scanner, 0);
      if (ret != SANE_STATUS_GOOD) {
	DBG (DBG_error,"sanei_pieusb_set_gain_offset(): not ready after sanei_pieusb_cmd_get_gain_offset(): %d\n", ret);
	return ret;
      }
    }
    else if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
        return SANE_STATUS_INVAL;
    }
    /* Adjust gain */
    gain = 1.0;
    if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_03) == 0) {
        gain = 0.3;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_05) == 0) {
        gain = 0.5;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_08) == 0) {
        gain = 0.8;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_10) == 0) {
        gain = 1.0;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_12) == 0) {
        gain = 1.2;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_16) == 0) {
        gain = 1.6;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_19) == 0) {
        gain = 1.9;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_24) == 0) {
        gain = 2.4;
    } else if (strcmp (scanner->val[OPT_GAIN_ADJUST].s, SCAN_GAIN_ADJUST_30) == 0) {
        gain = 3.0;
    }
    switch (scanner->mode.passes) {
        case SCAN_ONE_PASS_RGBI:
        case SCAN_ONE_PASS_COLOR:
            updateGain2 (scanner, 0, gain);
            updateGain2 (scanner, 1, gain);
            updateGain2 (scanner, 2, gain);
            /* Don't correct IR, hampers cleaning process... */
            break;
        case SCAN_FILTER_INFRARED:
            updateGain2 (scanner, 3, gain);
            break;
        case SCAN_FILTER_BLUE:
            updateGain2 (scanner, 2, gain);
            break;
        case SCAN_FILTER_GREEN:
            updateGain2 (scanner, 1, gain);
            break;
        case SCAN_FILTER_RED:
            updateGain2 (scanner, 0, gain);
            break;
        case SCAN_FILTER_NEUTRAL:
            break;
    }
    /* Now set values for gain, offset and exposure */
    sanei_pieusb_cmd_set_gain_offset (scanner->device_number, &(scanner->settings), &status);
    ret = sanei_pieusb_convert_status (status.pieusb_status);
    DBG (DBG_info_sane, "sanei_pieusb_set_gain_offset(): status %s\n", sane_strstatus (ret));
    return ret;
}

/*
 * get shading data
 * must be called immediately after sanei_pieusb_set_gain_offset
 */

SANE_Status
sanei_pieusb_get_shading_data(Pieusb_Scanner * scanner)
{
    struct Pieusb_Command_Status status;
    SANE_Int shading_width;
    SANE_Int shading_height;
    SANE_Byte* buffer;
    SANE_Int lines;
    SANE_Int cols;
    SANE_Int size;
    SANE_Status res = SANE_STATUS_GOOD;

    DBG (DBG_info_sane, "sanei_pieusb_get_shading_data()\n");
    shading_width = scanner->device->shading_parameters[0].pixelsPerLine;
    shading_height = scanner->device->shading_parameters[0].nLines;
    if (shading_height < 1) {
        DBG (DBG_error, "shading_height < 1\n");
	return SANE_STATUS_INVAL;
    }
    switch (scanner->mode.colorFormat) {
        case SCAN_COLOR_FORMAT_PIXEL: /* Pixel */
            lines = shading_height * 4;
            cols = 2 * shading_width;
            break;
        case SCAN_COLOR_FORMAT_INDEX: /* Indexed */
            lines = shading_height * 4;
            cols = (2 * shading_width + 2);
            break;
        default:
            DBG (DBG_error, "sanei_pieusb_get_shading_data(): color format %d not implemented\n", scanner->mode.colorFormat);
            return SANE_STATUS_INVAL;
    }

    size = cols * lines;
    buffer = malloc (size);
    if (buffer == NULL) {
        return SANE_STATUS_NO_MEM;
    }
    sanei_pieusb_cmd_get_scanned_lines (scanner->device_number, buffer, 4, cols * 4, &status);
    if (status.pieusb_status == PIEUSB_STATUS_GOOD) {
        res = sanei_pieusb_wait_ready (scanner, 0);
        if (res == SANE_STATUS_GOOD) {
            sanei_pieusb_cmd_get_scanned_lines (scanner->device_number, buffer + cols*4, lines - 4, (lines - 4) * cols, &status);
	    if (status.pieusb_status == PIEUSB_STATUS_GOOD) {
	        pieusb_calculate_shading (scanner, buffer);
	    }
	    res = sanei_pieusb_convert_status (status.pieusb_status);
	}
    }
    else {
        res = sanei_pieusb_convert_status (status.pieusb_status);
    }
    free (buffer);
    return res;
}

/*
 *
 */

SANE_Status
sanei_pieusb_get_ccd_mask(Pieusb_Scanner * scanner)
{
    struct Pieusb_Command_Status status;

    DBG(DBG_info_proc, "sanei_pieusb_get_ccd_mask()\n");

    sanei_pieusb_cmd_get_ccd_mask(scanner->device_number, scanner->ccd_mask, scanner->ccd_mask_size, &status);
    if (status.pieusb_status == PIEUSB_STATUS_GOOD) {
      /* Save CCD mask */
      if (scanner->val[OPT_SAVE_CCDMASK].b) {
        FILE* fs = fopen ("pieusb.ccd", "w");
        fwrite (scanner->ccd_mask, 1, scanner->ccd_mask_size, fs);
        fclose (fs);
      }
    }
  return sanei_pieusb_convert_status(status.pieusb_status);

}

/**
 * Read parameters from scanner
 * and initialize SANE parameters
 *
 * @param scanner
 * @return parameter_bytes for use in get_scan_data()
 */
SANE_Status
sanei_pieusb_get_parameters(Pieusb_Scanner * scanner, SANE_Int *parameter_bytes)
{
    struct Pieusb_Command_Status status;
    struct Pieusb_Scan_Parameters parameters;
    const char *mode;

    DBG (DBG_info_proc, "sanei_pieusb_get_parameters()\n");

    sanei_pieusb_cmd_get_parameters (scanner->device_number, &parameters, &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
        return sanei_pieusb_convert_status (status.pieusb_status);
    }
    *parameter_bytes = parameters.bytes;
    /* Use response from sanei_pieusb_cmd_get_parameters() for initialization of SANE parameters.
     * Note the weird values of the bytes-field: this is because of the colorFormat
     * setting in sanei_pieusb_cmd_set_mode(). The single-color modes all use the pixel format,
     * which makes sanei_pieusb_cmd_get_parameters() return a full color line although just
     * one color actually contains data. For the index format, the bytes field
     * gives the size of a single color line. */
    mode = scanner->val[OPT_MODE].s;
    if (strcmp (mode, SANE_VALUE_SCAN_MODE_LINEART) == 0) {
        scanner->scan_parameters.format = SANE_FRAME_GRAY;
        scanner->scan_parameters.depth = 1;
        scanner->scan_parameters.bytes_per_line = parameters.bytes/3;
    } else if (strcmp (mode, SANE_VALUE_SCAN_MODE_HALFTONE) == 0) {
        scanner->scan_parameters.format = SANE_FRAME_GRAY;
        scanner->scan_parameters.depth = 1;
        scanner->scan_parameters.bytes_per_line = parameters.bytes/3;
    } else if (strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY) == 0) {
        scanner->scan_parameters.format = SANE_FRAME_GRAY;
        scanner->scan_parameters.depth = scanner->val[OPT_BIT_DEPTH].w;
        scanner->scan_parameters.bytes_per_line = parameters.bytes/3;
    } else if (strcmp (mode, SANE_VALUE_SCAN_MODE_RGBI) == 0) {
        scanner->scan_parameters.format = SANE_FRAME_RGB; /* was: SANE_FRAME_RGBI */
        scanner->scan_parameters.depth = scanner->val[OPT_BIT_DEPTH].w;
        scanner->scan_parameters.bytes_per_line = 4*parameters.bytes;
    } else { /* SANE_VALUE_SCAN_MODE_COLOR, with and without option clean image set */
        scanner->scan_parameters.format = SANE_FRAME_RGB;
        scanner->scan_parameters.depth = scanner->val[OPT_BIT_DEPTH].w;
        scanner->scan_parameters.bytes_per_line = 3*parameters.bytes;
    }
    scanner->scan_parameters.lines = parameters.lines;
    scanner->scan_parameters.pixels_per_line = parameters.width;
    scanner->scan_parameters.last_frame = SANE_TRUE;

    DBG (DBG_info_sane,"sanei_pieusb_get_parameters(): mode '%s'\n", mode);
    DBG (DBG_info_sane," format = %d\n", scanner->scan_parameters.format);
    DBG (DBG_info_sane," depth = %d\n", scanner->scan_parameters.depth);
    DBG (DBG_info_sane," bytes_per_line = %d\n", scanner->scan_parameters.bytes_per_line);
    DBG (DBG_info_sane," lines = %d\n", scanner->scan_parameters.lines);
    DBG (DBG_info_sane," pixels_per_line = %d\n", scanner->scan_parameters.pixels_per_line);
    DBG (DBG_info_sane," last_frame = %d\n", scanner->scan_parameters.last_frame);

    return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pieusb_get_scan_data(Pieusb_Scanner * scanner, SANE_Int parameter_bytes)
{
    struct Pieusb_Command_Status status;
    SANE_Parameters *parameters = &scanner->scan_parameters;
    SANE_Int lines_to_read, lines_remaining;
    SANE_Int ppl, bpl;
    SANE_Byte *linebuf, *lboff;
    SANE_Bool compress;
    int n, k, i;

    switch (scanner->mode.colorFormat) {
        case SCAN_COLOR_FORMAT_PIXEL: /* Pixel */
            lines_to_read = scanner->buffer.height;
            break;
        case SCAN_COLOR_FORMAT_INDEX: /* Indexed */
            lines_to_read = scanner->buffer.colors * scanner->buffer.height;
            break;
        default:
            DBG(DBG_error, "sanei_pieusb_get_scan_data(): color format %d not implemented\n",scanner->mode.colorFormat);
            return SANE_STATUS_INVAL;
    }
    lines_remaining = lines_to_read;
    DBG (DBG_info_proc, "sanei_pieusb_get_scan_data(colorFormat %d), lines_to_read %d, bytes %d\n", scanner->mode.colorFormat, lines_to_read, parameter_bytes);

  /*
    fdraw = open("/tmp/pieusb.raw", O_WRONLY | O_CREAT | O_TRUNC, (mode_t)0600);
    if (fdraw == -1) {
         perror("error opening raw image buffer file");
    }
*/
    while (lines_remaining > 0) {
        SANE_Int lines;
        /* Read lines */
        /* The amount of bytes_per_line varies with color format setting; only 'pixel' and 'index' implemented */
        ppl = parameters->pixels_per_line;
        switch (scanner->mode.colorFormat) {
	    case SCAN_COLOR_FORMAT_PIXEL: /* Pixel */
	        bpl = parameter_bytes;
	        break;
            case SCAN_COLOR_FORMAT_INDEX: /* Indexed */
	        bpl = parameter_bytes + 2; /* Index bytes! */
	        break;
            default:
	        DBG(DBG_error, "sanei_pieusb_get_scan_data(): color format %d not implemented\n", scanner->mode.colorFormat);
	        return SANE_STATUS_INVAL;
        }
        lines = (lines_remaining < 256) ? lines_remaining : 255;
        DBG(DBG_info_sane, "sanei_pieusb_get_scan_data(): reading lines: now %d, bytes per line = %d\n", lines, bpl);
        linebuf = malloc(lines * bpl);
        sanei_pieusb_cmd_get_scanned_lines(scanner->device_number, linebuf, lines, lines * bpl, &status);
        if (status.pieusb_status != PIEUSB_STATUS_GOOD ) {
	    /* Error, return */
	    free(linebuf);
	    return SANE_STATUS_INVAL;
	}
        /* Save raw data */
/*
        if (fdraw != -1) {
            wcnt = write(fdraw,linebuf,parameters.lines*bpl);
            DBG(DBG_info_sane,"Raw written %d\n",wcnt);
        }
*/
        /* Copy into official buffer
	 * Sometimes the scanner returns too many lines. Take care not to
	 * overflow the buffer. */
        lboff = linebuf;
        switch (scanner->mode.colorFormat) {
	    case SCAN_COLOR_FORMAT_PIXEL:
	        /* The scanner may return lines with 3 colors even though only
		 * one color is actually scanned. Detect this situation and
		 * eliminate the excess samples from the line buffer before
		 * handing it to buffer_put_full_color_line(). */
	        compress = SANE_FALSE;
	        if (scanner->buffer.colors == 1
		    && (bpl * scanner->buffer.packing_density / ppl) == (3 * scanner->buffer.packet_size_bytes)) {
		    compress = SANE_TRUE;
	        }
	        for (n = 0; n < lines; n++) {
		     if (compress) {
		         /* Move samples to fill up all unused locations */
		         int ps = scanner->buffer.packet_size_bytes;
		         for (k = 0; k < scanner->buffer.line_size_packets; k++) {
			     for (i = 0; i < ps; i++) {
			         lboff[ps*k+i] = lboff[3*ps*k+i];
			     }
			 }
		     }
		     if (sanei_pieusb_buffer_put_full_color_line(&scanner->buffer, lboff, bpl/3) == 0) {
		         /* Error, return */
		         return SANE_STATUS_INVAL;
		     }
		     lboff += bpl;
	        }
	        break;
	    case SCAN_COLOR_FORMAT_INDEX:
	        /* Indexed data */
	        for (n = 0; n < lines; n++) {
		    if (sanei_pieusb_buffer_put_single_color_line(&scanner->buffer, *lboff, lboff+2, bpl-2) == 0) {
		        /* Error, return */
		        return SANE_STATUS_INVAL;
		    }
		    lboff += bpl;
	        }
	        break;
	    default:
	        DBG(DBG_error, "sanei_pieusb_get_scan_data(): store color format %d not implemented\n", scanner->mode.colorFormat);
	        free(linebuf);
	        return SANE_STATUS_INVAL;
        }
        free(linebuf);
        lines_remaining -= lines; /* Note: excess discarded */
        DBG(DBG_info_sane, "sanei_pieusb_get_scan_data(): reading lines: remaining %d\n", lines_remaining);
    }
/*
    if (fdraw != -1) close(fdraw);
*/
    return SANE_STATUS_GOOD;
}

/**
 * Wait for scanner to get ready
 *
 * loop of test_ready/read_state
 *
 * @param scanner
 * @param device_number, used if scanner == NULL
 * @return SANE_Status
 */

SANE_Status
sanei_pieusb_wait_ready(Pieusb_Scanner * scanner, SANE_Int device_number)
{
  struct Pieusb_Command_Status status;
  struct Pieusb_Scanner_State state;
  time_t start, elapsed;

  DBG (DBG_info_proc, "sanei_pieusb_wait_ready()\n");
  start = time(NULL);
  if (scanner)
    device_number = scanner->device_number;

  for(;;) {
    sanei_pieusb_cmd_test_unit_ready(device_number, &status);
    DBG (DBG_info_proc, "-> sanei_pieusb_cmd_test_unit_ready: %d\n", status.pieusb_status);
    if (status.pieusb_status == PIEUSB_STATUS_GOOD)
      break;
    if (status.pieusb_status == PIEUSB_STATUS_IO_ERROR)
      break;
    sanei_pieusb_cmd_read_state(device_number, &state, &status);
    DBG (DBG_info_proc, "-> sanei_pieusb_cmd_read_state: %d\n", status.pieusb_status);
    if (status.pieusb_status != PIEUSB_STATUS_DEVICE_BUSY)
      break;
    sleep(2);
    elapsed = time(NULL) - start;
    if (elapsed > 120) { /* 2 minute overall timeout */
      DBG (DBG_error, "scanner not ready after 2 minutes\n");
      break;
    }
    if (elapsed % 2) {
      DBG (DBG_info, "still waiting for scanner to get ready\n");
    }
  }
  return sanei_pieusb_convert_status(status.pieusb_status);
}


SANE_Status sanei_pieusb_analyze_preview(Pieusb_Scanner * scanner)
{
    int k, n;
    SANE_Parameters params;
    SANE_Int N;
    double *norm_histo;
    double level;

    DBG(DBG_info, "sanei_pieusb_analyze_preview(): saving preview data\n");

    /* Settings */
    scanner->preview_done = SANE_TRUE;
    for (k = 0; k < 4; k++) {
        scanner->preview_exposure[k] = scanner->settings.exposureTime[k];
        scanner->preview_gain[k] = scanner->settings.gain[k];
        scanner->preview_offset[k] = scanner->settings.offset[k];
    }
    /* Analyze color planes */
    N = scanner->buffer.width * scanner->buffer.height;
    params.format = SANE_FRAME_GRAY;
    params.depth = scanner->buffer.depth;
    params.pixels_per_line = scanner->buffer.width;
    params.lines = scanner->buffer.height;
    for (k = 0; k < scanner->buffer.colors; k++) {
        /* Create histogram for color k */
        sanei_ir_create_norm_histogram (&params, scanner->buffer.data + k * N, &norm_histo);
        /* Find 1% and 99% limits */
        level = 0;
        for (n =0; n < HISTOGRAM_SIZE; n++) {

            level += norm_histo[n];
            if (level < 0.01) {
                scanner->preview_lower_bound[k] = n;
            }
            if (level < 0.99) {
                scanner->preview_upper_bound[k] = n;
            }
        }
        DBG(DBG_info,"sanei_pieusb_analyze_preview(): 1%%-99%% levels for color %d: %d - %d\n", k, scanner->preview_lower_bound[k], scanner->preview_upper_bound[k]);
    }
    /* Disable remaining color planes */
    for (k = scanner->buffer.colors; k < 4; k++) {
        scanner->preview_lower_bound[k] = 0;
        scanner->preview_upper_bound[k] = 0;
    }
    return SANE_STATUS_GOOD;
}


/**
 * Return actual gain at given gain setting
 *
 * @param gain Gain setting (0 - 63)
 * @return
 */
static double getGain(int gain)
{
    int k;

    /* Actually an error, but don't be picky */
    if (gain <= 0) {
        return gains[0];
    }
    /* A gain > 63 is also an error, but don't be picky */
    if (gain >= 60) {
        return (gain-55)*(gains[12]-gains[11])/5 + gains[11];
    }
    /* Interpolate other values */
    k = gain/5; /* index of array value just below given gain */
    return (gain-5*k)*(gains[k+1]-gains[k])/5 + gains[k];
}

static int getGainSetting(double gain)
{
    int k, m;

    /* Out of bounds */
    if (gain < 1.0) {
        return 0;
    }
    if (gain >= gains[12]) {
        m = 60 + lround((gain-gains[11])/(gains[12]-gains[11])*5);
        if (m > 63) m = 63;
        return m;
    }
    /* Interpolate the rest */
    m = 0;
    for (k = 0; k <= 11; k++) {
        if (gains[k] <= gain && gain < gains[k+1]) {
            m = 5*k + lround((gain-gains[k])/(gains[k+1]-gains[k])*5);
        }
    }
    return m;
}

/**
 * Modify gain and exposure times in order to make maximal use of the scan depth.
 * Each color treated separately, infrared excluded.
 *
 * This may be too aggressive => leads to a noisy whitish border instead of the orange.
 * In a couuple of tries, gain was set to values of 60 and above, which introduces
 * the noise?
 * The whitish border is logical since the brightest parts of the negative, the
 * unexposed borders, are amplified to values near CCD saturation, which is white.
 * Maybe a uniform gain increase for each color is more appropriate? Somewhere
 * between 2.5 and 3 seems worthwhile trying, see updateGain2().
 *
        switch (scanner->mode.passes) {
            case SCAN_ONE_PASS_RGBI:
                updateGain(scanner,0);
                updateGain(scanner,1);
                updateGain(scanner,2);
                updateGain(scanner,3);
                break;
            case SCAN_ONE_PASS_COLOR:
                updateGain(scanner,0);
                updateGain(scanner,1);
                updateGain(scanner,2);
                break;
            case SCAN_FILTER_INFRARED:
                updateGain(scanner,3);
                break;
            case SCAN_FILTER_BLUE:
                updateGain(scanner,2);
                break;
            case SCAN_FILTER_GREEN:
                updateGain(scanner,1);
                break;
            case SCAN_FILTER_RED:
                updateGain(scanner,0);
                break;
            case SCAN_FILTER_NEUTRAL:
                break;
        }
 * @param scanner
 */
/*
static void updateGain(Pieusb_Scanner *scanner, int color_index)
{
    double g, dg;

    DBG(DBG_info_sane,"updateGain(): color %d preview used G=%d Exp=%d\n", color_index, scanner->preview_gain[color_index], scanner->preview_exposure[color_index]);
    // Additional gain to obtain
    dg = ((double)scanner->settings.saturationLevel[color_index] / 65536) / ((double)scanner->preview_upper_bound[color_index] / HISTOGRAM_SIZE);
    DBG(DBG_info_sane,"updateGain(): additional gain %f\n", dg);
    // Achieve this by modifying gain and exposure
    // Gain used for preview
    g = getGain(scanner->preview_gain[color_index]);
    DBG(DBG_info_sane,"updateGain(): preview had gain %d => %f\n",scanner->preview_gain[color_index],g);
    // Look up new gain setting g*sqrt(dg)
    DBG(DBG_info_sane,"updateGain(): optimized gain * %f = %f\n",sqrt(dg),sqrt(dg)*g);
    scanner->settings.gain[color_index] = getGainSetting(g*sqrt(dg));
    DBG(DBG_info_sane,"updateGain(): optimized gain setting %d => %f\n",scanner->settings.gain[color_index],getGain(scanner->settings.gain[color_index]));
    // Exposure change is straightforward
    DBG(DBG_info_sane,"updateGain(): remains for exposure %f\n",dg/(getGain(scanner->settings.gain[color_index])/g));
    scanner->settings.exposureTime[color_index] = lround( g / getGain(scanner->settings.gain[color_index]) * dg * scanner->preview_exposure[color_index] );
    DBG(DBG_info_sane,"updateGain(): new setting G=%d Exp=%d\n", scanner->settings.gain[color_index], scanner->settings.exposureTime[color_index]);
}
*/

static void updateGain2(Pieusb_Scanner *scanner, int color_index, double gain_increase)
{
    double g;

    DBG(DBG_info,"updateGain2(): color %d preview used G=%d Exp=%d\n", color_index, scanner->settings.gain[color_index], scanner->settings.exposureTime[color_index]);
    /* Additional gain to obtain */
    DBG(DBG_info,"updateGain2(): additional gain %f\n", gain_increase);
    /* Achieve this by modifying gain and exposure */
    /* Gain used for preview */
    g = getGain(scanner->settings.gain[color_index]);
    DBG(DBG_info,"updateGain2(): preview had gain %d => %f\n", scanner->settings.gain[color_index], g);
    /* Look up new gain setting g*sqrt(dg) */
    DBG(DBG_info,"updateGain2(): optimized gain * %f = %f\n", sqrt(gain_increase), sqrt(gain_increase) * g);
    scanner->settings.gain[color_index] = getGainSetting(g * sqrt(gain_increase));
    DBG(DBG_info,"updateGain2(): optimized gain setting %d => %f\n", scanner->settings.gain[color_index], getGain(scanner->settings.gain[color_index]));
    /* Exposure change is straightforward */
    DBG(DBG_info,"updateGain2(): remains for exposure %f\n", gain_increase / (getGain(scanner->settings.gain[color_index]) / g));
    scanner->settings.exposureTime[color_index] = lround( g / getGain(scanner->settings.gain[color_index]) * gain_increase * scanner->settings.exposureTime[color_index] );
    DBG(DBG_info,"updateGain2(): new setting G=%d Exp=%d\n", scanner->settings.gain[color_index], scanner->settings.exposureTime[color_index]);
}
