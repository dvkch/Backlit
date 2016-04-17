/* SANE - Scanner Access Now Easy.

   Copyright (C) 2011-2015 Rolf Bensch <rolf at bensch hyphen online dot de>
   Copyright (C) 2007-2008 Nicolas Martin, <nicols-guest at alioth dot debian dot org>
   Copyright (C) 2006-2007 Wittawat Yamwong <wittawat@web.de>

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
#ifndef PIXMA_H
#define PIXMA_H

/*!
 * \mainpage Scanner driver for Canon PIXMA MP series
 * \section example Sample code for application
 * \code
 *    pixma_set_debug_level(level);
 *    pixma_init();
 *    nscanners = pixma_find_scanners();
 *    devnr = choose_scanner(nscanners);
 *    scanner = pixma_open(devnr);
 *    setup_param(param);
 *    pixma_check_scan_param(scanner, param);
 *    do {
 *        if (I_need_events &&
 *            (ev = pixma_wait_event(scanner, timeout)) > 0) {
 *            handle_event(ev);
 *        }
 *        pixma_scan(scanner, param);
 *        while ((count = pixma_read_image(scanner, buf, len)) > 0) {
 *            write(buf, count);
 *            if (error_occured_in_write) {
 *                pixma_cancel(scanner);
 *            }
 *        }
 *    } while (!enough);
 *    pixma_close(scanner);
 *    pixma_cleanup();
 * \endcode
 *
 * <b>Note:</b> pixma_cancel() can be called asynchronously to
 * interrupt pixma_read_image(). It does not cancel the operation
 * immediately. pixma_read_image() <em>must</em> be called until it
 * returns zero or an error (probably \c PIXMA_ECANCELED).
 *
 * \section reference Reference
 * - \subpage API
 * - \subpage IO
 * - \subpage subdriver
 * - \subpage debug
 */

/*!
 * \defgroup API The driver API
 * \brief The driver API.
 *
 * The return value of functions that returns \c int has the following
 * meaning if not otherwise specified:
 *    - >= 0  if succeeded
 *    - < 0 if failed
 */

#ifdef HAVE_STDINT_H
# include <stdint.h>		/* available in ISO C99 */
#else
# include <sys/types.h>
typedef uint8_t uint8_t;
typedef uint16_t uint16_t;
typedef uint32_t uint32_t;
#endif /* HAVE_STDINT_H */

#ifdef HAVE_INTTYPES_H 
# include <inttypes.h>          /* available in ISO C99 */
#endif /* HAVE_INTTYPES_H */

/** \addtogroup API
 *  @{ */
/** \name Version of the driver */
/**@{*/
#define PIXMA_VERSION_MAJOR 0
#define PIXMA_VERSION_MINOR 17
#define PIXMA_VERSION_BUILD 23
/**@}*/

/** \name Error codes */
/**@{*/
#define PIXMA_EIO               -1
#define PIXMA_ENODEV            -2
#define PIXMA_EACCES            -3
#define PIXMA_ENOMEM            -4
#define PIXMA_EINVAL            -5
#define PIXMA_EBUSY             -6
#define PIXMA_ECANCELED         -7
#define PIXMA_ENOTSUP           -8
#define PIXMA_ETIMEDOUT         -9
#define PIXMA_EPROTO            -10
#define PIXMA_EPAPER_JAMMED     -11
#define PIXMA_ECOVER_OPEN       -12
#define PIXMA_ENO_PAPER         -13
#define PIXMA_EOF               -14
/**@}*/

/** \name Capabilities for using with pixma_config_t::cap */
/**@{*/
#define PIXMA_CAP_EASY_RGB     (1 << 0)
#define PIXMA_CAP_GRAY         (1 << 1)
#define PIXMA_CAP_ADF          (1 << 2)
#define PIXMA_CAP_48BIT        (1 << 3)
#define PIXMA_CAP_GAMMA_TABLE  (1 << 4)
#define PIXMA_CAP_EVENTS       (1 << 5)
#define PIXMA_CAP_TPU          (1 << 6)
#define PIXMA_CAP_ADFDUP       ((1 << 7) | PIXMA_CAP_ADF)
#define PIXMA_CAP_CIS          (0)
#define PIXMA_CAP_CCD          (1 << 8)
#define PIXMA_CAP_LINEART      (1 << 9)
#define PIXMA_CAP_NEGATIVE     (1 << 10)
#define PIXMA_CAP_TPUIR        ((1 << 11) | PIXMA_CAP_TPU)
#define PIXMA_CAP_EXPERIMENT   (1 << 31)
/**@}*/

/** \name Button events and related information returned by pixma_wait_event() */
/**@{*/
#define PIXMA_EV_NONE          0
#define PIXMA_EV_ACTION_MASK   (0xffffff)
#define PIXMA_EV_BUTTON1       (1 << 24)
#define PIXMA_EV_BUTTON2       (2 << 24)
#define PIXMA_EV_TARGET_MASK   (0xff)
#define PIXMA_EV_ORIGINAL_MASK (0xff00)
#define PIXMA_EV_DPI_MASK      (0xff0000)

#define GET_EV_TARGET(x) (x & PIXMA_EV_TARGET_MASK)
#define GET_EV_ORIGINAL(x) ( (x & PIXMA_EV_ORIGINAL_MASK) >> 8 )
#define GET_EV_DPI(x) ( (x & PIXMA_EV_DPI_MASK) >> 16 )

/**@}*/
/** @} end of API group */

#define PIXMA_CONFIG_FILE "pixma.conf"
#define MAX_CONF_DEVICES 15

struct pixma_t;
struct pixma_scan_ops_t;
struct pixma_scan_param_t;
struct pixma_config_t;
struct pixma_cmdbuf_t;
struct pixma_imagebuf_t;
struct pixma_device_status_t;

typedef struct pixma_t pixma_t;
typedef struct pixma_scan_ops_t pixma_scan_ops_t;
typedef struct pixma_scan_param_t pixma_scan_param_t;
typedef struct pixma_config_t pixma_config_t;
typedef struct pixma_cmdbuf_t pixma_cmdbuf_t;
typedef struct pixma_imagebuf_t pixma_imagebuf_t;
typedef struct pixma_device_status_t pixma_device_status_t;


/** \addtogroup API
 *  @{ */
/** String index constants */
typedef enum pixma_string_index_t
{
  PIXMA_STRING_MODEL,
  PIXMA_STRING_ID,
  PIXMA_STRING_LAST
} pixma_string_index_t;

/** Paper sources */
typedef enum pixma_paper_source_t
{
  PIXMA_SOURCE_FLATBED,
  PIXMA_SOURCE_ADF,
  PIXMA_SOURCE_TPU,
  PIXMA_SOURCE_ADFDUP		/* duplex */
} pixma_paper_source_t;

/** Scan modes */
typedef enum pixma_scan_mode_t
{
  /* standard scan modes */
  PIXMA_SCAN_MODE_COLOR,
  PIXMA_SCAN_MODE_GRAY,
  /* TPU scan modes for negatives */
  PIXMA_SCAN_MODE_NEGATIVE_COLOR,
  PIXMA_SCAN_MODE_NEGATIVE_GRAY,
  /* extended scan modes for 48 bit flatbed scanners */
  PIXMA_SCAN_MODE_COLOR_48,
  PIXMA_SCAN_MODE_GRAY_16,
  /* 1 bit lineart scan mode */
  PIXMA_SCAN_MODE_LINEART,
  /* TPUIR scan mode */
  PIXMA_SCAN_MODE_TPUIR
} pixma_scan_mode_t;

typedef enum pixma_hardware_status_t
{
  PIXMA_HARDWARE_OK,
  PIXMA_HARDWARE_ERROR
} pixma_hardware_status_t;

typedef enum pixma_lamp_status_t
{
  PIXMA_LAMP_OK,
  PIXMA_LAMP_WARMING_UP,
  PIXMA_LAMP_OFF,
  PIXMA_LAMP_ERROR
} pixma_lamp_status_t;

typedef enum pixma_adf_status_t
{
  PIXMA_ADF_OK,
  PIXMA_ADF_NO_PAPER,
  PIXMA_ADF_JAMMED,
  PIXMA_ADF_COVER_OPEN,
  PIXMA_ADF_ERROR
} pixma_adf_status_t;

typedef enum pixma_calibration_status_t
{
  PIXMA_CALIBRATION_OK,
  PIXMA_CALIBRATION_IN_PROGRESS,
  PIXMA_CALIBRATION_OFF,
  PIXMA_CALIBRATION_ERROR
} pixma_calibration_status_t;

/** Device status. */
struct pixma_device_status_t
{
  pixma_hardware_status_t hardware;
  pixma_lamp_status_t lamp;
  pixma_adf_status_t adf;
  pixma_calibration_status_t cal;
};

/** Scan parameters. */
struct pixma_scan_param_t
{
    /** Size in bytes of one image line (row).
     *  line_size >= depth / 8 * channels * w <br>
     *  This field will be set by pixma_check_scan_param(). */
  uint64_t line_size;
  
    /** Size in bytes of the whole image.
     *  image_size = line_size * h <br>
     *  This field will be set by pixma_check_scan_param(). */
  uint64_t image_size;

    /** Channels per pixel. 1 = grayscale and lineart, 3 = color */
  unsigned channels;

    /** Bits per channels.
     *   1 =  1 bit B/W lineart (flatbed)
     *   8 =  8 bit grayscale,
     *       24 bit color (both flatbed)
     *  16 = 16 bit grayscale (TPU, flatbed not implemeted),
     *       48 bit color (TPU, flatbed not implemented) */
  unsigned depth;

  /*@{ */
    /** Resolution. Valid values are 75,150,300,600,1200... */
  unsigned xdpi, ydpi;
  /*@} */

  /*! \name Scan area in pixels
   * (0,0) = top left; positive x extends to the right; positive y to the
   *  bottom; in pixels.
   * xs is the offset in x direction of the selected scan range relative
   * to the range read from the scanner and wx the width in x direction
   * of the scan line read from scanner. */
  /*@{ */
  unsigned x, y, w, h,   xs, wx;
  /*@} */

  /** Flag indicating whether the offset correction for TPU scans 
   *  was already performed (to avoid repeated corrections).
   *  Currently only used in pixma_mp810.c sub-driver */
  unsigned tpu_offset_added;

  /** Flag indicating whether a software-lineart scan is in progress
   *  0 = other scan
   *  1 = software-lineart scan */
  unsigned software_lineart;

  /** Threshold for software-lineart scans */
  unsigned threshold;

  /** lineart threshold curve for dynamic rasterization */
  unsigned threshold_curve;

  /* look up table used in dynamic rasterization */
  unsigned char lineart_lut[256];

    /** Gamma table. 4096 entries, 12 bit => 8 bit. If \c NULL, default gamma
     *  specified by subdriver will be used. */
  const uint8_t *gamma_table;

    /** \see #pixma_paper_source_t */
  pixma_paper_source_t source;

  /** \see #pixma_scan_mode_t */
  pixma_scan_mode_t mode;

    /** The current page # in the same ADF scan session, 0 in non ADF */
  unsigned adf_pageid;
};

/** PIXMA model information */
struct pixma_config_t
{
  /* If you change this structure, don't forget to update the device list in
   * subdrivers. */
  const char *name;	   /**< Model name. */
  const char *model;   /**< Short model */
  uint16_t vid;		     /**< USB Vendor ID */
  uint16_t pid;		     /**< USB Product ID */
  unsigned iface;	     /**< USB Interface number */
  const pixma_scan_ops_t *ops;	  /**< Subdriver ops */
  unsigned xdpi;	     /**< Maximum horizontal resolution[DPI] */
  unsigned ydpi;	     /**< Maximum vertical resolution[DPI] */
  unsigned adftpu_min_dpi;    /**< Maximum horizontal resolution[DPI] for adf/tpu
                                 *  only needed if ADF/TPU has another min. dpi value than 75 dpi */
  unsigned adftpu_max_dpi;    /**< Maximum vertical resolution[DPI] for adf/tpu
                                 *  only needed if ADF/TPU has another max. dpi value than xdpi */
  unsigned tpuir_min_dpi;       /**< Minimum resolution[DPI] for tpu-ir
                                   *  only needed if TPU-IR has another min. dpi value than 75 dpi */
  unsigned tpuir_max_dpi;       /**< Maximum resolution[DPI] for tpu-ir
                                   *  only needed if TPU-IR has another max. dpi value than xdpi */
  unsigned width;	     /**< Maximum width of scannable area in pixels at 75DPI */
  unsigned height;	   /**< Maximum height of scannable area in pixels at 75DPI */
  unsigned cap;		     /**< Capability bitfield \see PIXMA_CAP_* */
};


/* Defined in pixma_common.c */

/** Initialize the driver. It must be called before any other functions
 *  except pixma_set_debug_level(). */
int pixma_init (void);

/** Free resources allocated by the driver. */
void pixma_cleanup (void);

/** Set the debug level.
 *  \param[in] level the debug level
 *    - 0 No debug output at all
 *    - 1 Only errors and warning
 *    - 2 General information
 *    - 3 Debugging messages
 *    - 10 USB traffic dump */
void pixma_set_debug_level (int level);

/** Find scanners. The device number used in pixma_open(),
 *  pixma_get_device_model(), pixma_get_device_id() and
 *  pixma_get_device_config() must be less than the value returned by the last
 *  call of this function.
 *
 *  \return The number of scanners found currently. The return value is
 *  guaranteed to be valid until the next call to pixma_find_scanners(). */
int pixma_find_scanners (const char **conf_devices);

/** Return the model name of the device \a devnr. */
const char *pixma_get_device_model (unsigned devnr);

/** Return the unique ID of the device \a devnr. */
const char *pixma_get_device_id (unsigned devnr);

/** Return the device configuration of the device \a devnr. */
const struct pixma_config_t *pixma_get_device_config (unsigned devnr);

/** Open a connection to the scanner \a devnr.
 *  \param[in] devnr The scanner number
 *  \param[out] handle The device handle
 *  \see pixma_find_scanners() */
int pixma_open (unsigned devnr, pixma_t ** handle);

/** Close the connection to the scanner. The scanning process is aborted
 *  if necessary before the function returns. */
void pixma_close (pixma_t * s);

/** Initiate an image acquisition process. You must keep \a sp valid until the
 *  image acquisition process has finished. */
int pixma_scan (pixma_t *, pixma_scan_param_t * sp);

/** Read a block of image data. It blocks until there is at least one byte
 *  available or an error occurs.
 *
 *  \param[out] buf Pointer to the buffer
 *  \param[in] len Size of the buffer
 *
 *  \retval count Number of bytes written to the buffer or error. Possible
 *  return value:
 *     - count = 0 for end of image
 *     - count = \a len
 *     - 0 < count < \a len if and only if it is the last block.
 *     - count < 0 for error  */
int pixma_read_image (pixma_t *, void *buf, unsigned len);

#if 0
/** Read a block of image data and write to \a fd.
 *  \param[in] fd output file descriptor
 *  \see pixma_read_image() */
int pixma_read_image_write (pixma_t *, int fd);
#endif

/** Cancel the scanning process. No effect if no scanning process is in
 *  progress. It can be called asynchronously e.g. within a signal
 *  handle. pixma_cancel() doesn't abort the operation immediately.  It
 *  guarantees that the current call or, at the latest, the next call to
 *  pixma_read_image() will return zero or an error (probably PIXMA_ECANCELED). */
void pixma_cancel (pixma_t *);

/** Check the scan parameters. This function can change your parameters to
 *  match the device capability, e.g. adjust width and height to the available
 *  area.
 *  \return PIXMA_EINVAL for invalid parameters. */
int pixma_check_scan_param (pixma_t *, pixma_scan_param_t *);

/** Wait until a scanner button is pressed or it times out. It should not be
 *  called during image acquisition is in progress.
 *  \param[in] timeout in milliseconds, less than 0 means forever
 *  \return
 *   - \c PIXMA_EV_NONE if it timed out.
 *   - non-zero value indicates which button was pressed.
 *  \see PIXMA_EV_*
 */
uint32_t pixma_wait_event (pixma_t *, int timeout);

/** Activate connection to scanner */
int pixma_activate_connection (pixma_t *);

/** De-activate connection to scanner */

int pixma_deactivate_connection (pixma_t *);


/** Enable or disable background tasks. Currently, the only one task
 *  is submitting interrupt URB in background.
 *  \param[in] enabled if not zero, enable background task.
 *  \see pixma_set_interrupt_mode() */
int pixma_enable_background (pixma_t *, int enabled);

/** Read the current device status.
 *  \param[out] status the current device status
 *  \return 0 if succeeded. Otherwise, failed.
 */
int pixma_get_device_status (pixma_t *, pixma_device_status_t * status);

const char *pixma_get_string (pixma_t *, pixma_string_index_t);
const pixma_config_t *pixma_get_config (pixma_t *);
void pixma_fill_gamma_table (double gamma, uint8_t * table, unsigned n);
const char *pixma_strerror (int error);

/** @} end of API group */

#endif
