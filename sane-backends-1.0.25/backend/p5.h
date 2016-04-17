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

/** @file p5.h
 * @brief Declaration of high level structures used by the p5 backend.
 *
 * The structures and functions declared here are used to do the deal with
 * the SANE API.
 */


#ifndef P5_H
#define P5_H

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"

/**< macro to enable an option */
#define ENABLE(OPTION)  session->options[OPTION].descriptor.cap &= ~SANE_CAP_INACTIVE

/**< macro to disable an option */
#define DISABLE(OPTION) session->options[OPTION].descriptor.cap |=  SANE_CAP_INACTIVE

/** macro to test is an option is active */
#define IS_ACTIVE(OPTION) (((s->opt[OPTION].cap) & SANE_CAP_INACTIVE) == 0)

/**< name of the configuration file */
#define P5_CONFIG_FILE "p5.conf"

/**< macro to define texts that should translated */
#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif

/** color mode names
 */
/* @{ */
#define COLOR_MODE              "Color"
#define GRAY_MODE               "Gray"
#define LINEART_MODE            "Lineart"
/* @} */

#include "p5_device.h"

/** 
 * List of all SANE options available for the frontend. Given a specific
 * device, some options may be set to inactive when the scanner model is
 * detected. The default values and the ranges they belong maybe also model
 * dependent.
 */
enum P5_Options
{
  OPT_NUM_OPTS = 0,		/** first enum which must be zero */
  /** @name standard options group
   */
  /* @{ */
  OPT_STANDARD_GROUP,
  OPT_MODE,			/** set the mode: color, grey levels or lineart */
  OPT_PREVIEW,			/** set up for preview */
  OPT_RESOLUTION,		/** set scan's resolution */
  /* @} */

  /** @name geometry group 
   * geometry related options
   */
  /* @{ */
  OPT_GEOMETRY_GROUP,		/** group of options defining the position and size of the scanned area */
  OPT_TL_X,			/** top-left x of the scanned area*/
  OPT_TL_Y,			/** top-left y of the scanned area*/
  OPT_BR_X,			/** bottom-right x of the scanned area*/
  OPT_BR_Y,			/** bottom-right y of the scanned area*/
  /* @} */

  /** @name sensor group
   * detectors group
   */
  /* @{ */
  OPT_SENSOR_GROUP,
  OPT_PAGE_LOADED_SW,
  OPT_NEED_CALIBRATION_SW,
  /* @} */

  /** @name button group
   * buttons group
   */
  /* @{ */
  OPT_BUTTON_GROUP,
  OPT_CALIBRATE,
  OPT_CLEAR_CALIBRATION,
  /* @} */

  /** @name option list terminator
   * must come last so it can be used for array and list size
   */
  NUM_OPTIONS
};

/**
 * Contains one SANE option description and its value.
 */
typedef struct P5_Option
{
  SANE_Option_Descriptor descriptor;	/** option description */
  Option_Value value;			/** option value */
} P5_Option;

/** 
 * Frontend session. This struct holds informations usefull for
 * the functions defined in SANE's standard. Informations closer 
 * to the hardware are in the P5_Device structure. There is
 * as many session structure than frontends using the backend.
 */
typedef struct P5_Session
{
  /**
   * Point to the next session in a linked list
   */
  struct P5_Session *next;

  /**
   * low-level device object used by the session
   */
  P5_Device *dev;

  /**
   * array of possible options and their values for the backend
   */
  P5_Option options[NUM_OPTIONS];

  /**
   * SANE_True if a scan is in progress, ie sane_start has been called.
   * Stay SANE_True until sane_cancel() is called.
   */
  SANE_Bool scanning;

  /** @brief non blocking flag
   * SANE_TRUE if sane_read are non-blocking, ie returns immediatly if there
   * is no data available from the scanning device. Modified by sane_set_io_mode()
   */
  SANE_Bool non_blocking;

  /**
   * SANE Parameters describes what the next or current scan will be 
   * according to the current values of the options
   */
  SANE_Parameters params;

   /**
    * bytes to send to frontend for the scan
    */
  SANE_Int to_send;

   /**
    * bytes currently sent to frontend during the scan
    */
  SANE_Int sent;

} P5_Session;


static SANE_Status probe_p5_devices (void);
static P5_Model *probe (const char *devicename);
static SANE_Status config_attach (SANEI_Config * config, const char *devname);
static SANE_Status attach_p5 (const char *name, SANEI_Config * config);
static SANE_Status init_options (struct P5_Session *session);
static SANE_Status compute_parameters (struct P5_Session *session);

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */

#endif /* not P5_H */
