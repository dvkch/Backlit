/* HP Scanjet 3900 series - RTS8822 internal config

   Copyright (C) 2005-2009 Jonathan Bravo Lopez <jkdsoft@gmail.com>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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

/* returns device model according to given product and vendor id's */
static SANE_Int cfg_device_get(SANE_Int product, SANE_Int vendor);

/* returns information and capabilities about selected chipset model */
static SANE_Int cfg_chipset_get(SANE_Int model, struct st_chip *chipset);

/* returns the chipset model for each scanner */
static SANE_Int cfg_chipset_model_get(SANE_Int device);

/* buttons for each scanner */
static SANE_Int cfg_buttons_get(struct st_buttons *reg);

/* area constrains for each scanner */
static SANE_Int cfg_constrains_get(struct st_constrains *constrain);

/* spectrum clock generator for each scanner */
static SANE_Int cfg_sscg_get(SANE_Int *enable, SANE_Int *mode, SANE_Int *clock);

/* motor general configuration for each scanner */
static SANE_Int cfg_motor_get(struct st_motorcfg *reg);

/* motor resource for each scanner */
static SANE_Byte *cfg_motor_resource_get(SANE_Byte *size);

/* sensor general configuration for each scanner */
static SANE_Int cfg_sensor_get(struct st_sensorcfg *reg);

/* reference voltages for each scanner */
static void cfg_refvoltages_get(SANE_Int sensortype, SANE_Byte *vrts, SANE_Byte *vrms, SANE_Byte *vrbs);
static void hp3800_refvoltages(SANE_Int usb, SANE_Int sensor, SANE_Byte *vrts, SANE_Byte *vrms, SANE_Byte *vrbs);
static void hp3970_refvoltages(SANE_Int usb, SANE_Int sensor, SANE_Byte *vrts, SANE_Byte *vrms, SANE_Byte *vrbs);

/* offset calibration start and length */
static void cfg_offset_get(SANE_Int sensortype, SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width);
static void ua4900_offset(SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width);
static void hp3800_offset(SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width);
static void hp3970_offset(SANE_Int sensor, SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width);
static void hp4370_offset(SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width);

/* autoref configuration */
static void cfg_autoref_get(struct st_autoref *reg);

/* autoref start effective pixel */
static SANE_Int cfg_effectivepixel_get(SANE_Int sensortype, SANE_Int resolution);
static SANE_Int ua4900_effectivepixel(SANE_Int resolution);
static SANE_Int hp3800_effectivepixel(SANE_Int resolution);
static SANE_Int hp3970_effectivepixel(SANE_Int sensor, SANE_Int resolution);
static SANE_Int hp4370_effectivepixel(SANE_Int resolution);

/* default values for gain and offset */
static SANE_Int cfg_gainoffset_get(SANE_Int sensortype, struct st_gain_offset *reg);
static SANE_Int bq5550_gainoffset(SANE_Int usb, struct st_gain_offset *myreg);
static SANE_Int ua4900_gainoffset(SANE_Int usb, struct st_gain_offset *myreg);
static SANE_Int hp3800_gainoffset(SANE_Int usb, struct st_gain_offset *myreg);
static SANE_Int hp3970_gainoffset(SANE_Int usb, SANE_Int sensor, struct st_gain_offset *myreg);
static SANE_Int hp4370_gainoffset(SANE_Int usb, struct st_gain_offset *myreg);

/* values to detect optimum pulse-width modulation */
static SANE_Int cfg_checkstable_get(SANE_Int lamp, struct st_checkstable *check);
static SANE_Int ua4900_checkstable(SANE_Int lamp, struct st_checkstable *check);
static SANE_Int hp3800_checkstable(SANE_Int lamp, struct st_checkstable *check);
static SANE_Int hp3970_checkstable(SANE_Int lamp, struct st_checkstable *check);
static SANE_Int hp4370_checkstable(SANE_Int lamp, struct st_checkstable *check);

/* fixed pulse-width modulation values */
static SANE_Int cfg_fixedpwm_get(SANE_Int sensortype, SANE_Int scantype);
static SANE_Int ua4900_fixedpwm(SANE_Int scantype, SANE_Int usb);
static SANE_Int hp3800_fixedpwm(SANE_Int scantype, SANE_Int usb);
static SANE_Int hp3970_fixedpwm(SANE_Int scantype, SANE_Int usb, SANE_Int sensor);
static SANE_Int hp4370_fixedpwm(SANE_Int scantype, SANE_Int usb);

/* virtual origin (ser and ler references) */
static void cfg_vrefs_get(SANE_Int sensortype, SANE_Int res, SANE_Int *ser, SANE_Int *ler);
static void hp3800_vrefs(SANE_Int res, SANE_Int *ser, SANE_Int *ler);
static void hp3970_vrefs(SANE_Int usb, SANE_Int sensor, SANE_Int res, SANE_Int *ser, SANE_Int *ler);
static void hp4370_vrefs(SANE_Int res, SANE_Int *ser, SANE_Int *ler);

/* scanmodes supported by each scanner */
static SANE_Int cfg_scanmode_get(SANE_Int sensortype, SANE_Int sm, struct st_scanmode *mymode);
static SANE_Int bq5550_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode);
static SANE_Int ua4900_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode);
static SANE_Int hp3800_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode);
static SANE_Int hp3970_scanmodes(SANE_Int usb, SANE_Int ccd, SANE_Int sm, struct st_scanmode *mymode);
static SANE_Int hp4370_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode);

/* timing values for ccd sensors */
static SANE_Int cfg_timing_get(SANE_Int sensortype, SANE_Int tm, struct st_timing *reg);
static SANE_Int bq5550_timing_get(SANE_Int tm, struct st_timing *reg);
static SANE_Int ua4900_timing_get(SANE_Int tm, struct st_timing *reg);
static SANE_Int hp3800_timing_get(SANE_Int tm, struct st_timing *reg);
static SANE_Int hp3970_timing_get(SANE_Int sensortype, SANE_Int tm, struct st_timing *reg);
static SANE_Int hp4370_timing_get(SANE_Int tm, struct st_timing *reg);

/* motor movements */
static SANE_Int cfg_motormove_get(SANE_Int sensortype, SANE_Int mm, struct st_motormove *reg);
static SANE_Int bq5550_motormove(SANE_Int item, struct st_motormove *reg);
static SANE_Int hp3800_motormove(SANE_Int item, struct st_motormove *reg);
static SANE_Int hp3970_motormove(SANE_Int usb, SANE_Int ccd, SANE_Int item, struct st_motormove *reg);

/* motor curves */
static SANE_Int *cfg_motorcurve_get(void);
static SANE_Int *bq5550_motor(void);
static SANE_Int *hp3800_motor(void);
static SANE_Int *hp3970_motor(void);
static SANE_Int *hp4370_motor(void);

/* shading cut values */
static void cfg_shading_cut_get(SANE_Int sensortype, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void ua4900_shading_cut(SANE_Int usb, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void hp3800_shading_cut(SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void hp3970_shading_cut(SANE_Int usb, SANE_Int ccd, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void hp4370_shading_cut(SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);

/* wrefs values */
static void cfg_wrefs_get(SANE_Int sensortype, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void ua4900_wrefs(SANE_Int usb, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void hp3800_wrefs(SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void hp3970_wrefs(SANE_Int usb, SANE_Int ccd, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);
static void hp4370_wrefs(SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue);


/* DEPRECATED FUNCTIONS !!!!!!!!!!!!!!!!!!! */


static int get_value(int section, int option, int defvalue, int file);

/* HP Scanjet 3800 */
static int hp3800_calibreflective(int option, int defvalue);
static int hp3800_calibtransparent(int option, int defvalue);
static int hp3800_calibnegative(int option, int defvalue);
static int srt_hp3800_scanparam_get(int option, int defvalue);

/* UMAX Astra 4900 */
static int ua4900_calibreflective(int option, int defvalue);
static int ua4900_calibtransparent(int option, int defvalue);
static int ua4900_calibnegative(int option, int defvalue);

/* HP Scanjet 3970 */
static int hp3970_calibreflective(int option, int defvalue);
static int hp3970_calibtransparent(int option, int defvalue);
static int hp3970_calibnegative(int option, int defvalue);
static int srt_hp3970_scanparam_get(int file, int option, int defvalue);
static int srt_hp3970_platform_get(int option, int defvalue);

/* HP Scanjet 4370 */
static int hp4370_calibreflective(int option, int defvalue);
static int hp4370_calibtransparent(int option, int defvalue);
static int hp4370_calibnegative(int option, int defvalue);
static int srt_hp4370_scanparam_get(int file, int option, int defvalue);

/* HP Scanjet g3110 */
static int hpg3110_calibnegative(int option, int defvalue);
static int hpg3110_calibtransparent(int option, int defvalue);

/* ----- Implementation ----- */

/* DEPRECATED enumerations */

enum ConfigFiles
{
	FITCALIBRATE=0,

	T_RTINIFILE, T_USB1INIFILE,
	S_RTINIFILE, S_USB1INIFILE
};

enum fcsec6
{
	CALIBREFLECTIVE = 0, CALIBTRANSPARENT, CALIBNEGATIVEFILM,

	SCANINFO,
	SCAN_CALI,

	WSTRIPXPOS, WSTRIPYPOS,
	BSTRIPXPOS, BSTRIPYPOS,

	BREFR, BREFG, BREFB,

	REFBITDEPTH,
	OFFSETHEIGHT,

	OFFSETNSIGMA, OFFSETTARGETMAX, OFFSETTARGETMIN,
	OFFSETAVGTARGETR, OFFSETAVGTARGETG, OFFSETAVGTARGETB,

	ADCOFFEVENODD,
	CALIBOFFSET1ON,

	ADCOFFQUICKWAY, ADCOFFPREDICTSTART, ADCOFFPREDICTEND,

	OFFSETTUNESTEP1, OFFSETBOUNDARYRATIO1, OFFSETAVGRATIO1,

	OFFSETEVEN1R, OFFSETEVEN1G, OFFSETEVEN1B,
	OFFSETODD1R,  OFFSETODD1G,  OFFSETODD1B,

	ADCOFFPREDICTR,   ADCOFFPREDICTG,   ADCOFFPREDICTB,
	ADCOFFEVEN1R_1ST, ADCOFFEVEN1G_1ST, ADCOFFEVEN1B_1ST,
	ADCOFFODD1R_1ST,	ADCOFFODD1G_1ST,	ADCOFFODD1B_1ST,

	PEAKR, PEAKG, PEAKB,
	MINR, MING, MINB,

	CALIBOFFSET2ON,
	OFFSETTUNESTEP2, OFFSETBOUNDARYRATIO2, OFFSETAVGRATIO2,
	
	OFFSETEVEN2R, OFFSETEVEN2G, OFFSETEVEN2B,
	OFFSETODD2R,  OFFSETODD2G,  OFFSETODD2B,

	GAINHEIGHT, GAINTARGETFACTOR,

	CALIBPAGON,

	HIPAGR, HIPAGG, HIPAGB,
	LOPAGR, LOPAGG, LOPAGB,
	PAGR, PAGG, PAGB,

	CALIBGAIN1ON, GAIN1R, GAIN1G, GAIN1B,
	CALIBGAIN2ON, GAIN2R, GAIN2G, GAIN2B,

	TOTSHADING,

	BSHADINGON, BSHADINGHEIGHT, BSHADINGPREDIFFR, BSHADINGPREDIFFG, BSHADINGPREDIFFB,
	BSHADINGDEFCUTOFF,
	WSHADINGON, WSHADINGHEIGHT, WSHADINGPREDIFFR, WSHADINGPREDIFFG, WSHADINGPREDIFFB,

	PARKHOMEAFTERCALIB,

	SHADINGTIME_16BIT, SHADOWTIME_16BIT, SHADINGTIME_8BIT, SHADOWTIME_8BIT,
	PREVIEWDPI,

	FIRSTDCOFFSETEVEN0, FIRSTDCOFFSETODD0, FIRSTDCOFFSETEVEN1,
	FIRSTDCOFFSETODD1, FIRSTDCOFFSETEVEN2, FIRSTDCOFFSETODD2,

	CALIBOFFSET10N, CALIBOFFSET20N,
	CALIBGAIN10N, CALIBGAIN20N,
	ARRANGELINE,
	COMPRESSION,

	TA_X_START, TA_Y_START,

	DPIGAINCONTROL600,
	DPIGAINCONTROL_TA600,
	DPIGAINCONTROL_NEG600,

	CRVS, MLOCK,
	ENABLEWARMUP,

	NMAXTARGET, NMINTARGET,
	NMAXTARGETTA, NMINTARGETTA,
	NMAXTARGETNEG, NMINTARGETNEG,

	STABLEDIFF,
	DELTAPWM,

	PWMLAMPLEVEL,

	TMAPWMDUTY,

	PAG1, PAG2, PAG3,

	LEFTLEADING,

	WAVE_XSTART,

	WAVE_S575_XDUMMY_2400, WAVE_S575_XDUMMY_1200, WAVE_S575_XDUMMY_600,

	ODD_DCOFFSET11, ODD_DCOFFSET12, ODD_DCOFFSET13,
	ODD_DCOFFSET21, ODD_DCOFFSET22, ODD_DCOFFSET23,

	EVEN_DCOFFSET11, EVEN_DCOFFSET12, EVEN_DCOFFSET13,
	EVEN_DCOFFSET21, EVEN_DCOFFSET22, EVEN_DCOFFSET23,

	DCGAIN11, DCGAIN12, DCGAIN13,
	DCGAIN21, DCGAIN22, DCGAIN23,

	CRYSTALFREQ,

	PGA1, PGA2, PGA3,

	VGAGAIN11, VGAGAIN12, VGAGAIN13,

	DCSTEPEVEN1, DCSTEPODD1,
	DCSTEPEVEN2, DCSTEPODD2,
	DCSTEPEVEN3, DCSTEPODD3,

	FIRSTDCOFFSETEVEN11, FIRSTDCOFFSETODD11,
	FIRSTDCOFFSETEVEN12, FIRSTDCOFFSETODD12,
	FIRSTDCOFFSETEVEN13, FIRSTDCOFFSETODD13,

	DCOFFSETEVEN11, DCOFFSETODD11,
	DCOFFSETEVEN12, DCOFFSETODD12,
	DCOFFSETEVEN13, DCOFFSETODD13,

	SHADINGBASE, SHADINGFACT1, SHADINGFACT2, SHADINGFACT3,

	PIXELDARKLEVEL,

	EXPOSURETIME,
	SCANYSTART, SCANYLINES,

	BINARYTHRESHOLDH, BINARYTHRESHOLDL,
	CLOSETIME,
	PLATFORM,
	SCAN_PARAM,
	USB1_PWM, USB2_PWM,
	WAVETEST,
	DMA_PARAM,
	TRUE_GRAY_PARAM,
	CALI_PARAM,
	DUMMYLINE,

	MEXPT1, MEXPT2, MEXPT3,
	EXPT1, EXPT2, EXPT3,

	STARTPOS,
	LINEDARLAMPOFF,
	MCLKIOC
};

static SANE_Int cfg_device_get(SANE_Int product, SANE_Int vendor)
{
	struct st_myreg
	{
		SANE_Int vendor, product, device;
	};

	struct st_myreg myreg[] =
	{
		/*vendor, prodct, device */
		{  0x4a5, 0x2211, BQ5550 }, /* BenQ 5550                  */
		{  0x6dc, 0x0020, UA4900 }, /* UMAX Astra 4900            */
		{  0x3f0, 0x2605, HP3800 }, /* HP Scanjet 3800            */
		{  0x3f0, 0x2805, HPG2710}, /* HP Scanjet G2710           */
		{  0x3f0, 0x2305, HP3970 }, /* HP Scanjet 3970c           */
		{  0x3f0, 0x2405, HP4070 }, /* HP Scanjet 4070 Photosmart */
		{  0x3f0, 0x4105, HP4370 }, /* HP Scanjet 4370            */
		{  0x3f0, 0x4205, HPG3010}, /* HP Scanjet G3010           */
		{  0x3f0, 0x4305, HPG3110}  /* HP Scanjet G3010           */
	};

	SANE_Int rst = -1; /* default */
	SANE_Int a;
	SANE_Int count = sizeof(myreg) / sizeof(struct st_myreg);

	for (a = 0; a < count; a++)
	{
		if ((vendor == myreg[a].vendor)&&(product == myreg[a].product))
		{
			rst = myreg[a].device;

			break;
		}
	}

	return rst;
}

static SANE_Int cfg_chipset_model_get(SANE_Int device)
{
	/* returns the chipset model for each scanner */
	struct st_myreg
	{
		SANE_Int device, chipset;
	};

	struct st_myreg myreg[] =
	{
		/*device , chipset      */
		{ HP3800 , RTS8822BL_03A },
		{ HPG2710, RTS8822BL_03A },
		{ BQ5550 , RTS8823L_01E  },
		{ UA4900 , RTS8822L_01H  },
		{ HP3970 , RTS8822L_01H  },
		{ HP4070 , RTS8822L_01H  },
		{ HP4370 , RTS8822L_02A  },
		{ HPG3010, RTS8822L_02A  },
		{ HPG3110, RTS8822L_02A  }
	};

	SANE_Int rst = RTS8822L_01H; /* default */
	SANE_Int a;
	SANE_Int count = sizeof(myreg) / sizeof(struct st_myreg);

	for (a = 0; a < count; a++)
	{
		if (device == myreg[a].device)
		{
			rst = myreg[a].chipset;

			break;
		}
	}

	return rst;
}

static SANE_Int cfg_chipset_get(SANE_Int model, struct st_chip *chipset)
{
	/* returns info and capabilities of selected chipset */
	SANE_Int rst = ERROR;

	if (chipset != NULL)
	{
		struct st_chip data[] =
		{
			/* model      , capabilities, name            */
			{RTS8823L_01E , 0           , "RTS8823L-01E" },
			{RTS8822BL_03A, CAP_EEPROM  , "RTS8822BL-03A"},
			{RTS8822L_02A , CAP_EEPROM  , "RTS8822L-02A" },
			{RTS8822L_01H , CAP_EEPROM  , "RTS8822L-01H" }
		};

		SANE_Int a;

		for (a = 0; a < 4; a++)
		{
			if (model == data[a].model)
			{
				/* model found, fill information */
				chipset->model = data[a].model;
				chipset->capabilities = data[a].capabilities;
				chipset->name = strdup(data[a].name);

				if (chipset->name != NULL)
					rst = OK;

				break;
			}
		}
	}

	return rst;
}

/** SEC: Device's Buttons ---------- */

static SANE_Int cfg_buttons_get(struct st_buttons *reg)
{
	/* buttons for each scanner */
	SANE_Int rst = ERROR;

	if (reg != NULL)
	{
		struct st_myreg
		{
			SANE_Int device;
			struct st_buttons value;
		};

		struct st_myreg myreg[] =
		{
			/*device, {count, {btn1, btn2, btn3, btn4, btn5, btn6)} */
			{ BQ5550 , {3    , {0x01, 0x02, 0x08,   -1,   -1,   -1}}},
			{ UA4900 , {4    , {0x04, 0x08, 0x02, 0x01,   -1,   -1}}},
			{ HP3800 , {3    , {0x01, 0x02, 0x04,   -1,   -1,   -1}}},
			{ HPG2710, {3    , {0x01, 0x02, 0x04,   -1,   -1,   -1}}},
			{ HP3970 , {4    , {0x04, 0x08, 0x02, 0x01,   -1,   -1}}},
			{ HP4070 , {4    , {0x04, 0x08, 0x02, 0x01,   -1,   -1}}},
			{ HP4370 , {4    , {0x04, 0x08, 0x02, 0x01,   -1,   -1}}},
			{ HPG3010, {4    , {0x04, 0x08, 0x02, 0x01,   -1,   -1}}},
			{ HPG3110, {4    , {0x04, 0x08, 0x02, 0x01,   -1,   -1}}}
		};

		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_myreg);

		for (a = 0; a < count; a++)
		{
			if (RTS_Debug->dev_model == myreg[a].device)
			{
				memcpy(reg, &myreg[a].value, sizeof(struct st_buttons));
				rst = OK;

				break;
			}
		}
	}

	return rst;
}

/** SEC: Spectrum clock generator ---------- */

static SANE_Int cfg_sscg_get(SANE_Int *enable, SANE_Int *mode, SANE_Int *clock)
{
	SANE_Int rst = ERROR;

	if ((enable != NULL)&&(mode != NULL)&&(clock != NULL))
	{
		struct st_myreg
		{
			SANE_Int device;
			SANE_Int value[3];
		};

		struct st_myreg myreg[] =
		{
			/*device, {enable, mode, clock} */
			{ BQ5550, {1     ,    1,     1}},
			{ UA4900, {1     ,    1,     0}},
			{ HP3800, {1     ,    1,     0}},
			{HPG2710, {1     ,    1,     0}},
			{ HP3970, {1     ,    1,     0}},
			{ HP4070, {1     ,    1,     0}},
			{ HP4370, {1     ,    1,     0}},
			{HPG3010, {1     ,    1,     0}},
			{HPG3110, {1     ,    1,     0}}
		};

		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_myreg);

		/* default values */
		*enable = 0;
		*mode   = 0;
		*clock  = 3;

		for (a = 0; a < count; a++)
		{
			if (RTS_Debug->dev_model == myreg[a].device)
			{
				*enable = myreg[a].value[0];
				*mode   = myreg[a].value[1];
				*clock  = myreg[a].value[2];
				rst = OK;

				break;
			}
		}
	}
	return rst;
}

/** SEC: Motors ---------- */

static SANE_Int cfg_motor_get(struct st_motorcfg *reg)
{
	SANE_Int rst = ERROR;

	if (reg != NULL)
	{
		struct st_myreg
		{
			SANE_Int device;
			struct st_motorcfg motor;
		};

		struct st_myreg myreg[] =
		{
			/*device, {type          ,  res, freq, speed, basemove, highmove, parkmove, change}} */
			{ BQ5550, {MT_OUTPUTSTATE, 1200,   30,   800,        1,        0,        0,   TRUE}},
			{ UA4900, {MT_OUTPUTSTATE, 2400,   30,   800,        1,        0,        0,   TRUE}},
			{ HP3800, {MT_OUTPUTSTATE, 1200,   30,   800,        1,        0,        0,   TRUE}},
			{HPG2710, {MT_OUTPUTSTATE, 1200,   30,   800,        1,        0,        0,   TRUE}},
			{ HP3970, {MT_OUTPUTSTATE, 2400,   30,   800,        1,        0,        0,   TRUE}},
			{ HP4070, {MT_OUTPUTSTATE, 2400,   30,   800,        1,        0,        0,   TRUE}},
			{ HP4370, {MT_OUTPUTSTATE, 2400,   30,   800,        1,        0,        0,   TRUE}},
			{HPG3010, {MT_OUTPUTSTATE, 2400,   30,   800,        1,        0,        0,   TRUE}},
			{HPG3110, {MT_OUTPUTSTATE, 2400,   30,   800,        1,        0,        0,   TRUE}}
		};

		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_myreg);

		/* default values */
		memset(reg, 0, sizeof(struct st_motorcfg));
		reg->type = -1;

		for (a = 0; a < count; a++)
		{
			if (RTS_Debug->dev_model == myreg[a].device)
			{
				memcpy(reg, &myreg[a].motor, sizeof(struct st_motorcfg));
				rst = OK;

				break;
			}
		}
	}

	return rst;
}

/** SEC: Sensors ---------- */

static SANE_Int cfg_sensor_get(struct st_sensorcfg *reg)
{
	SANE_Int rst = ERROR;

	if (reg != NULL)
	{
		struct st_myreg
		{
			SANE_Int device;
			struct st_sensorcfg sensor;
		};

		struct st_myreg myreg[] =
		{
			/*device, {type      , name   , resolution, {chnl_colors               }, {chnl_gray  }, {rgb_order                 }, line_dist, evenodd_dist} */
			{ BQ5550, {CCD_SENSOR,      -1, 1200      , {CL_BLUE, CL_GREEN, CL_RED }, {CL_GREEN, 0}, {CL_BLUE, CL_GREEN, CL_RED }, 24       , 4           }},
			{ UA4900, {CIS_SENSOR, SNYS575, 2400      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 24       , 0           }},
			{ HP3800, {CCD_SENSOR, TCD2905, 2400      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 64       , 8           }},
			{HPG2710, {CCD_SENSOR, TCD2905, 2400      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 64       , 8           }},
			{ HP3970, {CCD_SENSOR, TCD2952, 2400      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 24       , 4           }},
			{ HP4070, {CCD_SENSOR, TCD2952, 2400      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 24       , 4           }},
			{ HP4370, {CCD_SENSOR, TCD2958, 4800      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 128      , 6           }},
			{HPG3010, {CCD_SENSOR, TCD2958, 4800      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 128      , 6           }},
			{HPG3110, {CCD_SENSOR, TCD2958, 4800      , {CL_RED , CL_GREEN, CL_BLUE}, {CL_RED  , 0}, {CL_RED , CL_GREEN, CL_BLUE}, 128      , 6           }}
		};

		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_myreg);

		/* default values */
		memset(reg, 0, sizeof(struct st_sensorcfg));
		reg->type = -1;

		for (a = 0; a < count; a++)
		{
			if (RTS_Debug->dev_model == myreg[a].device)
			{
				memcpy(reg, &myreg[a].sensor, sizeof(struct st_sensorcfg));
				rst = OK;

				break;
			}
		}
	}

	return rst;
}

/** SEC: Reference voltages ---------- */

static void hp3800_refvoltages(SANE_Int usb, SANE_Int sensor, SANE_Byte *vrts, SANE_Byte *vrms, SANE_Byte *vrbs)
{
	/* this function returns top, middle and bottom reference voltages for each scanner */
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int sensor;
		SANE_Byte values[3];
	};

	struct st_reg myreg[] =
	{
		/* usb, sensor    , {vrts, vrms, vrbs} */
		{USB20, CCD_SENSOR, {   2,    3,    2}},
		{USB11, CCD_SENSOR, {   2,    3,    2}},
	};

	if ((vrts != NULL)&&(vrms != NULL)&&(vrbs != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_reg);

		*vrts = *vrms = *vrbs = 0;

		for (a = 0; a < count; a++)
		{
			if ((myreg[a].usb == usb)&&(myreg[a].sensor == sensor))
			{
				*vrts = myreg[a].values[0];
				*vrms = myreg[a].values[1];
				*vrbs = myreg[a].values[2];
			}
		}
	}
}

static void hp3970_refvoltages(SANE_Int usb, SANE_Int sensor, SANE_Byte *vrts, SANE_Byte *vrms, SANE_Byte *vrbs)
{
	/* this function returns top, middle and bottom reference voltages for each scanner */
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int sensor;
		SANE_Byte values[3];
	};

	struct st_reg myreg[] =
	{
		/* usb, sensor     , {vrts, vrms, vrbs} */
		{USB20, CCD_SENSOR, {   0,    0,    0}},
		{USB11, CCD_SENSOR, {   0,    0,    0}},
		{USB20, CIS_SENSOR, {   0,    0,    0}},
		{USB11, CIS_SENSOR, {   0,    0,    0}}
	};

	if ((vrts != NULL)&&(vrms != NULL)&&(vrbs != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_reg);

		*vrts = *vrms = *vrbs = 0;

		for (a = 0; a < count; a++)
		{
			if ((myreg[a].usb == usb)&&(myreg[a].sensor == sensor))
			{
				*vrts = myreg[a].values[0];
				*vrms = myreg[a].values[1];
				*vrbs = myreg[a].values[2];
			}
		}
	}
}

static void cfg_refvoltages_get(SANE_Int sensortype, SANE_Byte *vrts, SANE_Byte *vrms, SANE_Byte *vrbs)
{
	/* this function returns top, middle and bottom reference voltages for each scanner */
	switch(RTS_Debug->dev_model)
	{
		case HP3800:
		case HPG2710:
			hp3800_refvoltages(RTS_Debug->usbtype, sensortype, vrts, vrms, vrbs);
			break;
		default:
			/* at this momment all analyzed scanners have the same values */
			hp3970_refvoltages(RTS_Debug->usbtype, sensortype, vrts, vrms, vrbs);
			break;
	}
}

/** SEC: Calibration Offset ---------- */

static void hp3800_offset(SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width)
{
	/* this function provides left coordinate and width to calculate offset
	   Sensor = Toshiba T2905
	 */

	struct st_ofst
	{
		SANE_Int left;
		SANE_Int width;
	};

	struct st_reg
	{
		SANE_Int resolution;
		struct st_ofst values[3];
	};

	struct st_reg myreg[] =
	{
		/*res , {ref(L,W), tma(L,W), neg(L,W)} */
		{ 2400, {{15, 20}, {15, 20}, {15, 20}}},
		{ 1200, {{10, 10}, {10, 10}, {10, 10}}},
		{  600, {{ 2, 10}, { 5, 10}, { 5, 10}}},
		{  300, {{ 1,  5}, { 1,  5}, { 1,  5}}},
		{  150, {{ 0,  3}, { 0,  3}, { 0,  3}}}
	};

	if ((left != NULL)&&(width != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (myreg[a].resolution == resolution)
			{
				scantype--;

				*left  = myreg[a].values[scantype].left;
				*width = myreg[a].values[scantype].width;

				break;
			}
		}
	}
}

static void hp3970_offset(SANE_Int sensor, SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width)
{
	/* this function provides left coordinate and width to calculate offset */

	struct st_ofst
	{
		SANE_Int left;
		SANE_Int width;
	};

	struct st_reg
	{
		SANE_Int sensor;
		SANE_Int resolution;
		struct st_ofst values[3];
	};

	struct st_reg myreg[] =
	{
		/* sensor   , res , {ref(L,W), tma(L,W), neg(L,W)} */
		{CCD_SENSOR, 2400, {{16, 20}, {16, 20}, {16, 20}}},
		{CCD_SENSOR, 1200, {{16, 10}, {16, 10}, {16, 10}}},
		{CCD_SENSOR,  600, {{15, 10}, {15, 10}, {15, 10}}},
		{CCD_SENSOR,  300, {{ 7,  5}, { 7,  5}, { 7,  5}}},
		{CCD_SENSOR,  200, {{ 7,  3}, { 7,  3}, { 7,  3}}},
		{CCD_SENSOR,  100, {{ 3,  3}, { 3,  3}, { 3,  3}}},

		/* sensor  , res , {ref(L,W), tma(L,W), neg(L,W)} */
		{CIS_SENSOR, 2400, {{84, 20}, {84, 20}, {84, 20}}},
		{CIS_SENSOR, 1200, {{54, 10}, {54, 10}, {54, 10}}},
		{CIS_SENSOR,  600, {{28, 10}, {28, 10}, {28, 10}}},
		{CIS_SENSOR,  300, {{15,  5}, {15,  5}, {15,  5}}},
		{CIS_SENSOR,  200, {{ 5,  3}, { 5,  3}, { 5,  3}}},
		{CIS_SENSOR,  100, {{ 2,  3}, { 2,  3}, { 2,  3}}}
	};

	if ((left != NULL)&&(width != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if ((myreg[a].sensor == sensor)&&(myreg[a].resolution == resolution))
			{
				scantype--;

				*left  = myreg[a].values[scantype].left;
				*width = myreg[a].values[scantype].width;

				break;
			}
		}
	}
}

static void hp4370_offset(SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width)
{
	/* this function provides left coordinate and width to calculate offset */

	struct st_ofst
	{
		SANE_Int left;
		SANE_Int width;
	};

	struct st_reg
	{
		SANE_Int resolution;
		struct st_ofst values[3];
	};

	struct st_reg myreg[] =
	{
		/* res, {ref(L,W), tma(L,W), neg(L,W)} */
		{ 4800, {{42, 20}, {42, 20}, {52, 26}}},
		{ 2400, {{14, 20}, {14, 20}, {14, 26}}},
		{ 1200, {{ 8, 14}, { 8, 14}, { 8, 14}}},
		{  600, {{ 4,  8}, { 4,  8}, { 4,  8}}},
		{  300, {{ 2,  4}, { 2,  4}, { 2,  4}}},
		{  150, {{ 1,  2}, { 1,  2}, { 1,  2}}}
	};

	if ((left != NULL)&&(width != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (myreg[a].resolution == resolution)
			{
				scantype--;

				*left  = myreg[a].values[scantype].left;
				*width = myreg[a].values[scantype].width;

				break;
			}
		}
	}
}

static void ua4900_offset(SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width)
{
	/* this function provides left coordinate and width to calculate offset */

	struct st_ofst
	{
		SANE_Int left;
		SANE_Int width;
	};

	struct st_reg
	{
		SANE_Int resolution;
		struct st_ofst values[3];
	};

	struct st_reg myreg[] =
	{
		/* res , {ref(L,W), tma(L,W), neg(L,W)} */
		{  2400, {{20, 20}, {16, 20}, {16, 20}}},
		{  1200, {{20, 10}, {10, 10}, {10, 10}}},
		{   600, {{ 7, 10}, {15, 10}, {15, 10}}},
		{   300, {{ 5, 10}, { 7,  8}, { 7,  8}}},
		{   200, {{ 2, 10}, { 7,  6}, { 7,  6}}},
		{   100, {{ 0, 10}, { 3,  4}, { 3,  4}}}
	};

	if ((left != NULL)&&(width != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (myreg[a].resolution == resolution)
			{
				scantype--;

				*left  = myreg[a].values[scantype].left;
				*width = myreg[a].values[scantype].width;

				break;
			}
		}
	}
}

static void cfg_offset_get(SANE_Int sensortype, SANE_Int resolution, SANE_Int scantype, SANE_Int *left, SANE_Int *width)
{
	switch(RTS_Debug->dev_model)
	{
		case UA4900:
			ua4900_offset(resolution, scantype, left, width);
			break;

		case HP3800:
		case HPG2710:
			hp3800_offset(resolution, scantype, left, width);
			break;

		case HPG3010:
		case HPG3110:
		case HP4370:
			hp4370_offset(resolution, scantype, left, width);
			break;

		default:
			hp3970_offset(sensortype, resolution, scantype, left, width);
			break;
	}
}

/** SEC: Device constrains ---------- */

static SANE_Int cfg_constrains_get(struct st_constrains *constrain)
{
	SANE_Int rst = ERROR;

	struct st_reg
	{
		SANE_Int device;
		struct st_constrains constrain;
	};

	struct st_reg reg[] =
	{
		/* constrains are set in milimeters */
		/*device ,   reflective               , negative                  , transparent                   */
		/*       , {{left, width, top, height}, {left, width, top, height}, {left, width, top, height}}}, */
		{ BQ5550 , {{   0,   220,   0,    300}, {  88,    42,   0,     83}, {  88,    42,   0,     83}}},
		{ HP3800 , {{   0,   220,   0,    300}, {  89,    45,   0,     85}, {  89,    45,   0,    100}}},
		{HPG2710 , {{   0,   220,   0,    300}, {  89,    45,   0,     85}, {  89,    45,   0,    100}}},
		{ HP3970 , {{   0,   220,   0,    300}, {  88,    42,   0,     83}, {  88,    42,   0,     83}}},
		{ HP4070 , {{   0,   220,   0,    300}, {  58,    99,   0,    197}, {  58,    99,   0,    197}}},
		{ HP4370 , {{   0,   220,   0,    300}, {  90,    45,   0,     85}, {  90,    45,   0,    100}}},
		{ UA4900 , {{   0,   220,   0,    300}, {  88,    42,   0,     83}, {  88,    42,   0,     83}}},
		{HPG3010 , {{   0,   220,   0,    300}, {  90,    45,   0,     85}, {  89,    45,   0,    100}}},
		{HPG3110 , {{   0,   220,   0,    300}, {  92,    28,   0,    190}, {  85,    44,   0,    190}}}
	};

	if (constrain != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].device == RTS_Debug->dev_model)
			{
				memcpy(constrain, &reg[a].constrain, sizeof(struct st_constrains));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

/** SEC: Motor resource ------------------- */
static SANE_Byte *cfg_motor_resource_get(SANE_Byte *size)
{
	/* this function returns the proper motor resources for a given device */
	SANE_Byte *rst = NULL;

	/* Until now, resource size is always 32 bytes */
	rst = (SANE_Byte *)malloc(sizeof(SANE_Byte) * 32);
	if (size != NULL)
		*size = 0;

	if (rst != NULL)
	{
		bzero(rst, sizeof(SANE_Byte) * 32);

		switch(RTS_Debug->dev_model)
		{
			case BQ5550:
				{
					SANE_Byte Resource[] = {0xff, 0xb4, 0xb0, 0xd4, 0xd0, 0x70, 0x50, 0x54, 0x30, 0x34, 0x14, 0x38, 0x18, 0x0c, 0x08, 0x28, 0x04, 0x24, 0x20, 0x44, 0x40, 0xe0, 0xc0, 0xc4, 0xa0, 0xa4, 0x84, 0xa8, 0x88, 0x9c, 0x98, 0xb8};
					memcpy(rst, &Resource, sizeof(SANE_Byte) * 32);
					if (size != NULL)
						*size = 32;
				}
				break;
			default:
				{
					SANE_Byte Resource[] = {0xff, 0x90, 0xb0, 0xd4, 0xd0, 0x70, 0x50, 0x54, 0x30, 0x10, 0x14, 0x38, 0x18, 0x0c, 0x08, 0x28, 0x04, 0x00, 0x20, 0x44, 0x40, 0xe0, 0xc0, 0xc4, 0xa0, 0x80, 0x84, 0xa8, 0x88, 0x9c, 0x98, 0xb8};
					memcpy(rst, &Resource, sizeof(SANE_Byte) * 32);
					if (size != NULL)
						*size = 32;
				}
				break;
		}
	}

	return rst;
}

/** SEC: Auto reference position ---------- */

static void cfg_autoref_get(struct st_autoref *reg)
{
	if (reg != NULL)
	{
		struct st_reg
		{
			SANE_Int device;
			struct st_autoref value;
		};

		struct st_reg myreg[] =
		{
			/* x and y offsets are based on 2400 dpi */
			/* device, { type              , x  , y  , resolution, extern_boundary}*/
			{ BQ5550 , {REF_NONE           , -40, -40, 600       , 40}},
			{ UA4900 , {REF_NONE           , -40, -40, 600       , 40}},
			{ HP3800 , {REF_TAKEFROMSCANNER,  88, 624, 600       , 40}},
			{HPG2710 , {REF_TAKEFROMSCANNER,  88, 624, 600       , 40}},
			{ HP3970 , {REF_TAKEFROMSCANNER,  88, 717, 600       , 40}},
			{ HP4070 , {REF_TAKEFROMSCANNER,  88, 717, 600       , 40}},
			{ HP4370 , {REF_TAKEFROMSCANNER,  88, 717, 600       , 40}},
			{HPG3010 , {REF_TAKEFROMSCANNER,  88, 717, 600       , 40}},
			{HPG3110 , {REF_TAKEFROMSCANNER,  88, 717, 600       , 40}}
		};

		SANE_Int a;
		SANE_Int count = sizeof(myreg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (myreg[a].device == RTS_Debug->dev_model)
			{
				memcpy(reg, &myreg[a].value, sizeof(struct st_autoref));
				break;
			}
		}
	}
}

static SANE_Int hp3800_effectivepixel(SANE_Int resolution)
{
	struct st_reg
	{
		SANE_Int resolution;
		SANE_Int pixel;
	};

	struct st_reg reg[] =
	{
		/* res , pixel */
		{  2400, 134 },
		{  1200, 134 },
		{   600, 230 },
		{   300, 172 },
		{   200, 230 },
		{   100, 230 }
	};

	SANE_Int a;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);
	SANE_Int rst = 230; /* default */

	for (a = 0; a < count; a++)
	{
		if (reg[a].resolution == resolution)
		{
			rst = reg[a].pixel;
			break;
		}
	}

	return rst;
}

static SANE_Int hp3970_effectivepixel(SANE_Int sensor, SANE_Int resolution)
{
	struct st_reg
	{
		SANE_Int resolution;
		SANE_Int pixel[2];
	};

	struct st_reg reg[] =
	{
		/* res , {Toshiba, sony}} */
		{  2400, {134    , 218 }},
		{  1200, {134    , 240 }},
		{   600, {230    , 242 }},
		{   300, {160    , 172 }},
		{   200, {230    , 242 }},
		{   100, {230    , 242 }}
	};

	SANE_Int a;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);
	SANE_Int rst = 230; /* default */

	for (a = 0; a < count; a++)
	{
		if (reg[a].resolution == resolution)
		{
			rst = (sensor == CCD_SENSOR)? reg[a].pixel[0] : reg[a].pixel[1];
			break;
		}
	}

	return rst;
}

static SANE_Int hp4370_effectivepixel(SANE_Int resolution)
{
	struct st_reg
	{
		SANE_Int resolution;
		SANE_Int pixel;
	};

	struct st_reg reg[] =
	{
		/* res , pxl */
		{  4800, 134},
		{  2400, 134},
		{  1200, 134},
		{   600, 230},
		{   300, 230},
		{   150, 230}
	};

	SANE_Int a;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);
	SANE_Int rst = 230; /* default */

	for (a = 0; a < count; a++)
	{
		if (reg[a].resolution == resolution)
		{
			rst = reg[a].pixel;
			break;
		}
	}

	return rst;
}

static SANE_Int ua4900_effectivepixel(SANE_Int resolution)
{
	struct st_reg
	{
		SANE_Int resolution;
		SANE_Int pixel;
	};

	struct st_reg reg[] =
	{
		/* res , pixel */
		{  2400, 134 },
		{  1200, 134 },
		{   600, 230 },
		{   300, 172 },
		{   200, 230 },
		{   100, 230 }
	};

	SANE_Int a;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);
	SANE_Int rst = 230; /* default */

	for (a = 0; a < count; a++)
	{
		if (reg[a].resolution == resolution)
		{
			rst = reg[a].pixel;
			break;
		}
	}

	return rst;
}

static SANE_Int cfg_effectivepixel_get(SANE_Int sensortype, SANE_Int resolution)
{
	SANE_Int rst;

	switch(RTS_Debug->dev_model)
	{
		case UA4900:
			rst = ua4900_effectivepixel(resolution);
			break;

		case HP3800:
		case HPG2710:
			rst = hp3800_effectivepixel(resolution);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			rst = hp4370_effectivepixel(resolution);
			break;

		default:
			rst = hp3970_effectivepixel(sensortype, resolution);
			break;
	}

	return rst;
}

/** SEC: Gain and offset values ---------- */

static SANE_Int bq5550_gainoffset(SANE_Int usb, struct st_gain_offset *myreg)
{
	struct st_reg
	{
		SANE_Int usb;
		struct st_gain_offset values;
	};

	struct st_reg reg[] =
	{
		/* usb  , {{edcg1        }, {edcg2  }, {odcg1        }, {odcg2  }, {pag    }, {vgag1     }, {vgag2  }}} */
		{  USB20, {{264, 264, 264}, {0, 0, 0}, {262, 262, 262}, {0, 0, 0}, {3, 3, 3}, {27, 27, 27}, {4, 4, 4}}},
		{  USB11, {{264, 264, 264}, {0, 0, 0}, {262, 262, 262}, {0, 0, 0}, {3, 3, 3}, {27, 27, 27}, {4, 4, 4}}}
	};

	SANE_Int rst = ERROR;

	if (myreg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].usb == usb)
			{
				memcpy(myreg, &reg[a].values, sizeof(struct st_gain_offset));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int hp3800_gainoffset(SANE_Int usb, struct st_gain_offset *myreg)
{
	struct st_reg
	{
		SANE_Int usb;
		struct st_gain_offset values;
	};

	struct st_reg reg[] =
	{
		/* usb  , {{edcg1  }, {edcg2  }, {odcg1  }, {odcg2  }, {pag    }, {vgag1  }, {vgag2  }}} */
		{  USB20, {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {2, 2, 2}, {4, 4, 4}, {4, 4, 4}}},
		{  USB11, {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {2, 2, 2}, {4, 4, 4}, {4, 4, 4}}}
	};

	SANE_Int rst = ERROR;

	if (myreg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].usb == usb)
			{
				memcpy(myreg, &reg[a].values, sizeof(struct st_gain_offset));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int hp3970_gainoffset(SANE_Int usb, SANE_Int sensor, struct st_gain_offset *myreg)
{
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int sensor;
		struct st_gain_offset values;
	};

	struct st_reg reg[] =
	{
		/* usb  , sensor    , {{edcg1        }, {edcg2  }, {odcg1        }, {odcg2  }, {pag    }, {vgag1  }, {vgag2  }}} */
		{  USB20, CCD_SENSOR, {{280, 266, 286}, {0, 0, 0}, {280, 266, 286}, {0, 0, 0}, {3, 3, 3}, {7, 4, 4}, {7, 4, 4}}},
		{  USB11, CCD_SENSOR, {{280, 266, 286}, {0, 0, 0}, {280, 266, 286}, {0, 0, 0}, {3, 3, 3}, {7, 4, 4}, {7, 4, 4}}},

		{  USB20, CIS_SENSOR, {{280, 266, 286}, {0, 0, 0}, {280, 266, 286}, {0, 0, 0}, {3, 3, 3}, {7, 4, 4}, {7, 4, 4}}},
		{  USB11, CIS_SENSOR, {{280, 266, 286}, {0, 0, 0}, {280, 266, 286}, {0, 0, 0}, {3, 3, 3}, {7, 4, 4}, {7, 4, 4}}}
	};

	SANE_Int rst = ERROR;

	if (myreg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; count < 4; a++)
		{
			if ((reg[a].usb == usb)&&(reg[a].sensor == sensor))
			{
				memcpy(myreg, &reg[a].values, sizeof(struct st_gain_offset));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int hp4370_gainoffset(SANE_Int usb, struct st_gain_offset *myreg)
{
	struct st_reg
	{
		SANE_Int usb;
		struct st_gain_offset values;
	};

	struct st_reg reg[] =
	{
		/* usb  , {{edcg1        }, {edcg2  }, {odcg1        }, {odcg2  }, {pag    }, {vgag1  }, {vgag2  }} */
		{  USB20, {{280, 266, 286}, {0, 0, 0}, {280, 266, 286}, {0, 0, 0}, {3, 3, 3}, {7, 4, 4}, {7, 4, 4}}},
		{  USB11, {{280, 266, 286}, {0, 0, 0}, {280, 266, 286}, {0, 0, 0}, {3, 3, 3}, {7, 4, 4}, {7, 4, 4}}}
	};

	SANE_Int rst = ERROR;

	if (myreg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].usb == usb)
			{
				memcpy(myreg, &reg[a].values, sizeof(struct st_gain_offset));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int ua4900_gainoffset(SANE_Int usb, struct st_gain_offset *myreg)
{
	struct st_reg
	{
		SANE_Int usb;
		struct st_gain_offset values;
	};

	struct st_reg reg[] =
	{
		/* usb  , {{edcg1        }, {edcg2  }, {odcg1        }, {odcg2  }, {pag    }, {vgag1     }, {vgag2  }}} */
		{  USB20, {{321, 321, 321}, {0, 0, 0}, {321, 321, 321}, {0, 0, 0}, {0, 0, 0}, {24, 21, 19}, {8, 8, 8}}},
		{  USB11, {{321, 321, 321}, {0, 0, 0}, {321, 321, 321}, {0, 0, 0}, {0, 0, 0}, {16, 16, 16}, {4, 4, 4}}}
	};

	SANE_Int rst = ERROR;

	if (myreg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].usb == usb)
			{
				memcpy(myreg, &reg[a].values, sizeof(struct st_gain_offset));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int cfg_gainoffset_get(SANE_Int sensortype, struct st_gain_offset *reg)
{
	SANE_Int rst;

	switch(RTS_Debug->dev_model)
	{
		case BQ5550:
			rst = bq5550_gainoffset(RTS_Debug->usbtype, reg);
			break;

		case UA4900:
			rst = ua4900_gainoffset(RTS_Debug->usbtype, reg);
			break;

		case HP3800:
		case HPG2710:
			rst = hp3800_gainoffset(RTS_Debug->usbtype, reg);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			rst = hp4370_gainoffset(RTS_Debug->usbtype, reg);
			break;

		default:
			rst = hp3970_gainoffset(RTS_Debug->usbtype, sensortype, reg);
			break;
	}

	return rst;
}

/** SEC: Pulse-width modulation check stable ---------- */

static SANE_Int hp3800_checkstable(SANE_Int lamp, struct st_checkstable *check)
{
	struct st_reg
	{
		SANE_Int lamp;
		struct st_checkstable values;
	};

	struct st_reg reg[] =
	{
		/* lamp    , { diff, interval, tottime } */
		{  0       , { 100.,      200,  10000}},
		{  FLB_LAMP, { 100.,      200,  10000}},
		{  TMA_LAMP, { 100.,      200,  10000}}
	};

	SANE_Int rst = ERROR;

	if (reg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].lamp == lamp)
			{
				memcpy(check, &reg[a].values, sizeof(struct st_checkstable));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int hp3970_checkstable(SANE_Int lamp, struct st_checkstable *check)
{
	struct st_reg
	{
		SANE_Int lamp;
		struct st_checkstable values;
	};

	struct st_reg reg[] =
	{
		/* lamp    , { diff, interval, tottime } */
		{  0       , {1000.,      200,   5000}},
		{  FLB_LAMP, { 500.,      200,   5000}},
		{  TMA_LAMP, { 500.,      200,   5000}}
	};

	SANE_Int rst = ERROR;

	if (reg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].lamp == lamp)
			{
				memcpy(check, &reg[a].values, sizeof(struct st_checkstable));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int hp4370_checkstable(SANE_Int lamp, struct st_checkstable *check)
{
	struct st_reg
	{
		SANE_Int lamp;
		struct st_checkstable values;
	};

	struct st_reg reg[] =
	{
		/* lamp    , { diff, interval, tottime } */
		{  0       , { 100.,      200,   5000}},
		{  FLB_LAMP, { 300.,      200,   5000}},
		{  TMA_LAMP, {   0.,      200,  25000}}
	};

	SANE_Int rst = ERROR;

	if (reg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].lamp == lamp)
			{
				memcpy(check, &reg[a].values, sizeof(struct st_checkstable));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int ua4900_checkstable(SANE_Int lamp, struct st_checkstable *check)
{
	struct st_reg
	{
		SANE_Int lamp;
		struct st_checkstable values;
	};

	struct st_reg reg[] =
	{
		/* lamp    , { diff, interval, tottime } */
		{  0       , { 100.,      200,   5000}},
		{  FLB_LAMP, {  10.,      200,   5000}},
		{  TMA_LAMP, {  10.,      200,  25000}}
	};

	SANE_Int rst = ERROR;

	if (reg != NULL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		for (a = 0; a < count; a++)
		{
			if (reg[a].lamp == lamp)
			{
				memcpy(check, &reg[a].values, sizeof(struct st_checkstable));
				rst = OK;
				break;
			}
		}
	}

	return rst;
}

static SANE_Int cfg_checkstable_get(SANE_Int lamp, struct st_checkstable *check)
{
	SANE_Int rst;

	switch(RTS_Debug->dev_model)
	{
		case UA4900:
			rst = ua4900_checkstable(lamp, check);
			break;

		case HP3800:
		case HPG2710:
			rst = hp3800_checkstable(lamp, check);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			rst = hp4370_checkstable(lamp, check);
			break;

		default:
			rst = hp3970_checkstable(lamp, check);
			break;
	}

	return rst;
}

/** SEC: Fixed pulse-width modulation values ---------- */

static SANE_Int hp3800_fixedpwm(SANE_Int scantype, SANE_Int usb)
{
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int pwm[3];
	};

	struct st_reg reg[] =
	{
		/* usb  , { ST_NORMAL, ST_TA, ST_NEG} */
		{  USB20, {         0,     0,      0}},
		{  USB11, {         0,     0,      0}}
	};

	SANE_Int a, rst = 0x16;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

	for (a = 0; a < count; a++)
	{
		if (reg[a].usb == usb)
		{
			if ((scantype < ST_NORMAL)||(scantype > ST_NEG))
				scantype = ST_NORMAL;

			rst = reg[a].pwm[scantype - 1];
			break;
		}
	}

	return rst;
}

static SANE_Int hp3970_fixedpwm(SANE_Int scantype, SANE_Int usb, SANE_Int sensor)
{
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int sensor;
		SANE_Int pwm[3];
	};

	struct st_reg reg[] =
	{
		/* usb  , sensor    , { ST_NORMAL, ST_TA, ST_NEG} */
		{  USB20, CCD_SENSOR, {        22,    22,     22}},
		{  USB11, CCD_SENSOR, {        22,    22,     22}},

		{  USB20, CIS_SENSOR, {        22,    22,     22}},
		{  USB11, CIS_SENSOR, {        22,    22,     22}}
	};

	SANE_Int a, rst = 0x16;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

	for (a = 0; a < count; a++)
	{
		if ((reg[a].usb == usb)&&(reg[a].sensor == sensor))
		{
			if ((scantype < ST_NORMAL)||(scantype > ST_NEG))
				scantype = ST_NORMAL;

			rst = reg[a].pwm[scantype - 1];
			break;
		}
	}

	return rst;
}

static SANE_Int hp4370_fixedpwm(SANE_Int scantype, SANE_Int usb)
{
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int pwm[3];
	};

	struct st_reg reg[] =
	{
		/* usb  , { ST_NORMAL, ST_TA, ST_NEG} */
		{  USB20, {        20,    28,     28}},
		{  USB11, {        20,    28,     28}}
	};

	SANE_Int a, rst = 0x16;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

	for (a = 0; a < count; a++)
	{
		if (reg[a].usb == usb)
		{
			if ((scantype < ST_NORMAL)||(scantype > ST_NEG))
				scantype = ST_NORMAL;

			rst = reg[a].pwm[scantype - 1];
			break;
		}
	}

	return rst;
}

static SANE_Int ua4900_fixedpwm(SANE_Int scantype, SANE_Int usb)
{
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int pwm[3];
	};

	struct st_reg reg[] =
	{
		/* usb  , { ST_NORMAL, ST_TA, ST_NEG} */
		{  USB20, {        20,    28,     28}},
		{  USB11, {        20,    28,     28}}
	};

	SANE_Int a, rst = 0x16;
	SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

	for (a = 0; a < count; a++)
	{
		if (reg[a].usb == usb)
		{
			if ((scantype < ST_NORMAL)||(scantype > ST_NEG))
				scantype = ST_NORMAL;

			rst = reg[a].pwm[scantype - 1];
			break;
		}
	}

	return rst;
}

static SANE_Int cfg_fixedpwm_get(SANE_Int sensortype, SANE_Int scantype)
{
	SANE_Int rst;

	switch(RTS_Debug->dev_model)
	{
		case UA4900:
			rst = ua4900_fixedpwm(scantype, RTS_Debug->usbtype);
			break;

		case HP3800:
		case HPG2710:
			rst = hp3800_fixedpwm(scantype, RTS_Debug->usbtype);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			rst = hp4370_fixedpwm(scantype, RTS_Debug->usbtype);
			break;

		default:
			rst = hp3970_fixedpwm(scantype, RTS_Debug->usbtype, sensortype);
			break;
	}

	return rst;
}

/** SEC: Fixed reference positions ---------- */

static void cfg_vrefs_get(SANE_Int sensortype, SANE_Int res, SANE_Int *ser, SANE_Int *ler)
{
	switch(RTS_Debug->dev_model)
	{
		case HP3800:
		case HPG2710:
			hp3800_vrefs(res, ser, ler);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			hp4370_vrefs(res, ser, ler);
			break;

		default:
			hp3970_vrefs(RTS_Debug->usbtype, sensortype, res, ser, ler);
			break;
	}
}

static void hp3800_vrefs(SANE_Int res, SANE_Int *ser, SANE_Int *ler)
{
	struct st_reg
	{
		SANE_Int resolution;
		SANE_Int vref[2];
	};

	struct st_reg reg[] =
	{
		/* res, { ser,  ler} */
		{  150, {  25,   50}},
		{  300, {  50,  101}},
		{  600, { 102,  202}},
		{ 1200, { 204,  404}},
		{ 2400, { 408,  808}}
	};

	if ((ser != NULL)&&(ler != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		/* values by default */
		*ser = *ler = 0;

		for (a = 0; a < count; a++)
		{
			if (reg[a].resolution == res)
			{
				*ser = reg[a].vref[0];
				*ler = reg[a].vref[1];
				break;
			}
		}
	}
}

static void hp3970_vrefs(SANE_Int usb, SANE_Int sensor, SANE_Int res, SANE_Int *ser, SANE_Int *ler)
{
	struct st_reg
	{
		SANE_Int usb;
		SANE_Int sensor;
		SANE_Int resolution;
		SANE_Int vref[2];
	};

	/* I think these references should be the same in all usb versions and in
	   all sensor types but windows driver has some different values in some cases.
	*/

	struct st_reg reg[] =
	{
		/* usb  , sensor    ,  res, { ser,  ler} */
		{  USB20, CCD_SENSOR,  100, {  28,   60}},
		{  USB20, CCD_SENSOR,  200, {  37,  117}},
		{  USB20, CCD_SENSOR,  300, {  52,  162}},
		{  USB20, CCD_SENSOR,  600, { 103,  344}},
		{  USB20, CCD_SENSOR, 1200, { 156,  660}},
		{  USB20, CCD_SENSOR, 2400, { 309, 1262}},

		/* usb  , sensor    ,  res, { ser,  ler} */
		{  USB11, CCD_SENSOR,  100, {  28,   60}},
		{  USB11, CCD_SENSOR,  200, {  37,  117}},
		{  USB11, CCD_SENSOR,  300, {  52,  162}},
		{  USB11, CCD_SENSOR,  600, { 103,  344}},
		{  USB11, CCD_SENSOR, 1200, { 156,  660}},
		{  USB11, CCD_SENSOR, 2400, { 309, 1262}},

		/* usb  , sensor    ,  res, { ser,  ler} */
		{  USB20, CIS_SENSOR,  100, {  15,   60}},
		{  USB20, CIS_SENSOR,  200, {  39,  117}},
		{  USB20, CIS_SENSOR,  300, {  56,  161}},
		{  USB20, CIS_SENSOR,  600, { 108,  342}},
		{  USB20, CIS_SENSOR, 1200, { 221,  642}},
		{  USB20, CIS_SENSOR, 2400, { 407, 1285}},

		/* usb  , sensor    ,  res, { ser,  ler} */
		{  USB11, CIS_SENSOR,  100, {  15,   60}},
		{  USB11, CIS_SENSOR,  200, {  39,  117}},
		{  USB11, CIS_SENSOR,  300, {  56,  161}},
		{  USB11, CIS_SENSOR,  600, { 108,  342}},
		{  USB11, CIS_SENSOR, 1200, { 221,  642}},
		{  USB11, CIS_SENSOR, 2400, { 407, 1285}}
	};

	if ((ser != NULL)&&(ler != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		/* values by default */
		*ser = *ler = 0;

		for (a = 0; a < count; a++)
		{
			if ((reg[a].usb == usb)&&(reg[a].sensor == sensor)&&(reg[a].resolution == res))
			{
				*ser = reg[a].vref[0];
				*ler = reg[a].vref[1];
				break;
			}
		}
	}
}

static void hp4370_vrefs(SANE_Int res, SANE_Int *ser, SANE_Int *ler)
{
	struct st_reg
	{
		SANE_Int resolution;
		SANE_Int vref[2];
	};

	struct st_reg reg[] =
	{
		/* res, { ser,  ler} */
		{  150, {  31,   81}},
		{  300, {  61,  162}},
		{  600, { 122,  324}},
		{ 1200, { 244,  648}},
		{ 2400, { 488, 1256}},
		{ 4800, { 976, 2512}}
	};

	if ((ser != NULL)&&(ler != NULL))
	{
		SANE_Int a;
		SANE_Int count = sizeof(reg) / sizeof(struct st_reg);

		/* values by default */
		*ser = *ler = 0;

		for (a = 0; a < count; a++)
		{
			if (reg[a].resolution == res)
			{
				*ser = reg[a].vref[0];
				*ler = reg[a].vref[1];
				break;
			}
		}
	}
}

/** SEC: Motor movements ---------- */

static SANE_Int cfg_motormove_get(SANE_Int sensortype, SANE_Int item, struct st_motormove *reg)
{
	SANE_Int rst = ERROR;

	switch(RTS_Debug->dev_model)
	{
		case BQ5550:
			rst = bq5550_motormove(item, reg);
			break;
		case HP3800:
		case HPG2710:
			rst = hp3800_motormove(item, reg);
			break;

		default:
			rst = hp3970_motormove(RTS_Debug->usbtype, sensortype, item, reg);
			break;
	}

	return rst;
}

static SANE_Int bq5550_motormove(SANE_Int item, struct st_motormove *reg)
{
	SANE_Int rst = ERROR;

	/* data is the same in all usb types and sensors so those args aren't needed */

	if (reg != NULL)
	{
		struct st_motormove mv[] =
		{
			/* systemclock, ctpc, steptype , motorcurve } */
			{  0x05       , 4059, STT_HALF ,  0         },
			{  0x02       , 1200, STT_QUART, -1         }
		};

		rst = OK;

		if ((item < 2)&&(item > -1))
			memcpy(reg, &mv[item], sizeof(struct st_motormove));
				else rst = ERROR;
	}

	return rst;
}

static SANE_Int hp3800_motormove(SANE_Int item, struct st_motormove *reg)
{
	SANE_Int rst = ERROR;

	/* data is the same in all usb types and sensors so those args aren't needed */

	if (reg != NULL)
	{
		struct st_motormove mv[] =
		{
			/* systemclock, ctpc, steptype, motorcurve } */
			{  0x04       , 1991, STT_HALF,  2         },
			{  0x02       , 1991, STT_HALF, -1         }
		};

		rst = OK;

		if ((item < 2)&&(item > -1))
			memcpy(reg, &mv[item], sizeof(struct st_motormove));
				else rst = ERROR;
	}

	return rst;
}

static SANE_Int hp3970_motormove(SANE_Int usb, SANE_Int ccd, SANE_Int item, struct st_motormove *reg)
{
	SANE_Int rst = ERROR;

	struct st_mtmove
	{
		SANE_Int usbtype;
		SANE_Int sensor;
		struct st_motormove move;
	};

	if (reg != NULL)
	{
		struct st_mtmove mv[] =
		{
			/* usb, sensor    , {systemclock, ctpc, steptype, motorcurve } */
			{USB20, CCD_SENSOR, {0x02       , 6431, STT_HALF,  1         }},
			{USB20, CCD_SENSOR, {0x02       , 2000, STT_HALF, -1         }},

			{USB20, CIS_SENSOR, {0x02       , 6431, STT_HALF,  1         }},
			{USB20, CIS_SENSOR, {0x02       , 2000, STT_HALF, -1         }},

			{USB11, CCD_SENSOR, {0x02       , 6431, STT_HALF,  1         }},
			{USB11, CCD_SENSOR, {0x02       , 2000, STT_HALF, -1         }},

			{USB11, CIS_SENSOR, {0x02       , 6431, STT_HALF,  1         }},
			{USB11, CIS_SENSOR, {0x02       , 2000, STT_HALF, -1         }}
		};

		if (item < 2)
		{
			SANE_Int a, count = 0;
			SANE_Int total = sizeof(mv) / sizeof(struct st_mtmove);

			for (a = 0; a < total; a++)
			{
				if ((mv[a].usbtype == usb)&&(mv[a].sensor == ccd))
				{
					if (item == count)
					{
						memcpy(reg, &mv[a].move, sizeof(struct st_motormove));
						rst = OK;
						break;
					} else count++;
				}
			}
		}
	}

	return rst;
}

/** SEC: Scanning modes ---------- */

static SANE_Int cfg_scanmode_get(SANE_Int sensortype, SANE_Int sm, struct st_scanmode *mymode)
{
	SANE_Int rst = ERROR;

	switch(RTS_Debug->dev_model)
	{
		case BQ5550:
			rst = bq5550_scanmodes(RTS_Debug->usbtype, sm, mymode);
			break;

		case UA4900:
			rst = ua4900_scanmodes(RTS_Debug->usbtype, sm, mymode);
			break;

		case HP3800:
		case HPG2710:
			rst = hp3800_scanmodes(RTS_Debug->usbtype, sm, mymode);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			rst = hp4370_scanmodes(RTS_Debug->usbtype, sm, mymode);
			break;

		default: /* hp3970 hp4070 */
			rst = hp3970_scanmodes(RTS_Debug->usbtype, sensortype, sm, mymode);
			break;
	}

	return rst;
}

static SANE_Int hp3970_scanmodes(SANE_Int usb, SANE_Int ccd, SANE_Int sm, struct st_scanmode *mymode)
{
	struct st_modes
	{
		SANE_Int usb;
		SANE_Int sensor;
		struct st_scanmode mode;
	};

	struct st_modes reg[] =
	{
		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x04 , 24499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x05 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  600, 0x02  ,  2   , PIXEL_RATE, 0x04 ,  5499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  2751, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, CCD_SENSOR, {ST_NORMAL, CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x04 , 24499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x05 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  600, 0x07  ,  2   , LINE_RATE , 0x04 ,  5499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  2751, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, CCD_SENSOR, {ST_TA    , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, CCD_SENSOR, {ST_TA    , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt           }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, CCD_SENSOR, {ST_NEG   , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  300, 0x03  , -1   , PIXEL_RATE, 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  200, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  100, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, CCD_SENSOR, {ST_NEG   , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  300, 0x08  , -1   , LINE_RATE , 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  200, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  100, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x04 , 24499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x05 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_COLOR ,  600, 0x02  ,  2   , PIXEL_RATE, 0x04 ,  5499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  2751, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, CIS_SENSOR, {ST_NORMAL, CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x04 , 24499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x05 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_GRAY  ,  600, 0x07  ,  2   , LINE_RATE , 0x04 ,  5499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  2751, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NORMAL, CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, CIS_SENSOR, {ST_TA    , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, CIS_SENSOR, {ST_TA    , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_TA    , CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt           }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, CIS_SENSOR, {ST_NEG   , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_COLOR ,  300, 0x03  , -1   , PIXEL_RATE, 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_COLOR ,  200, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_COLOR ,  100, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, CIS_SENSOR, {ST_NEG   , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_GRAY  ,  300, 0x08  , -1   , LINE_RATE , 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_GRAY  ,  200, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, CIS_SENSOR, {ST_NEG   , CM_GRAY  ,  100, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x03 , 21499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x04 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x04 , 21999, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 , 10727, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  5591, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x02 , 21499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x04 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  600, 0x07  ,  2   , LINE_RATE , 0x04 ,  6999, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  4671, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt           }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  300, 0x03  , -1   , PIXEL_RATE, 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  200, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  100, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  300, 0x08  , -1   , LINE_RATE , 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  200, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  100, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x02 , 21499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x04 , 21999, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 , 10727, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  5591, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x02 , 21499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x04 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  600, 0x07  ,  2   , LINE_RATE , 0x04 ,  6999, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  4671, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NORMAL, CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  200, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x04 , 25599, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  200, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_TA    , CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, sensor    , {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt           }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  300, 0x03  , -1   , PIXEL_RATE, 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  200, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_COLOR ,  100, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, CIS_SENSOR, {ST_NEG   , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x02 , 76799, 256    , STT_FULL, 0x00     , {25599, 51199, 0}, {25599, 25599, 76799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CIS_SENSOR, {ST_NEG   , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  300, 0x08  , -1   , LINE_RATE , 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  200, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, CCD_SENSOR, {ST_NEG   , CM_GRAY  ,  100, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }}
	};

	SANE_Int rst = ERROR;

	if (mymode != NULL)
	{
		SANE_Int a;
		SANE_Int total = sizeof(reg) / sizeof(struct st_modes);
		SANE_Int count = 0;
		struct st_modes *md;

		for (a = 0; a < total; a++)
		{
			md = &reg[a];
			if ((md->usb == usb)&&(md->sensor == ccd))
			{
				if (count == sm)
				{
					memcpy(mymode, &md->mode, sizeof(struct st_scanmode));
					rst = OK;
					break;
				}

				count++;
			}
		}
	}

	return rst;
}

static SANE_Int hp4370_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode)
{
	struct st_modes
	{
		SANE_Int usb;
		struct st_scanmode mode;
	};

	struct st_modes reg[] =
	{
		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt               }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NORMAL, CM_COLOR , 4800, 0x00  , -1   , PIXEL_RATE, 0x05 , 47799, 256    , STT_HALF, 0x00     , {23899, 23899, 23899}, {23899, 23899, 23899},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR , 2400, 0x01  , -1   , PIXEL_RATE, 0x05 , 31849, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR , 1200, 0x02  , -1   , PIXEL_RATE, 0x05 , 15999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  600, 0x03  ,  2   , PIXEL_RATE, 0x05 ,  7999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  300, 0x04  ,  3   , PIXEL_RATE, 0x04 ,  2799, 768    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  150, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2799, 1856   , STT_HALF, 0x01     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NORMAL, CM_GRAY  , 4800, 0x05  , -1   , LINE_RATE , 0x05 , 47799, 256    , STT_HALF, 0x00     , {23899, 23899, 23899}, {23899, 23899, 23899},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  , 2400, 0x06  , -1   , LINE_RATE , 0x05 , 31849, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  , 1200, 0x07  , -1   , LINE_RATE , 0x05 , 15999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  600, 0x08  ,  2   , LINE_RATE , 0x05 ,  7999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  300, 0x09  ,  3   , LINE_RATE , 0x04 ,  2799, 768    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  150, 0x09  ,  4   , LINE_RATE , 0x04 ,  2799, 1856   , STT_HALF, 0x01     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt               }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_TA    , CM_COLOR , 4800, 0x0A  , -1   , PIXEL_RATE, 0x05 , 53999, 256    , STT_HALF, 0x00     , {26999, 26999, 26999}, {26999, 26999, 26999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR , 2400, 0x01  , -1   , PIXEL_RATE, 0x05 , 35999, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR , 1200, 0x02  , -1   , PIXEL_RATE, 0x05 , 17999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  600, 0x03  ,  2   , PIXEL_RATE, 0x05 ,  8999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  300, 0x04  ,  3   , PIXEL_RATE, 0x04 ,  2959, 512    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  150, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2959, 1024   , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_TA    , CM_GRAY  , 4800, 0x0B  , -1   , LINE_RATE , 0x05 , 53999, 256    , STT_HALF, 0x00     , {26999, 26999, 26999}, {26999, 26999, 26999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  , 2400, 0x06  , -1   , LINE_RATE , 0x05 , 35999, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  , 1200, 0x07  , -1   , LINE_RATE , 0x05 , 17999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  600, 0x08  ,  3   , LINE_RATE , 0x05 ,  8999, 512    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  300, 0x09  ,  4   , LINE_RATE , 0x04 ,  2959, 1024   , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  150, 0x09  ,  4   , LINE_RATE , 0x05 ,  2959, 1024   , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt               }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NEG   , CM_COLOR , 4800, 0x0C  , -1   , PIXEL_RATE, 0x05 , 60599, 256    , STT_HALF, 0x00     , {30299, 30299, 30299}, {30299, 30299, 30299},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR , 2400, 0x01  , -1   , PIXEL_RATE, 0x05 , 145799, 256   , STT_FULL, 0x00     , {48599, 97199,     0}, {48599, 48599, 145799},  0      ,  1     , -1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR , 1200, 0x02  , -1   , PIXEL_RATE, 0x05 , 89999, 256    , STT_FULL, 0x00     , {29999, 59999,     0}, {29999, 29999, 89999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  600, 0x03  , -1   , PIXEL_RATE, 0x05 , 45999, 256    , STT_HALF, 0x00     , {15333, 30666,     0}, {15333, 15333, 45999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  300, 0x04  , -1   , PIXEL_RATE, 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  150, 0x04  ,  3   , PIXEL_RATE, 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NEG   , CM_GRAY  , 4800, 0x0D  , -1   , LINE_RATE , 0x05 , 60599, 256    , STT_FULL, 0x00     , {30299, 30299, 30299}, {30299, 30299, 30299},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  , 2400, 0x06  , -1   , LINE_RATE , 0x05 ,145799,  256   , STT_FULL, 0x00     , {48599, 97199,     0}, {48599, 48599, 145799},  0      ,  1     , -1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  , 1200, 0x07  , -1   , LINE_RATE , 0x05 , 89999, 256    , STT_FULL, 0x00     , {29999, 59999,     0}, {29999, 29999, 89999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  600, 0x08  , -1   , LINE_RATE , 0x05 , 45999, 256    , STT_HALF, 0x00     , {15333, 30666,     0}, {15333, 15333, 45999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  300, 0x09  , -1   , LINE_RATE , 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  150, 0x09  ,  3   , LINE_RATE , 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt               }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, {ST_NORMAL, CM_COLOR , 4800, 0x00  , -1   , PIXEL_RATE, 0x05 , 47799, 64     , STT_HALF, 0x00     , {23899, 23899, 23899}, {23899, 23899, 23899},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR , 2400, 0x01  , -1   , PIXEL_RATE, 0x05 , 31849, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR , 1200, 0x02  , -1   , PIXEL_RATE, 0x05 , 15999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR ,  600, 0x03  ,  2   , PIXEL_RATE, 0x05 ,  7999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR ,  300, 0x04  ,  3   , PIXEL_RATE, 0x04 ,  2799, 768    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR ,  150, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2799, 1856   , STT_HALF, 0x01     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, {ST_NORMAL, CM_GRAY  , 4800, 0x05  , -1   , LINE_RATE , 0x05 , 47799, 64     , STT_HALF, 0x00     , {23899, 23899, 23899}, {23899, 23899, 23899},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  , 2400, 0x06  , -1   , LINE_RATE , 0x05 , 31849, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  , 1200, 0x07  , -1   , LINE_RATE , 0x05 , 15999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  ,  600, 0x08  ,  2   , LINE_RATE , 0x05 ,  7999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  ,  300, 0x09  ,  3   , LINE_RATE , 0x04 ,  2799, 768    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  ,  150, 0x09  ,  4   , LINE_RATE , 0x04 ,  2799, 1856   , STT_HALF, 0x01     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt               }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, {ST_TA    , CM_COLOR , 4800, 0x0A  , -1   , PIXEL_RATE, 0x05 , 53999, 64     , STT_HALF, 0x00     , {26999, 26999, 26999}, {26999, 26999, 26999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR , 2400, 0x01  , -1   , PIXEL_RATE, 0x05 , 35999, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR , 1200, 0x02  , -1   , PIXEL_RATE, 0x05 , 17999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR ,  600, 0x03  ,  2   , PIXEL_RATE, 0x05 ,  8999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR ,  300, 0x04  ,  3   , PIXEL_RATE, 0x04 ,  2959, 512    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR ,  150, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2959, 1024   , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, {ST_TA    , CM_GRAY  , 4800, 0x0B  , -1   , LINE_RATE , 0x05 , 53999, 64     , STT_HALF, 0x00     , {26999, 26999, 26999}, {26999, 26999, 26999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  , 2400, 0x06  , -1   , LINE_RATE , 0x05 , 35999, 256    , STT_FULL, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  , 1200, 0x07  , -1   , LINE_RATE , 0x05 , 17999, 256    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  ,  600, 0x08  ,  3   , LINE_RATE , 0x05 ,  8999, 512    , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  ,  300, 0x09  ,  4   , LINE_RATE , 0x04 ,  2959, 1024   , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  ,  150, 0x09  ,  4   , LINE_RATE , 0x05 ,  2959, 1024   , STT_HALF, 0x00     , {    0,     0,     0}, {    0,     0,     0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt               }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, {ST_NEG   , CM_COLOR , 4800, 0x0C  , -1   , PIXEL_RATE, 0x05 , 60599, 64     , STT_HALF, 0x00     , {30299, 30299, 30299}, {30299, 30299, 30299},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR , 2400, 0x01  , -1   , PIXEL_RATE, 0x05 ,145799, 256    , STT_FULL, 0x00     , {48599, 97199,     0}, {48599, 48599,145799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR , 1200, 0x02  , -1   , PIXEL_RATE, 0x05 , 89999, 256    , STT_FULL, 0x00     , {29999, 59999,     0}, {29999, 29999, 89999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR ,  600, 0x03  , -1   , PIXEL_RATE, 0x05 , 45999, 256    , STT_HALF, 0x00     , {15333, 30666,     0}, {15333, 15333, 45999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR ,  300, 0x04  , -1   , PIXEL_RATE, 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR ,  150, 0x04  ,  3   , PIXEL_RATE, 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, {ST_NEG   , CM_GRAY  , 4800, 0x0D  , -1   , LINE_RATE , 0x05 , 60599,  64    , STT_FULL, 0x00     , {30299, 30299, 30299}, {30299, 30299, 30299},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  , 2400, 0x06  , -1   , LINE_RATE , 0x05 ,145799, 256    , STT_FULL, 0x00     , {48599, 97199,     0}, {48599, 48599,145799},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  , 1200, 0x07  , -1   , LINE_RATE , 0x05 , 89999, 256    , STT_FULL, 0x00     , {29999, 59999,     0}, {29999, 29999, 59999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  ,  600, 0x08  , -1   , LINE_RATE , 0x05 , 45999, 256    , STT_HALF, 0x00     , {15333, 30666,     0}, {15333, 15333, 45999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  ,  300, 0x09  , -1   , LINE_RATE , 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  ,  150, 0x09  ,  3   , LINE_RATE , 0x04 , 14879, 256    , STT_HALF, 0x00     , { 4959,  9919,     0}, { 4959,  4959, 14879},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }}
	};

	SANE_Int rst = ERROR;

	if (mymode != NULL)
	{
		SANE_Int a;
		SANE_Int total = sizeof(reg) / sizeof(struct st_modes);
		SANE_Int count = 0;
		struct st_modes *md;

		for (a = 0; a < total; a++)
		{
			md = &reg[a];
			if (md->usb == usb)
			{
				if (count == sm)
				{
					memcpy(mymode, &md->mode, sizeof(struct st_scanmode));
					rst = OK;
					break;
				}

				count++;
			}
		}
	}

	return rst;
}

static SANE_Int hp3800_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode)
{
	struct st_modes
	{
		SANE_Int usb;
		struct st_scanmode mode;
	};

	struct st_modes reg[] =
	{
		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NORMAL, CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x05 , 23999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x05 , 23999, 96     , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x04 ,  7999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  300, 0x03  ,  9   , PIXEL_RATE, 0x04 ,  4999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  150, 0x03  , 10   , PIXEL_RATE, 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NORMAL, CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x05 , 23999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x05 , 23999, 96     , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x04 ,  7999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  300, 0x08  ,  9   , LINE_RATE , 0x04 ,  4999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  150, 0x08  , 10   , LINE_RATE , 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_TA    , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  600, 0x09  , -1   , PIXEL_RATE, 0x04 ,  5999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  2999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  150, 0x03  , 10   , PIXEL_RATE, 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_TA    , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x04 ,  5999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  2999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  150, 0x08  , 10   , LINE_RATE , 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt           }, {mexpt               }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NEG   , CM_COLOR , 2400, 0x0A  , -1   , PIXEL_RATE, 0x04 ,127999, 96     , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR , 1200, 0x0B  , -1   , PIXEL_RATE, 0x05 ,127999, 128    , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  600, 0x0C  , -1   , PIXEL_RATE, 0x04 , 31999, 128    , STT_HALF, 0x00     , {    0,     0, 0}, { 31999, 31999, 31999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  300, 0x0D  , -1   , PIXEL_RATE, 0x04 , 15999, 256    , STT_HALF, 0x00     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  150, 0x0D  , 10   , PIXEL_RATE, 0x04 , 15999, 1024   , STT_HALF, 0x01     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NEG   , CM_GRAY  , 2400, 0x0F  , -1   , LINE_RATE , 0x05 ,127999, 96     , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  , 1200, 0x10  , -1   , LINE_RATE , 0x05 ,127999, 96     , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  600, 0x11  , -1   , LINE_RATE , 0x04 , 31999, 128    , STT_HALF, 0x00     , {    0,     0, 0}, { 31999, 31999, 31999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  300, 0x12  , -1   , LINE_RATE , 0x04 , 15999, 256    , STT_HALF, 0x00     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  150, 0x12  , 10   , LINE_RATE , 0x04 , 15999, 1024   , STT_HALF, 0x01     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, {ST_NORMAL, CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x05 , 23999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  2     ,  5       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x05 , 23999, 96     , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  2     ,  3       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR ,  600, 0x02  , -1   , PIXEL_RATE, 0x04 ,  7999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR ,  300, 0x03  ,  9   , PIXEL_RATE, 0x04 ,  4999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_COLOR ,  150, 0x03  , 10   , PIXEL_RATE, 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, {ST_NORMAL, CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x05 , 23999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  2     ,  2       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x05 , 23999, 96     , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  2     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x04 ,  7999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  ,  300, 0x08  ,  9   , LINE_RATE , 0x04 ,  4999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NORMAL, CM_GRAY  ,  150, 0x08  , 10   , LINE_RATE , 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, {ST_TA    , CM_COLOR , 2400, 0x00  , -1   , PIXEL_RATE, 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR , 1200, 0x01  , -1   , PIXEL_RATE, 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR ,  600, 0x09  , -1   , PIXEL_RATE, 0x04 ,  5999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR ,  300, 0x03  ,  3   , PIXEL_RATE, 0x04 ,  2999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_COLOR ,  150, 0x03  , 10   , PIXEL_RATE, 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, {ST_TA    , CM_GRAY  , 2400, 0x05  , -1   , LINE_RATE , 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  , 1200, 0x06  , -1   , LINE_RATE , 0x05 , 23999, 96     , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  ,  600, 0x07  , -1   , LINE_RATE , 0x04 ,  5999, 128    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  ,  300, 0x08  ,  3   , LINE_RATE , 0x04 ,  2999, 256    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_TA    , CM_GRAY  ,  150, 0x08  , 10   , LINE_RATE , 0x04 ,  2999, 1024   , STT_HALF, 0x01     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt           }, {mexpt               }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB11, {ST_NEG   , CM_COLOR , 2400, 0x0A  , -1   , PIXEL_RATE, 0x04 ,127999, 96     , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR , 1200, 0x0B  , -1   , PIXEL_RATE, 0x05 ,127999, 128    , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR ,  600, 0x0C  , -1   , PIXEL_RATE, 0x04 , 31999, 128    , STT_HALF, 0x00     , {    0,     0, 0}, { 31999, 31999, 31999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR ,  300, 0x0D  , -1   , PIXEL_RATE, 0x04 , 15999, 256    , STT_HALF, 0x00     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_COLOR ,  150, 0x0D  , 10   , PIXEL_RATE, 0x04 , 15999, 1024   , STT_HALF, 0x01     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB11, {ST_NEG   , CM_GRAY  , 2400, 0x0F  , -1   , LINE_RATE , 0x05 ,127999, 96     , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  , 1200, 0x10  , -1   , LINE_RATE , 0x05 ,127999, 96     , STT_HALF, 0x00     , {    0,     0, 0}, {127999,127999,127999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  ,  600, 0x11  , -1   , LINE_RATE , 0x04 , 31999, 128    , STT_HALF, 0x00     , {    0,     0, 0}, { 31999, 31999, 31999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  ,  300, 0x12  , -1   , LINE_RATE , 0x04 , 15999, 256    , STT_HALF, 0x00     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB11, {ST_NEG   , CM_GRAY  ,  150, 0x12  , 10   , LINE_RATE , 0x04 , 15999, 1024   , STT_HALF, 0x01     , {    0,     0, 0}, { 15999, 15999, 15999},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }}
	};

	SANE_Int rst = ERROR;

	if (mymode != NULL)
	{
		SANE_Int a;
		SANE_Int total = sizeof(reg) / sizeof(struct st_modes);
		SANE_Int count = 0;
		struct st_modes *md;

		for (a = 0; a < total; a++)
		{
			md = &reg[a];
			if (md->usb == usb)
			{
				if (count == sm)
				{
					memcpy(mymode, &md->mode, sizeof(struct st_scanmode));
					rst = OK;
					break;
				}

				count++;
			}
		}
	}

	return rst;
}

static SANE_Int bq5550_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode)
{
	struct st_modes
	{
		SANE_Int usb;
		struct st_scanmode mode;
	};

	struct st_modes reg[] =
	{
		/* usb, {scantype , colormode , res , timing, curve, samplerate, clock, ctpc , backstp, steptype , dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NORMAL, CM_COLOR  , 1200, 0x00  , -1   , PIXEL_RATE, 0x05 , 12999, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  3       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR  ,  600, 0x01  , -1   , PIXEL_RATE, 0x05 ,  6999, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  5       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR  ,  300, 0x02  ,  0   , PIXEL_RATE, 0x05 ,  5599, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  3       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR  ,  150, 0x02  ,  0   , PIXEL_RATE, 0x05 ,  6439, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  2       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR  ,  100, 0x02  ,  0   , PIXEL_RATE, 0x05 ,  5759, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NORMAL, CM_GRAY   , 1200, 0x03  , -1   , LINE_RATE , 0x05 , 12999, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  3       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY   ,  600, 0x04  , -1   , LINE_RATE , 0x05 ,  7199, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  2       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY   ,  300, 0x05  ,  0   , LINE_RATE , 0x05 ,  5599, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY   ,  150, 0x05  ,  0   , LINE_RATE , 0x05 ,  6239, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY   ,  100, 0x05  ,  0   , LINE_RATE , 0x05 ,  5759, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NORMAL, CM_LINEART, 1200, 0x03  , -1   , LINE_RATE , 0x05 , 12999, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  3       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_LINEART,  600, 0x04  , -1   , LINE_RATE , 0x05 ,  7199, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  2       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_LINEART,  300, 0x05  ,  0   , LINE_RATE , 0x05 ,  5599, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_LINEART,  150, 0x05  ,  0   , LINE_RATE , 0x05 ,  6239, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_LINEART,  100, 0x05  ,  0   , LINE_RATE , 0x05 ,  5759, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode , res , timing, curve, samplerate, clock, ctpc , backstp, steptype , dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_TA    , CM_COLOR  , 1200, 0x00  , -1   , PIXEL_RATE, 0x05 ,  9899, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR  ,  600, 0x01  , -1   , PIXEL_RATE, 0x05 ,  9999, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR  ,  300, 0x02  ,  0   , PIXEL_RATE, 0x05 ,  4799, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR  ,  150, 0x02  ,  0   , PIXEL_RATE, 0x05 ,  4959, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR  ,  100, 0x02  ,  0   , PIXEL_RATE, 0x05 ,  5059, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_TA    , CM_GRAY   , 1200, 0x03  , -1   , LINE_RATE , 0x05 ,  9899, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY   ,  600, 0x04  , -1   , LINE_RATE , 0x05 ,  9999, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY   ,  300, 0x05  ,  0   , LINE_RATE , 0x05 ,  4799, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY   ,  150, 0x05  ,  0   , LINE_RATE , 0x05 ,  4959, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY   ,  100, 0x05  ,  0   , LINE_RATE , 0x05 ,  5059, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_TA    , CM_LINEART, 1200, 0x03  , -1   , LINE_RATE , 0x05 ,  9899, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_LINEART,  600, 0x04  , -1   , LINE_RATE , 0x05 ,  9999, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_LINEART,  300, 0x05  ,  0   , LINE_RATE , 0x05 ,  4799, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_LINEART,  150, 0x05  ,  0   , LINE_RATE , 0x05 ,  4959, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_LINEART,  100, 0x05  ,  0   , LINE_RATE , 0x05 ,  5059, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode , res , timing, curve, samplerate, clock, ctpc , backstp, steptype , dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NEG   , CM_COLOR  , 1200, 0x00  , -1   , PIXEL_RATE, 0x05 , 51899, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR  ,  600, 0x01  , -1   , PIXEL_RATE, 0x05 , 51799, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR  ,  300, 0x02  ,  0   , PIXEL_RATE, 0x05 , 25899, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR  ,  150, 0x02  ,  0   , PIXEL_RATE, 0x05 , 25899, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR  ,  100, 0x02  ,  0   , PIXEL_RATE, 0x05 , 25499, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NEG   , CM_GRAY   , 1200, 0x03  , -1   , LINE_RATE , 0x05 , 51899, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY   ,  600, 0x04  , -1   , LINE_RATE , 0x05 , 51799, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY   ,  300, 0x05  ,  0   , LINE_RATE , 0x05 , 25899, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY   ,  150, 0x05  ,  0   , LINE_RATE , 0x05 , 25899, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY   ,  100, 0x05  ,  0   , LINE_RATE , 0x05 , 25499, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NEG   , CM_LINEART, 1200, 0x03  , -1   , LINE_RATE , 0x05 , 51899, 10     , STT_QUART, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_LINEART,  600, 0x04  , -1   , LINE_RATE , 0x05 , 51799, 32     , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_LINEART,  300, 0x05  ,  0   , LINE_RATE , 0x05 , 25899, 128    , STT_FULL , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_LINEART,  150, 0x05  ,  0   , LINE_RATE , 0x05 , 25899, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_LINEART,  100, 0x05  ,  0   , LINE_RATE , 0x05 , 25499, 128    , STT_HALF , 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }}
	};

	SANE_Int rst = ERROR;

	/* silence compiler */
	usb = usb;

	if (mymode != NULL)
	{
		SANE_Int a;
		SANE_Int total = sizeof(reg) / sizeof(struct st_modes);
		SANE_Int count = 0;
		struct st_modes *md;

		for (a = 0; a < total; a++)
		{
			md = &reg[a];
			if (count == sm)
			{
				memcpy(mymode, &md->mode, sizeof(struct st_scanmode));
				rst = OK;
				break;
			}

			count++;
		}
	}

	return rst;
}

static SANE_Int ua4900_scanmodes(SANE_Int usb, SANE_Int sm, struct st_scanmode *mymode)
{
	struct st_modes
	{
		SANE_Int usb;
		struct st_scanmode mode;
	};

	struct st_modes reg[] =
	{
		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NORMAL, CM_COLOR , 1200, 0x00  , -1   , PIXEL_RATE, 0x05 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  600, 0x01  ,  2   , PIXEL_RATE, 0x04 ,  5499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  300, 0x02  ,  3   , PIXEL_RATE, 0x04 ,  2751, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  200, 0x03  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NORMAL, CM_GRAY  , 1200, 0x05  , -1   , LINE_RATE , 0x05 , 14667, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  600, 0x06  ,  2   , LINE_RATE , 0x04 ,  5499, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  300, 0x07  ,  3   , LINE_RATE , 0x04 ,  2751, 768    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  200, 0x08  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NORMAL, CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  2255, 1856   , STT_HALF, 0x02     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt      }, {mexpt     }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_TA    , CM_COLOR , 1200, 0x00  , -1   , PIXEL_RATE, 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  600, 0x01  , -1   , PIXEL_RATE, 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  300, 0x02  ,  3   , PIXEL_RATE, 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  200, 0x03  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_COLOR ,  100, 0x04  ,  4   , PIXEL_RATE, 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_TA    , CM_GRAY  , 1200, 0x05  , -1   , LINE_RATE , 0x03 , 10899, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  600, 0x06  , -1   , LINE_RATE , 0x01 ,  5487, 256    , STT_FULL, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  300, 0x07  ,  3   , LINE_RATE , 0x04 ,  8351, 512    , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  200, 0x08  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_TA    , CM_GRAY  ,  100, 0x09  ,  4   , LINE_RATE , 0x04 ,  7343, 1024   , STT_HALF, 0x00     , { 0,  0,  0}, { 0,  0,  0},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		/* usb, {scantype , colormode, res , timing, curve, samplerate, clock, ctpc , backstp, steptype, dummyline, {expt           }, {mexpt              }, motorplus, mexpt16, mexptfull, mexposure, mri , msi , mmtir, mmtirh, skips } */
		{USB20, {ST_NEG   , CM_COLOR , 1200, 0x00  , -1   , PIXEL_RATE, 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  600, 0x01  , -1   , PIXEL_RATE, 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  300, 0x02  , -1   , PIXEL_RATE, 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  200, 0x03  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_COLOR ,  100, 0x04  , -1   , PIXEL_RATE, 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},

		{USB20, {ST_NEG   , CM_GRAY  , 1200, 0x05  , -1   , LINE_RATE , 0x03 , 36467, 256    , STT_FULL, 0x00     , {12155, 24311, 0}, {12155, 12155, 36467},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  600, 0x06  , -1   , LINE_RATE , 0x02 , 16463, 256    , STT_FULL, 0x00     , { 5487, 10975, 0}, { 5487,  5487, 16463},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  300, 0x07  , -1   , LINE_RATE , 0x02 ,  8351, 256    , STT_HALF, 0x00     , { 2783,  5567, 0}, { 2783,  2783,  8351},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  200, 0x08  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x00     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }},
		{USB20, {ST_NEG   , CM_GRAY  ,  100, 0x09  , -1   , LINE_RATE , 0x02 ,  6191, 256    , STT_HALF, 0x02     , { 2063,  4127, 0}, { 2063,  2063,  6191},  0       ,  1     ,  1       , 0x01     , 0x01, 0x10, 0x02 , 0x02  , 0x00  }}
	};

	SANE_Int rst = ERROR;

	if (mymode != NULL)
	{
		SANE_Int a;
		SANE_Int total = sizeof(reg) / sizeof(struct st_modes);
		SANE_Int count = 0;
		struct st_modes *md;

		for (a = 0; a < total; a++)
		{
			md = &reg[a];
			if (md->usb == usb)
			{
				if (count == sm)
				{
					memcpy(mymode, &md->mode, sizeof(struct st_scanmode));
					rst = OK;
					break;
				}

				count++;
			}
		}
	}

	return rst;
}

/** SEC: Calibration wreferences ---------- */

static void hp3970_wrefs(SANE_Int usb, SANE_Int ccd, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	struct st_wref
	{
		SANE_Int usb;
		SANE_Int sensor;
		SANE_Int depth;
		SANE_Int res;
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	struct st_wref wrefs[] =
	{
		/*usb , sensor     , depth, res , {transparent  }, {negative} */
		{USB20, CCD_SENSOR, 8    , 2400, { 78,  78,  68}, {120, 136, 157}},
		{USB20, CCD_SENSOR, 8    , 1200, { 78,  78,  68}, {120, 136, 157}},
		{USB20, CCD_SENSOR, 8    , 600 , { 79,  80,  70}, {119, 136, 157}},
		{USB20, CCD_SENSOR, 8    , 300 , { 79,  80,  70}, {119, 136, 157}},
		{USB20, CCD_SENSOR, 8    , 200 , { 79,  80,  70}, {119, 136, 157}},
		{USB20, CCD_SENSOR, 8    , 100 , { 79,  80,  70}, {119, 136, 157}},

		{USB20, CCD_SENSOR, 16   , 2400, { 81,  81,  71}, {119, 137, 158}},
		{USB20, CCD_SENSOR, 16   , 1200, { 81,  81,  71}, {119, 137, 158}},
		{USB20, CCD_SENSOR, 16   , 600 , { 81,  82,  72}, {119, 137, 158}},
		{USB20, CCD_SENSOR, 16   , 300 , { 81,  82,  72}, {119, 137, 158}},
		{USB20, CCD_SENSOR, 16   , 200 , { 81,  82,  72}, {119, 137, 158}},
		{USB20, CCD_SENSOR, 16   , 100 , { 81,  82,  72}, {119, 137, 158}},

		/*usb , sensor    , depth, res , {transparent  }, {negative     } */
		{USB20, CIS_SENSOR, 8    , 2400, { 94,  85,  78}, { 94,  85,  78}},
		{USB20, CIS_SENSOR, 8    , 1200, { 83,  82,  75}, {140, 155, 155}},
		{USB20, CIS_SENSOR, 8    , 600 , { 84,  84,  76}, {145, 155, 165}},
		{USB20, CIS_SENSOR, 8    , 300 , {146, 166, 166}, {146, 166, 166}},
		{USB20, CIS_SENSOR, 8    , 200 , {145, 310, 160}, {145, 310, 160}},
		{USB20, CIS_SENSOR, 8    , 100 , {140, 300, 155}, {140, 300, 155}},

		{USB20, CIS_SENSOR, 16   , 2400, { 94,  85,  78}, { 94,  85,  78}},
		{USB20, CIS_SENSOR, 16   , 1200, { 83,  82,  75}, {140, 155, 155}},
		{USB20, CIS_SENSOR, 16   , 600 , { 84,  84,  76}, {145, 155, 165}},
		{USB20, CIS_SENSOR, 16   , 300 , {146, 166, 166}, {146, 166, 166}},
		{USB20, CIS_SENSOR, 16   , 200 , {145, 310, 160}, {145, 310, 160}},
		{USB20, CIS_SENSOR, 16   , 100 , {140, 300, 155}, {140, 300, 155}},

		/*usb , sensor     , depth, res , {transparent  }, {negative     } */
		{USB11, CCD_SENSOR, 8    , 2400, { 78,  78,  68}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 8    , 1200, { 78,  78,  68}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 8    , 600 , { 79,  79,  70}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 8    , 300 , { 79,  79,  70}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 8    , 200 , { 79,  79,  70}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 8    , 100 , { 79,  79,  70}, {120, 137, 158}},

		{USB11, CCD_SENSOR, 16   , 2400, { 80,  80,  70}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 16   , 1200, { 80,  80,  70}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 16   , 600 , { 80,  81,  71}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 16   , 300 , { 80,  81,  71}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 16   , 200 , { 80,  81,  71}, {120, 137, 158}},
		{USB11, CCD_SENSOR, 16   , 100 , { 80,  81,  71}, {120, 137, 158}},

		/*usb , sensor    , depth, res , {transparent  }, {negative} */
		{USB11, CIS_SENSOR, 8    , 2400, { 94,  85,  78}, {94 , 85 ,  78}},
		{USB11, CIS_SENSOR, 8    , 1200, { 83,  82,  75}, {140, 155, 155}},
		{USB11, CIS_SENSOR, 8    , 600 , { 84,  84,  76}, {145, 155, 165}},
		{USB11, CIS_SENSOR, 8    , 300 , {146, 166, 166}, {146, 166, 166}},
		{USB11, CIS_SENSOR, 8    , 200 , {145, 310, 160}, {145, 310, 160}},
		{USB11, CIS_SENSOR, 8    , 100 , {140, 300, 155}, {140, 300, 155}},

		{USB11, CIS_SENSOR, 16   , 2400, { 94,  85,  78}, { 94,  85,  78}},
		{USB11, CIS_SENSOR, 16   , 1200, { 83,  82,  75}, {140, 155, 155}},
		{USB11, CIS_SENSOR, 16   , 600 , { 84,  84,  76}, {145, 155, 165}},
		{USB11, CIS_SENSOR, 16   , 300 , {146, 166, 166}, {146, 166, 166}},
		{USB11, CIS_SENSOR, 16   , 200 , {145, 310, 160}, {145, 310, 160}},
		{USB11, CIS_SENSOR, 16   , 100 , {140, 300, 155}, {140, 300, 155}}
	};

	struct st_wref *rf;

	*red = *green = *blue = 0x50;

	if (res <= 100)
		res = 100;
	else if (res <= 200)
		res = 200;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else if (res <= 1200)
		res = 1200;
	else res = 2400;

	if (scantype != ST_NORMAL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(wrefs) / sizeof(struct st_wref);

		for (a = 0; a < count; a++)
		{
			rf = &wrefs[a];
			if ((rf->usb == usb)&&(rf->sensor == ccd)&&(rf->depth == depth)&&(rf->res == res))
			{
				switch(scantype)
				{
					case ST_NEG:
						*red   = rf->negative[CL_RED];
						*green = rf->negative[CL_GREEN];
						*blue  = rf->negative[CL_BLUE];
						break;
					case ST_TA:
						*red   = rf->transparent[CL_RED];
						*green = rf->transparent[CL_GREEN];
						*blue  = rf->transparent[CL_BLUE];
						break;
				}

				break;
			}
		}
	} else
	{
		/* reflective mode */
		*red   = 233;
		*green = 230;
		*blue  = 222;
	}
}

static void ua4900_wrefs(SANE_Int usb, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	struct st_wref
	{
		SANE_Int usb;
		SANE_Int depth;
		SANE_Int res;
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	struct st_wref wrefs[] =
	{
		/*usb , depth, res , {transparent  }, {negative     } */
		{USB20, 8    , 1200, {136, 131, 121}, {215, 210, 260}},
		{USB20, 8    , 600 , {133, 129, 118}, {219, 215, 264}},
		{USB20, 8    , 300 , {133, 129, 119}, {218, 215, 261}},
		{USB20, 8    , 200 , {133, 130, 119}, {218, 215, 261}},
		{USB20, 8    , 100 , {132, 128, 118}, {218, 215, 261}},

		{USB20, 16   , 1200, {264, 263, 266}, {  0,   0,   0}},
		{USB20, 16   , 600 , {264, 265, 268}, {  0,   0,   0}},
		{USB20, 16   , 300 , {264, 260, 263}, {  0,   0,   0}},
		{USB20, 16   , 200 , {257, 255, 253}, {  0,   0,   0}},
		{USB20, 16   , 100 , {258, 259, 256}, {  0,   0,   0}},

		/*usb , depth, res , {transparent  }, {negative     } */
		{USB11, 8    , 1200, {135, 130, 119}, {220, 215, 264}},
		{USB11, 8    , 600 , {132, 128, 117}, {217, 213, 259}},
		{USB11, 8    , 300 , {132, 128, 117}, {216, 211, 259}},
		{USB11, 8    , 200 , {132, 128, 118}, {215, 352, 259}},
		{USB11, 8    , 100 , {266, 264, 264}, {215, 352, 259}},

		{USB11, 16   , 1200, {264, 263, 266}, {  0,   0,   0}},
		{USB11, 16   , 600 , {257, 256, 253}, {  0,   0,   0}},
		{USB11, 16   , 300 , {259, 252, 254}, {  0,   0,   0}},
		{USB11, 16   , 200 , {257, 255, 253}, {  0,   0,   0}},
		{USB11, 16   , 100 , {258, 259, 256}, {  0,   0,   0}}
	};

	struct st_wref *rf;

	*red = *green = *blue = 0x50;

	if (res <= 100)
		res = 100;
	else if (res <= 200)
		res = 200;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else res = 1200;

	if (scantype != ST_NORMAL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(wrefs) / sizeof(struct st_wref);

		for (a = 0; a < count; a++)
		{
			rf = &wrefs[a];
			if ((rf->usb == usb)&&(rf->depth == depth)&&(rf->res == res))
			{
				switch(scantype)
				{
					case ST_NEG:
						*red   = rf->negative[CL_RED];
						*green = rf->negative[CL_GREEN];
						*blue  = rf->negative[CL_BLUE];
						break;
					case ST_TA:
						*red   = rf->transparent[CL_RED];
						*green = rf->transparent[CL_GREEN];
						*blue  = rf->transparent[CL_BLUE];
						break;
				}

				break;
			}
		}
	} else
	{
		/* reflective mode */
		*red   = 233;
		*green = 230;
		*blue  = 222;
	}
}

static void hp3800_wrefs(SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	/* in this scanner, values are the same in all usb versions and depths */
	struct st_wref
	{
		SANE_Int res;
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	struct st_wref wrefs[] =
	{
		/* res , {transparent  }, {negative     } */
		{  2400, {276, 279, 243}, { 98, 162, 229}},
		{  1200, {276, 279, 243}, { 98, 162, 229}},
		{   600, {276, 279, 243}, {100, 162, 229}},
		{   300, {276, 279, 243}, {100, 162, 229}},
		{   150, {276, 279, 243}, {100, 162, 229}},
	};

	struct st_wref *rf;

	*red = *green = *blue = 0x50;

	if (res <= 150)
		res = 150;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else if (res <= 1200)
		res = 1200;
	else res = 2400;

	if (scantype != ST_NORMAL)
	{
		SANE_Int a;
		SANE_Int count = sizeof(wrefs) / sizeof(struct st_wref);

		for (a = 0; a < count; a++)
		{
			rf = &wrefs[a];
			if (rf->res == res)
			{
				switch(scantype)
				{
					case ST_NEG:
						*red   = rf->negative[CL_RED];
						*green = rf->negative[CL_GREEN];
						*blue  = rf->negative[CL_BLUE];
						break;
					case ST_TA:
						*red   = rf->transparent[CL_RED];
						*green = rf->transparent[CL_GREEN];
						*blue  = rf->transparent[CL_BLUE];
						break;
				}

				break;
			}
		}
	} else
	{
		/* reflective mode */
		*red   = 248;
		*green = 250;
		*blue  = 248;
	}
}

static void hp4370_wrefs(SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	struct st_wref
	{
		SANE_Int res;
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	/* values are the same in all usb versions and depths */

	struct st_wref wrefs[] =
	{
		/* res, {transparent  }, {negative} */
		{ 4800, { 93,  93,  82}, {156, 308, 454}},
		{ 2400, { 93,  93,  82}, {156, 156, 153}},
		{ 1200, { 93,  93,  82}, {156, 156, 153}},
		{  600, { 93,  93,  82}, {155, 155, 152}},
		{  300, { 94,  94,  83}, {155, 155, 152}},
		{  150, { 86,  87,  77}, {148, 145, 138}}
	};

	struct st_wref *rf;

	SANE_Int a;

	*red = *green = *blue = 0x50;

	if (res <= 150)
		res = 150;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else if (res <= 1200)
		res = 1200;
	else if (res <= 2400)
		res = 2400;
	else res = 4800;

	if (scantype != ST_NORMAL)
	{
		SANE_Int count = sizeof(wrefs) / sizeof(struct st_wref);

		for (a = 0; a < count; a++)
		{
			rf = &wrefs[a];
			if (rf->res == res)
			{
				switch(scantype)
				{
					case ST_NEG:
						*red   = rf->negative[CL_RED];
						*green = rf->negative[CL_GREEN];
						*blue  = rf->negative[CL_BLUE];
						break;
					case ST_TA:
						*red   = rf->transparent[CL_RED];
						*green = rf->transparent[CL_GREEN];
						*blue  = rf->transparent[CL_BLUE];
						break;
				}

				break;
			}
		}
	} else
	{
		/* reflective mode */
		*red   = 233;
		*green = 232;
		*blue  = 223;
	}
}

static void cfg_wrefs_get(SANE_Int sensortype, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	switch(RTS_Debug->dev_model)
	{
		case UA4900:
			ua4900_wrefs(RTS_Debug->usbtype, depth, res, scantype, red, green, blue);
			break;

		case HP3800:
		case HPG2710:
			hp3800_wrefs(res, scantype, red, green, blue);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			hp4370_wrefs(res, scantype, red, green, blue);
			break;

		default:
			/* hp3970 and hp4070 */
			hp3970_wrefs(RTS_Debug->usbtype, sensortype, depth, res, scantype, red, green, blue);
			break;
	}
}

static void hp3800_shading_cut(SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	/*  values are the same in all usb versions and depths */
	struct st_cut
	{
		SANE_Int res;
		SANE_Int reflective[3];
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	struct st_cut cuts[] =
	{
		/* res, {reflective }, {transparent  }, {negative       } */
		{ 2400, { -6, -6, -6}, {-75, -75, -75}, {440, -300, -250}},
		{ 1200, { -6, -6, -6}, {-75, -75, -75}, {440, -300, -250}},
		{  600, { 10,  9, 12}, {-75, -75, -75}, {440, -300, -250}},
		{  300, { -6, -6, -6}, {-75, -75, -75}, {440, -300, -250}},
		{  150, { -6, -6, -6}, {-75, -75, -75}, {440, -300, -250}}
	};

	struct st_cut *ct;

	SANE_Int a;
	SANE_Int count = sizeof(cuts) / sizeof(struct st_cut);

	*red = *green = *blue = 0;

	if (res <= 150)
		res = 150;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else if (res <= 1200)
		res = 1200;
	else res = 2400;

	for (a = 0; a < count; a++)
	{
		ct = &cuts[a];
		if (ct->res == res)
		{
			switch(scantype)
			{
				case ST_NEG:
					*red   = ct->negative[CL_RED];
					*green = ct->negative[CL_GREEN];
					*blue  = ct->negative[CL_BLUE];
					break;
				case ST_TA:
					*red   = ct->transparent[CL_RED];
					*green = ct->transparent[CL_GREEN];
					*blue  = ct->transparent[CL_BLUE];
					break;
				default: /* reflective */
					*red   = ct->reflective[CL_RED];
					*green = ct->reflective[CL_GREEN];
					*blue  = ct->reflective[CL_BLUE];
					break;
			}
			break;
		}
	}
}

static void hp3970_shading_cut(SANE_Int usb, SANE_Int ccd, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	struct st_cut
	{
		SANE_Int usb;
		SANE_Int sensor;
		SANE_Int depth;
		SANE_Int res;
		SANE_Int reflective[3];
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	struct st_cut cuts[] =
	{
		/*usb , sensor     , depth, res , {reflective   }, {transparent  }, {negative} */
		{USB20, CCD_SENSOR, 8    , 2400, {-15, -15, -15}, {  0,   0,   0}, {63, 0, 3}},
		{USB20, CCD_SENSOR, 8    , 1200, {-15, -15, -15}, {  0,   0,   0}, {63, 0, 5}},
		{USB20, CCD_SENSOR, 8    , 600 , {-15, -15, -15}, {  0,   0,   0}, {75, 0, 5}},
		{USB20, CCD_SENSOR, 8    , 300 , {-15, -15, -15}, {  0,   0,   0}, {75, 0, 5}},
		{USB20, CCD_SENSOR, 8    , 200 , {-15, -15, -15}, {  0,   0,   0}, {75, 0, 5}},
		{USB20, CCD_SENSOR, 8    , 100 , {-15, -15, -15}, {  0,   0,   0}, {75, 0, 5}},

		{USB20, CCD_SENSOR, 16   , 2400, {-15, -15, -15}, { -7,  -7,  -7}, {43, 0, 3}},
		{USB20, CCD_SENSOR, 16   , 1200, {-15, -15, -15}, {-15, -15, -15}, {63, 0, 5}},
		{USB20, CCD_SENSOR, 16   , 600 , {-15, -15, -15}, {-15, -15, -15}, {75, 0, 5}},
		{USB20, CCD_SENSOR, 16   , 300 , {-15, -15, -15}, {-15, -15, -15}, {75, 0, 5}},
		{USB20, CCD_SENSOR, 16   , 200 , {-15, -15, -15}, {-15, -15, -15}, {75, 0, 5}},
		{USB20, CCD_SENSOR, 16   , 100 , {-15, -15, -15}, {-15, -15, -15}, {75, 0, 5}},

		/*usb , sensor    , depth, res , {reflective   }, {transparent  }, {negative} */
		{USB20, CIS_SENSOR, 8    , 2400, {-15, -15, -15}, {-68, -37, -43}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 8    , 1200, {-15, -15, -15}, {-20, -20, -28}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 8    , 600 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 8    , 300 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 8    , 200 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 8    , 100 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},

		{USB20, CIS_SENSOR, 16   , 2400, {-15, -15, -15}, {-68, -37, -43}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 16   , 1200, {-15, -15, -15}, {-20, -20, -28}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 16   , 600 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 16   , 300 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 16   , 200 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB20, CIS_SENSOR, 16   , 100 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},

		/*usb , sensor     , depth, res , {reflective   }, {transparent  }, {negative} */
		{USB11, CCD_SENSOR, 8    , 2400, {-15, -15, -15}, {  0,   0,   0}, {63, 0, 3}},
		{USB11, CCD_SENSOR, 8    , 1200, {-15, -15, -15}, {  0,   0,   0}, {63, 0, 5}},
		{USB11, CCD_SENSOR, 8    , 600 , {-15, -15, -15}, {  0,   0,   0}, {50, 0, 5}},
		{USB11, CCD_SENSOR, 8    , 300 , {-15, -15, -15}, {  0,   0,   0}, {50, 0, 5}},
		{USB11, CCD_SENSOR, 8    , 200 , {-15, -15, -15}, {  0,   0,   0}, {50, 0, 5}},
		{USB11, CCD_SENSOR, 8    , 100 , {-15, -15, -15}, {  0,   0,   0}, {50, 0, 5}},

		{USB11, CCD_SENSOR, 16   , 2400, {-15, -15, -15}, { -7,  -7,  -7}, {43, 0, 3}},
		{USB11, CCD_SENSOR, 16   , 1200, {-15, -15, -15}, {-15, -15, -15}, {63, 0, 5}},
		{USB11, CCD_SENSOR, 16   , 600 , {-15, -15, -15}, {-15, -15, -15}, {50, 0, 5}},
		{USB11, CCD_SENSOR, 16   , 300 , {-15, -15, -15}, {-15, -15, -15}, {50, 0, 5}},
		{USB11, CCD_SENSOR, 16   , 200 , {-15, -15, -15}, {-15, -15, -15}, {50, 0, 5}},
		{USB11, CCD_SENSOR, 16   , 100 , {-15, -15, -15}, {-15, -15, -15}, {50, 0, 5}},

		/*usb , sensor    , depth, res , {reflective   }, {transparent  }, {negative} */
		{USB11, CIS_SENSOR, 8    , 2400, {-15, -15, -15}, {-68, -37, -43}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 8    , 1200, {-15, -15, -15}, {-20, -20, -28}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 8    , 600 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 8    , 300 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 8    , 200 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 8    , 100 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},

		{USB11, CIS_SENSOR, 16   , 2400, {-15, -15, -15}, {-68, -37, -43}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 16   , 1200, {-15, -15, -15}, {-20, -20, -28}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 16   , 600 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 16   , 300 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 16   , 200 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}},
		{USB11, CIS_SENSOR, 16   , 100 , {-15, -15, -15}, {  0,   0,   0}, { 0, 0, 0}}
	};

	struct st_cut *ct;

	SANE_Int a;
	SANE_Int count = sizeof(cuts) / sizeof(struct st_cut);

	*red = *green = *blue = 0;

	if (res <= 100)
		res = 100;
	else if (res <= 200)
		res = 200;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else if (res <= 1200)
		res = 1200;
	else res = 2400;

	for (a = 0; a < count; a++)
	{
		ct = &cuts[a];
		if ((ct->usb == usb)&&(ct->sensor == ccd)&&(ct->depth == depth)&&(ct->res == res))
		{
			switch(scantype)
			{
				case ST_NEG:
					*red   = ct->negative[CL_RED];
					*green = ct->negative[CL_GREEN];
					*blue  = ct->negative[CL_BLUE];
					break;
				case ST_TA:
					*red   = ct->transparent[CL_RED];
					*green = ct->transparent[CL_GREEN];
					*blue  = ct->transparent[CL_BLUE];
					break;
				default: /* reflective */
					*red   = ct->reflective[CL_RED];
					*green = ct->reflective[CL_GREEN];
					*blue  = ct->reflective[CL_BLUE];
					break;
			}
			break;
		}
	}
}

static void hp4370_shading_cut(SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	struct st_cut
	{
		SANE_Int depth;
		SANE_Int res;
		SANE_Int reflective[3];
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	/* values are the same in all usb versions for each depth */
	struct st_cut cuts[] =
	{
		/* depth, res , {reflective   }, {transparent  }, {negative    } */
		{    8  , 4800, {  0,   0,   0}, { -3,  -3,  -3}, {35, -10,  -7}},
		{    8  , 2400, {-15, -15, -15}, { -3,  -3,  -3}, {35, -10,  -7}},
		{    8  , 1200, {-15, -15, -15}, { -3,  -3,  -3}, {35, -10, -22}},
		{    8  , 600 , {-15, -15, -15}, { -2,  -2,  -2}, {38, -10, -22}},
		{    8  , 300 , {-15, -15, -15}, { -2,  -2,  -2}, {38, -10, -22}},
		{    8  , 150 , {-15, -15, -15}, {  0,   0,   0}, {50, -23, -14}},

		{   16  , 4800, {  0,   0,   0}, { -3,  -3,  -3}, {35, -10,  -7}},
		{   16  , 2400, {-15, -15, -15}, { -3,  -3,  -3}, {35, -10,  -7}},
		{   16  , 1200, {-15, -15, -15}, { -3,  -3,  -3}, {35, -10, -22}},
		{   16  , 600 , {-15, -15, -15}, { -2,  -2,  -2}, {38, -10, -22}},
		{   16  , 300 , {-15, -15, -15}, { -2,  -2,  -2}, {38, -10, -22}},
		{   16  , 150 , {-15, -15, -15}, {-15, -15, -15}, {40, -15,  -5}}
	};

	struct st_cut *ct;

	SANE_Int a;
	SANE_Int count = sizeof(cuts) / sizeof(struct st_cut);

	*red = *green = *blue = 0;

	if (res <= 150)
		res = 150;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else if (res <= 1200)
		res = 1200;
	else if (res <= 2400)
		res = 2400;
	else res = 4800;

	for (a = 0; a < count; a++)
	{
		ct = &cuts[a];
		if ((ct->depth == depth)&&(ct->res == res))
		{
			switch(scantype)
			{
				case ST_NEG:
					*red   = ct->negative[CL_RED];
					*green = ct->negative[CL_GREEN];
					*blue  = ct->negative[CL_BLUE];
					break;
				case ST_TA:
					*red   = ct->transparent[CL_RED];
					*green = ct->transparent[CL_GREEN];
					*blue  = ct->transparent[CL_BLUE];
					break;
				default: /* reflective */
					*red   = ct->reflective[CL_RED];
					*green = ct->reflective[CL_GREEN];
					*blue  = ct->reflective[CL_BLUE];
					break;
			}
			break;
		}
	}
}

static void ua4900_shading_cut(SANE_Int usb, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	struct st_cut
	{
		SANE_Int usb;
		SANE_Int depth;
		SANE_Int res;
		SANE_Int reflective[3];
		SANE_Int transparent[3];
		SANE_Int negative[3];
	};

	struct st_cut cuts[] =
	{
		/*usb , depth, res , {reflective   }, {transparent  }, {negative    } */
		{USB20, 8    , 1200, {-15, -17, -13}, {  6,   0,   0}, {110, 15, 250}},
		{USB20, 8    , 600 , {-16, -15, -15}, {  6,   7,   5}, {105, 11,  20}},
		{USB20, 8    , 300 , {-15, -15, -15}, {  6,   7,   5}, {105, 10,  25}},
		{USB20, 8    , 200 , {-15, -15, -15}, {  5,   5,   5}, {110, 15,  32}},
		{USB20, 8    , 100 , {-15, -15, -15}, {  5,   5,   5}, {110, 15,  32}},

		{USB20, 16   , 1200, {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB20, 16   , 600 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB20, 16   , 300 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB20, 16   , 200 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB20, 16   , 100 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},

		/*usb , depth, res , {reflective   }, {transparent  }, {negative    } */
		{USB11, 8    , 1200, {-15, -17, -13}, {  5,   5,   5}, {105, 12,  23}},
		{USB11, 8    , 600 , {-17, -15, -16}, {  8,   8,   8}, {105, 12,  20}},
		{USB11, 8    , 300 , {-15, -15, -15}, {  8,   8,   8}, {105, 15,  25}},
		{USB11, 8    , 200 , {-15, -15, -15}, {  8,   8,   8}, {105, 40,  25}},
		{USB11, 8    , 100 , {-15, -15, -15}, {  8,   8,   8}, {-20, 50,  23}},

		{USB11, 16   , 1200, {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB11, 16   , 600 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB11, 16   , 300 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB11, 16   , 200 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}},
		{USB11, 16   , 100 , {-15, -15, -15}, {  5,   5,   5}, { 0,   0,   0}}
	};

	struct st_cut *ct;

	SANE_Int a;
	SANE_Int count = sizeof(cuts) / sizeof(struct st_cut);

	*red = *green = *blue = 0;

	if (res <= 100)
		res = 100;
	else if (res <= 200)
		res = 200;
	else if (res <= 300)
		res = 300;
	else if (res <= 600)
		res = 600;
	else res = 1200;

	for (a = 0; a < count; a++)
	{
		ct = &cuts[a];
		if ((ct->usb == usb)&&(ct->depth == depth)&&(ct->res == res))
		{
			switch(scantype)
			{
				case ST_NEG:
					*red   = ct->negative[CL_RED];
					*green = ct->negative[CL_GREEN];
					*blue  = ct->negative[CL_BLUE];
					break;
				case ST_TA:
					*red   = ct->transparent[CL_RED];
					*green = ct->transparent[CL_GREEN];
					*blue  = ct->transparent[CL_BLUE];
					break;
				default: /* reflective */
					*red   = ct->reflective[CL_RED];
					*green = ct->reflective[CL_GREEN];
					*blue  = ct->reflective[CL_BLUE];
					break;
			}
			break;
		}
	}
}

static void cfg_shading_cut_get(SANE_Int sensortype, SANE_Int depth, SANE_Int res, SANE_Int scantype, SANE_Int *red, SANE_Int *green, SANE_Int *blue)
{
	switch(RTS_Debug->dev_model)
	{
		case UA4900:
			ua4900_shading_cut(RTS_Debug->usbtype, depth, res, scantype, red, green, blue);
			break;

		case HP3800:
		case HPG2710:
			hp3800_shading_cut(res, scantype, red, green, blue);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			hp4370_shading_cut(depth, res, scantype, red, green, blue);
			break;

		default:
			/* hp3970 and hp4070 */
			hp3970_shading_cut(RTS_Debug->usbtype, sensortype, depth, res, scantype, red, green, blue);
			break;
	}
}

/** SEC: Sensor Timings ---------- */

/* ccd timing values for bq5550 scanner */
static SANE_Int bq5550_timing_get(SANE_Int tm, struct st_timing *reg)
{
	SANE_Int rst = ERROR;

	if ((tm < 7)&&(reg != NULL))
	{
		/* bq5550 sensor timing values for each resolution and color mode */

		struct st_timing data[] =
		{
			/* res , cnpp, {cvtrp 1 2 3     }, cvtrw, cvtrfpw, cvtrbpw, {{cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}}, cphbp2s, cphbp2e, clamps, clampe , {cdss1, cdss2}, {cdsc1, cdsc2}, {cdscs1, cdscs2}, {adcclkp 1 y 2             }, adcclkp2e */
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x16 , 0x02   , 0x02   , {{67645734915., 0.          , 0x00 , 0x01 , 0x00 }, {132120576.  , 0.          , 0x00 , 0x01 , 0x00 }, {68585259519., 0.          , 0x01 , 0x01 , 0x00 }, {134217216.  , 0.          , 0x01 , 0x01 , 0x01 }, {68585259519., 0.   , 0x01 , 0x01 , 0x00 }, {134217216.  , 0.   , 0x01 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x0F , 0x1F }, {0x12 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x16 , 0x02   , 0x02   , {{66571993091., 0.          , 0x00 , 0x01 , 0x00 }, {132120576.  , 0.          , 0x00 , 0x01 , 0x00 }, {131071.     , 0.          , 0x00 , 0x01 , 0x00 }, {68719345664., 0.          , 0x00 , 0x01 , 0x01 }, {131071.     , 0.   , 0x00 , 0x01 , 0x00 }, {68719345664., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x10 , 0x1F }, {0x13 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x16 , 0x02   , 0x02   , {{67645734915., 0.          , 0x00 , 0x01 , 0x00 }, {125829120.  , 0.          , 0x00 , 0x01 , 0x00 }, {67654057987., 0.          , 0x00 , 0x01 , 0x00 }, {1065418748. , 0.          , 0x00 , 0x01 , 0x01 }, {16383.      , 0.   , 0x00 , 0x01 , 0x00 }, {68182605824., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x10 , 0x1F }, {0x13 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x16 , 0x02   , 0x02   , {{67645734915., 0.          , 0x00 , 0x01 , 0x00 }, {132120576.  , 0.          , 0x00 , 0x01 , 0x00 }, {68585259519., 0.          , 0x01 , 0x01 , 0x00 }, {134217216.  , 0.          , 0x01 , 0x01 , 0x01 }, {68585259519., 0.   , 0x01 , 0x01 , 0x00 }, {134217216.  , 0.   , 0x01 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x0F , 0x1F }, {0x12 , 0x22 }, {0x00  , 0x00  }, {262140.     , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x16 , 0x02   , 0x02   , {{66571993091., 0.          , 0x00 , 0x01 , 0x00 }, {132120576.  , 0.          , 0x00 , 0x01 , 0x00 }, {131071.     , 0.          , 0x00 , 0x01 , 0x00 }, {68719345664., 0.          , 0x00 , 0x01 , 0x01 }, {131071.     , 0.   , 0x00 , 0x01 , 0x00 }, {68719345664., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x10 , 0x1F }, {0x13 , 0x22 }, {0x00  , 0x00  }, {262140.     , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x16 , 0x02   , 0x02   , {{67645734915., 0.          , 0x00 , 0x01 , 0x00 }, {125829120.  , 0.          , 0x00 , 0x01 , 0x00 }, {67654057987., 0.          , 0x00 , 0x01 , 0x00 }, {1065418748. , 0.          , 0x00 , 0x01 , 0x01 }, {16383.      , 0.   , 0x00 , 0x01 , 0x00 }, {68182605824., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x10 , 0x1F }, {0x13 , 0x22 }, {0x00  , 0x00  }, {524284.     , 0.          }, 0x00}
		};

		memcpy(reg, &data[tm], sizeof(struct st_timing));
		rst = OK;
	}

	return rst;
}

/* ccd timing values for hp3800 scanner */
static SANE_Int hp3800_timing_get(SANE_Int tm, struct st_timing *reg)
{
	SANE_Int rst = ERROR;

	if ((tm < 20)&&(reg != NULL))
	{
		/* Toshiba T2905 sensor timing values for each resolution and color mode */

		struct st_timing data[] =
		{
			/* res , cnpp, {cvtrp 1 2 3     }, cvtrw, cvtrfpw, cvtrbpw, {{cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}}, cphbp2s, cphbp2e, clamps, clampe , {cdss1, cdss2}, {cdsc1, cdsc2}, {cdscs1, cdscs2}, {adcclkp 1 y 2             }, adcclkp2e */
			{0x0960, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{8589934588. , 0.          , 0x00 , 0x01 , 0x01 }, {68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {134217216.  , 0.          , 0x01 , 0x01 , 0x00 }, {68585259519., 0.          , 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x0F , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{64692944895., 0.          , 0x00 , 0x01 , 0x01 }, {66571993087., 0.          , 0x00 , 0x01 , 0x01 }, {524287.     , 0.          , 0x00 , 0x01 , 0x00 }, {68718952448., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x10 , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {68719476733., 0.          , 0x00 , 0x01 , 0x01 }, {1069563840. , 0.          , 0x00 , 0x01 , 0x00 }, {67649912895., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x02 , 0x21 }, {0x03 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {68719476733., 0.          , 0x00 , 0x01 , 0x01 }, {477225440.  , 0.          , 0x00 , 0x01 , 0x00 }, {68242251295., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x02 , 0x21 }, {0x04 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x0096, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {68719476733., 0.          , 0x00 , 0x01 , 0x01 }, {13743895344., 0.          , 0x00 , 0x01 , 0x00 }, {54975581391., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x01 , 0x21 }, {0x02 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},

			{0x0960, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{8589934588. , 0.          , 0x00 , 0x01 , 0x01 }, {68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {134217216.  , 0.          , 0x01 , 0x01 , 0x00 }, {68585259519., 0.          , 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x0F , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{64692944895., 0.          , 0x00 , 0x01 , 0x01 }, {66571993087., 0.          , 0x00 , 0x01 , 0x01 }, {524287.     , 0.          , 0x00 , 0x01 , 0x00 }, {68718952448., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x10 , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {68719476733., 0.          , 0x00 , 0x01 , 0x01 }, {1069563840. , 0.          , 0x00 , 0x01 , 0x00 }, {67649912895., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x02 , 0x21 }, {0x03 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {68719476733., 0.          , 0x00 , 0x01 , 0x01 }, {477225440.  , 0.          , 0x00 , 0x01 , 0x00 }, {68242251295., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x02 , 0x21 }, {0x04 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 0.          , 0x00 , 0x01 , 0x01 }, {68719476733., 0.          , 0x00 , 0x01 , 0x01 }, {266346464.  , 0.          , 0x00 , 0x01 , 0x00 }, {68453130271., 0.          , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x01  , -1     , {0x04 , 0x21 }, {0x06 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},

			{0x0960, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{8589934588. , 8589934588. , 0x00 , 0x01 , 0x01 }, {68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {134217216.  , 134217216.  , 0x01 , 0x01 , 0x00 }, {68585259519., 68585259519., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x80FF , 0x01  , -1     , {0x0F , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{64692944895., 64692944895., 0x00 , 0x01 , 0x01 }, {66571993087., 66571993087., 0x00 , 0x01 , 0x01 }, {524287.     , 524287.     , 0x00 , 0x01 , 0x00 }, {68718952448., 68718952448., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x80FF , 0x01  , -1     , {0x10 , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {68719476733., 68719476733., 0x00 , 0x01 , 0x01 }, {534781920.  , 534781920.  , 0x00 , 0x01 , 0x00 }, {68184694815., 68184694815., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x20FF , 0x01  , -1     , {0x05 , 0x21 }, {0x07 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {68719476733., 68719476733., 0x00 , 0x01 , 0x01 }, {477225440.  , 477225440.  , 0x00 , 0x01 , 0x00 }, {68242251295., 68242251295., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x11FF , 0x01  , -1     , {0x02 , 0x21 }, {0x04 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x0096, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {68719476733., 68719476733., 0x00 , 0x01 , 0x01 }, {13743895344., 13743895344., 0x00 , 0x01 , 0x00 }, {54975581391., 54975581391., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x110F , 0x01  , -1     , {0x01 , 0x21 }, {0x02 , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},

			{0x0960, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{8589934588. , 8589934588. , 0x00 , 0x01 , 0x01 }, {68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {134217216.  , 134217216.  , 0x01 , 0x01 , 0x00 }, {68585259519., 68585259519., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x80FF , 0x01  , -1     , {0x0F , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{64692944895., 64692944895., 0x00 , 0x01 , 0x01 }, {66571993087., 66571993087., 0x00 , 0x01 , 0x01 }, {524287.     , 524287.     , 0x00 , 0x01 , 0x00 }, {68718952448., 68718952448., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x80FF , 0x01  , -1     , {0x10 , 0x20 }, {0x11 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {68719476733., 68719476733., 0x00 , 0x01 , 0x01 }, {534781920.  , 534781920.  , 0x00 , 0x01 , 0x00 }, {68184694815., 68184694815., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x20FF , 0x01  , -1     , {0x05 , 0x21 }, {0x07 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {68719476733., 68719476733., 0x00 , 0x01 , 0x01 }, {477225440.  , 477225440.  , 0x00 , 0x01 , 0x00 }, {68242251295., 68242251295., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x110F , 0x01  , -1     , {0x02 , 0x21 }, {0x04 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x0096, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{68719476732., 68719476732., 0x00 , 0x01 , 0x01 }, {68719476733., 68719476733., 0x00 , 0x01 , 0x01 }, {13743895344., 13743895344., 0x00 , 0x01 , 0x00 }, {54975581391., 54975581391., 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0xFF   , 0x110F , 0x01  , -1     , {0x01 , 0x21 }, {0x02 , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
		};

		memcpy(reg, &data[tm], sizeof(struct st_timing));
		rst = OK;
	}

	return rst;
}

/* ccd timing values for hp3970 scanner */
static SANE_Int hp3970_timing_get(SANE_Int sensortype, SANE_Int tm, struct st_timing *reg)
{
	SANE_Int rst = ERROR;

	if ((tm < 12)&&(reg != NULL))
	{
		rst = OK;
		if (sensortype == CCD_SENSOR)
		{
			/* Toshiba T2952 sensor timing values for each resolution and color mode */
			struct st_timing data[] =
			{
				/* res , cnpp, {cvtrp 1 2 3     }, cvtrw, cvtrfpw, cvtrbpw, {{cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}}, cphbp2s, cphbp2e, clamps, clampe , {cdss1, cdss2}, {cdsc1, cdsc2}, {cdscs1, cdscs2}, {adcclkp 1 y 2             }, adcclkp2e */
				{0x0960, 0x0B, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {34376515584., 0.   , 0x00 , 0x01 , 0x00 }, {8455716864. , 0.   , 0x01 , 0x01 , 0x00 }, {60246982656., 0.   , 0x01 , 0x01 , 0x01 }, {68719214592., 0.   , 0x00 , 0x01 , 0x01 }, {68719214592., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x24000, {0x04 , 0x09 }, {0x06 , 0x0B }, {0x00  , 0x00  }, {27481079808., 0.          }, 0x00},
				{0x04B0, 0x1A, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {64424511232., 0.   , 0x00 , 0x01 , 0x00 }, {68711088128., 0.   , 0x00 , 0x01 , 0x00 }, {8388352.    , 0.   , 0x00 , 0x01 , 0x01 }, {68719476480., 0.   , 0x00 , 0x01 , 0x01 }, {68719476480., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x08 , 0x15 }, {0x0C , 0x18 }, {0x00  , 0x00  }, {33351135232., 33351135232.}, 0x01},
				{0x0258, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {60129570816., 0.   , 0x00 , 0x01 , 0x00 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {16773120.   , 0.   , 0x00 , 0x01 , 0x00 }, {68702699520., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x09 , 0x12 }, {0x0C , 0x14 }, {0x00  , 0x00  }, {64677150720., 0.          }, 0x00},
				{0x012C, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {51539611648., 0.   , 0x00 , 0x01 , 0x00 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {1057222656. , 0.   , 0x00 , 0x01 , 0x00 }, {67662249984., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x04 , 0x15 }, {0x06 , 0x17 }, {0x00  , 0x00  }, {8084643840. , 0.          }, 0x00},
				{0x00C8, 0x1D, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {68182605888., 0.   , 0x00 , 0x01 , 0x00 }, {68719476720., 0.   , 0x00 , 0x01 , 0x01 }, {68719476720., 0.   , 0x00 , 0x01 , 0x01 }, {16659267072., 0.   , 0x00 , 0x01 , 0x00 }, {52060209600., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x0A , 0x1B }, {0x0C , 0x1D }, {0x00  , 0x00  }, {4164816768. , 0.          }, 0x00},
				{0x0960, 0x0B, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {34376515584., 0.   , 0x00 , 0x01 , 0x00 }, {8455716864. , 0.   , 0x01 , 0x01 , 0x00 }, {60246982656., 0.   , 0x01 , 0x01 , 0x01 }, {68719214592., 0.   , 0x00 , 0x01 , 0x01 }, {68719214592., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x24000, {0x04 , 0x09 }, {0x06 , 0x0B }, {0x00  , 0x00  }, {2113929216. , 0.          }, 0x00},
				{0x04B0, 0x1A, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {64424511232., 0.   , 0x00 , 0x01 , 0x00 }, {68711088128., 0.   , 0x00 , 0x01 , 0x00 }, {8388352.    , 0.   , 0x00 , 0x01 , 0x01 }, {68719476480., 0.   , 0x00 , 0x01 , 0x01 }, {68719476480., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x08 , 0x15 }, {0x0C , 0x18 }, {0x00  , 0x00  }, {67104768.   , 67104768.   }, 0x01},
				{0x0258, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {60129570816., 0.   , 0x00 , 0x01 , 0x00 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {16773120.   , 0.   , 0x00 , 0x01 , 0x00 }, {68702699520., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x09 , 0x12 }, {0x0C , 0x14 }, {0x00  , 0x00  }, {268369920.  , 0.          }, 0x00},
				{0x012C, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {51539611648., 0.   , 0x00 , 0x01 , 0x00 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {68719472640., 0.   , 0x00 , 0x01 , 0x01 }, {1057222656. , 0.   , 0x00 , 0x01 , 0x00 }, {67662249984., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x04 , 0x15 }, {0x06 , 0x17 }, {0x00  , 0x00  }, {33546240.   , 0.          }, 0x00},
				{0x00C8, 0x1D, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{68585258944., 0.   , 0x00 , 0x01 , 0x00 }, {536870784.  , 0.   , 0x00 , 0x01 , 0x00 }, {16659267072., 0.   , 0x00 , 0x01 , 0x00 }, {52060209600., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x0A , 0x1B }, {0x0C , 0x1C }, {0x00  , 0x00  }, {4194176.    , 0.          }, 0x00},
				{0x0064, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {34359738368., 0.   , 0x00 , 0x01 , 0x00 }, {68719476735., 0.   , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x01 , 0x01 }, {7635497415. , 0.   , 0x00 , 0x01 , 0x00 }, {61083979320., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x02 , 0x22 }, {0x03 , 0x23 }, {0x00  , 0x00  }, {2114445438. , 0.          }, 0x00},
				{0x0064, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {34359738368., 0.   , 0x00 , 0x01 , 0x00 }, {68719476735., 0.   , 0x00 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x01 , 0x01 }, {7635497415. , 0.   , 0x00 , 0x01 , 0x00 }, {61083979320., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x02 , 0x22 }, {0x03 , 0x23 }, {0x00  , 0x00  }, {524286.     , 0.          }, 0x00}
			};

			memcpy(reg, &data[tm], sizeof(struct st_timing));
		} else
		{
			/* Sony S575 sensor timing values for each resolution and color mode
					I haven't found any hp3970 scanner using sony sensor but windows drivers support this case */
			struct st_timing data[] =
			{
				/* res , cnpp, {cvtrp1  2    3  }, cvtrw, cvtrfpw, cvtrbpw, {{cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}}, cphbp2s, cphbp2e, clamps, clampe , {cdss1, cdss2}, {cdsc1, cdsc2}, {cdscs1, cdscs2}, {adcclkp 1 y 2             }, adcclkp2e */
				{0x0960, 0x0B, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{60112764928., 0.   , 0x00 , 0x01 , 0x00 }, {34326183936., 0.   , 0x00 , 0x01 , 0x00 }, {1056964608. , 0.   , 0x01 , 0x01 , 0x00 }, {67645734912., 0.   , 0x01 , 0x01 , 0x01 }, {1056964608. , 0.   , 0x00 , 0x01 , 0x00 }, {67645734912., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x24000, {0x05 , 0x09 }, {0x07 , 0x0b }, {0x00  , 0x00  }, {27481079808., 0.          }, 0x00},
				{0x04B0, 0x1A, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{68316823296., 0.   , 0x00 , 0x01 , 0x00 }, {42949672704., 0.   , 0x00 , 0x01 , 0x00 }, {4194048.    , 0.   , 0x00 , 0x01 , 0x00 }, {68715282432., 0.   , 0x00 , 0x01 , 0x01 }, {4194048.    , 0.   , 0x00 , 0x01 , 0x00 }, {68715282432., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x0B , 0x17 }, {0x0D , 0x19 }, {0x00  , 0x00  }, {16675567616., 16675567616.}, 0x01},
				{0x0258, 0x17, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{67914166272., 0.   , 0x00 , 0x01 , 0x00 }, {42949668864., 0.   , 0x00 , 0x01 , 0x00 }, {16773120.   , 0.   , 0x00 , 0x01 , 0x00 }, {68702699520., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x09 , 0x15 }, {0x0B , 0x17 }, {0x00  , 0x00  }, {8084643840. , 0.          }, 0x00},
				{0x012C, 0x17, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{60129538048., 0.   , 0x00 , 0x01 , 0x00 }, {34359730176., 0.   , 0x00 , 0x01 , 0x00 }, {1057222656. , 0.   , 0x00 , 0x01 , 0x00 }, {67662249984., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x04 , 0x15 }, {0x06 , 0x17 }, {0x00  , 0x00  }, {8084643840. , 0.          }, 0x00},
				{0x00C8, 0x1D, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{68585258944., 0.   , 0x00 , 0x01 , 0x00 }, {536870784.  , 0.   , 0x00 , 0x01 , 0x00 }, {16659267072., 0.   , 0x00 , 0x01 , 0x00 }, {52060209600., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x0A , 0x1A }, {0x0C , 0x1C }, {0x00  , 0x00  }, {8329633536. , 0.          }, 0x00},
				{0x0960, 0x0B, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{60112764928., 0.   , 0x00 , 0x01 , 0x00 }, {34326183936., 0.   , 0x00 , 0x01 , 0x00 }, {1056964608. , 0.   , 0x01 , 0x01 , 0x00 }, {67645734912., 0.   , 0x01 , 0x01 , 0x01 }, {1056964608. , 0.   , 0x00 , 0x01 , 0x00 }, {67645734912., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x24000, {0x05 , 0x09 }, {0x07 , 0x0B }, {0x00  , 0x00  }, {2113929216. , 0.          }, 0x00},
				{0x04B0, 0x1A, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{68316823296., 0.   , 0x00 , 0x01 , 0x00 }, {42949672704., 0.   , 0x00 , 0x01 , 0x00 }, {4194048.    , 0.   , 0x00 , 0x01 , 0x00 }, {68715282432., 0.   , 0x00 , 0x01 , 0x01 }, {4194048.    , 0.   , 0x00 , 0x01 , 0x00 }, {68715282432., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x0B , 0x17 }, {0x0D , 0x19 }, {0x00  , 0x00  }, {33552384.   , 33552384.   }, 0x01},
				{0x0258, 0x17, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{67914166272., 0.   , 0x00 , 0x01 , 0x00 }, {42949668864., 0.   , 0x00 , 0x01 , 0x00 }, {16773120.   , 0.   , 0x00 , 0x01 , 0x00 }, {68702699520., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x09 , 0x15 }, {0x0B , 0x17 }, {0x00  , 0x00  }, {33546240.   , 0.          }, 0x00},
				{0x012C, 0x17, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{60129538048., 0.   , 0x00 , 0x01 , 0x00 }, {34359730176., 0.   , 0x00 , 0x01 , 0x00 }, {34359730176., 0.   , 0x00 , 0x01 , 0x00 }, {67662249984., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x04 , 0x15 }, {0x06 , 0x17 }, {0x00  , 0x00  }, {33546240.   , 0.          }, 0x00},
				{0x00C8, 0x1D, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {68182605888., 0.   , 0x00 , 0x01 , 0x00 }, {68719476720., 0.   , 0x00 , 0x01 , 0x01 }, {68719476720., 0.   , 0x00 , 0x01 , 0x01 }, {16659267072., 0.   , 0x00 , 0x01 , 0x00 }, {52060209600., 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x0A , 0x1A }, {0x0C , 0x1D }, {0x00  , 0x00  }, {4194176.    , 0.          }, 0x00},
				{0x0064, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{51539607551., 0.   , 0x00 , 0x01 , 0x00 }, {34359738366., 0.   , 0x00 , 0x01 , 0x00 }, {7635497415. , 0.   , 0x00 , 0x01 , 0x00 }, {61083979320., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x02 , 0x22 }, {0x03 , 0x23 }, {0x00  , 0x00  }, {2114445438. , 0.          }, 0x00},
				{0x0064, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x01   , 0x0F   , {{51539607551., 0.   , 0x00 , 0x01 , 0x00 }, {34359738366., 0.   , 0x00 , 0x01 , 0x00 }, {7635497415. , 0.   , 0x00 , 0x01 , 0x00 }, {61083979320., 0.   , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x01 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x02 , 0x22 }, {0x03 , 0x23 }, {0x00  , 0x00  }, {524286.     , 0.          }, 0x00}
			};

			memcpy(reg, &data[tm], sizeof(struct st_timing));
		}
	}

	return rst;
}

/* ccd timing values for hp4370 scanner */
static SANE_Int hp4370_timing_get(SANE_Int tm, struct st_timing *reg)
{
	SANE_Int rst = ERROR;

	if ((reg != NULL)&&(tm < 14))
	{
		/* Toshiba T2958 sensor timing values for each resolution and color mode */

		struct st_timing data[] =
		{
			/* res , cnpp, {cvtrp 1 2 3     }, cvtrw, cvtrfpw, cvtrbpw, {{cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2      , cphps, cphge, cphgo}, {cphp1       , cphp2       , cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}}, cphbp2s, cphbp2e, clamps, clampe , {cdss1, cdss2}, {cdsc1, cdsc2}, {cdscs1, cdscs2}, {adcclkp 1 y 2             }, adcclkp2e */
			{0x12C0, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {17179869183., 17179869183., 0x00 , 0x01 , 0x01 }, {1073479680. , 1073479680., 0x01 , 0x01 , 0x00 }, {67645997055., 67645997055., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x5D5B , 0xBAB7 , 0x1A  , -1     , {0x08 , 0x15 }, {0x0A , 0x17 }, {0x00  , 0x00  }, {8084644321. , 8084644321. }, 0x00},
			{0x0960, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {17179869182., 0.          , 0x00 , 0x01 , 0x01 }, {268434432.  , 0.         , 0x01 , 0x01 , 0x00 }, {68451042303., 0.          , 0x01 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x1A  , -1     , {0x07 , 0x18 }, {0x0B , 0x1E }, {0x00  , 0x00  }, {67662254016., 0.          }, 0x00},
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {68719475967., 0.          , 0x00 , 0x01 , 0x01 }, {1048572.    , 0.         , 0x00 , 0x01 , 0x01 }, {68718428163., 0.          , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x1F , 0x17 }, {0x21 , 0x19 }, {0x01  , 0x01  }, {34888349727., 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {68719476731., 0.          , 0x00 , 0x01 , 0x01 }, {4261672960. , 0.         , 0x00 , 0x01 , 0x00 }, {64457803775., 0.          , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x1A  , 0x05500, {0x00 , 0x1F }, {0x03 , 0x21 }, {0x00  , 0x00  }, {8457781752. , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {51808043007., 0.          , 0x00 , 0x01 , 0x01 }, {8084644320. , 0.         , 0x00 , 0x01 , 0x00 }, {60634832415., 0.          , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x1A  , 0x05500, {0x09 , 0x20 }, {0x0A , 0x22 }, {0x00  , 0x00  }, {4228890876. , 0.          }, 0x00},
			{0x12C0, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {17179869183., 17179869183., 0x00 , 0x01 , 0x01 }, {1073479680. , 1073479680., 0x01 , 0x01 , 0x00 }, {67645997055., 67645997055., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x5D5B , 0xBAB7 , 0x1A  , -1     , {0x08 , 0x15 }, {0x0A , 0x17 }, {0x00  , 0x00  }, {33546240.   , 33546240.   }, 0x00},
			{0x0960, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {17179869182., 0.          , 0x00 , 0x01 , 0x01 }, {268434432.  , 0.         , 0x01 , 0x01 , 0x00 }, {68451042303., 0.          , 0x01 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x1A  , -1     , {0x07 , 0x18 }, {0x0B , 0x1E }, {0x00  , 0x00  }, {16777152.   , 0.          }, 0x00},
			{0x04B0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {68719475967., 0.          , 0x00 , 0x01 , 0x01 }, {1048572.    , 0.         , 0x00 , 0x01 , 0x01 }, {68718428163., 0.          , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }, {0.          , 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x1F , 0x17 }, {0x21 , 0x19 }, {0x01  , 0x01  }, {536868864.  , 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {68719476731., 0.          , 0x00 , 0x01 , 0x01 }, {4261672960. , 0.         , 0x00 , 0x01 , 0x00 }, {64457803775., 0.          , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x1A  , 0x05500, {0x00 , 0x1F }, {0x03 , 0x21 }, {0x00  , 0x00  }, {2097144.    , 0.          }, 0x00},
			{0x012C, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x04   , 0x04   , {{0.          , 0.   , 0x00 , 0x01 , 0x00 }, {51808043007., 0.          , 0x00 , 0x01 , 0x01 }, {8084644320. , 0.         , 0x00 , 0x01 , 0x00 }, {60634832415., 0.          , 0x00 , 0x01 , 0x01 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }, {0.          , 0.   , 0x00 , 0x00 , 0x00 }}, 0x00   , 0x00   , 0x1A  , 0x05500, {0x09 , 0x20 }, {0x0A , 0x22 }, {0x00  , 0x00  }, {1048572.    , 0.          }, 0x00},
			{0x12C0, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {17179869183., 17179869183., 0x00 , 0x01 , 0x01 }, {1073479680. , 1073479680., 0x01 , 0x01 , 0x00 }, {67645997055., 67645997055., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x6977 , 0xD2EF , 0x1A  , -1     , {0x07 , 0x15 }, {0x0A , 0x17 }, {0x00  , 0x00  }, {8084644321. , 8084644321. }, 0x00},
			{0x12C0, 0x17, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {17179869183., 17179869183., 0x00 , 0x01 , 0x01 }, {1073479680. , 1073479680., 0x01 , 0x01 , 0x00 }, {67645997055., 67645997055., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x6977 , 0xD2EF , 0x1A  , -1     , {0x07 , 0x15 }, {0x0A , 0x17 }, {0x00  , 0x00  }, {33546240.   , 33546240.   }, 0x00},
			{0x12C0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {8589934591. , 8589934591. , 0x00 , 0x01 , 0x01 }, {134217216.  , 134217216. , 0x01 , 0x01 , 0x00 }, {68585259519., 68585259519., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x765B , 0xECB7 , 0x1A  , -1     , {0x07 , 0x1B }, {0x0D , 0x21 }, {0x00  , 0x00  }, {8457781752. , 8457781752. }, 0x00},
			{0x12C0, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x05   , 0x0E   , {{0.          , 0.   , 0x00 , 0x01 , 0x01 }, {8589934591. , 8589934591. , 0x00 , 0x01 , 0x01 }, {134217216.  , 134217216. , 0x01 , 0x01 , 0x00 }, {68585259519., 68585259519., 0x01 , 0x01 , 0x01 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }, {68719476735., 0.   , 0x00 , 0x00 , 0x00 }}, 0x765B , 0xECB7 , 0x1A  , -1     , {0x07 , 0x1B }, {0x0D , 0x21 }, {0x00  , 0x00  }, {2097144.    , 2097144.    }, 0x00}
		};

		memcpy(reg, &data[tm], sizeof(struct st_timing));
		rst = OK;
	}

	return rst;
}

/* ccd timing values for ua4900 scanner */
static SANE_Int ua4900_timing_get(SANE_Int tm, struct st_timing *reg)
{
	SANE_Int rst = ERROR;

	if ((tm < 10)&&(reg != NULL))
	{
		/* Sony S575 sensor timing values for each resolution and color mode */
		struct st_timing data[] =
		{
			/* res , cnpp, {cvtrp1  2    3  }, cvtrw, cvtrfpw, cvtrbpw, {{cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}, {cphp1       , cphp2, cphps, cphge, cphgo}}, cphbp2s, cphbp2e, clamps, clampe , {cdss1, cdss2}, {cdsc1, cdsc2}, {cdscs1, cdscs2}, {adcclkp 1 y 2             }, adcclkp2e */
			{0x04b0, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{ 1073737728., 0.   , 0x01 , 0x01 , 0x00 }, {67645739007., 0.   , 0x01 , 0x01 , 0x01 }, {67645739007., 0.   , 0x01 , 0x01 , 0x01 }, { 1073737728., 0.   , 0x01 , 0x01 , 0x00 }, {25769803776., 0.   , 0x00 , 0x01 , 0x00 }, {         62., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x08 , 0x1c }, {0x0A , 0x1e }, {0x00  , 0x00  }, {67662254016., 0.          }, 0x00},
			{0x0258, 0x23, {0x00, 0x00, 0x00}, 0x0F , 0x03   , 0x03   , {{     262143., 0.   , 0x00 , 0x01 , 0x00 }, {68719214592., 0.   , 0x00 , 0x01 , 0x01 }, {68719214592., 0.   , 0x00 , 0x01 , 0x01 }, {     262143., 3.   , 0x00 , 0x01 , 0x00 }, { 2080374784., 0.   , 0x00 , 0x01 , 0x00 }, {64424509455., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x10 , 0x1d }, {0x12 , 0x1f }, {0x00  , 0x00  }, {33831127008., 0.          }, 0x00},
			{0x012c, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{      32767., 0.   , 0x00 , 0x01 , 0x00 }, {68182605824., 0.   , 0x00 , 0x01 , 0x01 }, { 2130837500., 0.   , 0x00 , 0x01 , 0x01 }, {66588639235., 0.   , 0x00 , 0x01 , 0x00 }, {  117440512., 0.   , 0x00 , 0x01 , 0x00 }, {68182605825., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x12 , 0x1f }, {0x14 , 0x21 }, {0x00  , 0x00  }, { 8457781752., 0.          }, 0x00},
			{0x00c8, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{51539608575., 0.   , 0x00 , 0x01 , 0x00 }, {16642998272., 0.   , 0x00 , 0x01 , 0x01 }, { 2082408447., 0.   , 0x00 , 0x01 , 0x01 }, {66637068288., 0.   , 0x00 , 0x01 , 0x00 }, {   14680064., 0.   , 0x00 , 0x01 , 0x00 }, {68652367872., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x10 , 0x1f }, {0x12 , 0x21 }, {0x00  , 0x00  }, { 8457781752., 0.          }, 0x00},
			{0x0064, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{51539608575., 0.   , 0x00 , 0x01 , 0x00 }, {16642998272., 0.   , 0x00 , 0x01 , 0x01 }, { 2082408447., 0.   , 0x00 , 0x01 , 0x01 }, {66637068288., 0.   , 0x00 , 0x01 , 0x00 }, {   14680064., 0.   , 0x00 , 0x01 , 0x00 }, {68652367872., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x10 , 0x1f }, {0x12 , 0x21 }, {0x00  , 0x00  }, { 8457781752., 0.          }, 0x00},
			{0x04b0, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{  268434432., 0.   , 0x01 , 0x01 , 0x00 }, {68451042303., 0.   , 0x01 , 0x01 , 0x01 }, {68451042303., 0.   , 0x01 , 0x01 , 0x01 }, {  268434432., 0.   , 0x01 , 0x01 , 0x00 }, {51539607552., 0.   , 0x00 , 0x01 , 0x00 }, {30.         , 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x11000, {0x08 , 0x1d }, {0x09 , 0x1e }, {0x00  , 0x00  }, {16777152.   , 0.          }, 0x00},
			{0x0258, 0x1d, {0x00, 0x00, 0x00}, 0x0f , 0x03   , 0x03   , {{    2097088., 0.   , 0x00 , 0x01 , 0x00 }, {68717379632., 0.   , 0x00 , 0x01 , 0x01 }, {68717379632., 0.   , 0x00 , 0x01 , 0x01 }, {    2097088., 3.   , 0x00 , 0x01 , 0x00 }, { 1879048192., 0.   , 0x00 , 0x01 , 0x00 }, {60129542592., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x05500, {0x0d , 0x16 }, {0x0f , 0x18 }, {0x00  , 0x00  }, {134213632.  , 0.          }, 0x00},
			{0x012c, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{64424510463., 0.   , 0x00 , 0x01 , 0x00 }, { 4278190080., 0.   , 0x00 , 0x01 , 0x01 }, {  267390975., 0.   , 0x00 , 0x01 , 0x01 }, {68452085760., 0.   , 0x00 , 0x01 , 0x00 }, {    6291456., 0.   , 0x00 , 0x01 , 0x00 }, {68652367872., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x15 , 0x1f }, {0x17 , 0x21 }, {0x00  , 0x00  }, {2097144.    , 0.          }, 0x00},
			{0x00C8, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{51539608575., 0.   , 0x00 , 0x01 , 0x00 }, {16642998272., 0.   , 0x00 , 0x01 , 0x01 }, { 2082408447., 0.   , 0x00 , 0x01 , 0x01 }, {66637068288., 0.   , 0x00 , 0x01 , 0x00 }, {   14680064., 0.   , 0x00 , 0x01 , 0x00 }, {68652367872., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x17 , 0x1f }, {0x19 , 0x21 }, {0x00  , 0x00  }, {2097144.    , 0.          }, 0x00},
			{0x0064, 0x23, {0x00, 0x00, 0x00}, 0x1E , 0x03   , 0x03   , {{51539608575., 0.   , 0x00 , 0x01 , 0x00 }, {16642998272., 0.   , 0x00 , 0x01 , 0x01 }, { 2082408447., 0.   , 0x00 , 0x01 , 0x01 }, {66637068288., 0.   , 0x00 , 0x01 , 0x00 }, {   14680064., 0.   , 0x00 , 0x01 , 0x00 }, {68652367872., 0.   , 0x00 , 0x01 , 0x00 }}, 0x00   , 0x00   , 0x01  , 0x02700, {0x17 , 0x1f }, {0x19 , 0x21 }, {0x00  , 0x00  }, {2097144.    , 0.          }, 0x00}
		};

		memcpy(reg, &data[tm], sizeof(struct st_timing));
		rst = OK;
	}

	return rst;
}

static SANE_Int cfg_timing_get(SANE_Int sensortype, SANE_Int tm, struct st_timing *reg)
{
	/* return timing values for current device */

	SANE_Int rst = ERROR;

	switch(RTS_Debug->dev_model)
	{
		case BQ5550:
			rst = bq5550_timing_get(tm, reg);
			break;

		case UA4900:
			rst = ua4900_timing_get(tm, reg);
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			rst = hp4370_timing_get(tm, reg);
			break;

		case HP3800:
		case HPG2710:
			rst = hp3800_timing_get(tm, reg);
			break;

		default:
			/* hp3970 and hp4070 */
			rst = hp3970_timing_get(sensortype, tm, reg);
			break;
	}

	return rst;
}

/** SEC: Motor curves ---------- */

static SANE_Int *bq5550_motor()
{
	SANE_Int *rst = NULL;
	SANE_Int steps[]  =
	{
		/* motorcurve 1   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 20000 ,6323 ,4095 ,3270 ,2823 ,2533 ,2318 ,2151 ,2016 ,1905 ,1811 ,1730 ,1660 ,1599 ,1544 ,1494 ,1450 ,1409 ,1373 ,1339 ,1307 ,1278 ,1251 ,1225 ,1201 ,1179 ,1158 ,1138 ,1119 ,1101 ,1084 ,1067 ,1052 ,1037 ,1023 ,1009 ,996 ,984 ,972 ,960 ,949 ,938 ,928 ,918 ,909 ,899 ,890 ,882 ,873 ,865 ,857 ,849 ,842 ,835 ,828 ,821 ,814 ,807 ,801 ,795 ,789 ,783 ,777 ,772 ,766 ,761 ,756 ,751 ,746 ,741 ,736 ,731 ,727 ,722 ,718 ,714 ,709 ,705 ,701 ,697 ,693 ,690 ,686 ,682 ,678 ,675 ,671 ,668 ,665 ,661 ,658 ,655 ,652 ,649 ,645 ,642 ,640 ,637 ,634 ,631 ,628 ,625 ,623 ,620 ,617 ,615 ,612 ,610 ,607 ,605 ,602 ,600 ,597 ,595 ,593 ,591 ,588 ,586 ,584 ,582 ,580 ,577 ,575 ,573 ,571 ,569 ,567 ,565 ,563 ,562 ,560 ,558 ,556 ,554 ,552 ,550 ,549 ,547 ,545 ,543 ,542 ,540 ,538 ,537 ,535 ,534 ,532 ,530 ,529 ,527 ,526 ,524 ,523 ,521 ,520 ,518 ,517 ,515 ,514 ,513 ,511 ,510 ,508 ,507 ,506 ,504 ,503 ,502 ,501 ,499 ,498 ,497 ,495 ,494 ,493 ,492 ,491 ,489 ,488 ,487 ,486 ,485 ,484 ,482 ,481 ,480 ,479 ,478 ,477 ,476 ,475 ,474 ,473 ,472 ,470 ,469 ,468 ,467 ,466 ,465 ,464 ,463 ,462 ,461 ,460 ,460 ,459 ,458 ,457 ,456 ,455 ,454 ,453 ,452 ,451 ,450 ,449 ,449 ,448 ,447 ,446 ,445 ,444 ,443 ,443 ,442 ,441 ,440 ,439 ,438 ,438 ,437 ,436 ,435 ,434 ,434 ,433 ,432 ,431 ,431 ,430 ,429 ,428 ,428 ,427 ,426 ,425 ,425 ,424 ,423 ,422 ,422 ,421 ,420 ,420 ,419 ,418 ,418 ,417 ,416 ,416 ,415 ,414 ,414 ,413 ,412 ,412 ,411 ,410 ,410 ,409 ,408 ,408 ,407 ,407 ,406 ,405 ,405 ,404 ,404 ,403 ,402 ,402 ,401 ,401 ,400 ,399 ,399 ,398 ,398 ,397 ,397 ,396 ,395 ,395 ,394 ,394 ,393 ,393 ,392 ,392 ,391 ,390 ,390 ,389 ,389 ,388 ,388 ,387 ,387 ,386 ,386 ,385 ,385 ,384 ,384 ,383 ,383 ,382 ,382 ,381 ,381 ,380 ,380 ,379 ,379 ,378 ,378 ,377 ,377 ,377 ,376 ,376 ,375 ,375 ,374 ,374 ,373 ,373 ,372 ,372 ,372 ,371 ,371 ,370 ,370 ,369 ,369 ,368 ,368 ,368 ,367 ,367 ,366 ,366 ,365 ,365 ,365 ,364 ,364 ,363 ,363 ,363 ,362 ,362 ,361 ,361 ,361 ,360 ,360 ,359 ,359 ,359 ,358 ,358 ,357 ,357 ,357 ,356 ,356 ,356 ,355 ,355 ,354 ,354 ,354 ,353 ,353 ,353 ,352 ,352 ,351 ,351 ,351 ,350 ,350 ,350 ,349 ,349 ,349 ,348 ,348 ,348 ,347 ,347 ,347 ,346 ,346 ,346 ,345 ,345 ,345 ,344 ,344 ,344 ,343 ,343 ,343 ,342 ,342 ,342 ,341 ,341 ,341 ,340 ,340 ,340 ,339 ,339 ,339 ,338 ,338 ,338 ,337 ,337 ,337 ,337 ,336 ,336 ,336 ,335 ,335 ,335 ,334 ,334 ,334 ,334 ,333 ,333 ,333 ,332 ,332 ,332 ,332 ,331 ,331 ,331 ,330 ,330 ,330 ,330 ,329 ,329 ,329 ,328 ,328 ,328 ,328 ,327 ,327 ,327 ,326 ,326 ,326 ,326 ,325 ,325 ,325 ,325 ,324 ,324 ,324 ,324 ,323 ,323 ,323 ,323 ,322 ,322 ,322 ,321 ,321 ,321 ,321 ,320 ,320 ,320 ,320 ,319 ,319 ,319 ,319 ,318 ,318 ,318 ,318 ,317 ,317 ,317 ,317 ,317 ,316 ,316 ,316 ,316 ,315 ,315 ,315 ,315 ,314 ,314 ,314 ,314 ,313 ,313 ,313 ,313 ,313 ,312 ,312 ,312 ,312 ,311 ,311 ,311 ,311 ,311 ,310 ,310 ,310 ,310 ,309 ,309 ,309 ,309 ,309 ,308 ,308 ,308 ,308 ,307 ,307 ,307 ,307 ,307 ,306 ,306 ,306 ,306 ,306 ,305 ,305 ,305 ,305 ,305 ,304 ,304 ,304 ,304 ,303 ,303 ,303 ,303 ,303 ,302 ,302 ,302 ,302 ,302 ,301 ,301 ,301 ,301 ,301 ,300 ,300 ,300 ,300 ,300 ,300 ,299 ,299 ,299 ,299 ,299 ,298 ,298 ,298 ,298 ,298 ,297 ,297 ,297 ,297 ,297 ,297 ,296 ,296 ,296 ,296 ,296 ,295 ,295 ,295 ,295 ,295 ,295 ,294 ,294 ,294 ,294 ,294 ,293 ,293 ,293 ,293 ,293 ,293 ,292 ,292 ,292 ,292 ,292 ,292 ,291 ,291 ,291 ,291 ,291 ,290 ,290 ,290 ,290 ,290 ,290 ,289 ,289 ,289 ,289 ,289 ,289 ,288 ,288 ,288 ,288 ,288 ,288 ,288 ,287 ,287 ,287 ,287 ,287 ,287 ,286 ,286 ,286 ,286 ,286 ,286 ,285 ,285 ,285 ,285 ,285 ,285, 0,
		ACC_CURVE,CRV_SMEARING  , 4000, 3877, 3377, 2777, 2127, 1724, 1449, 1250, 0,
		DEC_CURVE,CRV_NORMALSCAN, 479 ,500 ,547 ,607 ,682 ,766 ,865 ,950 ,1040 ,1200 ,1369 ,1580 ,1810 ,2087 ,2500 ,3048 ,3877 ,4818 ,5822 ,6896, 0,
		DEC_CURVE,CRV_SMEARING  , 1250, 1449, 1724, 2127, 2777, 3377, 3877, 4000, 0,
		DEC_CURVE,CRV_BUFFERFULL, 479 ,500 ,547 ,607 ,682 ,766 ,865 ,950 ,1040 ,1200 ,1369 ,1580 ,1810 ,2087 ,2500 ,3048 ,3877 ,4818 ,5822 ,6896, 0,
		-1
	};

	rst = (SANE_Int *)malloc(sizeof(steps));
	if (rst != NULL)
		memcpy(rst, &steps, sizeof(steps));

	return rst;
}

static SANE_Int *hp4370_motor()
{
	SANE_Int *rst = NULL;
	SANE_Int steps[]  =
	{
		/* motorcurve 1   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 2000, 1984, 1968, 1953, 1937, 1921, 1906, 1890, 1874, 1859, 1843, 1827, 1812, 1796, 1781, 1765, 1749, 1734, 1715, 1700, 1684, 1669, 1653, 1637, 1622, 1606, 1590, 1572, 1556, 1541, 1525, 1510, 1494, 1478, 1463, 1447, 1431, 1416, 1400, 1384, 1366, 1351, 1335, 1319, 1304, 1288, 1272, 1257, 1241, 1225, 1210, 1194, 1179, 1160, 1145, 1129, 1113, 1098, 1082, 1066, 1051, 1035, 1017, 1001, 986, 970, 954, 939, 923, 907, 892, 876, 861, 845, 829, 814, 798, 782, 767, 749, 0,
		ACC_CURVE,CRV_PARKHOME  , 4705, 2913, 2253, 1894, 1662, 1498, 1374, 1276, 1196, 1129, 1073, 1024, 981, 944, 910, 880, 852, 827, 804, 783, 764, 746, 729, 713, 699, 685, 672, 659, 648, 637, 626, 616, 607, 598, 589, 581, 573, 565, 558, 551, 544, 538, 532, 526, 520, 514, 509, 503, 500, 0,
		ACC_CURVE,CRV_SMEARING  , 200, 12, 14, 16, 0,
		DEC_CURVE,CRV_NORMALSCAN, 749, 1166, 1583, 2000, 0,
		DEC_CURVE,CRV_PARKHOME  , 500, 503, 509, 514, 520, 526, 532, 538, 544, 551, 558, 565, 573, 581, 589, 598, 607, 616, 626, 637, 648, 659, 672, 685, 699, 713, 729, 746, 764, 783, 804, 827, 852, 880, 910, 944, 981, 1024, 1073, 1129, 1196, 1276, 1374, 1498, 1662, 1894, 2253, 2913, 4705, 0,
		DEC_CURVE,CRV_SMEARING  , 300, 234, 167, 100, 0,
		DEC_CURVE,CRV_BUFFERFULL, 1100, 867, 633, 400, 0,
		-2,

		/* motorcurve 2   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 4705, 2664, 2061, 1732, 1521, 1370, 1257, 1167, 1094, 1033, 982, 937, 898, 864, 833, 805, 780, 757, 736, 717, 699, 683, 667, 653, 640, 627, 615, 604, 593, 583, 574, 564, 556, 547, 540, 532, 525, 518, 511, 505, 499, 493, 487, 481, 476, 471, 466, 461, 456, 452, 447, 443, 439, 435, 431, 427, 424, 420, 417, 413, 410, 407, 403, 400, 397, 394, 391, 389, 386, 383, 381, 378, 375, 373, 371, 368, 366, 364, 361, 359, 357, 355, 353, 351, 349, 347, 345, 343, 341, 339, 338, 336, 334, 332, 331, 329, 327, 326, 324, 323, 321, 320, 318, 317, 315, 314, 312, 311, 310, 308, 307, 306, 304, 303, 302, 301, 299, 298, 297, 296, 295, 293, 292, 291, 290, 289, 288, 287, 286, 285, 284, 283, 282, 281, 280, 279, 278, 277, 276, 275, 274, 273, 272, 271, 270, 270, 269, 268, 267, 266, 265, 264, 264, 263, 262, 261, 260, 260, 259, 258, 257, 257, 256, 255, 255, 254, 253, 252, 252, 251, 250, 250, 249, 248, 248, 247, 246, 246, 245, 244, 244, 243, 242, 242, 241, 241, 240, 239, 239, 238, 238, 237, 237, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 230, 230, 229, 229, 228, 227, 227, 227, 226, 226, 225, 225, 224, 224, 223, 223, 222, 222, 221, 221, 220, 220, 219, 219, 219, 218, 218, 217, 217, 216, 216, 216, 215, 215, 214, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 210, 210, 209, 209, 208, 208, 208, 207, 207, 207, 206, 206, 206, 205, 205, 204, 204, 204, 203, 203, 203, 202, 202, 202, 201, 201, 201, 200, 200, 200, 199, 199, 199, 198, 198, 198, 197, 197, 197, 197, 196, 196, 196, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 192, 192, 192, 192, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 183, 183, 183, 183, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 171, 171, 171, 0,
		ACC_CURVE,CRV_PARKHOME  , 4705, 2913, 2253, 1894, 1662, 1498, 1374, 1276, 1196, 1129, 1073, 1024, 981, 944, 910, 880, 852, 827, 804, 783, 764, 746, 729, 713, 699, 685, 672, 659, 648, 637, 626, 616, 607, 598, 589, 581, 573, 565, 558, 551, 544, 538, 532, 526, 520, 514, 509, 503, 500, 0,
		ACC_CURVE,CRV_SMEARING  , 200, 12, 14, 16, 0,
		DEC_CURVE,CRV_NORMALSCAN, 171, 172, 172, 173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 188, 189, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255, 258, 260, 262, 265, 268, 270, 273, 276, 279, 282, 285, 289, 292, 296, 299, 303, 307, 311, 316, 320, 325, 330, 335, 341, 347, 353, 359, 366, 373, 381, 390, 398, 408, 418, 429, 441, 455, 469, 485, 503, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		DEC_CURVE,CRV_PARKHOME  , 500, 503, 509, 514, 520, 526, 532, 538, 544, 551, 558, 565, 573, 581, 589, 598, 607, 616, 626, 637, 648, 659, 672, 685, 699, 713, 729, 746, 764, 783, 804, 827, 852, 880, 910, 944, 981, 1024, 1073, 1129, 1196, 1276, 1374, 1498, 1662, 1894, 2253, 2913, 4705, 0,
		DEC_CURVE,CRV_SMEARING  , 300, 234, 167, 100, 0,
		DEC_CURVE,CRV_BUFFERFULL, 1100, 867, 633, 400, 0,
		-2,

		/* motorcurve 3   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 5360, 3655, 2855, 2422, 2142, 1944, 1795, 1678, 1582, 1503, 1434, 1374, 0,
		ACC_CURVE,CRV_PARKHOME  , 5360, 3362, 2628, 2229, 1973, 1791, 1654, 1547, 1458, 1384, 1319, 1264, 1214, 1170, 1131, 1096, 1063, 1034, 1006, 981, 958, 937, 916, 897, 880, 863, 847, 832, 818, 805, 792, 780, 769, 758, 747, 737, 727, 718, 709, 700, 692, 684, 677, 669, 662, 655, 648, 642, 636, 629, 624, 618, 612, 607, 602, 596, 591, 587, 582, 577, 573, 568, 564, 560, 556, 552, 548, 544, 540, 537, 533, 530, 526, 523, 520, 516, 513, 510, 507, 504, 501, 498, 496, 493, 490, 488, 485, 482, 480, 477, 475, 472, 470, 468, 466, 463, 461, 459, 457, 455, 453, 450, 448, 446, 444, 443, 441, 439, 437, 435, 433, 431, 430, 428, 426, 425, 423, 421, 420, 418, 416, 415, 413, 412, 410, 409, 407, 406, 405, 403, 402, 400, 399, 398, 396, 395, 394, 392, 391, 390, 389, 387, 386, 385, 384, 382, 381, 380, 379, 378, 377, 376, 374, 373, 372, 371, 370, 369, 368, 367, 366, 365, 364, 363, 362, 361, 360, 359, 358, 357, 356, 355, 354, 353, 353, 352, 351, 350, 349, 348, 0,
		ACC_CURVE,CRV_SMEARING  , 5360, 3362, 2628, 2229, 1973, 1791, 1654, 1547, 0,
		DEC_CURVE,CRV_NORMALSCAN, 1374, 1434, 1503, 1582, 1678, 1795, 1944, 2142, 2422, 2855, 3655, 5360, 0,
		DEC_CURVE,CRV_PARKHOME  , 348, 351, 353, 356, 359, 362, 365, 368, 371, 374, 378, 381, 385, 389, 392, 396, 400, 405, 409, 413, 418, 423, 428, 433, 439, 444, 450, 457, 463, 470, 477, 485, 493, 501, 510, 520, 530, 540, 552, 564, 577, 591, 607, 624, 642, 662, 684, 709, 737, 769, 805, 847, 897, 958, 1034, 1131, 1264, 1458, 1791, 2628, 5360, 0,
		DEC_CURVE,CRV_SMEARING  , 1547, 1654, 1791, 1973, 2229, 2628, 3362, 5360, 0,
		DEC_CURVE,CRV_BUFFERFULL, 1374, 1434, 1503, 1582, 1678, 1795, 1944, 2142, 2422, 2855, 3655, 5360, 0,
		-2,

		/* motorcurve 4   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 4705, 2664, 2061, 1732, 1521, 1370, 1257, 1167, 1094, 1033, 982, 937, 898, 864, 833, 805, 780, 757, 736, 717, 699, 683, 667, 653, 640, 627, 615, 604, 593, 583, 574, 564, 556, 547, 540, 532, 525, 518, 511, 505, 499, 493, 487, 481, 476, 471, 466, 461, 456, 452, 447, 443, 439, 435, 431, 427, 424, 420, 417, 413, 410, 407, 403, 400, 397, 394, 391, 389, 386, 383, 381, 378, 375, 373, 371, 368, 366, 364, 361, 359, 357, 355, 353, 351, 349, 347, 345, 343, 341, 339, 338, 336, 334, 332, 331, 329, 327, 326, 324, 323, 321, 320, 318, 317, 315, 314, 312, 311, 310, 308, 307, 306, 304, 303, 302, 301, 299, 298, 297, 296, 295, 293, 292, 291, 290, 289, 288, 287, 286, 285, 284, 283, 282, 281, 280, 279, 278, 277, 276, 275, 274, 273, 272, 271, 270, 270, 269, 268, 267, 266, 265, 264, 264, 263, 262, 261, 260, 260, 259, 258, 257, 257, 256, 255, 255, 254, 253, 252, 252, 251, 250, 250, 249, 248, 248, 247, 246, 246, 245, 244, 244, 243, 242, 242, 241, 241, 240, 239, 239, 238, 238, 237, 237, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 230, 230, 229, 229, 228, 227, 227, 227, 226, 226, 225, 225, 224, 224, 223, 223, 222, 222, 221, 221, 220, 220, 219, 219, 219, 218, 218, 217, 217, 216, 216, 216, 215, 215, 214, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 210, 210, 209, 209, 208, 208, 208, 207, 207, 207, 206, 206, 206, 205, 205, 204, 204, 204, 203, 203, 203, 202, 202, 202, 201, 201, 201, 200, 200, 200, 199, 199, 199, 198, 198, 198, 197, 197, 197, 197, 196, 196, 196, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 192, 192, 192, 192, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 183, 183, 183, 183, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 171, 171, 171, 0,
		ACC_CURVE,CRV_PARKHOME  , 4705, 2888, 2234, 1878, 1648, 1485, 1362, 1265, 1186, 1120, 1064, 1016, 973, 936, 903, 873, 845, 821, 798, 777, 758, 740, 723, 708, 693, 679, 666, 654, 643, 632, 621, 612, 602, 593, 585, 576, 569, 561, 554, 547, 540, 534, 528, 522, 516, 510, 505, 499, 494, 490, 485, 480, 476, 471, 467, 463, 459, 455, 451, 448, 444, 440, 437, 434, 430, 427, 424, 421, 418, 415, 412, 409, 407, 404, 401, 399, 396, 394, 391, 389, 387, 384, 382, 380, 378, 376, 374, 371, 369, 367, 366, 364, 362, 360, 358, 356, 354, 353, 351, 349, 348, 346, 344, 343, 341, 340, 338, 337, 335, 334, 332, 331, 329, 328, 327, 325, 324, 323, 321, 320, 319, 318, 316, 315, 314, 313, 312, 311, 309, 308, 307, 306, 305, 304, 303, 302, 301, 300, 299, 298, 297, 296, 295, 294, 293, 292, 291, 290, 289, 288, 287, 286, 285, 285, 284, 283, 282, 281, 280, 279, 279, 278, 277, 276, 275, 275, 274, 273, 272, 272, 271, 270, 269, 269, 268, 267, 267, 266, 265, 264, 264, 263, 262, 262, 261, 260, 260, 259, 259, 258, 257, 257, 256, 255, 255, 254, 254, 253, 252, 252, 251, 251, 250, 249, 249, 248, 248, 247, 247, 246, 246, 245, 245, 244, 243, 243, 242, 242, 241, 241, 240, 240, 239, 239, 238, 238, 237, 237, 237, 236, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 231, 230, 230, 229, 229, 228, 228, 228, 227, 227, 226, 226, 225, 225, 225, 224, 224, 223, 223, 223, 222, 222, 222, 221, 221, 220, 220, 220, 219, 219, 219, 218, 218, 217, 217, 217, 216, 216, 216, 215, 215, 215, 214, 214, 214, 213, 213, 213, 212, 212, 212, 211, 211, 211, 210, 210, 210, 209, 209, 209, 208, 208, 208, 207, 207, 207, 207, 206, 206, 206, 205, 205, 205, 204, 204, 204, 204, 203, 203, 203, 202, 202, 202, 202, 201, 201, 201, 200, 200, 200, 200, 199, 199, 199, 199, 198, 198, 198, 198, 197, 197, 197, 196, 196, 196, 196, 195, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 193, 192, 192, 192, 192, 192, 191, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 187, 186, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 184, 183, 183, 183, 183, 183, 182, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 180, 180, 179, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 172, 171, 171, 171, 171, 0,
		ACC_CURVE,CRV_SMEARING  , 4705, 3056, 2724, 2497, 1498, 1498, 1374, 1276, 1196, 1130, 1073, 1025, 982, 944, 911, 880, 853, 828, 805, 784, 764, 746, 730, 714, 699, 685, 675, 0,
		DEC_CURVE,CRV_NORMALSCAN, 171, 172, 172, 173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 188, 189, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255, 258, 260, 262, 265, 268, 270, 273, 276, 279, 282, 285, 289, 292, 296, 299, 303, 307, 311, 316, 320, 325, 330, 335, 341, 347, 353, 359, 366, 373, 381, 390, 398, 408, 418, 429, 441, 455, 469, 485, 503, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		DEC_CURVE,CRV_PARKHOME  , 171, 172, 172, 173, 173, 174, 174, 175, 175, 176, 176, 177, 177, 178, 179, 179, 180, 180, 181, 182, 182, 183, 183, 184, 185, 185, 186, 187, 187, 188, 189, 189, 190, 191, 192, 192, 193, 194, 195, 195, 196, 197, 198, 199, 199, 200, 201, 202, 203, 204, 205, 206, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 217, 218, 219, 220, 221, 222, 223, 225, 226, 227, 228, 230, 231, 232, 234, 235, 237, 238, 240, 241, 243, 244, 246, 247, 249, 251, 253, 254, 256, 258, 260, 262, 264, 266, 268, 271, 273, 275, 278, 280, 282, 285, 288, 290, 293, 296, 299, 302, 305, 309, 312, 316, 319, 323, 327, 331, 336, 340, 345, 350, 355, 360, 365, 371, 377, 384, 391, 398, 406, 414, 422, 432, 441, 452, 463, 476, 489, 504, 520, 538, 558, 580, 605, 633, 667, 706, 752, 810, 883, 979, 1116, 1326, 1714, 4705, 0,
		DEC_CURVE,CRV_SMEARING  , 675, 685, 699, 714, 730, 746, 764, 784, 805, 828, 853, 880, 911, 944, 982, 1025, 1073, 1130, 1196, 1276, 1374, 1498, 1498, 2497, 2724, 3056, 4705, 0,
		DEC_CURVE,CRV_BUFFERFULL, 171, 172, 172, 173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 188, 189, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255, 258, 260, 262, 265, 268, 270, 273, 276, 279, 282, 285, 289, 292, 296, 299, 303, 307, 311, 316, 320, 325, 330, 335, 341, 347, 353, 359, 366, 373, 381, 390, 398, 408, 418, 429, 441, 455, 469, 485, 503, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		-2,

		/* motorcurve 5   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 3763, 3763, 3763, 3763, 3763, 3763, 2444, 2178, 1997, 1198, 1198, 1098, 1020, 956, 903, 858, 819, 785, 754, 727, 703, 681, 662, 644, 626, 610, 596, 582, 571, 558, 547, 537, 527, 518, 509, 500, 492, 485, 478, 471, 464, 458, 452, 446, 440, 435, 430, 425, 420, 415, 410, 407, 402, 398, 394, 391, 386, 383, 380, 376, 373, 369, 366, 364, 360, 357, 355, 352, 349, 347, 344, 341, 338, 337, 334, 332, 329, 328, 325, 323, 321, 319, 317, 315, 313, 311, 310, 308, 306, 304, 302, 301, 299, 297, 295, 294, 293, 291, 290, 288, 286, 285, 284, 283, 281, 280, 278, 277, 275, 275, 274, 272, 271, 270, 268, 267, 266, 265, 264, 263, 262, 261, 259, 258, 257, 257, 256, 255, 254, 253, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 243, 242, 241, 240, 239, 239, 238, 237, 236, 235, 235, 234, 233, 232, 231, 230, 230, 230, 229, 228, 228, 227, 226, 225, 225,224, 223, 222, 222, 221, 221, 221, 220, 219, 219, 218, 217, 217, 216, 215, 215, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 206, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 190, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 185, 185, 185, 185, 185, 185, 184, 184, 183, 183, 183, 182, 182, 182, 181, 181, 180, 180, 180, 179, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 171, 170, 170, 170, 169, 169, 169, 169, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 159, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 138, 138, 138, 138, 138, 138, 137, 137, 137, 137, 137, 137, 137, 136, 136, 136, 136, 136, 136, 136, 135, 135, 135, 135, 135, 135, 135, 134, 134, 134, 134, 134, 134, 134, 134, 133, 133, 133, 133, 133, 133, 133, 133, 132, 132, 132, 132, 132, 132, 132, 132, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 130, 130, 130, 130, 130, 130, 130, 130, 129, 129, 129, 129, 129, 129, 129, 129, 129, 128, 128, 128, 128, 128, 128, 128, 128, 128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126, 126, 126, 126, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 101, 101, 101, 101, 101, 101, 101, 101, 101,101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 0,
		ACC_CURVE,CRV_PARKHOME  , 3763, 2330, 1803, 1515, 1330, 1198, 1099, 1021, 957, 904, 859, 819, 785, 755, 728, 704, 682, 662, 644, 626, 611, 597, 583, 571, 559, 548, 537, 527, 518, 509, 501, 493, 485, 478, 472, 464, 458, 453, 446, 441, 436, 430, 425, 420, 416, 411, 407, 402, 399, 394, 391, 387, 383, 380, 376, 374, 370, 366, 364, 361, 358, 355, 352, 349, 347, 344, 342, 339, 337, 335, 332, 330, 328, 326, 323, 321, 320, 318, 315, 313, 311, 310, 308, 306, 304, 302, 301, 300, 298, 296, 294, 293, 292, 290, 289, 287, 285, 284, 283, 282, 280, 279, 277, 276, 275, 274, 273, 271, 270, 269, 267, 266, 266, 265, 263, 262, 261, 260, 259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 239, 239, 238, 237, 236, 235, 234, 233, 233, 232, 231, 230, 230, 230, 229, 228, 227, 227, 226, 225, 224, 224, 223, 222, 221, 221, 221, 220, 220, 219, 218, 218, 217, 216, 216, 215, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 205, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 191, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 186, 185, 185, 185, 185, 185, 184, 184, 184, 183, 183, 182, 182, 182, 181, 181, 181, 180, 180, 180, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 170, 170, 170, 170, 169, 169, 169, 168, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 160, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 0,
		ACC_CURVE,CRV_SMEARING  , 3763, 2330, 1803, 1515, 1330, 1198, 1099, 1021, 957, 904, 859, 819, 785, 755, 728, 704, 682, 662, 644, 626, 611, 597, 583, 571, 559, 548, 537, 527, 518, 509, 501, 493, 485, 478, 472, 464, 458, 453, 446, 441, 436, 430, 425, 420, 416, 411, 407, 402, 399, 394, 391, 387, 383, 380, 376, 374, 370, 366, 364, 361, 358, 355, 352, 349, 347, 344, 342, 339, 337, 335, 332, 330, 328, 326, 323, 321, 320, 318, 315, 313, 311, 310, 308, 306, 304, 302, 301, 300, 298, 296, 294, 293, 292, 290, 289, 287, 285, 284, 283, 282, 280, 279, 277, 276, 275, 274, 273, 271, 270, 269, 267, 266, 266, 265, 263, 262, 261, 260, 259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 239, 239, 238, 237, 236, 235, 234, 233, 233, 232, 231, 230, 230, 230, 229, 228, 227, 227, 226, 225, 224, 224, 223, 222, 221, 221, 221, 220, 220, 219, 218, 218, 217, 216, 216, 215, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 205, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 191, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 186, 185, 185, 185, 185, 185, 184, 184, 184, 183, 183, 182, 182, 182, 181, 181, 181, 180, 180, 180, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 170, 170, 170, 170, 169, 169, 169, 168, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 160, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 0,
		DEC_CURVE,CRV_NORMALSCAN, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 278, 280, 282, 284, 286, 288, 290, 292, 294, 296, 298, 300, 302, 304, 306, 308, 310, 312, 314, 316, 318, 320, 322, 324, 326, 328, 330, 332, 334, 336, 338, 340, 342, 345, 348, 351, 354, 357, 360, 363, 366, 369, 372, 375, 378, 381, 384, 387, 391, 395, 399, 403, 407, 411, 415, 419, 422, 425, 429, 433, 437, 441, 445, 449, 453, 458, 463, 468, 473, 478, 483, 489, 495, 501, 507, 514, 521, 528, 536, 544, 553, 562, 572, 582, 593, 604, 616, 629, 643, 658, 674, 692, 711, 732, 755, 781, 810, 843, 880, 923, 974,1035, 1110, 1205, 1331, 1510, 1796, 2368, 3400, 4922, 0,
		DEC_CURVE,CRV_PARKHOME  , 138, 142, 146, 149, 153, 158, 162, 167, 171, 176, 180, 186, 193, 202, 209, 216, 223, 232, 243, 254, 266, 279, 292, 306, 320, 335, 337, 351, 367, 380, 396, 414, 437, 464, 493, 520, 549, 583, 611, 644, 675, 711, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		DEC_CURVE,CRV_SMEARING  , 138, 139, 139, 139, 139, 139, 139, 139, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 141, 141, 141, 141, 141, 141, 141, 142, 142, 142, 142, 142, 142, 143, 143, 143, 143, 143, 143, 144, 144, 144, 144, 144, 144, 145, 145, 145, 145, 145, 145, 146, 146, 146, 146, 146, 147, 147, 147, 147, 147, 147, 148, 148, 148, 148, 148, 148, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 150, 150, 150, 150, 150, 151, 151, 151, 151, 151, 152, 152, 152, 152, 152, 153, 153, 153, 153, 153, 154, 154, 154, 154, 154, 155, 155, 155, 155, 155, 156, 156, 156, 156, 157, 157, 157, 157, 157, 158, 158, 158, 158, 158, 158, 158, 158, 159, 159, 159, 159, 160, 160, 160, 160, 160, 161, 161, 161, 161, 162, 162, 162, 162, 163, 163, 163, 163, 164, 164, 164, 165, 165, 165, 165, 166, 166, 166, 166, 167, 167, 167, 167, 167, 167, 167, 168, 168, 168, 168, 169, 169, 169, 170, 170, 170, 170, 171, 171, 171, 172, 172, 172, 173, 173, 173, 174, 174, 174, 174, 175, 175, 175, 176, 176, 176, 176, 176, 176, 177, 177, 177, 178, 178, 178, 179, 179, 180, 180, 180, 181, 181, 181, 182, 182, 182, 183, 183, 184, 184, 184, 185, 185, 185, 185, 185, 186, 186, 186, 187, 187, 188, 188, 188, 189, 189, 190, 190, 191, 191, 191, 192, 192, 193, 193, 194, 194, 194, 194, 194, 195, 195, 196, 196, 197, 197, 198, 198, 199, 199, 200, 200, 201, 201, 202, 202, 203, 203, 203, 203, 204, 204, 205, 205, 206, 207, 207, 208, 208, 209, 209, 210, 211, 211, 212, 212, 212, 213, 213, 214, 214, 215, 216, 216, 217, 218, 218, 219, 220, 220, 221, 221, 221, 222, 223, 224, 224, 225, 226, 227, 227, 228, 229, 230, 230, 230, 231, 232, 233, 233, 234, 235, 236, 237, 238, 239, 239, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 265, 266, 266, 267, 269, 270, 271, 273, 274, 275, 276, 277, 279, 280, 282, 283, 284, 285, 287, 289, 290, 292, 293, 294, 296, 298, 300, 301, 302, 304, 306, 308, 310, 311, 313, 315, 318, 320, 321, 323, 326, 328, 330, 332, 335, 337, 339, 342, 344, 347, 349, 352, 355, 358, 361, 364, 366, 370, 374, 376, 380, 383, 387, 391, 394, 399, 402, 407, 411, 416, 420, 425, 430, 436, 441, 446, 453, 458, 464, 472, 478, 485, 493, 501, 509, 518, 527, 537, 548, 559, 571, 583, 597, 611, 626, 644, 662, 682, 704, 728, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		DEC_CURVE,CRV_BUFFERFULL, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 94, 94, 94, 95, 95, 95, 95, 95, 96, 96, 97, 97, 98, 98, 99, 99, 100, 100, 101, 101, 102, 102, 103, 104, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 113, 115, 117, 119, 121, 122, 124, 127, 130, 132, 135, 139, 142, 146, 149, 153, 158, 162, 167, 171, 176, 180, 186, 193, 202, 209, 216, 223, 232, 243, 254, 266, 279, 292, 306, 320, 335, 337, 351, 367, 380, 396, 414, 437, 464, 493, 520, 549, 583, 611, 644, 675, 711, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		-2,

		/* motorcurve 6   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 4705, 2664, 2061, 1732, 1521, 1370, 1257, 1167, 1094, 1033, 982, 937, 898, 864, 833, 805, 780, 757, 736, 717, 699, 683, 667, 653, 640, 627, 615, 604, 593, 583, 574, 564, 556, 547, 540, 532, 525, 518, 511, 505, 499, 493, 487, 481, 476, 471, 466, 461, 456, 452, 447, 443, 439, 435, 431, 427, 424, 420, 417, 413, 410, 407, 403, 400, 397, 394, 391, 389, 386, 383, 381, 378, 375, 373, 371, 368, 366, 364, 361, 359, 357, 355, 353, 351, 349, 347, 345, 343, 341, 339, 338, 336, 334, 332, 331, 329, 327, 326, 324, 323, 321, 320, 318, 317, 315, 314, 312, 311, 310, 308, 307, 306, 304, 303, 302, 301, 299, 298, 297, 296, 295, 293, 292, 291, 290, 289, 288, 287, 286, 285, 284, 283, 282, 281, 280, 279, 278, 277, 276, 275, 274, 273, 272, 271, 270, 270, 269, 268, 267, 266, 265, 264, 264, 263, 262, 261, 260, 260, 259, 258, 257, 257, 256, 255, 255, 254, 253, 252, 252, 251, 250, 250, 249, 248, 248, 247, 246, 246, 245, 244, 244, 243, 242, 242, 241, 241, 240, 239, 239, 238, 238, 237, 237, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 230, 230, 229, 229, 228, 227, 227, 227, 226, 226, 225, 225, 224, 224, 223, 223, 222, 222, 221, 221, 220, 220, 219, 219, 219, 218, 218, 217, 217, 216, 216, 216, 215, 215, 214, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 210, 210, 209, 209, 208, 208, 208, 207, 207, 207, 206, 206, 206, 205, 205, 204, 204, 204, 203, 203, 203, 202, 202, 202, 201, 201, 201, 200, 200, 200, 199, 199, 199, 198, 198, 198, 197, 197, 197, 197, 196, 196, 196, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 192, 192, 192, 192, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 183, 183, 183, 183, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 171, 171, 171, 0,
		ACC_CURVE,CRV_PARKHOME  , 4705, 2888, 2234, 1878, 1648, 1485, 1362, 1265, 1186, 1120, 1064, 1016, 973, 936, 903, 873, 845, 821, 798, 777, 758, 740, 723, 708, 693, 679, 666, 654, 643, 632, 621, 612, 602, 593, 585, 576, 569, 561, 554, 547, 540, 534, 528, 522, 516, 510, 505, 499, 494, 490, 485, 480, 476, 471, 467, 463, 459, 455, 451, 448, 444, 440, 437, 434, 430, 427, 424, 421, 418, 415, 412, 409, 407, 404, 401, 399, 396, 394, 391, 389, 387, 384, 382, 380, 378, 376, 374, 371, 369, 367, 366, 364, 362, 360, 358, 356, 354, 353, 351, 349, 348, 346, 344, 343, 341, 340, 338, 337, 335, 334, 332, 331, 329, 328, 327, 325, 324, 323, 321, 320, 319, 318, 316, 315, 314, 313, 312, 311, 309, 308, 307, 306, 305, 304, 303, 302, 301, 300, 299, 298, 297, 296, 295, 294, 293, 292, 291, 290, 289, 288, 287, 286, 285, 285, 284, 283, 282, 281, 280, 279, 279, 278, 277, 276, 275, 275, 274, 273, 272, 272, 271, 270, 269, 269, 268, 267, 267, 266, 265, 264, 264, 263, 262, 262, 261, 260, 260, 259, 259, 258, 257, 257, 256, 255, 255, 254, 254, 253, 252, 252, 251, 251, 250, 249, 249, 248, 248, 247, 247, 246, 246, 245, 245, 244, 243, 243, 242, 242, 241, 241, 240, 240, 239, 239, 238, 238, 237, 237, 237, 236, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 231, 230, 230, 229, 229, 228, 228, 228, 227, 227, 226, 226, 225, 225, 225, 224, 224, 223, 223, 223, 222, 222, 222, 221, 221, 220, 220, 220, 219, 219, 219, 218, 218, 217, 217, 217, 216, 216, 216, 215, 215, 215, 214, 214, 214, 213, 213, 213, 212, 212, 212, 211, 211, 211, 210, 210, 210, 209, 209, 209, 208, 208, 208, 207, 207, 207, 207, 206, 206, 206, 205, 205, 205, 204, 204, 204, 204, 203, 203, 203, 202, 202, 202, 202, 201, 201, 201, 200, 200, 200, 200, 199, 199, 199, 199, 198, 198, 198, 198, 197, 197, 197, 196, 196, 196, 196, 195, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 193, 192, 192, 192, 192, 192, 191, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 187, 186, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 184, 183, 183, 183, 183, 183, 182, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 180, 180, 179, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 172, 171, 171, 171, 171, 0,
		ACC_CURVE,CRV_SMEARING  , 4705, 3056, 2724, 2497, 1498, 1498, 1374, 1276, 1196, 1130, 1073, 1025, 982, 944, 911, 880, 853, 828, 805, 784, 764, 746, 730, 714, 699, 685, 675, 0,
		DEC_CURVE,CRV_NORMALSCAN, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		DEC_CURVE,CRV_PARKHOME  , 171, 172, 172, 173, 173, 174, 174, 175, 175, 176, 176, 177, 177, 178, 179, 179, 180, 180, 181, 182, 182, 183, 183, 184, 185, 185, 186, 187, 187, 188, 189, 189, 190, 191, 192, 192, 193, 194, 195, 195, 196, 197, 198, 199, 199, 200, 201, 202, 203, 204, 205, 206, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 217, 218, 219, 220, 221, 222, 223, 225, 226, 227, 228, 230, 231, 232, 234, 235, 237, 238, 240, 241, 243, 244, 246, 247, 249, 251, 253, 254, 256, 258, 260, 262, 264, 266, 268, 271, 273, 275, 278, 280, 282, 285, 288, 290, 293, 296, 299, 302, 305, 309, 312, 316, 319, 323, 327, 331, 336, 340, 345, 350, 355, 360, 365, 371, 377, 384, 391, 398, 406, 414, 422, 432, 441, 452, 463, 476, 489, 504, 520, 538, 558, 580, 605, 633, 667, 706, 752, 810, 883, 979, 1116, 1326, 1714, 4705, 0,
		DEC_CURVE,CRV_SMEARING  , 675, 685, 699, 714, 730, 746, 764, 784, 805, 828, 853, 880, 911, 944, 982, 1025, 1073, 1130, 1196, 1276, 1374, 1498, 1498, 2497, 2724, 3056, 4705, 0,
		DEC_CURVE,CRV_BUFFERFULL, 171, 172, 172, 173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 188, 189, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255, 258, 260, 262, 265, 268, 270, 273, 276, 279, 282, 285, 289, 292, 296, 299, 303, 307, 311, 316, 320, 325, 330, 335, 341, 347, 353, 359, 366, 373, 381, 390, 398, 408, 418, 429, 441, 455, 469, 485, 503, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		-2,

		/* motorcurve 7   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 3763, 3763, 3763, 3763, 3763, 3763, 2444, 2178, 1997, 1198, 1198, 1098, 1020, 956, 903, 858, 819, 785, 754, 727, 703, 681, 662, 644, 626, 610, 596, 582, 571, 558, 547, 537, 527, 518, 509, 500, 492, 485, 478, 471, 464, 458, 452, 446, 440, 435, 430, 425, 420, 415, 410, 407, 402, 398, 394, 391, 386, 383, 380, 376, 373, 369, 366, 364, 360, 357, 355, 352, 349, 347, 344, 341, 338, 337, 334, 332, 329, 328, 325, 323, 321, 319, 317, 315, 313, 311, 310, 308, 306, 304, 302, 301, 299, 297, 295, 294, 293, 291, 290, 288, 286, 285, 284, 283, 281, 280, 278, 277, 275, 275, 274, 272, 271, 270, 268, 267, 266, 265, 264, 263, 262, 261, 259, 258, 257, 257, 256, 255, 254, 253, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 243, 242, 241, 240, 239, 239, 238, 237, 236, 235, 235, 234, 233, 232, 231, 230, 230, 230, 229, 228, 228, 227, 226, 225, 225,224, 223, 222, 222, 221, 221, 221, 220, 219, 219, 218, 217, 217, 216, 215, 215, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 206, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 190, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 185, 185, 185, 185, 185, 185, 184, 184, 183, 183, 183, 182, 182, 182, 181, 181, 180, 180, 180, 179, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 171, 170, 170, 170, 169, 169, 169, 169, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 159, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 138, 138, 138, 138, 138, 138, 137, 137, 137, 137, 137, 137, 137, 136, 136, 136, 136, 136, 136, 136, 135, 135, 135, 135, 135, 135, 135, 134, 134, 134, 134, 134, 134, 134, 134, 133, 133, 133, 133, 133, 133, 133, 133, 132, 132, 132, 132, 132, 132, 132, 132, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 130, 130, 130, 130, 130, 130, 130, 130, 129, 129, 129, 129, 129, 129, 129, 129, 129, 128, 128, 128, 128, 128, 128, 128, 128, 128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126, 126, 126, 126, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 101, 101, 101, 101, 101, 101, 101, 101, 101,101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 0,
		ACC_CURVE,CRV_PARKHOME  , 3763, 2330, 1803, 1515, 1330, 1198, 1099, 1021, 957, 904, 859, 819, 785, 755, 728, 704, 682, 662, 644, 626, 611, 597, 583, 571, 559, 548, 537, 527, 518, 509, 501, 493, 485, 478, 472, 464, 458, 453, 446, 441, 436, 430, 425, 420, 416, 411, 407, 402, 399, 394, 391, 387, 383, 380, 376, 374, 370, 366, 364, 361, 358, 355, 352, 349, 347, 344, 342, 339, 337, 335, 332, 330, 328, 326, 323, 321, 320, 318, 315, 313, 311, 310, 308, 306, 304, 302, 301, 300, 298, 296, 294, 293, 292, 290, 289, 287, 285, 284, 283, 282, 280, 279, 277, 276, 275, 274, 273, 271, 270, 269, 267, 266, 266, 265, 263, 262, 261, 260, 259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 239, 239, 238, 237, 236, 235, 234, 233, 233, 232, 231, 230, 230, 230, 229, 228, 227, 227, 226, 225, 224, 224, 223, 222, 221, 221, 221, 220, 220, 219, 218, 218, 217, 216, 216, 215, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 205, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 191, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 186, 185, 185, 185, 185, 185, 184, 184, 184, 183, 183, 182, 182, 182, 181, 181, 181, 180, 180, 180, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 170, 170, 170, 170, 169, 169, 169, 168, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 160, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 0,
		ACC_CURVE,CRV_SMEARING  , 3763, 2330, 1803, 1515, 1330, 1198, 1099, 1021, 957, 904, 859, 819, 785, 755, 728, 704, 682, 662, 644, 626, 611, 597, 583, 571, 559, 548, 537, 527, 518, 509, 501, 493, 485, 478, 472, 464, 458, 453, 446, 441, 436, 430, 425, 420, 416, 411, 407, 402, 399, 394, 391, 387, 383, 380, 376, 374, 370, 366, 364, 361, 358, 355, 352, 349, 347, 344, 342, 339, 337, 335, 332, 330, 328, 326, 323, 321, 320, 318, 315, 313, 311, 310, 308, 306, 304, 302, 301, 300, 298, 296, 294, 293, 292, 290, 289, 287, 285, 284, 283, 282, 280, 279, 277, 276, 275, 274, 273, 271, 270, 269, 267, 266, 266, 265, 263, 262, 261, 260, 259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 239, 239, 238, 237, 236, 235, 234, 233, 233, 232, 231, 230, 230, 230, 229, 228, 227, 227, 226, 225, 224, 224, 223, 222, 221, 221, 221, 220, 220, 219, 218, 218, 217, 216, 216, 215, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 205, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 191, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 186, 185, 185, 185, 185, 185, 184, 184, 184, 183, 183, 182, 182, 182, 181, 181, 181, 180, 180, 180, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 170, 170, 170, 170, 169, 169, 169, 168, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 160, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 0,
		DEC_CURVE,CRV_NORMALSCAN, 522, 522, 528, 536, 544, 553, 562, 572, 582, 593, 604, 616, 629, 643, 658, 674, 692, 711, 732, 755, 781, 810, 843, 880, 923, 974, 1035, 1110, 1205, 1331, 1510, 1796, 2368, 3400, 4922, 0,
		DEC_CURVE,CRV_PARKHOME  , 138, 142, 146, 149, 153, 158, 162, 167, 171, 176, 180, 186, 193, 202, 209, 216, 223, 232, 243, 254, 266, 279, 292, 306, 320, 335, 337, 351, 367, 380, 396, 414, 437, 464, 493, 520, 549, 583, 611, 644, 675, 711, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		DEC_CURVE,CRV_SMEARING  , 138, 139, 139, 139, 139, 139, 139, 139, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 141, 141, 141, 141, 141, 141, 141, 142, 142, 142, 142, 142, 142, 143, 143, 143, 143, 143, 143, 144, 144, 144, 144, 144, 144, 145, 145, 145, 145, 145, 145, 146, 146, 146, 146, 146, 147, 147, 147, 147, 147, 147, 148, 148, 148, 148, 148, 148, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 150, 150, 150, 150, 150, 151, 151, 151, 151, 151, 152, 152, 152, 152, 152, 153, 153, 153, 153, 153, 154, 154, 154, 154, 154, 155, 155, 155, 155, 155, 156, 156, 156, 156, 157, 157, 157, 157, 157, 158, 158, 158, 158, 158, 158, 158, 158, 159, 159, 159, 159, 160, 160, 160, 160, 160, 161, 161, 161, 161, 162, 162, 162, 162, 163, 163, 163, 163, 164, 164, 164, 165, 165, 165, 165, 166, 166, 166, 166, 167, 167, 167, 167, 167, 167, 167, 168, 168, 168, 168, 169, 169, 169, 170, 170, 170, 170, 171, 171, 171, 172, 172, 172, 173, 173, 173, 174, 174, 174, 174, 175, 175, 175, 176, 176, 176, 176, 176, 176, 177, 177, 177, 178, 178, 178, 179, 179, 180, 180, 180, 181, 181, 181, 182, 182, 182, 183, 183, 184, 184, 184, 185, 185, 185, 185, 185, 186, 186, 186, 187, 187, 188, 188, 188, 189, 189, 190, 190, 191, 191, 191, 192, 192, 193, 193, 194, 194, 194, 194, 194, 195, 195, 196, 196, 197, 197, 198, 198, 199, 199, 200, 200, 201, 201, 202, 202, 203, 203, 203, 203, 204, 204, 205, 205, 206, 207, 207, 208, 208, 209, 209, 210, 211, 211, 212, 212, 212, 213, 213, 214, 214, 215, 216, 216, 217, 218, 218, 219, 220, 220, 221, 221, 221, 222, 223, 224, 224, 225, 226, 227, 227, 228, 229, 230, 230, 230, 231, 232, 233, 233, 234, 235, 236, 237, 238, 239, 239, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 265, 266, 266, 267, 269, 270, 271, 273, 274, 275, 276, 277, 279, 280, 282, 283, 284, 285, 287, 289, 290, 292, 293, 294, 296, 298, 300, 301, 302, 304, 306, 308, 310, 311, 313, 315, 318, 320, 321, 323, 326, 328, 330, 332, 335, 337, 339, 342, 344, 347, 349, 352, 355, 358, 361, 364, 366, 370, 374, 376, 380, 383, 387, 391, 394, 399, 402, 407, 411, 416, 420, 425, 430, 436, 441, 446, 453, 458, 464, 472, 478, 485, 493, 501, 509, 518, 527, 537, 548, 559, 571, 583, 597, 611, 626, 644, 662, 682, 704, 728, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		DEC_CURVE,CRV_BUFFERFULL, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 94, 94, 94, 95, 95, 95, 95, 95, 96, 96, 97, 97, 98, 98, 99, 99, 100, 100, 101, 101, 102, 102, 103, 104, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 113, 115, 117, 119, 121, 122, 124, 127, 130, 132, 135, 139, 142, 146, 149, 153, 158, 162, 167, 171, 176, 180, 186, 193, 202, 209, 216, 223, 232, 243, 254, 266, 279, 292, 306, 320, 335, 337, 351, 367, 380, 396, 414, 437, 464, 493, 520, 549, 583, 611, 644, 675, 711, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		-2,

		/* motorcurve 8   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 1046, 1046, 1046, 1046, 1046, 1046, 647, 501, 421, 370, 333, 305, 284, 266, 251, 239, 228, 218, 210, 202, 196, 190, 184, 179, 174, 170, 166, 162, 159, 155, 152, 149, 147, 144, 142, 139, 137, 135, 133, 131, 129, 127, 126, 124, 123, 121, 120, 118, 117, 116, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 100, 99, 98, 97, 96, 96, 95, 94, 94, 93, 92, 92, 91, 91, 90, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 83, 82, 82, 82, 81, 81, 80, 80, 79, 79, 79, 78, 78, 78, 77, 77, 76, 76, 76, 75, 75, 75, 74, 74, 74, 74, 73, 73, 73, 72, 72, 72, 71, 71, 71, 71, 70, 70, 70, 70, 69, 69, 69, 69, 68, 68, 68, 68, 67, 67, 67, 67, 66, 66, 66, 66, 65, 65, 65, 65, 64, 64, 64, 64, 63, 63, 63, 63, 62, 62, 62, 62, 61, 61, 61, 61, 61, 60, 60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 57, 57, 57, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54, 54, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 23, 23, 0,
		ACC_CURVE,CRV_PARKHOME  , 1045, 678, 604, 554, 332, 332, 304, 283, 265, 250, 238, 227, 217, 209, 201, 195, 189, 183, 178,173, 169, 165, 161, 158, 154, 151, 148, 146, 143, 141, 138, 136, 134, 132, 130, 128, 126, 125, 123, 122, 120, 119, 117, 116, 115, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103,102, 101, 100, 99, 99, 98, 97, 96, 95, 95, 94, 93, 93, 92, 91, 91, 90, 90, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 83, 82, 82, 81, 81, 81, 80, 80, 79, 79, 78, 78, 78, 77, 77, 77, 76, 76, 75, 75, 75, 74, 74, 74, 73, 73, 73, 73, 72, 72, 72, 71, 71, 71, 70, 70, 70, 70, 69, 69, 69, 69, 68, 68, 68, 68, 67, 67, 67, 67, 66, 66, 66, 66, 65, 65, 65, 65, 65, 64, 64, 64, 64, 64, 63, 63, 63, 63, 63, 62, 62, 62, 62, 62, 61, 61, 61, 61, 61, 61, 60, 60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 58, 57, 57, 57, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54, 54, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 0,
		ACC_CURVE,CRV_SMEARING  , 1045, 678, 604, 554, 332, 332, 304, 283, 265, 250, 238, 227, 217, 209, 201, 195, 189, 183, 178,173, 169, 165, 161, 158, 154, 151, 148, 146, 143, 141, 138, 136, 134, 132, 130, 128, 126, 125, 123, 122, 120, 119, 117, 116, 115, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103,102, 101, 100, 99, 99, 98, 97, 96, 95, 95, 94, 93, 93, 92, 91, 91, 90, 90, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 83, 82, 82, 81, 81, 81, 80, 80, 79, 79, 78, 78, 78, 77, 77, 77, 76, 76, 75, 75, 75, 74, 74, 74, 73, 73, 73, 73, 72, 72, 72, 71, 71, 71, 70, 70, 70, 70, 69, 69, 69, 69, 68, 68, 68, 68, 67, 67, 67, 67, 66, 66, 66, 66, 65, 65, 65, 65, 65, 64, 64, 64, 64, 64, 63, 63, 63, 63, 63, 62, 62, 62, 62, 62, 61, 61, 61, 61, 61, 61, 60, 60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 58, 57, 57, 57, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54, 54, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 0,
		DEC_CURVE,CRV_NORMALSCAN, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 31, 32, 32, 32, 32, 33, 33, 34, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 50, 51, 53, 54, 57, 59, 61, 63, 65, 68, 71, 75, 78, 82, 86, 90, 94, 94, 98, 103, 106, 111, 116, 122, 130, 138, 145, 153, 163, 171, 180, 188, 198, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		DEC_CURVE,CRV_PARKHOME  , 39, 40, 41, 42, 43, 45, 46, 47, 48, 50, 51, 53, 54, 57, 59, 61, 63, 65, 68, 71, 75, 78, 82, 86, 90, 94, 94, 98, 103, 106, 111, 116, 122, 130, 138, 145, 153, 163, 171, 180, 188, 198, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		DEC_CURVE,CRV_SMEARING  , 39, 39, 39, 39, 39, 39, 39, 39, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 54, 54, 54, 54, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 55, 55, 55, 55, 56, 56, 56, 56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58, 58, 58, 59, 59, 59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 61, 61, 61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 63, 63, 63, 63, 63, 64, 64, 64, 64, 64, 65, 65, 65, 65, 65, 66, 66, 66, 66, 66, 67, 67, 67, 67, 67, 68, 68, 68, 68, 69, 69, 69, 69, 70, 70, 70, 70, 71, 71, 71, 71, 72, 72, 72, 73, 73, 73, 73, 74, 74, 74, 75, 75, 75, 76, 76, 76, 77, 77, 77, 78, 78, 78, 79, 79, 79, 80, 80, 81, 81, 81, 82, 82, 83, 83, 84, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 89, 90, 90, 91, 91, 92, 93, 93, 94, 94, 95, 96, 96, 97, 98, 99, 99, 100, 101, 102, 103, 104, 105, 105, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116, 118, 119, 120, 122, 123, 125, 127, 128, 130, 132, 134, 136, 138, 140, 142, 145, 147, 150, 153, 156, 159, 163, 167, 171, 175, 180, 185, 190, 196, 203, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		DEC_CURVE,CRV_BUFFERFULL, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 31, 32, 32, 32, 32, 33, 33, 34, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 50, 51, 53, 54, 57, 59, 61, 63, 65, 68, 71, 75, 78, 82, 86, 90, 94, 94, 98, 103, 106, 111, 116, 122, 130, 138, 145, 153, 163, 171, 180, 188, 198, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		-1
	};

	rst = (SANE_Int *)malloc(sizeof(steps));
	if (rst != NULL)
		memcpy(rst, &steps, sizeof(steps));

	return rst;
}

static SANE_Int *hp3970_motor()
{
	SANE_Int *rst = NULL;
	SANE_Int steps[]  =
	{
		/* motorcurve 1   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 2000, 1984, 1968, 1953, 1937, 1921, 1906, 1890, 1874, 1859, 1843, 1827, 1812, 1796, 1781, 1765, 1749, 1734, 1715, 1700, 1684, 1669, 1653, 1637, 1622, 1606, 1590, 1572, 1556, 1541, 1525, 1510, 1494, 1478, 1463, 1447, 1431, 1416, 1400, 1384, 1366, 1351, 1335, 1319, 1304, 1288, 1272, 1257, 1241, 1225, 1210, 1194, 1179, 1160, 1145, 1129, 1113, 1098, 1082, 1066, 1051, 1035, 1017, 1001, 986, 970, 954, 939, 923, 907, 892, 876, 861, 845, 829, 814, 798, 782, 767, 749, 0,
		ACC_CURVE,CRV_PARKHOME  , 4705, 2913, 2253, 1894, 1662, 1498, 1374, 1276, 1196, 1129, 1073, 1024, 981, 944, 910, 880, 852, 827, 804, 783, 764, 746, 729, 713, 699, 685, 672, 659, 648, 637, 626, 616, 607, 598, 589, 581, 573, 565, 558, 551, 544, 538, 532, 526, 520, 514, 509, 503, 500, 0,
		ACC_CURVE,CRV_SMEARING  , 200, 12, 14, 16, 0,
		DEC_CURVE,CRV_NORMALSCAN, 749, 1166, 1583, 2000, 0,
		DEC_CURVE,CRV_PARKHOME  , 500, 503, 509, 514, 520, 526, 532, 538, 544, 551, 558, 565, 573, 581, 589, 598, 607, 616, 626, 637, 648, 659, 672, 685, 699, 713, 729, 746, 764, 783, 804, 827, 852, 880, 910, 944, 981, 1024, 1073, 1129, 1196, 1276, 1374, 1498, 1662, 1894, 2253, 2913, 4705, 0,
		DEC_CURVE,CRV_SMEARING  , 300, 234, 167, 100, 0,
		DEC_CURVE,CRV_BUFFERFULL, 1100, 867, 633, 400, 0,
		-2,

		/* motorcurve 2   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 4705, 2664, 2061, 1732, 1521, 1370, 1257, 1167, 1094, 1033, 982, 937, 898, 864, 833, 805, 780, 757, 736, 717, 699, 683, 667, 653, 640, 627, 615, 604, 593, 583, 574, 564, 556, 547, 540, 532, 525, 518, 511, 505, 499, 493, 487, 481, 476, 471, 466, 461, 456, 452, 447, 443, 439, 435, 431, 427, 424, 420, 417, 413, 410, 407, 403, 400, 397, 394, 391, 389, 386, 383, 381, 378, 375, 373, 371, 368, 366, 364, 361, 359, 357, 355, 353, 351, 349, 347, 345, 343, 341, 339, 338, 336, 334, 332, 331, 329, 327, 326, 324, 323, 321, 320, 318, 317, 315, 314, 312, 311, 310, 308, 307, 306, 304, 303, 302, 301, 299, 298, 297, 296, 295, 293, 292, 291, 290, 289, 288, 287, 286, 285, 284, 283, 282, 281, 280, 279, 278, 277, 276, 275, 274, 273, 272, 271, 270, 270, 269, 268, 267, 266, 265, 264, 264, 263, 262, 261, 260, 260, 259, 258, 257, 257, 256, 255, 255, 254, 253, 252, 252, 251, 250, 250, 249, 248, 248, 247, 246, 246, 245, 244, 244, 243, 242, 242, 241, 241, 240, 239, 239, 238, 238, 237, 237, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 230, 230, 229, 229, 228, 227, 227, 227, 226, 226, 225, 225, 224, 224, 223, 223, 222, 222, 221, 221, 220, 220, 219, 219, 219, 218, 218, 217, 217, 216, 216, 216, 215, 215, 214, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 210, 210, 209, 209, 208, 208, 208, 207, 207, 207, 206, 206, 206, 205, 205, 204, 204, 204, 203, 203, 203, 202, 202, 202, 201, 201, 201, 200, 200, 200, 199, 199, 199, 198, 198, 198, 197, 197, 197, 197, 196, 196, 196, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 192, 192, 192, 192, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 183, 183, 183, 183, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 171, 171, 171, 0,
		ACC_CURVE,CRV_PARKHOME  , 4705, 2913, 2253, 1894, 1662, 1498, 1374, 1276, 1196, 1129, 1073, 1024, 981, 944, 910, 880, 852, 827, 804, 783, 764, 746, 729, 713, 699, 685, 672, 659, 648, 637, 626, 616, 607, 598, 589, 581, 573, 565, 558, 551, 544, 538, 532, 526, 520, 514, 509, 503, 500, 0,
		ACC_CURVE,CRV_SMEARING  , 200, 12, 14, 16, 0,
		DEC_CURVE,CRV_NORMALSCAN, 171, 172, 172, 173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 188, 189, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255, 258, 260, 262, 265, 268, 270, 273, 276, 279, 282, 285, 289, 292, 296, 299, 303, 307, 311, 316, 320, 325, 330, 335, 341, 347, 353, 359, 366, 373, 381, 390, 398, 408, 418, 429, 441, 455, 469, 485, 503, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		DEC_CURVE,CRV_PARKHOME  , 500, 503, 509, 514, 520, 526, 532, 538, 544, 551, 558, 565, 573, 581, 589, 598, 607, 616, 626, 637, 648, 659, 672, 685, 699, 713, 729, 746, 764, 783, 804, 827, 852, 880, 910, 944, 981, 1024, 1073, 1129, 1196, 1276, 1374, 1498, 1662, 1894, 2253, 2913, 4705, 0,
		DEC_CURVE,CRV_SMEARING  , 300, 234, 167, 100, 0,
		DEC_CURVE,CRV_BUFFERFULL, 1100, 867, 633, 400, 0,
		-2,

		/* motorcurve 3   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 5360, 3655, 2855, 2422, 2142, 1944, 1795, 1678, 1582, 1503, 1434, 1374, 0,
		ACC_CURVE,CRV_PARKHOME  , 5360, 3362, 2628, 2229, 1973, 1791, 1654, 1547, 1458, 1384, 1319, 1264, 1214, 1170, 1131, 1096, 1063, 1034, 1006, 981, 958, 937, 916, 897, 880, 863, 847, 832, 818, 805, 792, 780, 769, 758, 747, 737, 727, 718, 709, 700, 692, 684, 677, 669, 662, 655, 648, 642, 636, 629, 624, 618, 612, 607, 602, 596, 591, 587, 582, 577, 573, 568, 564, 560, 556, 552, 548, 544, 540, 537, 533, 530, 526, 523, 520, 516, 513, 510, 507, 504, 501, 498, 496, 493, 490, 488, 485, 482, 480, 477, 475, 472, 470, 468, 466, 463, 461, 459, 457, 455, 453, 450, 448, 446, 444, 443, 441, 439, 437, 435, 433, 431, 430, 428, 426, 425, 423, 421, 420, 418, 416, 415, 413, 412, 410, 409, 407, 406, 405, 403, 402, 400, 399, 398, 396, 395, 394, 392, 391, 390, 389, 387, 386, 385, 384, 382, 381, 380, 379, 378, 377, 376, 374, 373, 372, 371, 370, 369, 368, 367, 366, 365, 364, 363, 362, 361, 360, 359, 358, 357, 356, 355, 354, 353, 353, 352, 351, 350, 349, 348, 0,
		ACC_CURVE,CRV_SMEARING  , 5360, 3362, 2628, 2229, 1973, 1791, 1654, 1547, 0,
		DEC_CURVE,CRV_NORMALSCAN, 1374, 1434, 1503, 1582, 1678, 1795, 1944, 2142, 2422, 2855, 3655, 5360, 0,
		DEC_CURVE,CRV_PARKHOME  , 348, 351, 353, 356, 359, 362, 365, 368, 371, 374, 378, 381, 385, 389, 392, 396, 400, 405, 409, 413, 418, 423, 428, 433, 439, 444, 450, 457, 463, 470, 477, 485, 493, 501, 510, 520, 530, 540, 552, 564, 577, 591, 607, 624, 642, 662, 684, 709, 737, 769, 805, 847, 897, 958, 1034, 1131, 1264, 1458, 1791, 2628, 5360, 0,
		DEC_CURVE,CRV_SMEARING  , 1547, 1654, 1791, 1973, 2229, 2628, 3362, 5360, 0,
		DEC_CURVE,CRV_BUFFERFULL, 1374, 1434, 1503, 1582, 1678, 1795, 1944, 2142, 2422, 2855, 3655, 5360, 0,
		-2,

		/* motorcurve 4   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 4705, 2664, 2061, 1732, 1521, 1370, 1257, 1167, 1094, 1033, 982, 937, 898, 864, 833, 805, 780, 757, 736, 717, 699, 683, 667, 653, 640, 627, 615, 604, 593, 583, 574, 564, 556, 547, 540, 532, 525, 518, 511, 505, 499, 493, 487, 481, 476, 471, 466, 461, 456, 452, 447, 443, 439, 435, 431, 427, 424, 420, 417, 413, 410, 407, 403, 400, 397, 394, 391, 389, 386, 383, 381, 378, 375, 373, 371, 368, 366, 364, 361, 359, 357, 355, 353, 351, 349, 347, 345, 343, 341, 339, 338, 336, 334, 332, 331, 329, 327, 326, 324, 323, 321, 320, 318, 317, 315, 314, 312, 311, 310, 308, 307, 306, 304, 303, 302, 301, 299, 298, 297, 296, 295, 293, 292, 291, 290, 289, 288, 287, 286, 285, 284, 283, 282, 281, 280, 279, 278, 277, 276, 275, 274, 273, 272, 271, 270, 270, 269, 268, 267, 266, 265, 264, 264, 263, 262, 261, 260, 260, 259, 258, 257, 257, 256, 255, 255, 254, 253, 252, 252, 251, 250, 250, 249, 248, 248, 247, 246, 246, 245, 244, 244, 243, 242, 242, 241, 241, 240, 239, 239, 238, 238, 237, 237, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 230, 230, 229, 229, 228, 227, 227, 227, 226, 226, 225, 225, 224, 224, 223, 223, 222, 222, 221, 221, 220, 220, 219, 219, 219, 218, 218, 217, 217, 216, 216, 216, 215, 215, 214, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 210, 210, 209, 209, 208, 208, 208, 207, 207, 207, 206, 206, 206, 205, 205, 204, 204, 204, 203, 203, 203, 202, 202, 202, 201, 201, 201, 200, 200, 200, 199, 199, 199, 198, 198, 198, 197, 197, 197, 197, 196, 196, 196, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 192, 192, 192, 192, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 183, 183, 183, 183, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 171, 171, 171, 0,
		ACC_CURVE,CRV_PARKHOME  , 4705, 2888, 2234, 1878, 1648, 1485, 1362, 1265, 1186, 1120, 1064, 1016, 973, 936, 903, 873, 845, 821, 798, 777, 758, 740, 723, 708, 693, 679, 666, 654, 643, 632, 621, 612, 602, 593, 585, 576, 569, 561, 554, 547, 540, 534, 528, 522, 516, 510, 505, 499, 494, 490, 485, 480, 476, 471, 467, 463, 459, 455, 451, 448, 444, 440, 437, 434, 430, 427, 424, 421, 418, 415, 412, 409, 407, 404, 401, 399, 396, 394, 391, 389, 387, 384, 382, 380, 378, 376, 374, 371, 369, 367, 366, 364, 362, 360, 358, 356, 354, 353, 351, 349, 348, 346, 344, 343, 341, 340, 338, 337, 335, 334, 332, 331, 329, 328, 327, 325, 324, 323, 321, 320, 319, 318, 316, 315, 314, 313, 312, 311, 309, 308, 307, 306, 305, 304, 303, 302, 301, 300, 299, 298, 297, 296, 295, 294, 293, 292, 291, 290, 289, 288, 287, 286, 285, 285, 284, 283, 282, 281, 280, 279, 279, 278, 277, 276, 275, 275, 274, 273, 272, 272, 271, 270, 269, 269, 268, 267, 267, 266, 265, 264, 264, 263, 262, 262, 261, 260, 260, 259, 259, 258, 257, 257, 256, 255, 255, 254, 254, 253, 252, 252, 251, 251, 250, 249, 249, 248, 248, 247, 247, 246, 246, 245, 245, 244, 243, 243, 242, 242, 241, 241, 240, 240, 239, 239, 238, 238, 237, 237, 237, 236, 236, 235, 235, 234, 234, 233, 233, 232, 232, 231, 231, 231, 230, 230, 229, 229, 228, 228, 228, 227, 227, 226, 226, 225, 225, 225, 224, 224, 223, 223, 223, 222, 222, 222, 221, 221, 220, 220, 220, 219, 219, 219, 218, 218, 217, 217, 217, 216, 216, 216, 215, 215, 215, 214, 214, 214, 213, 213, 213, 212, 212, 212, 211, 211, 211, 210, 210, 210, 209, 209, 209, 208, 208, 208, 207, 207, 207, 207, 206, 206, 206, 205, 205, 205, 204, 204, 204, 204, 203, 203, 203, 202, 202, 202, 202, 201, 201, 201, 200, 200, 200, 200, 199, 199, 199, 199, 198, 198, 198, 198, 197, 197, 197, 196, 196, 196, 196, 195, 195, 195, 195, 194, 194, 194, 194, 193, 193, 193, 193, 192, 192, 192, 192, 192, 191, 191, 191, 191, 190, 190, 190, 190, 189, 189, 189, 189, 189, 188, 188, 188, 188, 187, 187, 187, 187, 187, 186, 186, 186, 186, 186, 185, 185, 185, 185, 184, 184, 184, 184, 184, 183, 183, 183, 183, 183, 182, 182, 182, 182, 182, 181, 181, 181, 181, 181, 180, 180, 180, 180, 180, 180, 179, 179, 179, 179, 179, 178, 178, 178, 178, 178, 177, 177, 177, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 175, 175, 174, 174, 174, 174, 174, 174, 173, 173, 173, 173, 173, 173, 172, 172, 172, 172, 172, 172, 171, 171, 171, 171, 0,
		ACC_CURVE,CRV_SMEARING  , 4705, 3056, 2724, 2497, 1498, 1498, 1374, 1276, 1196, 1130, 1073, 1025, 982, 944, 911, 880, 853, 828, 805, 784, 764, 746, 730, 714, 699, 685, 675, 0,
		DEC_CURVE,CRV_NORMALSCAN, 171, 172, 172, 173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 188, 189, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255, 258, 260, 262, 265, 268, 270, 273, 276, 279, 282, 285, 289, 292, 296, 299, 303, 307, 311, 316, 320, 325, 330, 335, 341, 347, 353, 359, 366, 373, 381, 390, 398, 408, 418, 429, 441, 455, 469, 485, 503, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		DEC_CURVE,CRV_PARKHOME  , 171, 172, 172, 173, 173, 174, 174, 175, 175, 176, 176, 177, 177, 178, 179, 179, 180, 180, 181, 182, 182, 183, 183, 184, 185, 185, 186, 187, 187, 188, 189, 189, 190, 191, 192, 192, 193, 194, 195, 195, 196, 197, 198, 199, 199, 200, 201, 202, 203, 204, 205, 206, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 217, 218, 219, 220, 221, 222, 223, 225, 226, 227, 228, 230, 231, 232, 234, 235, 237, 238, 240, 241, 243, 244, 246, 247, 249, 251, 253, 254, 256, 258, 260, 262, 264, 266, 268, 271, 273, 275, 278, 280, 282, 285, 288, 290, 293, 296, 299, 302, 305, 309, 312, 316, 319, 323, 327, 331, 336, 340, 345, 350, 355, 360, 365, 371, 377, 384, 391, 398, 406, 414, 422, 432, 441, 452, 463, 476, 489, 504, 520, 538, 558, 580, 605, 633, 667, 706, 752, 810, 883, 979, 1116, 1326, 1714, 4705, 0,
		DEC_CURVE,CRV_SMEARING  , 675, 685, 699, 714, 730, 746, 764, 784, 805, 828, 853, 880, 911, 944, 982, 1025, 1073, 1130, 1196, 1276, 1374, 1498, 1498, 2497, 2724, 3056, 4705, 0,
		DEC_CURVE,CRV_BUFFERFULL, 171, 172, 172, 173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 188, 189, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215, 217, 218, 219, 221, 222, 224, 225, 227, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255, 258, 260, 262, 265, 268, 270, 273, 276, 279, 282, 285, 289, 292, 296, 299, 303, 307, 311, 316, 320, 325, 330, 335, 341, 347, 353, 359, 366, 373, 381, 390, 398, 408, 418, 429, 441, 455, 469, 485, 503, 523, 545, 571, 601, 636, 678, 730, 796, 883, 1005, 1195, 1544, 4705, 0,
		-2,

		/* motorcurve 5   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 3763, 3763, 3763, 3763, 3763, 3763, 2444, 2178, 1997, 1198, 1198, 1098, 1020, 956, 903, 858, 819, 785, 754, 727, 703, 681, 662, 644, 626, 610, 596, 582, 571, 558, 547, 537, 527, 518, 509, 500, 492, 485, 478, 471, 464, 458, 452, 446, 440, 435, 430, 425, 420, 415, 410, 407, 402, 398, 394, 391, 386, 383, 380, 376, 373, 369, 366, 364, 360, 357, 355, 352, 349, 347, 344, 341, 338, 337, 334, 332, 329, 328, 325, 323, 321, 319, 317, 315, 313, 311, 310, 308, 306, 304, 302, 301, 299, 297, 295, 294, 293, 291, 290, 288, 286, 285, 284, 283, 281, 280, 278, 277, 275, 275, 274, 272, 271, 270, 268, 267, 266, 265, 264, 263, 262, 261, 259, 258, 257, 257, 256, 255, 254, 253, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 243, 242, 241, 240, 239, 239, 238, 237, 236, 235, 235, 234, 233, 232, 231, 230, 230, 230, 229, 228, 228, 227, 226, 225, 225,224, 223, 222, 222, 221, 221, 221, 220, 219, 219, 218, 217, 217, 216, 215, 215, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 206, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 190, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 185, 185, 185, 185, 185, 185, 184, 184, 183, 183, 183, 182, 182, 182, 181, 181, 180, 180, 180, 179, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 171, 170, 170, 170, 169, 169, 169, 169, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 159, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 138, 138, 138, 138, 138, 138, 137, 137, 137, 137, 137, 137, 137, 136, 136, 136, 136, 136, 136, 136, 135, 135, 135, 135, 135, 135, 135, 134, 134, 134, 134, 134, 134, 134, 134, 133, 133, 133, 133, 133, 133, 133, 133, 132, 132, 132, 132, 132, 132, 132, 132, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 130, 130, 130, 130, 130, 130, 130, 130, 129, 129, 129, 129, 129, 129, 129, 129, 129, 128, 128, 128, 128, 128, 128, 128, 128, 128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126, 126, 126, 126, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 101, 101, 101, 101, 101, 101, 101, 101, 101,101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 0,
		ACC_CURVE,CRV_PARKHOME  , 3763, 2330, 1803, 1515, 1330, 1198, 1099, 1021, 957, 904, 859, 819, 785, 755, 728, 704, 682, 662, 644, 626, 611, 597, 583, 571, 559, 548, 537, 527, 518, 509, 501, 493, 485, 478, 472, 464, 458, 453, 446, 441, 436, 430, 425, 420, 416, 411, 407, 402, 399, 394, 391, 387, 383, 380, 376, 374, 370, 366, 364, 361, 358, 355, 352, 349, 347, 344, 342, 339, 337, 335, 332, 330, 328, 326, 323, 321, 320, 318, 315, 313, 311, 310, 308, 306, 304, 302, 301, 300, 298, 296, 294, 293, 292, 290, 289, 287, 285, 284, 283, 282, 280, 279, 277, 276, 275, 274, 273, 271, 270, 269, 267, 266, 266, 265, 263, 262, 261, 260, 259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 239, 239, 238, 237, 236, 235, 234, 233, 233, 232, 231, 230, 230, 230, 229, 228, 227, 227, 226, 225, 224, 224, 223, 222, 221, 221, 221, 220, 220, 219, 218, 218, 217, 216, 216, 215, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 205, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 191, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 186, 185, 185, 185, 185, 185, 184, 184, 184, 183, 183, 182, 182, 182, 181, 181, 181, 180, 180, 180, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 170, 170, 170, 170, 169, 169, 169, 168, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 160, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 0,
		ACC_CURVE,CRV_SMEARING  , 3763, 2330, 1803, 1515, 1330, 1198, 1099, 1021, 957, 904, 859, 819, 785, 755, 728, 704, 682, 662, 644, 626, 611, 597, 583, 571, 559, 548, 537, 527, 518, 509, 501, 493, 485, 478, 472, 464, 458, 453, 446, 441, 436, 430, 425, 420, 416, 411, 407, 402, 399, 394, 391, 387, 383, 380, 376, 374, 370, 366, 364, 361, 358, 355, 352, 349, 347, 344, 342, 339, 337, 335, 332, 330, 328, 326, 323, 321, 320, 318, 315, 313, 311, 310, 308, 306, 304, 302, 301, 300, 298, 296, 294, 293, 292, 290, 289, 287, 285, 284, 283, 282, 280, 279, 277, 276, 275, 274, 273, 271, 270, 269, 267, 266, 266, 265, 263, 262, 261, 260, 259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 239, 239, 238, 237, 236, 235, 234, 233, 233, 232, 231, 230, 230, 230, 229, 228, 227, 227, 226, 225, 224, 224, 223, 222, 221, 221, 221, 220, 220, 219, 218, 218, 217, 216, 216, 215, 214, 214, 213, 213, 212, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 207, 206, 205, 205, 204, 204, 203, 203, 203, 203, 202, 202, 201, 201, 200, 200, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 194, 194, 194, 193, 193, 192, 192, 191, 191, 191, 190, 190, 189, 189, 188, 188, 188, 187, 187, 186, 186, 186, 185, 185, 185, 185, 185, 184, 184, 184, 183, 183, 182, 182, 182, 181, 181, 181, 180, 180, 180, 179, 179, 178, 178, 178, 177, 177, 177, 176, 176, 176, 176, 176, 176, 175, 175, 175, 174, 174, 174, 174, 173, 173, 173, 172, 172, 172, 171, 171, 171, 170, 170, 170, 170, 169, 169, 169, 168, 168, 168, 168, 167, 167, 167, 167, 167, 167, 167, 166, 166, 166, 166, 165, 165, 165, 165, 164, 164, 164, 163, 163, 163, 163, 162, 162, 162, 162, 161, 161, 161, 161, 160, 160, 160, 160, 160, 159, 159, 159, 159, 158, 158, 158, 158, 158, 158, 158, 158, 157, 157, 157, 157, 157, 156, 156, 156, 156, 155, 155, 155, 155, 155, 154, 154, 154, 154, 154, 153, 153, 153, 153, 153, 152, 152, 152, 152, 152, 151, 151, 151, 151, 151, 150, 150, 150, 150, 150, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 148, 147, 147, 147, 147, 147, 147, 146, 146, 146, 146, 146, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144, 144, 143, 143, 143, 143, 143, 143, 142, 142, 142, 142, 142, 142, 141, 141, 141, 141, 141, 141, 141, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 139, 139, 139, 139, 139, 139, 139, 138, 0,
		DEC_CURVE,CRV_NORMALSCAN, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 87, 87, 87, 87, 87, 87, 87, 87, 88, 88, 88, 88, 88, 88, 88, 88, 89, 89, 89, 89, 89, 89, 89, 90, 90, 90, 90, 90, 90, 91, 91, 91, 91, 91, 92, 92, 92, 92, 93, 93, 93, 93, 94, 94, 94, 95, 95, 95, 95, 95, 96, 96, 97, 97, 98, 98, 99, 99, 100, 100, 101, 101, 102, 102, 103, 104, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 113, 115, 117, 119, 121, 122, 124, 127, 130, 132, 135, 139, 142, 146, 149, 153, 158, 162, 167, 171, 176, 180, 186, 193, 202, 209, 216, 223, 232, 243, 254, 266, 279, 292, 306, 320, 335, 337, 351, 367, 380, 396, 414, 437, 464, 493, 520, 549, 583, 611, 644, 675, 711, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		DEC_CURVE,CRV_PARKHOME  , 138, 142, 146, 149, 153, 158, 162, 167, 171, 176, 180, 186, 193, 202, 209, 216, 223, 232, 243, 254, 266, 279, 292, 306, 320, 335, 337, 351, 367, 380, 396, 414, 437, 464, 493, 520, 549, 583, 611, 644, 675, 711, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		DEC_CURVE,CRV_SMEARING  , 138, 139, 139, 139, 139, 139, 139, 139, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 141, 141, 141, 141, 141, 141, 141, 142, 142, 142, 142, 142, 142, 143, 143, 143, 143, 143, 143, 144, 144, 144, 144, 144, 144, 145, 145, 145, 145, 145, 145, 146, 146, 146, 146, 146, 147, 147, 147, 147, 147, 147, 148, 148, 148, 148, 148, 148, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 150, 150, 150, 150, 150, 151, 151, 151, 151, 151, 152, 152, 152, 152, 152, 153, 153, 153, 153, 153, 154, 154, 154, 154, 154, 155, 155, 155, 155, 155, 156, 156, 156, 156, 157, 157, 157, 157, 157, 158, 158, 158, 158, 158, 158, 158, 158, 159, 159, 159, 159, 160, 160, 160, 160, 160, 161, 161, 161, 161, 162, 162, 162, 162, 163, 163, 163, 163, 164, 164, 164, 165, 165, 165, 165, 166, 166, 166, 166, 167, 167, 167, 167, 167, 167, 167, 168, 168, 168, 168, 169, 169, 169, 170, 170, 170, 170, 171, 171, 171, 172, 172, 172, 173, 173, 173, 174, 174, 174, 174, 175, 175, 175, 176, 176, 176, 176, 176, 176, 177, 177, 177, 178, 178, 178, 179, 179, 180, 180, 180, 181, 181, 181, 182, 182, 182, 183, 183, 184, 184, 184, 185, 185, 185, 185, 185, 186, 186, 186, 187, 187, 188, 188, 188, 189, 189, 190, 190, 191, 191, 191, 192, 192, 193, 193, 194, 194, 194, 194, 194, 195, 195, 196, 196, 197, 197, 198, 198, 199, 199, 200, 200, 201, 201, 202, 202, 203, 203, 203, 203, 204, 204, 205, 205, 206, 207, 207, 208, 208, 209, 209, 210, 211, 211, 212, 212, 212, 213, 213, 214, 214, 215, 216, 216, 217, 218, 218, 219, 220, 220, 221, 221, 221, 222, 223, 224, 224, 225, 226, 227, 227, 228, 229, 230, 230, 230, 231, 232, 233, 233, 234, 235, 236, 237, 238, 239, 239, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 265, 266, 266, 267, 269, 270, 271, 273, 274, 275, 276, 277, 279, 280, 282, 283, 284, 285, 287, 289, 290, 292, 293, 294, 296, 298, 300, 301, 302, 304, 306, 308, 310, 311, 313, 315, 318, 320, 321, 323, 326, 328, 330, 332, 335, 337, 339, 342, 344, 347, 349, 352, 355, 358, 361, 364, 366, 370, 374, 376, 380, 383, 387, 391, 394, 399, 402, 407, 411, 416, 420, 425, 430, 436, 441, 446, 453, 458, 464, 472, 478, 485, 493, 501, 509, 518, 527, 537, 548, 559, 571, 583, 597, 611, 626, 644, 662, 682, 704, 728, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		DEC_CURVE,CRV_BUFFERFULL, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 94, 94, 94, 95, 95, 95, 95, 95, 96, 96, 97, 97, 98, 98, 99, 99, 100, 100, 101, 101, 102, 102, 103, 104, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 113, 115, 117, 119, 121, 122, 124, 127, 130, 132, 135, 139, 142, 146, 149, 153, 158, 162, 167, 171, 176, 180, 186, 193, 202, 209, 216, 223, 232, 243, 254, 266, 279, 292, 306, 320, 335, 337, 351, 367, 380, 396, 414, 437, 464, 493, 520, 549, 583, 611, 644, 675, 711, 755, 785, 819, 859, 904, 957, 1021, 1099, 1198, 1330, 1515, 1803, 2330, 3763, 0,
		-2,

		/* motorcurve 6   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 23999, 0,
		ACC_CURVE,CRV_PARKHOME  , 5360, 3362, 2628, 2229, 1973, 1791, 1654, 1547, 1458, 1384, 1319, 1264, 1214, 1170, 1131, 1096, 1063, 1034, 1006, 981, 958, 937, 916, 897, 880, 863, 847, 832, 818, 805, 792, 780, 769, 758, 747, 737, 727, 718, 709, 700, 692, 0,
		ACC_CURVE,CRV_SMEARING  , 23999, 0,
		DEC_CURVE,CRV_NORMALSCAN, 23999, 0,
		DEC_CURVE,CRV_PARKHOME  , 692, 700, 709, 718, 727, 737, 747, 758, 769, 780, 792, 805, 818, 832, 847, 863, 880, 897, 916, 937, 958, 981, 1006, 1034, 1063, 1096, 1131, 1170, 1214, 1264, 1319, 1384, 1458, 1547, 1654, 1791, 1973, 2229, 2628, 3362, 5360, 0,
		DEC_CURVE,CRV_SMEARING  , 23999, 0,
		DEC_CURVE,CRV_BUFFERFULL, 23999, 0,
		-2,

		/* motorcurve 7   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 6667, 0,
		ACC_CURVE,CRV_PARKHOME  , 5360, 3362, 2628, 2229, 1973, 1791, 1654, 1547, 1458, 1384, 1319, 1264, 1214, 1170, 1131, 1096, 1063, 1034, 1006, 981, 958, 937, 916, 897, 880, 863, 847, 832, 818, 805, 792, 780, 769, 758, 747, 737, 727, 718, 709, 700, 692, 0,
		ACC_CURVE,CRV_SMEARING  , 6667, 0,
		DEC_CURVE,CRV_NORMALSCAN, 6667, 0,
		DEC_CURVE,CRV_PARKHOME  , 692, 700, 709, 718, 727, 737, 747, 758, 769, 780, 792, 805, 818, 832, 847, 863, 880, 897, 916, 937, 958, 981, 1006, 1034, 1063, 1096, 1131, 1170, 1214, 1264, 1319, 1384, 1458, 1547, 1654, 1791, 1973, 2229, 2628, 3362, 5360, 0,
		DEC_CURVE,CRV_SMEARING  , 6667, 0,
		DEC_CURVE,CRV_BUFFERFULL, 6667, 0,
		-2,

		/* motorcurve 8   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN, 1046, 1046, 1046, 1046, 1046, 1046, 647, 501, 421, 370, 333, 305, 284, 266, 251, 239, 228, 218, 210, 202, 196, 190, 184, 179, 174, 170, 166, 162, 159, 155, 152, 149, 147, 144, 142, 139, 137, 135, 133, 131, 129, 127, 126, 124, 123, 121, 120, 118, 117, 116, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 100, 99, 98, 97, 96, 96, 95, 94, 94, 93, 92, 92, 91, 91, 90, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 83, 82, 82, 82, 81, 81, 80, 80, 79, 79, 79, 78, 78, 78, 77, 77, 76, 76, 76, 75, 75, 75, 74, 74, 74, 74, 73, 73, 73, 72, 72, 72, 71, 71, 71, 71, 70, 70, 70, 70, 69, 69, 69, 69, 68, 68, 68, 68, 67, 67, 67, 67, 66, 66, 66, 66, 65, 65, 65, 65, 64, 64, 64, 64, 63, 63, 63, 63, 62, 62, 62, 62, 61, 61, 61, 61, 61, 60, 60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 57, 57, 57, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54, 54, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 23, 23, 0,
		ACC_CURVE,CRV_PARKHOME  , 1045, 678, 604, 554, 332, 332, 304, 283, 265, 250, 238, 227, 217, 209, 201, 195, 189, 183, 178,173, 169, 165, 161, 158, 154, 151, 148, 146, 143, 141, 138, 136, 134, 132, 130, 128, 126, 125, 123, 122, 120, 119, 117, 116, 115, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103,102, 101, 100, 99, 99, 98, 97, 96, 95, 95, 94, 93, 93, 92, 91, 91, 90, 90, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 83, 82, 82, 81, 81, 81, 80, 80, 79, 79, 78, 78, 78, 77, 77, 77, 76, 76, 75, 75, 75, 74, 74, 74, 73, 73, 73, 73, 72, 72, 72, 71, 71, 71, 70, 70, 70, 70, 69, 69, 69, 69, 68, 68, 68, 68, 67, 67, 67, 67, 66, 66, 66, 66, 65, 65, 65, 65, 65, 64, 64, 64, 64, 64, 63, 63, 63, 63, 63, 62, 62, 62, 62, 62, 61, 61, 61, 61, 61, 61, 60, 60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 58, 57, 57, 57, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54, 54, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 0,
		ACC_CURVE,CRV_SMEARING  , 1045, 678, 604, 554, 332, 332, 304, 283, 265, 250, 238, 227, 217, 209, 201, 195, 189, 183, 178,173, 169, 165, 161, 158, 154, 151, 148, 146, 143, 141, 138, 136, 134, 132, 130, 128, 126, 125, 123, 122, 120, 119, 117, 116, 115, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103,102, 101, 100, 99, 99, 98, 97, 96, 95, 95, 94, 93, 93, 92, 91, 91, 90, 90, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 83, 82, 82, 81, 81, 81, 80, 80, 79, 79, 78, 78, 78, 77, 77, 77, 76, 76, 75, 75, 75, 74, 74, 74, 73, 73, 73, 73, 72, 72, 72, 71, 71, 71, 70, 70, 70, 70, 69, 69, 69, 69, 68, 68, 68, 68, 67, 67, 67, 67, 66, 66, 66, 66, 65, 65, 65, 65, 65, 64, 64, 64, 64, 64, 63, 63, 63, 63, 63, 62, 62, 62, 62, 62, 61, 61, 61, 61, 61, 61, 60, 60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 58, 57, 57, 57, 57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54, 54, 54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 0,
		DEC_CURVE,CRV_NORMALSCAN, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 31, 32, 32, 32, 32, 33, 33, 34, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 50, 51, 53, 54, 57, 59, 61, 63, 65, 68, 71, 75, 78, 82, 86, 90, 94, 94, 98, 103, 106, 111, 116, 122, 130, 138, 145, 153, 163, 171, 180, 188, 198, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		DEC_CURVE,CRV_PARKHOME  , 39, 40, 41, 42, 43, 45, 46, 47, 48, 50, 51, 53, 54, 57, 59, 61, 63, 65, 68, 71, 75, 78, 82, 86, 90, 94, 94, 98, 103, 106, 111, 116, 122, 130, 138, 145, 153, 163, 171, 180, 188, 198, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		DEC_CURVE,CRV_SMEARING  , 39, 39, 39, 39, 39, 39, 39, 39, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 54, 54, 54, 54, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 55, 55, 55, 55, 56, 56, 56, 56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58, 58, 58, 59, 59, 59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 61, 61, 61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 63, 63, 63, 63, 63, 64, 64, 64, 64, 64, 65, 65, 65, 65, 65, 66, 66, 66, 66, 66, 67, 67, 67, 67, 67, 68, 68, 68, 68, 69, 69, 69, 69, 70, 70, 70, 70, 71, 71, 71, 71, 72, 72, 72, 73, 73, 73, 73, 74, 74, 74, 75, 75, 75, 76, 76, 76, 77, 77, 77, 78, 78, 78, 79, 79, 79, 80, 80, 81, 81, 81, 82, 82, 83, 83, 84, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 89, 90, 90, 91, 91, 92, 93, 93, 94, 94, 95, 96, 96, 97, 98, 99, 99, 100, 101, 102, 103, 104, 105, 105, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116, 118, 119, 120, 122, 123, 125, 127, 128, 130, 132, 134, 136, 138, 140, 142, 145, 147, 150, 153, 156, 159, 163, 167, 171, 175, 180, 185, 190, 196, 203, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		DEC_CURVE,CRV_BUFFERFULL, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 31, 32, 32, 32, 32, 33, 33, 34, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 50, 51, 53, 54, 57, 59, 61, 63, 65, 68, 71, 75, 78, 82, 86, 90, 94, 94, 98, 103, 106, 111, 116, 122, 130, 138, 145, 153, 163, 171, 180, 188, 198, 211, 219, 228, 239, 252, 267, 284, 306, 334, 370, 422, 502, 648, 1045, 0,
		-1
	};

	rst = (SANE_Int *)malloc(sizeof(steps));
	if (rst != NULL)
		memcpy(rst, &steps, sizeof(steps));

	return rst;
}

static SANE_Int *hp3800_motor()
{
	SANE_Int *rst = NULL;
	SANE_Int steps[]  =
	{
		/* motorcurve 1   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,2000,1984,1968,1953,1937,1921,1906,1890,1874,1859,1843,1827,1812,1796,1781,1765,1749,1734,1715,1700,1684,1669,1653,1637,1622,1606,1590,1572,1556,1541,1525,1510,1494,1478,1463,1447,1431,1416,1400,1384,1366,1351,1335,1319,1304,1288,1272,1257,1241,1225,1210,1194,1179,1160,1145,1129,1113,1098,1082,1066,1051,1035,1017,1001,986,970,954,939,923,907,892,876,861,845,829,814,798,782,767,749, 0,
		ACC_CURVE,CRV_PARKHOME  ,4705,2913,2253,1894,1662,1498,1374,1276,1196,1129,1073,1024,981,944,910,880,852,827,804,783,764,746,729,713,699,685,672,659,648,637,626,616,607,598,589,581,573,565,558,551,544,538,532,526,520,514,509,503,500, 0,
		ACC_CURVE,CRV_SMEARING  ,200,12,14,16, 0,
		DEC_CURVE,CRV_NORMALSCAN,749,1166,1583,2000, 0,
		DEC_CURVE,CRV_PARKHOME  ,500,503,509,514,520,526,532,538,544,551,558,565,573,581,589,598,607,616,626,637,648,659,672,685,699,713,729,746,764,783,804,827,852,880,910,944,981,1024,1073,1129,1196,1276,1374,1498,1662,1894,2253,2913,4705, 0,
		DEC_CURVE,CRV_SMEARING  ,300,234,167,100, 0,
		DEC_CURVE,CRV_BUFFERFULL,1100,867,633,400, 0,
		-2,

		/* motorcurve 2   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,4705,2664,2061,1732,1521,1370,1257,1167,1094,1033,982,937,898,864,833,805,780,757,736,717,699,683,667,653,640,627,615,604,593,583,574,564,556,547,540,532,525,518,511,505,499,493,487,481,476,471,466,461,456,452,447,443,439,435,431,427,424,420,417,413,410,407,403,400,397,394,391,389,386,383,381,378,375,373,371,368,366,364,361,359,357,355,353,351,349,347,345,343,341,339,338,336,334,332,331,329,327,326,324,323,321,320,318,317,315,314,312,311,310,308,307,306,304,303,302,301,299,298,297,296,295,293,292,291,290,289,288,287,286,285,284,283,282,281,280,279,278,277,276,275,274,273,272,271,270,270,269,268,267,266,265,264,264,263,262,261,260,260,259,258,257,257,256,255,255,254,253,252,252,251,250,250,249,248,248,247,246,246,245,244,244,243,242,242,241,241,240,239,239,238,238,237,237,236,235,235,234,234,233,233,232,232,231,231,230,230,229,229,228,227,227,227,226,226,225,225,224,224,223,223,222,222,221,221,220,220,219,219,219,218,218,217,217,216,216,216,215,215,214,214,214,213,213,212,212,212,211,211,210,210,210,209,209,208,208,208,207,207,207,206,206,206,205,205,204,204,204,203,203,203,202,202,202,201,201,201,200,200,200,199,199,199,198,198,198,197,197,197,197,196,196,196,195,195,195,194,194,194,194,193,193,193,192,192,192,192,191,191,191,190,190,190,190,189,189,189,188,188,188,188,187,187,187,187,186,186,186,186,185,185,185,185,184,184,184,184,183,183,183,183,182,182,182,182,181,181,181,181,181,180,180,180,180,179,179,179,179,178,178,178,178,178,177,177,177,177,177,176,176,176,176,175,175,175,175,175,174,174,174,174,174,173,173,173,173,173,172,172,172,172,172,171,171,171, 0,
		ACC_CURVE,CRV_PARKHOME  ,4705,2913,2253,1894,1662,1498,1374,1276,1196,1129,1073,1024,981,944,910,880,852,827,804,783,764,746,729,713,699,685,672,659,648,637,626,616,607,598,589,581,573,565,558,551,544,538,532,526,520,514,509,503,500, 0,
		ACC_CURVE,CRV_SMEARING  ,200,12,14,16, 0,
		DEC_CURVE,CRV_NORMALSCAN,171,172,172,173,174,174,175,176,176,177,178,178,179,180,181,181,182,183,184,184,185,186,187,188,189,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,207,208,209,210,211,213,214,215,217,218,219,221,222,224,225,227,228,230,232,233,235,237,239,241,243,245,247,249,251,253,255,258,260,262,265,268,270,273,276,279,282,285,289,292,296,299,303,307,311,316,320,325,330,335,341,347,353,359,366,373,381,390,398,408,418,429,441,455,469,485,503,523,545,571,601,636,678,730,796,883,1005,1195,1544,4705, 0,
		DEC_CURVE,CRV_PARKHOME  ,500,503,509,514,520,526,532,538,544,551,558,565,573,581,589,598,607,616,626,637,648,659,672,685,699,713,729,746,764,783,804,827,852,880,910,944,981,1024,1073,1129,1196,1276,1374,1498,1662,1894,2253,2913,4705, 0,
		DEC_CURVE,CRV_SMEARING  ,300,234,167,100, 0,
		DEC_CURVE,CRV_BUFFERFULL,1100,867,633,400, 0,
		-2,

		/* motorcurve 3   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,5360,3655,2855,2422,2142,1944,1795,1678,1582,1503,1434,1374, 0,
		ACC_CURVE,CRV_PARKHOME  ,5360,3362,2628,2229,1973,1791,1654,1547,1458,1384,1319,1264,1214,1170,1131,1096,1063,1034,1006,981,958,937,916,897,880,863,847,832,818,805,792,780,769,758,747,737,727,718,709,700,692,684,677,669,662,655,648,642,636,629,624,618,612,607,602,596,591,587,582,577,573,568,564,560,556,552,548,544,540,537,533,530,526,523,520,516,513,510,507,504,501,498,496,493,490,488,485,482,480,477,475,472,470,468,466,463,461,459,457,455,453,450,448,446,444,443,441,439,437,435,433,431,430,428,426,425,423,421,420,418,416,415,413,412,410,409,407,406,405,403,402,400,399,398,396,395,394,392,391,390,389,387,386,385,384,382,381,380,379,378,377,376,374,373,372,371,370,369,368,367,366,365,364,363,362,361,360,359,358,357,356,355,354,353,353,352,351,350,349,348, 0,
		ACC_CURVE,CRV_SMEARING  ,5360,3362,2628,2229,1973,1791,1654,1547, 0,
		DEC_CURVE,CRV_NORMALSCAN,1374,1434,1503,1582,1678,1795,1944,2142,2422,2855,3655,5360, 0,
		DEC_CURVE,CRV_PARKHOME  ,348,351,353,356,359,362,365,368,371,374,378,381,385,389,392,396,400,405,409,413,418,423,428,433,439,444,450,457,463,470,477,485,493,501,510,520,530,540,552,564,577,591,607,624,642,662,684,709,737,769,805,847,897,958,1034,1131,1264,1458,1791,2628,5360, 0,
		DEC_CURVE,CRV_SMEARING  ,1547,1654,1791,1973,2229,2628,3362,5360, 0,
		DEC_CURVE,CRV_BUFFERFULL,1374,1434,1503,1582,1678,1795,1944,2142,2422,2855,3655,5360, 0,
		-2,

		/* motorcurve 4   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,4705,2664,2061,1732,1521,1370,1257,1167,1094,1033,982,937,898,864,833,805,780,757,736,717,699,683,667,653,640,627,615,604,593,583,574,564,556,547,540,532,525,518,511,505,499,493,487,481,476,471,466,461,456,452,447,443,439,435,431,427,424,420,417,413,410,407,403,400,397,394,391,389,386,383,381,378,375,373,371,368,366,364,361,359,357,355,353,351,349,347,345,343,341,339,338,336,334,332,331,329,327,326,324,323,321,320,318,317,315,314,312,311,310,308,307,306,304,303,302,301,299,298,297,296,295,293,292,291,290,289,288,287,286,285,284,283,282,281,280,279,278,277,276,275,274,273,272,271,270,270,269,268,267,266,265,264,264,263,262,261,260,260,259,258,257,257,256,255,255,254,253,252,252,251,250,250,249,248,248,247,246,246,245,244,244,243,242,242,241,241,240,239,239,238,238,237,237,236,235,235,234,234,233,233,232,232,231,231,230,230,229,229,228,227,227,227,226,226,225,225,224,224,223,223,222,222,221,221,220,220,219,219,219,218,218,217,217,216,216,216,215,215,214,214,214,213,213,212,212,212,211,211,210,210,210,209,209,208,208,208,207,207,207,206,206,206,205,205,204,204,204,203,203,203,202,202,202,201,201,201,200,200,200,199,199,199,198,198,198,197,197,197,197,196,196,196,195,195,195,194,194,194,194,193,193,193,192,192,192,192,191,191,191,190,190,190,190,189,189,189,188,188,188,188,187,187,187,187,186,186,186,186,185,185,185,185,184,184,184,184,183,183,183,183,182,182,182,182,181,181,181,181,181,180,180,180,180,179,179,179,179,178,178,178,178,178,177,177,177,177,177,176,176,176,176,175,175,175,175,175,174,174,174,174,174,173,173,173,173,173,172,172,172,172,172,171,171,171, 0,
		ACC_CURVE,CRV_PARKHOME  ,4705,2888,2234,1878,1648,1485,1362,1265,1186,1120,1064,1016,973,936,903,873,845,821,798,777,758,740,723,708,693,679,666,654,643,632,621,612,602,593,585,576,569,561,554,547,540,534,528,522,516,510,505,499,494,490,485,480,476,471,467,463,459,455,451,448,444,440,437,434,430,427,424,421,418,415,412,409,407,404,401,399,396,394,391,389,387,384,382,380,378,376,374,371,369,367,366,364,362,360,358,356,354,353,351,349,348,346,344,343,341,340,338,337,335,334,332,331,329,328,327,325,324,323,321,320,319,318,316,315,314,313,312,311,309,308,307,306,305,304,303,302,301,300,299,298,297,296,295,294,293,292,291,290,289,288,287,286,285,285,284,283,282,281,280,279,279,278,277,276,275,275,274,273,272,272,271,270,269,269,268,267,267,266,265,264,264,263,262,262,261,260,260,259,259,258,257,257,256,255,255,254,254,253,252,252,251,251,250,249,249,248,248,247,247,246,246,245,245,244,243,243,242,242,241,241,240,240,239,239,238,238,237,237,237,236,236,235,235,234,234,233,233,232,232,231,231,231,230,230,229,229,228,228,228,227,227,226,226,225,225,225,224,224,223,223,223,222,222,222,221,221,220,220,220,219,219,219,218,218,217,217,217,216,216,216,215,215,215,214,214,214,213,213,213,212,212,212,211,211,211,210,210,210,209,209,209,208,208,208,207,207,207,207,206,206,206,205,205,205,204,204,204,204,203,203,203,202,202,202,202,201,201,201,200,200,200,200,199,199,199,199,198,198,198,198,197,197,197,196,196,196,196,195,195,195,195,194,194,194,194,193,193,193,193,192,192,192,192,192,191,191,191,191,190,190,190,190,189,189,189,189,189,188,188,188,188,187,187,187,187,187,186,186,186,186,186,185,185,185,185,184,184,184,184,184,183,183,183,183,183,182,182,182,182,182,181,181,181,181,181,180,180,180,180,180,180,179,179,179,179,179,178,178,178,178,178,177,177,177,177,177,177,176,176,176,176,176,176,175,175,175,175,175,174,174,174,174,174,174,173,173,173,173,173,173,172,172,172,172,172,172,171,171,171,171, 0,
		ACC_CURVE,CRV_SMEARING  ,4705,3056,2724,2497,1498,1498,1374,1276,1196,1130,1073,1025,982,944,911,880,853,828,805,784,764,746,730,714,699,685,675, 0,
		DEC_CURVE,CRV_NORMALSCAN,171,172,172,173,174,174,175,176,176,177,178,178,179,180,181,181,182,183,184,184,185,186,187,188,189,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,207,208,209,210,211,213,214,215,217,218,219,221,222,224,225,227,228,230,232,233,235,237,239,241,243,245,247,249,251,253,255,258,260,262,265,268,270,273,276,279,282,285,289,292,296,299,303,307,311,316,320,325,330,335,341,347,353,359,366,373,381,390,398,408,418,429,441,455,469,485,503,523,545,571,601,636,678,730,796,883,1005,1195,1544,4705, 0,
		DEC_CURVE,CRV_PARKHOME  ,171,172,172,173,173,174,174,175,175,176,176,177,177,178,179,179,180,180,181,182,182,183,183,184,185,185,186,187,187,188,189,189,190,191,192,192,193,194,195,195,196,197,198,199,199,200,201,202,203,204,205,206,206,207,208,209,210,211,212,213,214,215,217,218,219,220,221,222,223,225,226,227,228,230,231,232,234,235,237,238,240,241,243,244,246,247,249,251,253,254,256,258,260,262,264,266,268,271,273,275,278,280,282,285,288,290,293,296,299,302,305,309,312,316,319,323,327,331,336,340,345,350,355,360,365,371,377,384,391,398,406,414,422,432,441,452,463,476,489,504,520,538,558,580,605,633,667,706,752,810,883,979,1116,1326,1714,4705, 0,
		DEC_CURVE,CRV_SMEARING  ,675,685,699,714,730,746,764,784,805,828,853,880,911,944,982,1025,1073,1130,1196,1276,1374,1498,1498,2497,2724,3056,4705, 0,
		DEC_CURVE,CRV_BUFFERFULL,171,172,172,173,174,174,175,176,176,177,178,178,179,180,181,181,182,183,184,184,185,186,187,188,189,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,207,208,209,210,211,213,214,215,217,218,219,221,222,224,225,227,228,230,232,233,235,237,239,241,243,245,247,249,251,253,255,258,260,262,265,268,270,273,276,279,282,285,289,292,296,299,303,307,311,316,320,325,330,335,341,347,353,359,366,373,381,390,398,408,418,429,441,455,469,485,503,523,545,571,601,636,678,730,796,883,1005,1195,1544,4705, 0,
		-2,

		/* motorcurve 5   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,3763,3763,3763,3763,3763,3763,2444,2178,1997,1198,1198,1098,1020,956,903,858,819,785,754,727,703,681,662,644,626,610,596,582,571,558,547,537,527,518,509,500,492,485,478,471,464,458,452,446,440,435,430,425,420,415,410,407,402,398,394,391,386,383,380,376,373,369,366,364,360,357,355,352,349,347,344,341,338,337,334,332,329,328,325,323,321,319,317,315,313,311,310,308,306,304,302,301,299,297,295,294,293,291,290,288,286,285,284,283,281,280,278,277,275,275,274,272,271,270,268,267,266,265,264,263,262,261,259,258,257,257,256,255,254,253,251,250,249,248,248,247,246,245,244,243,243,242,241,240,239,239,238,237,236,235,235,234,233,232,231,230,230,230,229,228,228,227,226,225,225,224,223,222,222,221,221,221,220,219,219,218,217,217,216,215,215,214,213,213,212,212,212,211,211,210,209,209,208,208,207,207,206,206,205,204,204,203,203,203,203,202,202,201,201,200,200,199,199,198,198,197,197,196,196,195,195,194,194,194,194,194,193,193,192,192,191,191,190,190,190,189,189,188,188,188,187,187,186,186,185,185,185,185,185,185,184,184,183,183,183,182,182,182,181,181,180,180,180,179,179,179,178,178,178,177,177,177,176,176,176,176,176,176,175,175,175,174,174,174,173,173,173,172,172,172,171,171,171,171,170,170,170,169,169,169,169,168,168,168,167,167,167,167,167,167,167,166,166,166,166,165,165,165,165,164,164,164,163,163,163,163,162,162,162,162,161,161,161,161,160,160,160,160,159,159,159,159,159,158,158,158,158,158,158,158,158,157,157,157,157,157,156,156,156,156,155,155,155,155,155,154,154,154,154,154,153,153,153,153,152,152,152,152,152,151,151,151,151,151,150,150,150,150,150,149,149,149,149,149,149,149,149,149,149,149,148,148,148,148,148,147,147,147,147,147,147,146,146,146,146,146,145,145,145,145,145,145,144,144,144,144,144,144,143,143,143,143,143,143,142,142,142,142,142,142,141,141,141,141,141,141,140,140,140,140,140,140,140,140,140,140,140,140,140,139,139,139,139,139,139,139,138,138,138,138,138,138,138,137,137,137,137,137,137,137,136,136,136,136,136,136,136,135,135,135,135,135,135,135,134,134,134,134,134,134,134,134,133,133,133,133,133,133,133,133,132,132,132,132,132,132,132,132,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,131,130,130,130,130,130,130,130,130,129,129,129,129,129,129,129,129,129,128,128,128,128,128,128,128,128,128,127,127,127,127,127,127,127,127,127,126,126,126,126,126,126,126,126,126,125,125,125,125,125,125,125,125,125,125,124,124,124,124,124,124,124,124,124,124,123,123,123,123,123,123,123,123,123,123,122,122,122,122,122,122,122,122,122,122,122,122,122,122,122,122,122,122,122,122,121,121,121,121,121,121,121,121,121,121,121,120,120,120,120,120,120,120,120,120,120,120,119,119,119,119,119,119,119,119,119,119,119,119,118,118,118,118,118,118,118,118,118,118,118,118,117,117,117,117,117,117,117,117,117,117,117,117,116,116,116,116,116,116,116,116,116,116,116,116,115,115,115,115,115,115,115,115,115,115,115,115,115,114,114,114,114,114,114,114,114,114,114,114,114,114,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,112,112,112,112,112,112,112,112,112,112,112,112,112,112,112,111,111,111,111,111,111,111,111,111,111,111,111,111,111,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,110,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,105,105,105,105,105,105,105,105,105,105,105,105,105,105,105,105,105,105,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,104,103,103,103,103,103,103,103,103,103,103,103,103,103,103,103,103,103,103,103,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,102,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,101,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,94,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93, 0,
		ACC_CURVE,CRV_PARKHOME  ,3763,2330,1803,1515,1330,1198,1099,1021,957,904,859,819,785,755,728,704,682,662,644,626,611,597,583,571,559,548,537,527,518,509,501,493,485,478,472,464,458,453,446,441,436,430,425,420,416,411,407,402,399,394,391,387,383,380,376,374,370,366,364,361,358,355,352,349,347,344,342,339,337,335,332,330,328,326,323,321,320,318,315,313,311,310,308,306,304,302,301,300,298,296,294,293,292,290,289,287,285,284,283,282,280,279,277,276,275,274,273,271,270,269,267,266,266,265,263,262,261,260,259,258,257,256,255,254,253,252,251,250,249,248,248,247,246,245,244,243,242,241,240,239,239,239,238,237,236,235,234,233,233,232,231,230,230,230,229,228,227,227,226,225,224,224,223,222,221,221,221,220,220,219,218,218,217,216,216,215,214,214,213,213,212,212,212,211,211,210,209,209,208,208,207,207,206,205,205,204,204,203,203,203,203,202,202,201,201,200,200,199,199,198,198,197,197,196,196,195,195,194,194,194,194,194,193,193,192,192,191,191,191,190,190,189,189,188,188,188,187,187,186,186,186,185,185,185,185,185,184,184,184,183,183,182,182,182,181,181,181,180,180,180,179,179,178,178,178,177,177,177,176,176,176,176,176,176,175,175,175,174,174,174,174,173,173,173,172,172,172,171,171,171,170,170,170,170,169,169,169,168,168,168,168,167,167,167,167,167,167,167,166,166,166,166,165,165,165,165,164,164,164,163,163,163,163,162,162,162,162,161,161,161,161,160,160,160,160,160,159,159,159,159,158,158,158,158,158,158,158,158,157,157,157,157,157,156,156,156,156,155,155,155,155,155,154,154,154,154,154,153,153,153,153,153,152,152,152,152,152,151,151,151,151,151,150,150,150,150,150,149,149,149,149,149,149,149,149,149,149,148,148,148,148,148,148,147,147,147,147,147,147,146,146,146,146,146,145,145,145,145,145,145,144,144,144,144,144,144,143,143,143,143,143,143,142,142,142,142,142,142,141,141,141,141,141,141,141,140,140,140,140,140,140,140,140,140,140,140,140,140,139,139,139,139,139,139,139,138, 0,
		ACC_CURVE,CRV_SMEARING  ,3763,2330,1803,1515,1330,1198,1099,1021,957,904,859,819,785,755,728,704,682,662,644,626,611,597,583,571,559,548,540, 0,
		DEC_CURVE,CRV_NORMALSCAN,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,86,87,87,87,87,87,87,87,87,88,88,88,88,88,88,88,88,89,89,89,89,89,89,89,90,90,90,90,90,90,91,91,91,91,91,92,92,92,92,93,93,93,93,94,94,94,95,95,95,95,95,96,96,97,97,98,98,99,99,100,100,101,101,102,102,103,104,104,105,106,107,108,109,110,111,112,113,113,115,117,119,121,122,124,127,130,132,135,139,142,146,149,153,158,162,167,171,176,180,186,193,202,209,216,223,232,243,254,266,279,292,306,320,335,337,351,367,380,396,414,437,464,493,520,549,583,611,644,675,711,755,785,819,859,904,957,1021,1099,1198,1330,1515,1803,2330,3763, 0,
		DEC_CURVE,CRV_PARKHOME  ,138,142,146,149,153,158,162,167,171,176,180,186,193,202,209,216,223,232,243,254,266,279,292,306,320,335,337,351,367,380,396,414,437,464,493,520,549,583,611,644,675,711,755,785,819,859,904,957,1021,1099,1198,1330,1515,1803,2330,3763, 0,
		DEC_CURVE,CRV_SMEARING  ,138,139,139,139,139,139,139,139,140,140,140,140,140,140,140,140,140,140,140,140,140,141,141,141,141,141,141,141,142,142,142,142,142,142,143,143,143,143,143,143,144,144,144,144,144,144,145,145,145,145,145,145,146,146,146,146,146,147,147,147,147,147,147,148,148,148,148,148,148,149,149,149,149,149,149,149,149,149,149,150,150,150,150,150,151,151,151,151,151,152,152,152,152,152,153,153,153,153,153,154,154,154,154,154,155,155,155,155,155,156,156,156,156,157,157,157,157,157,158,158,158,158,158,158,158,158,159,159,159,159,160,160,160,160,160,161,161,161,161,162,162,162,162,163,163,163,163,164,164,164,165,165,165,165,166,166,166,166,167,167,167,167,167,167,167,168,168,168,168,169,169,169,170,170,170,170,171,171,171,172,172,172,173,173,173,174,174,174,174,175,175,175,176,176,176,176,176,176,177,177,177,178,178,178,179,179,180,180,180,181,181,181,182,182,182,183,183,184,184,184,185,185,185,185,185,186,186,186,187,187,188,188,188,189,189,190,190,191,191,191,192,192,193,193,194,194,194,194,194,195,195,196,196,197,197,198,198,199,199,200,200,201,201,202,202,203,203,203,203,204,204,205,205,206,207,207,208,208,209,209,210,211,211,212,212,212,213,213,214,214,215,216,216,217,218,218,219,220,220,221,221,221,222,223,224,224,225,226,227,227,228,229,230,230,230,231,232,233,233,234,235,236,237,238,239,239,239,240,241,242,243,244,245,246,247,248,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263,265,266,266,267,269,270,271,273,274,275,276,277,279,280,282,283,284,285,287,289,290,292,293,294,296,298,300,301,302,304,306,308,310,311,313,315,318,320,321,323,326,328,330,332,335,337,339,342,344,347,349,352,355,358,361,364,366,370,374,376,380,383,387,391,394,399,402,407,411,416,420,425,430,436,441,446,453,458,464,472,478,485,493,501,509,518,527,537,548,559,571,583,597,611,626,644,662,682,704,728,755,785,819,859,904,957,1021,1099,1198,1330,1515,1803,2330,3763, 0,
		DEC_CURVE,CRV_BUFFERFULL,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,93,94,94,94,95,95,95,95,95,96,96,97,97,98,98,99,99,100,100,101,101,102,102,103,104,104,105,106,107,108,109,110,111,112,113,113,115,117,119,121,122,124,127,130,132,135,139,142,146,149,153,158,162,167,171,176,180,186,193,202,209,216,223,232,243,254,266,279,292,306,320,335,337,351,367,380,396,414,437,464,493,520,549,583,611,644,675,711,755,785,819,859,904,957,1021,1099,1198,1330,1515,1803,2330,3763, 0,
		-2,

		/* motorcurve 6   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,23999, 0,
		ACC_CURVE,CRV_PARKHOME  ,5360,3362,2628,2229,1973,1791,1654,1547,1458,1384,1319,1264,1214,1170,1131,1096,1063,1034,1006,981,958,937,916,897,880,863,847,832,818,805,792,780,769,758,747,737,727,718,709,700,692, 0,
		ACC_CURVE,CRV_SMEARING  ,23999, 0,
		DEC_CURVE,CRV_NORMALSCAN,23999, 0,
		DEC_CURVE,CRV_PARKHOME  ,692,700,709,718,727,737,747,758,769,780,792,805,818,832,847,863,880,897,916,937,958,981,1006,1034,1063,1096,1131,1170,1214,1264,1319,1384,1458,1547,1654,1791,1973,2229,2628,3362,5360, 0,
		DEC_CURVE,CRV_SMEARING  ,23999, 0,
		DEC_CURVE,CRV_BUFFERFULL,23999, 0,
		-2,

		/* motorcurve 7   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,6667, 0,
		ACC_CURVE,CRV_PARKHOME  ,5360,3362,2628,2229,1973,1791,1654,1547,1458,1384,1319,1264,1214,1170,1131,1096,1063,1034,1006,981,958,937,916,897,880,863,847,832,818,805,792,780,769,758,747,737,727,718,709,700,692, 0,
		ACC_CURVE,CRV_SMEARING  ,6667, 0,
		DEC_CURVE,CRV_NORMALSCAN,6667, 0,
		DEC_CURVE,CRV_PARKHOME  ,692,700,709,718,727,737,747,758,769,780,792,805,818,832,847,863,880,897,916,937,958,981,1006,1034,1063,1096,1131,1170,1214,1264,1319,1384,1458,1547,1654,1791,1973,2229,2628,3362,5360, 0,
		DEC_CURVE,CRV_SMEARING  ,6667, 0,
		DEC_CURVE,CRV_BUFFERFULL,6667, 0,
		-2,

		/* motorcurve 8   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,1046,1046,1046,1046,1046,1046,647,501,421,370,333,305,284,266,251,239,228,218,210,202,196,190,184,179,174,170,166,162,159,155,152,149,147,144,142,139,137,135,133,131,129,127,126,124,123,121,120,118,117,116,114,113,112,111,110,109,108,107,106,105,104,103,102,101,100,100,99,98,97,96,96,95,94,94,93,92,92,91,91,90,89,89,88,88,87,87,86,86,85,85,84,84,83,83,82,82,82,81,81,80,80,79,79,79,78,78,78,77,77,76,76,76,75,75,75,74,74,74,74,73,73,73,72,72,72,71,71,71,71,70,70,70,70,69,69,69,69,68,68,68,68,67,67,67,67,66,66,66,66,65,65,65,65,64,64,64,64,63,63,63,63,62,62,62,62,61,61,61,61,61,60,60,60,60,60,60,59,59,59,59,59,59,59,58,58,58,58,58,58,57,57,57,57,57,57,57,56,56,56,56,56,56,56,56,55,55,55,55,55,55,55,55,54,54,54,54,54,54,54,54,53,53,53,53,53,53,53,53,52,52,52,52,52,52,52,52,51,51,51,51,51,51,51,51,51,51,51,51,50,50,50,50,50,50,50,50,50,50,50,49,49,49,49,49,49,49,49,49,49,49,49,48,48,48,48,48,48,48,48,48,48,48,48,47,47,47,47,47,47,47,47,47,47,47,47,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,38,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,35,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,23, 0,
		ACC_CURVE,CRV_PARKHOME  ,1045,678,604,554,332,332,304,283,265,250,238,227,217,209,201,195,189,183,178,173,169,165,161,158,154,151,148,146,143,141,138,136,134,132,130,128,126,125,123,122,120,119,117,116,115,113,112,111,110,109,108,107,106,105,104,103,102,101,100,99,99,98,97,96,95,95,94,93,93,92,91,91,90,90,89,88,88,87,87,86,86,85,85,84,84,83,83,82,82,81,81,81,80,80,79,79,78,78,78,77,77,77,76,76,75,75,75,74,74,74,73,73,73,73,72,72,72,71,71,71,70,70,70,70,69,69,69,69,68,68,68,68,67,67,67,67,66,66,66,66,65,65,65,65,65,64,64,64,64,64,63,63,63,63,63,62,62,62,62,62,61,61,61,61,61,61,60,60,60,60,60,60,59,59,59,59,59,59,58,58,58,58,58,58,58,57,57,57,57,57,57,57,56,56,56,56,56,56,56,55,55,55,55,55,55,55,55,54,54,54,54,54,54,54,54,53,53,53,53,53,53,53,53,53,52,52,52,52,52,52,52,52,52,51,51,51,51,51,51,51,51,51,51,50,50,50,50,50,50,50,50,50,50,50,49,49,49,49,49,49,49,49,49,49,49,48,48,48,48,48,48,48,48,48,48,48,48,47,47,47,47,47,47,47,47,47,47,47,47,47,46,46,46,46,46,46,46,46,46,46,46,46,46,46,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,39, 0,
		ACC_CURVE,CRV_SMEARING  ,1045,678,604,554,332,332,304,283,265,250,238,227,217,209,201,195,189,183,178,173,169,165,161,158,154,151,148,146,143,141,138,136,134,132,130,128,126,125,123,122,120,119,117,116,115,113,112,111,110,109,108,107,106,105,104,103,102,101,100,99,99,98,97,96,95,95,94,93,93,92,91,91,90,90,89,88,88,87,87,86,86,85,85,84,84,83,83,82,82,81,81,81,80,80,79,79,78,78,78,77,77,77,76,76,75,75,75,74,74,74,73,73,73,73,72,72,72,71,71,71,70,70,70,70,69,69,69,69,68,68,68,68,67,67,67,67,66,66,66,66,65,65,65,65,65,64,64,64,64,64,63,63,63,63,63,62,62,62,62,62,61,61,61,61,61,61,60,60,60,60,60,60,59,59,59,59,59,59,58,58,58,58,58,58,58,57,57,57,57,57,57,57,56,56,56,56,56,56,56,55,55,55,55,55,55,55,55,54,54,54,54,54,54,54,54,53,53,53,53,53,53,53,53,53,52,52,52,52,52,52,52,52,52,51,51,51,51,51,51,51,51,51,51,50,50,50,50,50,50,50,50,50,50,50,49,49,49,49,49,49,49,49,49,49,49,48,48,48,48,48,48,48,48,48,48,48,48,47,47,47,47,47,47,47,47,47,47,47,47,47,46,46,46,46,46,46,46,46,46,46,46,46,46,46,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,39, 0,
		DEC_CURVE,CRV_NORMALSCAN,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,30,30,30,30,31,31,31,31,32,32,32,32,33,33,34,34,35,35,36,37,38,38,39,40,41,42,43,45,46,47,48,50,51,53,54,57,59,61,63,65,68,71,75,78,82,86,90,94,94,98,103,106,111,116,122,130,138,145,153,163,171,180,188,198,211,219,228,239,252,267,284,306,334,370,422,502,648,1045, 0,
		DEC_CURVE,CRV_PARKHOME  ,39,40,41,42,43,45,46,47,48,50,51,53,54,57,59,61,63,65,68,71,75,78,82,86,90,94,94,98,103,106,111,116,122,130,138,145,153,163,171,180,188,198,211,219,228,239,252,267,284,306,334,370,422,502,648,1045, 0,
		DEC_CURVE,CRV_SMEARING  ,39,39,39,39,39,39,39,39,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,43,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,48,48,48,48,48,48,48,48,48,48,48,48,48,48,49,49,49,49,49,49,49,49,49,49,49,49,49,50,50,50,50,50,50,50,50,50,50,50,50,51,51,51,51,51,51,51,51,51,51,51,52,52,52,52,52,52,52,52,52,52,53,53,53,53,53,53,53,53,53,53,54,54,54,54,54,54,54,54,54,55,55,55,55,55,55,55,55,55,56,56,56,56,56,56,56,56,57,57,57,57,57,57,57,57,58,58,58,58,58,58,58,59,59,59,59,59,59,59,60,60,60,60,60,60,60,61,61,61,61,61,61,62,62,62,62,62,62,63,63,63,63,63,64,64,64,64,64,65,65,65,65,65,66,66,66,66,66,67,67,67,67,67,68,68,68,68,69,69,69,69,70,70,70,70,71,71,71,71,72,72,72,73,73,73,73,74,74,74,75,75,75,76,76,76,77,77,77,78,78,78,79,79,79,80,80,81,81,81,82,82,83,83,84,84,84,85,85,86,86,87,87,88,88,89,90,90,91,91,92,93,93,94,94,95,96,96,97,98,99,99,100,101,102,103,104,105,105,106,107,108,109,110,112,113,114,115,116,118,119,120,122,123,125,127,128,130,132,134,136,138,140,142,145,147,150,153,156,159,163,167,171,175,180,185,190,196,203,211,219,228,239,252,267,284,306,334,370,422,502,648,1045, 0,
		DEC_CURVE,CRV_BUFFERFULL,1045,648,502,422,370,334,306,284,267,252,239,228,219,211,198,188,180,171,163,153,145,138,130,122,116,111,106,103,98,94,94,90,86,82,78,75,71,68,65,63,61,59,57,54,53,51,50,48,47,46,45,43,42,41,40,39,38,38,37,36,35,35,34,34,33,33,32,32,32,32,31,31,31,31,30,30,30,30,29,29,29,29,29,29,29,28,28,28,28,28,28,28,28,27,27,27,27,27,27,27,27,27,27,27,27,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25, 0,
		-2,

		/* motorcurve 9   */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166, 0,
		ACC_CURVE,CRV_PARKHOME  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166,166,166, 0,
		ACC_CURVE,CRV_SMEARING  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,455, 0,
		DEC_CURVE,CRV_NORMALSCAN,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136,3136,3136,3136,3136,3136, 0,
		DEC_CURVE,CRV_PARKHOME  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136,3136,3136,3136,3136,3136, 0,
		DEC_CURVE,CRV_SMEARING  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136,3136,3136,3136,3136,3136, 0,
		DEC_CURVE,CRV_BUFFERFULL,110,110,111,111,112,112,113,113,114,114,115,115,116,116,117,117,118,118,119,119,120,120,121,121,122,122,123,123,124,124,125,125,126,126,127,127,128,128,129,129,130,131,132,133,133,134,135,136,137,138,139,140,141,143,144,145,146,147,149,150,151,153,154,156,157,159,161,162,164,166,168,170,172,174,176,179,181,184,186,189,192,195,198,202,206,209,213,218,222,227,233,239,245,252,259,267,276,282,286,291,298,305,310,319,325,331,342,354,362,376,387,404,417,431,456,475,509,551,586,654,715,850,998,1664,3136,3136, 0,
		-2,

		/* motorcurve 10  */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376, 0,
		ACC_CURVE,CRV_PARKHOME  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166, 0,
		ACC_CURVE,CRV_SMEARING  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,455, 0,
		DEC_CURVE,CRV_NORMALSCAN,381,392,404,417,431,447,465,485,508,535,566,604,650,709,787,897,1067,2227,3136, 0,
		DEC_CURVE,CRV_PARKHOME  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136, 0,
		DEC_CURVE,CRV_SMEARING  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136, 0,
		DEC_CURVE,CRV_BUFFERFULL,381,392,404,417,431,447,465,485,508,535,566,604,650,709,787,897,1067,2227,3136, 0,
		-2,

		/* motorcurve 11  */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166, 0,
		ACC_CURVE,CRV_PARKHOME  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166, 0,
		ACC_CURVE,CRV_SMEARING  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166, 0,
		DEC_CURVE,CRV_NORMALSCAN,167,168,169,170,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,190,191,192,193,194,196,197,198,200,201,203,204,206,207,209,211,212,214,216,218,219,221,223,225,227,230,232,234,236,239,241,244,247,249,252,255,258,261,265,268,272,275,279,283,288,292,297,302,307,313,318,325,331,338,346,353,362,371,381,392,404,417,431,447,465,485,508,535,566,604,650,709,787,897,1067,2227,3136, 0,
		DEC_CURVE,CRV_PARKHOME  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136, 0,
		DEC_CURVE,CRV_SMEARING  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136, 0,
		DEC_CURVE,CRV_BUFFERFULL,167,168,169,170,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,190,191,192,193,194,196,197,198,200,201,203,204,206,207,209,211,212,214,216,218,219,221,223,225,227,230,232,234,236,239,241,244,247,249,252,255,258,261,265,268,272,275,279,283,288,292,297,302,307,313,318,325,331,338,346,353,362,371,381,392,404,417,431,447,465,485,508,535,566,604,650,709,787,897,1067,2227,3136, 0,
		-2,

		/* motorcurve 12  */
		1, 1, 1, 0, /* mri, msi, skiplinecount, motorbackstep */
		ACC_CURVE,CRV_NORMALSCAN,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166,166,166,165,165,165,164,164,163,163,163,162,162,162,161,161,161,160,160,160,159,159,159,158,158,158,157,157,157,156,156,156,156,155,155,155,154,154,154,153,153,153,153,152,152,152,151,151,151,151,150,150,150,150,149,149,149,148,148,148,148,147,147,147,147,146,146,146,146,145,145,145,145,144,144,144,144,144,143,143,143,143,142,142,142,142,141,141,141,141,141,140,140,140,140,140,139,139,139,139,138,138,138,138,138,137,137,137,137,137,136,136,136,136,136,135,135,135,135,135,135,134,134,134,134,134,133,133,133,133,133,133,132,132,132,132,132,131,131,131,131,131,131,130,130,130,130,130,130,129,129,129,129,129,129,128,128,128,128,128,128,127,127,127,127,127,127,127,126,126,126,126,126,126,125, 0,
		ACC_CURVE,CRV_PARKHOME  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,447,439,431,424,417,410,404,398,392,387,381,376,371,367,362,358,354,350,346,342,338,335,331,328,325,322,319,316,313,310,308,305,302,300,298,295,293,291,288,286,284,282,280,278,276,274,272,271,269,267,265,264,262,261,259,257,256,254,253,252,250,249,247,246,245,244,242,241,240,239,237,236,235,234,233,232,231,230,228,227,226,225,224,223,222,221,221,220,219,218,217,216,215,214,213,213,212,211,210,209,209,208,207,206,206,205,204,203,203,202,201,200,200,199,198,198,197,196,196,195,195,194,193,193,192,192,191,190,190,189,189,188,187,187,186,186,185,185,184,184,183,183,182,182,181,181,180,180,179,179,178,178,177,177,176,176,175,175,175,174,174,173,173,172,172,172,171,171,170,170,170,169,169,168,168,168,167,167,166, 0,
		ACC_CURVE,CRV_SMEARING  ,3136,3136,3136,3136,3136,3136,2036,1815,1664,998,998,915,850,797,752,715,682,654,628,606,586,568,551,536,522,509,497,485,475,465,456,455, 0,
		DEC_CURVE,CRV_NORMALSCAN,110,110,110,110,110,110,110,110,111,111,111,111,111,112,112,112,112,112,113,113,113,113,114,114,114,114,115,115,115,115,115,116,116,116,116,117,117,117,117,118,118,118,118,119,119,119,120,120,120,120,121,121,121,121,122,122,122,123,123,123,124,124,124,124,125,125,125,126,126,126,127,127,127,128,128,128,129,129,129,130,130,130,131,131,132,132,132,133,133,133,134,134,135,135,135,136,136,137,137,138,138,138,139,139,140,140,141,141,142,142,142,143,143,144,144,145,145,146,146,147,148,148,149,149,150,150,151,151,152,153,153,154,154,155,156,156,157,158,158,159,160,160,161,162,163,163,164,165,166,166,167,168,169,170,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,190,191,192,193,194,196,197,198,200,201,203,204,206,207,209,211,212,214,216,218,219,221,223,225,227,230,232,234,236,239,241,244,247,249,252,255,258,261,265,268,272,275,279,283,288,292,297,302,307,313,318,325,331,338,346,353,362,371,381,392,404,417,431,447,465,485,508,535,566,604,650,709,787,897,1067,2227,3136, 0,
		DEC_CURVE,CRV_PARKHOME  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136, 0,
		DEC_CURVE,CRV_SMEARING  ,455,456,465,475,485,497,509,522,536,551,568,586,606,628,654,682,715,752,797,850,915,998,998,1664,1815,2036,3136, 0,
		DEC_CURVE,CRV_BUFFERFULL,110,110,111,111,112,112,113,113,114,114,115,115,116,116,117,117,118,118,119,119,120,120,121,121,122,122,123,123,124,124,125,125,126,126,127,127,128,128,129,129,130,131,132,133,133,134,135,136,137,138,139,140,141,143,144,145,146,147,149,150,151,153,154,156,157,159,161,162,164,166,168,170,172,174,176,179,181,184,186,189,192,195,198,202,206,209,213,218,222,227,233,239,245,252,259,267,276,282,286,291,298,305,310,319,325,331,342,354,362,376,387,404,417,431,456,475,509,551,586,654,715,850,998,1664,3136, 0,
		-1
	};

	rst = (SANE_Int *)malloc(sizeof(steps));
	if (rst != NULL)
		memcpy(rst, &steps, sizeof(steps));

	return rst;
}

static SANE_Int *cfg_motorcurve_get()
{
	/* returns motor setting buffer for a device */

	SANE_Int *rst = NULL;

	switch(RTS_Debug->dev_model)
	{
		case BQ5550:
			rst = bq5550_motor();
			break;

		case HP3800:
		case HPG2710:
			rst = hp3800_motor();
			break;

		case HP4370:
		case HPG3010:
		case HPG3110:
			rst = hp4370_motor();
			break;

		default:
			rst = hp3970_motor();
			break;
	}

	return rst;
}

/* DEPRECATED functions */

static int ua4900_calibreflective(int option, int defvalue)
{
	int rst = defvalue;

	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 0; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 300; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 310; break;
		case OFFSETODD1R: rst = 310; break;
		case OFFSETEVEN1G: rst = 313; break;
		case OFFSETODD1G: rst = 313; break;
		case OFFSETEVEN1B: rst = 319; break;
		case OFFSETODD1B: rst = 319; break;
		case ADCOFFPREDICTR: rst = 321; break;
		case ADCOFFPREDICTG: rst = 321; break;
		case ADCOFFPREDICTB: rst = 321; break;
		case ADCOFFEVEN1R_1ST: rst = 344; break;
		case ADCOFFODD1R_1ST: rst = 344; break;
		case ADCOFFEVEN1G_1ST: rst = 328; break;
		case ADCOFFODD1G_1ST: rst = 328; break;
		case ADCOFFEVEN1B_1ST: rst = 341; break;
		case ADCOFFODD1B_1ST: rst = 341; break;
		case PEAKR: rst = 122; break;
		case PEAKG: rst = 122; break;
		case PEAKB: rst = 122; break;
		case MINR: rst = 50; break;
		case MING: rst = 50; break;
		case MINB: rst = 50; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 10; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 8; break;
		case GAIN1G: rst = 8; break;
		case GAIN1B: rst = 8; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 8; break;
		case GAIN2G: rst = 8; break;
		case GAIN2B: rst = 8; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 10; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 1; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 15; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp3800_calibreflective(int option, int defvalue)
{
	int rst = defvalue;
	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 0; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 300; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 310; break;
		case OFFSETODD1R: rst = 310; break;
		case OFFSETEVEN1G: rst = 317; break;
		case OFFSETODD1G: rst = 317; break;
		case OFFSETEVEN1B: rst = 293; break;
		case OFFSETODD1B: rst = 293; break;
		case ADCOFFPREDICTR: rst = 500; break;
		case ADCOFFPREDICTG: rst = 500; break;
		case ADCOFFPREDICTB: rst = 500; break;
		case ADCOFFEVEN1R_1ST: rst = 128; break;
		case ADCOFFODD1R_1ST: rst = 128; break;
		case ADCOFFEVEN1G_1ST: rst = 128; break;
		case ADCOFFODD1G_1ST: rst = 128; break;
		case ADCOFFEVEN1B_1ST: rst = 128; break;
		case ADCOFFODD1B_1ST: rst = 128; break;
		case PEAKR: rst = 104; break;
		case PEAKG: rst = 111; break;
		case PEAKB: rst = 105; break;
		case MINR: rst = 50; break;
		case MING: rst = 56; break;
		case MINB: rst = 57; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 10; break;
		case GAINTARGETFACTOR: rst = 80; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 23; break;
		case GAIN1G: rst = 19; break;
		case GAIN1B: rst = 0; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -3; break;
		case BSHADINGHEIGHT: rst = 20; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp3970_calibreflective(int option, int defvalue)
{
	int rst = defvalue;
	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 0; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 300; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 327; break;
		case OFFSETODD1R: rst = 327; break;
		case OFFSETEVEN1G: rst = 315; break;
		case OFFSETODD1G: rst = 315; break;
		case OFFSETEVEN1B: rst = 322; break;
		case OFFSETODD1B: rst = 322; break;
		case ADCOFFPREDICTR: rst = 322; break;
		case ADCOFFPREDICTG: rst = 310; break;
		case ADCOFFPREDICTB: rst = 322; break;
		case ADCOFFEVEN1R_1ST: rst = 344; break;
		case ADCOFFODD1R_1ST: rst = 344; break;
		case ADCOFFEVEN1G_1ST: rst = 328; break;
		case ADCOFFODD1G_1ST: rst = 328; break;
		case ADCOFFEVEN1B_1ST: rst = 341; break;
		case ADCOFFODD1B_1ST: rst = 341; break;
		case PEAKR: rst = 82; break;
		case PEAKG: rst = 117; break;
		case PEAKB: rst = 116; break;
		case MINR: rst = 37; break;
		case MING: rst = 51; break;
		case MINB: rst = 53; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 10; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 28; break;
		case GAIN1G: rst = 22; break;
		case GAIN1B: rst = 21; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 10; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp4370_calibreflective(int option, int defvalue)
{
	int rst = defvalue;
	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 0; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 300; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 305; break;
		case OFFSETODD1R: rst = 3305; break;
		case OFFSETEVEN1G: rst = 313; break;
		case OFFSETODD1G: rst = 313; break;
		case OFFSETEVEN1B: rst = 317; break;
		case OFFSETODD1B: rst = 317; break;
		case ADCOFFPREDICTR: rst = 500; break;
		case ADCOFFPREDICTG: rst = 500; break;
		case ADCOFFPREDICTB: rst = 500; break;
		case ADCOFFEVEN1R_1ST: rst = 344; break;
		case ADCOFFODD1R_1ST: rst = 344; break;
		case ADCOFFEVEN1G_1ST: rst = 328; break;
		case ADCOFFODD1G_1ST: rst = 328; break;
		case ADCOFFEVEN1B_1ST: rst = 341; break;
		case ADCOFFODD1B_1ST: rst = 341; break;
		case PEAKR: rst = 159; break;
		case PEAKG: rst = 191; break;
		case PEAKB: rst = 191; break;
		case MINR: rst = 146; break;
		case MING: rst = 180; break;
		case MINB: rst = 179; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 10; break;
		case GAINTARGETFACTOR: rst = 80; break;
		case CALIBPAGON: rst = 0; break;
		case HIPAGR: rst = 3; break;
		case HIPAGG: rst = 0; break;
		case HIPAGB: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case LOPAGR: rst = 3; break;
		case LOPAGG: rst = 3; break;
		case LOPAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 10; break;
		case GAIN1G: rst = 2; break;
		case GAIN1B: rst = 1; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 10; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int fc_calibreflective(int option, int defvalue)
{
	int rst;

	switch(RTS_Debug->dev_model)
	{
		case UA4900: rst = ua4900_calibreflective(option, defvalue); break;
		case HPG2710:
		case HP3800: rst = hp3800_calibreflective(option, defvalue); break;
		case HPG3010:
		case HPG3110:
		case HP4370: rst = hp4370_calibreflective(option, defvalue); break;
		default    : rst = hp3970_calibreflective(option, defvalue); break;
	}

	return rst;
}

static int ua4900_calibtransparent(int option, int defvalue)
{
	int rst = defvalue;

	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 12100; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 2; break;
		case BREFG: rst = 2; break;
		case BREFB: rst = 2; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 13; break;
		case OFFSETAVGTARGETG: rst = 13; break;
		case OFFSETAVGTARGETB: rst = 13; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 321; break;
		case OFFSETODD1R: rst = 321; break;
		case OFFSETEVEN1G: rst = 321; break;
		case OFFSETODD1G: rst = 321; break;
		case OFFSETEVEN1B: rst = 321; break;
		case OFFSETODD1B: rst = 321; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 177; break;
		case PEAKG: rst = 177; break;
		case PEAKB: rst = 177; break;
		case MINR: rst = 136; break;
		case MING: rst = 136; break;
		case MINB: rst = 136; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 0; break;
		case PAGG: rst = 0; break;
		case PAGB: rst = 0; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 24; break;
		case GAIN1G: rst = 21; break;
		case GAIN1B: rst = 19; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 8; break;
		case GAIN2G: rst = 8; break;
		case GAIN2B: rst = 8; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = 2; break;
		case BSHADINGHEIGHT: rst = 10; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp3800_calibtransparent(int option, int defvalue)
{
	int rst = defvalue;
	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 4155; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 2; break;
		case BREFG: rst = 2; break;
		case BREFB: rst = 2; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 310; break;
		case OFFSETODD1R: rst = 310; break;
		case OFFSETEVEN1G: rst = 299; break;
		case OFFSETODD1G: rst = 299; break;
		case OFFSETEVEN1B: rst = 309; break;
		case OFFSETODD1B: rst = 309; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 67; break;
		case PEAKG: rst = 61; break;
		case PEAKB: rst = 57; break;
		case MINR: rst = 10; break;
		case MING: rst = 10; break;
		case MINB: rst = 10; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 2; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 9; break;
		case GAIN1G: rst = 12; break;
		case GAIN1B: rst = 10; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -3; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp3970_calibtransparent(int option, int defvalue)
{
	int rst = defvalue;
	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 7500; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 2; break;
		case BREFG: rst = 2; break;
		case BREFB: rst = 2; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 323; break;
		case OFFSETODD1R: rst = 323; break;
		case OFFSETEVEN1G: rst = 327; break;
		case OFFSETODD1G: rst = 327; break;
		case OFFSETEVEN1B: rst = 327; break;
		case OFFSETODD1B: rst = 327; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 42; break;
		case PEAKG: rst = 55; break;
		case PEAKB: rst = 53; break;
		case MINR: rst = 31; break;
		case MING: rst = 39; break;
		case MINB: rst = 38; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 2; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 21; break;
		case GAIN1G: rst = 14; break;
		case GAIN1B: rst = 12; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = 1; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp4370_calibtransparent(int option, int defvalue)
{
	int rst = defvalue;
	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 6580; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 2; break;
		case BREFG: rst = 2; break;
		case BREFB: rst = 2; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 315; break;
		case OFFSETODD1R: rst = 321; break;
		case OFFSETEVEN1G: rst = 321; break;
		case OFFSETODD1G: rst = 324; break;
		case OFFSETEVEN1B: rst = 324; break;
		case OFFSETODD1B: rst = 327; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 62; break;
		case PEAKG: rst = 66; break;
		case PEAKB: rst = 54; break;
		case MINR: rst = 42; break;
		case MING: rst = 45; break;
		case MINB: rst = 45; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 80; break;
		case CALIBPAGON: rst = 0; break;
		case HIPAGR: rst = 3; break;
		case HIPAGG: rst = 3; break;
		case HIPAGB: rst = 2; break;
		case LOPAGR: rst = 3; break;
		case LOPAGG: rst = 3; break;
		case LOPAGB: rst = 2; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 2; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 11; break;
		case GAIN1G: rst = 9; break;
		case GAIN1B: rst = 12; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hpg3110_calibtransparent(int option, int defvalue)
{
	int rst = defvalue;
	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 5100; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 2; break;
		case BREFG: rst = 2; break;
		case BREFB: rst = 2; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 315; break;
		case OFFSETODD1R: rst = 321; break;
		case OFFSETEVEN1G: rst = 321; break;
		case OFFSETODD1G: rst = 324; break;
		case OFFSETEVEN1B: rst = 324; break;
		case OFFSETODD1B: rst = 327; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 62; break;
		case PEAKG: rst = 66; break;
		case PEAKB: rst = 54; break;
		case MINR: rst = 42; break;
		case MING: rst = 45; break;
		case MINB: rst = 45; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 80; break;
		case CALIBPAGON: rst = 0; break;
		case HIPAGR: rst = 3; break;
		case HIPAGG: rst = 3; break;
		case HIPAGB: rst = 2; break;
		case LOPAGR: rst = 3; break;
		case LOPAGG: rst = 3; break;
		case LOPAGB: rst = 2; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 2; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 11; break;
		case GAIN1G: rst = 9; break;
		case GAIN1B: rst = 12; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int fc_calibtransparent(int option, int defvalue)
{
	int rst;

	switch(RTS_Debug->dev_model)
	{
		case UA4900: rst = ua4900_calibtransparent(option, defvalue); break;
		case HPG2710:
		case HP3800: rst = hp3800_calibtransparent(option, defvalue); break;
		case HPG3010:
		case HP4370: rst = hp4370_calibtransparent(option, defvalue); break;
		case HPG3110: rst = hpg3110_calibtransparent(option, defvalue); break;
		default    : rst = hp3970_calibtransparent(option, defvalue); break;
	}

	return rst;
}

static int ua4900_calibnegative(int option, int defvalue)
{
	int rst = defvalue;

	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 12100; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 2; break;
		case BREFG: rst = 2; break;
		case BREFB: rst = 2; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 5; break;
		case OFFSETAVGTARGETG: rst = 5; break;
		case OFFSETAVGTARGETB: rst = 5; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 283; break;
		case OFFSETODD1R: rst = 283; break;
		case OFFSETEVEN1G: rst = 279; break;
		case OFFSETODD1G: rst = 279; break;
		case OFFSETEVEN1B: rst = 295; break;
		case OFFSETODD1B: rst = 295; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 113; break;
		case PEAKG: rst = 145; break;
		case PEAKB: rst = 126; break;
		case MINR: rst = 80; break;
		case MING: rst = 105; break;
		case MINB: rst = 96; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 10; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 0; break;
		case PAGG: rst = 0; break;
		case PAGB: rst = 0; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 8; break;
		case GAIN1G: rst = 8; break;
		case GAIN1B: rst = 8; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 8; break;
		case GAIN2G: rst = 8; break;
		case GAIN2B: rst = 8; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = 2; break;
		case BSHADINGHEIGHT: rst = 10; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp3800_calibnegative(int option, int defvalue)
{
	int rst = defvalue;

	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 4155; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 315; break;
		case OFFSETODD1R: rst = 315; break;
		case OFFSETEVEN1G: rst = 307; break;
		case OFFSETODD1G: rst = 304; break;
		case OFFSETEVEN1B: rst = 309; break;
		case OFFSETODD1B: rst = 309; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 37; break;
		case PEAKG: rst = 234; break;
		case PEAKB: rst = 202; break;
		case MINR: rst = 32; break;
		case MING: rst = 195; break;
		case MINB: rst = 166; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 30; break;
		case GAIN1G: rst = 3; break;
		case GAIN1B: rst = 0; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp3970_calibnegative(int option, int defvalue)
{
	int rst = defvalue;

	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 7500; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 294; break;
		case OFFSETODD1R: rst = 294; break;
		case OFFSETEVEN1G: rst = 276; break;
		case OFFSETODD1G: rst = 276; break;
		case OFFSETEVEN1B: rst = 266; break;
		case OFFSETODD1B: rst = 266; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 33; break;
		case PEAKG: rst = 75; break;
		case PEAKB: rst = 105; break;
		case MINR: rst = 19; break;
		case MING: rst = 34; break;
		case MINB: rst = 42; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 90; break;
		case CALIBPAGON: rst = 0; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 23; break;
		case GAIN1G: rst = 18; break;
		case GAIN1B: rst = 23; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = 1; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hpg3110_calibnegative(int option, int defvalue)
{
	int rst = defvalue;

	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 5100; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 308; break;
		case OFFSETODD1R: rst = 308; break;
		case OFFSETEVEN1G: rst = 317; break;
		case OFFSETODD1G: rst = 317; break;
		case OFFSETEVEN1B: rst = 319; break;
		case OFFSETODD1B: rst = 319; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 116; break;
		case PEAKG: rst = 126; break;
		case PEAKB: rst = 102; break;
		case MINR: rst = 103; break;
		case MING: rst = 112; break;
		case MINB: rst = 80; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 80; break;
		case CALIBPAGON: rst = 0; break;
		case HIPAGR: rst = 3; break;
		case HIPAGG: rst = 3; break;
		case HIPAGB: rst = 3; break;
		case LOPAGR: rst = 3; break;
		case LOPAGG: rst = 3; break;
		case LOPAGB: rst = 3; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 6; break;
		case GAIN1G: rst = 1; break;
		case GAIN1B: rst = 7; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int hp4370_calibnegative(int option, int defvalue)
{
	int rst = defvalue;

	switch(option)
	{
		case WSTRIPXPOS: rst = 0; break;
		case WSTRIPYPOS: rst = 6580; break;
		case BSTRIPXPOS: rst = 0; break;
		case BSTRIPYPOS: rst = 0; break;
		case BREFR: rst = 10; break;
		case BREFG: rst = 10; break;
		case BREFB: rst = 10; break;
		case REFBITDEPTH: rst = 8; break;
		case OFFSETHEIGHT: rst = 10; break;
		case OFFSETNSIGMA: rst = 2; break;
		case OFFSETTARGETMAX: rst = 50; break;
		case OFFSETTARGETMIN: rst = 2; break;
		case OFFSETAVGTARGETR: rst = 10; break;
		case OFFSETAVGTARGETG: rst = 10; break;
		case OFFSETAVGTARGETB: rst = 10; break;
		case ADCOFFEVENODD: rst = 1; break;
		case CALIBOFFSET1ON: rst = 2; break;
		case ADCOFFQUICKWAY: rst = 1; break;
		case ADCOFFPREDICTSTART: rst = 200; break;
		case ADCOFFPREDICTEND: rst = 500; break;
		case OFFSETTUNESTEP1: rst = 5; break;
		case OFFSETBOUNDARYRATIO1: rst = 100; break;
		case OFFSETAVGRATIO1: rst = 100; break;
		case OFFSETEVEN1R: rst = 308; break;
		case OFFSETODD1R: rst = 308; break;
		case OFFSETEVEN1G: rst = 317; break;
		case OFFSETODD1G: rst = 317; break;
		case OFFSETEVEN1B: rst = 319; break;
		case OFFSETODD1B: rst = 319; break;
		case ADCOFFPREDICTR: rst = 333; break;
		case ADCOFFPREDICTG: rst = 313; break;
		case ADCOFFPREDICTB: rst = 317; break;
		case ADCOFFEVEN1R_1ST: rst = 69; break;
		case ADCOFFODD1R_1ST: rst = 69; break;
		case ADCOFFEVEN1G_1ST: rst = 87; break;
		case ADCOFFODD1G_1ST: rst = 87; break;
		case ADCOFFEVEN1B_1ST: rst = 106; break;
		case ADCOFFODD1B_1ST: rst = 106; break;
		case PEAKR: rst = 116; break;
		case PEAKG: rst = 126; break;
		case PEAKB: rst = 102; break;
		case MINR: rst = 103; break;
		case MING: rst = 112; break;
		case MINB: rst = 80; break;
		case CALIBOFFSET2ON: rst = 0; break;
		case OFFSETTUNESTEP2: rst = 1; break;
		case OFFSETBOUNDARYRATIO2: rst = 100; break;
		case OFFSETAVGRATIO2: rst = 100; break;
		case OFFSETEVEN2R: rst = 0; break;
		case OFFSETODD2R: rst = 0; break;
		case OFFSETEVEN2G: rst = 0; break;
		case OFFSETODD2G: rst = 0; break;
		case OFFSETEVEN2B: rst = 0; break;
		case OFFSETODD2B: rst = 0; break;
		case GAINHEIGHT: rst = 30; break;
		case GAINTARGETFACTOR: rst = 80; break;
		case CALIBPAGON: rst = 0; break;
		case HIPAGR: rst = 3; break;
		case HIPAGG: rst = 3; break;
		case HIPAGB: rst = 3; break;
		case LOPAGR: rst = 3; break;
		case LOPAGG: rst = 3; break;
		case LOPAGB: rst = 3; break;
		case PAGR: rst = 3; break;
		case PAGG: rst = 3; break;
		case PAGB: rst = 3; break;
		case CALIBGAIN1ON: rst = 1; break;
		case GAIN1R: rst = 6; break;
		case GAIN1G: rst = 1; break;
		case GAIN1B: rst = 7; break;
		case CALIBGAIN2ON: rst = 0; break;
		case GAIN2R: rst = 4; break;
		case GAIN2G: rst = 4; break;
		case GAIN2B: rst = 4; break;
		case TOTSHADING: rst = 0; break;
		case BSHADINGON: rst = -2; break;
		case BSHADINGHEIGHT: rst = 30; break;
		case BSHADINGPREDIFFR: rst = 2; break;
		case BSHADINGPREDIFFG: rst = 2; break;
		case BSHADINGPREDIFFB: rst = 2; break;
		case BSHADINGDEFCUTOFF: rst = 0; break;
		case WSHADINGON: rst = 3; break;
		case WSHADINGHEIGHT: rst = 24; break;
		case WSHADINGPREDIFFR: rst = -1; break;
		case WSHADINGPREDIFFG: rst = -1; break;
		case WSHADINGPREDIFFB: rst = -1; break;
	}

	return rst;
}

static int fc_calibnegative(int option, int defvalue)
{
	int rst;

	switch(RTS_Debug->dev_model)
	{
		case UA4900: rst = ua4900_calibnegative(option, defvalue); break;
		case HPG2710:
		case HP3800: rst = hp3800_calibnegative(option, defvalue); break;
		case HPG3010:
		case HP4370: rst = hp4370_calibnegative(option, defvalue); break;
		case HPG3110: rst = hpg3110_calibnegative(option, defvalue); break;
		default    : rst = hp3970_calibnegative(option, defvalue); break;
	}

	return rst;
}

static int fc_scaninfo_get(int option, int defvalue)
{
	int value[] = {1, 0, 0, 0, 0, 100};
	int ua4900_value[] = {1, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 100};

	int rst = defvalue;
	int *myvalue = NULL;

	switch(RTS_Debug->dev_model)
	{
		case UA4900: myvalue = ua4900_value; break;
		default: myvalue = value; break;
	}

	switch(option)
	{
		case PARKHOMEAFTERCALIB: rst = myvalue[0]; break;
		case SHADINGTIME_16BIT: rst = myvalue[1]; break;
		case SHADOWTIME_16BIT: rst = myvalue[2]; break;
		case SHADINGTIME_8BIT: rst = myvalue[3]; break;
		case SHADOWTIME_8BIT: rst = myvalue[4]; break;
		case PREVIEWDPI: rst = myvalue[5]; break;
	}

	return rst;
}

/* fitcalibrate */
static int fitcalibrate_get(int section, int option, int defvalue)
{
	int rst = defvalue;

	switch(section)
	{
		case CALIBREFLECTIVE:
			rst = fc_calibreflective(option, defvalue); break;
		case CALIBTRANSPARENT:
			rst = fc_calibtransparent(option, defvalue); break;
		case CALIBNEGATIVEFILM:
			rst = fc_calibnegative(option, defvalue); break;
		case SCANINFO:
			rst = fc_scaninfo_get(option, defvalue); break;
	}
	
	return rst;
}

static int srt_hp3800_scanparam_get(int option, int defvalue)
{
	int rst = defvalue;

	/* t_rtinifile */
	int value3[] = {1, 0, 0, 0, 1, 12, 0, 1, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 0};

	int *value = value3;

	if (value != NULL)
		switch(option)
		{
			case ARRANGELINE: rst = value[0]; break;
			case COMPRESSION: rst = value[1]; break;
			case TA_X_START:  rst = value[2]; break;
			case TA_Y_START:  rst = value[3]; break;
			case DPIGAINCONTROL600: rst = value[4]; break;
			case CRVS:  rst = value[5]; break;
			case MLOCK: rst = value[6]; break;
			case ENABLEWARMUP:  rst = value[7]; break;
			case NMAXTARGET:    rst = value[8]; break;
			case NMINTARGET:    rst = value[9]; break;
			case NMAXTARGETTA:  rst = value[10]; break;
			case NMINTARGETTA:  rst = value[11]; break;
			case NMAXTARGETNEG: rst = value[12]; break;
			case NMINTARGETNEG: rst = value[13]; break;
			case STABLEDIFF: rst = value[14]; break;
			case DELTAPWM:   rst = value[15]; break;
			case PWMLAMPLEVEL:   rst = value[16]; break;
			case TMAPWMDUTY: rst = value[17]; break;
			case LEFTLEADING: rst = value[18]; break;
	}

	return rst;
}

static int srt_hp3970_scanparam_get(int file, int option, int defvalue)
{
	int rst = defvalue;
	/* s_rtinifile */
	int value1[] = {1, 0, 150, 0, 1, 6, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 360};
	/* s_usb1inifile */
	int value2[] = {1, 0, 150, 0, 1, 6, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 360};
	/* t_rtinifile */
	int value3[] = {1, 0, 150, 0, 1, 12, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 0};
	/* t_usb1inifile */
	int value4[] = {1, 0, 150, 0, 1, 12, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 0};

	int *value = NULL;

	switch(file)
	{
		case S_RTINIFILE: value = value1; break;
		case S_USB1INIFILE: value = value2; break;
		case T_RTINIFILE: value = value3; break;
		case T_USB1INIFILE: value = value4; break;
	}

	if (value != NULL)
		switch(option)
		{
			case ARRANGELINE: rst = value[0]; break;
			case COMPRESSION: rst = value[1]; break;
			case TA_X_START:  rst = value[2]; break;
			case TA_Y_START:  rst = value[3]; break;
			case DPIGAINCONTROL600: rst = value[4]; break;
			case CRVS:  rst = value[5]; break;
			case MLOCK: rst = value[6]; break;
			case ENABLEWARMUP:  rst = value[7]; break;
			case NMAXTARGET:    rst = value[8]; break;
			case NMINTARGET:    rst = value[9]; break;
			case NMAXTARGETTA:  rst = value[10]; break;
			case NMINTARGETTA:  rst = value[11]; break;
			case NMAXTARGETNEG: rst = value[12]; break;
			case NMINTARGETNEG: rst = value[13]; break;
			case STABLEDIFF: rst = value[14]; break;
			case DELTAPWM:   rst = value[15]; break;
			case PWMLAMPLEVEL:   rst = value[16]; break;
			case TMAPWMDUTY: rst = value[17]; break;
			case LEFTLEADING: rst = value[18]; break;
	}

	return rst;
}

static int srt_hp4370_scanparam_get(int file, int option, int defvalue)
{
	/* s_rtinifile */
	int value1[] = {1, 0, 150, 0, 1, 6, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 360};
	/* s_usb1inifile */
	int value2[] = {1, 0, 150, 0, 1, 6, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 360};
	/* t_rtinifile */
	int value3[] = {1, 0, 150, 0, 1, 12, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 0};
	/* t_usb1inifile */
	int value4[] = {1, 0, 150, 0, 1, 12, 0, 0, 170, 140, 40, 30, 40, 30, 1500, 20, 0, 36, 0};
	int *value = NULL;

	int rst = defvalue;

	switch(file)
	{
		case S_RTINIFILE: value = value1; break;
		case S_USB1INIFILE: value = value2; break;
		case T_RTINIFILE: value = value3; break;
		case T_USB1INIFILE: value = value4; break;
	}

	if (value != NULL)
		switch(option)
		{
			case ARRANGELINE: rst = value[0]; break;
			case COMPRESSION: rst = value[1]; break;
			case TA_X_START:  rst = value[2]; break;
			case TA_Y_START:  rst = value[3]; break;
			case DPIGAINCONTROL600: rst = value[4]; break;
			case CRVS:  rst = value[5]; break;
			case MLOCK: rst = value[6]; break;
			case ENABLEWARMUP: rst = value[7]; break;
			case NMAXTARGET:   rst = value[8]; break;
			case NMINTARGET:   rst = value[9]; break;
			case NMAXTARGETTA: rst = value[10]; break;
			case NMINTARGETTA: rst = value[11]; break;
			case NMAXTARGETNEG: rst = value[12]; break;
			case NMINTARGETNEG: rst = value[13]; break;
			case STABLEDIFF: rst = value[14]; break;
			case DELTAPWM: rst = value[15]; break;
			case PWMLAMPLEVEL: rst = value[16]; break;
			case TMAPWMDUTY: rst = value[17]; break;
			case LEFTLEADING: rst = value[18]; break;
	}

	return rst;
}

static int srt_scancali_get(int file, int option, int defvalue)
{
	int rst = defvalue;
	/* s_rtinifile */
	int value1[] = {3, 3, 3, 14, 4, 4, 41, 41, 42, 41, 41, 42, 91, 91,
	                53, 53, 48, 48, 104, 104, 59, 59, 64,64};
	/* s_usb1inifile */
	int value2[] = {3, 3, 3, 14, 4, 4, 41, 41, 42, 41, 41, 42, 91, 91,
	                53, 53, 48, 48, 104, 104, 59, 59, 64, 64};
	/* t_rtinifile*/
	int value3[] = {3, 3, 3, 14, 4, 4, 41, 41, 42, 41, 41, 42, 270, 270,
	                511, 511, 511, 511, 270, 270, 511, 511, 511, 511};
	/* t_usb1inifile*/
	int value4[] = {3, 3, 3, 14, 4, 4, 41, 41, 42, 41, 41, 42, 270, 270,
	                511, 511, 511, 511, 270, 270, 511, 511, 511, 511};
	int *value = NULL;

	switch(file)
	{
		case S_RTINIFILE: value = value1; break;
		case S_USB1INIFILE: value = value2; break;
		case T_RTINIFILE: value = value3; break;
		case T_USB1INIFILE: value = value4; break;
	}

	if (value != NULL)
		switch(option)
		{
			case PGA1: rst = value[0]; break;
			case PGA2: rst = value[1]; break;
			case PGA3: rst = value[2]; break;
			case VGAGAIN11: rst = value[3]; break;
			case VGAGAIN12: rst = value[4]; break;
			case VGAGAIN13: rst = value[5]; break;
			case DCSTEPEVEN1: rst = value[6]; break;
			case DCSTEPODD1: rst = value[7]; break;
			case DCSTEPEVEN2: rst = value[8]; break;
			case DCSTEPODD2: rst = value[9]; break;
			case DCSTEPEVEN3: rst = value[10]; break;
			case DCSTEPODD3: rst = value[11]; break;
			case FIRSTDCOFFSETEVEN11: rst = value[12]; break;
			case FIRSTDCOFFSETODD11: rst = value[13]; break;
			case FIRSTDCOFFSETEVEN12: rst = value[14]; break;
			case FIRSTDCOFFSETODD12: rst = value[15]; break;
			case FIRSTDCOFFSETEVEN13: rst = value[16]; break;
			case FIRSTDCOFFSETODD13: rst = value[17]; break;
			case DCOFFSETEVEN11: rst = value[18]; break;
			case DCOFFSETODD11: rst = value[19]; break;
			case DCOFFSETEVEN12: rst = value[20]; break;
			case DCOFFSETODD12: rst = value[21]; break;
			case DCOFFSETEVEN13: rst = value[22]; break;
			case DCOFFSETODD13: rst = value[23]; break;
		}

	return rst;
}

static int srt_truegrayparam_get(int file, int option, int defvalue)
{
	int rst = defvalue;
	/* s_rtinifile */
	int value1[] = {100, 30, 59, 11};
	/* s_usb1inifile */
	int value2[] = {100, 30, 59, 11};
	/* t_rtinifile */
	int value3[] = {100, 30, 59, 11};
	/* t_usb1inifile */
	int value4[] = {100, 30, 59, 11};
	int *value = NULL;

	switch(file)
	{
		case S_RTINIFILE: value = value1; break;
		case S_USB1INIFILE: value = value2; break;
		case T_RTINIFILE: value = value3; break;
		case T_USB1INIFILE: value = value4; break;
	}

	if (value != NULL)
		switch(option)
		{
			case SHADINGBASE: rst = value[0]; break;
			case SHADINGFACT1: rst = value[1]; break;
			case SHADINGFACT2: rst = value[2]; break;
			case SHADINGFACT3: rst = value[3]; break;
		}

	return rst;
}

static int srt_caliparam_get(int file, int option, int defvalue)
{
	int rst = defvalue;
	/* s_rtinifile */
	int value1[] = {0xffff};
	/* s_usb1inifile */
	int value2[] = {0xffff};
	/* t_rtinifile */
	int value3[] = {0xffff};
	/* t_usb1inifile */
	int value4[] = {0xffff};
	int *value = NULL;

	switch(file)
	{
		case S_RTINIFILE: value = value1; break;
		case S_USB1INIFILE: value = value2; break;
		case T_RTINIFILE: value = value3; break;
		case T_USB1INIFILE: value = value4; break;
	}

	if (value != NULL)
		switch(option)
		{
			case PIXELDARKLEVEL: rst = value[0]; break;
		}

	return rst;
}

static int srt_hp3800_platform_get(int option, int defvalue)
{
	/* s_rtinifile*/
	int value1[] = {100, 99, 1214636};

	int *value = value1;
	int rst = defvalue;

	if (value != NULL)
	{
		switch(option)
		{
			case BINARYTHRESHOLDH: rst = value[0]; break;
			case BINARYTHRESHOLDL: rst = value[1]; break;
			case CLOSETIME: rst = value[2]; break;
		}
	}

	return rst;
}

static int srt_hp3970_platform_get(int option, int defvalue)
{
	/* s_rtinifile*/
	int value1[] = {128, 127, 1214636};

	int *value = value1;
	int rst = defvalue;

	if (value != NULL)
	{
		switch(option)
		{
			case BINARYTHRESHOLDH: rst = value[0]; break;
			case BINARYTHRESHOLDL: rst = value[1]; break;
			case CLOSETIME: rst = value[2]; break;
		}
	}

	return rst;
}


static int srt_ua4900_platform_get(int option, int defvalue)
{
	int value1[] = {128, 127, 1214636};
	int *value = value1;
	int rst = defvalue;

	if (value != NULL)
	{
		switch(option)
		{
			case BINARYTHRESHOLDH: rst = value[0]; break;
			case BINARYTHRESHOLDL: rst = value[1]; break;
			case CLOSETIME: rst = value[2]; break;
		}
	}

	return rst;
}

static int srt_hp4370_platform_get(int option, int defvalue)
{
	/* t_rtinifile */
	int value3[] = {128, 127, 1214636};

	int *value = value3;
	int rst = defvalue;

	if (value != NULL)
	{
		switch(option)
		{
			case BINARYTHRESHOLDH: rst = value[0]; break;
			case BINARYTHRESHOLDL: rst = value[1]; break;
			case CLOSETIME: rst = value[2]; break;
		}
	}

	return rst;
}

static int srt_scaninfo_get(int file, int option, int defvalue)
{
	int rst = defvalue;
	int value1[] = {0, 0, 0, 0};
	int value2[] = {0, 0, 0, 0};
	int value3[] = {0, 0, 0, 0};
	int value4[] = {0, 0, 0, 0};
	int *value = NULL;

	switch(file)
	{
		case S_RTINIFILE: value = value1; break;
		case S_USB1INIFILE: value = value2; break;
		case T_RTINIFILE: value = value3; break;
		case T_USB1INIFILE: value = value4; break;
	}

	if (value != NULL)
	{
		switch(option)
		{
			case SHADINGTIME_16BIT: rst = value[0]; break;
			case SHADOWTIME_16BIT: rst = value[1]; break;
			case SHADINGTIME_8BIT: rst = value[2]; break;
			case SHADOWTIME_8BIT: rst = value[3]; break;
		}
	}

	return rst;
}

static int srt_sec_get(int file, int section, int option, int defvalue)
{
	int rst = defvalue;

	switch(section)
	{
		case SCAN_PARAM:
			switch(RTS_Debug->dev_model)
			{
				case HPG2710:
				case HP3800: rst = srt_hp3800_scanparam_get(option, defvalue); break;
				case HPG3010:
				case HPG3110:
				case HP4370: rst = srt_hp4370_scanparam_get(file, option, defvalue); break;
				default    : rst = srt_hp3970_scanparam_get(file, option, defvalue); break;
			}
			break;
		case SCAN_CALI:
			rst = srt_scancali_get(file, option, defvalue); break;
		case TRUE_GRAY_PARAM:
			rst = srt_truegrayparam_get(file, option, defvalue); break;
		case CALI_PARAM:
			rst = srt_caliparam_get(file, option, defvalue); break;
		case PLATFORM:
			switch(RTS_Debug->dev_model)
			{
				case HPG2710:
				case HP3800: rst = srt_hp3800_platform_get(option, defvalue); break;
				case UA4900: rst = srt_ua4900_platform_get(option, defvalue); break;
				case HPG3010:
				case HPG3110:
				case HP4370: rst = srt_hp4370_platform_get(option, defvalue); break;
				default    : rst = srt_hp3970_platform_get(option, defvalue); break;
			}
			break;
		case SCANINFO:
			rst = srt_scaninfo_get(file, option, defvalue); break;
	}

	return rst;
}

static int get_value(int section, int option, int defvalue, int file)
{
	int rst = defvalue;

	switch(file)
	{
		case FITCALIBRATE:
			rst = fitcalibrate_get(section, option, defvalue); break;
		case S_RTINIFILE:
		case S_USB1INIFILE:
		case T_RTINIFILE:
		case T_USB1INIFILE:
			rst = srt_sec_get(file, section, option, defvalue); break;
	}

	return rst;
}
