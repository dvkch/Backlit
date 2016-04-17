/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   Parts of the structs have been taken from the gt68xx backend by
   Sergey Vlasov <vsu@altlinux.ru> et al.

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

#ifndef GENESYS_LOW_H
#define GENESYS_LOW_H


#include "../include/sane/config.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stddef.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_MKDIR
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"

#include "../include/_stdint.h"

#ifndef UNIT_TESTING
#define GENESYS_STATIC static
#else
#define GENESYS_STATIC
#endif

#define DBG_error0      0	/* errors/warnings printed even with devuglevel 0 */
#define DBG_error       1	/* fatal errors */
#define DBG_init        2	/* initialization and scanning time messages */
#define DBG_warn        3	/* warnings and non-fatal errors */
#define DBG_info        4	/* informational messages */
#define DBG_proc        5	/* starting/finishing functions */
#define DBG_io          6	/* io functions */
#define DBG_io2         7	/* io functions that are called very often */
#define DBG_data        8	/* log image data */

/**
 * call a function and return on error
 */
#define RIE(function)                                   \
  do { status = function;                               \
    if (status != SANE_STATUS_GOOD) \
      { \
        DBG(DBG_error, "%s: %s\n", __FUNCTION__, sane_strstatus (status)); \
	return status; \
      }	\
  } while (SANE_FALSE)

#define RIEF(function, mem)                                   \
  do { status = function;                               \
    if (status != SANE_STATUS_GOOD) \
      { \
	 free(mem); \
	 DBG(DBG_error, "%s: %s\n", __FUNCTION__, sane_strstatus (status)); \
	 return status;      \
      } \
  } while (SANE_FALSE)

#define RIEF2(function, mem1, mem2)                                   \
  do { status = function;                               \
    if (status != SANE_STATUS_GOOD) \
	  { \
		 free(mem1); \
		 free(mem2); \
		 return status;      \
	  } \
  } while (SANE_FALSE)

#define DBGSTART DBG (DBG_proc, "%s start\n", __FUNCTION__);
#define DBGCOMPLETED DBG (DBG_proc, "%s completed\n", __FUNCTION__);

#define FREE_IFNOT_NULL(x)		if(x!=NULL) { free(x); x=NULL;}

#define GENESYS_RED   0
#define GENESYS_GREEN 1
#define GENESYS_BLUE  2

/* Flags */
#define GENESYS_FLAG_UNTESTED     (1 << 0)	/**< Print a warning for these scanners */
#define GENESYS_FLAG_14BIT_GAMMA  (1 << 1)	/**< use 14bit Gamma table instead of 12 */
#define GENESYS_FLAG_LAZY_INIT    (1 << 2)	/**< skip extensive ASIC test at init   */
#define GENESYS_FLAG_XPA          (1 << 3)
#define GENESYS_FLAG_SKIP_WARMUP  (1 << 4)	/**< skip genesys_warmup()              */
/** @brief offset calibration flag
 * signals that the scanner does offset calibration. In this case off_calibration() and
 * coarse_gain_calibration() functions must be implemented
 */
#define GENESYS_FLAG_OFFSET_CALIBRATION   (1 << 5)
#define GENESYS_FLAG_SEARCH_START (1 << 6)	/**< do start search before scanning    */
#define GENESYS_FLAG_REPARK       (1 << 7)	/**< repark head (and check for lock) by
						   moving without scanning */
#define GENESYS_FLAG_DARK_CALIBRATION (1 << 8)	/**< do dark calibration */
#define GENESYS_FLAG_STAGGERED_LINE   (1 << 9)	/**< pixel columns are shifted vertically for hi-res modes */

#define GENESYS_FLAG_MUST_WAIT        (1 << 10)	/**< tells wether the scanner must wait for the head when parking */


#define GENESYS_FLAG_HAS_UTA          (1 << 11)	/**< scanner has a transparency adapter */

#define GENESYS_FLAG_DARK_WHITE_CALIBRATION (1 << 12) /**< yet another calibration method. does white and dark shading in one run, depending on a black and a white strip*/
#define GENESYS_FLAG_CUSTOM_GAMMA     (1 << 13)       /**< allow custom gamma tables */
#define GENESYS_FLAG_NO_CALIBRATION   (1 << 14)       /**< allow scanners to use skip the calibration, needed for sheetfed scanners */
#define GENESYS_FLAG_HALF_CCD_MODE    (1 << 15)       /**< scanner has setting for half ccd mode */
#define GENESYS_FLAG_SIS_SENSOR       (1 << 16)       /**< handling of multi-segments sensors in software */
#define GENESYS_FLAG_SHADING_NO_MOVE  (1 << 17)       /**< scanner doesn't move sensor during shading calibration */
#define GENESYS_FLAG_SHADING_REPARK   (1 << 18)       /**< repark head between shading scans */
#define GENESYS_FLAG_FULL_HWDPI_MODE  (1 << 19)       /**< scanner always use maximum hw dpi to setup the sensor */

#define GENESYS_HAS_NO_BUTTONS       0              /**< scanner has no supported button */
#define GENESYS_HAS_SCAN_SW          (1 << 0)       /**< scanner has SCAN button */
#define GENESYS_HAS_FILE_SW          (1 << 1)       /**< scanner has FILE button */
#define GENESYS_HAS_COPY_SW          (1 << 2)       /**< scanner has COPY button */
#define GENESYS_HAS_EMAIL_SW         (1 << 3)       /**< scanner has EMAIL button */
#define GENESYS_HAS_PAGE_LOADED_SW   (1 << 4)       /**< scanner has paper in detection */
#define GENESYS_HAS_OCR_SW           (1 << 5)       /**< scanner has OCR button */
#define GENESYS_HAS_POWER_SW         (1 << 6)       /**< scanner has power button */
#define GENESYS_HAS_CALIBRATE        (1 << 7)       /**< scanner has 'calibrate' software button to start calibration */
#define GENESYS_HAS_EXTRA_SW         (1 << 8)       /**< scanner has extra function button */

/* USB control message values */
#define REQUEST_TYPE_IN		(USB_TYPE_VENDOR | USB_DIR_IN)
#define REQUEST_TYPE_OUT	(USB_TYPE_VENDOR | USB_DIR_OUT)
#define REQUEST_REGISTER	0x0c
#define REQUEST_BUFFER		0x04
#define VALUE_BUFFER		0x82
#define VALUE_SET_REGISTER	0x83
#define VALUE_READ_REGISTER	0x84
#define VALUE_WRITE_REGISTER	0x85
#define VALUE_INIT		0x87
#define GPIO_OUTPUT_ENABLE	0x89
#define GPIO_READ		0x8a
#define GPIO_WRITE		0x8b
#define VALUE_BUF_ENDACCESS	0x8c
#define VALUE_GET_REGISTER	0x8e
#define INDEX			0x00

/* todo: used?
#define VALUE_READ_STATUS	0x86
*/

/* Read/write bulk data/registers */
#define BULK_OUT		0x01
#define BULK_IN			0x00
#define BULK_RAM		0x00
#define BULK_REGISTER		0x11

#define BULKIN_MAXSIZE          0xFE00
#define GL646_BULKIN_MAXSIZE    0xFFC0
#define GL646_BULKIN_MINSIZE    0x0800
#define BULKOUT_MAXSIZE         0xF000

/* AFE values */
#define AFE_INIT       1
#define AFE_SET        2
#define AFE_POWER_SAVE 4

#define LOWORD(x)  ((uint16_t)((x) & 0xffff))
#define HIWORD(x)  ((uint16_t)((x) >> 16))
#define LOBYTE(x)  ((uint8_t)((x) & 0xFF))
#define HIBYTE(x)  ((uint8_t)((x) >> 8))

/* Global constants */
/* TODO: emove this leftover of early backend days */
#define MOTOR_SPEED_MAX		350
#define DARK_VALUE		0

#define PWRBIT	        0x80
#define BUFEMPTY	0x40
#define FEEDFSH	        0x20
#define SCANFSH	        0x10
#define HOMESNR	        0x08
#define LAMPSTS	        0x04
#define FEBUSY	        0x02
#define MOTORENB	0x01

typedef struct Genesys_Register_Set
{
  uint16_t address;
  uint8_t value;
} Genesys_Register_Set;

/** @brief Data structure to set up analog frontend.
 * The analog frontend converts analog value from image sensor to
 * digital value. It has its own control registers which are set up
 * with this structure. The values are written using sanei_genesys_fe_write_data.
 * The actual register addresses they map to depends on the frontend used.
 * @see sanei_genesys_fe_write_data
 */
typedef struct
{
  uint8_t fe_id;      /**< id of the frontend description */
  uint8_t reg[4];     /**< values to set up frontend control register, they
 		      usually map to analog register 0x00 to 0x03 */
  uint8_t sign[3];    /**< sets the sign of the digital value */
  uint8_t offset[3];  /**< offset correction to apply to signal, most often
			maps to frontend register 0x20-0x22 */
  uint8_t gain[3];     /**< amplification to apply to signal, most often
			maps to frontend register 0x28-0x2a */
  uint8_t reg2[3];    /**< extra control registers */
} Genesys_Frontend;

typedef struct
{
  uint8_t sensor_id;	       /**< id of the sensor description */
  int optical_res;
  int black_pixels;
  int dummy_pixel;              /* value of dummy register. */
  int CCD_start_xoffset;	/* last pixel of CCD margin at optical resolution */
  int sensor_pixels;		/* total pixels used by the sensor */
  int fau_gain_white_ref;	/* TA CCD target code (reference gain) */
  int gain_white_ref;		/* CCD target code (reference gain) */
  uint8_t regs_0x08_0x0b[4];
  uint8_t regs_0x10_0x1d[14];
  uint8_t regs_0x52_0x5e[13];
  float gamma[3];		/**< red, green and blue gamma coefficient for default gamma tables */
  uint16_t *gamma_table[3];	/**< sensor specific gamma tables */
} Genesys_Sensor;

typedef struct
{
  uint8_t gpo_id;	/**< id of the gpo description */
  uint8_t value[2];	/**< registers 0x6c and 0x6d on gl843 */
  uint8_t enable[2];	/**< registers 0x6e and 0x6F on gl843 */
} Genesys_Gpo;

typedef struct
{
  SANE_Int maximum_start_speed; /* maximum speed allowed when accelerating from standstill. Unit: pixeltime/step */
  SANE_Int maximum_speed;       /* maximum speed allowed. Unit: pixeltime/step */
  SANE_Int minimum_steps;       /* number of steps used for default curve */
  float g;                      /* power for non-linear acceleration curves. */
/* vs*(1-i^g)+ve*(i^g) where
   vs = start speed, ve = end speed,
   i = 0.0 for first entry and i = 1.0 for last entry in default table*/
} Genesys_Motor_Slope;


typedef struct
{
  uint8_t motor_id;	         /**< id of the motor description */
  SANE_Int base_ydpi;		 /* motor base steps. Unit: 1/" */
  SANE_Int optical_ydpi;	 /* maximum resolution in y-direction. Unit: 1/"  */
  SANE_Int max_step_type;        /* maximum step type. 0-2 */
  SANE_Int power_mode_count;        /* number of power modes*/
  Genesys_Motor_Slope slopes[2][3]; /* slopes to derive individual slopes from */
} Genesys_Motor;

typedef enum Genesys_Color_Order
{
  COLOR_ORDER_RGB,
  COLOR_ORDER_BGR
}
Genesys_Color_Order;


#define MAX_RESOLUTIONS 13
#define MAX_DPI 4

#define GENESYS_GL646	 646
#define GENESYS_GL841	 841
#define GENESYS_GL843	 843
#define GENESYS_GL845	 845
#define GENESYS_GL846	 846
#define GENESYS_GL847	 847
#define GENESYS_GL848	 848
#define GENESYS_GL123	 123
#define GENESYS_GL124	 124

#define GENESYS_MAX_REGS 256

#define DAC_WOLFSON_UMAX   0
#define DAC_WOLFSON_ST12   1
#define DAC_WOLFSON_ST24   2
#define DAC_WOLFSON_5345   3
#define DAC_WOLFSON_HP2400 4
#define DAC_WOLFSON_HP2300 5
#define DAC_CANONLIDE35    6
#define DAC_AD_XP200       7   /* Analog Device frontend */
#define DAC_WOLFSON_XP300  8
#define DAC_WOLFSON_HP3670 9
#define DAC_WOLFSON_DSM600 10
#define DAC_CANONLIDE200   11
#define DAC_KVSS080        12
#define DAC_G4050          13
#define DAC_CANONLIDE110   14
#define DAC_PLUSTEK_3600   15
#define DAC_CANONLIDE700   16
#define DAC_CS8400F        17
#define DAC_IMG101         18
#define DAC_PLUSTEK3800    19
#define DAC_CANONLIDE80    20

#define CCD_UMAX         0
#define CCD_ST12         1	/* SONY ILX548: 5340 Pixel  ??? */
#define CCD_ST24         2	/* SONY ILX569: 10680 Pixel ??? */
#define CCD_5345         3
#define CCD_HP2400       4
#define CCD_HP2300       5
#define CCD_CANONLIDE35  6
#define CIS_XP200        7      /* CIS sensor for Strobe XP200 */
				/* 8 is unused currently */
#define CCD_HP3670       9
#define CCD_DP665        10
#define CCD_ROADWARRIOR  11
#define CCD_DSMOBILE600  12
#define CCD_XP300        13
#define CCD_DP685        14
#define CIS_CANONLIDE200 15
#define CIS_CANONLIDE100 16
#define CCD_KVSS080      17
#define CCD_G4050        18
#define CIS_CANONLIDE110 19
#define CCD_PLUSTEK_3600 20
#define CCD_HP_N6310     21
#define CIS_CANONLIDE700 22
#define CCD_CS4400F      23
#define CCD_CS8400F      24
#define CCD_IMG101       25
#define CCD_PLUSTEK3800  26
#define CIS_CANONLIDE210 27
#define CIS_CANONLIDE80  28
#define CIS_CANONLIDE220 29
#define CIS_CANONLIDE120 30

#define GPO_UMAX         0
#define GPO_ST12         1
#define GPO_ST24         2
#define GPO_5345         3
#define GPO_HP2400       4
#define GPO_HP2300       5
#define GPO_CANONLIDE35  6
#define GPO_XP200        7
#define GPO_XP300        8
#define GPO_HP3670       9
#define GPO_DP665        10
#define GPO_DP685        11
#define GPO_CANONLIDE200 12
#define GPO_KVSS080      13
#define GPO_G4050        14
#define GPO_CANONLIDE110 15
#define GPO_PLUSTEK_3600 16
#define GPO_CANONLIDE210 17
#define GPO_HP_N6310     18
#define GPO_CANONLIDE700 19
#define GPO_CS4400F      20
#define GPO_CS8400F      21
#define GPO_IMG101       22
#define GPO_PLUSTEK3800  23
#define GPO_CANONLIDE80  24

#define MOTOR_UMAX          0
#define MOTOR_5345          1
#define MOTOR_ST24          2
#define MOTOR_HP2400        3
#define MOTOR_HP2300        4
#define MOTOR_CANONLIDE35   5
#define MOTOR_XP200         6
#define MOTOR_XP300         7
#define MOTOR_HP3670        9
#define MOTOR_DP665        10
#define MOTOR_ROADWARRIOR  11
#define MOTOR_DSMOBILE_600 12
#define MOTOR_CANONLIDE200 13
#define MOTOR_CANONLIDE100 14
#define MOTOR_KVSS080      15
#define MOTOR_G4050        16
#define MOTOR_CANONLIDE110 17
#define MOTOR_PLUSTEK_3600 18
#define MOTOR_CANONLIDE700 19
#define MOTOR_CS8400F      20
#define MOTOR_IMG101       21
#define MOTOR_PLUSTEK3800  22
#define MOTOR_CANONLIDE210 23
#define MOTOR_CANONLIDE80  24


/* Forward typedefs */
typedef struct Genesys_Device Genesys_Device;
struct Genesys_Scanner;
typedef struct Genesys_Calibration_Cache  Genesys_Calibration_Cache;

/**
 * Scanner command set description.
 *
 * This description contains parts which are common to all scanners with the
 * same command set, but may have different optical resolution and other
 * parameters.
 */
typedef struct Genesys_Command_Set
{
  /** @name Identification */
  /*@{ */

  /** Name of this command set */
  SANE_String_Const name;

  /*@} */

  /** For ASIC initialization */
    SANE_Status (*init) (Genesys_Device * dev);

    SANE_Status (*init_regs_for_warmup) (Genesys_Device * dev,
					 Genesys_Register_Set * regs,
					 int *channels, int *total_size);
    SANE_Status (*init_regs_for_coarse_calibration) (Genesys_Device * dev);
    SANE_Status (*init_regs_for_shading) (Genesys_Device * dev);
    SANE_Status (*init_regs_for_scan) (Genesys_Device * dev);

    SANE_Bool (*get_filter_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_lineart_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_bitset_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_gain4_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_fast_feed_bit) (Genesys_Register_Set * reg);

    SANE_Bool (*test_buffer_empty_bit) (SANE_Byte val);
    SANE_Bool (*test_motor_flag_bit) (SANE_Byte val);

  int (*bulk_full_size) (void);

    SANE_Status (*set_fe) (Genesys_Device * dev, uint8_t set);
    SANE_Status (*set_powersaving) (Genesys_Device * dev, int delay);
    SANE_Status (*save_power) (Genesys_Device * dev, SANE_Bool enable);

  void (*set_motor_power) (Genesys_Register_Set * regs, SANE_Bool set);
  void (*set_lamp_power) (Genesys_Device * dev,
			  Genesys_Register_Set * regs,
			  SANE_Bool set);

    SANE_Status (*begin_scan) (Genesys_Device * dev,
			       Genesys_Register_Set * regs,
			       SANE_Bool start_motor);
    SANE_Status (*end_scan) (Genesys_Device * dev,
			     Genesys_Register_Set * regs,
			     SANE_Bool check_stop);

    /**
     * Send gamma tables to ASIC
     */
    SANE_Status (*send_gamma_table) (Genesys_Device * dev);

    SANE_Status (*search_start_position) (Genesys_Device * dev);
    SANE_Status (*offset_calibration) (Genesys_Device * dev);
    SANE_Status (*coarse_gain_calibration) (Genesys_Device * dev, int dpi);
    SANE_Status (*led_calibration) (Genesys_Device * dev);

    SANE_Status (*slow_back_home) (Genesys_Device * dev,
				   SANE_Bool wait_until_home);

    SANE_Status (*bulk_write_register) (Genesys_Device * dev,
					Genesys_Register_Set * reg,
					size_t elems);
    SANE_Status (*bulk_write_data) (Genesys_Device * dev, uint8_t addr,
				    uint8_t * data, size_t len);

    SANE_Status (*bulk_read_data) (Genesys_Device * dev, uint8_t addr,
				   uint8_t * data, size_t len);

  /* Updates hardware sensor information in Genesys_Scanner.val[].
     If possible, just get information for given option.
     The sensor state in Genesys_Scanner.val[] should be merged with the
     new sensor state, using the information that was last read by the frontend
     in Genesys_Scanner.last_val[], in such a way that a button up/down
     relative to Genesys_Scanner.last_val[] is not lost.
   */
  SANE_Status (*update_hardware_sensors) (struct Genesys_Scanner * s);

    /* functions for sheetfed scanners */
    /**
     * load document into scanner
     */
    SANE_Status (*load_document) (Genesys_Device * dev);
    /**
     * detects is the scanned document has left scanner. In this
     * case it updates the amount of data to read and set up
     * flags in the dev struct
     */
    SANE_Status (*detect_document_end) (Genesys_Device * dev);
    /**
     * eject document from scanner
     */
    SANE_Status (*eject_document) (Genesys_Device * dev);
    /**
     * search for an black or white area in forward or reverse
     * direction */
    SANE_Status (*search_strip) (Genesys_Device * dev, SANE_Bool forward, SANE_Bool black);

    SANE_Status (*is_compatible_calibration) (
	Genesys_Device * dev,
	Genesys_Calibration_Cache *cache,
        SANE_Bool for_overwrite);

    /* functions for transparency adapter */
    /**
     * move scanning head to transparency adapter
     */
    SANE_Status (*move_to_ta) (Genesys_Device * dev);

    /**
     * write shading data calibration to ASIC
     */
    SANE_Status (*send_shading_data) (Genesys_Device * dev, uint8_t * data, int size);

    /**
     * calculate current scan setup
     */
    SANE_Status (*calculate_current_setup) (Genesys_Device * dev);

    /**
     * cold boot init function
     */
    SANE_Status (*asic_boot) (Genesys_Device * dev, SANE_Bool cold);

    /**
     * Scan register setting interface
     */
    SANE_Status (*init_scan_regs) (Genesys_Device * dev,
				   Genesys_Register_Set * reg,
				   float xres,
				   float yres,
				   float startx,
				   float starty,
				   float pixels,
				   float lines,
				   unsigned int depth,
				   unsigned int channels,
				   int scan_method,
				   int scan_mode,
				   int color_filter,
				   unsigned int flags);

} Genesys_Command_Set;

/** @brief structure to describe a scanner model
 * This structure describes a model. It is composed of information on the
 * sensor, the motor, scanner geometry and flags to drive operation.
 */
typedef struct Genesys_Model
{
  SANE_String_Const name;
  SANE_String_Const vendor;
  SANE_String_Const model;

  SANE_Int asic_type;		/* ASIC type gl646 or gl841 */
  Genesys_Command_Set *cmd_set;	/* pointers to low level functions */

  SANE_Int xdpi_values[MAX_RESOLUTIONS];	/* possible x resolutions */
  SANE_Int ydpi_values[MAX_RESOLUTIONS];	/* possible y resolutions */
  SANE_Int bpp_gray_values[MAX_DPI];	/* possible depths in gray mode */
  SANE_Int bpp_color_values[MAX_DPI];	/* possible depths in color mode */

  SANE_Fixed x_offset;		/* Start of scan area in mm */
  SANE_Fixed y_offset;		/* Start of scan area in mm (Amount of
				   feeding needed to get to the medium) */
  SANE_Fixed x_size;		/* Size of scan area in mm */
  SANE_Fixed y_size;		/* Size of scan area in mm */

  SANE_Fixed y_offset_calib;	/* Start of white strip in mm */
  SANE_Fixed x_offset_mark;	/* Start of black mark in mm */

  SANE_Fixed x_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed y_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed x_size_ta;		/* Size of scan area in TA mode in mm */
  SANE_Fixed y_size_ta;		/* Size of scan area in TA mode in mm */

  SANE_Fixed y_offset_calib_ta;	/* Start of white strip in TA mode in mm */

  SANE_Fixed post_scan;		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_Fixed eject_feed;	/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  /* Line-distance correction (in pixel at optical_ydpi) for CCD scanners */
  SANE_Int ld_shift_r;		/* red */
  SANE_Int ld_shift_g;		/* green */
  SANE_Int ld_shift_b;		/* blue */

  Genesys_Color_Order line_mode_color_order;	/* Order of the CCD/CIS colors */

  SANE_Bool is_cis;		/* Is this a CIS or CCD scanner? */
  SANE_Bool is_sheetfed;	/* Is this sheetfed scanner? */

  SANE_Int ccd_type;		/* which SENSOR type do we have ? */
  SANE_Int dac_type;		/* which DAC do we have ? */
  SANE_Int gpo_type;		/* General purpose output type */
  SANE_Int motor_type;		/* stepper motor type */
  SANE_Word flags;		/* Which hacks are needed for this scanner? */
  SANE_Word buttons;		/* Button flags, described existing buttons for the model */
  /*@} */
  SANE_Int shading_lines;	/* how many lines are used for shading calibration */
  SANE_Int search_lines;	/* how many lines are used to search start position */
} Genesys_Model;

#define SCAN_METHOD_FLATBED      0     /**< normal scan method */
#define SCAN_METHOD_TRANSPARENCY 2     /**< scan using transparency adaptor */
#define SCAN_METHOD_NEGATIVE     0x88  /**< scan using negative adaptor */

#define SCAN_MODE_LINEART        0 	/**< lineart scan mode */
#define SCAN_MODE_HALFTONE       1 	/**< halftone scan mode */
#define SCAN_MODE_GRAY           2 	/**< gray scan mode */
#define SCAN_MODE_COLOR          4 	/**< color scan mode */

typedef struct
{
  int scan_method;		/* todo: change >=2: Transparency, 0x88: negative film */
  int scan_mode;		/* todo: change 0,1 = lineart, halftone; 2 = gray, 3 = 3pass color, 4=single pass color */
  int xres;			/**< horizontal dpi */
  int yres;			/**< vertical dpi */

  double tl_x;			/* x start on scan table in mm */
  double tl_y;			/* y start on scan table in mm */

  unsigned int lines;		/**< number of lines at scan resolution */
  unsigned int pixels;		/**< number of pixels at scan resolution */

  unsigned int depth;/* bit depth of the scan */

  /* todo : remove these fields ? */
  int exposure_time;

  unsigned int color_filter;

  /**< true if scan is true gray, false if monochrome scan */
  int true_gray;

  /**< lineart threshold */
  int threshold;

  /**< lineart threshold curve for dynamic rasterization */
  int threshold_curve;

  /**< Disable interpolation for xres<yres*/
  int disable_interpolation;

  /**< Use double x resolution internally to provide better
   * quality */
  int double_xres;

  /**< true is lineart is generated from gray data by
   * the dynamic rasterization algo */
  int dynamic_lineart;

  /**< value for contrast enhancement in the [-100..100] range */
  int contrast;

  /**< value for brightness enhancement in the [-100..100] range */
  int brightness;
  
  /**< cahe entries expiration time */
  int expiration_time;
} Genesys_Settings;

typedef struct Genesys_Current_Setup
{
    int pixels;         /* pixel count expected from scanner */
    int lines;          /* line count expected from scanner */
    int depth;          /* depth expected from scanner */
    int channels;       /* channel count expected from scanner */
    int scan_method;	/* scanning method: flatbed or XPA */
    int exposure_time;  /* used exposure time */
    float xres;         /* used xres */
    float yres;         /* used yres*/
    SANE_Bool half_ccd; /* half ccd mode */
    SANE_Int stagger;
    SANE_Int max_shift;	/* max shift of any ccd component, including staggered pixels*/
} Genesys_Current_Setup;

typedef struct Genesys_Buffer
{
  SANE_Byte *buffer;
  size_t size;
  size_t pos;	/* current position in read buffer */
  size_t avail;	/* data bytes currently in buffer */
} Genesys_Buffer;

struct Genesys_Calibration_Cache
{
  Genesys_Current_Setup used_setup;/* used to check if entry is compatible */
  time_t last_calibration;

  Genesys_Frontend frontend;
  Genesys_Sensor sensor;

  size_t calib_pixels;
  size_t calib_channels;
  size_t average_size;
  uint8_t *white_average_data;
  uint8_t *dark_average_data;

  struct Genesys_Calibration_Cache *next;
};

/**
 * Describes the current device status for the backend
 * session. This should be more accurately called
 * Genesys_Session .
 */
struct Genesys_Device
{
  SANE_Int dn;
  SANE_Word vendorId;			/**< USB vendor identifier */
  SANE_Word productId;			/**< USB product identifier */
  SANE_Int usb_mode;			/**< USB mode: 1 for USB 1.1, 2 for USB 2.0,
					  0 unset and -1 for fake USB device */
  SANE_String file_name;
  SANE_String calib_file;
  Genesys_Model *model;

  Genesys_Register_Set reg[256];
  Genesys_Register_Set calib_reg[256];
  Genesys_Settings settings;
  Genesys_Frontend frontend;
  Genesys_Sensor sensor;
  Genesys_Gpo gpo;
  Genesys_Motor motor;
  uint16_t slope_table0[256];
  uint16_t slope_table1[256];
  uint8_t  control[6];
  time_t init_date;

  size_t average_size;
  size_t calib_pixels;	/**< number of pixels used during shading calibration */
  size_t calib_lines;	/**< number of lines used during shading calibration */
  size_t calib_channels;
  size_t calib_resolution;
  uint8_t *white_average_data;
  uint8_t *dark_average_data;
  uint16_t dark[3];

  SANE_Bool already_initialized;
  SANE_Int scanhead_position_in_steps;
  SANE_Int lamp_off_time;

  SANE_Bool read_active;
  SANE_Bool parking;		/**< signal wether the park command has been issued */
  SANE_Bool document;		/**< for sheetfed scanner's, is TRUE when there
				   is a document in the scanner */

  Genesys_Buffer read_buffer;
  Genesys_Buffer lines_buffer;
  Genesys_Buffer shrink_buffer;
  Genesys_Buffer out_buffer;
  Genesys_Buffer binarize_buffer; /**< buffer for digital lineart from gray data */
  Genesys_Buffer local_buffer;    /**< local buffer for gray data during dynamix lineart */

  size_t read_bytes_left;	/**< bytes to read from scanner */

  size_t total_bytes_read;	/**< total bytes read sent to frontend */
  size_t total_bytes_to_read;	/**< total bytes read to be sent to frontend */
  size_t wpl;			/**< asic's word per line */

  Genesys_Current_Setup current_setup; /* contains the real used values */

  /**< look up table used in dynamic rasterization */
  unsigned char lineart_lut[256];

  Genesys_Calibration_Cache *calibration_cache;

  struct Genesys_Device *next;

  SANE_Int ld_shift_r;		/**< used red line-distance shift*/
  SANE_Int ld_shift_g;		/**< used green line-distance shift*/
  SANE_Int ld_shift_b;		/**< used blue line-distance shift*/
  int segnb;       /**< number of segments composing the sensor */
  int line_interp; /**< number of lines used in line interpolation */
  int line_count;  /**< number of scan lines used during scan */
  size_t bpl;      /**< bytes per full scan widthline */
  size_t dist;     /**< bytes distance between an odd and an even pixel */
  size_t len;      /**< number of even pixels */
  size_t cur;      /**< current pixel position within sub window */
  size_t skip;     /**< number of bytes to skip at start of line */
  size_t *order;   /**< array describing the order of the sub-segments of the sensor */
  Genesys_Buffer oe_buffer; /**< buffer to handle even/odd data */

  SANE_Bool buffer_image; /**< when true the scanned picture is first buffered
			   * to allow software image enhancements */
  SANE_Byte *img_buffer; /**< image buffer where the scanned picture is stored */

  FILE *binary; /**< binary logger file */
};

typedef struct Genesys_USB_Device_Entry
{
  SANE_Word vendor;			/**< USB vendor identifier */
  SANE_Word product;			/**< USB product identifier */
  Genesys_Model *model;			/**< Scanner model information */
} Genesys_USB_Device_Entry;

/**
 * structure for motor database
 */
typedef struct {
	int motor_type;	 /**< motor id */
	int exposure;    /**< exposure for the slope table */
        int step_type;   /**< default step type for given exposure */
	uint32_t *table; /**< 0 terminated slope table at full step */
} Motor_Profile;

#define FULL_STEP       0
#define HALF_STEP       1
#define QUARTER_STEP    2
#define EIGHTH_STEP     3

#define SLOPE_TABLE_SIZE 1024

#define SCAN_TABLE 	0 	/* table 1 at 0x4000 for gl124 */
#define BACKTRACK_TABLE 1 	/* table 2 at 0x4800 for gl124 */
#define STOP_TABLE 	2 	/* table 3 at 0x5000 for gl124 */
#define FAST_TABLE 	3 	/* table 4 at 0x5800 for gl124 */
#define HOME_TABLE 	4 	/* table 5 at 0x6000 for gl124 */

#define SCAN_FLAG_SINGLE_LINE               0x001
#define SCAN_FLAG_DISABLE_SHADING           0x002
#define SCAN_FLAG_DISABLE_GAMMA             0x004
#define SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE  0x008
#define SCAN_FLAG_IGNORE_LINE_DISTANCE      0x010
#define SCAN_FLAG_USE_OPTICAL_RES           0x020
#define SCAN_FLAG_DISABLE_LAMP              0x040
#define SCAN_FLAG_DYNAMIC_LINEART           0x080
#define SCAN_FLAG_CALIBRATION               0x100
#define SCAN_FLAG_FEEDING                   0x200
#define SCAN_FLAG_USE_XPA                   0x400
#define SCAN_FLAG_ENABLE_LEDADD             0x800
#define MOTOR_FLAG_AUTO_GO_HOME             0x01
#define MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE 0x02
#define MOTOR_FLAG_FEED                     0x04
#define MOTOR_FLAG_USE_XPA                  0x08

/** @name "Optical flags" */
/*@{ optical flags available when setting up sensor for scan */

#define OPTICAL_FLAG_DISABLE_GAMMA          0x01 /**< disable gamma correction */
#define OPTICAL_FLAG_DISABLE_SHADING        0x02 /**< disable shading correction */
#define OPTICAL_FLAG_DISABLE_LAMP           0x04 /**< turn off lamp */
#define OPTICAL_FLAG_ENABLE_LEDADD          0x08 /**< enable true CIS gray by enabling LED addition */
#define OPTICAL_FLAG_DISABLE_DOUBLE         0x10 /**< disable automatic x-direction double data expansion */
#define OPTICAL_FLAG_STAGGER                0x20 /**< disable stagger correction */
#define OPTICAL_FLAG_USE_XPA                0x40 /**< use XPA lamp rather than regular one */

/*@} */

/*--------------------------------------------------------------------------*/
/*       common functions needed by low level specific functions            */
/*--------------------------------------------------------------------------*/

extern Genesys_Register_Set *sanei_genesys_get_address (Genesys_Register_Set * regs, uint16_t addr);

extern SANE_Byte
sanei_genesys_read_reg_from_set (Genesys_Register_Set * regs, uint16_t address);

extern void
sanei_genesys_set_reg_from_set (Genesys_Register_Set * regs, uint16_t address, SANE_Byte value);

extern SANE_Status sanei_genesys_init_cmd_set (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_read_register (Genesys_Device * dev, uint16_t reg, uint8_t * val);

extern SANE_Status
sanei_genesys_write_register (Genesys_Device * dev, uint16_t reg, uint8_t val);

extern SANE_Status
sanei_genesys_read_hregister (Genesys_Device * dev, uint16_t reg, uint8_t * val);

extern SANE_Status
sanei_genesys_write_hregister (Genesys_Device * dev, uint16_t reg, uint8_t val);

extern SANE_Status
sanei_genesys_bulk_write_register (Genesys_Device * dev,
			           Genesys_Register_Set * reg,
                                   size_t elems);

extern SANE_Status sanei_genesys_write_0x8c (Genesys_Device * dev, uint8_t index, uint8_t val);

extern SANE_Status sanei_genesys_get_status (Genesys_Device * dev, uint8_t * status);

extern void sanei_genesys_print_status (uint8_t val);

extern SANE_Status
sanei_genesys_write_ahb (SANE_Int dn, int usb_mode, uint32_t addr, uint32_t size, uint8_t * data);

extern void sanei_genesys_init_fe (Genesys_Device * dev);

extern void sanei_genesys_init_structs (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_init_shading_data (Genesys_Device * dev, int pixels_per_line);

extern SANE_Status sanei_genesys_read_valid_words (Genesys_Device * dev,
						  unsigned int *steps);

extern SANE_Status sanei_genesys_read_scancnt (Genesys_Device * dev,
						  unsigned int *steps);

extern SANE_Status sanei_genesys_read_feed_steps (Genesys_Device * dev,
						  unsigned int *steps);

extern void
sanei_genesys_calculate_zmode2 (SANE_Bool two_table,
				uint32_t exposure_time,
				uint16_t * slope_table,
				int reg21,
				int move, int reg22, uint32_t * z1,
				uint32_t * z2);

extern void
sanei_genesys_calculate_zmode (uint32_t exposure_time,
			       uint32_t steps_sum,
			       uint16_t last_speed, uint32_t feedl,
			       uint8_t fastfed, uint8_t scanfed,
			       uint8_t fwdstep, uint8_t tgtime,
			       uint32_t * z1, uint32_t * z2);

extern SANE_Status
sanei_genesys_set_buffer_address (Genesys_Device * dev, uint32_t addr);

/** @brief Reads data from frontend register.
 * Reads data from the given frontend register. May be used to query
 * analog frontend status by reading the right register.
 */
extern SANE_Status
sanei_genesys_fe_read_data (Genesys_Device * dev, uint8_t addr,
			    uint16_t *data);
/** @brief Write data to frontend register.
 * Writes data to analog frontend register at the given address.
 * The use and address of registers change from model to model.
 */
extern SANE_Status
sanei_genesys_fe_write_data (Genesys_Device * dev, uint8_t addr,
			     uint16_t data);

extern SANE_Int
sanei_genesys_exposure_time2 (Genesys_Device * dev,
			      float ydpi, int step_type, int endpixel,
			      int led_exposure, int power_mode);

extern SANE_Int
sanei_genesys_exposure_time (Genesys_Device * dev, Genesys_Register_Set * reg,
			     int xdpi);
extern SANE_Int
sanei_genesys_generate_slope_table (uint16_t * slope_table, unsigned int max_steps,
			      unsigned int use_steps, uint16_t stop_at,
			      uint16_t vstart, uint16_t vend,
			      unsigned int steps, double g,
			      unsigned int *used_steps, unsigned int *vfinal);

extern SANE_Int
sanei_genesys_create_slope_table (Genesys_Device * dev,
				  uint16_t * slope_table, int steps,
				  int step_type, int exposure_time,
				  SANE_Bool same_speed, double yres,
				  int power_mode);

SANE_Int
sanei_genesys_create_slope_table3 (Genesys_Device * dev,
				   uint16_t * slope_table, int max_step,
				   unsigned int use_steps,
				   int step_type, int exposure_time,
				   double yres,
				   unsigned int *used_steps,
				   unsigned int *final_exposure,
				   int power_mode);

extern void
sanei_genesys_create_gamma_table (uint16_t * gamma_table, int size,
				  float maximum, float gamma_max,
				  float gamma);

extern SANE_Status sanei_genesys_send_gamma_table (Genesys_Device * dev);

extern SANE_Status sanei_genesys_start_motor (Genesys_Device * dev);

extern SANE_Status sanei_genesys_stop_motor (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_search_reference_point (Genesys_Device * dev, uint8_t * data,
				      int start_pixel, int dpi, int width,
				      int height);

extern SANE_Status
sanei_genesys_write_pnm_file (char *filename, uint8_t * data, int depth,
			      int channels, int pixels_per_line, int lines);

extern SANE_Status
sanei_genesys_test_buffer_empty (Genesys_Device * dev, SANE_Bool * empty);

extern SANE_Status
sanei_genesys_read_data_from_scanner (Genesys_Device * dev, uint8_t * data,
				      size_t size);

extern SANE_Status
sanei_genesys_buffer_alloc(Genesys_Buffer * buf, size_t size);

extern SANE_Status
sanei_genesys_buffer_free(Genesys_Buffer * buf);

extern SANE_Byte *
sanei_genesys_buffer_get_write_pos(Genesys_Buffer * buf, size_t size);

extern SANE_Byte *
sanei_genesys_buffer_get_read_pos(Genesys_Buffer * buf);

extern SANE_Status
sanei_genesys_buffer_produce(Genesys_Buffer * buf, size_t size);

extern SANE_Status
sanei_genesys_buffer_consume(Genesys_Buffer * buf, size_t size);

extern SANE_Status
sanei_genesys_set_double(Genesys_Register_Set *regs, uint16_t addr, uint16_t value);

extern SANE_Status
sanei_genesys_set_triple(Genesys_Register_Set *regs, uint16_t addr, uint32_t value);

extern SANE_Status
sanei_genesys_get_double(Genesys_Register_Set *regs, uint16_t addr, uint16_t *value);

extern SANE_Status
sanei_genesys_get_triple(Genesys_Register_Set *regs, uint16_t addr, uint32_t *value);

extern SANE_Status
sanei_genesys_wait_for_home(Genesys_Device *dev);

extern SANE_Status
sanei_genesys_asic_init(Genesys_Device *dev, SANE_Bool cold);

extern
int sanei_genesys_compute_dpihw(Genesys_Device *dev, int xres);

extern
Motor_Profile *sanei_genesys_get_motor_profile(Motor_Profile *motors, int motor_type, int exposure);

extern
int sanei_genesys_compute_step_type(Motor_Profile *motors, int motor_type, int exposure);

extern
int sanei_genesys_slope_table(uint16_t *slope, int *steps, int dpi, int exposure, int base_dpi, int step_type, int factor, int motor_type, Motor_Profile *motors);

/** @brief find lowest motor resolution for the device.
 * Parses the resolution list for motor and
 * returns the lowest value.
 * @param dev for which to find the lowest motor resolution
 * @return the lowest available motor resolution for the device
 */
extern
int sanei_genesys_get_lowest_ydpi(Genesys_Device *dev);

/** @brief find lowest resolution for the device.
 * Parses the resolution list for motor and sensor and
 * returns the lowest value.
 * @param dev for which to find the lowest resolution
 * @return the lowest available resolution for the device
 */
extern
int sanei_genesys_get_lowest_dpi(Genesys_Device *dev);

/**
 * reads previously cached calibration data
 * from file
 */
extern SANE_Status
sanei_genesys_read_calibration (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_is_compatible_calibration (Genesys_Device * dev,
				 Genesys_Calibration_Cache * cache,
				 int for_overwrite);

/** @brief compute maximum line distance shift
 * compute maximum line distance shift for the motor and sensor
 * combination. Line distance shift is the distance between different
 * color component of CCD sensors. Since these components aren't at
 * the same physical place, they scan diffrent lines. Software must
 * take this into account to accurately mix color data.
 * @param dev device session to compute max_shift for
 * @param channels number of color channels for the scan
 * @param yres motor resolution used for the scan
 * @param flags scan flags
 * @return 0 or line distance shift
 */
extern
int sanei_genesys_compute_max_shift(Genesys_Device *dev,
                                    int channels,
                                    int yres,
                                    int flags);

extern SANE_Status
sanei_genesys_load_lut (unsigned char * lut,
                        int in_bits,
                        int out_bits,
                        int out_min,
                        int out_max,
                        int slope,
                        int offset);

extern SANE_Status
sanei_genesys_generate_gamma_buffer(Genesys_Device * dev,
                                    int bits,
                                    int max,
                                    int size,
                                    uint8_t *gamma);

#ifdef UNIT_TESTING
SANE_Status
genesys_send_offset_and_shading (Genesys_Device * dev,
          	                 uint8_t * data,
				 int size);

void
genesys_average_data (uint8_t * average_data,
		      uint8_t * calibration_data,
                      uint32_t lines,
		      uint32_t pixel_components_per_line);

void
compute_averaged_planar (Genesys_Device * dev,
			     uint8_t * shading_data,
			     unsigned int pixels_per_line,
			     unsigned int words_per_color,
			     unsigned int channels,
			     unsigned int o,
			     unsigned int coeff,
			     unsigned int target_bright,
			     unsigned int target_dark);


void
compute_coefficients (Genesys_Device * dev,
		      uint8_t * shading_data,
		      unsigned int pixels_per_line,
		      unsigned int channels,
		      unsigned int cmat[3],
		      int offset,
		      unsigned int coeff,
		      unsigned int target);

void
compute_planar_coefficients (Genesys_Device * dev,
			     uint8_t * shading_data,
			     unsigned int factor,
			     unsigned int pixels_per_line,
			     unsigned int words_per_color,
			     unsigned int channels,
			     unsigned int cmat[3],
			     unsigned int offset,
			     unsigned int coeff,
			     unsigned int target);

void
compute_shifted_coefficients (Genesys_Device * dev,
			      uint8_t * shading_data,
			      unsigned int pixels_per_line,
			      unsigned int channels,
			      unsigned int cmat[3],
			      int offset,
			      unsigned int coeff,
			      unsigned int target_dark,
			      unsigned int target_bright,
			      unsigned int patch_size);		/* contigous extent */

SANE_Status
probe_genesys_devices (void);

SANE_Status genesys_flatbed_calibration (Genesys_Device *dev);

SANE_Status genesys_send_shading_coefficient (Genesys_Device *dev);
#endif


/*---------------------------------------------------------------------------*/
/*                ASIC specific functions declarations                       */
/*---------------------------------------------------------------------------*/
extern SANE_Status sanei_gl646_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl841_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl843_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl846_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl847_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl124_init_cmd_set (Genesys_Device * dev);

#endif /* not GENESYS_LOW_H */
