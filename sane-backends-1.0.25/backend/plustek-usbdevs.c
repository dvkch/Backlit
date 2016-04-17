/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbdevs.c
 *  @brief Here we have our USB device definitions.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - added EPSON1250 entries
 *        - changed reg 0x58 of EPSON Hw0x04B8_0x010F to 0x0d
 *        - reduced memory size of EPSON to 512
 *        - adjusted tpa origin of UT24
 * - 0.42 - added register 0x27, 0x2c-0x37
 *        - tweaked EPSON1250 settings according to Gene and Reinhard
 *        - tweaked HP2200 settings according to Stefan
 *        - added UMAX 3400 entries
 *        - added HP2100 settings according to Craig Smoothey
 *        - added LM9832 based U24
 *        - added Canon N650U entry
 * - 0.43 - tweaked HP 2200C entries
 *        - added _WAF_MISC_IO5 for HP lamp switching
 *        - added motor profiles
 *        - cleanup
 * - 0.44 - added EPSON 1260 and 660
 *        - added Genius Model strings
 *        - added Canon N670U entry
 *        - added bStepsToReverse to the HwDesc structure
 *        - tweaked EPSON1250 settings for TPA (thanks to Till Kamppeter)
 * - 0.45 - added UMAX motor settings
 *        - added UMAX 5400 settings
 *        - added CanoScan1240 settings (thanks to Johann Philipp)
 *        - tweaked EPSON 1260 settings
 *        - removed EPSON 660 stuff
 *        - added Canon 1220U entry
 *        - added entry for Compaq S4-100
 * - 0.46 - fine-tuning for the CanoScan devices
 *        - fixed HP2200 shading position
 *        - renamed to plustek-usbdevs.c
 * - 0.47 - added BearPaw 1200 settings for PID 0x4001 (LM9832)
 *        - tweaked the LiDE20 MCLK setting for 75DPI grayscale
 *        - enlarged the scan-area of the UMAX3400 to 11.7"
 *        - added CanoScan D660U
 * - 0.48 - added another incarnation of the UMAX 3400
 *        - added parameters for UMAX 3450 TPA
 *        - parameter tuning for CanoScan D660U
 *        - cleanup
 * - 0.49 - tweaked motor settings for EPSON and CANON1200
 *        - added support for CanoScan LiDE25
 * - 0.50 - cleanup
 *        - removed obsolete _WAF_BLACKFINE
 *        - LiDE20 does not seem to have a reliable black calibration area
 *          so the devices now will switch off the lamp for dark calibration
 *        - added Stephan Februarys' <stephanf@singnet.com.sg> LiDE25 changes
 *        - fixed high-speed feature of CanoScan D660U
 *        - tweaked LiDE25, LiDE30 and N1220U settings
 *        - changed high-speed setting for UMAX 3400, due to bugreport #302317
 *        - fixed CanoScan N650U settings
 *        - fixed CanoScan N670U settings, see (bugreport #302738)
 *        - added high-speed setting for HP2200
 * - 0.51 - tweaked CanoScan N1220U settings again
 *        - added settings for Syscan TravelScan 662
 *        - tweaked settings for Bearpaw 1200
 *        - fixed CanoScan LiDE20 settings, cause of various reports, seems
 *          Canon has built-in different motortypes
 *        - also fixed Motorsettings for LiDE30
 * - 0.52 - added Q-Scan USB001 settings
 *        - added Q-Scan USB201 settings (thanks to Hiroshi Miura)
 *        - tweaked motor settings for Bearpaw 1200
 *        - added TravelScan 464 settings
 *        - tweaked highspeed motor settings for Epson 1260
 *        - tweaked CanoScan N650U motor settings
 *
 * <hr>
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 * <hr>
 */
 
/* the other stuff is included by plustek.c ...*/
#include "plustek-usb.h"

/** for Register 0x26
 */
#define _ONE_CH_COLOR 0x04
#define _RED_CH       0x00
#define _GREEN_CH     0x08
#define _BLUE_CH      0x10


/* Plustek Model: UT12/UT16
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0017_0 =
{
	{	/* Normal */
		{0, 93},        /* DataOrigin (X: 0, Y: 8mm from home) */
		0, -1,          /* ShadingOriginY, DarkShadOrgY        */
		{2550, 3508},   /* Size                                */
		{50, 50}       /* MinDpi                              */
	},
	{	/* Positive */
		{1040 + 15, 744 - 32},/* DataOrigin (X: 7cm + 1.8cm, Y: 8mm + 5.5cm)*/
		543, -1,              /* ShadingOriginY (Y: 8mm + 3.8cm)     */
		{473, 414},           /* Size (X: 4cm, Y: 3.5cm)             */
		{150, 150}           /* MinDpi                              */
	},
	{	/* Negative */
		{1004 + 55, 744 + 12},	/* DataOrigin (X: 7cm + 1.5cm, Y: 8mm + 5.5cm)*/
		
		/* 533 blaustichig */
		537 /* hell */
		/* 543 gruenstichig  */
		
		/*543*/, -1,    /* ShadingOriginY (Y: 8mm + 3.8cm)     */
		{567, 414},     /* Size	(X: 4.8cm, Y: 3.5cm)           */
		{150, 150}     /* MinDpi                              */
	},
	{	/* Adf */
		{0, 95},        /* DataOrigin (X: 0, Y: 8mm from home) */
		0, -1,          /* ShadingOriginY, DarkShadOrgY        */
		{2550, 3508},   /* Size                                */
		{50, 50}        /* MinDpi                              */
	},
	{600, 600},                                   /* OpticDpi */
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,  /* wFlags   */
	SENSORORDER_rgb,    /* bSensorOrder    */
	4,                  /* bSensorDistance */
	4,                  /* bButtons        */
	kNEC3799,           /* bCCD            */
	0x07,               /* bPCB            */
	_WAF_NONE,          /* no workarounds or other special stuff needed */
 	_NO_MIO             /* does not use misc I/O for lamp               */
};

/* Plustek Model: U24
 * Description of the entries, see above...
 */
static DCapsDef Cap0x07B3_0x0015_0 =
{
	{{0, 93},               0,   -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32},	543, -1, {473, 414},   {150, 150}},
	{{1004 + 20, 744 + 32},	543, -1, {567, 414},   {150, 150}},
	{{0, 95},               0,   -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 4, kNEC3799, 0x05, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0014_0 =
{
	{{0, 93},               0,   -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32},	543, -1, {473, 414},   {150, 150}},
	{{1004 + 20, 744 + 32},	543, -1, {567, 414},   {150, 150}},
	{{0, 95},               0,   -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 0, kNEC3799, 0x04, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ??? and Genius ColorPage-HR6 V2
 * KH: NS9831 + TPA + Button + NEC3799
*/
static DCapsDef Cap0x07B3_0x0007_0 =
{
	{{0,    124},  36, -1, {2550, 3508}, { 50, 50 }},
	{{1040, 744}, 543, -1, { 473, 414 }, {150, 150}},
	{{1004, 744}, 543, -1, { 567, 414 }, {150, 150}},
	{{0,     95},	0, -1, {2550, 3508}, { 50, 50 }},
	{600, 600},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	4, 5, kNEC3799, 0x07, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * Tokyo: NS9832 + Button + SONY548
 */
static DCapsDef Cap0x07B3_0x0005_2 =
{
	{{ 0, 64}, 0, -1, {2550, 3508}, { 50, 50 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{600, 600},
	0,
	SENSORORDER_rgb,
	8, 2, kSONY548, 0x05, _WAF_NONE, _NO_MIO
};

/* Genius ColorPage-HR7
 * Hualien: NS9832 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0007_4 =
{
	{{       0,  111 - 4 },	  0, -1, {2550, 3508}, { 50, 50 }},
	{{1040 + 5,  744 - 32}, 543, -1, { 473, 414 }, {150, 150}},
	{{1040 - 20, 768     },	543, -1, { 567, 414 }, {150, 150}},
	{{        0, 95      },   0, -1, {2550, 3508}, { 50, 50 }},
	{1200, 1200},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	12,	5, kNEC3778, 0x07, _WAF_NONE, _NO_MIO
};

/* Genius ColorPage-HR7LE and ColorPage-HR6X
 * Hualien: NS9832 + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0005_4 =
{
	{{ 0,  111 - 4 }, 0, -1, {2550, 3508}, {50, 50}},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12, 5, kNEC3778, 0x05, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???, Genius ColorPage HR6A
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x000F_0 =
{
	{{   0, 130},  12, -1, {2550, 3508}, { 50, 50 }},
	{{1040, 744}, 543, -1, { 473, 414 }, {150, 150}},
	{{1004, 744}, 543, -1, { 567, 414 }, {150, 150}},
	{{   0, 244},  12, -1, {2550, 4200}, { 50, 50 }},
	{600, 600},
	DEVCAPSFLAG_Normal + DEVCAPSFLAG_Adf,
	SENSORORDER_rgb,
	4, 5, kNEC3799, 0x0F, _WAF_NONE, _NO_MIO
};

/* Plustek Model: UT12
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0013_0 =
{
	{{        0,       93},   0, -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 30, 744 + 32},	543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	4, 4, kNEC3799, 0x03, _WAF_NONE, _NO_MIO
};

/* Plustek Model: U24
 * KH: NS9831 + Button + NEC3799
//  */
static DCapsDef Cap0x07B3_0x0011_0 =
{
	{{        0,       93},   0, -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32},	543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20, 744 + 32}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 4, kNEC3799, 0x01, _WAF_NONE, _NO_MIO
};

/* Plustek Model: U12
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0010_0 =
{
	{{        0,       93},   0, -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32},	543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20, 744 + 32}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},	  0, -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 0, kNEC3799, 0x00, _WAF_BSHIFT7_BUG, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0013_4 =
{
	{{        0, 99 /*114*/},   0, -1, {2550, 3508}, { 50,  50}},
	{{     1055,   744 - 84}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20,   744 - 20}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,         95},   0, -1, {2550, 3508}, { 50,  50}},
	{1200, 1200},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,	
	SENSORORDER_rgb,
	12, 4, kNEC3778, 0x03, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9831 + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0011_4 =
{
	{{        0, 99 /*114*/},   0, -1, {2550, 3508}, { 50,  50}},
	{{     1055,   744 - 84}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20,   744 - 20}, 543, -1 ,{ 567,  414}, {150, 150}},
	{{        0,         95},   0, -1, {2550, 3508}, { 50,  50}},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12, 4, kNEC3778, 0x01, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0010_4 =
{
	{{        0, 99 /*114*/},   0, -1, {2550, 3508}, { 50,  50}},
	{{     1055,   744 - 84}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20,   744 - 20}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,         95},   0, -1, {2550, 3508}, { 50,  50}},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12, 0, kNEC3778, 0x00, _WAF_NONE, _NO_MIO
};

/* Plustek Model: UA18?
 * KH: NS9831 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x000F_4 =
{
	{{        0,      107},   0, -1, {2550, 3508}, { 50,  50}},
	{{ 1040 + 5, 744 - 32}, 543, -1, { 473,  414}, {150, 150}},
	{{1040 - 20,      768}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,      244},   0, -1, {2550, 4200}, { 50,  50}},
	{1200, 1200},
	DEVCAPSFLAG_Normal + DEVCAPSFLAG_Adf,
	SENSORORDER_rgb,
	12, 5, kNEC3778, 0x0F, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0016_4 =
{
	{{   0,  93},   0, -1, {2550, 3508}, { 50,  50}},
	{{ 954, 422}, 272, -1, { 624, 1940}, {150, 150}},
	{{1120, 438}, 275, -1, { 304, 1940}, {150, 150}},
	{{   0,  95},   0, -1, {2550, 3508}, { 50,  50}},
	{1200, 1200},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	12, 4, kNEC3778, 0x06, _WAF_NONE, _NO_MIO
};

/* Plustek Model: UT24
 * KH: NS9832 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0017_4 =
{
	{{             0,  99 - 6},	    0, -1, {2550, 3508}, { 50,  50}},
	{{1025 /*1055*/, 744 - 84},   543, -1, { 473,  414}, {150, 150}},
	{{1048 /*1024*/, 754/*724*/}, 543, -1, { 567,  414}, {150, 150}},
	{{            0,       95},     0, -1, {2550, 3508}, { 50,  50}},
	{1200, 1200},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	12, 4, kNEC3778, 0x07, _WAF_NONE, _NO_MIO
};

/* Plustek Model: U24
 * KH: NS9832 + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0015_4 =
{
	{{        0,   99 - 6},   0, -1, {2550, 3508}, { 50,  50}},
	{{     1055, 744 - 84}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20, 744 - 20}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12, 4, kNEC3778, 0x05, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9832 + TPA + Button + NEC3778
 */
static DCapsDef Cap0x07B3_0x0014_4 =
{
	{{        0,   99 - 6},   0, -1, {2550, 3508}, { 50,  50}},
	{{     1055, 744 - 84}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20, 744 - 20},	543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	12, 0, kNEC3778, 0x04, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ??? A3 model
 * KH: NS9831 + TPA + Button + SONY518
 */
static DCapsDef Cap0x07B3_0x0014_1 =
{
	{{        0,       93},   0, -1, {3600, 5100}, { 50,  50}},
	{{1040 + 15, 744 - 32}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20, 744 + 32}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{400, 400},
	0,
	SENSORORDER_rgb,
	8, 0, kSONY518, 0x04, _WAF_NONE, _NO_MIO
};

/* Model: ???
 * KH: NS9832 + NEC3799 + 600 DPI Motor (for Brother demo only)
 */
static DCapsDef Cap0x07B3_0x0012_0 =
{
	{{        0,       93},   0, -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32},	543, -1, { 473,  414}, {150, 150}},
	{{1004 + 20, 744 + 32}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 0, kNEC3799, 0x02, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + SONY548
 */
static DCapsDef Cap0x07B3_0x0017_2 =
{
	{{        0,       93},   0, -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32}, 543, -1, { 473,  414}, {150, 150}},
	{{     1004,      744},	543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_bgr,
	8, 4, kSONY548, 0x07, _WAF_NONE, _NO_MIO
};

/* Plustek Model: ???
 * KH: NS9831 + TPA + Button + NEC3799
 */
static DCapsDef Cap0x07B3_0x0017_3 =
{
	{{        0,       93},   0, -1, {2550, 3508}, { 50,  50}},
	{{1040 + 15, 744 - 32}, 543, -1, { 473,  414}, {150, 150}},
	{{1004 + 30, 744 + 32}, 543, -1, { 567,  414}, {150, 150}},
	{{        0,       95},   0, -1, {2550, 3508}, { 50,  50}},
	{600, 600},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	8, 4, kNEC8861, 0x07, _WAF_NONE, _NO_MIO
};

/* Model: HP Scanjet 2100c */
static DCapsDef Cap0x03F0_0x0505 =
{
	{{ 0,  65}, 10, -1, {2550, 3508}, { 50,  50}},
	{{ 0,   0},  0, -1, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0, -1, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0, -1, {0, 0}, { 0, 0 }},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 0, kNECSLIM, 0x00, _WAF_NONE, _NO_MIO
};

/* Model: HP Scanjet 2200c (thanks to Stefan Nilsen)
 * NS9832 + 2 Buttons + NEC3799 + 600 DPI Motor
 */
static DCapsDef Cap0x03F0_0x0605 =
{
	/* DataOrigin (x, y), ShadingOriginY */
	{{ 0, 209},  40, -1, {2550, 3508}, { 50,  50}},
	{{ 0,   0},   0, -1, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0, -1, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0, -1, {0, 0}, { 0, 0 }},
	{600, 600},
	0,
	SENSORORDER_rgb,
	4, 2, kNECSLIM, 0x00, _WAF_NONE, _NO_MIO
};

/* Mustek BearPaw 1200 (thanks to Henning Meier-Geinitz)
 * NS9831 + 5 Buttons + NEC3798
 */
static DCapsDef Cap0x0400_0x1000_0 =
{
	{{ 0, 130}, 20, -1, {2550, 3508}, { 50, 50 }},
	{{ 0,  0},   0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0},   0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0},   0, -1, {0, 0}, { 0, 0 }},
	{600, 600},
	0,
	SENSORORDER_rgb,
	8,
	5, kNEC8861, 0x00,
	_WAF_MISC_IO_LAMPS | _WAF_LOFF_ON_START | _WAF_USE_ALT_DESC,
	_NO_MIO
};

/* Mustek BearPaw 2400
 * NS9832 + 5 Buttons + SONY548
 */
static DCapsDef Cap0x0400_0x1001_0 =
{
	{{ 0, 130/*209*/}, 35/*20*/, -1, {2550, 3508}, { 50, 50 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{{ 0,  0}, 0, -1, {0, 0}, { 0, 0 }},
	{ 600, 600 }, /*{ 1200, 1200 }, */
	0,
	SENSORORDER_rgb,
	4,/*16*/            /* sensor distance   */
	5,                  /* number of buttons */
	kSONY548,           /* CCD type          */
	0, _WAF_USE_ALT_DESC, _NO_MIO
};

/* Epson Perfection/Photo1250 (thanks to Gene Heskett and Reinhard Max)
 * Epson Perfection/Photo1260 (thanks to Till Kamppeter)
 * NS9832 + 4 Buttons + CCD????
 */
static DCapsDef Cap0x04B8_0x010F =
{
	/* Normal */
	{{   25,   85},  10, -1, {2550, 3508}, { 100, 100 }},
	/* Positive */
	{{ 1100,  972}, 720, -1, { 473,  414}, { 150, 150 }},
	/* Negative */
	{{ 1116, 1049}, 720, -1, { 567,  414}, { 150, 150 }},
	{{ 0,  0},   0, -1, {0, 0}, { 0, 0 }},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	8,                  /* sensor distance                         */
	4,                  /* number of buttons                       */
	kEPSON,             /* use default settings during calibration */
	0,                  /* not used here...                        */
	_WAF_MISC_IO_LAMPS, /* use miscio 6 for lamp switching         */
	_MIO6 + _TPA(_MIO1) /* and miscio 1 for optional TPA           */
};

/* Umax 3400/3450
 */
static DCapsDef Cap0x1606_0x0060 =
{
	/* Normal */
	{{  30, 105 },  15, -1, {2550, 3508}, { 100, 100 }},
	/* Positive */
	{{ 700, 760 }, 650, -1, {1200, 1500}, { 150, 150 }},
	/* Negative */
	{{ 700, 760 }, 650, -1, {1200, 1500}, { 150, 150 }},
	{{   0,   0 },   0, -1, {0, 0}, { 0, 0 }},
	{600, 600},
	DEVCAPSFLAG_LargeTPA,
	SENSORORDER_bgr,
	8,                  /* sensor distance                         */
	4,                  /* number of buttons                       */
	kNEC8861,           /* use default settings during calibration */
	0,                  /* not used here...                        */
	_WAF_MISC_IO_LAMPS, /* use miscio 3 for lamp switching         */
	_MIO3 + _TPA(_MIO6) /* and miscio 6 for optional TPA           */
};

/* Umax 5400
 */
static DCapsDef Cap0x1606_0x0160 =
{
 	{{ 30, 165}, 0, -1, {2550, 3508}, {100, 100}},
 	{{  0,   0}, 0, -1, {0, 0}, { 0, 0 }},
 	{{  0,   0}, 0, -1, {0, 0}, { 0, 0 }},
 	{{  0,   0}, 0, -1, {0, 0}, { 0, 0 }},
 	{1200, 1200},
 	0,
 	SENSORORDER_bgr,
 	12,                 /* sensor distance                         */
 	4,                  /* number of buttons                       */
 	kNEC3778,           /* use default settings during calibration */
 	0,                  /* not used here...                        */
 	_WAF_MISC_IO_LAMPS, /* use miscio 3 for lamp switching         */
    _MIO3
};

/* Canon N650U/N656U
 */
static DCapsDef Cap0x04A9_0x2206 =
{
	{{ 0, 90}, 45, 10, {2550, 3508}, {75, 75}},
	{{ 0,  0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,  0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,  0},  0,  0, {0, 0}, { 0, 0 }},
	{600, 600},
	0,
	SENSORORDER_rgb,
	8,                  /* sensor distance                         */
	1,                  /* number of buttons                       */
	kCIS650,            /* use default settings during calibration */
	0,                  /* not used here...                        */
	_WAF_MISC_IO_LAMPS, _NO_MIO
};

/* Canon N1220U
 */
static DCapsDef Cap0x04A9_0x2207 =
{
	{{ 0, 85}, 45, 10, {2550, 3508}, {75, 75}},
	{{ 0,  0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,  0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,  0},  0,  0, {0, 0}, { 0, 0 }},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	16,                 /* sensor distance                         */
	1,                  /* number of buttons                       */
	kCIS1220,           /* use default settings during calibration */
	0,                  /* not used here...                        */
	_WAF_MISC_IO_LAMPS, _NO_MIO
};

/* Canon D660U
 */
static DCapsDef Cap0x04A9_0x2208 =
{
	{{  45, 125},  15, -1, {2550, 3508}, { 50,  50}},
	{{1060, 744}, 510, -1, { 473,  414}, {150, 150}},
	{{1082, 842}, 610, -1, { 567,  414}, {150, 150}},
	{{   0,   0},   0,  0, {   0,    0}, {  0,   0}},
	{600, 600},
	DEVCAPSFLAG_Positive + DEVCAPSFLAG_Negative,
	SENSORORDER_rgb,
	4,
	1,
	kNEC8861,           /* use default settings during calibration */
	0,
	(_WAF_MISC_IO_LAMPS | _WAF_BIN_FROM_COLOR | _WAF_GRAY_FROM_COLOR),
	_MIO5 + _TPA(_MIO6)
};

/* Canon N670U/N676U/LiDE20
 */
static DCapsDef Cap0x04A9_0x220D =
{
	{{ 0, 110}, 45, -1, {2550, 3508}, {75, 75}},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{600, 600},
	0,
	SENSORORDER_rgb,
	8,                  /* sensor distance                         */
	3,                  /* number of buttons                       */
	kCIS670,
	0,                  /* not used here...                        */
	_WAF_MISC_IO_LAMPS, _NO_MIO
};

/* Canon N1240U/LiDE30
 */
static DCapsDef Cap0x04A9_0x220E =
{
	{{ 0, 100}, 50, 10, {2550, 3508}, {75, 75}},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	16,                 /* sensor distance                         */
	3,                  /* number of buttons                       */
	kCIS1240,
	0,                  /* not used here...                        */
	_WAF_MISC_IO_LAMPS, _NO_MIO
};

/* Canon LiDE25
 */
static DCapsDef Cap0x04A9_0x2220 =
{
	{{ 0, 100}, 50, 10, {2550, 3508}, {75, 75}},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},  0,  0, {0, 0}, { 0, 0 }},
	{1200, 1200},
	0,
	SENSORORDER_rgb,
	16,                 /* sensor distance                         */
	3,                  /* number of buttons                       */
	kCIS1240,
	0,                  /* not used here...                        */
	_WAF_MISC_IO_LAMPS, _NO_MIO
};

/* Syscan TravelScan 662 A6 sheet-fed scanner
 */
static DCapsDef Cap0x0A82_0x6620 =
{
	{{ 0,   0}, 100, -1, {1226, 3508}, {75, 75}},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{600, 600},
	DEVCAPSFLAG_SheetFed,
	SENSORORDER_rgb,
	8,
	1,
	kNEC8861,           /* use default settings during calibration */
	0,                  /* not used here... */
	(_WAF_MISC_IO_LAMPS | _WAF_MISC_IO_BUTTONS |
	 _WAF_BIN_FROM_COLOR | _WAF_GRAY_FROM_COLOR),
	_MIO5 + _PORT1 + _PS_INP_MIO2
};

/* Syscan TravelScan 464 A4 sheet-fed scanner
 */
static DCapsDef Cap0x0A82_0x4600 =
{
	{{ 0,   0}, 150, -1, {2550, 3508}, {75, 75}},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{600, 600},
	DEVCAPSFLAG_SheetFed,
	SENSORORDER_rgb,
	8,
	1,
	kNEC8861,           /* use default settings during calibration */
	0,                  /* not used here... */
	(_WAF_MISC_IO_LAMPS | _WAF_MISC_IO_BUTTONS |
	 _WAF_BIN_FROM_COLOR | _WAF_GRAY_FROM_COLOR),
	_MIO5 + _PORT1 + _PS_INP_MIO2
};

/* IRIScan/Q-Scan USB001 A4 sheet-fed scanner
 */
static DCapsDef Cap0x0A53_0x1000 =
{
	{{ 0,   0}, 150, -1, {2550, 3508}, {150, 150}},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{300, 300},
	DEVCAPSFLAG_SheetFed,
	SENSORORDER_gbr,
	2,                  /* sensor distance                         */
	0,                  /* number of buttons                       */
	kNEC8861,           /* use default settings during calibration */
	200,                /* threshold for resetting sensor-order    */
	(_WAF_MISC_IO_LAMPS | _WAF_RESET_SO_TO_RGB | _WAF_ONLY_8BIT),
	_PS_INP1
};

/* PandP USB201 Q-Scan A6 Scanner
 */
static DCapsDef Cap0x0A53_0x2000 =
{
	{{ 0,   0},   0, -1, {1226, 3508}, { 50, 50 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{{ 0,   0},   0,  0, {0, 0}, { 0, 0 }},
	{600, 600},
	DEVCAPSFLAG_SheetFed,
	SENSORORDER_rgb,
	4,
	0,
	kNEC8861,
	0,
	_WAF_NONE,
	_PS_INP1
};

/******************* additional Hardware descriptions ************************/

/** U24, UT12 and UT16
 */
static HWDef Hw0x07B3_0x0017_0 =
{
	1.5,            /* dMaxMotorSpeed                           */
	1.2,            /* dMaxMoveSpeed                            */
	0.0,            /* dHighSpeed                               */
	9,              /* dIntegrationTimeLowLamp                  */
	9,              /* dIntegrationTimeHighLamp                 */
	300,            /* wMotorDpi (Full step DPI)                */
	512,            /* wRAMSize (KB)                            */
	4,              /* dMinIntegrationTimeLowres (ms)           */
	5,              /* dMinIntegrationTimeHighres (ms)          */
	3000,           /* wGreenPWMDutyCycleLow                    */
	4095,           /* wGreenPWMDutyCycleHigh                   */
	0x02,           /* bSensorConfiguration (0x0b)              */
	0x04,           /* bReg_0x0c                                */
	0x37,           /* bReg_0x0d                                */
	0x13,           /* bReg_0x0e                                */
	/* bReg_0x0f_Mono [10]  (0x0f to 0x18)                      */
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	/* bReg_0x0f_Color [10]	(0x0f to 0x18)                      */
	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},
	
	_GREEN_CH,      /* bReg_0x26 color mode - bits 4 and 5      */
	0,              /* bReg 0x27 color mode                     */

	1,              /* bReg 0x29 illumination mode              */

	/* illumination mode settings (not used for CCD devices)    */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	1,              /* StepperPhaseCorrection (0x1a & 0x1b)     */
	14,             /* 15,  bOpticBlackStart (0x1c)             */
	62,             /* 60,  bOpticBlackEnd (0x1d)               */
	110,            /* 65,  wActivePixelsStart (0x1e & 0x1f)    */
	5400,           /* 5384 ,wLineEnd   (0x20 & 0x21)           */

	0,              /* red lamp on    (reg 0x2c + 0x2d)         */
	16383,          /* red lamp off   (reg 0x2e + 0x2f)         */
	0,              /* green lamp on  (reg 0x30 + 0x31)         */
	0,              /* green lamp off (reg 0x32 + 0x33)         */
	0,              /* blue lamp on   (reg 0x34 + 0x35)         */
	16383,          /* blue lamp off  (reg 0x36 + 0x37)         */

	/* Misc                                                     */
	3,              /* bReg_0x45                                */
	0,              /* wStepsAfterPaperSensor2 (0x4c & 0x4d)    */
	0x1e,           /* bstepsToReverse reg 0x50)                */
	0xa8,           /* 0xfc -bReg_0x51                          */
	0,              /* bReg_0x54                                */
	0xff,           /* 0xa3 - bReg_0x55                         */
	64,             /* bReg_0x56                                */
	20,             /* bReg_0x57                                */
	0x0d,           /* bReg_0x58                                */
	0x22,           /* bReg_0x59                                */
	0x82,           /* bReg_0x5a                                */
	0x88,           /* bReg_0x5b                                */
	0,              /* bReg_0x5c                                */
	0,              /* bReg_0x5d                                */
	0,              /* bReg_0x5e                                */
	_LM9832,        /* chip type                                */
	MODEL_KaoHsiung,/* motorModel                               */
	1.0
};

/** Genius ColorPage-HR6 V2 and ColorPage-HR6X
 */
static HWDef Hw0x07B3_0x0007_0 =
{
	1.5, 1.2, 0.0,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x14,	0x27, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 3, 0, 0, 0, 6, 10, 22},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	14,
	62,
	110,
	5384,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	64,
	20,
	0x0d, 0x88, 0x28, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_HuaLien,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x0007_2 =
{
	1.4, 1.2, 0.0,
	9, 9,
	600,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x3f,	0x2f, 0x36,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{7, 20, 1, 4, 7, 10, 0, 6, 12, 0},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	16,
	64,
	152,
	5416,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xfc,
	0,
	0xff,
	64,
	20,
	0x0d, 0x88, 0x28, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_Tokyo600,
	1.0
};

/** Genius ColorPage-HR7 and ColorPage-HR7LE
 */
static HWDef Hw0x07B3_0x0007_4 =
{
	1.1, 0.9, 0.0,
	12, 12,
	600,
	2048,
	8, 8,
	4095, 4095,
	0x06, 0x30,	0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 6, 23, 11},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	13,
	62,
	304,
	10684,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	24,
	40,
	0x0d, 0x88, 0x28, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_HuaLien,
	1.0
};

/** Genius ColorPage-HR6A
 */
static HWDef Hw0x07B3_0x000F_0 =
{
	1.5, 1.0, 0.0,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x14,	0x27, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 3, 0, 0, 0, 6, 10, 22},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices)    */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	14,
	62,
	110,
	5384,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	64,
	20,
	0x05, 0x88, 0x08, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_HuaLien,
	1.0
};

/** U12/UT12 and U24
 */
static HWDef Hw0x07B3_0x0013_0 =
{
	1.5, 1.2, 0.0,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x04, 0x37, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	14,
	62,
	110,
	5400,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,	
	0xff,
	64,	
	20,
	0x0d, 0x22,	0x82, 0x88,
	0, 0, 0,
	_LM9831,
	MODEL_KaoHsiung,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x0013_4 =
{
	1.0, 0.9, 0.0,
	12, 12,
	600,
	2048,
	8, 8,
	4095, 4095,
	0x06, 0x20, 0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 0, 23, 11},
	_GREEN_CH,
	0,
	1,		
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	13,
	62,	
	320,	
	10684,	
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	10,
	48,
	0x0d, 0x22, 0x82, 0x88,
	0, 0, 0,
	_LM9831,
	MODEL_KaoHsiung,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x000F_4 =
{
	1.1, 0.9, 0.0,
	12,	12,
	600,	
	2048,	
	8, 8,		
	4095, 4095,
	0x06, 0x30,	0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 6, 23, 11},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	13,
	62,
	304,
	10684,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	24,
	40,
	0x05, 0x88, 0x08, 0x3b,
	0, 0, 0,
	_LM9832,
	MODEL_HuaLien,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x0016_4 =
{
	1.0, 0.9, 0.0,
	12, 12,
	600,
	2048,
	8, 8,
	4095, 4095,
	0x06, 0x20,	0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},
	{20, 4, 13, 16, 19, 22, 0, 0, 23, 11},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	13,
	62,
	320,
	10684,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	10,
	48,
	0x0d, 0x22, 0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung,
	1.0
};

/** Plustek OpticPro UT24 and others...
 */
static HWDef Hw0x07B3_0x0017_4 =
{
	1.0, 0.9, 0.0,
	12, 12,
	600,
	2048,
	8, 8,
	4095, 4095,
	0x06, 0x20, 0x2f, 0x2a,
	{2, 7, 5, 6, 6, 7, 0, 0, 0, 5},	
	{20, 4, 13, 16, 19, 22, 0, 0, 23, 11},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	13,
	62,
	320,
	10684,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	10,
	48,
	0x0d, 0x22, 0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x0017_1 =
{
	1.5, 1.5, 0.0,
	9, 9,
	200,
	2048,
	4, 5,
	3000, 4095,
	0x02, 0x08, 0x2f, 0x36,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 4, 7, 10, 0, 0, 10, 12},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	15,
	60,
	110,
	5415,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,	
	0xff,
	64,
	20,
	0x0d, 0x22, 0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x0012_0 =
{
	1.5, 1.4, 0.0,
	9, 9,
	600,
	2048,
	4, 5,
	3000, 4095,
	0x02, 0x04, 0x37, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 3, 0, 0, 0, 12, 10, 22},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	14,
	62,
	110,
	5400,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	64,
	20,
	0x0d, 0x22, 0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x0017_2 =
{
	1.5, 1.2, 0.0,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0, 0x2f, 0x36,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 0, 1, 4, 7, 10, 0, 0, 12, 0},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	16,
	64,
	110,
	5416,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,	
	0x1e,
	0xa8,
	0,
	0xff,
	64,
	20,
	0x0d, 0x22,	0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung,
	1.0
};

/** unknown
 */
static HWDef Hw0x07B3_0x0017_3 =
{
	1.5, 1.2, 0.0,
	9, 9,
	300,
	512,
	4, 5,
	3000, 4095,
	0x02, 0x04, 0x37, 0x13,
	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},
	{5, 23, 1, 4, 7, 10, 0, 0, 11, 23},
	_GREEN_CH,
	0,
	1,
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	1,
	14,
	62,
	110,
	5400,
	0,
	16383,
	0,
	0,
	0,
	16383,
	3,
	0,
	0x1e,
	0xa8,
	0,
	0xff,
	64,
	20,
	0x0d, 0x22, 0x82, 0x88,
	0, 0, 0,
	_LM9832,
	MODEL_KaoHsiung,
	1.0
};

/** HP Scanjet 2100C
 */
static HWDef Hw0x03F0_0x0505 =
{
	1.05,   /* dMaxMotorSpeed                           */
	1.05,   /* dMaxMoveSpeed                            */
	0.0,    /* dHighSpeed                               */
	6,      /* dIntegrationTimeLowLamp                  */
	8,      /* dIntegrationTimeHighLamp                 */
	600,    /* wMotorDpi (Full step DPI)                */
	512,    /* wRAMSize (KB)                            */
	6,      /* dMinIntegrationTimeLowres (ms)           */
	6,      /* dMinIntegrationTimeHighres (ms)          */
	0,      /* wGreenPWMDutyCycleLow                    */
	0,      /* wGreenPWMDutyCycleHigh                   */
	0x02,   /* bSensorConfiguration (0x0b)              */
	0x00,   /* bReg_0x0c                                */
	0x2F,   /* bReg_0x0d                                */
	0x13,   /* bReg_0x0e                                */
	        /* bReg_0x0f_Mono[10] (0x0f to 0x18)        */

	{ 0x02, 0x07, 0x01, 0x02, 0x02, 0x03, 0x00, 0x00, 0x04, 0x07 },

	       /* bReg_0x0f_Color[10] (0x0f to 0x18)       */
	{ 0x08, 0x17, 0x00, 0x03, 0x08, 0x0b, 0x00, 0x00, 0x0a, 0x14 },
	
	_GREEN_CH,  /* bReg_0x26 color mode - bits 4 and 5  */
	0,          /* bReg 0x27 color mode                 */
	
	1,          /* bReg 0x29 illumination mode          */

	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	1,      /* StepperPhaseCorrection (0x1a & 0x1b)     */
	15,     /* bOpticBlackStart (0x1c)                  */
	50,     /* bOpticBlackEnd (0x1d)                    */
	140,    /* wActivePixelsStart (0x1e & 0x1f)         */
	5414,   /* wLineEnd=(0x20 & 0x21)                   */

	1,      /* red lamp on    (reg 0x2c + 0x2d)         */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)         */
	16383,  /* green lamp on  (reg 0x30 + 0x31)         */
	1,      /* green lamp off (reg 0x32 + 0x33)         */
	16383,  /* blue lamp on   (reg 0x34 + 0x35)         */
	1,      /* blue lamp off  (reg 0x36 + 0x37)         */
	
	/* Misc                                             */
	0x13,   /* bReg_0x45                                */
	0,      /* wStepsAfterPaperSensor2 (0x4c & 0x4d)    */
	0x1e,   /* steps to reverse on buffer full reg 0x50 */
	0xfc,   /* 0xa8 -bReg_0x51                          */
	0,      /* bReg_0x54                                */
	0x18,   /* bReg_0x55                                */
	8,      /* bReg_0x56                                */
	60,     /* bReg_0x57                                */
	0x0d,   /* bReg_0x58                                */
	0xaa,   /* bReg_0x59                                */
	0xba,   /* bReg_0x5a                                */
	0xbb,   /* bReg_0x5b                                */
	0,      /* bReg_0x5c                                */
	0,      /* bReg_0x5d                                */
	0,      /* bReg_0x5e                                */
	_LM9831,
	MODEL_HP,
	1.0
};

/** HP Scanjet 2200C */
static HWDef Hw0x03F0_0x0605 =
{
	1.05,   /* dMaxMotorSpeed                           */
	1.05,   /* dMaxMoveSpeed                            */
	2.2,    /* dHighSpeed                               */
	6,      /* dIntegrationTimeLowLamp                  */
	8,      /* dIntegrationTimeHighLamp                 */
	600,    /* ok wMotorDpi (Full step DPI)             */
	512,    /* wRAMSize (KB)                            */
	6,      /* dMinIntegrationTimeLowres (ms)           */
	6,      /* dMinIntegrationTimeHighres (ms)          */
	0,      /* wGreenPWMDutyCycleLow                    */
	0,      /* wGreenPWMDutyCycleHigh                   */
	0x02,   /* bSensorConfiguration (0x0b)              */
	0x04,   /* bReg_0x0c                                */
	0x2F,   /* bReg_0x0d                                */
	0x1F,   /* bReg_0x0e                                */

	/* bReg_0x0f_Mono[10] (0x0f to 0x18)        */
	{ 0x02, 0x07, 0x01, 0x02, 0x02, 0x03, 0x00, 0x00, 0x04, 0x07 },

	/* bReg_0x0f_Color[10] (0x0f to 0x18)       */
	{ 0x08, 0x17, 0x00, 0x03, 0x08, 0x0b, 0x00, 0x00, 0x0a, 0x14 },
	
	_GREEN_CH,  /* bReg_0x26 color mode - bits 4 and 5  */
	0,          /* bReg 0x27 color mode                 */
	1,          /* bReg 0x29 illumination mode          */

	/* illumination mode settings (not used for CCD devices)*/
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	1,      /* StepperPhaseCorrection (0x1a & 0x1b)     */
	14,     /* bOpticBlackStart (0x1c)                  */
	63,     /* bOpticBlackEnd (0x1d)                    */
	140,    /* wActivePixelsStart (0x1e & 0x1f)         */
	5367,   /* wLineEnd=(0x20 & 0x21)                   */

	1,      /* red lamp on    (reg 0x2c + 0x2d)         */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)         */
	16383,  /* green lamp on  (reg 0x30 + 0x31)         */
	1,      /* green lamp off (reg 0x32 + 0x33)         */
	16383,  /* blue lamp on   (reg 0x34 + 0x35)         */
	1,      /* blue lamp off  (reg 0x36 + 0x37)         */
	
	/* Misc                                             */
	0x13,   /* bReg_0x45                                */
	0,      /* wStepsAfterPaperSensor2 (0x4c & 0x4d)    */
	0x1e,   /* steps to reverse on buffer full (0x50)   */
	0xfc,   /* 0xa8 -bReg_0x51                          */
	0,      /* bReg_0x54                                */
	0x18,   /* bReg_0x55                                */
	8,      /* bReg_0x56                                */
	60,     /* bReg_0x57                                */
	0x0d,   /* bReg_0x58                                */
	0xcc,   /* bReg_0x59                                */
	0xbc,   /* bReg_0x5a                                */
	0xbb,   /* bReg_0x5b                                */
	0,      /* bReg_0x5c                                */
	0,      /* bReg_0x5d                                */
	0,      /* bReg_0x5e                                */
	_LM9832,
	MODEL_HP,
	1.0
};

/** Mustek BearPaw 1200 */
static HWDef Hw0x0400_0x1000_0 =
{
	1.75,   /* ok dMaxMotorSpeed                            */
	1.25,   /* ok dMaxMoveSpeed                             */
	0.0,    /* ok dHighSpeed                                */
	12,     /* ok dIntegrationTimeLowLamp                   */
	12,     /* ok dIntegrationTimeHighLamp                  */
	600,    /* ok wMotorDpi (Full step DPI)                 */
	512,    /* ok wRAMSize (KB)                             */
	9,      /* ok dMinIntegrationTimeLowres (ms)            */
	9,      /* ok dMinIntegrationTimeHighres (ms)           */
	1169,   /* ok wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
	1169,   /* ok wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */
	0x02,   /* ok bSensorConfiguration (0x0b)               */
	0x7c,   /* ok sensor control settings (reg 0x0c)        */
	0x3f,   /* ok sensor control settings (reg 0x0d)        */
	0x15,   /* ok sensor control settings (reg 0x0e)        */
            /* ok  mono (reg 0x0f to 0x18) */
	{ 0x04, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x03, 0x06},
            /* ok color (reg 0x0f to 0x18)                  */
	{ 0x04, 0x16, 0x01, 0x02, 0x05, 0x06, 0x00, 0x00, 0x0a, 0x16},
	_GREEN_CH,	/* bReg_0x26 color mode - bits 4 and 5      */
	0,          /* bReg 0x27 color mode                     */
	1,          /* bReg 0x29 illumination mode              */
	/* illumination mode settings (not used for CCD devices)*/
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	257,    /* ok StepperPhaseCorrection (reg 0x1a + 0x1b)  */
	0x0e,   /* ok bOpticBlackStart (reg 0x1c)               */
	0x1d,   /* ok bOpticBlackEnd (reg 0x1d)                 */
	140,    /* ok wActivePixelsStart (reg 0x1e + 0x1f)      */
	5369,   /* ok wLineEnd (reg 0x20 + 0x21)                */
	0,      /* red lamp on    (reg 0x2c + 0x2d)             */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)             */
	0,      /* green lamp on  (reg 0x30 + 0x31)             */
	0,      /* green lamp off (reg 0x32 + 0x33)             */
	0,      /* blue lamp on   (reg 0x34 + 0x35)             */
	16383,  /* blue lamp off  (reg 0x36 + 0x37)             */
	0x13,   /* ok stepper motor control (reg 0x45)          */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)    */
	0x1e,   /* steps to reverse on buffer full (reg 0x50)   */
	0xfc,   /* ok acceleration profile (reg 0x51)           */
	0,      /* ok lines to process (reg 0x54)               */
	0x13,   /* ok kickstart (reg 0x55)                      */
	0x03,   /* ok pwm freq (reg 0x56)                       */
	0x20,   /* ok pwm duty cycle (reg 0x57)                 */
	0x0d,	/* ok Paper sense (reg 0x58)                    */
	0x44,   /* ok misc io12 (reg 0x59)                      */
	0x44,   /* ok misc io34 (reg 0x5a)                      */
	0x16,   /* ok misc io56 (reg 0x5b)                      */
	0,      /* ok test mode ADC Output CODE MSB (reg 0x5c)  */
	0,      /* ok test mode ADC Output CODE LSB (reg 0x5d)  */
	0,      /* ok test mode (reg 0x5e)                      */
	_LM9831,
	MODEL_MUSTEK600,
	1.5
};

/** Mustek BearPaw 1200 (LM9832) */
static HWDef Hw0x0400_0x1001_1 =
{
	1.25,   /* ok dMaxMotorSpeed                            */
	1.25,   /* ok dMaxMoveSpeed                             */
	0.0,    /* dHighSpeed                                   */
	12,     /* ok dIntegrationTimeLowLamp                   */
	12,     /* ok dIntegrationTimeHighLamp                  */
	600,    /* ok wMotorDpi (Full step DPI)                 */
	512,    /* ok wRAMSize (KB)                             */
	9,      /* ok dMinIntegrationTimeLowres (ms)            */
	9,      /* ok dMinIntegrationTimeHighres (ms)           */
	1169,   /* ok wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
	1169,   /* ok wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */
	0x02,   /* ok bSensorConfiguration (0x0b)               */
	0x7c,   /* ok sensor control settings (reg 0x0c)        */
	0x3f,   /* ok sensor control settings (reg 0x0d)        */
	0x15,   /* ok sensor control settings (reg 0x0e)        */
            /* ok  mono (reg 0x0f to 0x18) */
	{ 0x04, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x03, 0x06},
            /* ok color (reg 0x0f to 0x18)                  */
	{ 0x04, 0x16, 0x01, 0x02, 0x05, 0x06, 0x00, 0x00, 0x0a, 0x16},
	_GREEN_CH,	/* bReg_0x26 color mode - bits 4 and 5      */
	0,          /* bReg 0x27 color mode                     */
	1,          /* bReg 0x29 illumination mode              */
	/* illumination mode settings (not used for CCD devices)*/
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	257,    /* ok StepperPhaseCorrection (reg 0x1a + 0x1b)  */
	0x0e,   /* ok bOpticBlackStart (reg 0x1c)               */
	0x1d,   /* ok bOpticBlackEnd (reg 0x1d)                 */
	140,    /* ok wActivePixelsStart (reg 0x1e + 0x1f)      */
	5369,   /* ok wLineEnd (reg 0x20 + 0x21)                */
	0,      /* red lamp on    (reg 0x2c + 0x2d)             */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)             */
	0,      /* green lamp on  (reg 0x30 + 0x31)             */
	0,      /* green lamp off (reg 0x32 + 0x33)             */
	0,      /* blue lamp on   (reg 0x34 + 0x35)             */
	16383,  /* blue lamp off  (reg 0x36 + 0x37)             */
	0x13,   /* ok stepper motor control (reg 0x45)          */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)    */
	0x1e,   /* steps to reverse on buffer full (reg 0x50)   */
	0xfc,   /* ok acceleration profile (reg 0x51)           */
	0,      /* ok lines to process (reg 0x54)               */
	0x13,   /* ok kickstart (reg 0x55)                      */
	0x03,   /* ok pwm freq (reg 0x56)                       */
	0x20,   /* ok pwm duty cycle (reg 0x57)                 */
	0x0d,	/* ok Paper sense (reg 0x58)                    */
	0x44,   /* ok misc io12 (reg 0x59)                      */
	0x44,   /* ok misc io34 (reg 0x5a)                      */
	0x16,   /* ok misc io56 (reg 0x5b)                      */
	0,      /* ok test mode ADC Output CODE MSB (reg 0x5c)  */
	0,      /* ok test mode ADC Output CODE LSB (reg 0x5d)  */
	0,      /* ok test mode (reg 0x5e)                      */
	_LM9832,
	MODEL_MUSTEK600,
	1.5
};

/** BearPaw 2400 */
static HWDef Hw0x0400_0x1001_0 =
{
	1.0/*1.8*/, /* ok dMaxMotorSpeed                        */
	0.9/*1.8*/, /* ok dMaxMoveSpeed                         */
	0.0,    /* ok dHighSpeed                                */
	12,     /* ok dIntegrationTimeLowLamp                   */
	12,     /* ok dIntegrationTimeHighLamp                  */
	1200 /*	600*/ ,   /* ok wMotorDpi (Full step DPI)       */
	2048,   /* ok wRAMSize (KB)                             */
	9,      /* ok dMinIntegrationTimeLowres (ms)            */
	9,      /* ok dMinIntegrationTimeHighres (ms)           */
	1169,   /* ok wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
	1169,   /* ok wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */

	0x02 /*0x06*/,   /* ok bSensorConfiguration (0x0b)          */
	0x3c,   /* ok sensor control settings (reg 0x0c)        */
	0x3f,   /* ok sensor control settings (reg 0x0d)        */
	0x11,   /* ok sensor control settings (reg 0x0e)        */
            /* ok  mono (reg 0x0f to 0x18) */

	{2, 7, 0, 1, 0, 0, 0, 0, 4, 0},

/*	{5, 14, 12, 15, 18, 21, 0, 0, 0, 9 },*/
	{1,  4,  4,  5,  6,  7, 0, 0, 0, 3 },

	_GREEN_CH, /* bReg_0x26 color mode - bits 4 and 5      */
	0,         /* bReg 0x27 color mode                     */
	1,         /* bReg 0x29 illumination mode              */
	/* illumination mode settings (not used for CCD devices)*/
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	257,    /* StepperPhaseCorrection (reg 0x1a + 0x1b)    */
	13,     /* bOpticBlackStart (reg 0x1c)                 */
	60,     /* bOpticBlackEnd (reg 0x1d)                   */
	10,     /* wActivePixelsStart (reg 0x1e + 0x1f)        */
5416 /*	11000*/,  /* wLineEnd (reg 0x20 + 0x21)            */

	1,      /* ok red lamp on    (reg 0x2c + 0x2d)         */
	16383,  /* ok red lamp off   (reg 0x2e + 0x2f)         */
	1,      /* ok green lamp on  (reg 0x30 + 0x31)         */
	16383,  /* ok green lamp off (reg 0x32 + 0x33)         */
	1,      /* ok blue lamp on   (reg 0x34 + 0x35)         */
	16383,  /* ok blue lamp off  (reg 0x36 + 0x37)         */

	0x03,   /* ok stepper motor control (reg 0x45)         */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)   */
	0x1e,   /* steps to reverse on buffer full (reg 0x50)  */
	0xfc,   /* ok acceleration profile (reg 0x51)          */
	0x03,   /* ok lines to process (reg 0x54)              */
	0x13,   /* Kickstart      0x55                         */
	2,      /* PWM frequency  0x56                         */
	32,     /* PWM duty cycle 0x57                         */
	0x15,   /* paper sense 0x58                            */
	0x44,   /* misc I/O 0x59                               */
	0x44,   /* misc I/O 0x5a,                              */
	0x46,   /* misc I/O 0x5b                               */
	0, 0, 0,/* test registers, set to 0 (0x5c, 0x5d, 0x5e) */
	_LM9832,
	MODEL_MUSTEK1200,
	1.0
};

/** EPSON Perfection/Photo 1250 */
static HWDef Hw0x04B8_0x010F =
{
	0.8,    /* dMaxMotorSpeed                               */
	0.8,    /* dMaxMoveSpeed                                */
	4.1,    /* dHighSpeed                                   */
	12,     /* dIntegrationTimeLowLamp                      */
	12,     /* dIntegrationTimeHighLamp                     */
	600,    /* wMotorDpi (Full step DPI)                    */
	512,    /* wRAMSize (KB)                                */
	4,      /* dMinIntegrationTimeLowres (ms)               */
	5,      /* dMinIntegrationTimeHighres (ms)              */
	1,      /* ok wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
	1,      /* ok wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */

	0x02,   /* ok bSensorConfiguration (0x0b)               */
	0x04,   /* ok sensor control settings (reg 0x0c)        */
	0x7d,   /* ok sensor control settings (reg 0x0d)        */
	0x37,   /* ok sensor control settings (reg 0x0e)        */

	{0x02, 0x07, 0x00, 0x01, 0x04, 0x07, 0x00, 0x00, 0x03, 0x07},
	        /* ok mono (reg 0x0f to 0x18) */
	{0x06, 0x16, 0x00, 0x05, 0x0c, 0x17, 0x00, 0x00, 0x0a, 0x17},
	        /* ok color (reg 0x0f to 0x18)                  */
	_GREEN_CH,	/* ok bReg_0x26 color mode - bits 4 and 5   */
	0x40,       /* ok bReg 0x27 color mode                  */
	3,          /* bReg 0x29 illumination mode              */
	/* illumination mode settings (not used for CCD devices)*/
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	1,      /* ok StepperPhaseCorrection (reg 0x1a + 0x1b)  */
	0x00,   /* ok bOpticBlackStart (reg 0x1c)               */
	0x42,   /* ok bOpticBlackEnd (reg 0x1d)                 */
	69,     /* ok wActivePixelsStart (reg 0x1e + 0x1f)      */
	10758,  /* ok wLineEnd (reg 0x20 + 0x21)                */

	16383,  /* ok red lamp on    (reg 0x2c + 0x2d)          */
	0,      /* ok red lamp off   (reg 0x2e + 0x2f)          */
	16383,  /* ok green lamp on  (reg 0x30 + 0x31)          */
	0,      /* ok green lamp off (reg 0x32 + 0x33)          */
	16383,  /* ok blue lamp on   (reg 0x34 + 0x35)          */
	0,      /* ok blue lamp off  (reg 0x36 + 0x37)          */

	3,      /* ok stepper motor control (reg 0x45)          */
	0,      /* ok wStepsAfterPaperSensor2 (reg 0x4c + 0x4d) */
	0x1e,   /* steps to reverse on buffer full (reg 0x50)   */
	0x0c,   /* ok acceleration profile (reg 0x51)           */
	0,      /* ok lines to process (reg 0x54)               */
	0x0f,   /* ok kickstart (reg 0x55)                      */
	0x02,   /* ok pwm freq (reg 0x56)                       */
	1,      /* ok pwm duty cycle (reg 0x57)                 */

	0x0d,   /* ok Paper sense (reg 0x58)                    */

	0x41,   /* ok misc io12 (reg 0x59)                      */
	0x44,   /* ok misc io34 (reg 0x5a)                      */
	0x14,   /* ok misc io56 (reg 0x5b)                      */
	0,      /* ok test mode ADC Output CODE MSB (reg 0x5c)  */
	0,      /* ok test mode ADC Output CODE LSB (reg 0x5d)  */
	0,      /* ok test mode (reg 0x5e)                      */
	_LM9832,
	MODEL_EPSON,
	1.0
};

/** EPSON Perfection/Photo 1260 */
static HWDef Hw0x04B8_0x011D =
{
	0.9,    /* dMaxMotorSpeed                               */
	0.8,    /* dMaxMoveSpeed                                */
	3.0,    /* dHighSpeed                                   */
	12,     /* dIntegrationTimeLowLamp                      */
	12,     /* dIntegrationTimeHighLamp                     */
	600,    /* wMotorDpi (Full step DPI)                    */
	512,    /* wRAMSize (KB)                                */
	4,      /* dMinIntegrationTimeLowres (ms)               */
	5,      /* dMinIntegrationTimeHighres (ms)              */
	1,      /* ok wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
	1,      /* ok wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */

	0x02,   /* ok bSensorConfiguration (0x0b)               */
	0x04,   /* ok sensor control settings (reg 0x0c)        */
	0x7d,   /* ok sensor control settings (reg 0x0d)        */
	0x37,   /* ok sensor control settings (reg 0x0e)        */

	{0x02, 0x07, 0x00, 0x01, 0x04, 0x07, 0x00, 0x00, 0x03, 0x07},
	        /* ok mono (reg 0x0f to 0x18) */
	{0x06, 0x0b, 0x00, 0x05, 0x0c, 0x17, 0x00, 0x00, 0x0a, 0x17},
	        /* ok color (reg 0x0f to 0x18)                  */
	_GREEN_CH,  /* ok bReg_0x26 color mode - bits 4 and 5   */
	0x42,       /* ok bReg 0x27 color mode                  */
	3,          /* bReg 0x29 illumination mode              */
	/* illumination mode settings (not used for CCD devices)*/
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	1,      /* ok StepperPhaseCorrection (reg 0x1a + 0x1b)  */
	0x00,   /* ok bOpticBlackStart (reg 0x1c)               */
	0x42,   /* ok bOpticBlackEnd (reg 0x1d)                 */
	69,     /* ok wActivePixelsStart (reg 0x1e + 0x1f)      */
	10766,  /* ok wLineEnd (reg 0x20 + 0x21)                */

	16383,  /* ok red lamp on    (reg 0x2c + 0x2d)          */
	0,      /* ok red lamp off   (reg 0x2e + 0x2f)          */
	16383,  /* ok green lamp on  (reg 0x30 + 0x31)          */
	0,      /* ok green lamp off (reg 0x32 + 0x33)          */
	16383,  /* ok blue lamp on   (reg 0x34 + 0x35)          */
	0,      /* ok blue lamp off  (reg 0x36 + 0x37)          */

	3,      /* ok stepper motor control (reg 0x45)          */
	0,      /* ok wStepsAfterPaperSensor2 (reg 0x4c + 0x4d) */
	0x1e,   /* steps to reverse on buffer full (reg 0x50)   */
	0x0c,   /* ok acceleration profile (reg 0x51)           */
	0,      /* ok lines to process (reg 0x54)               */
	0x0f,   /* ok kickstart (reg 0x55)                      */
	0x02,   /* ok pwm freq (reg 0x56)                       */
	1,      /* ok pwm duty cycle (reg 0x57)                 */

	0x0d,   /* ok Paper sense (reg 0x58)                    */

	0x41,   /* ok misc io12 (reg 0x59)                      */
	0x44,   /* ok misc io34 (reg 0x5a)                      */
	0x14,   /* ok misc io56 (reg 0x5b)                      */
	0,      /* ok test mode ADC Output CODE MSB (reg 0x5c)  */
	0,      /* ok test mode ADC Output CODE LSB (reg 0x5d)  */
	0,      /* ok test mode (reg 0x5e)                      */
	_LM9832,
	MODEL_EPSON,
	1.5
};

/** Umax 3400/3450 */
static HWDef Hw0x1606_0x0060 =
{
	1.5,    /* dMaxMotorSpeed                            */
	0.8,    /* dMaxMoveSpeed                             */
	2.75,   /* dHighSpeed                                */
	9,      /* dIntegrationTimeLowLamp                   */
	9,      /* dIntegrationTimeHighLamp                  */
	600,    /* wMotorDpi (Full step DPI)                 */
	512,    /* wRAMSize (KB)                             */
	8,      /* dMinIntegrationTimeLowres (ms)            */
	8,      /* dMinIntegrationTimeHighres (ms)           */
	4095,   /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
	4095,   /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */

	0x06,   /* bSensorConfiguration (0x0b)               */
	0x73,   /* sensor control settings (reg 0x0c)        */
	0x77,   /* sensor control settings (reg 0x0d)        */
	0x15,   /* sensor control settings (reg 0x0e)        */

	{0x00, 0x03, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x07, 0x03},
            /* mono (reg 0x0f to 0x18)                   */

	{0x01, 0x0c, 0x0e, 0x10, 0x00, 0x00, 0x00, 0x00, 0x16, 0x0c},
                /* color (reg 0x0f to 0x18)              */
	_GREEN_CH,  /* bReg_0x26 color mode - bits 4 and 5   */
	0x40,       /* bReg 0x27 color mode                  */
	1,          /* bReg 0x29 illumination mode           */
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)  */
	0x2f,   /* bOpticBlackStart (reg 0x1c)               */
	0x3e,   /* bOpticBlackEnd (reg 0x1d)                 */
	110,    /* ? wActivePixelsStart (reg 0x1e + 0x1f)    */
	5469,   /* wLineEnd (reg 0x20 + 0x21)                */

	1,      /* red lamp on    (reg 0x2c + 0x2d)          */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)          */
	0,      /* green lamp on  (reg 0x30 + 0x31)          */
	0,      /* green lamp off (reg 0x32 + 0x33)          */
	32,     /* blue lamp on   (reg 0x34 + 0x35)          */
	48,     /* blue lamp off  (reg 0x36 + 0x37)          */

	3,      /* stepper motor control (reg 0x45)          */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d) */
	11,     /* steps to reverse on buffer full (reg 0x50)*/
	0xfc,   /* acceleration profile (reg 0x51)           */
	3,      /* lines to process (reg 0x54)               */
	0xcb,   /* kickstart (reg 0x55)                      */
	0x05,   /* pwm freq (reg 0x56)                       */
	5,      /* pwm duty cycle (reg 0x57)                 */

	0x0d,   /* Paper sense (reg 0x58)                    */

	0x44,   /* misc io12 (reg 0x59)                      */
	0x45,   /* misc io34 (reg 0x5a)                      */
	0x74,   /* misc io56 (reg 0x5b)                      */
	0,      /* test mode ADC Output CODE MSB (reg 0x5c)  */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)  */
	0,      /* test mode (reg 0x5e)                      */
	_LM9832, 
	MODEL_UMAX,
	1.0
};

/** Umax 5400 */
static HWDef Hw0x1606_0x0160 =
{
	1.1,    /* dMaxMotorSpeed                            */
	0.9,    /* dMaxMoveSpeed                             */
	0.0,    /* dHighSpeed                                */
	9,      /* dIntegrationTimeLowLamp                   */
	9,      /* dIntegrationTimeHighLamp                  */
	600,    /* wMotorDpi (Full step DPI)                 */
	512,    /* wRAMSize (KB)                             */
	8,      /* dMinIntegrationTimeLowres (ms)            */
	8,      /* dMinIntegrationTimeHighres (ms)           */
	4095,   /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)   */
	4095,   /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)  */

	0x06,   /* bSensorConfiguration (0x0b)               */
	0x73,   /* sensor control settings (reg 0x0c)        */
	0x77,   /* sensor control settings (reg 0x0d)        */
	0x25,   /* sensor control settings (reg 0x0e)        */

	/* mono (reg 0x0f to 0x18) */
	{0x00, 0x03, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x07, 0x03},

	/* color (reg 0x0f to 0x18)              */
	{0x01, 0x0c, 0x0e, 0x10, 0x00, 0x00, 0x00, 0x00, 0x16, 0x0c},

	_GREEN_CH,	/* bReg_0x26 color mode - bits 4 and 5   */
	0x40,       /* bReg 0x27 color mode                  */
	1,          /* bReg 0x29 illumination mode           */
	
	/* illumination mode settings (not used for CCD devices) */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)  */
	20,     /* bOpticBlackStart (reg 0x1c)               */
	45,     /* bOpticBlackEnd (reg 0x1d)                 */
	110,    /* ? wActivePixelsStart (reg 0x1e + 0x1f)    */
	10669,  /* wLineEnd (reg 0x20 + 0x21)                */

	1,      /* red lamp on    (reg 0x2c + 0x2d)          */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)          */
	0,      /* green lamp on  (reg 0x30 + 0x31)          */
	0,      /* green lamp off (reg 0x32 + 0x33)          */
	32,     /* blue lamp on   (reg 0x34 + 0x35)          */
	48,     /* blue lamp off  (reg 0x36 + 0x37)          */

	3,      /* stepper motor control (reg 0x45)          */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d) */
	11,     /* steps to reverse on buffer full (reg 0x50)*/
	0xfc,   /* acceleration profile (reg 0x51)           */
	3,      /* lines to process (reg 0x54)               */
	0xcb,   /* kickstart (reg 0x55)                      */
	0x05,   /* pwm freq (reg 0x56)                       */
	5,      /* pwm duty cycle (reg 0x57)                 */

	0x0d,   /* Paper sense (reg 0x58)                    */

	0x44,   /* misc io12 (reg 0x59)                      */
	0x45,   /* misc io34 (reg 0x5a)                      */
	0x7c,   /* misc io56 (reg 0x5b)                      */
	0,      /* test mode ADC Output CODE MSB (reg 0x5c)  */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)  */
	0,      /* test mode (reg 0x5e)                      */
	_LM9832,
	MODEL_UMAX1200,
	1.0
};

/** Canon N650U/N656U */
static HWDef Hw0x04A9_0x2206 =
{
	0.76,   /* dMaxMotorSpeed                                */
	0.243,  /* dMaxMoveSpeed                                 */
	0.0,    /* dHighSpeed                                    */
	100,    /* dIntegrationTimeLowLamp                       */
	100,    /* dIntegrationTimeHighLamp                      */
	1200,   /* wMotorDpi (Full step DPI)                     */
	512,    /* wRAMSize (KB)                                 */
	3.75,   /* dMinIntegrationTimeLowres (ms)                */
	5.75,   /* dMinIntegrationTimeHighres (ms)               */
	0,      /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)       */
	0,      /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)      */

	0x15,   /* bSensorConfiguration (0x0b)                   */
	0x4c,   /* sensor control settings (reg 0x0c)            */
	0x2f,   /* sensor control settings (reg 0x0d)            */
	0x00,   /* sensor control settings (reg 0x0e)            */

	/* mono & color (reg 0x0f to 0x18) the
	   same for CIS devices                          */

	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},
	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode (runtime)          */
            /* illumination mode settings                     */
	{ 3,  0,    0, 23,  850,  0,   0 },
	{ 2, 23, 2500, 23, 1800, 23, 950 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	0,      /* bOpticBlackStart (reg 0x1c)                    */
	0,      /* bOpticBlackEnd (reg 0x1d)                      */
	89,     /* ? wActivePixelsStart (reg 0x1e + 0x1f)         */
	6074,   /* wLineEnd (reg 0x20 + 0x21)                     */

	23,     /* red lamp on    (reg 0x2c + 0x2d)               */
	2500,   /* red lamp off   (reg 0x2e + 0x2f)               */
	23,     /* green lamp on  (reg 0x30 + 0x31)               */
	1800,   /* green lamp off (reg 0x32 + 0x33)               */
	23,     /* blue lamp on   (reg 0x34 + 0x35)               */
	950,    /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */
	0x3f,   /* steps to reverse when buffer is full reg 0x50) */
	0xfc,   /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x0f,   /* kickstart (reg 0x55)                           */
	0x08,   /* pwm freq (reg 0x56)                            */
	0x1f,   /* pwm duty cycle (reg 0x57)                      */

	0x05,   /* Paper sense (reg 0x58)                         */

	0x66,   /* misc io12 (reg 0x59)                           */
	0x16,   /* misc io34 (reg 0x5a)                           */
	0x91,   /* misc io56 (reg 0x5b)                           */
	0x01,   /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9832,
	MODEL_CANON600,
	2.0
};

/** Canon N1220U */
static HWDef Hw0x04A9_0x2207 =
{
	0.72,   /* dMaxMotorSpeed                                 */
	0.36,   /* dMaxMoveSpeed                                  */
	0.0,    /* dHighSpeed                                     */
	100,    /* wIntegrationTimeLowLamp                        */
	100,    /* wIntegrationTimeHighLamp                       */
	1200,   /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	3.75,   /* dMinIntegrationTimeLowres (ms)                 */
	5.75,   /* dMinIntegrationTimeHighres (ms)                */
	0,      /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	0,      /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x15,   /* bSensorConfiguration (0x0b)                    */
	0x4c,   /* sensor control settings (reg 0x0c)             */
	0x2f,   /* sensor control settings (reg 0x0d)             */
	0x00,   /* sensor control settings (reg 0x0e)             */

    /* mono (reg 0x0f to 0x18)                        */
	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},

	/* color (reg 0x0f to 0x18)                       */
	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode                    */
	{ 3,  0,     0, 23,  2200,  0,    0 },
	{ 2, 23, 16383, 23, 15000, 23, 6600 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	0,      /* bOpticBlackStart (reg 0x1c)                    */
	0,      /* bOpticBlackEnd (reg 0x1d)                      */
	124,    /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	10586,  /* wLineEnd (reg 0x20 + 0x21)                     */

	23,     /* red lamp on    (reg 0x2c + 0x2d)               */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)               */
	23,     /* green lamp on  (reg 0x30 + 0x31)               */
	15000,  /* green lamp off (reg 0x32 + 0x33)               */
	23,     /* blue lamp on   (reg 0x34 + 0x35)               */
	6600,   /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */
	0,      /* steps to reverse when buffer is full reg 0x50) */
	0xfc,   /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x0f,   /* kickstart (reg 0x55)                           */
	0x08,   /* pwm freq (reg 0x56)                            */
	0x1f,   /* pwm duty cycle (reg 0x57)                      */

	0x05,   /* Paper sense (reg 0x58)                         */

	0x66,   /* misc io12 (reg 0x59)                           */
	0x16,   /* misc io34 (reg 0x5a)                           */
	0x91,   /* misc io56 (reg 0x5b)                           */
	0x01,   /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9832,
	MODEL_CANON1200,
	2.0
};

/** Canon D660U */
static HWDef Hw0x04A9_0x2208 =
{
	1.2,    /* dMaxMotorSpeed                                 */
	1.1,    /* dMaxMoveSpeed                                  */
	2.75,   /* dHighSpeed                                     */
	9,      /* dIntegrationTimeLowLamp                        */
	9,      /* dIntegrationTimeHighLamp                       */
	300,    /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	8,      /* dMinIntegrationTimeLowres (ms)                 */
	8,      /* dMinIntegrationTimeHighres (ms)                */
	4095,   /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	4095,   /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x02,   /* bSensorConfiguration (0x0b)                    */
	0x60,   /* sensor control settings (reg 0x0c)             */
	0x2f,   /* sensor control settings (reg 0x0d)             */
	0x13,   /* sensor control settings (reg 0x0e)             */

	{0x06, 0x17, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x0b },
	{0x06, 0x17, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x0b },
	_GREEN_CH,
	0,      /* bReg 0x27 color mode                           */
	1,      /* bReg 0x29 illumination mode                    */

	/* illumination mode settings (not used for CCD devices)  */
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },

	0,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	13,     /* bOpticBlackStart (reg 0x1c)                    */
	33,     /* bOpticBlackEnd (reg 0x1d)                      */
	64,     /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	5440,   /* wLineEnd (reg 0x20 + 0x21)                     */

	16383,  /* red lamp on    (reg 0x2c + 0x2d)               */
	1,      /* red lamp off   (reg 0x2e + 0x2f)               */
	0,      /* green lamp on  (reg 0x30 + 0x31)               */
	0,      /* green lamp off (reg 0x32 + 0x33)               */
	0,      /* blue lamp on   (reg 0x34 + 0x35)               */
	0,      /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */
	0x20,   /* steps to reverse when buffer is full reg 0x50) */
	0xfc,   /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0,      /* kickstart (reg 0x55)                           */
	0x02,   /* pwm freq (reg 0x56)                            */
	0x1f,   /* pwm duty cycle (reg 0x57)                      */
	0x05,   /* Paper sense (reg 0x58)                         */
	0x44,   /* misc io12 (reg 0x59)                           */
	0x14,   /* misc io34 (reg 0x5a)                           */
	0x11,   /* misc io56 (reg 0x5b)                           */
	0,      /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9832,
	MODEL_CANONCCD1200,
	1.5
};

/** Canon 670/676/LiDE20 */
static HWDef Hw0x04A9_0x220D =
{
	0.72,   /* dMaxMotorSpeed                                 */
	0.23,   /* dMaxMoveSpeed                                  */
	0.0,    /* dHighSpeed                                     */
	100,    /* dIntegrationTimeLowLamp                        */
	100,    /* dIntegrationTimeHighLamp                       */
	1200,   /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	3.75,   /* dMinIntegrationTimeLowres (ms)                 */
	5.75,   /* dMinIntegrationTimeHighres (ms)                */
	0,      /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	0,      /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x15,   /* bSensorConfiguration (0x0b)                    */
	0x4c,   /* sensor control settings (reg 0x0c)             */
	0x2f,   /* sensor control settings (reg 0x0d)             */
	0x00,   /* sensor control settings (reg 0x0e)             */

	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},
	        /* mono (reg 0x0f to 0x18)                        */

	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},
	        /* color (reg 0x0f to 0x18)                       */

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode (runtime)          */

	{ 3,  0,    0, 23, 1800,  0,    0 },
	{ 2, 23, 3800, 23, 3300, 23, 2700 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	0,      /* bOpticBlackStart (reg 0x1c)                    */
	0,      /* bOpticBlackEnd (reg 0x1d)                      */
	75,     /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	6074,   /* wLineEnd (reg 0x20 + 0x21)                     */

	23,     /* red lamp on    (reg 0x2c + 0x2d)               */
	3800,   /* red lamp off   (reg 0x2e + 0x2f)               */
	23,     /* green lamp on  (reg 0x30 + 0x31)               */
	3300,   /* green lamp off (reg 0x32 + 0x33)               */
	23,     /* blue lamp on   (reg 0x34 + 0x35)               */
	2700,   /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */
	0x3f,   /* steps to reverse when buffer is full reg 0x50) */
	0xfc,   /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x0f,   /* kickstart (reg 0x55)                           */
	0x08,   /* pwm freq (reg 0x56)                            */
	0x1f,   /* pwm duty cycle (reg 0x57)                      */

	0x04,   /* Paper sense (reg 0x58)                         */

	0x66,   /* misc io12 (reg 0x59)                           */
	0x16,   /* misc io34 (reg 0x5a)                           */
	0x91,   /* misc io56 (reg 0x5b)                           */
	0x01,   /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9833,
	MODEL_CANON600,
	2.0
};

/** Canon N1240U/LiDE30 */
static HWDef Hw0x04A9_0x220E =
{
	0.72,   /* dMaxMotorSpeed                                 */
	0.30,   /* dMaxMoveSpeed                                  */
	0.0,    /* dHighSpeed                                     */
	100,    /* wIntegrationTimeLowLamp                        */
	100,    /* wIntegrationTimeHighLamp                       */
	1200,   /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	3.75,   /* dMinIntegrationTimeLowres (ms)                 */
	5.75,   /* dMinIntegrationTimeHighres (ms)                */
	0,      /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	0,      /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x15,   /* bSensorConfiguration (0x0b)                    */
	0x4c,   /* sensor control settings (reg 0x0c)             */
	0x2f,   /* sensor control settings (reg 0x0d)             */
	0x00,   /* sensor control settings (reg 0x0e)             */

	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},
	        /* mono (reg 0x0f to 0x18)                        */

	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x05},
	        /* color (reg 0x0f to 0x18)                       */

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode                    */

	{ 3,  0,     0, 23,  3000,  0,    0 },
	{ 2, 23, 16383, 23,  6500, 23, 4900 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	0,      /* bOpticBlackStart (reg 0x1c)                    */
	0,      /* bOpticBlackEnd (reg 0x1d)                      */
	52,     /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	10586,  /* wLineEnd (reg 0x20 + 0x21)                     */

	23,     /* red lamp on    (reg 0x2c + 0x2d)               */
	16383,  /* red lamp off   (reg 0x2e + 0x2f)               */
	23,     /* green lamp on  (reg 0x30 + 0x31)               */
	6500,   /* green lamp off (reg 0x32 + 0x33)               */
	23,     /* blue lamp on   (reg 0x34 + 0x35)               */
	4900,   /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */
	0x20,   /* steps to reverse when buffer is full reg 0x50) */
	0xfc,   /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x0f,   /* kickstart (reg 0x55)                           */
	0x08,   /* pwm freq (reg 0x56)                            */
	0x1f,   /* pwm duty cycle (reg 0x57)                      */

	0x04,   /* Paper sense (reg 0x58)                         */

	0x66,   /* misc io12 (reg 0x59)                           */
	0x16,   /* misc io34 (reg 0x5a)                           */
	0x91,   /* misc io56 (reg 0x5b)                           */
	0x01,   /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9833,
	MODEL_CANON1200,
	2.0
};

/** Canon LiDE25 */
static HWDef Hw0x04A9_0x2220 =
{
	0.70,   /* dMaxMotorSpeed                                 */
	0.25,   /* dMaxMoveSpeed                                  */
	0.0,    /* dHighSpeed                                     */
	100,    /* wIntegrationTimeLowLamp                        */
	100,    /* wIntegrationTimeHighLamp                       */
	1200,   /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	3.75,   /* dMinIntegrationTimeLowres (ms)                 */
	5.75,   /* dMinIntegrationTimeHighres (ms)                */
	0,      /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	0,      /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x11,   /* bSensorConfiguration (0x0b)                    */
	0x4d,   /* sensor control settings (reg 0x0c)             */
	0x2f,   /* sensor control settings (reg 0x0d)             */
	0x00,   /* sensor control settings (reg 0x0e)             */

	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x07},
	        /* mono (reg 0x0f to 0x18)                        */

	{0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x07},
	        /* color (reg 0x0f to 0x18)                       */

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode                    */

	{ 3,  0,    0, 50, 3700,  0,    0 },
	{ 2, 50, 5600, 50, 7100, 50, 4600 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	0,      /* bOpticBlackStart (reg 0x1c)                    */
	0,      /* bOpticBlackEnd (reg 0x1d)                      */
	0x7f,   /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	10559,  /* wLineEnd (reg 0x20 + 0x21)                     */

	50,     /* red lamp on    (reg 0x2c + 0x2d)               */
	5600,   /* red lamp off   (reg 0x2e + 0x2f)               */
	50,     /* green lamp on  (reg 0x30 + 0x31)               */
	7100,   /* green lamp off (reg 0x32 + 0x33)               */
	50,     /* blue lamp on   (reg 0x34 + 0x35)               */
	4600,   /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */
	0x20,   /* steps to reverse when buffer is full reg 0x50) */
	0xfc,   /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x0f,   /* kickstart (reg 0x55)                           */
	0x08,   /* pwm freq (reg 0x56)                            */
	0x1f,   /* pwm duty cycle (reg 0x57)                      */

	0x04,   /* Paper sense (reg 0x58)                         */

	0x66,   /* misc io12 (reg 0x59)                           */
	0x16,   /* misc io34 (reg 0x5a)                           */
	0x91,   /* misc io56 (reg 0x5b)                           */
	0x01,   /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9833,
	MODEL_CANON_LIDE25,
	2.0
};

/** TravelScan 662 */
static HWDef Hw0x0A82_0x6620 =
{
	0.72,   /* dMaxMotorSpeed                                 */
	0.188,  /* dMaxMoveSpeed                                  */
	0.0,    /* dHighSpeed                                     */
	100,    /* wIntegrationTimeLowLamp                        */
	100,    /* wIntegrationTimeHighLamp                       */
	740,    /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	3.75,   /* dMinIntegrationTimeLowres (ms)                 */
	5.75,   /* dMinIntegrationTimeHighres (ms)                */
	3000,   /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	3000,   /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x0d,   /* bSensorConfiguration (0x0b)                    */
	0x00,   /* sensor control settings (reg 0x0c)             */
	0x25,   /* sensor control settings (reg 0x0d)             */
	0x00,   /* sensor control settings (reg 0x0e)             */

	{0x18, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07},
	        /* mono (reg 0x0f to 0x18)                        */

	{0x18, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07},
	        /* color (reg 0x0f to 0x18)                       */

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode                    */

	{ 3,  0,    0, 10, 450,  0,   0 },
	{ 2, 10, 1000, 10, 880, 10, 630 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	1,      /* bOpticBlackStart (reg 0x1c)                    */
	2,      /* bOpticBlackEnd (reg 0x1d)                      */
	0x17,   /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	2600,   /* wLineEnd (reg 0x20 + 0x21)                     */

	10,     /* red lamp on    (reg 0x2c + 0x2d)               */
	1000,   /* red lamp off   (reg 0x2e + 0x2f)               */
	10,     /* green lamp on  (reg 0x30 + 0x31)               */
	880,    /* green lamp off (reg 0x32 + 0x33)               */
	10,     /* blue lamp on   (reg 0x34 + 0x35)               */
	630,    /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */

	0,      /* steps to reverse when buffer is full reg 0x50) */
	0,      /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x09,   /* kickstart (reg 0x55)                           */
	0x02,   /* pwm freq (reg 0x56)                            */
	0x16,   /* pwm duty cycle (reg 0x57)                      */

	0x01,   /* Paper sense (reg 0x58)                         */

	0x0e,   /* misc io12 (reg 0x59)                           */
	0x96,   /* misc io34 (reg 0x5a)                           */
	0x01,   /* misc io56 (reg 0x5b)                           */
	0,      /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9833,
	MODEL_TSCAN,
	1.8
};

/** TravelScan 464 */
static HWDef Hw0x0A82_0x4600 =
{
	0.72,   /* dMaxMotorSpeed                                 */
	0.26,   /* dMaxMoveSpeed                                  */
	0.0,    /* dHighSpeed                                     */
	100,    /* wIntegrationTimeLowLamp                        */
	100,    /* wIntegrationTimeHighLamp                       */
	740,    /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	3.75,   /* dMinIntegrationTimeLowres (ms)                 */
	5.75,   /* dMinIntegrationTimeHighres (ms)                */
	3000,   /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	3000,   /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x0d,   /* bSensorConfiguration (0x0b)                    */
	0x00,   /* sensor control settings (reg 0x0c)             */
	0x25,   /* sensor control settings (reg 0x0d)             */
	0x00,   /* sensor control settings (reg 0x0e)             */

	{0x18, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07},
	        /* mono (reg 0x0f to 0x18)                        */

	{0x18, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07},
	        /* color (reg 0x0f to 0x18)                       */

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode                    */

	{ 3, 500, 1045, 500, 990, 500, 990 },
	{ 2, 500, 1045, 500, 990, 500, 990 },

	1,      /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	1,      /* bOpticBlackStart (reg 0x1c)                    */
	2,      /* bOpticBlackEnd (reg 0x1d)                      */
	0x17,   /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	5300,   /* wLineEnd (reg 0x20 + 0x21)                     */

	500,    /* red lamp on    (reg 0x2c + 0x2d)               */
	1045,   /* red lamp off   (reg 0x2e + 0x2f)               */
	500,    /* green lamp on  (reg 0x30 + 0x31)               */
	990,    /* green lamp off (reg 0x32 + 0x33)               */
	500,    /* blue lamp on   (reg 0x34 + 0x35)               */
	990,    /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */

	0,      /* steps to reverse when buffer is full reg 0x50) */
	0,      /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x09,   /* kickstart (reg 0x55)                           */
	0x02,   /* pwm freq (reg 0x56)                            */
	0x16,   /* pwm duty cycle (reg 0x57)                      */

	0x01,   /* Paper sense (reg 0x58)                         */

	0x0e,   /* misc io12 (reg 0x59)                           */
	0x96,   /* misc io34 (reg 0x5a)                           */
	0x01,   /* misc io56 (reg 0x5b)                           */
	0,      /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9833,
	MODEL_TSCAN_A4,
	1.8
};

/** IRIScan/Q-Scan USB001 - Portable Peripheral Co., Ltd. */
static HWDef Hw0x0A53_0x1000 =
{
	0.50,   /* dMaxMotorSpeed                                 */
	0.40,   /* dMaxMoveSpeed                                  */
	0.0,    /* dHighSpeed                                     */
	100,    /* wIntegrationTimeLowLamp                        */
	100,    /* wIntegrationTimeHighLamp                       */
	300,    /* wMotorDpi (Full step DPI)                      */
	512,    /* wRAMSize (KB)                                  */
	3.75,   /* dMinIntegrationTimeLowres (ms)                 */
	5.75,   /* dMinIntegrationTimeHighres (ms)                */
	4095,   /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	4095,   /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x09,   /* bSensorConfiguration (0x0b)                    */
	0x00,   /* sensor control settings (reg 0x0c)             */
	0x65,   /* sensor control settings (reg 0x0d)             */
	0x13,   /* sensor control settings (reg 0x0e)             */

	{0x02, 0x07, 0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0x06, 0x02},
	       /* mono (reg 0x0f to 0x18)                         */

	{0x02, 0x07, 0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0x06, 0x02},
	      /* color (reg 0x0f to 0x18)                         */

	(_BLUE_CH | _ONE_CH_COLOR), /* bReg_0x26 color mode       */

	0x00,   /* bReg 0x27 color mode                           */
	2,      /* bReg 0x29 illumination mode                    */

	{ 3,    0,    0, 2593, 4600,    0,    0 },
	{ 2, 2593, 7100, 2593, 4600, 2593, 4480 },

	256,    /* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	0,      /* bOpticBlackStart (reg 0x1c)                    */
	0x15,   /* bOpticBlackEnd (reg 0x1d)                      */
	0x15,   /* wActivePixelsStart (reg 0x1e + 0x1f)           */
	5500,   /* wLineEnd (reg 0x20 + 0x21)                     */

	2593,   /* red lamp on    (reg 0x2c + 0x2d)               */
	7100,   /* red lamp off   (reg 0x2e + 0x2f)               */
	2593,   /* green lamp on  (reg 0x30 + 0x31)               */
	4600,   /* green lamp off (reg 0x32 + 0x33)               */
	2593,   /* blue lamp on   (reg 0x34 + 0x35)               */
	4480,   /* blue lamp off  (reg 0x36 + 0x37)               */

	3,      /* stepper motor control (reg 0x45)               */
	0,      /* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */

	0,      /* steps to reverse when buffer is full reg 0x50) */
	0,      /* acceleration profile (reg 0x51)                */
	0,      /* lines to process (reg 0x54)                    */
	0x1b,   /* kickstart (reg 0x55)                           */
	0x08,   /* pwm freq (reg 0x56)                            */
	0x15,   /* pwm duty cycle (reg 0x57)                      */

	0x00,   /* Paper sense (reg 0x58)                         */

	0x00,   /* misc io12 (reg 0x59)                           */
	0x00,   /* misc io34 (reg 0x5a)                           */
	0x00,   /* misc io56 (reg 0x5b)                           */
	0,      /* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,      /* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,      /* test mode (reg 0x5e)                           */
	_LM9832,
	MODEL_QSCAN,
	1.1
};

/** PandP USB201 Q-Scan A6 Scanner */
static HWDef Hw0x0A53_0x2000 =
{
	0.5,	/* dMaxMotorSpeed                                 */
	0.2,	/* dMaxMoveSpeed                                  */
	0.0,	/* dHighSpeed                                     */
	100,	/* wIntegrationTimeLowLamp                        */
	100,	/* wIntegrationTimeHighLamp                       */
	570,	/* wMotorDpi (Full step DPI)                      */
	512,	/* wRAMSize (KB)                                  */
	3.75,	/* dMinIntegrationTimeLowres (ms)                 */
	5.75,	/* dMinIntegrationTimeHighres (ms)                */
	0x0fff, /* wGreenPWMDutyCycleLow (reg 0x2a + 0x2b)        */
	0x0fff, /* wGreenPWMDutyCycleHigh (reg 0x2a + 0x2b)       */

	0x02,	/* bSensorConfiguration (0x0b)                    */
	0x00,	/* sensor control settings (reg 0x0c)             */
	0x25,	/* sensor control settings (reg 0x0d)             */
	0x06,	/* sensor control settings (reg 0x0e)             */

	{0x05, 0x0a, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x04, 0x07},
		/* mono (reg 0x0f to 0x18)                        */

	{0x05, 0x0a, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x04, 0x07},
		/* color (reg 0x0f to 0x18)                       */

	(_BLUE_CH | _ONE_CH_COLOR),	/* bReg_0x26 color mode   */

	0x00,	/* bReg 0x27 color mode                           */
	2,	/* bReg 0x29 illumination mode                    */

	{ 3,  0,    0, 10, 450,  0,   0 },
		/* initial illumination settings - mono           */
	{ 2, 10, 1000, 10, 880, 10, 630 },
		/* initial illumination settings - color          */

	0x0101,	/* StepperPhaseCorrection (reg 0x1a + 0x1b)       */
	0x004d,	/* bOpticBlackStart (reg 0x22 + 0x23)             */
	0x11,	/* bOpticBlackEnd                                 */
	0x0011,	/* wActivePixelsStart                             */
	0x0bb8,	/* wLineEnd (reg 0x20 + 0x21)                     */

	10,	/* red lamp on    (reg 0x2c + 0x2d)               */
	1000,	/* red lamp off   (reg 0x2e + 0x2f)               */
	10,	/* green lamp on  (reg 0x30 + 0x31)               */
	880,	/* green lamp off (reg 0x32 + 0x33)               */
	10,	/* blue lamp on   (reg 0x34 + 0x35)               */
	630,	/* blue lamp off  (reg 0x36 + 0x37)               */

	0x13,	/* stepper motor control (reg 0x45)               */
	0x0000,	/* wStepsAfterPaperSensor2 (reg 0x4c + 0x4d)      */

	0,	/* steps to reverse when buffer is full reg 0x50) */
	0,	/* acceleration profile (reg 0x51)                */
	0,	/* lines to process (reg 0x54)                    */
	0x1b,	/* kickstart (reg 0x55)                           */
	0x08,	/* pwm freq (reg 0x56)                            */
	0x15,	/* pwm duty cycle (reg 0x57)                      */

	0x00,	/* Paper sense (reg 0x58)                         */

	0x20,	/* misc io12 (reg 0x59)                           */
	0x02,	/* misc io34 (reg 0x5a)                           */
	0x90,	/* misc io56 (reg 0x5b)                           */
	0,	/* test mode ADC Output CODE MSB (reg 0x5c)       */
	0,	/* test mode ADC Output CODE LSB (reg 0x5d)       */
	0,	/* test mode (reg 0x5e)                           */
	_LM9832,
	MODEL_QSCAN_A6,
	1.8
};

/******************** all available combinations *****************************/

/** here we have all supported devices and their settings...
 */
static SetDef Settings[] =
{
	/* Plustek devices... */
	/* LM9831 based */
	{"0x07B3-0x0010-0", &Cap0x07B3_0x0010_0, &Hw0x07B3_0x0013_0, "OpticPro U12"   },
	{"0x07B3-0x0011-0", &Cap0x07B3_0x0011_0, &Hw0x07B3_0x0013_0, "OpticPro U24"   },
	{"0x07B3-0x0013-0", &Cap0x07B3_0x0013_0, &Hw0x07B3_0x0013_0, "OpticPro UT12"  },

	/* LM9832 based */
	{"0x07B3-0x0017-0", &Cap0x07B3_0x0017_0, &Hw0x07B3_0x0017_0, "OpticPro UT12/UT16" },
	{"0x07B3-0x0015-0", &Cap0x07B3_0x0015_0, &Hw0x07B3_0x0017_0, "OpticPro U24"   },
	{"0x07B3-0x0015-4", &Cap0x07B3_0x0015_4, &Hw0x07B3_0x0017_4, "OpticPro U24"   },
	{"0x07B3-0x0017-4", &Cap0x07B3_0x0017_4, &Hw0x07B3_0x0017_4, "OpticPro UT24"  },

	/* never seen yet */
	{"0x07B3-0x0013-4", &Cap0x07B3_0x0013_4, &Hw0x07B3_0x0013_4, "Unknown device" },
	{"0x07B3-0x0011-4", &Cap0x07B3_0x0011_4, &Hw0x07B3_0x0013_4, "Unknown device" },
	{"0x07B3-0x0010-4", &Cap0x07B3_0x0010_4, &Hw0x07B3_0x0013_4, "Unknown device" },
	{"0x07B3-0x0014-0", &Cap0x07B3_0x0014_0, &Hw0x07B3_0x0017_0, "Unknown device" },
	{"0x07B3-0x0014-4", &Cap0x07B3_0x0014_4, &Hw0x07B3_0x0017_4, "Unknown device" },
	{"0x07B3-0x0016-4", &Cap0x07B3_0x0016_4, &Hw0x07B3_0x0016_4, "Unknown device" },
	{"0x07B3-0x0017-2", &Cap0x07B3_0x0017_2, &Hw0x07B3_0x0017_2, "Unknown device" },
	{"0x07B3-0x0017-3", &Cap0x07B3_0x0017_3, &Hw0x07B3_0x0017_3, "Unknown device" },
	{"0x07B3-0x0007",   &Cap0x07B3_0x0007_0, &Hw0x07B3_0x0007_0, "Unknown device" },
	{"0x07B3-0x000F",   &Cap0x07B3_0x000F_0, &Hw0x07B3_0x000F_0, "Unknown device" },
	{"0x07B3-0x000F-4", &Cap0x07B3_0x000F_4, &Hw0x07B3_0x000F_4, "Unknown device" },
	{"0x07B3-0x0005-2", &Cap0x07B3_0x0005_2, &Hw0x07B3_0x0007_2, "Unknown device" }, /* TOKYO 600 */
	{"0x07B3-0x0014-1", &Cap0x07B3_0x0014_1, &Hw0x07B3_0x0017_1, "Unknown device" }, /* A3 */
	{"0x07B3-0x0012-0", &Cap0x07B3_0x0012_0, &Hw0x07B3_0x0012_0, "Unknown device" }, /* Brother Demo */

	/* Mustek BearPaw...*/
	{"0x0400-0x1000-0", &Cap0x0400_0x1000_0, &Hw0x0400_0x1000_0, "BearPaw 1200" },
	{"0x0400-0x1001-1", &Cap0x0400_0x1000_0, &Hw0x0400_0x1001_1, "BearPaw 1200" },
	{"0x0400-0x1001-0", &Cap0x0400_0x1001_0, &Hw0x0400_0x1001_0, "BearPaw 2400" },

	/* Genius devices... */
	{"0x0458-0x2007",   &Cap0x07B3_0x0007_0, &Hw0x07B3_0x0007_0, "ColorPage-HR6 V2" },
	{"0x0458-0x2008",   &Cap0x07B3_0x0007_0, &Hw0x07B3_0x0007_0, "ColorPage-HR6 V2" },
	{"0x0458-0x2009",   &Cap0x07B3_0x000F_0, &Hw0x07B3_0x000F_0, "ColorPage-HR6A"   },
	{"0x0458-0x2013",   &Cap0x07B3_0x0007_4, &Hw0x07B3_0x0007_4, "ColorPage-HR7"    },
	{"0x0458-0x2015",   &Cap0x07B3_0x0005_4, &Hw0x07B3_0x0007_4, "ColorPage-HR7LE"  },
	{"0x0458-0x2016",   &Cap0x07B3_0x0005_4, &Hw0x07B3_0x0007_0, "ColorPage-HR6X"   },

	/* Hewlett Packard... */
	{"0x03F0-0x0505",   &Cap0x03F0_0x0505, &Hw0x03F0_0x0505, "Scanjet 2100c" },
	{"0x03F0-0x0605",   &Cap0x03F0_0x0605, &Hw0x03F0_0x0605, "Scanjet 2200c" },

	/* EPSON... */
	{"0x04B8-0x010F",   &Cap0x04B8_0x010F, &Hw0x04B8_0x010F, "Perfection 1250/Photo" },
	{"0x04B8-0x011D",   &Cap0x04B8_0x010F, &Hw0x04B8_0x011D, "Perfection 1260/Photo" },

	/* UMAX... */
	{"0x1606-0x0050",   &Cap0x1606_0x0060, &Hw0x1606_0x0060, "3400" },
	{"0x1606-0x0060",   &Cap0x1606_0x0060, &Hw0x1606_0x0060, "3400" },
	{"0x1606-0x0160",   &Cap0x1606_0x0160, &Hw0x1606_0x0160, "5400" },
  
	/* COMPAQ... */
	{"0x049F-0x001A",   &Cap0x1606_0x0060, &Hw0x1606_0x0060, "S4-100" },

	/* CANON... */
	{"0x04A9-0x2206",   &Cap0x04A9_0x2206, &Hw0x04A9_0x2206, "CanoScan N650U/N656U" },
	{"0x04A9-0x2207",   &Cap0x04A9_0x2207, &Hw0x04A9_0x2207, "CanoScan N1220U"      },
	{"0x04A9-0x2208",   &Cap0x04A9_0x2208, &Hw0x04A9_0x2208, "CanoScan D660U"       },
	{"0x04A9-0x220D",   &Cap0x04A9_0x220D, &Hw0x04A9_0x220D, "CanoScan N670U/N676U/LiDE20" },
	{"0x04A9-0x220E",   &Cap0x04A9_0x220E, &Hw0x04A9_0x220E, "CanoScan N1240U/LiDE30"      },
	{"0x04A9-0x2220",   &Cap0x04A9_0x2220, &Hw0x04A9_0x2220, "CanoScan LiDE25"      },

	/* SYSCAN... */
	{"0x0A82-0x4600",   &Cap0x0A82_0x4600, &Hw0x0A82_0x4600, "TravelScan 460/464" },
	{"0x0A82-0x6620",   &Cap0x0A82_0x6620, &Hw0x0A82_0x6620, "TravelScan 662" },

	/* Visioneer... */
	{"0x04A7-0x0427",   &Cap0x0A82_0x4600, &Hw0x0A82_0x4600, "Strobe XP100" },

	/* Portable Peripheral Co., Ltd. */
	{"0x0A53-0x1000",   &Cap0x0A53_0x1000, &Hw0x0A53_0x1000, "Q-Scan USB001" },
	{"0x0A53-0x2000",   &Cap0x0A53_0x2000, &Hw0x0A53_0x2000, "Q-Scan USB201" },

	/* Please add other devices here...
	 * The first entry is a string, composed out of the vendor and product id,
	 * it's used by the driver to select the device settings. For other devices
	 * than those of Plustek, you'll not need to add the second '-' part
	 *
	 * The second entry decribes the capabilities of the device, you may find
	 * one suitable for your scanner, for a better description of the entries
	 * have a look at the beginning of this file at Cap0x07B3_0x0017_0 for
	 * the UT12
	 *
	 * The third entry is for the default setting of the LM983x register
	 * settings, you can often find these in your Windoze driver ini
	 * Have a look at the Hw0x0400_0x1000_0 or Hw0x07B3_0x0017_0 for further
	 * description
	 *
	 * The fourth entry is simply the name of the device, which will be
	 * displayed by the frontend
	 */
	{ NULL, NULL, NULL, NULL }  /* last entry, never remove... */
};

/**
 * tables for the motor settings
 * The models KaoHsiung, HuaLien and Tokyo600 are currently set
 * within the code in conjunction with some CCD combinations.
 * NOTE: the touples PWM and PWM_Duty are used to set the registers
 *       0x56 and 0x57, the recommended setting is 8,10
 *       if you notice a whining noise and the motor does not move,
 *       you might increase the MCLK variable.
 */
static ClkMotorDef Motors[] = {

	{ MODEL_KaoHsiung,
		64, 20, 6, /* PWM, PWM_Duty, MCLK for fast move */
		0, 0,      /* for lineend adjustment, here disabled */
		/* Motor settings (PWM and PWM_Duty) */
		{{ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
		 { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }},
		/* Color mode MCLK settings */
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		/* Gray mode MCLK settings */
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 }
	},

	{ MODEL_HuaLien, 64, 20, 6, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		{{ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
		 { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }},
		/* Color mode MCLK settings */
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		/* Gray mode MCLK settings */
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 }
	},

	{ MODEL_Tokyo600, 4, 4, 6, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		{{ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
		 { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }},
		/* Color mode MCLK settings */
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		/* Gray mode MCLK settings */
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 },
		{ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3 }
	},

	{ MODEL_MUSTEK600, 4, 4, 6, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		{{ 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 },
		 { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }},
		/* Color mode MCLK settings */
		{ 4.0, 3.5, 3.5, 4.0, 4.0, 5.0, 5.0, 7.5, 7.5, 7.5 },
		{ 4.0, 3.5, 3.5, 4.0, 4.0, 5.0, 5.0, 7.5, 7.5, 7.5 },
		/* Gray mode MCLK settings */
		{ 7.5, 7.0, 6.5, 5.5, 5.5, 5.5, 7.0, 7.0, 7.0, 7.0 },
		{ 7.5, 7.0, 6.5, 5.5, 5.5, 5.5, 7.0, 7.0, 7.0, 7.0 }
	},

	{ MODEL_MUSTEK1200, 2, 32, 3, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi   */
		{{ 2, 32, 1 }, { 2, 32, 1 }, { 2, 32, 1 }, { 2, 32, 1 }, { 2, 32, 1 },
		/* <=400dpi      <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		 { 2, 32, 1 }, { 2, 32, 1 }, { 2, 32, 1 }, { 2, 32, 1 }, { 2, 32, 1 }},
		/* Color mode MCLK settings */
		{ 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5 },
		{ 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5 },
		/* Gray mode MCLK settings */
		{ 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0 },
		{ 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0 }
	},

	/* settings good for the HP models (tested with 2200)*/
	{ MODEL_HP, 8, 60, 6, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		{{ 8, 60, 1 }, { 8, 60, 1 }, { 8, 60, 1 }, { 8, 60, 1 }, { 8, 60, 1 },
		 { 8, 60, 1 }, { 8, 60, 1 }, { 8, 60, 1 }, { 8, 60, 1 }, { 8, 60, 1 }},
    	/* Color mode MCLK settings */
		{ 4.0, 4.0, 4.0, 4.0, 3.0, 4.0, 6.0, 6.0, 6.0, 6.0 },
		{ 4.0, 4.0, 4.0, 4.0, 3.0, 4.0, 6.0, 6.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 8.0, 8.0, 8.0, 8.0, 8.0, 13.0, 13.0, 13.0, 13.0, 13.0 },
		{ 8.0, 8.0, 8.0, 8.0, 8.0, 13.0, 13.0, 13.0, 13.0, 13.0 }
	},

	{ MODEL_CANON600, 8, 31, 12, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi  */
		{{ 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 },
		/* <=400dpi      <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		 { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }},
		/* Color mode MCLK settings */
		{ 7.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		{ 7.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 15.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0 },
		{ 15.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0 }
	},

	{ MODEL_CANON1200, 8, 31, 8, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi  */
		{{ 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 },
		/* <=400dpi      <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		 { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }},
		/* Color mode MCLK settings */
		{ 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0 },
		{ 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 5.0, 6.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 8.5, 7.0, 5.0, 4.0, 2.5, 2.5, 2.5,  2.5,  2.5,  2.5 },
		{ 8.5, 6.5, 6.0, 6.0, 6.0, 6.0, 8.0, 12.0, 12.0, 12.0 }
	},

	/* settings good for the UMAX models (tested with 3400) */
	{ MODEL_UMAX, 16, 4, 6, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		{{ 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 },
		 { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }},
    	/* Color mode MCLK settings */
		{ 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5 },
		{ 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5 },
		/* Gray mode MCLK settings */
		{ 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5 },
		{ 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5, 10.5 }
	},

	{ MODEL_UMAX1200, 16, 4, 6, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		{{ 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 },
		 { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }, { 16, 4, 1 }},
		/* Color mode MCLK settings */
		{ 3.0, 3.0, 3.0, 3.0, 3.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		{ 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0 },
		/* Gray mode MCLK settings */
		{  6.0,  6.0,  6.0,  6.0,  6.0, 13.0, 13.0, 13.0, 13.0, 13.0 },
		{ 13.0, 13.0, 13.0, 13.0, 13.0, 13.0, 13.0, 13.0, 13.0, 13.0 }
	},

	/* settings good for the EPSON models */
	{ MODEL_EPSON, 2, 1, 6, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi      <=100dpi     <=150dpi     <=200dpi     <=300dpi  */
		{{ 2, 1, 1 }, { 2, 1, 1 }, { 2, 1, 1 }, { 2, 1, 1 }, { 2, 1, 1 },
		/* <=400dpi     <=600dpi     <=800dpi     <=1200dpi    <=2400dpi */
		 { 2, 1, 1 }, { 2, 1, 1 }, { 2, 1, 1 }, { 2, 1, 1 }, { 2, 1, 1 }},
		/* Color mode MCLK settings */
		{ 2.0, 2.0, 2.5, 2.5, 2.5, 2.5, 4.0, 4.0, 6.0, 6.0 },
		{ 2.0, 2.0, 2.5, 2.5, 3.0, 3.0, 3.0, 4.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 9.0, 9.0, 18.0, 18.0 },
		{ 6.0, 6.0, 6.0, 6.0, 6.0, 8.0, 9.0, 9.0, 18.0, 18.0 }
	},

	{ MODEL_CANONCCD1200, 2, 31, 3, 0, 0,
		/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi  */
		{{ 2, 31, 1 }, { 2, 31, 1 }, { 2, 31, 1 }, { 2, 31, 1 }, { 2, 31, 1 },
		/* <=400dpi      <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		 { 2, 31, 1 }, { 2, 31, 1 }, { 2, 31, 1 }, { 2, 31, 1 }, { 2, 31, 1 }},
		/* Color mode MCLK settings */
		{ 2.0, 2.0, 2.0, 2.5, 2.5, 2.5, 3.0, 3.5, 5.0, 5.0 },
		{ 2.0, 2.0, 2.0, 3.0, 3.0, 3.0, 3.0, 4.0, 5.0, 5.0 },
		/* Gray mode MCLK settings */
		{ 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0,  6.0,  6.0,  6.0 },
		{ 6.5, 6.5, 6.5, 7.0, 8.0, 8.0, 8.0, 10.0, 10.0, 10.0 }
	},

	{ MODEL_CANON_LIDE25, 8, 31, 3, 0, 0,
	/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi */
		{{ 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 },
		/* <=400dpi     <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		{ 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }, { 8, 31, 1 }},
		/* Color mode MCLK settings */
		{ 3.0, 3.0, 3.0, 3.0, 3.5, 3.0, 3.0, 3.0, 3.0, 3.0 },
		{ 3.0, 3.0, 3.0, 3.0, 3.5, 3.5, 5.0, 6.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 8.5, 7.0, 5.0, 4.0, 2.5, 2.5, 2.5,  2.5,  2.5,  2.5 },
		{ 7.5, 6.5, 6.0, 6.0, 6.0, 6.0, 8.0, 12.0, 12.0, 12.0 }
	},

	{ MODEL_TSCAN, 2, 22, 6, 75, 4000,
	/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi */
		{{ 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 },
		/* <=400dpi     <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		{ 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }},
		/* Color mode MCLK settings */
		{ 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		{ 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0 },
		{ 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0 },
	},

	{ MODEL_TSCAN_A4, 2, 22, 6, 75, 8400,
	/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi */
		{{ 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 },
		/* <=400dpi     <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		{ 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }, { 2, 22, 1 }},
		/* Color mode MCLK settings */
		{ 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		{ 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0 },
		{ 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0 },
	},

	{ MODEL_QSCAN, 8, 21, 6, 300, 4600,
	/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi */
		{{ 8, 21, 1 }, { 8, 21, 1 }, { 8, 21, 1 }, { 8, 21, 1 }, { 8, 21, 1 },
		/* <=400dpi     <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		{ 8, 21, 1 }, { 8, 21, 1 }, { 8, 21, 1 }, { 8, 21, 1 }, { 8, 21, 1 }},
		/* Color mode MCLK settings */
		{ 6.5, 6.5, 6.5, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		{ 6.5, 6.5, 6.5, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		/* Gray mode MCLK settings */
		{ 6.5, 6.5, 6.5, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
		{ 6.5, 6.5, 6.5, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0 },
	},

	{ MODEL_QSCAN_A6, 8, 15, 6, 0, 0,
	/* Motor settings (PWM and PWM_Duty) */
		/* <=75dpi       <=100dpi      <=150dpi      <=200dpi      <=300dpi */
		{{ 8, 15, 1}, { 8, 15, 1 }, { 8, 15, 1 }, { 8, 15, 1 }, { 8, 15, 1 },
		/* <=400dpi     <=600dpi      <=800dpi      <=1200dpi     <=2400dpi */
		{ 8, 15, 1 }, { 8, 15, 1 }, { 8, 15, 1 }, { 8, 15, 1 }, { 8, 15, 1 }},
		/* Color mode MCLK settings */
		{ 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5 },
		{ 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5 },
		/* Gray mode MCLK settings */
		{ 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0 },
		{ 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0, 15.0 },
	}
};

/* END PLUSTEK-USBDEVS.C ....................................................*/
