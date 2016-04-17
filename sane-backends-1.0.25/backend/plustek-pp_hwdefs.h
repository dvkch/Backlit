/* @file plustek-pp_hwdefs.h
 * @brief different definitions for describing the scanner hardware
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson <rick@efn.org>
 *
 * History:
 * 0.30 - initial version
 * 0.31 - corrected the values of _GAIN_P98_HIGH, _GAIN_P96_HIGH and _GAIN_LOW
 * 0.32 - removed _LampDelay defines
 *		  removed _MODE_xxx defines
 * 0.33 - cosmetic changes
 *		  removed _PORT_BPP from modelist
 * 0.34 - no changes
 * 0.35 - no changes
 * 0.36 - moved struct ScanState to this header
 *		  moved some definitions from scandata.h to this file
 * 0.37 - some cleanup work
 *        added model override defines here
 *        added _A3
 *        changed some _ defines to _ defines
 * 0.38 - added ASIC98003 stuff
 *        removed the _ASIC_xxxxx definitions
 * 0.39 - major changes: moved a lot of stuff to this file
 * 0.40 - no changes
 * 0.41 - added _OVR_PLUSTEK_4800P definition
 * 0.42 - added _OVR_PRIMAX_4800D30 definition
 * 0.43 - no changes
 * .
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
#ifndef __HWDEFS_H__
#define __HWDEFS_H__

/*
 * port modes
 * (WARNING: never change these defines, as they are used as entries
 *           to jump tables !!!)
 */
#define _PORT_EPP       0
#define _PORT_SPP       1
#define _PORT_BIDI      2
#define _PORT_ECP       3
#define _PORT_ESP       4
#define _PORT_NONE      5

/*
 * ScannerSize
 */
#define _SCANSIZE_A4        0
#define _SCANSIZE_LETTER    1
#define _SCANSIZE_LEGAL     2
#define _SCANSIZE_A3        3

/*
 * Magic IDs switch printer port to scanner mode
 */
#define _ID1ST          0x69
#define _ID2ND          0x96
#define _ID3RD          0xa5
#define _ID4TH          0x5a

/*
 * Special IDs used to reset scanner (ASIC98003)
 */
#define _RESET1ST       0x69
#define _RESET2ND       0x96
#define _RESET3RD       0xaa
#define _RESET4TH       0x55

/*
 * ID switch to printer mode
 */
#define _ID_TO_PRINTER	0

/*
 * Flags for internal use
 */
#define _VF_BUILDMAP		    0x0000001
#define _VF_DATATOUSERBUFFER    0x0000002
#define _VF_ENVIRONMENT_READY   0x0000004
#define _VF_FIRSTSCANLINE	    0x0000008
#define _VF_PREVIEW     		0x0000020

/*
 * Printer Control Port: Definitions
 */
#define _CTRL_STROBE		0x01
#define _CTRL_AUTOLF		0x02
#define _CTRL_NOT_INIT		0x04
#define _CTRL_SELECT_IN		0x08
#define _CTRL_ENABLE_IRQ	0x10
#define _CTRL_DIRECTION		0x20
#define _CTRL_RESERVED  	0xc0

/*
 * here are the definitions for the different registers
 * first the ASIC 96001/3 section
 */

/* Status Register 0x10 */
#define _FLAG_P96_PAPER		 0x01
#define _FLAG_P96_ADF		 0x02
#define _FLAG_P96_KEY		 0x04
#define _FLAG_P96_EPP		 0x08
#define _FLAG_P96_FIFOFULL	 0x10
#define _FLAG_P96_FIFOEMPTY	 0x20
#define _FLAG_P96_CCDTYPE	 0x40
#define _FLAG_P96_MOTORTYPE  0x80

/*
 * the 98001 section
 */
#define _DEFAULT_LINESCANTIME   96

/* Status Register (Addr: 0x30) */
#define	_FLAG_P98_PAPER      0x01
#define _FLAG_P98_KEY		 0x80

/*
 * some buffer sizes (for ASIC 98001 based devices)
 */
#define _LINE_BUFSIZE		(5500 * 6)
#define _LINE_BUFSIZE1		(5500 * 8)
#define _PROCESS_BUFSIZE 	(5120 * 3)

/*
 * generic equates
 */
#define _DEF_BW_THRESHOLD	  111		/* default B/W mode threshold value	*/
#define _NUMBER_OF_SCANSTEPS   64		/* Asic spec.: up to 64 scan steps 	*/
#define _SCANSTATE_BYTES      (_NUMBER_OF_SCANSTEPS/2)

/* CHECK: Play around with the P98003/1
 * gain values - maybe we get a brighter picture...
 */
#define _GAIN_P98_HIGH		  225		/* Volt. max. value, Asic 98001		*/
#define _GAIN_P98003_HIGH	  240		/* Volt. max. value, Asic 98003		*/
#define _GAIN_P96_HIGH		  240		/* Volt. max. value, Asic 96001/3	*/
#define _GAIN_LOW			  210		/* Volt. min. value					*/
#define _GAIN_P98003_LOW	  220		/* Volt. min. value, Asic 98003 	*/
#define _MOTOR_ONE_LINE_TIME    4

/* for ASIC 98001/3 shading */
#define _DEF_BRIGHTEST_SKIP     3
#define _DEF_DARKEST_SKIP		5

#define _BUF_SIZE_BASE_CONST 	1280
#define _SCANSTATE_TABLE_SIZE	250		/* was 200 for 4830 */
#define _P98_OFFSET70			60

/* for Data channel (BYTE) */
#define _RED_DATA_READY		0x01
#define _GREEN_DATA_READY 	0x02
#define _BLUE_DATA_READY	0x04

#define _ASIC_REDCOLOR		0x01
#define _ASIC_GREENCOLOR    0x03
#define _ASIC_BLUECOLOR		0x02

/*
 * for Asic I/O signal control
 */
#define _CTRL_GENSIGNAL         (_CTRL_RESERVED + _CTRL_NOT_INIT)   /* 0xc4 */

#define _CTRL_START_REGWRITE    (_CTRL_GENSIGNAL + _CTRL_SELECT_IN) /* 0xcc */
#define _CTRL_END_REGWRITE      (_CTRL_GENSIGNAL)                   /* 0xc4 */

#define _CTRL_START_DATAWRITE   (_CTRL_GENSIGNAL + _CTRL_AUTOLF)    /* 0xc6 */
#define _CTRL_END_DATAWRITE     (_CTRL_GENSIGNAL)                   /* 0xc4 */

#define _CTRL_EPPSIGNAL_WRITE   (_CTRL_GENSIGNAL + _CTRL_STROBE)    /* 0xc5 */
#define _CTRL_EPPTRIG_REGWRITE  (_CTRL_GENSIGNAL + _CTRL_SELECT_IN + _CTRL_STROBE)

#define _CTRL_START_BIDIREAD    (_CTRL_GENSIGNAL + _CTRL_DIRECTION + _CTRL_AUTOLF)
#define _CTRL_END_BIDIREAD      (_CTRL_GENSIGNAL + _CTRL_DIRECTION) /* 0xe4 */


typedef struct
{
    ULong	dwFullSpeed;
    Byte	bCurrentSpeed;
    Byte	bStepSpeed;
} DiffModeVar, *pDiffModeVar;

typedef struct
{
    UShort wHomePos;		/* scanner's scanning home position	*/
    UShort wMaxSteps;		/* maximum steps for this scan		*/
    Byte   bExposureTime;	/* exposure time for one line		*/
    Byte   bMotorStep;
    Byte   bFlagScanMode;	/* see below	*/
    Byte   bTimesShading;	/* see below	*/
} ModeTypeVar, *pModeTypeVar;

typedef struct {
	Byte	bStep;
	Byte	bStatus;
} ScanState, *pScanState;

typedef struct {
    Byte    bReg;
    Byte    bParam;
} RegDef, *pRegDef;

typedef union {
   	RGBByteDef	Colors;
    UChar		bColors[3];
} ColorByte, *pColorByte;

typedef union {
    RGBUShortDef Colors;
    UShort		 wColors[3];
} ColorWord, *pColorWord;

typedef struct {
    UShort exposureTime;
    UShort xStepTime;
} ExpXStepDef, *pExtXStepDef;

typedef struct {
    UShort  thresholdBW;
    UShort  thresholdGray;
    UShort  thresholdColor;
} ThreshDef, *pThreshDef;

/* for decription of the DAC specific stuff*/
typedef struct {
    ColorWord	GainResize;
    ColorWord   DarkCmpHi;
    ColorWord   DarkCmpLo;
    ColorWord   DarkOffSub;
    ColorByte   DarkDAC;
    UChar		Reserved;
} DACTblDef, *pDACTblDef;

/*
 * some function types
 */
typedef struct scandata *pScanData;
typedef void (*pFnDataProcess)(pScanData, pVoid, pVoid, ULong);
typedef Bool (*pFnReadData)(pScanData, pUChar, ULong);
typedef void (*pFnVoid)(pScanData);
typedef Bool (*pFnBool)(pScanData);
typedef void (*pFnDACOffs)(pScanData, pDACTblDef, ULong);
typedef void (*pFnDACDark)(pScanData, pDACTblDef, ULong, UShort);
typedef void (*pFnOut)(Byte,UShort);
typedef Byte (*pFnIn)(UShort);

/*
 * For Motor step control
 */
#define _FullStepStrong 	0x10
#define _HalfStep			0x04
#define _QuarterStep		0x08
#define _EightStep			0x0c

/* bTimesShading */
#define _Shading_32Times	0
#define _Shading_4Times 	1
#define _Shading_16Times	2

/* Scan.bModuleState */
#define _MotorInNormalState	0
#define _MotorGoBackward	1
#define _MotorInStopState	2
#define _MotorAdvancing	    3
#define _MotorAdvanced	    4

/* bMoveDataOutFlag */
#define _DataInNormalState		0
#define _DataAfterRefreshState	1
#define _DataFromStopState		2

/* bFastMoveFlag */
#define _FastMove_Fast_C50_G100 	0
#define _FastMove_Middle_C75_G150	1
#define _FastMove_Low_C75_G150		2
#define _FastMove_Low_C75_G150_Back	4
#define _FastMove_Pos_300			5
#define _FastMove_Film_150			6

/*
 * ASIC 98003 base models...
 */
#define _TPA_P98003_SHADINGORG	2172U
#define _RFT_SCANNING_ORG	    380U

#define _POS_SCANNING_ORG	    2840U
#define _POS_PAGEWIDTH  		450U
#define _POS_ORG_OFFSETX    	0x41C

#define _NEG_SCANNING_ORG	    3000U
#define _NEG_PAGEWIDTH		    464U
#define _NEG_PAGEWIDTH600       992U
#define _NEG_ORG_OFFSETX    	0x430
#define _NEG_SHADING_OFFS       1500U

#define _NEG_EDGE_VALUE 		0x800

#define _SHADING_BEGINX 		4U


/* home positions */
#define _Home_CE50		0x1f	/* 0x1d     1b	*/
#define _Home_CE100		0x27	/* 0x34     23	*/
#define _Home_CE150		0x29	/* 0x23     25	*/
#define _Home_CE300		0x34	/* 0x2e     30	*/
#define _Home_CE600		0x3a	/* 0x35     36	*/
#define _Home_CB50		0x20	/* 0x22     1c	*/
#define _Home_CB100		0x41	/* 0x42     3d	*/
#define _Home_CB150		0x41	/* 0x3f     3d	*/
#define _Home_CB300		0x4c	/* 0x4a     48	*/
#define _Home_CB600		0x53	/* 0x54     4f	*/
#define _Home_CS50		0x20	/* 0x1b     1c	*/
#define _Home_CS100		0x41	/* 0x40     3d	*/
#define _Home_CS150		0x41	/* 0x3e     3d	*/
#define _Home_CS300		0x4c	/* 0x4b     48	*/
#define _Home_CS600		0x53	/* 0x4f     4f	*/
#define _Home_GE75		0x08	/* 7-14-98  00	*/
#define _Home_GE150		0x04	/* 0x22     00	*/
#define _Home_GE300		0x12	/* 0x2d     0e	*/
#define _Home_GE600		0x19	/* 0x33     15	*/
#define _Home_GB75		0x40	/* 7-14-98  3e	*/
#define _Home_GB150		0x40	/* 0x3f     3e	*/
#define _Home_GB300		0x4e	/* 0x4c     4a	*/
#define _Home_GB600		0x53	/* 0x51     4f	*/
#define _Home_GS75		0x40	/* 7-14-98  3c	*/
#define _Home_GS150		0x40	/* 0x3f     3c	*/
#define _Home_GS300		0x4e	/* 0x4d     4a	*/
#define _Home_GS600		0x53	/* 0x51     4f	*/
#define _Home_BE75		0x30	/* 7-14-98  20	*/
#define _Home_BE150		0x30	/* 0x3c     20	*/
#define _Home_BE300		0x32	/* 0x48     2e	*/
#define _Home_BE600		0x37	/* 0x35     33	*/
#define _Home_BB75		0x42	/* 7-14-98  3e	*/
#define _Home_BB150		0x42	/* 0x47     3e 	*/
#define _Home_BB300		0x50	/* 0x51     4c	*/
#define _Home_BB600		0x53	/* 0x51     4f	*/
#define _Home_BS75		0x42	/* 7-14-98  3e	*/
#define _Home_BS150		0x42	/* 0x46     3e	*/
#define _Home_BS300		0x50	/* 0x4f     4c	*/
#define _Home_BS600		0x53	/* 0x55     4f	*/

/*
 * for ModeTypeVar indexes
 */
#define _FixParamEppBw		    0
#define _FixParamBppBw		    _FixParamEppBw + 4
#define _FixParamSppBw		    _FixParamBppBw + 4
#define _FixParamEppGray	    0
#define _FixParamBppGray	    _FixParamEppGray + 4
#define _FixParamSppGray	    _FixParamBppGray + 4
#define _FixParamEppColor	    0
#define _FixParamBppColor	    _FixParamEppColor + 5
#define _FixParamSppColor	    _FixParamBppColor + 5
#define _FixParamSppNegative    0
#define _FixParamBppNegative    _FixParamSppNegative + 3
#define _FixParamEppNegative    _FixParamBppNegative + 3
#define _FixParamSppPositive    _FixParamEppNegative + 3
#define _FixParamBppPositive    _FixParamSppPositive + 3
#define _FixParamEppPositive    _FixParamBppPositive + 3

/*
 * for DiffModeVar indexes
 */
#define _BwEpp150		    0
#define _BwEpp300		    1
#define _BwEpp600		    2
#define _BwBpp150		    _BwEpp150
#define _BwBpp300		    _BwEpp300
#define _BwBpp600		    _BwEpp600
#define _BwSpp150		    3
#define _BwSpp300		    4
#define _BwSpp600		    5
#define _GrayEpp150		    6
#define _GrayEpp300		    7
#define _GrayEpp600		    8
#define _GrayEpp600_3000	9	    /* > 3000 pixels per channel */
#define _GrayBpp150		    10
#define _GrayBpp300		    11
#define _GrayBpp300_1600	12
#define _GrayBpp600		    13
#define _GrayBpp600_1600	14
#define _GrayBpp600_3200	15
#define _GraySpp150		    16
#define _GraySpp150_800 	17
#define _GraySpp300		    18
#define _GraySpp300_800 	19
#define _GraySpp300_1600	20
#define _GraySpp600		    21
#define _GraySpp600_800 	22
#define _GraySpp600_1600	23
#define _GraySpp600_3200	24
#define _ColorEpp60		    25
#define _ColorEpp100		_ColorEpp60
#define _ColorEpp150		26
#define _ColorEpp300		27
#define _ColorEpp300_1200	28
#define _ColorEpp600		29
#define _ColorEpp600_1400	30
#define _ColorEpp600_2800	31
#define _ColorEpp600_4000	32
#define _ColorBpp60		    33
#define _ColorBpp100		34
#define _ColorBpp150		35
#define _ColorBpp150_800	36
#define _ColorBpp300		37
#define _ColorBpp300_800	38
#define _ColorBpp300_1600	39
#define _ColorBpp600		40
#define _ColorBpp600_800	41
#define _ColorBpp600_1600	42
#define _ColorBpp600_3200	43
#define _ColorSpp60		    _ColorBpp60
#define _ColorSpp100		_ColorBpp100
#define _ColorSpp150		_ColorBpp150
#define _ColorSpp150_800	_ColorBpp150_800
#define _ColorSpp300		44
#define _ColorSpp300_500	45
#define _ColorSpp300_1000	46
#define _ColorSpp300_2000	47
#define _ColorSpp600		48
#define _ColorSpp600_500	49
#define _ColorSpp600_1000	50
#define _ColorSpp600_2000	51
#define _ColorSpp600_4000	52

#define _NegativeSpp150 	53
#define _NegativeSpp300 	54
#define _NegativeSpp600 	55
#define _NegativeBpp150 	_NegativeSpp150
#define _NegativeBpp300 	_NegativeSpp300
#define _NegativeBpp600 	_NegativeSpp600
#define _NegativeEpp150 	_NegativeSpp150
#define _NegativeEpp300 	_NegativeSpp300
#define _NegativeEpp600 	_NegativeSpp600

#define _BwEpp75		    56
#define _BwBpp75		    56
#define _BwSpp75		    56
#define _GrayEpp75		    56
#define _GrayBpp75		    56
#define _GraySpp75		    56

#define _TransparencySpp150	_NegativeSpp150
#define _TransparencySpp300	_NegativeSpp300
#define _TransparencySpp600	_NegativeSpp600
#define _TransparencyBpp150	_NegativeSpp150
#define _TransparencyBpp300	_NegativeSpp300
#define _TransparencyBpp600	_NegativeSpp600
#define _TransparencyEpp150	_NegativeSpp150
#define _TransparencyEpp300	_NegativeSpp300
#define _TransparencyEpp600	_NegativeSpp600

/* for 48 bits color */
#define _ColorEpp100_1400	63
#define _ColorEpp150_1900	64
#define _ColorEpp300_4000	65
#define _ColorEpp600_9600	66
#define _ColorSpp300_3000	67

/*
 * for mirroring parts of the 98001/3 asic register set
 */
typedef struct {
    Byte	RD_Motor1Control;		/* 0x0b             	*/
    Byte	RD_StepControl; 		/* 0x14					*/
    Byte	RD_Motor0Control;		/* 0x15					*/
    Byte	RD_XStepTime;			/* 0x16					*/
    Byte	RD_ModeControl; 		/* 0x1b					*/
    Byte	RD_LineControl; 		/* 0x1c					*/
    Byte	RD_ScanControl; 		/* 0x1d, init = 5		*/
    Byte	RD_ModelControl;		/* 0x1f					*/
    Byte	RD_Model1Control;		/* 0x20					*/
    UShort	RD_Dpi; 			    /* 0x21					*/
    UShort	RD_Origin;			    /* 0x23					*/
    UShort	RD_Pixels;			    /* 0x25					*/
    UShort	RD_ThresholdControl;	/* 0x27					*/
    Byte	RD_ThresholdGapCtrl;	/* 0x29					*/
    UShort	RD_RedDarkOff;			/* 0x33					*/
    UShort	RD_GreenDarkOff;		/* 0x35					*/
    UShort	RD_BlueDarkOff; 		/* 0x37					*/

    ULong   RD_BufFullSize;         /* 0x54, ASIC 98003     */
    UShort  RD_MotorTotalSteps;     /* 0x57, ASIC 98003     */

    Byte    RD_ScanControl1;        /* 0x5b, ASIC 98003     */
    Byte    RD_MotorDriverType;     /* 0x64, ASIC 98003     */
    Byte    RD_ExtLineControl;      /* 0x6d, ASIC 98003     */
    Byte    RD_ExtXStepTime;        /* 0x6e, ASIC 98003     */

} RegData, *pRegData;

/*
 * for registers, that only exist on the 96001/3
 */
typedef struct
{
    Byte	RD_MotorControl;			/* 0x1b, init = 3		*/
    Byte	RD_MemAccessControl;		/* 0x1d					*/
    Byte	RD_WatchDogControl;			/* 0x25, init = 0x8f	*/

    union {
		Byte	RD_ModelControl2;		/* 0x26, P96003 		*/
    } u26;

	union {
		Byte	RD_RedChShadingOff;		/* 0x28, P96003			*/
    } u28;
    union {
		Byte	RD_GreenChShadingOff;	/* 0x29, P96003			*/
    } u29;
	Byte	RD_BlueChShadingOff;		/* 0x2a, P96003			*/
    Byte	RD_RedChDarkOff;			/* 0x2b, P96003			*/
    Byte	RD_GreenChDarkOff;			/* 0x2c, P96003			*/
    Byte	RD_BlueChDarkOff;			/* 0x2d, P96003			*/

    Byte	RD_RedChEvenOff;			/* 0x31, P96003			*/
    Byte	RD_GreenChEvenOff;			/* 0x32, P96003			*/
    Byte	RD_BlueChEvenOff;			/* 0x33, P96003			*/
    Byte	RD_RedChOddOff; 			/* 0x34, P96003			*/
    Byte	RD_GreenChOddOff;			/* 0x35, P96003			*/
    Byte	RD_BlueChOddOff;			/* 0x36, P96003			*/
    Byte	RD_RedGainOut;				/* 0x37, P96003			*/
    Byte	RD_GreenGainOut;			/* 0x38, P96003			*/
    Byte	RD_BlueGainOut; 			/* 0x39, P96003			*/
    Byte	RD_LedControl;				/* 0x3a, P96003			*/
    Byte	RD_ShadingCorrectCtrl;		/* 0x3b, P96003			*/

} Reg96, *pReg96;

/*
 * model override defines
 */
#define _OVR_NONE				0
#define _OVR_PLUSTEK_9630PL		1	/* for legal version of the OP9630	*/
#define _OVR_PRIMAX_4800D		2	/* for the Primax 4800 Direct		*/
#define _OVR_PLUSTEK_9636		3	/* for 9636T/P+/Turbo				*/
#define _OVR_PLUSTEK_9636P		4	/* for 9636P						*/
#define _OVR_PLUSTEK_A3I		5	/* for A3I  						*/
#define _OVR_PLUSTEK_4800P		6	/* for OpticPro4800					*/
#define _OVR_PRIMAX_4800D30		7	/* for the Primax 4800 Direct 30bit	*/

/* CHECK: THIS has to be changed and merged with the old code */

/*
 * structure to hold IO port specific stuff
 */
typedef struct {

#ifdef __KERNEL__
	pFnOut fnOut;
	pFnIn  fnIn;

	UShort pbSppDataPort;
	UShort pbEppDataPort;

	UShort pbStatusPort;
	UShort pbControlPort;
	UShort pbAddrOffsetPort;
#endif

	UShort portBase;
	UShort portMode;
	UShort lastPortMode;

	Byte   bOldControlValue;
	Byte   bOldDataValue;
	Byte   bOpenCount;
	Byte   delay;           /* to allow various delay on port operations    */

	Bool   slowIO;
	UShort forceMode;
	Bool   useEPPCmdMode;

} IODef, *pIODef;


/*
 * structure to hold device specific stuff
 */
typedef struct
{
#if 0
    PFNVOID			pfnIOWrite;
    PFNVOID			pfnCCDInit;
    DWORD			dwLampOnTicks;
    DWORD			dwLampOnBegin;
    LONG			lLeftPositive;
    LONG			lLeftNegative;
    UCHAR			bLampOnMinutes;
    DRVBOOL			fButton;
    DRVBOOL			fSoftReset;
    UCHAR			bStatusLamp;
#endif

	pFnReadData     ReadData;       /* read function, portmode specific */

    ULong   		dwModelOriginY;
    Bool			f0_8_16;
    Bool            fTpa;           /* transparency adapter ?           */
    Bool			f2003;          /* has 2003 motor driver ?          */

    UChar			bMotorID;       /* the type of the motor drivers    */
    UChar			bPCBID;         /* which version of the PCB         */

    UChar			bCCDID;         /* what CCD do we have              */
    pRegDef         pCCDRegisters;  /* pointer to the register descr    */
    UShort			wNumCCDRegs;    /* number of values to write        */
    UShort   		DataOriginX;

    UChar   		bDACType;       /* what DAC do we have              */
    pRegDef			pDACRegisters;  /* pointer to DAC reg descr.        */
    UShort			wNumDACRegs;    /* number of values to write        */
    pFnDACOffs		fnDarkOffset;   /* func-ptr to func for DAC         */
    pFnDACDark		fnDACDark;      /* adjustments                      */

    RGBByteDef		RegDACOffset;
    RGBByteDef		RegDACGain;

    UChar			buttons;        /* Number of buttons                */

    UChar			ModelCtrl;      /* contents of the model control reg*/
    UChar			Model1Mono;     /* for model control 1 in mono mode */
    UChar			Model1Color;    /* for model control 1 in color mode*/

    UChar			XStepMono;
    UChar			XStepColor;
    UChar			XStepBack;

    long			lUpNormal;      /* device specific offsets */
    long			lUpPositive;
    long			lUpNegative;
    long			lLeftNormal;

} DeviceDef, *pDeviceDef;

/*
 * to hold all the shading stuff for calibrating a scanner
 */
typedef struct
{
    pRGBUShortDef	pHilight;
    ColorByte		Hilight;
    ULong   		dwDiv;
    UShort			wExposure;
    UShort  		wXStep;

    UChar			skipHilight;
    UChar			skipShadow;
    ULong			shadingBytes;

    pDACTblDef		pCcdDac;
    ColorByte		DarkDAC;
    ColorWord		DarkOffset;
    UShort			wDarkLevels;
    UChar			bIntermediate;

    ColorByte		Gain;
    UChar			bGainDouble;
    UChar			bUniGain;
    Byte			bMinGain;
    Byte			bMaxGain;
    Byte			bGainHigh;
    Byte			bGainLow;

    Bool			fStop;

} ShadingDef, *pShadingDef;


/*
 * structure to hold scan-specific data
 */
typedef struct _SCANDEF
{
#if 0
    DWORD			dwLinesRead;
    LONG			lBufAdjust;
    DRVBOOL			fBackmove;
    DRVBOOL			fEsc;
    UCHAR			Reserved;
#endif

    pFnBool			DoSample;
    pFnDataProcess	DataProcess;    /* to convert RGB buffers to RGB pixels */
    pFnBool			DataRead;       /* function to get data from scanner    */

    ULong			dwLinesToRead;  /* number of images lines to read       */
    Long	    	lBufferAdjust;

    ULong			dwScanOrigin;   /* where to start the scan              */
    Bool			fRefreshState;  /* refresh ?                            */
    Bool			fMotorBackward;
    UChar			motorPower;     /* how to drive the motor               */

    ULong			dwMinReadFifo;
    ULong			dwMaxReadFifo;
    Byte            bFifoSelect;    /* defines which FIFO to use            */

    UChar			bDiscardAll;
    ULong			dwInterval;
    ULong			dwInterlace;    /* CHECK: here always 0 - remove ?     */

    union {
        UShort		wGreenDiscard;
        UShort  	wGreenKeep;
    } gd_gk;
    union {
        UShort		wBlueDiscard;
        UShort  	wRedKeep;
    } bd_rk;

    UChar			bRefresh;       /* for controlling the movement         */
    UChar			bOldScanState;
    UChar   		bNowScanState;
    UChar			bModuleState;

    DataPointer 	p48BitBuf;      /* for handling 48-bit data             */
    union {
        pUChar  	pMonoBuf;
        DataPointer ColorBuf;
    } bp;

    RBGPtrDef       BufBegin;       /* for reading/writing the scan-data    */
    RBGPtrDef       BufEnd;
    RBGPtrDef       BufGet;
    RBGPtrDef       BufData;
    RBGPtrDef       BufPut;

    ULong           negBegin;       /* used while scanning in TPA modes     */
    ULong           posBegin;

    ULong           dpiIdx;         /* index to get/set values in the table */
    pExtXStepDef    negScan;        /* reference to exposure/xtep table     */

} ScanDef, *pScanDef;

/*
 * structure to hold buffer pointers
 */
typedef struct {

    union {
    	pUChar		pReadBuf;
    	pUChar		pShadingMap;
    	pUChar		pRWTestBuffer;
    	pUShort		pShadingRam;
    	DataPointer	Buf;
    } b1;

    union {
        pUChar        pSumBuf;
	    pRGBUShortDef pSumRGB;
	    pUChar		  pRWTestBuffer1;
    } b2;
    DataPointer TpaBuf;

} BufferDef, *pBufferDef;


/* Register RegBitDepth (Addr: 0x13) - ASIC 9800x */
#define _BIT0_7		            0x00
#define _BIT8_15		        0x01
#define _BIT16_20	    	    0x02

/* Register RegStepControl (Addr: 0x14) - ASIC 9800x */
#define _MOTOR0_ONESTEP	        0x01
#define _MOTOR0_SCANSTATE	    0x02
#define _MOTOR_FREERUN          0x40
#define _MOTOR_NOFREERUN        0x00

/* Register RegGetScanState (Addr: 0x17, 0x12 on 9600x) - ASIC 9800x */
#define _SCANSTATE_MASK         0x3f	/* bit 0-5 */
#define _SCANSTATE_STOP	        0x80

/* Register RegReadIOBufBus (Addr: 0x17) - ASIC 9600x only */
#define _IOBUF_BUSMASK	        0x0f	/* bit 0-3 */

/* Register RegMemoryLow/High (Addr: 0x19/0x1a) - ASIC 9800x */
#define _MAP_ADDR_RED           0x00
#define _MAP_ADDR_GREEN	        0x40
#define _MAP_ADDR_BLUE	        0x80
#define _MAP_ADDR_SIZE	        0x40	/* 0x4000 */

/* Register RegModeControl (Addr: 0x1b, 0x18 on 9600x) - all ASICs*/
#define _ModeScan		        0x00    /* all ASICs       */
#define _ModeProgram	        0x01    /* 9600x def       */
#define _ModeIdle		        0x01    /* 9800x defs      */
#define _ModeShadingMem	        0x02
#define _ModeMappingMem	        0x03
#define _ModeReadMappingMem     0x07
#define _ModeFifoRSel	        0x00
#define _ModeFifoGSel	        0x08
#define _ModeFifoBSel	        0x10
#define _ModeFifoClose          0x18

/* Register RegLineControl (Addr: 0x1c, 0x19 on 9600x) - all ASICs*/
#define _LINE_SCANTIME_MASK     0x3f	/* bit 0-6              */
#define _LINE_CDSINVERSE        0x80	/* Color Drive Signal   */

/* Register RegScanControl (Addr: 0x1d) - all ASICs*/
#define _SCAN_BITMODE	        0x00
#define _SCAN_BYTEMODE	        0x01	/* Gray/Color mode                  */
#define _SCAN_1ST_AVERAGE	    0x04	/* first pixel is averaged pixel    */
#define _SCAN_BITDIRR2L         0x08	/* bit shift from right to left     */

/* ASIC 9600x section */
#define _P96_SCANDATA_INVERT    0x02
#define _SCAN_LAMP_ON 	        0x10

/* ASIC 9800x section */
#define _SCAN_12BITMODE         0x02
#define _SCAN_NORMALLAMP_ON     0x10    /* normal Lamp  */
#define _SCAN_TPALAMP_ON        0x20
#define _P98_SCANDATA_INVERT    0x40
#define _BITALIGN_LEFT	        0x80

#define _SCAN_LAMPS_ON          (_SCAN_NORMALLAMP_ON | _SCAN_TPALAMP_ON)

/* Register RegMotor0Control (Addr: 0x15) */
#define _MotorDirForward	    0x01
#define _MotorDirBackward	    0x00
#define _MotorOn		        0x02
#define _MotorHFullStepH	    0x00
#define _MotorHHalfStep	        0x04
#define _MotorHQuarterStep	    0x08
#define _MotorHEightStep	    0x08	/* for 2916 driver      */
#define _MotorPowerEnable	    0x40    /* ASIC 98003 specific  */
#define _MotorHHomeStop	        0x80

#define _FORWARD_MOTOR			(_MotorDirForward + _MotorOn + \
               					 _MotorHQuarterStep + _MotorPowerEnable)

/* Register RegConfiguration (Addr: 0x1e), ASIC 9800x */
#define	_P98_CCD_TYPE_ID	    0x07
#define	_P98_NEC_MACHINE        0x08
#define _P98_PCBID		        0xF0

#define _CCD_3797		        0
#define _CCD_3717		        1
#define _CCD_3799	            1   /* new for 98003     */
#define _CCD_535		        2
#define _CCD_2556		        3
#define _CCD_518		        4
#define _CCD_539		        5	/* default for 98001 */
#define _CCD_3777	            6   /* new for 98003     */
#define _CCD_548 	            7   /* new for 98003     */

/* ASIC 98003 section */
#define _OPTICWORKS2000         0x00
#define _PLUSTEK_SCANNER        0x10
#define _SCANNER_WITH_TPA       0x20
#define _SCANNER4Button         0x30
#define _SCANNER4ButtonTPA      0x40
#define _SCANNER5Button         0x50
#define _SCANNER5ButtonTPA      0x60
#define _SCANNER1Button         0x70
#define _SCANNER1ButtonTPA      0x80
#define _SCANNER2Button         0x90
#define _AGFA_SCANNER	        0xf0
#define _AGFA_PCB   	        0x1f

/* Register RegModelControl (Addr: 0x1f), all ASICs */
#define _ModelSampleAndHold     0x01	/* sample and hold              */
#define _ModelWhiteIs0	        0x02	/* white is 0                   */
#define _ModelInvertPF	        0x04	/* invert paper flag            */
#define _ModelDpi200	           0	/* (400 / 2)                    */
#define _ModelDpi300	        0x08	/* (600 / 2)                    */
#define _ModelDpi400	        0x10	/* (800 / 2)                    */
#define _ModelDpi600	        0x18	/* (1200 / 2)                   */
#define _ModelMemSize32k3	       0	/* 32k for 300 dpi color        */
#define _ModelMemSize64k3	    0x20	/* 64k for 300 dpi color        */
#define _ModelMemSize128k3	    0x40	/* 128k for 300 dpi color       */
#define _ModelMemSize64k4	    0x60	/* 64k for 400/600 dpi color    */
#define _ModelMemSize128k4	    0x80	/* 128k for 400/600 dpi color   */

#define _ModelMemSize8k	           0    /* for 96001 */
#define _ModelMemSize8k3	    0x20
#define _ModelMemSize32k96001   0x40
#define _ModelMemSize128k396001 0x60
#define _ModelMemSize128k696001 0x80

#define _HOME_SENSOR_POLARITY   0x01    /* 9800x */
#define _LED_CONTROL 	        0x02
#define _LED_ACTIVITY	        0x04
#define _MODEL_DPI800	        0x20
#define _MODEL_DPI1200	        0x28
#define _DUMMY_12BIT 	        0x40

/* Register RegModel1Control (Addr: 0x20), 9800x */
#define _SCAN_GRAYTYPE	        0x01
#define _CCD_SHIFT_GATE 	    0x02
#define _CCD_SHIFT_PULSE	    0x04
#define _BUTTON_MODE     	    0x08
#define _MOTOR_2003		        0x00
#define _MOTOR_2916		        0x10
#define _MOTOR_7042		        0x20

/* Register RegThresholdGapControl (Addr: 0x29, 0x27 on 9600x ) - all ASICs */
#define _THRESHOLDGAP_MASK      0x0f

/* Register RegResetConfig (Addr: 0x2e) */
#define _ADC_MASK		        0x07
#define _DA_WOLFSON8143	        0x00
#define _DA_ESIC 	            0x04
#define _DA_SAMSUNG8531	        0x05
#define _DA_WOLFSON8141	        0x06
#define _DA_SAMSUNG1224	        0x07
#define _MOTOR0_MASK 	        0x18
#define _MOTOR0_2003	        0x00
#define _MOTOR0_2916	        0x08
#define _MOTOR0_7042	        0x10
#define _MOTOR1_MASK 	        0x60
#define _MOTOR1_2003	        0x00
#define _MOTOR1_2916	        0x20
#define _MOTOR1_7042	        0x40

/* Register RegFifoFullLength (Addr: 0x54) */
#define _RED_FULLSIZE	        0x00
#define _GREEN_FULLSIZE         0x08
#define _BLUE_FULLSIZE	        0x10

/* Register RegScanControl1	(Addr: 0x5b) */
#define _MTSC_ENABLE 	        0x01
#define _SCANSTOPONBUFFULL      0x02
#define _MFRC_RUNSCANSTATE      0x04
#define _MFRC_BY_XSTEP	        0x08

/* Register RegMotorDriverType (Addr: 0x64) */
#define _MOTORS_MASK 	        0x33
#define _MOTORR_MASK	        0xf3
#define _MOTORR_WEAK	        0x04
#define _MOTORR_MEDIUM	        0x08
#define _MOTORR_STRONG	        0x0c
#define _MOTORT_WEAK	        0x40
#define _MOTORT_MEDIUM	        0x80
#define _MOTORT_STRONG	        0xc0
#define _BUTTON_SELECT1	        0x40
#define _BUTTON_SELECT2	        0x80
#define _BUTTON_DISABLE	        0xc0

/* Register RegStatus2	(Addr: 0x66) */
#define _REFLECTIONLAMP_ON      0x01
#define _TPALAMP_ON 	        0x02
#define _STILL_FREE_RUNNING     0x04
#define _BUFFER_IS_FULL	        0x08

/* Register RegTestMode	(Addr: 0xf0) */
#define _SW_TESTMODE	        0x20

/* other stuff... */

/* CHECK: changes this stuff... */
#define _BytesPerChannel	    5500UL
#define _SizeDataBuf		(ULong)(_BytesPerChannel * 3 * 2)
#define _SizeTpaDataBuf 	(ULong)(_BytesPerChannel * 3 * 2)
#define _SizeShadingSumBuf	(ULong)(_BytesPerChannel * 3 * 4)
#define _SizeTotalBuf		(ULong)(_SizeDataBuf + _SizeShadingSumBuf)
#define _SizeTotalBufTpa	(ULong)(_SizeTotalBuf + _SizeTpaDataBuf)

/* for DAC programming (ASIC 98003)*/
#define _VALUE_CONFIG		    0x51
#define _DAC_RED 	    	    (Byte)(_VALUE_CONFIG | 0x00)
#define _DAC_GREENCOLOR		    (Byte)(_VALUE_CONFIG | 0x04)
#define _DAC_GREENMONO		    (Byte)(_VALUE_CONFIG | 0x06)
#define _DAC_BLUE   		    (Byte)(_VALUE_CONFIG | 0x08)

/* internal FIFO buffers (ASIC 9800x) */
#define _SIZE_REDFIFO		    196608UL	/* 192k */
#define _SIZE_GREENFIFO		    147456UL	/* 144k */
#define _SIZE_BLUEFIFO		    114688UL	/* 112k */

#define _SIZE_COLORFIFO	        _SIZE_BLUEFIFO
#define _SIZE_GRAYFIFO	    (_SIZE_REDFIFO + _SIZE_GREENFIFO + _SIZE_BLUEFIFO)

/* Scan State Definitions */
#define _SS_STEP		        0x08
#define _SS_RED 		        0x04
#define _SS_GREEN		        0x02
#define _SS_BLUE		        0x01

#define _SS_MONO		        _SS_GREEN
#define _SS_COLOR		        (_SS_RED | _SS_GREEN | _SS_BLUE)

/* for shading */
#define _CHANNEL_RED	        0
#define _CHANNEL_GREEN	        1
#define _CHANNEL_BLUE	        2

#endif	/* guard __HWDEFS_H__ */

/* END PLUSTEK-PP_HWDEFS.H ..................................................*/
