/* sane - Scanner Access Now Easy.

   Copyright (C) 2012-2013 Stéphane Voltz <stef.dev@free.fr>

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
#define REG0D_SCSYNC    0x40
#define REG0D_CLRERR    0x20
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
#define REG40_DOCSNR    0x80
#define REG40_ADFSNR    0x40
#define REG40_COVERSNR  0x20
#define REG40_CHKVER    0x10
#define REG40_DOCJAM    0x08
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

#define REG87_ACYCNRLC  0x10
#define REG87_ENOFFSET  0x08
#define REG87_LEDADD    0x04
#define REG87_CK4ADC    0x02
#define REG87_AUTOCONF  0x01

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

#define REGF8           0xf8
#define REGF8_MAXSEL    0xf0
#define REGF8_SMAXSEL   4
#define REGF8_MINSEL    0x0f

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
  reg_0x70,
  reg_0x71,
  reg_0x72,
  reg_0x73,
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
  reg_0x7e,
  reg_0x7f,
  reg_0x80,
  reg_0x87,
  reg_0x94,
  reg_0x98,
  reg_0x99,
  reg_0x9a,
  reg_0x9b,
  reg_0x9d,
  reg_0x9e,
  reg_0xa1,
  reg_0xa2,
  reg_0xa3,
  reg_0xa4,
  reg_0xa5,
  reg_0xa6,
  reg_0xa7,
  reg_0xa8,
  reg_0xa9,
  reg_0xab,
  reg_0xbb,
  reg_0xbc,
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
  reg_0xdb,
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
  reg_0xff,
  GENESYS_GL846_MAX_REGS
};

#define SETREG(adr,val) {dev->reg[reg_##adr].address=adr;dev->reg[reg_##adr].value=val;}


/** set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl846_init_scan_regs (Genesys_Device * dev,
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
SANE_Status gl846_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg, SANE_Bool start_motor);

/* Send the stop scan command */
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl846_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg, SANE_Bool check_stop);

#ifndef UNIT_TESTING
static
#endif
SANE_Status gl846_init (Genesys_Device * dev);

/** @brief moves the slider to steps at motor base dpi
 * @param dev device to work on
 * @param steps number of steps to move
 * */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_feed (Genesys_Device * dev, unsigned int steps);

#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_stop_action (Genesys_Device * dev);

#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home);

#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_boot (Genesys_Device * dev, SANE_Bool cold);



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
    { GPO_IMG101, 0x72, 0x1f, 0xa4, 0x13, 0xa7, 0x11, 0xff, 0x19, 0x05},
    { GPO_PLUSTEK3800, 0x30, 0x01, 0x80, 0x2d, 0x80, 0x0c, 0x8f, 0x08, 0x04},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct
{
  char    *model;
  uint8_t dramsel;
  /* shading data address */
  uint8_t rd0;
  uint8_t rd1;
  uint8_t rd2;
  /* scanned data address */
  uint8_t rx[24];
} Memory_layout;

static Memory_layout layouts[]={
	/* Image formula 101 */
	{
          "canon-image-formula-101",
          0x8b,
          0x0a, 0x1b, 0x00,
          {                         /* RED ODD START / RED ODD END */
            0x00, 0xb0, 0x05, 0xe7, /* [0x00b0, 0x05e7] 1336*4000w */
                                    /* RED EVEN START / RED EVEN END */
            0x05, 0xe8, 0x0b, 0x1f, /* [0x05e8, 0x0b1f] */
                                    /* GREEN ODD START / GREEN ODD END */
            0x0b, 0x20, 0x10, 0x57, /* [0x0b20, 0x1057] */
                                    /* GREEN EVEN START / GREEN EVEN END */
            0x10, 0x58, 0x15, 0x8f, /* [0x1058, 0x158f] */
                                    /* BLUE ODD START / BLUE ODD END */
            0x15, 0x90, 0x1a, 0xc7, /* [0x1590,0x1ac7] */
                                    /* BLUE EVEN START / BLUE EVEN END */
            0x1a, 0xc8, 0x1f, 0xff  /* [0x1ac8,0x1fff] */
          }
	},
        /* OpticBook 3800 */
	{
          "plustek-opticbook-3800",
          0x2a,
          0x0a, 0x0a, 0x0a,
          { /* RED ODD START / RED ODD END */
            0x00, 0x68, 0x03, 0x00,
            /* RED EVEN START / RED EVEN END */
            0x03, 0x01, 0x05, 0x99,
            /* GREEN ODD START / GREEN ODD END */
            0x05, 0x9a, 0x08, 0x32,
            /* GREEN EVEN START / GREEN EVEN END */
            0x08, 0x33, 0x0a, 0xcb,
            /* BLUE ODD START / BLUE ODD END */
            0x0a, 0xcc, 0x0d, 0x64,
            /* BLUE EVEN START / BLUE EVEN END */
            0x0d, 0x65, 0x0f, 0xfd
          }
	},
        /* list terminating entry */
        { NULL, 0, 0, 0, 0, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} }
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

/**
 * order of the scanned pixel
 */
static size_t order_01[]={0,1};

/**
 * database of sensor profiles
 */
static Sensor_Profile sensors[]={
	{CCD_IMG101, 1200, 11000,  60, 159, 85, 5136, 255,  0,  0,  0, order_01  , 0x13},
	{CCD_PLUSTEK3800, 1200, 11000,  60, 159, 85, 5136, 255,  0,  0,  0, order_01  , 0x13},
};
/* *INDENT-ON* */

/* base motor slopes in full step unit */
/* target=((exposure * dpi) / base_dpi)>>step_type; */
static uint32_t img101_high[] = {22000, 22000, 22000, 18450, 15974, 14284, 13054, 12076, 11286, 10660, 10100, 9632, 9224, 8864, 8532, 8250, 7986, 7750, 7530, 7330, 7142, 6972, 6810, 6656, 6518, 6384, 6264, 6150, 6038, 5930, 5834, 5732, 5642, 5560, 5476, 5398, 5324, 5252, 5180, 5112, 5050, 4990, 4926, 4868, 4816, 4760, 4708, 4658, 4608, 4562, 4516, 4472, 4428, 4384, 4344, 4306, 4266, 4230, 4194, 4156, 4122, 4088, 4054, 4022, 3990, 3960, 3930, 3900, 3872, 3842, 3816, 3790, 3762, 3736, 3710, 3686, 3662, 3638, 3614, 3590, 3570, 3548, 3526, 3506, 3484, 3462, 3442, 3424, 3402, 3384, 3366, 3346, 3328, 3310, 3292, 3276, 3258, 3242, 3224, 3208, 3192, 3176, 3162, 3146, 3130, 3114, 3100, 3086, 3072, 3058, 3044, 3030, 3016, 3002, 2990, 2976, 2964, 2950, 2938, 2926, 2914, 2902, 2890, 2878, 2866, 2854, 2844, 2832, 2820, 2810, 2800, 2790, 2778, 2768, 2756, 2748, 2738, 2726, 2716, 2708, 2698, 2688, 2678, 2668, 2660, 2650, 2642, 2632, 2624, 2616, 2606, 2598, 2588, 2580, 2572, 2564, 2556, 2548, 2540, 2530, 2522, 2516, 2508, 2500, 2492, 2486, 2476, 2470, 2462, 2454, 2448, 2440, 2434, 2426, 2420, 2412, 2406, 2400, 2392, 2386, 2378, 2372, 2366, 2360, 2354, 2346, 2340, 2334, 2328, 2322, 2314, 2310, 2304, 2296, 2292, 2286, 2280, 2274, 2268, 2262, 2256, 2252, 2246, 2240, 2234, 2230, 2224, 2218, 2214, 2208, 2202, 2196, 2192, 2188, 2182, 2176, 2172, 2166, 2162, 2156, 2152, 2146, 2142, 2136, 2132, 2128, 2124, 2118, 2114, 2108, 2104, 2100, 2096, 2092, 2086, 2082, 2078, 2072, 2068, 2064, 2060, 2056, 2052, 2048, 2044, 2038, 2034, 2030, 2026, 2022, 2018, 2014, 2010, 2006, 2002, 1998, 1994, 1990, 1988, 1984, 1980, 1976, 1972, 1968, 1964, 1960, 1956, 1952, 1950, 1946, 1942, 1938, 1934, 1932, 1928, 1924, 1920, 1918, 1914, 1910, 1908, 1904, 1900, 1898, 1894, 1890, 1888, 1884, 1880, 1878, 1874, 1870, 1868, 1864, 1862, 1858, 1854, 1852, 1848, 1846, 1842, 1840, 1836, 1834, 1830, 1828, 1824, 1822, 1818, 1816, 1812, 1810, 1806, 1804, 1800, 1798, 1794, 1792, 1790, 1786, 1784, 1780, 1778, 1776, 1772, 1770, 1768, 1764, 1762, 1758, 1756, 1754, 1752, 1748, 1746, 1744, 1740, 1738, 1736, 1734, 1730, 1728, 1726, 1724, 1720, 1718, 1716, 1714, 1710, 1708, 1706, 1704, 1700, 1698, 1696, 1694, 1692, 1688, 1686, 1684, 1682, 1680, 1676, 1674, 1672, 1670, 1668, 1666, 1664, 1662, 1660, 1656, 1654, 1652, 1650, 1648, 1646, 1644, 1642, 1638, 1636, 1634, 1632, 1630, 1628, 1626, 1624, 1622, 1620, 1618, 1616, 1614, 1612, 1610, 1608, 1606, 1604, 1602, 1600, 1598, 1596, 1594, 1592, 1590, 1588, 1586, 1584, 1582, 1580, 1578, 1576, 1574, 1572, 1570, 1568, 1566, 1564, 1562, 1560, 1558, 1556, 1556, 1554, 1552, 1550, 1548, 1546, 1544, 1542, 1540, 1538, 1536, 1536, 1534, 1532, 1530, 1528, 1526, 1524, 1522, 1522, 1520, 1518, 1516, 1514, 1512, 1510, 1510, 1508, 1506, 1504, 1502, 1500, 1500, 1498, 1496, 1494, 1492, 1490, 1490, 1488, 1486, 1484, 1482, 1482, 1480, 1478, 1476, 1474, 1474, 1472, 1470, 1468, 1466, 1466, 1464, 1462, 1460, 1460, 1458, 1456, 1454, 1454, 1452, 1450, 1448, 1448, 1446, 1444, 1442, 1442, 1440, 1438, 1438, 1436, 1434, 1432, 1432, 1430, 1428, 1426, 1426, 1424, 1422, 1422, 1420, 1418, 1418, 1416, 1414, 1412, 1412, 1410, 1408, 1408, 1406, 1404, 1404, 1402, 1400, 1400, 1398, 1396, 1394, 1394, 1392, 1390, 1390, 1388, 1388, 1386, 1384, 1384, 1382, 1380, 1380, 1378, 1376, 1376, 1374, 1374, 1372, 1370, 1370, 1368, 1366, 1366, 1364, 1362, 1362, 1360, 1360, 1358, 1356, 1356, 1354, 1352, 1352, 1350, 1350, 1348, 1348, 1346, 1344, 1344, 1342, 1340, 1340, 1338, 1338, 1336, 1336, 1334, 1332, 1332, 1330, 1330, 1328, 1328, 1326, 1324, 1324, 1322, 1322, 1320, 1318, 1318, 1316, 1316, 1314, 1314, 1312, 1310, 1310, 1308, 1308, 1306, 1306, 1304, 1304, 1302, 1302, 1300, 1300, 1298, 1298, 1296, 1294, 1294, 1292, 1292, 1290, 1290, 1288, 1288, 1286, 1286, 1284, 1284, 1282, 1282, 1280, 1280, 1278, 1278, 1276, 1276, 1274, 1272, 1272, 1270, 1270, 1270, 1268, 1268, 1266, 1264, 1264, 1262, 1262, 1260, 1260, 1260, 1258, 1258, 1256, 1254, 1254, 1254, 1252, 1252, 1250, 1250, 1248, 1248, 1246, 1246, 1244, 1244, 1242, 1242, 1240, 1240, 1238, 1238, 1236, 1236, 1236, 1234, 1234, 1232, 1232, 1230, 1230, 1228, 1228, 1226, 1226, 1226, 1224, 1224, 1222, 1222, 1220, 1220, 1218, 1218, 1218, 1216, 1216, 1214, 1214, 1212, 1212, 1210, 1210, 1210, 1208, 1208, 1206, 1206, 1204, 1204, 1204, 1202, 1202, 1200, 1200, 1198, 1198, 1198, 1196, 1196, 1194, 1194, 1192, 1192, 1192, 1190, 1190, 1188, 1188, 1188, 1186, 1186, 1184, 1184, 1182, 1182, 1182, 1180, 1180, 1180, 1178, 1178, 1176, 1176, 1174, 1174, 1174, 1172, 1172, 1172, 1170, 1170, 1168, 1168, 1168, 1166, 1166, 1164, 1164, 1164, 1162, 1162, 1160, 1160, 1160, 1158, 1158, 1156, 1156, 1156, 1154, 1154, 1154, 1152, 1152, 1152, 1150, 1150, 1148, 1148, 1148, 1146, 1146, 1146, 1144, 1144, 1142, 1142, 1142, 1140, 1140, 1140, 1138, 1138, 1136, 1136, 1136, 1134, 1134, 1134, 1132, 1132, 1132, 1130, 1130, 1130, 1128, 1128, 1126, 1126, 1126, 1124, 1124, 1124, 1122, 1122, 1122, 1120, 1120, 1120, 1118, 1118, 1118, 1116, 1116, 1116, 1114, 1114, 1114, 1112, 1112, 1112, 1110, 1110, 1110, 1108, 1108, 1108, 1106, 1106, 1106, 1104, 1104, 1104, 1102, 1102, 1102, 1100, 1100, 1100, 1098, 1098, 1098, 1096, 1096, 1096, 1094, 1094, 1094, 1092, 1092, 1092, 1090, 1090, 1090, 1088, 1088, 1088, 1086, 1086, 1086, 1086, 1084, 1084, 1084, 1082, 1082, 1082, 1080, 1080, 1080, 1078, 1078, 1078, 1078, 1076, 1076, 1076, 1074, 1074, 1074, 1072, 1072, 1072, 1072, 1070, 1070, 1070, 1068, 1068, 1068, 1066, 1066, 1066, 1064, 1064, 1064, 1064, 1062, 1062, 1062, 1060, 1060, 1060, 1058, 1058, 1058, 1058, 1056, 1056, 1056, 1054, 1054, 1054, 1054, 1052, 1052, 1052, 1050, 1050, 1050, 1050, 1048, 1048, 1048, 1046, 1046, 1046, 1046, 1044, 1044, 1044, 1044, 1042, 1042, 1042, 1040, 1040, 1040, 1040, 1038, 1038, 1038, 1036, 1036, 1036, 1036, 1034, 1034, 1034, 1034, 1032, 1032, 1032, 1030, 1030, 1030, 1030, 1028, 1028, 1028, 1028, 1026, 1026, 1026, 1026, 1024, 1024, 1024, 1022, 1022, 1022, 1022, 1020, 1020, 1020, 1020, 1018, 1018, 1018, 1018, 1016, 1016, 1016, 1016, 1014, 1014, 1014, 1014, 1012, 1012, 1012, 1012, 1010, 1010, 1010, 1010, 1008, 1008, 1008, 1008, 1006, 1006, 1006, 1006, 1004, 1004, 1004, 1004, 1002, 1002, 1002, 1002, 1000, 1000, 1000, 1000, 0 };

/**
 * database of motor profiles
 */

/* *INDENT-OFF* */
static Motor_Profile gl846_motors[]={
        /* Image Formula 101 */
	{MOTOR_IMG101,  11000, HALF_STEP , img101_high},
	{MOTOR_PLUSTEK3800,  11000, HALF_STEP , img101_high},

	/* end of database entry */
	{0,  0, 0, NULL},
};
/* *INDENT-ON* */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
