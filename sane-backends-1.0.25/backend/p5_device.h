/* sane - Scanner Access Now Easy.

   Copyright (C) 2009-2012 stef.dev@free.fr
   
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
*/

/** @file p5_device.h
 * @brief Declaration of low level structures used by the p5 backend.
 *
 * The structures and function declared here are used to do the low level
 * communication with the physical device.
 */

#ifndef P5_DEVICE_H
#define P5_DEVICE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/_stdint.h"

#ifdef HAVE_LINUX_PPDEV_H
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ppdev.h>
#include <linux/parport.h>
#endif

/** @name debugging levels
 */
/* @{ */
#define DBG_error0      0	/* errors/warnings printed even with devuglevel 0 */
#define DBG_error       1	/* fatal errors */
#define DBG_warn        2	/* warnings and non-fatal errors */
#define DBG_info        4	/* informational messages */
#define DBG_proc        8	/* starting/finishing functions */
#define DBG_trace       16	/* tracing messages */
#define DBG_io          32	/* io functions */
#define DBG_io2         64	/* io functions that are called very often */
#define DBG_data        128	/* log image data */
/* @} */

/**
 * maximal number of resolutions
 */
#define MAX_RESOLUTIONS 8

/**> sensor's number of pixels 8.5' @ 300 dpi */
#define MAX_SENSOR_PIXELS     2550

/**> number of lines to skip when doing calibration */
#define CALIBRATION_SKIP_LINES 80

/**> last value considered as black for calibration */
#define BLACK_LEVEL 40

/**> white target value for calibration */
#define WHITE_TARGET 220.0

/** per dpi calibration rgb data
 * Calibration data structure
 */
typedef struct P5_Calibration_Data
{
  unsigned int dpi;
  uint8_t black_data[MAX_SENSOR_PIXELS * 3];
  uint8_t white_data[MAX_SENSOR_PIXELS * 3];
} P5_Calibration_Data;

/** 
 * This structure describes a particular model which is handled by the backend. 
 * Contained data is immutable and is used to initalize the P5_Device
 * structure.
 */
typedef struct P5_Model
{
  /** @name device identifier
   * These values are set up once the physical device has been detected. They
   * are used to build the return value of sane_get_devices().
   */
  /* @{ */
  SANE_String_Const name;
  SANE_String_Const vendor;
  SANE_String_Const product;
  SANE_String_Const type;
  /* @} */

  /** @name resolution
   * list of avalailable physical resolution.
   * The resolutions must sorted from lower to higher value. The list is terminated
   * by a value of 0.
   */
  /* @{ */
  int xdpi_values[MAX_RESOLUTIONS];	/** possible x resolutions */
  int ydpi_values[MAX_RESOLUTIONS];	/** possible y resolutions */
  /* @} */

  /** @name scan area description
   * Minimal and maximal values. It's easier to have dedicated members instead
   * of searching these values in the dpi lists. They are initialized from dpi
   * lists.
   */
  /* @{ */
  int max_xdpi;		/** physical maximum x dpi */
  int max_ydpi;		/** physical maximum y dpi */
  int min_xdpi;		/** physical minimum x dpi */
  int min_ydpi;		/** physical minimum y dpi */
  /* @} */

  /** @name line distance shift
   * Distance between CCD arrays for each color. Expressed in line
   * number at maximum motor resolution.
   */
  int lds;

  /** @name scan area description
   * The geometry values are expressed from the head parking position,
   * or the start. For a given model, the scan area selected by a frontend
   * will have to fit within these values.
   */
  /* @{ */
  SANE_Fixed x_offset;		/** Start of scan area in mm */
  SANE_Fixed y_offset;		/** Start of scan area in mm */
  SANE_Fixed x_size;		/** Size of scan area in mm */
  SANE_Fixed y_size;		/** Size of scan area in mm */
  /* @} */

} P5_Model;


/**
 * Enumeration of configuration options for a device. It must starts at 0.
 */
enum P5_Configure_Option
{
  CFG_MODEL_NAME = 0,		/**<option to override model name */
  NUM_CFG_OPTIONS		/** MUST be last to give the actual number of configuration options */
};

/**
 * Device specific configuration structure to hold option values for
 * devices handled by the p5 backend. There must one member for
 * each configuration option.
 */
typedef struct P5_Config
{
  SANE_String modelname;	/** model name to use, overrinding the one from detection */
} P5_Config;


/**
 * Hardware device description.
 * Since the settings used for a scan may actually differ from the one of the
 * SANE level, it may contains scanning parameters and data relative to a current
 * scan such as data buffers and counters.
 */
typedef struct P5_Device
{
  /**
   * Point to the next device in a linked list
   */
  struct P5_Device *next;

  /**
   * Points to a structure that decribes model capabilities, geometry 
   * and default settings.
   */
  P5_Model *model;

  /**
   * @brief name of the device
   * Name of the device: it may be the file name used to access the hardware.
   * For instance parport0 for a parallel port device, or the libusb file name
   * for an USB scanner.
   */
  SANE_String name;

  /**
   * SANE_TRUE if the device is local (ie not over network)
   */
  SANE_Bool local;

  /**
   * True if device has been intialized.
   */
  SANE_Bool initialized;

  /**
   * Configuration options for the device read from
   * configuration file at attach time. This member is filled at 
   * attach time.
   */
  P5_Config *config;

  /** @brief scan parameters
   * The scan done by the hardware can be different from the one at the SANE
   * frontend session. For instance: 
   *  - xdpy and ydpi may be different to accomodate hardware capabilites.
   *  - many CCD scanners need to scan more lines to correct the 'line 
   *  distance shift' effect.
   *  - emulated modes (lineart from gray scan, or gray scan for color one)
   */
  /* @{ */
  int xdpi;		/** real horizontal resolution */
  int ydpi;		/** real vertical resolution */
  int lines;		/** physical lines to scan */
  int pixels;		/** physical width of scan area */
  int bytes_per_line;	/** number of bytes per line */
  int xstart;		/** x start coordinate */
  int ystart;		/** y start coordinate */
  int mode;		/** color, gray or lineart mode */
  int lds;		/** line distance shift */
  /* @} */

  /** @brief device file descriptor
   * low level device file descriptor
   */
  int fd;

  /**
   * work buffer for scans
   */
  uint8_t *buffer;

  /**
   * buffer size
   */
  size_t size;

  /**
   * position in buffer
   */
  size_t position;

  /**
   * top value of available bytes in buffer
   */
  size_t top;

  /**
   * bottom value of available bytes in buffer
   */
  size_t bottom;

  /**
   * True if device has been calibrated.
   */
  SANE_Bool calibrated;

  P5_Calibration_Data *calibration_data[MAX_RESOLUTIONS * 2];

  /**> correction coefficient for the current scan */
  float *gain;
  uint8_t *offset;

} P5_Device;


#define DATA    0
#define STATUS  1
#define CONTROL 2
#define EPPADR  3
#define EPPDATA 4

#define REG0 0x00
#define REG1 0x11
#define REG2 0x22
#define REG3 0x33
#define REG4 0x44
#define REG5 0x55
#define REG6 0x66
#define REG7 0x77
#define REG8 0x88
#define REG9 0x99
#define REGA 0xAA
#define REGB 0xBB
#define REGC 0xCC
#define REGD 0xDD
#define REGE 0xEE
#define REGF 0xFF

#define MODE_COLOR 	0
#define MODE_GRAY  	1
#define MODE_LINEART    2

#endif /* not P5_DEVICE_H */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
