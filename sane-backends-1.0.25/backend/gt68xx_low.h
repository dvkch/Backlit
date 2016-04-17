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

#ifndef GT68XX_LOW_H
#define GT68XX_LOW_H

/** @file
 * @brief Low-level scanner interface functions.
 */

#include "../include/sane/sane.h"

#include <stddef.h>

#ifdef USE_FORK
#include <sys/types.h>
#include "gt68xx_shm_channel.h"
#endif

#ifdef NDEBUG
#undef MAX_DEBUG
#endif

/* calculate the minimum/maximum values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/* return the lower/upper 8 bits of a 16 bit word */
#define HIBYTE(w) ((SANE_Byte)(((SANE_Word)(w) >> 8) & 0xFF))
#define LOBYTE(w) ((SANE_Byte)(w))


/* return if an error occured while the function was called */
#ifdef MAX_DEBUG
#  ifndef __FUNCTION__
#    define __FUNCTION__ "somewhere"
#  endif

#  define RIE(function) \
  do \
    { \
      status = function; \
      if (status != SANE_STATUS_GOOD) \
        { \
          DBG (7, "%s: %s: %s\n", __FUNCTION__, STRINGIFY(function), \
               sane_strstatus (status)); \
          return status; \
        } \
    } \
  while (SANE_FALSE)

#else

#  define RIE(function)                                   \
  do { status = function;                               \
    if (status != SANE_STATUS_GOOD) return status;      \
  } while (SANE_FALSE)

#endif

/* Flags */
#define GT68XX_FLAG_MIRROR_X	    (1 << 0)	/* CIS unit mounted the other way round? */
#define GT68XX_FLAG_MOTOR_HOME	    (1 << 1)	/* Use motor_home command (0x34) */
#define GT68XX_FLAG_OFFSET_INV      (1 << 2)	/* Offset control is inverted */
#define GT68XX_FLAG_UNTESTED        (1 << 3)	/* Print a warning for these scanners */
#define GT68XX_FLAG_SE_2400         (1 << 4)	/* Special quirks for SE 2400USB */
#define GT68XX_FLAG_NO_STOP         (1 << 5)	/* Don't call stop_scan before the scan */
#define GT68XX_FLAG_CIS_LAMP        (1 << 6)	/* CIS sensor with lamp */
#define GT68XX_FLAG_NO_POWER_STATUS (1 << 7)	/* get_power_status_doesn't work */
#define GT68XX_FLAG_NO_LINEMODE     (1 << 8)	/* Linemode does not work with this scanner */
#define GT68XX_FLAG_SCAN_FROM_HOME  (1 << 9)	/* Move home after calibration */
#define GT68XX_FLAG_USE_OPTICAL_X   (1 << 10)	/* Use optical xdpi for 50 dpi and below */
#define GT68XX_FLAG_ALWAYS_LINEMODE (1 << 11)	/* Linemode must be used for any resolution */
#define GT68XX_FLAG_SHEET_FED       (1 << 12)	/* we have a sheet fed scanner */
#define GT68XX_FLAG_HAS_CALIBRATE   (1 << 13)	/* for sheet fed scanners that be calibrated with
                                                   an calibration sheet */

/* Forward typedefs */
typedef struct GT68xx_USB_Device_Entry GT68xx_USB_Device_Entry;
typedef struct GT68xx_Command_Set GT68xx_Command_Set;
typedef struct GT68xx_Model GT68xx_Model;
typedef struct GT68xx_Device GT68xx_Device;
typedef struct GT68xx_Scan_Request GT68xx_Scan_Request;
typedef struct GT68xx_Scan_Parameters GT68xx_Scan_Parameters;
typedef struct GT68xx_AFE_Parameters GT68xx_AFE_Parameters;
typedef struct GT68xx_Exposure_Parameters GT68xx_Exposure_Parameters;

typedef enum GT68xx_Color_Order
{
  COLOR_ORDER_RGB,
  COLOR_ORDER_BGR
}
GT68xx_Color_Order;

#define GT68XX_COLOR_RED SANE_I18N ("Red")
#define GT68XX_COLOR_GREEN SANE_I18N ("Green")
#define GT68XX_COLOR_BLUE SANE_I18N ("Blue")

/** Scan action code (purpose of the scan).
 *
 * The scan action code affects various scanning mode fields in the setup
 * command.
 */
typedef enum GT68xx_Scan_Action
{
  SA_CALIBRATE,			/**< Calibration scan, no offsets, no LD
				   correction */
  SA_CALIBRATE_ONE_LINE,	/**< Same, but only one line for auto AFE */
  SA_SCAN			/**< Normal scan */
}
GT68xx_Scan_Action;

/** USB device list entry. */
struct GT68xx_USB_Device_Entry
{
  SANE_Word vendor;			/**< USB vendor identifier */
  SANE_Word product;			/**< USB product identifier */
  GT68xx_Model *model;			/**< Scanner model information */
};

#define MAX_SCANNERS 50
/* Looks like gcc doesn't like declarations without specificating the number
   of array elements at least when --enable-warnings is active */

/** List of all supported devices.
 *
 * This is an array of GT68xx_USB_Device_Entry structures which describe 
 * USB devices supported by this backend.  The array is terminated by an
 * entry with model = NULL.
 *
 */
static GT68xx_USB_Device_Entry gt68xx_usb_device_list[MAX_SCANNERS];

/** GT68xx analog front-end (AFE) parameters.
 */
struct GT68xx_AFE_Parameters
{
  SANE_Byte r_offset;	/**< Red channel offset */
  SANE_Byte r_pga;	/**< Red channel PGA gain */
  SANE_Byte g_offset;	/**< Green channel offset (also used for mono) */
  SANE_Byte g_pga;	/**< Green channel PGA gain (also used for mono) */
  SANE_Byte b_offset;	/**< Blue channel offset */
  SANE_Byte b_pga;	/**< Blue channel PGA gain */
};

/** GT68xx exposure time parameters.
 */
struct GT68xx_Exposure_Parameters
{
  SANE_Int r_time;     /**< Red exposure time */
  SANE_Int g_time;     /**< Red exposure time */
  SANE_Int b_time;     /**< Red exposure time */
};


/**
 * Scanner command set description.
 *
 * This description contains parts which are common to all scanners with the
 * same command set, but may have different optical resolution and other
 * parameters.
 */
struct GT68xx_Command_Set
{
  /** @name Identification */
  /*@{ */

  /** Name of this command set */
  SANE_String_Const name;

  /*@} */

  /** @name USB request parameters
   *
   * These values are used in the USB control transfer parameters (wValue and
   * wIndex fields, as in the USB specification).
   */
  /*@{ */

  SANE_Byte request_type;		/**< Request type (should be 0x40, vendor spec) */
  SANE_Byte request;			/**< Vendor spec resquest (0x01 or 0x04) */
  SANE_Word memory_read_value;		/**< Memory read - wValue */
  SANE_Word memory_write_value;		/**< Memory write - wValue */
  SANE_Word send_cmd_value;		/**< Send normal command - wValue */
  SANE_Word send_cmd_index;		/**< Send normal command - wIndex */
  SANE_Word recv_res_value;		/**< Receive normal result - wValue */
  SANE_Word recv_res_index;		/**< Receive normal result - wIndex */
  SANE_Word send_small_cmd_value;	/**< Send small command - wValue */
  SANE_Word send_small_cmd_index;	/**< Send small command - wIndex */
  SANE_Word recv_small_res_value;	/**< Receive small result - wValue */
  SANE_Word recv_small_res_index;	/**< Receive small result - wIndex */

  /*@} */

  /** @name Activation/deactivation hooks
   *
   * These hooks can be used to perform additional actions when the device is
   * activated and deactivated.
   */
  /*@{ */

  /** Activate the device.
   *
   * This function may allocate a command-set-specific data structure and place
   * the pointer to it into the GT68xx_Device::command_set_private field.
   */
    SANE_Status (*activate) (GT68xx_Device * dev);

  /** Deactivate the device.
   *
   * If the activate function has allocated a command-set-specific data
   * structure, this function must free all corresponding resources and set the
   * GT68xx_Device::command_set_private pointer to #NULL.
   */
    SANE_Status (*deactivate) (GT68xx_Device * dev);

  /*@} */

  /** @name Low-level command implementation functions
   *
   * These functions should implement the corresponding commands for the
   * particular command set.  If a function cannot be implemented because there
   * is no corresponding command in the command set, the function pointer
   * should be set to #NULL; the wrapper will return #SANE_STATUS_UNSUPPORTED
   * in this case.
   */
  /*@{ */

  /** Check whether the firmware is already downloaded.
   *
   * @param dev Device object.
   * @param loaded Returned firmware status:
   * - #SANE_TRUE - the firmware is already loaded.
   * - #SANE_FALSE - the firmware is not loaded.
   */
    SANE_Status (*check_firmware) (GT68xx_Device * dev, SANE_Bool * loaded);

  /** Download the firmware */
    SANE_Status (*download_firmware) (GT68xx_Device * dev,
				      SANE_Byte * data, SANE_Word size);

  /** Check whether the external power supply is connected.
   *
   * @param dev Device object.
   * @param power_ok Returned power status:
   * - #SANE_TRUE - the external power supply is connected, or the scanner does
   *   not need external power.
   * - #SANE_FALSE - the external power supply is not connected, so the scanner
   *   will not work.
   */
    SANE_Status (*get_power_status) (GT68xx_Device * dev,
				     SANE_Bool * power_ok);

  /** Check whether a transparency adapter is attached to the scanner.
   *
   * @param dev Device object.
   * @param ta_attached Returned transparency adapter status:
   * - #SANE_TRUE - the transparency adapter is connected.
   * - #SANE_FALSE - the transparency adapter is not connected.
   *
   * @return
   * - #SANE_STATUS_GOOD - transparency adapter status was checked
   *   successfully; check @a *ta_attached for the result.
   * - #SANE_STATUS_UNSUPPORTED - this scanner model does not support the
   *   transparency adapter.
   * */
    SANE_Status (*get_ta_status) (GT68xx_Device * dev,
				  SANE_Bool * ta_attached);

  /** Turn the lamps in the scanner and/or the transparency adapter on or off.
   *
   * @param dev Device object.
   * @param fb_lamp #SANE_TRUE turns on the flatbed lamp.
   * @param ta_lamp #SANE_TRUE turns on the transparency adapter lamp.
   *
   * @return
   * - #SANE_STATUS_GOOD - the command completed successfully.
   * - #SANE_STATUS_UNSUPPORTED - unsupported request was made (like attempt to
   *   turn on the TA lamp on a scanner which does not support TA).
   */
    SANE_Status (*lamp_control) (GT68xx_Device * dev, SANE_Bool fb_lamp,
				 SANE_Bool ta_lamp);

  /** Check whether the scanner carriage is still moving. 
   *
   * @param dev Device object.
   * @param moving Returned state of the scanner:
   * - #SANE_TRUE  - the scanner carriage is still moving.
   * - #SANE_FALSE - the scanner carriage has stopped.
   *
   * @return
   * - #SANE_STATUS_GOOD - the command completed successfully, the status in @a
   *   *moving is valid.
   */
    SANE_Status (*is_moving) (GT68xx_Device * dev, SANE_Bool * moving);


  /** Move the scanner carriage by the specified number of steps.
   *
   * @param dev Device object.
   * @param distance Number of steps to move (positive to move forward,
   * negative to move backward).  The measurement unit is model-dependent;
   * number of steps per inch is found in the GT68xx_Model::base_ydpi field.
   *
   * @return
   * - #SANE_STATUS_GOOD - the command completed successfully; the movement is
   *   started.  Call gt68xx_device_is_moving() periodically to determine when
   *   the movement is complete.
   */
    SANE_Status (*move_relative) (GT68xx_Device * dev, SANE_Int distance);

  /** Move the scanner carriage to the home position.
   *
   * @param dev Device object.
   */
    SANE_Status (*carriage_home) (GT68xx_Device * dev);

  /** Eject the paper at the end of the scan.
   *
   * @param dev Device object.
   */
    SANE_Status (*paperfeed) (GT68xx_Device * dev);

  /** Start scanning the image.
   *
   * @param dev Device object.
   */
    SANE_Status (*start_scan) (GT68xx_Device * dev);

  /** Start reading the scanned image data from the scanner.
   *
   * @param dev Device object.
   * */
    SANE_Status (*read_scanned_data) (GT68xx_Device * dev, SANE_Bool * ready);

  /** Stop scanning the image and reading the data. */
    SANE_Status (*stop_scan) (GT68xx_Device * dev);

  /** Set parameters for the next scan. */
    SANE_Status (*setup_scan) (GT68xx_Device * dev,
			       GT68xx_Scan_Request * request,
			       GT68xx_Scan_Action action,
			       GT68xx_Scan_Parameters * params);

    SANE_Status (*set_afe) (GT68xx_Device * dev,
			    GT68xx_AFE_Parameters * params);

    SANE_Status (*set_exposure_time) (GT68xx_Device * dev,
				      GT68xx_Exposure_Parameters * params);

  /** Get the vendor, product and some more ids from the scanner */
    SANE_Status (*get_id) (GT68xx_Device * dev);

  /** Move the paper by the amount of y offset needed to reach scan area
   *
   * @param dev Device object.
   * @param request scan request used to compute move to reach scan area
   */
    SANE_Status (*move_paper) (GT68xx_Device * dev,
			       GT68xx_Scan_Request * request);

  /** Detect if a document is inserted in the feeder
   *
   * @param dev Device object.
   * @param present 
   */
    SANE_Status (*document_present) (GT68xx_Device * dev,
			             SANE_Bool *present);
  /*@} */
};

#define MAX_RESOLUTIONS 12
#define MAX_DPI 4

/** Model-specific scanner data.
 */
struct GT68xx_Model
{
  /** @name Identification */
  /*@{ */

  /** A single lowercase word to be used in the configuration file. */
  SANE_String_Const name;

  /** Device vendor string. */
  SANE_String_Const vendor;

  /** Device model name. */
  SANE_String_Const model;

  /** Name of the firmware file. */
  SANE_String_Const firmware_name;

  /** Dynamic allocation flag.
   *
   * This flag must be set to SANE_TRUE if the structure is dynamically
   * allocated; in this case the structure will be freed when the GT68xx_Device
   * is freed.
   */
  SANE_Bool allocated;
  /*@} */

  /** @name Scanner model parameters */
  /*@{ */

  GT68xx_Command_Set *command_set;

  SANE_Int optical_xdpi;	/* maximum resolution in x-direction */
  SANE_Int optical_ydpi;	/* maximum resolution in y-direction */
  SANE_Int base_xdpi;		/* x-resolution used to calculate geometry */
  SANE_Int base_ydpi;		/* y-resolution used to calculate geometry */
  SANE_Int ydpi_no_backtrack;	/* if ydpi is equal or higher, disable backtracking */
  SANE_Bool constant_ydpi;	/* Use base_ydpi for all resolutions        */

  SANE_Int xdpi_values[MAX_RESOLUTIONS];	/* possible x resolutions */
  SANE_Int ydpi_values[MAX_RESOLUTIONS];	/* possible y resolutions */
  SANE_Int bpp_gray_values[MAX_DPI];	/* possible depths in gray mode */
  SANE_Int bpp_color_values[MAX_DPI];	/* possible depths in color mode */

  SANE_Fixed x_offset;		/* Start of scan area in mm */
  SANE_Fixed y_offset;		/* Start of scan area in mm */
  SANE_Fixed x_size;		/* Size of scan area in mm */
  SANE_Fixed y_size;		/* Size of scan area in mm */

  SANE_Fixed y_offset_calib;	/* Start of white strip in mm */
  SANE_Fixed x_offset_mark;	/* Start of black mark in mm */

  SANE_Fixed x_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed y_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed x_size_ta;		/* Size of scan area in TA mode in mm */
  SANE_Fixed y_size_ta;		/* Size of scan area in TA mode in mm */

  SANE_Fixed y_offset_calib_ta;	/* Start of white strip in TA mode in mm */

  /* Line-distance correction (in pixel at optical_ydpi) for CCD scanners */
  SANE_Int ld_shift_r;		/* red */
  SANE_Int ld_shift_g;		/* green */
  SANE_Int ld_shift_b;		/* blue */
  SANE_Int ld_shift_double;	/* distance between two CCD lines of one color
				   (only applicable for CCD with 6 lines) */

  GT68xx_Color_Order line_mode_color_order;	/* Order of the CCD/CIS colors */

  GT68xx_AFE_Parameters afe_params;	/* Default offset/gain */
  GT68xx_Exposure_Parameters exposure;	/* Default exposure parameters */
  SANE_Fixed default_gamma_value;	/* Default gamma value */

  SANE_Bool is_cis;		/* Is this a CIS or CCD scanner? */

  SANE_Word flags;		/* Which hacks are needed for this scanner? */
  /*@} */
};

/** GT68xx device instance.
 *
 */
struct GT68xx_Device
{
  /** Device file descriptor. */
  int fd;

  /** Device activation flag. */
  SANE_Bool active;

  /** Device missing to flag devices that are unplugged
   * after sane_init and befor sane_exit */
  SANE_Bool missing;

  /** Scanner model data. */
  GT68xx_Model *model;

  /** Pointer to command-set-specific data. */
  void *command_set_private;

  GT68xx_AFE_Parameters *afe;
  GT68xx_Exposure_Parameters *exposure;
  SANE_Fixed gamma_value;

  SANE_Bool read_active;
  SANE_Bool final_scan;
  SANE_Byte *read_buffer;
  size_t requested_buffer_size;
  size_t read_buffer_size;
  size_t read_pos;
  size_t read_bytes_in_buffer;
  size_t read_bytes_left;
  SANE_Byte gray_mode_color;
  SANE_Bool manual_selection;
#ifdef USE_FORK
  Shm_Channel *shm_channel;
  pid_t reader_pid;
#endif				/* USE_FORK */

  /** Pointer to next device */
  struct GT68xx_Device *next;

  /** Device file name */
  SANE_String file_name;
};

/** Parameters for the high-level scan request.
 *
 * These parameters describe the scan request sent by the SANE frontend.
 */
struct GT68xx_Scan_Request
{
  SANE_Fixed x0;	/**< Left boundary  */
  SANE_Fixed y0;	/**< Top boundary */
  SANE_Fixed xs;	/**< Width */
  SANE_Fixed ys;	/**< Height */
  SANE_Int xdpi;	/**< Horizontal resolution */
  SANE_Int ydpi;	/**< Vertical resolution */
  SANE_Int depth;	/**< Number of bits per channel */
  SANE_Bool color;	/**< Color mode flag */
  SANE_Bool mbs;	/**< Move before scan */
  SANE_Bool mds;	/**< Move during scan */
  SANE_Bool mas;	/**< Move after scan */
  SANE_Bool lamp;	/**< Lamp on/off */
  SANE_Bool calculate;	/**< Don't scan, only calculate parameters */
  SANE_Bool use_ta;	/**< Use the tansparency adapter */
  SANE_Bool backtrack;	/**< Enable backtracking */
  SANE_Bool backtrack_lines;  /**< How many lines to backtrack */
};

/** Scan parameters for gt68xx_device_setup_scan().
 *
 * These parameters describe a low-level scan request; many such requests are
 * executed during calibration, and they need to have parameters separate from
 * the main request (GT68xx_Scan_Request).  
 */
struct GT68xx_Scan_Parameters
{
  SANE_Int xdpi;	/**< Horizontal resolution */
  SANE_Int ydpi;	/**< Vertical resolution */
  SANE_Int depth;	/**< Number of bits per channel */
  SANE_Bool color;	/**< Color mode flag */

  SANE_Int pixel_xs;		/**< Logical width in pixels */
  SANE_Int pixel_ys;		/**< Logical height in pixels */
  SANE_Int scan_xs;		/**< Physical width in pixels */
  SANE_Int scan_ys;		/**< Physical height in pixels */
  SANE_Int scan_bpl;		/**< Number of bytes per scan line */
  SANE_Bool line_mode;		/**< Use line mode instead of pixel mode */
  SANE_Int overscan_lines;	/**< Number of extra scan lines */
  SANE_Int ld_shift_r;
  SANE_Int ld_shift_g;
  SANE_Int ld_shift_b;
  SANE_Int ld_shift_double;
  SANE_Int double_column;
  SANE_Int pixel_x0;		/**< x start postion */
};


#define GT68XX_PACKET_SIZE 64

typedef SANE_Byte GT68xx_Packet[GT68XX_PACKET_SIZE];

/** Create a new GT68xx_Device object.
 *
 * The newly created device object is in the closed state.
 *
 * @param dev_return Returned pointer to the created device object.
 *
 * @return
 * - #SANE_STATUS_GOOD   - the device object was created.
 * - #SANE_STATUS_NO_MEM - not enough system resources to create the object.
 */
static SANE_Status gt68xx_device_new (GT68xx_Device ** dev_return);

/** Destroy the device object and release all associated resources.
 *
 * If the device was active, it will be deactivated; if the device was open, it
 * will be closed.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD  - success.
 */
static SANE_Status gt68xx_device_free (GT68xx_Device * dev);

/** Open the scanner device.
 *
 * This function opens the device special file @a dev_name and tries to detect
 * the device model by its USB ID.
 *
 * If the device is detected successfully (its USB ID is found in the supported
 * device list), this function sets the appropriate model parameters.
 *
 * If the USB ID is not recognized, the device remains unconfigured; an attempt
 * to activate it will fail unless gt68xx_device_set_model() is used to force
 * the parameter set.  Note that the open is considered to be successful in
 * this case.
 *
 * @param dev Device object.
 * @param dev_name Scanner device name.
 *
 * @return
 * - #SANE_STATUS_GOOD - the device was opened successfully (it still may be
 *   unconfigured).
 */
static SANE_Status
gt68xx_device_open (GT68xx_Device * dev, const char *dev_name);

/** Close the scanner device.
 *
 * @param dev Device object.
 */
static SANE_Status gt68xx_device_close (GT68xx_Device * dev);

/** Check if the device is configured.
 *
 * A device is considered configured when it has a model parameters structure
 * (GT68xx_Model) and a command set (GT68xx_Command_Set).  Normally these
 * parameters are assigned automatically by gt68xx_device_open(), if the device
 * is known.  If the USB ID of the device is not found in the list, or the OS
 * does not support identification of USB devices, the device will be
 * unconfigured after opening.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_TRUE  - device is configured and can be activated.
 * - #SANE_FALSE - device is not configured; attempt to activate it will fail.
 */
static SANE_Bool gt68xx_device_is_configured (GT68xx_Device * dev);

/** Change the device model structure.
 *
 * This function can be used to change all model-dependent parameters at once
 * by supplying a whole model data structure.  The model may be changed only
 * when the device is not active.
 *
 * If the device already had a model structure which was dynamically allocated,
 * the old structure is freed.
 *
 * If the new model structure @a model is dynamically allocated, the device @a
 * dev takes ownership of it: @a model will be freed when @a dev is destroyed
 * or gt68xx_device_set_model() is called again.
 *
 * @param dev Device object.
 * @param model Device model data.
 *
 * @return
 * - #SANE_STATUS_GOOD  - model successfully changed.
 * - #SANE_STATUS_INVAL - invalid request (attempt to change model when the
 *   device is already active, or not yet opened).
 */
static SANE_Status
gt68xx_device_set_model (GT68xx_Device * dev, GT68xx_Model * model);

/** Get model by name.
 *
 * This function can be used to find a model by its name.
 *
 * @param name Device model name.
 * @param model Device model data.
 *
 * @return
 * - #SANE_TRUE  - model successfully found.
 * - #SANE_FALSE - model not found.
 */
static SANE_Bool
gt68xx_device_get_model (SANE_String name, GT68xx_Model ** model);

#if 0
/** Create a new private copy of the model data for this device.
 *
 * Normally the model data structures can be shared between several devices.
 * If the program needs to modify some model parameters for a device, it must
 * call this function to make a private copy of parameters.  This private copy
 * will be automatically freed when the device is destroyed.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD   - a private copy was made successfully.
 * - #SANE_STATUS_INVAL  - invalid request (the device was already active, or
 *   not yet opened).
 * - #SANE_STATUS_NO_MEM - not enough memory for copy of the model parameters.
 */
static SANE_Status gt68xx_device_unshare_model (GT68xx_Device * dev);
#endif

/** Activate the device.
 *
 * The device must be activated before performing any I/O operations with it.
 * All device model parameters must be configured before activation; it is
 * impossible to change them after the device is active.
 *
 * This function might need to acquire resources (it calls
 * GT68xx_Command_Set::activate).  These resources will be released when
 * gt68xx_device_deactivate() is called.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD  - device activated successfully.
 * - #SANE_STATUS_INVAL - invalid request (attempt to activate a closed or
 *   unconfigured device).
 */
static SANE_Status gt68xx_device_activate (GT68xx_Device * dev);

/** Deactivate the device.
 *
 * This function reverses the action of gt68xx_device_activate().
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD  - device deactivated successfully.
 * - #SANE_STATUS_INVAL - invalid request (the device was not activated).
 */
static SANE_Status gt68xx_device_deactivate (GT68xx_Device * dev);

/** Write a data block to the GT68xx memory.
 *
 * @param dev  Device object.
 * @param addr Start address in the GT68xx memory.
 * @param size Size of the data block in bytes.
 * @param data Data block to write.
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 *
 * @warning
 * @a size must be a multiple of 64 (at least with GT6816), otherwise the
 * scanner (and possibly the entire USB bus) will lock up.
 */
static SANE_Status
gt68xx_device_memory_write (GT68xx_Device * dev, SANE_Word addr,
			    SANE_Word size, SANE_Byte * data);

/** Read a data block from the GT68xx memory.
 *
 * @param dev  Device object.
 * @param addr Start address in the GT68xx memory.
 * @param size Size of the data block in bytes.
 * @param data Buffer for the read data.
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 *
 * @warning
 * @a size must be a multiple of 64 (at least with GT6816), otherwise the
 * scanner (and possibly the entire USB bus) will lock up.
 */
static SANE_Status
gt68xx_device_memory_read (GT68xx_Device * dev, SANE_Word addr,
			   SANE_Word size, SANE_Byte * data);

/** Execute a control command.
 *
 * @param dev Device object.
 * @param cmd Command packet.
 * @param res Result packet (may point to the same buffer as @a cmd).
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_req (GT68xx_Device * dev, GT68xx_Packet cmd, GT68xx_Packet res);

/** Execute a "small" control command.
 *
 * @param dev Device object.
 * @param cmd Command packet; only first 8 bytes are used.
 * @param res Result packet (may point to the same buffer as @a cmd).
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_small_req (GT68xx_Device * dev, GT68xx_Packet cmd,
			 GT68xx_Packet res);

#if 0
/** Check whether the firmware is downloaded into the scanner.
 *
 * @param dev Device object.
 * @param loaded Returned firmware status:
 * - #SANE_TRUE - the firmware is already loaded.
 * - #SANE_FALSE - the firmware is not loaded.
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_check_firmware (GT68xx_Device * dev, SANE_Bool * loaded);
#endif

static SANE_Status
gt68xx_device_download_firmware (GT68xx_Device * dev,
				 SANE_Byte * data, SANE_Word size);

/** Check whether the external power supply is connected.
 *
 * @param dev Device object.
 * @param power_ok Returned power status:
 * - #SANE_TRUE - the external power supply is connected, or the scanner does
 *   not need external power.
 * - #SANE_FALSE - the external power supply is not connected, so the scanner
 *   will not work.
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_get_power_status (GT68xx_Device * dev, SANE_Bool * power_ok);

/** Check whether the transparency adapter is connected.
 *
 * @param dev Device object.
 * @param ta_attached Returned TA status:
 * - #SANE_TRUE  - the TA is connected.
 * - #SANE_FALSE - the TA is not connected.
 *
 * @return
 * - #SANE_STATUS_GOOD        - success.
 * - #SANE_STATUS_UNSUPPORTED - the scanner does not support TA connection.
 * - #SANE_STATUS_IO_ERROR    - a communication error occured.
 */
static SANE_Status
gt68xx_device_get_ta_status (GT68xx_Device * dev, SANE_Bool * ta_attached);

/** Turn the lamps in the scanner and/or the transparency adapter on or off.
 *
 * @param dev Device object.
 * @param fb_lamp #SANE_TRUE turns on the flatbed lamp.
 * @param ta_lamp #SANE_TRUE turns on the transparency adapter lamp.
 *
 * @return
 * - #SANE_STATUS_GOOD        - success.
 * - #SANE_STATUS_IO_ERROR    - a communication error occured.
 * - #SANE_STATUS_UNSUPPORTED - unsupported request was made (like attempt to
 *   turn on the TA lamp on a scanner which does not support TA).
 */
static SANE_Status
gt68xx_device_lamp_control (GT68xx_Device * dev, SANE_Bool fb_lamp,
			    SANE_Bool ta_lamp);

/** Check whether the scanner carriage is still moving. 
 *
 * @param dev Device object.
 * @param moving Returned state of the scanner:
 * - #SANE_TRUE  - the scanner carriage is still moving.
 * - #SANE_FALSE - the scanner carriage has stopped.
 *
 * @return
 * - #SANE_STATUS_GOOD     - success; the status in @a *moving is valid.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_is_moving (GT68xx_Device * dev, SANE_Bool * moving);

#if 0
/** Move the scanner carriage by the specified number of steps.
 *
 * @param dev Device object.
 * @param distance Number of steps to move (positive to move forward, negative
 * to move backward).  The measurement unit is model-dependent; number of steps
 * per inch is found in the GT68xx_Model::base_ydpi field.
 *
 * @return
 * - #SANE_STATUS_GOOD - success; the movement is started.  Call
 *   gt68xx_device_is_moving() periodically to determine when the movement is
 *   complete.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_move_relative (GT68xx_Device * dev, SANE_Int distance);
#endif

/** Move the scanner carriage to the home position.
 *
 * This function starts moving the scanner carriage to the home position, but
 * deos not wait for finishing the movement.  To determine when the carriage
 * returned to the home position, the program should periodically call
 * gt68xx_device_is_moving().
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD - success; the movement is started.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status gt68xx_device_carriage_home (GT68xx_Device * dev);

/** Eject the paper after the end of scanning.
 *
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD - success; the movement is started.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status gt68xx_device_paperfeed (GT68xx_Device * dev);

/** Start scanning the image.
 *
 * This function initiates scanning with parameters set by
 * gt68xx_device_setup_scan() (which should be called before).  In particular,
 * it starts the carriage movement to the start of the scanning window.
 *
 * After calling this function, gt68xx_device_read_scanned_data() should be
 * called repeatedly until the scanner signals that it is ready to deliver the
 * data.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status gt68xx_device_start_scan (GT68xx_Device * dev);

/** Start reading the scanned image data.
 *
 * This function should be used only after gt68xx_device_start_scan().  It
 * should be called repeatedly until @a *ready flag becomes true, at which
 * point reading the image data should be started using
 * gt68xx_device_read_prepare().
 *
 * @param dev Device object.
 * @param ready Returned status of the scanner:
 * - #SANE_TRUE  - the scanner is ready to send data.
 * - #SANE_FALSE - the scanner is not ready (e.g., the carriage has not reached
 *   the start of the scanning window).
 *
 * @return
 * - #SANE_STATUS_GOOD - success; the value in @a *ready is valid.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_read_scanned_data (GT68xx_Device * dev, SANE_Bool * ready);

/** Stop scanning the image.
 *
 * This function should be used after reading all image data from the scanner,
 * or in the middle of scan to abort the scanning process.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status gt68xx_device_stop_scan (GT68xx_Device * dev);

/** Set parameters for the next scan.
 *
 * This function calculates the hardware-dependent scanning parameters and,
 * unless @a calculate_only is set, sends the command to prepare for scanning
 * with the specified window and parameters.
 *
 * @param dev Device object.
 * @param request High-level scanning request.
 * @param action Action code describing the phase of calibration or scanning
 * process.
 * @param params Returned structure with hardware-dependent scanning
 * parameters.
 *
 * @return
 * - #SANE_STATUS_GOOD - success.
 * - #SANE_STATUS_UNSUPPORTED - the requested scanning parameters in @a request
 *   are not supported by hardware.
 * - #SANE_STATUS_INVAL - some of the parameters in @a request, or the @a
 *   action code, are completely invalid.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_setup_scan (GT68xx_Device * dev,
			  GT68xx_Scan_Request * request,
			  GT68xx_Scan_Action action,
			  GT68xx_Scan_Parameters * params);

/** Configure the analog front-end (AFE) of the GT68xx.
 *
 * @param dev Device object.
 * @param params AFE parameters.
 *
 * @return
 * - #SANE_STATUS_GOOD - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_set_afe (GT68xx_Device * dev, GT68xx_AFE_Parameters * params);

static SANE_Status
gt68xx_device_set_exposure_time (GT68xx_Device * dev,
				 GT68xx_Exposure_Parameters * params);

/** Read raw data from the bulk-in scanner pipe.
 *
 * @param dev Device object.
 * @param buffer Buffer for the read data.
 * @param size Pointer to the variable which must be set to the requested data
 * size before call.  After completion this variable will hold the number of
 * bytes actually read.
 *
 * @return
 * - #SANE_STATUS_GOOD - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
gt68xx_device_read_raw (GT68xx_Device * dev, SANE_Byte * buffer,
			size_t * size);

static SANE_Status
gt68xx_device_set_read_buffer_size (GT68xx_Device * dev, size_t buffer_size);

static SANE_Status
gt68xx_device_read_prepare (GT68xx_Device * dev, size_t expected_count,
			    SANE_Bool final_scan);

static SANE_Status
gt68xx_device_read (GT68xx_Device * dev, SANE_Byte * buffer, size_t * size);

static SANE_Status gt68xx_device_read_finish (GT68xx_Device * dev);

/** Make sure that the result of a command is ok.
 *
 * @param res Result packet from the last command
 * @param command Command
 * 
 * @return
 * - #SANE_STATUS_GOOD - success.
 * - #SANE_STATUS_IO_ERROR - the command wasn't successful
*/
static SANE_Status
gt68xx_device_check_result (GT68xx_Packet res, SANE_Byte command);


static SANE_Status 
gt68xx_device_get_id (GT68xx_Device * dev);

/** Read the device descriptor of the scanner.
 *
 * This function should be called before closing the device to make sure
 * that the device descriptor is propperly stored in the scanner's memory.
 * If that's not done, the next try to get the config descriptor will
 * result in a corrupted descriptor.
 *
 * @param dev device
*/
static void 
gt68xx_device_fix_descriptor (GT68xx_Device * dev);

#endif /* not GT68XX_LOW_H */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
