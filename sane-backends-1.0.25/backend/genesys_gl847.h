/* sane - Scanner Access Now Easy.

   Copyright (C) 2010-2013 Stéphane Voltz <stef.dev@free.fr>

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


#ifdef UNIT_TESTING
SANE_Status gl847_stop_action (Genesys_Device * dev);
SANE_Status gl847_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home);
#endif

#define REG01           0x01
#define REG01_CISSET	0x80
#define REG01_DOGENB	0x40
#define REG01_DVDSET	0x20
#define REG01_STAGGER   0x10
#define REG01_COMPENB	0x08
#define REG01_TRUEGRAY  0x04
#define REG01_SHDAREA	0x02
#define REG01_SCAN	0x01

#define REG02        	0x02
#define REG02_NOTHOME	0x80
#define REG02_ACDCDIS	0x40
#define REG02_AGOHOME	0x20
#define REG02_MTRPWR	0x10
#define REG02_FASTFED	0x08
#define REG02_MTRREV	0x04
#define REG02_HOMENEG	0x02
#define REG02_LONGCURV	0x01

#define REG03           0x03
#define REG03_LAMPDOG	0x80
#define REG03_AVEENB	0x40
#define REG03_XPASEL	0x20
#define REG03_LAMPPWR	0x10
#define REG03_LAMPTIM	0x0f

#define REG04        	0x04
#define REG04_LINEART	0x80
#define REG04_BITSET	0x40
#define REG04_AFEMOD	0x30
#define REG04_FILTER	0x0c
#define REG04_FESET	0x03

#define REG04S_AFEMOD   4

#define REG05 		0x05
#define REG05_DPIHW	0xc0
#define REG05_DPIHW_600	0x00
#define REG05_DPIHW_1200	0x40
#define REG05_DPIHW_2400	0x80
#define REG05_DPIHW_4800	0xc0
#define REG05_MTLLAMP	0x30
#define REG05_GMMENB	0x08
#define REG05_MTLBASE	0x03

#define REG06_SCANMOD	0xe0
#define REG06S_SCANMOD	5
#define REG06_PWRBIT	0x10
#define REG06_GAIN4	0x08
#define REG06_OPTEST	0x07

#define	REG07_LAMPSIM	0x80

#define REG08_DRAM2X  	0x80
#define REG08_MPENB     0x20
#define REG08_CIS_LINE  0x10
#define REG08_IR1ENB	0x08
#define REG08_IR2ENB    0x04
#define REG08_ENB24M    0x01

#define REG09_MCNTSET	0xc0
#define REG09_EVEN1ST   0x20
#define REG09_BLINE1ST  0x10
#define REG09_BACKSCAN	0x08
#define REG09_ENHANCE	0x04
#define REG09_SHORTTG	0x02
#define REG09_NWAIT	0x01

#define REG09S_MCNTSET  6
#define REG09S_CLKSET   4


#define REG0A_LPWMEN	0x10

#define REG0B           0x0b
#define REG0B_DRAMSEL   0x07
#define REG0B_ENBDRAM   0x08
#define REG0B_ENBDRAM   0x08
#define REG0B_RFHDIS    0x10
#define REG0B_CLKSET    0xe0
#define REG0B_24MHZ     0x00
#define REG0B_30MHZ     0x20
#define REG0B_40MHZ     0x40
#define REG0B_48MHZ     0x60
#define REG0B_60MHZ     0x80

#define REG0C           0x0c
#define REG0C_CCDLMT    0x0f

#define REG0D 		0x0d
#define REG0D_FULLSTP   0x10
#define REG0D_SEND      0x80
#define REG0D_CLRMCNT   0x04
#define REG0D_CLRDOCJM  0x02
#define REG0D_CLRLNCNT	0x01

#define REG0F 		0x0f

#define REG16_CTRLHI	0x80
#define REG16_TOSHIBA	0x40
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
#define REG17S_TGW      0

#define REG18      	0x18
#define REG18_CNSET	0x80
#define REG18_DCKSEL	0x60
#define REG18_CKTOGGLE	0x10
#define REG18_CKDELAY	0x0c
#define REG18_CKSEL	0x03

#define REG1A_SW2SET 	0x80
#define REG1A_SW1SET 	0x40
#define REG1A_MANUAL3	0x02
#define REG1A_MANUAL1	0x01
#define REG1A_CK4INV	0x08
#define REG1A_CK3INV	0x04
#define REG1A_LINECLP	0x02

#define REG1C           0x1c
#define REG1C_TGTIME    0x07

#define REG1D_CK4LOW	0x80
#define REG1D_CK3LOW	0x40
#define REG1D_CK1LOW	0x20
#define REG1D_TGSHLD	0x1f
#define REG1DS_TGSHLD   0


#define REG1E_WDTIME	0xf0
#define REG1ES_WDTIME   4
#define REG1E_LINESEL	0x0f
#define REG1ES_LINESEL  0

#define REG_FEDCNT      0x1f

#define REG24           0x1c
#define REG40           0x40
#define REG40_CHKVER    0x10
#define REG40_HISPDFLG  0x04
#define REG40_MOTMFLG   0x02
#define REG40_DATAENB   0x01

#define REG41_PWRBIT	0x80
#define REG41_BUFEMPTY	0x40
#define REG41_FEEDFSH	0x20
#define REG41_SCANFSH	0x10
#define REG41_HOMESNR	0x08
#define REG41_LAMPSTS	0x04
#define REG41_FEBUSY	0x02
#define REG41_MOTORENB	0x01

#define REG58_VSMP      0xf8
#define REG58S_VSMP     3
#define REG58_VSMPW     0x07
#define REG58S_VSMPW    0

#define REG59_BSMP      0xf8
#define REG59S_BSMP     3
#define REG59_BSMPW     0x07
#define REG59S_BSMPW    0

#define REG5A_ADCLKINV  0x80
#define REG5A_RLCSEL    0x40
#define REG5A_CDSREF    0x30
#define REG5AS_CDSREF   4
#define REG5A_RLC       0x0f
#define REG5AS_RLC      0

#define REG5E_DECSEL    0xe0
#define REG5ES_DECSEL   5
#define REG5E_STOPTIM   0x1f
#define REG5ES_STOPTIM  0

#define REG60           0x60
#define REG60_Z1MOD	0x1f
#define REG61           0x61
#define REG61_Z1MOD	0xff
#define REG62           0x62
#define REG62_Z1MOD	0xff

#define REG63           0x63
#define REG63_Z2MOD	0x1f
#define REG64           0x64
#define REG64_Z2MOD	0xff
#define REG65           0x65
#define REG65_Z2MOD	0xff

#define REG60S_STEPSEL      5
#define REG60_STEPSEL	 0xe0
#define REG60_FULLSTEP	 0x00
#define REG60_HALFSTEP	 0x20
#define REG60_EIGHTHSTEP 0x60
#define REG60_16THSTEP   0x80

#define REG63S_FSTPSEL      5
#define REG63_FSTPSEL	 0xe0
#define REG63_FULLSTEP	 0x00
#define REG63_HALFSTEP	 0x20
#define REG63_EIGHTHSTEP 0x60
#define REG63_16THSTEP   0x80

#define REG67 		0x67
#define REG67_MTRPWM	0x80

#define REG68 		0x68
#define REG68_FASTPWM	0x80

#define REG6B          	0x6b
#define REG6B_MULTFILM	0x80
#define REG6B_GPOM13	0x40
#define REG6B_GPOM12	0x20
#define REG6B_GPOM11	0x10
#define REG6B_GPO18	0x02
#define REG6B_GPO17	0x01

#define REG6C           0x6c
#define REG6C_GPIO16    0x80
#define REG6C_GPIO15    0x40
#define REG6C_GPIO14    0x20
#define REG6C_GPIO13    0x10
#define REG6C_GPIO12    0x08
#define REG6C_GPIO11    0x04
#define REG6C_GPIO10    0x02
#define REG6C_GPIO9     0x01
#define REG6C_GPIOH	0xff
#define REG6C_GPIOL	0xff

#define REG6D           0x6d
#define REG6E           0x6e
#define REG6F           0x6f
#define REG7E           0x7e

#define REG87_LEDADD    0x04

#define REG9E 		0x9e
#define REG9F 		0x9f

#define REGA6   	0xa6
#define REGA7 		0xa7
#define REGA8 		0xa8
#define REGA9 		0xa9
#define REGAB 		0xab

#define REG_EXPR        0x10
#define REG_EXPG        0x12
#define REG_EXPB        0x14
#define REG_EXPDMY      0x19
#define REG_STEPNO      0x21
#define REG_FWDSTEP     0x22
#define REG_BWDSTEP     0x23
#define REG_FASTNO      0x24
#define REG_DPISET      0x2c
#define REG_STRPIXEL    0x30
#define REG_ENDPIXEL    0x32
#define REG_LINCNT      0x25
#define REG_MAXWD       0x35
#define REG_LPERIOD     0x38
#define REG_FEEDL       0x3d
#define REG_FMOVDEC     0x5f
#define REG_FSHDEC      0x69
#define REG_FMOVNO      0x6a
#define REG_CK1MAP      0x74
#define REG_CK3MAP      0x77
#define REG_CK4MAP      0x7a

/**
 * writable scanner registers */
enum
{
  reg_0x01 = 0,
  reg_0x02,
  reg_0x03,
  reg_0x04,
  reg_0x05,
  reg_0x06,
  reg_0x08,
  reg_0x09,
  reg_0x0a,
  reg_0x0b,
  reg_0x0c,
  reg_0x0d,
  reg_0x0e,
  reg_0x0f,
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
  reg_0x3a,
  reg_0x3b,
  reg_0x3d,
  reg_0x3e,
  reg_0x3f,
  reg_0x51,
  reg_0x52,
  reg_0x53,
  reg_0x54,
  reg_0x55,
  reg_0x56,
  reg_0x57,
  reg_0x58,
  reg_0x59,
  reg_0x5a,
  reg_0x5e,
  reg_0x5f,
  reg_0x60,
  reg_0x61,
  reg_0x62,
  reg_0x63,
  reg_0x64,
  reg_0x65,
  reg_0x67,
  reg_0x68,
  reg_0x69,
  reg_0x6a,
  reg_0x6b,
  reg_0x6c,
  reg_0x6d,
  reg_0x6e,
  reg_0x6f,
  reg_0x74,
  reg_0x75,
  reg_0x76,
  reg_0x77,
  reg_0x78,
  reg_0x79,
  reg_0x7a,
  reg_0x7b,
  reg_0x7c,
  reg_0x7d,
  reg_0x87,
  reg_0x9d,
  reg_0xa2,
  reg_0xa3,
  reg_0xa4,
  reg_0xa5,
  reg_0xa6,
  reg_0xa7,
  reg_0xa8,
  reg_0xa9,
  reg_0xbd,
  reg_0xbe,
  reg_0xc5,
  reg_0xc6,
  reg_0xc7,
  reg_0xc8,
  reg_0xc9,
  reg_0xca,
  reg_0xd0,
  reg_0xd1,
  reg_0xd2,
  reg_0xe0,
  reg_0xe1,
  reg_0xe2,
  reg_0xe3,
  reg_0xe4,
  reg_0xe5,
  reg_0xe6,
  reg_0xe7,
  reg_0xe8,
  reg_0xe9,
  reg_0xea,
  reg_0xeb,
  reg_0xec,
  reg_0xed,
  reg_0xee,
  reg_0xef,
  reg_0xf0,
  reg_0xf1,
  reg_0xf2,
  reg_0xf3,
  reg_0xf4,
  reg_0xf5,
  reg_0xf6,
  reg_0xf7,
  reg_0xf8,
  reg_0xfe,
  GENESYS_GL847_MAX_REGS
};

#define SETREG(adr,val) {dev->reg[reg_##adr].address=adr;dev->reg[reg_##adr].value=val;}

/** set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl847_init_scan_regs (Genesys_Device * dev,
                      Genesys_Register_Set * reg,
                      float xres,	/*dpi */
		      float yres,	/*dpi */
		      float startx,	/*optical_res, from dummy_pixel+1 */
		      float starty,	/*base_ydpi, from home! */
		      float pixels,
		      float lines,
		      unsigned int depth,
		      unsigned int channels,
		      int color_filter,
                      unsigned int flags);

/* Send the low-level scan command */
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl847_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg, SANE_Bool start_motor);

/* Send the stop scan command */
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl847_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg, SANE_Bool check_stop);

#ifndef UNIT_TESTING
static
#endif
SANE_Status gl847_init (Genesys_Device * dev);

/** @brief moves the slider to steps at motor base dpi
 * @param dev device to work on
 * @param steps number of steps to move
 * */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl847_feed (Genesys_Device * dev, unsigned int steps);

typedef struct
{
  uint8_t sensor_id;
  uint8_t r6b;
  uint8_t r6c;
  uint8_t r6d;
  uint8_t r6e;
  uint8_t r6f;
  uint8_t ra6;
  uint8_t ra7;
  uint8_t ra8;
  uint8_t ra9;
} Gpio_Profile;

static Gpio_Profile gpios[]={
    { GPO_CANONLIDE200, 0x02, 0xf9, 0x20, 0xff, 0x00, 0x04, 0x04, 0x00, 0x00},
    { GPO_CANONLIDE700, 0x06, 0xdb, 0xff, 0xff, 0x80, 0x15, 0x07, 0x20, 0x10},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct
{
  uint8_t dramsel;
  uint8_t rd0;
  uint8_t rd1;
  uint8_t rd2;
  uint8_t re0;
  uint8_t re1;
  uint8_t re2;
  uint8_t re3;
  uint8_t re4;
  uint8_t re5;
  uint8_t re6;
  uint8_t re7;
} Memory_layout;

static Memory_layout layouts[]={
	/* LIDE 100 */
	{
                0x29,
		0x0a, 0x15, 0x20,
		0x00, 0xac, 0x02, 0x55, 0x02, 0x56, 0x03, 0xff
	},
	/* LIDE 200 */
	{
                0x29,
		0x0a, 0x1f, 0x34,
		0x01, 0x24, 0x02, 0x91, 0x02, 0x92, 0x03, 0xff
	},
	/* 5600F */
	{
                0x29,
		0x0a, 0x1f, 0x34,
		0x01, 0x24, 0x02, 0x91, 0x02, 0x92, 0x03, 0xff
	},
	/* LIDE 700F */
	{
                0x2a,
		0x0a, 0x33, 0x5c,
		0x02, 0x14, 0x09, 0x09, 0x09, 0x0a, 0x0f, 0xff
	}
};

/** @brief structure for sensor settings
 * this structure describes the sensor settings to use for a given
 * exposure.
 */
typedef struct {
  int sensor_type;      /**> sensor id */
  int dpi;              /**> maximum dpi for which data are valid */
  int exposure;         /**> exposure */
  int ck1map;           /**> CK1MAP */
  int ck3map;           /**> CK3MAP */
  int ck4map;           /**> CK4MAP */
  int segcnt;           /**> SEGCNT */
  int expdummy;         /**> exposure dummy */
  int expr;             /**> initial red exposure */
  int expg;             /**> initial green exposure */
  int expb;             /**> initial blue exposure */
  size_t *order;        /**> order of sub-segments */
  uint8_t r17;		/**> TG width */
} Sensor_Profile;

/* *INDENT-OFF* */

static size_t order_01[]={0,1};
static size_t order_0213[]={0,2,1,3};
static size_t order_0246[]={0,2,4,6,1,3,5,7};

static size_t new_order[]={0,1,2,3};
static size_t order_0145[]={0,1,4,5,2,3,6,7};

/**
 * database of sensor profiles
 */
static Sensor_Profile sensors[]={
	{CIS_CANONLIDE100,  200,  2848,  60, 159, 85, 5136, 255,  410,  275,  203, NULL      , 0x0a},
	{CIS_CANONLIDE100,  300,  1424,  60, 159, 85, 5136, 255,  410,  275,  203, NULL      , 0x0a},
	{CIS_CANONLIDE100,  600,  1432,  60, 159, 85, 5136, 255,  410,  275,  203, NULL      , 0x0a},
	{CIS_CANONLIDE100, 1200,  2712,  60, 159, 85, 5136, 255,  746,  478,  353, order_01  , 0x08},
	{CIS_CANONLIDE100, 2400,  5280,  60, 159, 85, 5136, 255, 1417,  909,  643, order_0213, 0x06},
	/*
	{CIS_CANONLIDE200,  150,  2848, 240, 636, 340, 5144, 0, 255,  637,  637,  637},
	{CIS_CANONLIDE200,  300,  1424, 240, 636, 340, 5144, 0, 255,  637,  637,  637},
	*/
	{CIS_CANONLIDE200,  200,  2848,  60, 159, 85, 5136, 255,  410,  275,  203, NULL      , 0x0a},
	{CIS_CANONLIDE200,  300,  1424,  60, 159, 85, 5136, 255,  410,  275,  203, NULL      , 0x0a},
	{CIS_CANONLIDE200,  600,  1432,  60, 159, 85, 5136, 255,  410,  275,  203, NULL      , 0x0a},
	{CIS_CANONLIDE200, 1200,  2712,  60, 159, 85, 5136, 255,  746,  478,  353, order_01  , 0x08},
	{CIS_CANONLIDE200, 2400,  5280,  60, 159, 85, 5136, 255, 1417,  909,  643, order_0213, 0x06},
	{CIS_CANONLIDE200, 4800, 10416,  60, 159, 85, 5136, 255, 2692, 1728, 1221, order_0246, 0x04},

        /* LiDE 700F */
	{CIS_CANONLIDE700,  150,  2848, 135, 249, 85, 5187, 255,  465,  310,  239, NULL      , 0x0c},
	{CIS_CANONLIDE700,  300,  1424, 135, 249, 85, 5187, 255,  465,  310,  239, NULL      , 0x0c},
	{CIS_CANONLIDE700,  600,  1504, 135, 249, 85, 5187, 255,  465,  310,  239, NULL      , 0x0c},
	{CIS_CANONLIDE700, 1200,  2696, 135, 249, 85, 5187, 255, 1464,  844,  555, order_01  , 0x0a},
	{CIS_CANONLIDE700, 2400, 10576, 135, 249, 85, 5187, 255, 2798, 1558,  972, new_order , 0x08},
	{CIS_CANONLIDE700, 4800, 10576, 135, 249, 85, 5187, 255, 2798, 1558,  972, order_0145, 0x06},
};
/* *INDENT-ON* */

/* base motor sopes in full step unit */
/* target=((exposure * dpi) / base_dpi)>>step_type; */
static uint32_t lide200_base[] = { 46876, 2343, 2343, 2343, 2343, 2343, 2343, 2343, 2343, 2336, 2329, 2322, 2314, 2307, 2300,2292,2285,2278,2271,2263,2256,2249,2241,2234,2227,2219,2212,2205,2198,2190,2183,2176,2168,2161,2154,2146,2139,2132,2125,2117,2110,2103,2095,2088,2081,2073,2066,2059,2052,2044,2037,2030,2022,2015,2008,2001,1993,1986,1979,1971,1964,1957,1949,1942,1935,1928,1920,1913,1906,1898,1891,1884,1876,1869,1862,1855,1847,1840,1833,1825,1818,1811,1803,1796,1789,1782,1774,1767,1760,1752,1745,1738,1731,1723,1716,1709,1701,1694,1687,1679,1672,1665,1658,1650,1643,1636,1628,1621,1614,1606,1599,1592,1585,1577,1570,1563,1555,1548,1541,1533,1526,1519,1512,1504,1497,1490,1482,1475,1468,1461,1453,1446,1439,1431,1424,1417,1409,1402,1395,1388,1380,1373,1366,1358,1351,1344,1336,1329,1322,1315,1307,1300,1293,1285,1278,1271,1263,1256,1249,1242,1234,1227,1220,1212,1205,1198,1191,1183,1176,1169,1161,1154,1147,1139,1132,1125,1118,1110,1103,1096,1088,1081,1074,1066,1059,1052,1045,1037,1030,1023,1015,1008,1001,993,986,979,972,964,957,950,942,935,928,921,913,906,899,891,884,877,869,862,855,848,840,833,826,818,811,804,796,789,782,775,767,760,753,745,738,731,723,716,709,702,694,687,680,672,665,658,651,643,636,629,621,614,607,599,592,585,578,570,563,556,534,534, 0};
static uint32_t lide200_medium[] = { 46876, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136, 8136,2343, 2336, 2329, 2322, 2314, 2307, 2300,2292,2285,2278,2271,2263,2256,2249,2241,2234,2227,2219,2212,2205,2198,2190,2183,2176,2168,2161,2154,2146,2139,2132,2125,2117,2110,2103,2095,2088,2081,2073,2066,2059,2052,2044,2037,2030,2022,2015,2008,2001,1993,1986,1979,1971,1964,1957,1949,1942,1935,1928,1920,1913,1906,1898,1891,1884,1876,1869,1862,1855,1847,1840,1833,1825,1818,1811,1803,1796,1789,1782,1774,1767,1760,1752,1745,1738,1731,1723,1716,1709,1701,1694,1687,1679,1672,1665,1658,1650,1643,1636,1628,1621,1614,1606,1599,1592,1585,1577,1570,1563,1555,1548,1541,1533,1526,1519,1512,1504,1497,1490,1482,1475,1468,1461,1453,1446,1439,1431,1424,1417,1409,1402,1395,1388,1380,1373,1366,1358,1351,1344,1336,1329,1322,1315,1307,1300,1293,1285,1278,1271,1263,1256,1249,1242,1234,1227,1220,1212,1205,1198,1191,1183,1176,1169,1161,1154,1147,1139,1132,1125,1118,1110,1103,1096,1088,1081,1074,1066,1059,1052,1045,1037,1030,1023,1015,1008,1001,993,986,979,972,964,957,950,942,935,928,921,913,906,899,891,884,877,869,862,855,848,840,833,826,818,811,804,796,789,782,775,767,760,753,745,738,731,723,716,709,702,694,687,680,672,665,658,651,643,636,629,621,614,607,599,592,585,578,570,563,556,534,534, 0};
static uint32_t lide200_high[] = { 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 31680, 2219,2212,2205,2198,2190,2183,2176,2168,2161,2154,2146,2139,2132,2125,2117,2110,2103,2095,2088,2081,2073,2066,2059,2052,2044,2037,2030,2022,2015,2008,2001,1993,1986,1979,1971,1964,1957,1949,1942,1935,1928,1920,1913,1906,1898,1891,1884,1876,1869,1862,1855,1847,1840,1833,1825,1818,1811,1803,1796,1789,1782,1774,1767,1760,1752,1745,1738,1731,1723,1716,1709,1701,1694,1687,1679,1672,1665,1658,1650,1643,1636,1628,1621,1614,1606,1599,1592,1585,1577,1570,1563,1555,1548,1541,1533,1526,1519,1512,1504,1497,1490,1482,1475,1468,1461,1453,1446,1439,1431,1424,1417,1409,1402,1395,1388,1380,1373,1366,1358,1351,1344,1336,1329,1322,1315,1307,1300,1293,1285,1278,1271,1263,1256,1249,1242,1234,1227,1220,1212,1205,1198,1191,1183,1176,1169,1161,1154,1147,1139,1132,1125,1118,1110,1103,1096,1088,1081,1074,1066,1059,1052,1045,1037,1030,1023,1015,1008,1001,993,986,979,972,964,957,950,942,935,928,921,913,906,899,891,884,877,869,862,855,848,840,833,826,818,811,804,796,789,782,775,767,760,753,745,738,731,723,716,709,702,694,687,680,672,665,658,651,643,636,629,621,614,607,599,592,585,578,570,563,556,534,534, 0};
static uint32_t lide700_medium[] = { 46876,2342,2342,2342,2342,2342,2342,2342,2342,2302,2286,2274,2266,2258,2252,2244,2240,2234,2228,2224,2218,2216,2210,2208,2202,2200,2194,2192,2190,2186,2182,2180,2176,2174,2172,2170,2166,2162,2160,2156,2154,2152,2150,2150,2146,2144,2142,2140,2136,2134,2132,2130,2130,2128,2124,2122,2120,2120,2118,2116,2112,2112,2110,2108,2106,2106,2104,2102,2102,2098,2096,2094,2094,2092,2090,2090,2086,2084,2084,2082,2082,2080,2078,2078,2076,2074,2074,2070,2070,2068,2066,2066,2064,2064,2062,2062,2060,2058,2058,2054,2054,2052,2052,2050,2050,2048,2048,2046,2046,2044,2042,2042,2040,2040,2038,2038,2034,2034,2032,2032,2030,2030,2028,2028,2026,2026,2022,2022};
static uint32_t lide700_high[] = { 46876,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864,15864};
/* 5190 trop
 * 5186 pas assez
 */
/*
static uint32_t lide200_max[] = { 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 124992, 2219,2212,2205,2198,2190,2183,2176,2168,2161,2154,2146,2139,2132,2125,2117,2110,2103,2095,2088,2081,2073,2066,2059,2052,2044,2037,2030,2022,2015,2008,2001,1993,1986,1979,1971,1964,1957,1949,1942,1935,1928,1920,1913,1906,1898,1891,1884,1876,1869,1862,1855,1847,1840,1833,1825,1818,1811,1803,1796,1789,1782,1774,1767,1760,1752,1745,1738,1731,1723,1716,1709,1701,1694,1687,1679,1672,1665,1658,1650,1643,1636,1628,1621,1614,1606,1599,1592,1585,1577,1570,1563,1555,1548,1541,1533,1526,1519,1512,1504,1497,1490,1482,1475,1468,1461,1453,1446,1439,1431,1424,1417,1409,1402,1395,1388,1380,1373,1366,1358,1351,1344,1336,1329,1322,1315,1307,1300,1293,1285,1278,1271,1263,1256,1249,1242,1234,1227,1220,1212,1205,1198,1191,1183,1176,1169,1161,1154,1147,1139,1132,1125,1118,1110,1103,1096,1088,1081,1074,1066,1059,1052,1045,1037,1030,1023,1015,1008,1001,993,986,979,972,964,957,950,942,935,928,921,913,906,899,891,884,877,869,862,855,848,840,833,826,818,811,804,796,789,782,775,767,760,753,745,738,731,723,716,709,702,694,687,680,672,665,658,651,643,636,629,621,614,607,599,592,585,578,570,563,556,534,534, 0};
*/

/**
 * database of motor profiles
 */

/* *INDENT-OFF* */
static Motor_Profile gl847_motors[]={
        /* LiDE 100 */
	{MOTOR_CANONLIDE100,  2848, HALF_STEP   , lide200_base},
	{MOTOR_CANONLIDE100,  1424, HALF_STEP   , lide200_base},
	{MOTOR_CANONLIDE100,  1432, HALF_STEP   , lide200_base},
	{MOTOR_CANONLIDE100,  2712, QUARTER_STEP, lide200_medium},
	{MOTOR_CANONLIDE100,  5280, EIGHTH_STEP , lide200_high},

        /* LiDE 200 */
	{MOTOR_CANONLIDE200,  2848, HALF_STEP   , lide200_base},
	{MOTOR_CANONLIDE200,  1424, HALF_STEP   , lide200_base},
	{MOTOR_CANONLIDE200,  1432, HALF_STEP   , lide200_base},
	{MOTOR_CANONLIDE200,  2712, QUARTER_STEP, lide200_medium},
	{MOTOR_CANONLIDE200,  5280, EIGHTH_STEP , lide200_high},
	{MOTOR_CANONLIDE200, 10416, EIGHTH_STEP , lide200_high},

        /* LiDE 700F */
	{MOTOR_CANONLIDE700,  2848, HALF_STEP  , lide200_base},
	{MOTOR_CANONLIDE700,  1424, HALF_STEP  , lide200_base},
	{MOTOR_CANONLIDE700,  1504, HALF_STEP  , lide200_base},
	{MOTOR_CANONLIDE700,  2696, HALF_STEP  , lide700_medium}, /* 2696 , 2838 */
	{MOTOR_CANONLIDE700, 10576, EIGHTH_STEP, lide700_high},

	/* end of database entry */
	{0,  0, 0, NULL},
};
/* *INDENT-ON* */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
