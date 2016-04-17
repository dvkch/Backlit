/* sane - Scanner Access Now Easy.

   Copyright (C) 2011-2013 Stéphane Voltz <stef.dev@free.fr>

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

/* Individual bits */
#define REG01           0x01
#define REG01_CISSET	0x80
#define REG01_DOGENB	0x40
#define REG01_DVDSET	0x20
#define REG01_M16DRAM	0x08
#define REG01_DRAMSEL	0x04
#define REG01_SHDAREA	0x02
#define REG01_SCAN	0x01

#define REG02           0x02
#define REG02_NOTHOME	0x80
#define REG02_ACDCDIS	0x40
#define REG02_AGOHOME	0x20
#define REG02_MTRPWR	0x10
#define REG02_FASTFED	0x08
#define REG02_MTRREV	0x04
#define REG02_HOMENEG	0x02
#define REG02_LONGCURV	0x01

#define REG03_LAMPDOG	0x80
#define REG03_AVEENB	0x40
#define REG03_XPASEL	0x20
#define REG03_LAMPPWR	0x10
#define REG03_LAMPTIM	0x0f

#define REG04_LINEART	0x80
#define REG04_BITSET	0x40
#define REG04_AFEMOD	0x30
#define REG04_FILTER	0x0c
#define REG04_FESET	0x03

#define REG04S_AFEMOD   4

#define REG05_DPIHW	0xc0
#define REG05_DPIHW_600	0x00
#define REG05_DPIHW_1200	0x40
#define REG05_DPIHW_2400	0x80
#define REG05_MTLLAMP	0x30
#define REG05_GMMENB	0x08
#define REG05_MTLBASE	0x03

#define REG06_SCANMOD	0xe0
#define REG06S_SCANMOD	5
#define REG06_PWRBIT	0x10
#define REG06_GAIN4	0x08
#define REG06_OPTEST	0x07

#define	REG07_SRAMSEL	0x08
#define REG07_FASTDMA	0x04
#define REG07_DMASEL	0x02
#define REG07_DMARDWR	0x01

#define REG08_DECFLAG 	0x40
#define REG08_GMMFFR	0x20
#define REG08_GMMFFG	0x10
#define REG08_GMMFFB	0x08
#define REG08_GMMZR	0x04
#define REG08_GMMZG	0x02
#define REG08_GMMZB	0x01

#define REG09_MCNTSET	0xc0
#define REG09_CLKSET	0x30
#define REG09_BACKSCAN	0x08
#define REG09_ENHANCE	0x04
#define REG09_SHORTTG	0x02
#define REG09_NWAIT	0x01

#define REG09S_MCNTSET  6
#define REG09S_CLKSET   4


#define REG0A_SRAMBUF	0x01

#define REG0D         	0x0d
#define REG0D_CLRLNCNT	0x01

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

#define REG18_CNSET	0x80
#define REG18_DCKSEL	0x60
#define REG18_CKTOGGLE	0x10
#define REG18_CKDELAY	0x0c
#define REG18_CKSEL	0x03

#define REG1A_MANUAL3	0x02
#define REG1A_MANUAL1	0x01
#define REG1A_CK4INV	0x08
#define REG1A_CK3INV	0x04
#define REG1A_LINECLP	0x02

#define REG1C_TGTIME    0x07

#define REG1D_CK4LOW	0x80
#define REG1D_CK3LOW	0x40
#define REG1D_CK1LOW	0x20
#define REG1D_TGSHLD	0x1f
#define REG1DS_TGSHLD   0


#define REG1E       	0x1e
#define REG1E_WDTIME	0xf0
#define REG1ES_WDTIME   4
#define REG1E_LINESEL	0x0f
#define REG1ES_LINESEL  0

#define REG_EXPR        0x10
#define REG_EXPG        0x12
#define REG_EXPB        0x14
#define REG_STEPNO      0x21
#define REG_FWDSTEP     0x22
#define REG_BWDSTEP     0x23
#define REG_FASTNO      0x24
#define REG_LINCNT      0x25
#define REG_DPISET      0x2c
#define REG_STRPIXEL    0x30
#define REG_ENDPIXEL    0x32
#define REG_LPERIOD     0x38

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

#define REG60_ZIMOD	0x1f
#define REG61_Z1MOD	0xff
#define REG62_Z1MOD	0xff

#define REG63_Z2MOD	0x1f
#define REG64_Z2MOD	0xff
#define REG65_Z2MOD	0xff

#define REG67_STEPSEL	0xc0
#define REG67_FULLSTEP	0x00
#define REG67_HALFSTEP	0x40
#define REG67_QUATERSTEP	0x80
#define REG67_MTRPWM	0x3f

#define REG68_FSTPSEL	0xc0
#define REG68_FULLSTEP	0x00
#define REG68_HALFSTEP	0x40
#define REG68_QUATERSTEP	0x80
#define REG68_FASTPWM	0x3f

#define REG6B_MULTFILM	0x80
#define REG6B_GPOM13	0x40
#define REG6B_GPOM12	0x20
#define REG6B_GPOM11	0x10
#define REG6B_GPO18	0x02
#define REG6B_GPO17	0x01

#define REG6B      	0x6b

#define REG6C      	0x6c
#define REG6C_GPIOH	0xff
#define REG6C_GPIOL	0xff

#define REG6D      	0x6d
#define REG6E      	0x6e
#define REG6F      	0x6f

#define REG87_LEDADD    0x04

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

  reg_0x5d,
  reg_0x5e,
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
  GENESYS_GL841_MAX_REGS
};

#define INITREG(adr,val) {dev->reg[index].address=adr;dev->reg[index].value=val;index++;}

/**
 * prototypes declaration in case of unit testing
 */
#ifdef UNIT_TESTING
SANE_Status
gl841_init_scan_regs (Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      float xres,
		      float yres,
		      float startx,
		      float starty,
		      float pixels,
		      float lines,
		      unsigned int depth,
		      unsigned int channels,
		      int color_filter,
		      unsigned int flags);

SANE_Status
gl841_begin_scan (Genesys_Device * dev,
	 	  Genesys_Register_Set * reg,
		  SANE_Bool start_motor);

SANE_Status
gl841_end_scan (Genesys_Device * dev,
		Genesys_Register_Set __sane_unused__ * reg,
		SANE_Bool check_stop);

SANE_Status
gl841_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home);

SANE_Status
sanei_gl841_repark_head (Genesys_Device * dev);

SANE_Status
gl841_feed (Genesys_Device * dev, int steps);

SANE_Status
gl841_init_motor_regs_scan(Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      unsigned int scan_exposure_time,
		      float scan_yres,
		      int scan_step_type,
		      unsigned int scan_lines,
		      unsigned int scan_dummy,
		      unsigned int feed_steps,
		      int scan_power_mode,
		      unsigned int flags) ;

SANE_Status
gl841_stop_action (Genesys_Device * dev);

SANE_Status
gl841_start_action (Genesys_Device * dev);

SANE_Status
gl841_init_motor_regs(Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      unsigned int feed_steps,
		      unsigned int action,
		      unsigned int flags);

SANE_Status gl841_send_slope_table (Genesys_Device * dev, int table_nr, uint16_t * slope_table, int steps);

SANE_Status gl841_bulk_write_data_gamma (Genesys_Device * dev, uint8_t addr, uint8_t * data, size_t len);

SANE_Status gl841_offset_calibration (Genesys_Device * dev);

SANE_Status gl841_coarse_gain_calibration (Genesys_Device * dev, int dpi);

SANE_Status gl841_led_calibration (Genesys_Device * dev);

SANE_Status gl841_send_shading_data (Genesys_Device * dev, uint8_t * data, int size);

int gl841_scan_step_type(Genesys_Device *dev, int yres);
SANE_Status gl841_write_freq(Genesys_Device *dev, unsigned int ydpi);
#endif

GENESYS_STATIC
int gl841_exposure_time(Genesys_Device *dev,
                    float slope_dpi,
                    int scan_step_type,
                    int start,
                    int used_pixels,
                    int *scan_power_mode);
