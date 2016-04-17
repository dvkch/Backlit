/* sane - Scanner Access Now Easy.

   Copyright (C) 2003-2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004-2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>

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

#include "genesys.h"

/*
 * Genesys Logic GL646 based scanners
 */
/* Individual bits */
#define REG01_CISSET	0x80
#define REG01_DOGENB	0x40
#define REG01_DVDSET	0x20
#define REG01_FASTMOD	0x10
#define REG01_COMPENB	0x08
#define REG01_DRAMSEL	0x04
#define REG01_SHDAREA	0x02
#define REG01_SCAN	0x01

#define REG02_NOTHOME	0x80
#define REG02_ACDCDIS	0x40
#define REG02_AGOHOME	0x20
#define REG02_MTRPWR	0x10
#define REG02_FASTFED	0x08
#define REG02_MTRREV	0x04
#define REG02_STEPSEL	0x03

#define REG02_FULLSTEP	0x00
#define REG02_HALFSTEP	0x01
#define REG02_QUATERSTEP	0x02

#define REG03_TG3	0x80
#define REG03_AVEENB	0x40
#define REG03_XPASEL	0x20
#define REG03_LAMPPWR	0x10
#define REG03_LAMPDOG	0x08
#define REG03_LAMPTIM	0x07

#define REG04_LINEART	0x80
#define REG04_BITSET	0x40
#define REG04_ADTYPE	0x30
#define REG04_FILTER	0x0c
#define REG04_FESET	0x03

#define REG05_DPIHW	0xc0
#define REG05_DPIHW_600	0x00
#define REG05_DPIHW_1200	0x40
#define REG05_DPIHW_2400	0x80
#define REG05_DPIHW_4800	0xc0
#define REG05_GMMTYPE	0x30
#define REG05_GMM14BIT  0x10
#define REG05_GMMENB	0x08
#define REG05_LEDADD	0x04
#define REG05_BASESEL	0x03

#define REG06_PWRBIT	0x10
#define REG06_GAIN4	0x08
#define REG06_OPTEST	0x07

#define REG07_DMASEL	0x02
#define REG07_DMARDWR	0x01

#define REG16_CTRLHI	0x80
#define REG16_SELINV	0x40
#define REG16_TGINV	0x20
#define REG16_CK1INV	0x10
#define REG16_CK2INV	0x08
#define REG16_CTRLINV	0x04
#define REG16_CKDIS	0x02
#define REG16_CTRLDIS	0x01

#define REG17_TGMODE	0xc0
#define REG17_TGMODE_NO_DUMMY	0x00
#define REG17_TGMODE_REF	0x40
#define REG17_TGMODE_XPA	0x80
#define REG17_TGW	0x3f

#define REG18_CNSET	0x80
#define REG18_DCKSEL	0x60
#define REG18_CKTOGGLE	0x10
#define REG18_CKDELAY	0x0c
#define REG18_CKSEL	0x03

#define REG1D_CKMANUAL	0x80

#define REG1E_WDTIME	0xf0
#define REG1E_LINESEL	0x0f

#define REG41_PWRBIT	0x80
#define REG41_BUFEMPTY	0x40
#define REG41_FEEDFSH	0x20
#define REG41_SCANFSH	0x10
#define REG41_HOMESNR	0x08
#define REG41_LAMPSTS	0x04
#define REG41_FEBUSY	0x02
#define REG41_MOTMFLG	0x01

#define REG66_LOW_CURRENT	0x10

#define REG6A_FSTPSEL	0xc0
#define REG6A_FASTPWM	0x3f

#define REG6C_TGTIME	0xc0
#define REG6C_Z1MOD	0x38
#define REG6C_Z2MOD	0x07

#define REG_SCANFED     0x1f
#define REG_BUFSEL      0x20
#define REG_LINCNT      0x25
#define REG_DPISET      0x2c
#define REG_STRPIXEL    0x30
#define REG_ENDPIXEL    0x32
#define REG_DUMMY       0x34
#define REG_MAXWD       0x35
#define REG_LPERIOD     0x38
#define REG_FEEDL       0x3d
#define REG_VALIDWORD   0x42
#define REG_FEDCNT      0x48
#define REG_SCANCNT     0x4b
#define REG_Z1MOD       0x60
#define REG_Z2MOD       0x62


#include "genesys.h"

enum
{
  reg_0x01 = 0,
  reg_0x02,
  reg_0x03,
  reg_0x04,
  reg_0x05,
  reg_0x06,
  reg_0x07,
  reg_0x08,
  reg_0x09,
  reg_0x0a,
  reg_0x0b,
  reg_0x10,
  reg_0x11,
  reg_0x12,
  reg_0x13,
  reg_0x14,
  reg_0x15,
  reg_0x16,
  reg_0x17,
  reg_0x18,
  reg_0x19,
  reg_0x1a,
  reg_0x1b,
  reg_0x1c,
  reg_0x1d,
  reg_0x1e,
  reg_0x1f,
  reg_0x20,
  reg_0x21,
  reg_0x22,
  reg_0x23,
  reg_0x24,
  reg_0x25,
  reg_0x26,
  reg_0x27,
  reg_0x28,
  reg_0x29,
  reg_0x2c,
  reg_0x2d,
  reg_0x2e,
  reg_0x2f,
  reg_0x30,
  reg_0x31,
  reg_0x32,
  reg_0x33,
  reg_0x34,
  reg_0x35,
  reg_0x36,
  reg_0x37,
  reg_0x38,
  reg_0x39,
  reg_0x3d,
  reg_0x3e,
  reg_0x3f,
  reg_0x52,
  reg_0x53,
  reg_0x54,
  reg_0x55,
  reg_0x56,
  reg_0x57,
  reg_0x58,
  reg_0x59,
  reg_0x5a,
  reg_0x5b,
  reg_0x5c,
  reg_0x5d,
  reg_0x5e,
  reg_0x60,
  reg_0x61,
  reg_0x62,
  reg_0x63,
  reg_0x64,
  reg_0x65,
  reg_0x66,
  reg_0x67,
  reg_0x68,
  reg_0x69,
  reg_0x6a,
  reg_0x6b,
  reg_0x6c,
  reg_0x6d,
  GENESYS_GL646_MAX_REGS
};

#ifndef UNIT_TESTING
static
#endif
SANE_Status gl646_set_fe (Genesys_Device * dev, uint8_t set, int dpi);

#ifndef UNIT_TESTING
static
#endif
SANE_Status gl646_public_set_fe (Genesys_Device * dev, uint8_t set);

GENESYS_STATIC
SANE_Status
gl646_save_power (Genesys_Device * dev, SANE_Bool enable);

GENESYS_STATIC
SANE_Status
gl646_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home);

GENESYS_STATIC
SANE_Status
gl646_move_to_ta (Genesys_Device * dev);

/**
 * sets up the scanner for a scan, registers, gamma tables, shading tables
 * and slope tables, based on the parameter struct.
 * @param device   device to set up
 * @param regs     registers to set up
 * @param settings settings of the scan
 * @param split    true if move before scan has to be done
 * @param xcorrection true if scanner's X geometry must be taken into account to
 * 		     compute X, ie add left margins
 * @param ycorrection true if scanner's Y geometry must be taken into account to
 * 		     compute Y, ie add top margins
 */
GENESYS_STATIC SANE_Status
setup_for_scan (Genesys_Device *device,
		Genesys_Register_Set *regs,
		Genesys_Settings settings,
		SANE_Bool split,
		SANE_Bool xcorrection,
		SANE_Bool ycorrection);

/**
 * sets up the registers for a scan corresponding to the settings.
 * Builds motor slope tables. Computes buffer sizes and data amount to
 * transfer. It also sets up analog frontend.
 * */
static SANE_Status
gl646_setup_registers (Genesys_Device * dev,
		       Genesys_Register_Set * regs,
		       Genesys_Settings scan_settings,
		       uint16_t * slope_table1,
		       uint16_t * slope_table2,
		       SANE_Int resolution,
		       uint32_t move,
		       uint32_t linecnt,
		       uint16_t startx,
		       uint16_t endx, SANE_Bool color, SANE_Int depth);

/**
 * Does a simple move of the given distance by doing a scan at lowest resolution
 * shading correction. Memory for data is allocated in this function
 * and must be freed by caller.
 * @param dev device of the scanner
 * @param distance distance to move in MM
 */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
simple_move (Genesys_Device * dev, SANE_Int distance);

/**
 * Does a simple scan of the area given by the settings. Scanned data
 * it put in an allocated area which must be freed by the caller.
 * and slope tables, based on the parameter struct. There is no shading
 * correction while gamma correction is active.
 * @param dev      device to set up
 * @param settings settings of the scan
 * @param move     flag to enable scanhead to move
 * @param forward  flag to tell movement direction
 * @param shading  flag to tell if shading correction should be done
 * @param data     pointer that will point to the scanned data
 */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
simple_scan (Genesys_Device * dev, Genesys_Settings settings, SANE_Bool move, SANE_Bool forward,
	     SANE_Bool shading, unsigned char **data);

/**
 * Send the stop scan command
 * */
static SANE_Status
end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
	  SANE_Bool check_stop, SANE_Bool eject);
/**
 * writes control data to an area behind the last motor table.
 */
static SANE_Status write_control (Genesys_Device * dev, int resolution);


/**
 * initialize scanner's registers at SANE init time
 */
static void gl646_init_regs (Genesys_Device * dev);

#ifndef UNIT_TESTING
static
#endif
SANE_Status gl646_load_document (Genesys_Device * dev);

static SANE_Status
gl646_detect_document_end (Genesys_Device * dev);

#define FULL_STEP   0
#define HALF_STEP   1
#define QUATER_STEP 2

#define CALIBRATION_LINES 10

/**
 * master motor settings table entry
 */
typedef struct
{
  /* key */
  SANE_Int motor;
  SANE_Int dpi;
  SANE_Bool color;

  /* settings */
  SANE_Int ydpi;		/* real motor dpi, may be different from the resolution */
  SANE_Int steptype;		/* 0=full, 1=half, 2=quarter */
  SANE_Bool fastmod;		/* fast scanning 0/1 */
  SANE_Bool fastfed;		/* fast fed slope tables */
  SANE_Int mtrpwm;
  SANE_Int steps1;		/* table 1 informations */
  SANE_Int vstart1;
  SANE_Int vend1;
  SANE_Int steps2;		/* table 2 informations */
  SANE_Int vstart2;
  SANE_Int vend2;
  float g1;
  float g2;
  SANE_Int fwdbwd;		/* forward/backward steps */
} Motor_Master;

/**
 * master sensor settings table entry
 */
typedef struct
{
  /* key */
  SANE_Int sensor;	/**< sensor identifier */
  SANE_Int dpi;		/**< required dpi */
  SANE_Bool color;	/**< SANE_TRUE if color scan */

  /* settings */
  SANE_Int xdpi;		/**< real sensor dpi, may be different from the required resolution */
  SANE_Int exposure;		/**< exposure time */
  SANE_Int dpiset;		/**< set sensor dpi */
  SANE_Int cksel;		/**< dpiset 'divisor', part of reg 18h */
  SANE_Int dummy;		/**< dummy exposure time */
  /* uint8_t regs_0x10_0x15[6];*/
  uint8_t *regs_0x10_0x15; 	/**< per color exposure time for CIS scanners */
  SANE_Bool half_ccd;		/**> true if manual CCD/2 clock programming or real dpi is half dpiset */
  uint8_t r18;			/**> content of register 18h */
  uint8_t r1d;			/**> content of register 1dh */
} Sensor_Master;

/**
 * settings for a given resolution and DPISET
 * TODO clean up this when all scanners will have been added
 */
typedef struct
{
  /* key */
  SANE_Int sensor;
  SANE_Int cksel;

  /* values */
  uint8_t regs_0x08_0x0b[4];	/**< settings for normal CCD clock */
  uint8_t manual_0x08_0x0b[4];	/**< settings for CCD/2 clock */
  uint8_t regs_0x16_0x1d[8];
  uint8_t regs_0x52_0x5e[13];
  uint8_t manual_0x52_0x58[7];
} Sensor_Settings;

static uint8_t xp200_color[6]={0x16, 0x44, 0x0c, 0x80, 0x09, 0x2e};
static uint8_t xp200_gray[6]={0x05, 0x0a, 0x0f, 0xa0, 0x10, 0x10};

/**
 * master sensor settings, for a given sensor and dpi,
 * it gives exposure and CCD time
 */
/* *INDENT-OFF* */
static Sensor_Master sensor_master[] = {
  /* HP3670 master settings */
  {CCD_HP3670,  75, SANE_TRUE ,   75,  4879,  300, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 100, SANE_TRUE ,  100,  4487,  400, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 150, SANE_TRUE ,  150,  4879,  600, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 300, SANE_TRUE ,  300,  4503, 1200, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 600, SANE_TRUE ,  600, 10251, 1200, 2, 42, NULL, SANE_FALSE, 0x31, 0x43},
  {CCD_HP3670,1200, SANE_TRUE , 1200, 12750, 1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x43},
  {CCD_HP3670,2400, SANE_TRUE , 1200, 12750, 1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x43},
  {CCD_HP3670,  75, SANE_FALSE,   75,  4879,  300, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 100, SANE_FALSE,  100,  4487,  400, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 150, SANE_FALSE,  150,  4879,  600, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 300, SANE_FALSE,  300,  4503, 1200, 4, 42, NULL, SANE_FALSE, 0x33, 0x43},
  {CCD_HP3670, 600, SANE_FALSE,  600, 10251, 1200, 2, 42, NULL, SANE_FALSE, 0x31, 0x43},
  {CCD_HP3670,1200, SANE_FALSE, 1200, 12750, 1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x43},
  {CCD_HP3670,2400, SANE_FALSE, 1200, 12750, 1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x43},

  /* HP 2400 master settings */
  {CCD_HP2400,  50, SANE_TRUE ,   50,  7211,  200, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 100, SANE_TRUE ,  100,  7211,  400, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 150, SANE_TRUE ,  150,  7211,  600, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 300, SANE_TRUE ,  300,  8751, 1200, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 600, SANE_TRUE ,  600, 18760, 1200, 2, 42, NULL, SANE_FALSE, 0x31, 0x02},
  {CCD_HP2400,1200, SANE_TRUE , 1200, 21749, 1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x42},
  {CCD_HP2400,  50, SANE_FALSE,   50,  7211,  200, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 100, SANE_FALSE,  100,  7211,  400, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 150, SANE_FALSE,  150,  7211,  600, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 300, SANE_FALSE,  300,  8751, 1200, 4, 42, NULL, SANE_FALSE, 0x3f, 0x02},
  {CCD_HP2400, 600, SANE_FALSE,  600, 18760, 1200, 2, 42, NULL, SANE_FALSE, 0x31, 0x02},
  {CCD_HP2400,1200, SANE_FALSE, 1200, 21749, 1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x42},

  /* XP 200 master settings */
  {CIS_XP200 ,  75, SANE_TRUE ,  75,  5700,  75, 1, 42, xp200_color, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 100, SANE_TRUE , 100,  5700, 100, 1, 42, xp200_color, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 200, SANE_TRUE , 200,  5700, 200, 1, 42, xp200_color, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 300, SANE_TRUE , 300,  9000, 300, 1, 42, xp200_color, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 600, SANE_TRUE , 600, 16000, 600, 1, 42, xp200_color, SANE_FALSE, 0x00, 0x11},

  {CIS_XP200 ,  75, SANE_FALSE,  75, 16000,  75, 1, 42, xp200_gray, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 100, SANE_FALSE, 100,  7800, 100, 1, 42, xp200_gray, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 200, SANE_FALSE, 200, 11000, 200, 1, 42, xp200_gray, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 300, SANE_FALSE, 300, 13000, 300, 1, 42, xp200_gray, SANE_FALSE, 0x00, 0x11},
  {CIS_XP200 , 600, SANE_FALSE, 600, 24000, 600, 1, 42, xp200_gray, SANE_FALSE, 0x00, 0x11},

  /* HP 2300 master settings */
  {CCD_HP2300,  75, SANE_TRUE ,  75,  4480, 150, 1, 42, NULL, SANE_TRUE , 0x20, 0x85},
  {CCD_HP2300, 150, SANE_TRUE , 150,  4350, 300, 1, 42, NULL, SANE_TRUE , 0x20, 0x85},
  {CCD_HP2300, 300, SANE_TRUE,  300,  4350, 600, 1, 42, NULL, SANE_TRUE , 0x20, 0x85},
  {CCD_HP2300, 600, SANE_TRUE , 600,  8700, 600, 1, 42, NULL, SANE_FALSE, 0x20, 0x05},
  {CCD_HP2300,1200, SANE_TRUE , 600,  8700, 600, 1, 42, NULL, SANE_FALSE, 0x20, 0x05},
  {CCD_HP2300,  75, SANE_FALSE,  75,  4480, 150, 1, 42, NULL, SANE_TRUE , 0x20, 0x85},
  {CCD_HP2300, 150, SANE_FALSE, 150,  4350, 300, 1, 42, NULL, SANE_TRUE , 0x20, 0x85},
  {CCD_HP2300, 300, SANE_FALSE, 300,  4350, 600, 1, 42, NULL, SANE_TRUE , 0x20, 0x85},
  {CCD_HP2300, 600, SANE_FALSE, 600,  8700, 600, 1, 42, NULL, SANE_FALSE, 0x20, 0x05},
  {CCD_HP2300,1200, SANE_FALSE, 600,  8700, 600, 1, 42, NULL, SANE_FALSE, 0x20, 0x05},
  /* non half ccd 300 dpi settings
  {CCD_HP2300, 300, SANE_TRUE , 300,  8700, 300, 1, 42, NULL, SANE_FALSE, 0x20, 0x05},
  {CCD_HP2300, 300, SANE_FALSE, 300,  8700, 300, 1, 42, NULL, SANE_FALSE, 0x20, 0x05},
  */

  /* MD5345/6471 master settings */
  {CCD_5345  ,  50, SANE_TRUE ,  50, 12000, 100, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  ,  75, SANE_TRUE ,  75, 11000, 150, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 100, SANE_TRUE , 100, 11000, 200, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 150, SANE_TRUE , 150, 11000, 300, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 200, SANE_TRUE , 200, 11000, 400, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 300, SANE_TRUE , 300, 11000, 600, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 400, SANE_TRUE , 400, 11000, 800, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 600, SANE_TRUE , 600, 11000,1200, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  ,1200, SANE_TRUE ,1200, 11000,1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x03},
  {CCD_5345  ,2400, SANE_TRUE ,1200, 11000,1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x03},
  {CCD_5345  ,  50, SANE_FALSE,  50, 12000, 100, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  ,  75, SANE_FALSE,  75, 11000, 150, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 100, SANE_FALSE, 100, 11000, 200, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 150, SANE_FALSE, 150, 11000, 300, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 200, SANE_FALSE, 200, 11000, 400, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 300, SANE_FALSE, 300, 11000, 600, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 400, SANE_FALSE, 400, 11000, 800, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  , 600, SANE_FALSE, 600, 11000,1200, 1, 42, NULL, SANE_TRUE , 0x28, 0x03},
  {CCD_5345  ,1200, SANE_FALSE,1200, 11000,1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x03},
  {CCD_5345  ,2400, SANE_FALSE,1200, 11000,1200, 1, 42, NULL, SANE_FALSE, 0x30, 0x03},

};

/**
 * master motor settings, for a given motor and dpi,
 * it gives steps and speed informations
 */
static Motor_Master motor_master[] = {
  /* HP3670 motor settings */
  {MOTOR_HP3670,  75, SANE_TRUE ,  75, FULL_STEP, SANE_FALSE, SANE_TRUE , 1, 200,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 100, SANE_TRUE , 100, HALF_STEP, SANE_FALSE, SANE_TRUE , 1, 143,  2905,  187, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 150, SANE_TRUE , 150, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  73,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 300, SANE_TRUE , 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  11,  1055,  563, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 600, SANE_TRUE , 600, FULL_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 10687, 5126, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,1200, SANE_TRUE ,1200, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 6375, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,2400, SANE_TRUE ,2400, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 12750, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,  75, SANE_FALSE,  75, FULL_STEP, SANE_FALSE, SANE_TRUE , 1, 200,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 100, SANE_FALSE, 100, HALF_STEP, SANE_FALSE, SANE_TRUE , 1, 143,  2905,  187, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 150, SANE_FALSE, 150, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  73,  3429,  305, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 300, SANE_FALSE, 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 1,  11,  1055,  563, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670, 600, SANE_FALSE, 600, FULL_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 10687, 5126, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,1200, SANE_FALSE,1200, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 6375, 192, 3399, 337, 0.3, 0.4, 192},
  {MOTOR_HP3670,2400, SANE_TRUE ,2400, HALF_STEP, SANE_FALSE, SANE_TRUE , 0,   3, 15937, 12750, 192, 3399, 337, 0.3, 0.4, 192},

  /* HP2400/G2410 motor settings base motor dpi = 600 */
  {MOTOR_HP2400,  50, SANE_TRUE ,  50, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 100, SANE_TRUE , 100, HALF_STEP, SANE_FALSE, SANE_TRUE,  63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 150, SANE_TRUE , 150, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 67, 15902,   902, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 300, SANE_TRUE , 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 32, 16703,  2188, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 600, SANE_TRUE , 600, FULL_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 18761, 18761, 192, 4905,  627, 0.30, 0.4, 192},
  {MOTOR_HP2400,1200, SANE_TRUE ,1200, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 43501, 43501, 192, 4905,  627, 0.30, 0.4, 192},
  {MOTOR_HP2400,  50, SANE_FALSE,  50, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 100, SANE_FALSE, 100, HALF_STEP, SANE_FALSE, SANE_TRUE,  63, 120, 8736,   601, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 150, SANE_FALSE, 150, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 67, 15902,   902, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 300, SANE_FALSE, 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 63, 32, 16703,  2188, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400, 600, SANE_FALSE, 600, FULL_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 18761, 18761, 192, 4905,  337, 0.30, 0.4, 192},
  {MOTOR_HP2400,1200, SANE_FALSE,1200, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  3, 43501, 43501, 192, 4905,  337, 0.30, 0.4, 192},

  /* XP 200 motor settings */
  {MOTOR_XP200,  75,  SANE_TRUE,  75, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  2136, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 100,  SANE_TRUE, 100, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  2850, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 200,  SANE_TRUE, 200, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6999,  5700, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 250,  SANE_TRUE, 250, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6999,  6999, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 300,  SANE_TRUE, 300, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4, 13500, 13500, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 600,  SANE_TRUE, 600, HALF_STEP, SANE_TRUE ,  SANE_TRUE, 0, 4, 31998, 31998, 2, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200,  75, SANE_FALSE,  75, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  2000, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 100, SANE_FALSE, 100, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6000,  1300, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 200, SANE_FALSE, 200, HALF_STEP, SANE_TRUE ,  SANE_TRUE, 0, 4,  6000,  3666, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 300, SANE_FALSE, 300, HALF_STEP, SANE_TRUE , SANE_FALSE, 0, 4,  6500,  6500, 8, 12000, 1200, 0.3, 0.5, 1},
  {MOTOR_XP200, 600, SANE_FALSE, 600, HALF_STEP, SANE_TRUE ,  SANE_TRUE, 0, 4, 24000, 24000, 2, 12000, 1200, 0.3, 0.5, 1},

  /* HP scanjet 2300c */
  {MOTOR_HP2300,  75,  SANE_TRUE,  75, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120,  8139,   560, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 150,  SANE_TRUE, 150, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  67,  7903,   543, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 300,  SANE_TRUE, 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  2175,  1087, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 600,  SANE_TRUE, 600, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  8700,  4350, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300,1200,  SANE_TRUE,1200, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3, 17400,  8700, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300,  75, SANE_FALSE,  75, FULL_STEP, SANE_FALSE, SANE_TRUE , 63, 120,  8139,   560, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 150, SANE_FALSE, 150, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  67,  7903,   543, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 300, SANE_FALSE, 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  2175,  1087, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 600, SANE_FALSE, 600, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3,  8700,  4350, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300,1200, SANE_FALSE,1200, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,   3, 17400,  8700, 120, 4905,  337, 0.3, 0.4, 16},
  /* non half ccd settings for 300 dpi
  {MOTOR_HP2300, 300,  SANE_TRUE, 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  44,  5386,  2175, 120, 4905,  337, 0.3, 0.4, 16},
  {MOTOR_HP2300, 300, SANE_FALSE, 300, HALF_STEP, SANE_FALSE, SANE_TRUE , 63,  44,  5386,  2175, 120, 4905,  337, 0.3, 0.4, 16},
  */

  /* MD5345/6471 motor settings */
  /* vfinal=(exposure/(1200/dpi))/step_type */
  {MOTOR_5345,    50, SANE_TRUE ,  50, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   250, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,    75, SANE_TRUE ,  75, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   343, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   100, SANE_TRUE , 100, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   458, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   150, SANE_TRUE , 150, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   687, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   200, SANE_TRUE , 200, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   916, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   300, SANE_TRUE,  300, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,  1375, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   400, SANE_TRUE,  400, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2000,  1833, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   500, SANE_TRUE,  500, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2291,  2291, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   600, SANE_TRUE,  600, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,  1200, SANE_TRUE ,1200, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 146},
  {MOTOR_5345,  2400, SANE_TRUE ,2400, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  5500,  5500, 255, 2000,  300, 0.3, 0.4, 146},
  {MOTOR_5345,    50, SANE_FALSE,  50, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   250, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,    75, SANE_FALSE,  75, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   343, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   100, SANE_FALSE, 100, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   458, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   150, SANE_FALSE, 150, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   687, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   200, SANE_FALSE, 200, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,   916, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   300, SANE_FALSE, 300, HALF_STEP  , SANE_FALSE, SANE_TRUE , 2, 255,  2500,  1375, 255, 2000,  300, 0.3, 0.4, 64},
  {MOTOR_5345,   400, SANE_FALSE, 400, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2000,  1833, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   500, SANE_FALSE, 500, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2291,  2291, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,   600, SANE_FALSE, 600, HALF_STEP  , SANE_FALSE, SANE_TRUE , 0,  32,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 32},
  {MOTOR_5345,  1200, SANE_FALSE,1200, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  2750,  2750, 255, 2000,  300, 0.3, 0.4, 146},
  {MOTOR_5345,  2400, SANE_FALSE,2400, QUATER_STEP, SANE_FALSE, SANE_TRUE , 0,  16,  5500,  5500, 255, 2000,  300, 0.3, 0.4, 146}, /* 5500 guessed */
};

/**
 * sensor settings for a given sensor and timing method
 */
static Sensor_Settings sensor_settings[] = {
  /* HP 3670 */
  {CCD_HP3670, 1,
   {0x0d, 0x0f, 0x11, 0x13},
   {0x00, 0x00, 0x00, 0x00},
   {0x2b, 0x07, 0x30, 0x2a, 0x00, 0x00, 0xc0, 0x43},
   {0x03, 0x07, 0x0b, 0x0f, 0x13, 0x17, 0x23, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00, },
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
  },
  {CCD_HP3670, 2,
   {0x00, 0x05, 0x06, 0x08},
   {0x00, 0x00, 0x00, 0x00},
   {0x33, 0x07, 0x31, 0x2a, 0x02, 0x0e, 0xc0, 0x43},
   {0x0b, 0x0f, 0x13, 0x17, 0x03, 0x07, 0x63, 0x00, 0xc1, 0x02, 0x0e, 0x00, 0x00, },
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
  },
  {CCD_HP3670, 4,
   {0x00, 0x0a, 0x0b, 0x0d},
   {0x00, 0x00, 0x00, 0x00},
   {0x33, 0x07, 0x33, 0x2a, 0x02, 0x13, 0xc0, 0x43},
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x15, 0xc1, 0x05, 0x0a, 0x0f, 0x00, },
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
  },
  /* HP 2400 */
  {CCD_HP2400, 4,
   {0x14, 0x15, 0x00, 0x00},
   {0x00, 0x00, 0x00, 0x00},
   {0xbf, 0x08, 0x3f, 0x2a, 0x00, 0x00, 0x00, 0x02},
   {11, 15, 19, 23, 3, 7, 0x63, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00},
   {11, 15, 19, 23, 3, 7, 0x63}
   },
  {CCD_HP2400, 2,
   {14, 15, 0, 0},
   {14, 15, 0, 0},
   {0xbf, 0x08, 0x31, 0x2a, 0, 0, 0, 0x02},
   {3, 7, 11, 15, 19, 23, 0x23, 0, 0xc1, 0, 0, 0, 0},
   {3, 7, 11, 15, 19, 23, 0x23}
   },
  {CCD_HP2400, 1,
   {0x02, 0x04, 0x00, 0x00},
   {0x02, 0x04, 0x00, 0x00},
   {0xbf, 0x08, 0x30, 0x2a, 0x00, 0x00, 0xc0, 0x42},
   {0x0b, 0x0f, 0x13, 0x17, 0x03, 0x07, 0x63, 0x00, 0xc1, 0x00, 0x0e, 0x00, 0x00},
   {0x0b, 0x0f, 0x13, 0x17, 0x03, 0x07, 0x63}
   },
  {CIS_XP200, 1,
   {6, 7, 10, 4},
   {6, 7, 10, 4},
   {0x24, 0x04, 0x00, 0x2a, 0x0a, 0x0a, 0, 0x11},
   {8, 2, 0, 0, 0, 0, 0x1a, 0x51, 0, 0, 0, 0, 0},
   {8, 2, 0, 0, 0, 0, 0x1a}
   },
  {CCD_HP2300, 1,
   {0x01, 0x03, 0x04, 0x06},
   {0x16, 0x00, 0x01, 0x03},
   {0xb7, 0x0a, 0x20, 0x2a, 0x6a, 0x8a, 0x00, 0x05},
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x00, 0xc1, 0x06, 0x0b, 0x10, 0x16},
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83}
   },
  {CCD_5345, 1,
   {0x0d, 0x0f, 0x11, 0x13},
   {0x00, 0x05, 0x06, 0x08}, /* manual clock 1/2 settings or half ccd */
   {0x0b, 0x0a, 0x30, 0x2a, 0x00, 0x00, 0x00, 0x03, },
   {0x03, 0x07, 0x0b, 0x0f, 0x13, 0x17, 0x23, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00},
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83} /* half ccd settings */
  },
};
/* *INDENT-ON* */
