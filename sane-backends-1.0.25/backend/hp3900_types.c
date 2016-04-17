/* HP Scanjet 3900 series - Structures and global variables

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

/* devices */
#define DEVSCOUNT           0x09	/* Number of scanners supported by this backend */

#define HP3970              0x00	/* rts8822l-01H  HP Scanjet 3970  */
#define HP4070              0x01	/* rts8822l-01H  HP Scanjet 4070  */
#define HP4370              0x02	/* rts8822l-02A  HP Scanjet 4370  */
#define UA4900              0x03	/* rts8822l-01H  UMAX Astra 4900  */
#define HP3800              0x04	/* rts8822bl-03A HP Scanjet 3800  */
#define HPG3010             0x05	/* rts8822l-02A  HP Scanjet G3010 */
#define BQ5550              0x06	/* rts8823l-01E  BenQ 5550        */
#define HPG2710             0x07	/* rts8822bl-03A HP Scanjet G2710 */
#define HPG3110             0x08	/* rts8822l-02A  HP Scanjet G3110 */

/* chipset models */
#define RTS8822L_01H        0x00
#define RTS8822L_02A        0x01
#define RTS8822BL_03A       0x02
#define RTS8823L_01E        0x03

/* chipset capabilities */
#define CAP_EEPROM          0x01

/* acceleration types */
#define ACC_CURVE           0x00
#define DEC_CURVE           0x01

/* curve types */
#define CRV_NORMALSCAN      0x00
#define CRV_PARKHOME        0x01
#define CRV_SMEARING        0x02
#define CRV_BUFFERFULL      0x03

/* Sample rates */
#define PIXEL_RATE          0x00
#define LINE_RATE           0x01

/* motor types */
#define MT_OUTPUTSTATE      0x00
#define MT_ONCHIP_PWM       0x01

/* motor step types */
#define STT_FULL            0x00	/* 90    degrees */
#define STT_HALF            0x01	/* 45    degrees */
#define STT_QUART           0x02	/* 22.5  degrees */
#define STT_OCT             0x03	/* 11.25 degrees */

/* motor options */
#define MTR_BACKWARD        0x00
#define MTR_FORWARD         0x08
#define MTR_ENABLED         0x00
#define MTR_DISABLED        0x10

/* sensors */
#define CCD_SENSOR          0x01
#define CIS_SENSOR          0x00

/* sony sensor models */
#define SNYS575             0x00

/* toshiba sensor models */
#define TCD2952             0x01
#define TCD2958             0x02
#define TCD2905             0x03

/* usb types */
#define USB20               0x01
#define USB11               0x00

/* scan types */
#define ST_NEG              0x03
#define ST_TA               0x02
#define ST_NORMAL           0x01

/* colour modes */
#define CM_COLOR            0x00
#define CM_GRAY             0x01
#define CM_LINEART          0x02

/* colour channels */
#define CL_RED              0x00
#define CL_GREEN            0x01
#define CL_BLUE             0x02

/* lamp types */
#define FLB_LAMP            0x01
#define TMA_LAMP            0x02

#define IST_NORMAL          0x00
#define IST_TA              0x01
#define IST_NEG             0x02

#define ICM_GRAY            0x00
#define ICM_LINEART         0x01
#define ICM_COLOR           0x02

#define TRUE                0x01
#define FALSE               0x00

/* function results */
#define OK                  0x00
#define ERROR              -1

#define RT_BUFFER_LEN       0x71a

#define FIX_BY_HARD         0x01
#define FIX_BY_SOFT         0x02

#define REF_AUTODETECT      0x02
#define REF_TAKEFROMSCANNER 0x01
#define REF_NONE            0x00

/* bulk operations */
#define BLK_WRITE           0x00
#define BLK_READ            0x01

/* constants for resizing functions */
#define RSZ_NONE            0x00
#define RSZ_DECREASE        0x01
#define RSZ_INCREASE        0x02

#define RSZ_GRAYL           0x00
#define RSZ_COLOURL         0x01
#define RSZ_COLOURH         0x02
#define RSZ_LINEART         0x03
#define RSZ_GRAYH           0x04

/* Macros for managing data */
#define _B0(x)              ((SANE_Byte)((x) & 0xFF))
#define _B1(x)              ((SANE_Byte)((x) >> 0x08))
#define _B2(x)              ((SANE_Byte)((x) >> 0x10))
#define _B3(x)              ((SANE_Byte)((x) >> 0x18))

/* operation constants used in RTS_GetImage */
#define OP_STATIC_HEAD      0x00000001
#define OP_COMPRESSION      0x00000004
#define OP_BACKWARD         0x00000010
#define OP_WHITE_SHAD       0x00000020
#define OP_USE_GAMMA        0x00000040
#define OP_BLACK_SHAD       0x00000080
#define OP_LAMP_ON          0x00000200

/* data types */

typedef unsigned short USHORT;

#ifdef STANDALONE
/* Stand-alone*/
#define SANE_STATUS_GOOD 0x00

typedef unsigned char SANE_Byte;
typedef int SANE_Int;
typedef usb_dev_handle *USB_Handle;

#else

/* SANE backend */
typedef SANE_Int USB_Handle;

#endif

/* structures */

struct st_debug_opts
{
  /* device capabilities */
  SANE_Int dev_model;

  SANE_Byte SaveCalibFile;
  SANE_Byte DumpShadingData;
  SANE_Byte ScanWhiteBoard;
  SANE_Byte EnableGamma;
  SANE_Byte use_fixed_pwm;
  SANE_Int dmabuffersize;
  SANE_Int dmatransfersize;
  SANE_Int dmasetlength;
  SANE_Int usbtype;

  SANE_Int calibrate;
  SANE_Int wshading;

  SANE_Int overdrive_flb;
  SANE_Int overdrive_ta;
  SANE_Byte warmup;

  SANE_Int shd;
};

struct st_chip
{
  SANE_Int model;
  SANE_Int capabilities;
  char *name;
};

struct st_shading
{
  double *rates;
  SANE_Int count;
  SANE_Int ptr;
};

struct st_scanning
{
  SANE_Byte *imagebuffer;
  SANE_Byte *imagepointer;
  SANE_Int bfsize;
  SANE_Int channel_size;

  /* arrange line related variables */
  SANE_Int arrange_hres;
  SANE_Int arrange_compression;
  SANE_Int arrange_sensor_evenodd_dist;
  SANE_Int arrange_orderchannel;
  SANE_Int arrange_size;

  /* Pointers to each channel colour */
  SANE_Byte *pColour[3];
  SANE_Byte *pColour1[3];
  SANE_Byte *pColour2[3];

  /* Channel displacements */
  SANE_Int desp[3];
  SANE_Int desp1[3];
  SANE_Int desp2[3];
};

struct st_resize
{
  SANE_Byte mode;
  SANE_Int type;
  SANE_Int fromwidth;
  SANE_Int towidth;
  SANE_Int bytesperline;
  SANE_Int rescount;
  SANE_Int resolution_x;
  SANE_Int resolution_y;

  SANE_Byte *v3624;
  SANE_Byte *v3628;
  SANE_Byte *v362c;
};

struct st_gammatables
{
  SANE_Int depth;		/*0=0x100| 4=0x400 |8=0x1000 */
  SANE_Byte *table[3];
};

struct st_readimage
{
  SANE_Int Size4Lines;

  SANE_Byte Starting;
  SANE_Byte *DMABuffer;
  SANE_Int DMABufferSize;
  SANE_Byte *RDStart;
  SANE_Int RDSize;
  SANE_Int DMAAmount;
  SANE_Int Channel_size;
  SANE_Byte Channels_per_dot;
  SANE_Int ImageSize;
  SANE_Int Bytes_Available;
  SANE_Int Max_Size;
  SANE_Byte Cancel;
};

struct st_gain_offset
{
  /* 32 bytes 08be|08e0|3654
     red green blue */
  SANE_Int edcg1[3];		/* 08e0|08e2|08e4 *//*Even offset 1 */
  SANE_Int edcg2[3];		/* 08e6|08e8|08ea *//*Even offset 2 */
  SANE_Int odcg1[3];		/* 08ec|08ee|08f0 *//*Odd  offset 1 */
  SANE_Int odcg2[3];		/* 08f2|08f4|08f6 *//*Odd  offset 2 */
  SANE_Byte pag[3];		/* 08f8|08f9|08fa */
  SANE_Byte vgag1[3];		/* 08fb|08fc|08fd */
  SANE_Byte vgag2[3];		/* 08fe|08ff|0900 */
};

struct st_calibration_config
{
  SANE_Int WStripXPos;
  SANE_Int WStripYPos;
  SANE_Int BStripXPos;
  SANE_Int BStripYPos;
  SANE_Int WRef[3];
  SANE_Int BRef[3];
  SANE_Byte RefBitDepth;
  double OffsetTargetMax;
  double OffsetTargetMin;
  double OffsetBoundaryRatio1;
  double OffsetBoundaryRatio2;
  double OffsetAvgRatio1;
  double OffsetAvgRatio2;
  SANE_Int CalibOffset10n;
  SANE_Int CalibOffset20n;
  SANE_Int AdcOffEvenOdd;
  SANE_Int AdcOffQuickWay;
  SANE_Int OffsetEven1[3];
  SANE_Int OffsetOdd1[3];
  SANE_Int OffsetEven2[3];
  SANE_Int OffsetOdd2[3];
  SANE_Byte OffsetHeight;
  SANE_Int OffsetPixelStart;
  SANE_Int OffsetNPixel;
  SANE_Byte OffsetNSigma;
  SANE_Int AdcOffPredictStart;
  SANE_Int AdcOffPredictEnd;
  SANE_Byte OffsetAvgTarget[3];
  SANE_Byte OffsetTuneStep1;
  SANE_Byte OffsetTuneStep2;
  double GainTargetFactor;
  SANE_Int CalibGain10n;
  SANE_Int CalibGain20n;
  SANE_Int CalibPAGOn;
  SANE_Int GainHeight;
  SANE_Int unk1[3];
  SANE_Int unk2[3];
  SANE_Byte PAG[3];
  SANE_Byte Gain1[3];
  SANE_Byte Gain2[3];
  /* White Shading */
  SANE_Int WShadingOn;
  SANE_Int WShadingHeight;
  SANE_Int WShadingPreDiff[3];
  SANE_Int unknown;		/*?? */
  double ShadingCut[3];
  /* Black Shading */
  SANE_Int BShadingOn;
  SANE_Int BShadingHeight;
  SANE_Int BShadingDefCutOff;
  SANE_Int BShadingPreDiff[3];
  double ExternBoundary;
  SANE_Int EffectivePixel;
  SANE_Byte TotShading;
};

struct st_calibration
{
  /* faac */
  struct st_gain_offset gain_offset;	/* 0..35 */
  USHORT *white_shading[3];	/* +36 +40 +44 */
  USHORT *black_shading[3];	/* +48 +52 +56 */
  SANE_Int WRef[3];		/* +60 +62 +64 */
  SANE_Byte shading_type;	/* +66 */
  SANE_Byte shading_enabled;	/* +67 */
  SANE_Int first_position;	/* +68 */
  SANE_Int shadinglength;	/* +72 */
};

struct st_cal2
{
  /* f9f8  35 bytes */
  SANE_Int table_count;		/* +0  f9f8 */
  SANE_Int shadinglength1;	/* +4  f9fc */
  SANE_Int tables_size;		/* +8  fa00 */
  SANE_Int shadinglength3;	/* +12 fa04 */
  USHORT *tables[4];		/* +16+20+24+28  fa08 fa0c fa10 fa14 */
  USHORT *table2;		/* +32 fa18 */
};

struct st_coords
{
  SANE_Int left;
  SANE_Int width;
  SANE_Int top;
  SANE_Int height;
};

struct params
{
  SANE_Int scantype;
  SANE_Int colormode;
  SANE_Int resolution_x;
  SANE_Int resolution_y;
  struct st_coords coords;
  SANE_Int depth;
  SANE_Int channel;
};

struct st_constrains
{
  struct st_coords reflective;
  struct st_coords negative;
  struct st_coords slide;
};

struct st_scanparams		/* 44 bytes size */
{
  /* 760-78b|155c-1587|fa58-fa83|f0c4 */
  SANE_Byte colormode;		/* [+00] 760 */
  SANE_Byte depth;		/* [+01] 761 */
  SANE_Byte samplerate;		/* [+02] 762 */
  SANE_Byte timing;		/* [+03] 763 */
  SANE_Int channel;		/* [+04] 764 */
  SANE_Int sensorresolution;	/* [+06] 766 */
  SANE_Int resolution_x;	/* [+08] 768 */
  SANE_Int resolution_y;	/* [+10] 76a */
  struct st_coords coord;	/* [+12] left */
  /* [+16] width */
  /* [+20] top */
  /* [+24] height */
  SANE_Int shadinglength;	/* [+28] 77c */
  SANE_Int v157c;		/* [+32] 780 */
  SANE_Int bytesperline;	/* [+36] 784 */
  SANE_Int expt;		/* [+40] 788 */

  SANE_Int startpos;		/* [+44] 78c */
  SANE_Int leftleading;		/* [+46] 78e */
  SANE_Int ser;			/* [+48] 790 */
  SANE_Int ler;			/* [+52] 794 */
  SANE_Int scantype;		/* [+58] 79a */
};

struct st_hwdconfig		/* 28 bytes size */
{
  /* fa84-fa9f|f0ac-f0c7|e838-e853|f3a4-f3bf */
  SANE_Int startpos;		/* +0 */
  /* +1..7 */
  SANE_Byte arrangeline;	/* +8 */
  SANE_Byte scantype;		/* +9 */
  SANE_Byte compression;	/* +10 */
  SANE_Byte use_gamma_tables;	/* +11 */
  SANE_Byte gamma_tablesize;	/* +12 */
  SANE_Byte white_shading;	/* +13 */
  SANE_Byte black_shading;	/* +14 */
  SANE_Byte unk3;		/* +15 */
  SANE_Byte motorplus;		/* +16 */
  SANE_Byte static_head;	/* +17 */
  SANE_Byte motor_direction;	/* +18 */
  SANE_Byte dummy_scan;		/* +19 */
  SANE_Byte highresolution;	/* +20 */
  SANE_Byte sensorevenodddistance;	/* +21 */
  /* +22..23 */
  SANE_Int calibrate;		/* +24 */
};

struct st_calibration_data
{
  SANE_Byte Regs[RT_BUFFER_LEN];
  struct st_scanparams scancfg;
  struct st_gain_offset gain_offset;
};

struct st_cph
{
  double p1;
  double p2;
  SANE_Byte ps;
  SANE_Byte ge;
  SANE_Byte go;
};

struct st_timing
{
  SANE_Int sensorresolution;
  SANE_Byte cnpp;
  SANE_Byte cvtrp[3];		/* 3 transfer gates */
  SANE_Byte cvtrw;
  SANE_Byte cvtrfpw;
  SANE_Byte cvtrbpw;
  struct st_cph cph[6];		/* Linear Image Sensor Clocks */
  SANE_Int cphbp2s;
  SANE_Int cphbp2e;
  SANE_Int clamps;
  SANE_Int clampe;
  SANE_Byte cdss[2];
  SANE_Byte cdsc[2];
  SANE_Byte cdscs[2];		/* Toshiba T958 ccd from hp4370 */
  double adcclkp[2];
  SANE_Int adcclkp2e;
};

struct st_scanmode
{
  SANE_Int scantype;
  SANE_Int colormode;
  SANE_Int resolution;

  SANE_Byte timing;
  SANE_Int motorcurve;
  SANE_Byte samplerate;
  SANE_Byte systemclock;
  SANE_Int ctpc;
  SANE_Int motorbackstep;
  SANE_Byte scanmotorsteptype;

  SANE_Byte dummyline;
  SANE_Int expt[3];
  SANE_Int mexpt[3];
  SANE_Int motorplus;
  SANE_Int multiexposurefor16bitmode;
  SANE_Int multiexposureforfullspeed;
  SANE_Int multiexposure;
  SANE_Int mri;
  SANE_Int msi;
  SANE_Int mmtir;
  SANE_Int mmtirh;
  SANE_Int skiplinecount;
};

struct st_motormove
{
  SANE_Byte systemclock;
  SANE_Int ctpc;
  SANE_Byte scanmotorsteptype;
  SANE_Int motorcurve;
};

struct st_motorpos
{
  SANE_Int coord_y;
  SANE_Byte options;
  SANE_Int v12e448;
  SANE_Int v12e44c;
};

struct st_find_edge
{
  SANE_Int exposuretime;
  SANE_Int scanystart;
  SANE_Int scanylines;
  SANE_Int findlermethod;
  SANE_Int findlerstart;
  SANE_Int findlerend;
  SANE_Int checkoffsetser;
  SANE_Int findserchecklines;
  SANE_Int findserstart;
  SANE_Int findserend;
  SANE_Int findsermethod;
  SANE_Int offsettoser;
  SANE_Int offsettoler;
};

struct st_curve
{
  SANE_Int crv_speed;		/* acceleration or deceleration */
  SANE_Int crv_type;
  SANE_Int step_count;
  SANE_Int *step;
};

struct st_motorcurve
{
  SANE_Int mri;
  SANE_Int msi;
  SANE_Int skiplinecount;
  SANE_Int motorbackstep;
  SANE_Int curve_count;
  struct st_curve **curve;
};

struct st_checkstable
{
  double diff;
  SANE_Int interval;
  long tottime;
};

struct st_sensorcfg
{
  SANE_Int type;
  SANE_Int name;
  SANE_Int resolution;

  SANE_Int channel_color[3];
  SANE_Int channel_gray[2];
  SANE_Int rgb_order[3];

  SANE_Int line_distance;
  SANE_Int evenodd_distance;
};

struct st_autoref
{
  SANE_Byte type;
  SANE_Int offset_x;
  SANE_Int offset_y;
  SANE_Int resolution;
  SANE_Int extern_boundary;
};

struct st_motorcfg
{
  SANE_Byte type;
  SANE_Int resolution;
  SANE_Byte pwmfrequency;
  SANE_Int basespeedpps;
  SANE_Int basespeedmotormove;
  SANE_Int highspeedmotormove;
  SANE_Int parkhomemotormove;
  SANE_Byte changemotorcurrent;
};

struct st_buttons
{
  SANE_Int count;
  SANE_Int mask[6];		/* up to 6 buttons */
};

struct st_status
{
  SANE_Byte warmup;
  SANE_Byte parkhome;
  SANE_Byte cancel;
};

struct st_device
{
  /* next var handles usb device, used for every usb operations */
  USB_Handle usb_handle;

  /* next buffer will contain initial state registers of the chipset */
  SANE_Byte *init_regs;

  /* next structure will contain information and capabilities about chipset */
  struct st_chip *chipset;

  /* next structure will contain general configuration of stepper motor */
  struct st_motorcfg *motorcfg;

  /* next structure will contain general configuration of ccd sensor */
  struct st_sensorcfg *sensorcfg;

  /* next structure will contain all ccd timing values */
  SANE_Int timings_count;
  struct st_timing **timings;

  /* next structure will contain all possible motor movements */
  SANE_Int motormove_count;
  struct st_motormove **motormove;

  /* next structure will contain all motorcurve values */
  SANE_Int mtrsetting_count;
  struct st_motorcurve **mtrsetting;

  /* next structure will contain all possible scanning modes for one scanner */
  SANE_Int scanmodes_count;
  struct st_scanmode **scanmodes;

  /* next structure contains constrain values for one scanner */
  struct st_constrains *constrains;

  /* next structure contains supported buttons and their order */
  struct st_buttons *buttons;

  /* next structure will be used to resize scanned image */
  struct st_resize *Resize;

  /* next structure will be used while reading image from device */
  struct st_readimage *Reading;

  /* next structure will be used to arrange color channels while scanning */
  struct st_scanning *scanning;

  /* next structure will contain some status which can be requested */
  struct st_status *status;
};

/* Unknown vars */
SANE_Int v14b4 = 0;
SANE_Byte *v1600 = NULL;	/* tabla */
SANE_Byte *v1604 = NULL;	/* tabla */
SANE_Byte *v1608 = NULL;	/* tabla */
SANE_Byte v160c_block_size;
SANE_Int mem_total;
SANE_Byte v1619;
SANE_Int v15f8;

SANE_Int acccurvecount;		/* counter used y MotorSetup */
SANE_Int deccurvecount;		/* counter used y MotorSetup */
SANE_Int smearacccurvecount;	/* counter used y MotorSetup */
SANE_Int smeardeccurvecount;	/* counter used y MotorSetup */

/* Known vars */
SANE_Int offset[3];
SANE_Byte gain[3];

static SANE_Int usbfile = -1;
SANE_Int scantype;

SANE_Byte pwmlamplevel;

SANE_Byte arrangeline;
SANE_Byte binarythresholdh;
SANE_Byte binarythresholdl;

SANE_Byte shadingbase;
SANE_Byte shadingfact[3];
SANE_Byte arrangeline;
SANE_Int compression;

SANE_Byte linedarlampoff;
SANE_Int pixeldarklevel;

SANE_Int bw_threshold = 0x00;

/* SetScanParams */
struct st_scanparams scan;
struct st_scanparams scan2;

SANE_Int bytesperline;		/* width * (3 colors [RGB]) */
SANE_Int imagewidth3;
SANE_Int lineart_width;
SANE_Int imagesize;		/* bytesperline * coords.height */
SANE_Int imageheight;
SANE_Int line_size;
SANE_Int v15b4;
SANE_Int v15bc;
SANE_Int waitforpwm;

SANE_Byte WRef[3];

USHORT *fixed_black_shading[3] = { NULL, NULL, NULL };
USHORT *fixed_white_shading[3] = { NULL, NULL, NULL };

/* Calibration */
struct st_gain_offset mitabla2;	/* calibration table */
SANE_Int v0750;

static SANE_Byte use_gamma_tables;

SANE_Int read_v15b4 = 0;

SANE_Int v35b8 = 0;
SANE_Int arrangeline2;

SANE_Int v07c0 = 0;

/* next structure contains coefficients for white shading correction */
struct st_shading *wshading;

struct st_gammatables *hp_gamma;
struct st_gain_offset *default_gain_offset;
struct st_calibration_data *calibdata;

struct st_debug_opts *RTS_Debug;

/* testing */
SANE_Byte *jkd_black = NULL;
SANE_Int jkd_blackbpl;
