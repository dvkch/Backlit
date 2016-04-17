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
#define REG04_FILTER	0x30
#define REG04_AFEMOD    0x07

#define REG05 		0x05
#define REG05_DPIHW	0xc0
#define REG05_DPIHW_600	0x00
#define REG05_DPIHW_1200	0x40
#define REG05_DPIHW_2400	0x80
#define REG05_DPIHW_4800	0xc0
#define REG05_MTLLAMP	0x30
#define REG05_GMMENB	0x08
#define REG05_ENB20M	0x04
#define REG05_MTLBASE	0x03

#define REG06 		0x06
#define REG06_SCANMOD	0xe0
#define REG06S_SCANMOD	5
#define REG06_PWRBIT	0x10
#define REG06_GAIN4	0x08
#define REG06_OPTEST	0x07

#define	REG07_LAMPSIM	0x80

#define REG08_DRAM2X 	0x80
#define REG08_MPENB     0x20
#define REG08_CIS_LINE  0x10
#define REG08_IR2_ENB   0x08
#define REG08_IR1_ENB   0x04
#define REG08_ENB24M    0x01

#define REG09_MCNTSET	0xc0
#define REG09_EVEN1ST   0x20
#define REG09_BLINE1ST  0x10
#define REG09_BACKSCAN	0x08
#define REG09_OUTINV 	0x04
#define REG09_SHORTTG	0x02

#define REG09S_MCNTSET  6
#define REG09S_CLKSET   4

#define REG0A           0x0a
#define REG0A_SIFSEL    0xc0
#define REG0AS_SIFSEL   6
#define REG0A_SHEETFED  0x20
#define REG0A_LPWMEN    0x10

#define REG0B           0x0b
#define REG0B_DRAMSEL   0x07
#define REG0B_16M       0x01
#define REG0B_64M       0x02
#define REG0B_128M      0x03
#define REG0B_256M      0x04
#define REG0B_512M      0x05
#define REG0B_1G        0x06
#define REG0B_ENBDRAM   0x08
#define REG0B_RFHDIS    0x10
#define REG0B_CLKSET    0xe0
#define REG0B_24MHZ     0x00
#define REG0B_30MHZ     0x20
#define REG0B_40MHZ     0x40
#define REG0B_48MHZ     0x60
#define REG0B_60MHZ     0x80

#define REG0D 		0x0d
#define REG0D_MTRP_RDY  0x80
#define REG0D_FULLSTP   0x10
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
#define REG17_SNRSYN	0x0f

#define REG18 		0x18
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

#define REG1C_TBTIME    0x07

#define REG1D       	0x1d
#define REG1D_CK4LOW	0x80
#define REG1D_CK3LOW	0x40
#define REG1D_CK1LOW	0x20
#define REG1D_LINESEL	0x1f
#define REG1DS_LINESEL  0

#define REG1E           0x1e
#define REG1E_WDTIME	0xf0
#define REG1ES_WDTIME   4
#define REG1E_WDTIME	0xf0

#define REG30           0x30
#define REG31           0x31
#define REG32           0x32
#define REG32_GPIO16    0x80
#define REG32_GPIO15    0x40
#define REG32_GPIO14    0x20
#define REG32_GPIO13    0x10
#define REG32_GPIO12    0x08
#define REG32_GPIO11    0x04
#define REG32_GPIO10    0x02
#define REG32_GPIO9     0x01
#define REG33           0x33
#define REG34           0x34
#define REG35           0x35
#define REG36           0x36
#define REG37           0x37
#define REG38           0x38
#define REG39           0x39

#define REG60           0x60
#define REG60_LED4TG    0x80
#define REG60_YENB      0x40
#define REG60_YBIT      0x20
#define REG60_ACYNCNRLC 0x10
#define REG60_ENOFFSET  0x08
#define REG60_LEDADD    0x04
#define REG60_CK4ADC    0x02
#define REG60_AUTOCONF  0x01

#define REG80           0x80
#define REG81           0x81

#define REGA0           0xa0
#define REGA0_FSTPSEL   0x28
#define REGA0S_FSTPSEL  3
#define REGA0_STEPSEL   0x03
#define REGA0S_STEPSEL  0

#define REGA1           0xa1
#define REGA2           0xa2
#define REGA3           0xa3
#define REGA4           0xa4
#define REGA5           0xa5
#define REGA6           0xa6
#define REGA7           0xa7
#define REGA8           0xa8
#define REGA9           0xa9
#define REGAA           0xaa
#define REGAB           0xab
#define REGAC           0xac
#define REGAD           0xad
#define REGAE           0xae
#define REGAF           0xaf
#define REGB0           0xb0
#define REGB1           0xb1

#define REGB2           0xb2
#define REGB2_Z1MOD	0x1f
#define REGB3           0xb3
#define REGB3_Z1MOD	0xff
#define REGB4           0xb4
#define REGB4_Z1MOD	0xff

#define REGB5           0xb5
#define REGB5_Z2MOD	0x1f
#define REGB6           0xb6
#define REGB6_Z2MOD	0xff
#define REGB7           0xb7
#define REGB7_Z2MOD	0xff

#define REG100          0x100
#define REG100_DOCSNR   0x80
#define REG100_ADFSNR   0x40
#define REG100_COVERSNR 0x20
#define REG100_CHKVER   0x10
#define REG100_DOCJAM   0x08
#define REG100_HISPDFLG 0x04
#define REG100_MOTMFLG  0x02
#define REG100_DATAENB  0x01

#define REG114          0x114
#define REG115          0x115

#define REG_LINCNT      0x25
#define REG_MAXWD       0x28
#define REG_DPISET      0x2c
#define REG_FEEDL       0x3d
#define REG_CK1MAP      0x74
#define REG_CK3MAP      0x77
#define REG_CK4MAP      0x7a
#define REG_LPERIOD     0x7d
#define REG_DUMMY       0x80
#define REG_STRPIXEL    0x82
#define REG_ENDPIXEL    0x85
#define REG_EXPDMY      0x88
#define REG_EXPR        0x8a
#define REG_EXPG        0x8d
#define REG_EXPB        0x90
#define REG_SEGCNT      0x93
#define REG_TG0CNT      0x96
#define REG_SCANFED     0xa2
#define REG_STEPNO      0xa4
#define REG_FWDSTEP     0xa6
#define REG_BWDSTEP     0xa8
#define REG_FASTNO      0xaa
#define REG_FSHDEC      0xac
#define REG_FMOVNO      0xae
#define REG_FMOVDEC     0xb0
#define REG_Z1MOD       0xb2
#define REG_Z2MOD       0xb5

#define REG_TRUER       0x110
#define REG_TRUEG       0x111
#define REG_TRUEB       0x112

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
  reg_0x07,
  reg_0x08,
  reg_0x09,
  reg_0x0a,
  reg_0x0b,
  reg_0x0c,
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
  reg_0x2a,
  reg_0x2b,
  reg_0x2c,
  reg_0x2d,
  reg_0x3b,
  reg_0x3c,
  reg_0x3d,
  reg_0x3e,
  reg_0x3f,
  reg_0x40,
  reg_0x41,
  reg_0x42,
  reg_0x43,
  reg_0x44,
  reg_0x45,
  reg_0x46,
  reg_0x47,
  reg_0x48,
  reg_0x49,
  reg_0x4f,
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
  reg_0x5f,
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
  reg_0x81,
  reg_0x82,
  reg_0x83,
  reg_0x84,
  reg_0x85,
  reg_0x86,
  reg_0x87,
  reg_0x88,
  reg_0x89,
  reg_0x8a,
  reg_0x8b,
  reg_0x8c,
  reg_0x8d,
  reg_0x8e,
  reg_0x8f,
  reg_0x90,
  reg_0x91,
  reg_0x92,
  reg_0x93,
  reg_0x94,
  reg_0x95,
  reg_0x96,
  reg_0x97,
  reg_0x98,
  reg_0x99,
  reg_0x9a,
  reg_0x9b,
  reg_0x9c,
  reg_0x9d,
  reg_0x9e,
  reg_0x9f,
  reg_0xa0,
  reg_0xa1,
  reg_0xa2,
  reg_0xa3,
  reg_0xa4,
  reg_0xa5,
  reg_0xa6,
  reg_0xa7,
  reg_0xa8,
  reg_0xa9,
  reg_0xaa,
  reg_0xab,
  reg_0xac,
  reg_0xad,
  reg_0xae,
  reg_0xaf,
  reg_0xb0,
  reg_0xb1,
  reg_0xb2,
  reg_0xb3,
  reg_0xb4,
  reg_0xb5,
  reg_0xb6,
  reg_0xb7,
  reg_0xb8,
  reg_0xbb,
  reg_0xbc,
  reg_0xbd,
  reg_0xbe,
  reg_0xc3,
  reg_0xc4,
  reg_0xc5,
  reg_0xc6,
  reg_0xc7,
  reg_0xc8,
  reg_0xc9,
  reg_0xca,
  reg_0xcb,
  reg_0xcc,
  reg_0xcd,
  reg_0xce,
  reg_0xd0,
  reg_0xd1,
  reg_0xd2,
  reg_0xd3,
  reg_0xd4,
  reg_0xd5,
  reg_0xd6,
  reg_0xd7,
  reg_0xd8,
  reg_0xd9,
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
  reg_0xf9,
  reg_0xfa,
  reg_0xfb,
  reg_0xfc,
  reg_0xff,
  GENESYS_GL124_MAX_REGS
};

#define SETREG(adr,val) {dev->reg[reg_##adr].address=adr;dev->reg[reg_##adr].value=val;}

typedef struct
{
  uint8_t r31;
  uint8_t r32;
  uint8_t r33;
  uint8_t r34;
  uint8_t r35;
  uint8_t r36;
  uint8_t r38;
} Gpio_layout;

/** @brief gpio layout
 * describes initial gpio settings for a given model
 */
static Gpio_layout gpios[]={
	/* LiDE 110 */
	{ /*    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x38 */
		0x9f, 0x59, 0x01, 0x80, 0x5f, 0x01, 0x00
	},
	/* LiDE 210 */
	{
		0x9f, 0x59, 0x01, 0x80, 0x5f, 0x01, 0x00
	},
};

typedef struct
{
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
	/* LIDE 110 */
	{
		0x0a, 0x15, 0x20,
		0x00, 0xac, 0x08, 0x55, 0x08, 0x56, 0x0f, 0xff
	},
	/* LIDE 210 */
	{
		0x0a, 0x1f, 0x34,
		0x01, 0x24, 0x08, 0x91, 0x08, 0x92, 0x0f, 0xff
	}
};

/** @brief structure for sensor settings
 * this structure describes the sensor settings to use for a given
 * exposure. Data settings are identified by
 * - sensor id
 * - sensor hardware dpi
 * - half ccd mode
 */
typedef struct {
  int sensor_type;      /**> sensor id */
  int dpi;              /**> maximum dpi for which data are valid */
  int half_ccd;         /**> half ccd mode  */
  int exposure;         /**> exposure */
  int ck1map;           /**> CK1MAP */
  int ck3map;           /**> CK2MAP */
  int ck4map;           /**> CK3MAP */
  int segcnt;           /**> SEGCNT */
  int tg0cnt;           /**> TG0CNT */
  int expdummy;         /**> exposure dummy */
  int expr;             /**> initial red exposure */
  int expg;             /**> initial green exposure */
  int expb;             /**> initial blue exposure */
  size_t *order;        /**> order of sub-segments */
  uint8_t reg18;        /**> register 0x18 value */
  uint8_t reg20;        /**> register 0x20 value */
  uint8_t reg61;        /**> register 0x61 value */
  uint8_t reg98;        /**> register 0x98 value */
} Sensor_Profile;

static size_t order_01[]={0,1};
static size_t order_0213[]={0,2,1,3};

/* *INDENT-OFF* */

/**
 * database of sensor profiles
 */
static Sensor_Profile sensors[]={
        /* LiDE 110 */
	{CIS_CANONLIDE110,  600, 1,  2768, 0x1e, 0x9f, 0x55, 2584, 154,  101,  388,  574,  393, NULL      , 0x00, 0x0c, 0x20, 0x21},
	{CIS_CANONLIDE110,  600, 0,  5360, 0x1e, 0x9f, 0x55, 5168, 163,  101,  388,  574,  393, NULL      , 0x00, 0x0a, 0x20, 0x21},
	{CIS_CANONLIDE110, 1200, 0, 10528, 0x1e, 0x9f, 0x55, 5168, 163,  101,  388,  574,  393, order_01  , 0x00, 0x08, 0x20, 0x22},
	{CIS_CANONLIDE110, 2400, 0, 20864, 0x1e, 0x9f, 0x55, 5168, 163, 4679, 6839, 8401, 6859, order_0213, 0x00, 0x06, 0x20, 0x24},

        /* LiDE 120 */
	{CIS_CANONLIDE120,  600, 1,  2768, 0x0f, 0x9f, 0x55, 2552, 112,   94,  388,  574,  393, NULL      , 0x00, 0x02, 0x20, 0x21},
	{CIS_CANONLIDE120,  600, 0,  5360, 0x0f, 0x9f, 0x55, 5168, 163,   94,  388,  574,  393, NULL      , 0x00, 0x0a, 0x20, 0x21},
	{CIS_CANONLIDE120, 1200, 0, 10528, 0x0f, 0x9f, 0x55, 5168, 163,   94,  388,  574,  393, order_01  , 0x00, 0x08, 0x20, 0x22},
	{CIS_CANONLIDE120, 2400, 0, 20864, 0x0f, 0x9f, 0x55, 5168, 163, 4679, 6839, 8401, 6859, order_0213, 0x00, 0x06, 0x20, 0x24},

        /* LiDE 210 */
	{CIS_CANONLIDE210,  600, 1,  2768, 0x1e, 0x9f, 0x55, 2584, 154,  101,  388,  574,  393, NULL      , 0x00, 0x0c, 0x20, 0x21},
	{CIS_CANONLIDE210,  600, 0,  5360, 0x1e, 0x9f, 0x55, 5168, 163,  101,  388,  574,  393, NULL      , 0x00, 0x0a, 0x20, 0x21},
	{CIS_CANONLIDE210, 1200, 0, 10528, 0x1e, 0x9f, 0x55, 5168, 163,  101,  388,  574,  393, order_01  , 0x00, 0x08, 0x20, 0x22},
	{CIS_CANONLIDE210, 2400, 0, 20864, 0x1e, 0x9f, 0x55, 5168, 163, 4679, 6839, 8401, 6859, order_0213, 0x00, 0x06, 0x20, 0x24},

        /* LiDE 220 */
	{CIS_CANONLIDE220,  600, 1,  2768, 0x0f, 0x9f, 0x55, 2584, 154,  101,  388,  574,  393, NULL      , 0x00, 0x0c, 0x20, 0x21},
	{CIS_CANONLIDE220,  600, 0,  5360, 0x0f, 0x9f, 0x55, 5168, 163,  101,  388,  574,  393, NULL      , 0x00, 0x0a, 0x20, 0x21},
	{CIS_CANONLIDE220, 1200, 0, 10528, 0x0f, 0x9f, 0x55, 5168, 163,  101,  388,  574,  393, order_01  , 0x00, 0x08, 0x20, 0x22},
	{CIS_CANONLIDE220, 2400, 0, 20864, 0x0f, 0x9f, 0x55, 5168, 163, 4679, 6839, 8401, 6859, order_0213, 0x00, 0x06, 0x20, 0x24},
};


#define MOVE_DPI 200
#define MOVE_EXPOSURE 2304

static uint32_t lide210_fast[] = { 62496, 2343, 2343, 2343, 2343, 2343, 2343, 2343, 2343, 2051, 1432, 1372, 1323, 1280, 1246, 1216, 1188, 1163, 1142, 1121, 1101, 1084, 1068, 1051, 1036, 1020, 1007, 995, 983, 971, 959, 949, 938, 929, 917, 908, 900, 891, 882, 874, 866, 857, 849, 843, 835, 829, 821, 816, 808, 802, 795, 789, 784, 778, 773, 765, 760, 755, 749, 744, 739, 734, 731, 726, 721, 716, 711, 707, 702, 698, 693, 690, 685, 682, 677, 672, 669, 665, 662, 657, 654, 650, 647, 644, 639, 637, 632, 629, 626, 622, 619, 617, 614, 610, 607, 604, 601, 599, 595, 592, 589, 587, 584, 581, 579, 576, 572, 570, 567, 564, 562, 559, 557, 554, 552, 549, 547, 544, 542, 539, 538, 536, 533, 531, 529, 526, 524, 522, 519, 518, 516, 513, 511, 509, 506, 505, 503, 501, 498, 497, 495, 493, 491, 490, 487, 485, 483, 482, 480, 477, 476, 474, 472, 470, 469, 467, 465, 464, 462, 460, 458, 456, 455, 453, 451, 450, 448, 447, 445, 444, 442, 440, 439, 437, 436, 434, 433, 431, 430, 428, 427, 425, 423, 422, 420, 419, 417, 417, 415, 414, 413, 411, 410, 408, 407, 405, 404, 402, 401, 400, 399, 398, 396, 395, 393, 392, 391, 390, 389, 387, 386, 385, 383, 382, 381, 380, 379, 377, 376, 375, 374, 373, 371, 370, 369, 368, 367, 366, 364, 363, 363, 361, 360, 359, 358, 357, 356, 355, 353, 352, 352, 350, 349, 348, 347, 346, 345, 344, 343, 342, 341, 340, 339, 338, 335, 335, 0};
static uint32_t lide110_ok[]   = { 62496, 2343, 2343, 2343, 2343, 2343, 2343, 2343, 2343, 2051, 1961, 1901, 1852, 1809, 1775, 1745, 1717, 1692, 1671, 1650, 1630, 1613, 1597,1580,1565,1549,1536,1524,1512,1500,1488,1478,1467,1458,1446,1437,1429,1420,1411,1403,1395,1386,1378,1372,1364,1358,1350,1345,1337,1331,1324,1318,1313,1307,1302,1294,1289,1284,1278,1273,1268,1263,1260,1255,1250,1245,1240,1236,1231,1227,1222,1219,1214,1211,1206,1201,1198,1194,1191,1186,1183,1179,1176,1173,1168,1166,1161,1158,1155,1151,1148,1146,1143,1139,1136,1133,1130,1128,1124,1121,1118,1116,1113,1110,1108,1105,1101,1099,1096,1093,1091,1088,1086,1083,1081,1078,1076,1073,1071,1068,1067,1065,1062,1060,1058,1055,1053,1051,1048,1047,1045,1042,1040,1038,1035,1034,1032,1030,1027,1026,1024,1022,1020,1019,1016,1014,1012,1011,1009,1006,1005,1003,1001,999,998,996,994,993,991,989,987,985,984,982,980,979,977,976,974,973,971,969,968,966,965,963,962,960,959,957,956,954,952,951,949,948,946,946,944,943,942,940,939,937,936,934,933,931,930,929,928,927,925,924,922,921,920,919,918,916,915,914,912,911,910,909,908,906,905,904,903,902,900,899,898,897,896,895,893,892,892,890,889,888,887,886,885,884,882,881,881,879,878,877,876,875,874,873,872,871,870,869,868,867,864,857, 849, 843, 835, 829, 821, 816, 808, 802, 795, 789, 784, 778, 773, 765, 760, 755, 749, 744, 739, 734, 731, 726, 721, 716, 711, 707, 702, 698, 693, 690, 685, 682, 677, 672, 669, 665, 662, 657, 654, 650, 647, 644, 639, 637, 632, 629, 626, 622, 619, 617, 614, 610, 607, 604, 601, 599, 595, 592, 589, 587, 584, 581, 579, 576, 572, 570, 567, 564, 562, 559, 557, 554, 552, 549, 547, 544, 542, 539, 538, 536, 533, 531, 529, 526, 524, 522, 519, 518, 516, 513, 511, 509, 506, 505, 503, 501, 498, 497, 495, 493, 491, 490, 487, 485, 483, 482, 480, 477, 476, 474, 472, 470, 469, 467, 465, 464, 462, 460, 458, 456, 455, 453, 451, 450, 448, 447, 445, 444, 442, 440, 439, 437, 436, 434, 433, 431, 430, 428, 427, 425, 423, 422, 420, 419, 417, 417, 415, 414, 413, 411, 410, 408, 407, 405, 404, 402, 401, 400, 399, 398, 396, 395, 393, 392, 391, 390, 389, 387, 386, 385, 383, 382, 381, 380, 379, 377, 376, 375, 374, 373, 371, 370, 369, 368, 367, 366, 364, 363, 363, 361, 360, 359, 358, 357, 356, 355, 353, 352, 352, 350, 349, 348, 347, 346, 345, 344, 343, 342, 341, 340, 339, 338, 335, 335, 0};
static uint32_t lide110_slow[] = { 62496, 7896, 2632, 0};
static uint32_t lide110_max[]  = { 62496, 31296, 10432, 0};
static uint32_t lide210_max[]  = { 62496, 31296, 20864, 10432, 0};

/**
 * database of motor profiles
 */


/* NEXT LPERIOD=PREVIOUS*2-192 */
static Motor_Profile motors[]={
	{MOTOR_CANONLIDE110,  2768, 0, lide210_fast},
	{MOTOR_CANONLIDE110,  5360, 0, lide110_ok},
	{MOTOR_CANONLIDE110, 10528, 1, lide110_slow},
	{MOTOR_CANONLIDE110, 20864, 2, lide110_max},
	{MOTOR_CANONLIDE210,  2768, 0, lide210_fast},
	{MOTOR_CANONLIDE210,  5360, 0, lide110_ok},
	{MOTOR_CANONLIDE210, 10528, 1, lide110_slow},
	{MOTOR_CANONLIDE210, 20864, 2, lide210_max},
	{0, 0, 0, NULL},
};
/* *INDENT-ON* */
GENESYS_STATIC
SANE_Status gl124_init_scan_regs (Genesys_Device * dev,
                                  Genesys_Register_Set * reg,
                                  float xres,	/*dpi */
                                  float yres,	/*dpi */
                                  float startx,	/*optical_res, from dummy_pixel+1 */
                                  float starty,	/*base_ydpi, from home! */
                                  float pixels,
                                  float lines,
                                  unsigned int depth,
                                  unsigned int channels,
                                  int scan_method,
                                  int scan_mode,
                                  int color_filter,
                                  unsigned int flags);

#ifndef UNIT_TESTING
static
#endif
  SANE_Status gl124_start_action (Genesys_Device * dev);
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl124_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		  SANE_Bool start_motor);
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl124_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		SANE_Bool check_stop);
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl124_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home);
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl124_init (Genesys_Device * dev);
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl124_send_shading_data (Genesys_Device * dev, uint8_t * data, int size);

#ifndef UNIT_TESTING
static
#endif
SANE_Status gl124_feed (Genesys_Device * dev, unsigned int steps, int reverse);

GENESYS_STATIC SANE_Status
gl124_stop_action (Genesys_Device * dev);

GENESYS_STATIC SANE_Status
gl124_send_slope_table (Genesys_Device * dev, int table_nr,
			uint16_t * slope_table, int steps);

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
