/** @file u12-hwdef.h
 *  @brief Definitions for the ASIC.
 *
 * Copyright (c) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - change _DEF_BW_THRESHOLD
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
#ifndef __U12_HWDEF_H__
#define __U12_HWDEF_H__

#define _LOWORD(x)	((u_short)(x & 0xffff))
#define _HIWORD(x)	((u_short)(x >> 16))
#define _LOBYTE(x)  ((SANE_Byte)((x) & 0xFF))
#define _HIBYTE(x)  ((SANE_Byte)((x) >> 8))

#define _SET_REG(b,c,r,v) {b[c*2]=r; b[(c*2)+1]=v;c++;}

/** some magics for the ASIC */
#define _ID_TO_PRINTER  0x00
#define _ID1ST          0x69
#define _ID2ND          0x96
#define _ID3RD          0xa5
#define _ID4TH          0x5a

/** some more for reset...
 */
#define _RESET1ST       0x69
#define _RESET2ND       0x96
#define _RESET3RD       0xaa
#define _RESET4TH       0x55

/** Printer Control Port: Definitions
 */
#define _CTRL_STROBE        0x01
#define _CTRL_AUTOLF        0x02
#define _CTRL_NOT_INIT      0x04
#define _CTRL_SELECT_IN     0x08
#define _CTRL_ENABLE_IRQ    0x10
#define _CTRL_DIRECTION     0x20
#define _CTRL_RESERVED      0xc0

/** for Asic I/O signal control
 */
#define _CTRL_GENSIGNAL         (_CTRL_RESERVED + _CTRL_NOT_INIT)   /* 0xc4 */
#define _CTRL_SIGNAL_REGWRITE   (_CTRL_GENSIGNAL + _CTRL_SELECT_IN) /* 0xcc */
#define _CTRL_END_REGWRITE	    (_CTRL_GENSIGNAL)                   /* 0xc4 */
#define _CTRL_SIGNAL_DATAWRITE  (_CTRL_GENSIGNAL + _CTRL_AUTOLF)    /* 0xc6 */
#define _CTRL_END_DATAWRITE     (_CTRL_GENSIGNAL)                   /* 0xc4 */

#define ASIC_ID 0x83

/** the Register set
 */
#define REG_SWITCHBUS           0x00
#define REG_EPPENABLE           0x01

#define REG_READDATAMODE        0x03
#define REG_WRITEDATAMODE       0x04
#define REG_INITDATAFIFO        0x05
#define REG_FORCESTEP           0x06

#define REG_REFRESHSCANSTATE    0x08

#define REG_WAITSTATEINSERT     0x0a
#define REG_RFIFOOFFSET         0x0a
#define REG_GFIFOOFFSET         0x0b
#define REG_BFIFOOFFSET         0x0c

#define REG_STEPCONTROL         0x14
#define REG_MOTOR0CONTROL       0x15
#define REG_XSTEPTIME           0x16

#define REG_GETSCANSTATE        0x17
#define REG_ASICID              0x18
#define REG_MEMORYLO            0x19
#define REG_MEMORYHI            0x1a
#define REG_MODECONTROL         0x1b
#define REG_LINECONTROL         0x1c
#define REG_SCANCONTROL         0x1d
#define REG_CONFIG              0x1e
#define REG_MODELCONTROL        0x1f
#define REG_MODEL1CONTROL       0x20
#define REG_DPILO               0x21
#define REG_DPIHI               0x22
#define REG_SCANPOSLO           0x23
#define REG_SCANPOSHI           0x24
#define REG_WIDTHPIXELLO        0x25
#define REG_WIDTHPIXELHI        0x26
#define REG_THRESHOLDLO         0x27
#define REG_THRESHOLDHI         0x28

#define REG_ADCADDR             0x2a
#define REG_ADCDATA             0x2b
#define REG_ADCSERIALOUT        0x2d

#define REG_RESETCONFIG         0x2e

#define REG_STATUS              0x30
#define REG_SCANSTATECONTROL    0x31

#define REG_REDCHDARKOFFSETLO   0x33
#define REG_REDCHDARKOFFSETHI   0x34
#define REG_GREENCHDARKOFFSETLO 0x35
#define REG_GREENCHDARKOFFSETHI 0x36
#define REG_BLUECHDARKOFFSETLO  0x37
#define REG_BLUECHDARKOFFSETHI  0x38

#define REG_FIFOFULLEN0         0x54
#define REG_FIFOFULLEN1         0x55
#define REG_FIFOFULLEN2         0x56
#define REG_MOTORTOTALSTEP0     0x57
#define REG_MOTORTOTALSTEP1     0x58
#define REG_MOTORFREERUNCOUNT0  0x59
#define REG_MOTORFREERUNCOUNT1  0x5a
#define REG_SCANCONTROL1        0x5b
#define REG_MOTORFREERUNTRIGGER 0x5c
#define REG_RESETMTSC           0x5d

#define REG_MOTORDRVTYPE        0x64

#define REG_STATUS2             0x66

#define REG_EXTENDEDLINECONTROL 0x6d
#define REG_EXTENDEDXSTEP       0x6e

#define REG_PLLPREDIV           0x71
#define REG_PLLMAINDIV          0x72
#define REG_PLLPOSTDIV          0x73
#define REG_CLOCKSELECTOR       0x74

#define REG_TESTMODE            0xf0

/* Register RegStepControl (Addr: 0x14) */
#define _MOTOR0_ONESTEP         0x01
#define _MOTOR0_SCANSTATE       0x02
#define _MOTOR_FREERUN          0x40
#define _MOTOR_NOFREERUN        0x00

/* Register RegGetScanState (Addr: 0x17)*/
#define _SCANSTATE_MASK         0x3f	/* bit 0-5 */
#define _SCANSTATE_STOP         0x80

/* Register RegMemoryLow/High (Addr: 0x19/0x1a)*/
#define _MAP_ADDR_RED           0x00
#define _MAP_ADDR_GREEN         0x40
#define _MAP_ADDR_BLUE          0x80
#define _MAP_ADDR_SIZE          0x40

/* Register RegModeControl (Addr: 0x1b)*/
#define _ModeScan               0x00
#define _ModeIdle               0x01
#define _ModeShadingMem         0x02
#define _ModeMappingMem         0x03
#define _ModeReadMappingMem     0x07
#define _ModeFifoRSel           0x00
#define _ModeFifoGSel           0x08
#define _ModeFifoBSel           0x10
#define _ModeFifoClose          0x18

/* Register RegLineControl (Addr: 0x1c) */
#define _LINE_SCANTIME_MASK     0x3f	/* bit 0-6              */
#define _LINE_CDSINVERSE        0x80	/* Color Drive Signal   */

/* Register RegScanControl (Addr: 0x1d) */
#define _SCAN_BITMODE           0x00
#define _SCAN_BYTEMODE          0x01    /* Gray/Color mode                  */
#define _SCAN_12BITMODE         0x02
#define _SCAN_1ST_AVERAGE       0x04    /* first pixel is averaged pixel    */
#define _SCAN_BITDIRR2L         0x08    /* bit shift from right to left     */
#define _SCAN_NORMALLAMP_ON     0x10    /* normal Lamp  */
#define _SCAN_TPALAMP_ON        0x20
#define _SCAN_DATA_INVERT       0x40
#define _BITALIGN_LEFT          0x80

#define _SCAN_LAMPS_ON          (_SCAN_NORMALLAMP_ON | _SCAN_TPALAMP_ON)
#define _SCAN_LAMP_MASK         _SCAN_LAMPS_ON

/* Register RegMotor0Control (Addr: 0x15) */
#define _MotorDirForward        0x01
#define _MotorDirBackward       0x00
#define _MotorOn                0x02
#define _MotorHFullStepH        0x00
#define _MotorHHalfStep         0x04
#define _MotorHQuarterStep      0x08
#define _MotorPowerEnable       0x40    
#define _MotorHHomeStop         0x80

#define _DIR_FW   1
#define _DIR_BW   2
#define _DIR_NONE 0

#define _FORWARD_MOTOR  (_MotorDirForward + _MotorOn + \
                         _MotorHQuarterStep + _MotorPowerEnable)
#define _BACKWARD_MOTOR (_MotorDirBackward + _MotorOn + _MotorHHomeStop + \
                         _MotorHQuarterStep + _MotorPowerEnable)

/* Register RegConfiguration (Addr: 0x1e) */
#define _P98_CCD_TYPE_ID       0x07
#define _P98_NEC_MACHINE       0x08
#define _P98_PCBID             0xF0

#define _DEF_BW_THRESHOLD      128      /* default B/W mode threshold value */
#define _NUMBER_OF_SCANSTEPS   64       /* Asic spec.: up to 64 scan steps  */
#define _SCANSTATE_BYTES      (_NUMBER_OF_SCANSTEPS/2)

#define _CCD_3797               0
#define _CCD_3799               1
#define _CCD_535                2
#define _CCD_2556               3
#define _CCD_518                4
#define _CCD_539                5
#define _CCD_3777               6
#define _CCD_548                7

/* PCB-IDs (from parport driver)...  */
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
#define _AGFA_SCANNER           0xf0
#define _AGFA_PCB               0x1f

/* Register RegModelControl (Addr: 0x1f) */
#define _HOME_SENSOR_POLARITY   0x01    
#define _LED_CONTROL            0x02
#define _LED_ACTIVITY           0x04
#define _MODEL_DPI200           0x00
#define _MODEL_DPI300           0x08
#define _MODEL_DPI400           0x10
#define _MODEL_DPI600           0x18
#define _MODEL_DPI800           0x20
#define _MODEL_DPI1200          0x28
#define _DUMMY_12BIT            0x40

/* Register RegModel1Control (Addr: 0x20) */
#define _SCAN_GRAYTYPE          0x01
#define _CCD_SHIFT_GATE         0x02
#define _CCD_SHIFT_PULSE        0x04
#define _BUTTON_MODE            0x08
#define _MOTOR_2003             0x00
#define _MOTOR_2916             0x10
#define _MOTOR_7042             0x20

/* Register RegThresholdGapControl (Addr: 0x29) */
#define _THRESHOLDGAP_MASK      0x0f

/* Register RegResetConfig (Addr: 0x2e) */
#define _ADC_MASK               0x07
#define _DA_WOLFSON8143         0x00
#define _DA_ESIC                0x04
#define _DA_SAMSUNG8531         0x05
#define _DA_WOLFSON8141         0x06
#define _DA_SAMSUNG1224         0x07
#define _MOTOR0_MASK            0x18
#define _MOTOR0_2003            0x00
#define _MOTOR0_2916            0x08
#define _MOTOR0_7042            0x10
#define _MOTOR1_MASK            0x60
#define _MOTOR1_2003            0x00
#define _MOTOR1_2916            0x20
#define _MOTOR1_7042            0x40

/* Status Register (Addr: 0x30) */
#define _FLAG_PAPER             0x01
#define _FLAG_KEY               0x80

/* Register RegFifoFullLength (Addr: 0x54) */
#define _RED_FULLSIZE           0x00
#define _GREEN_FULLSIZE         0x08
#define _BLUE_FULLSIZE          0x10

/* Register RegScanControl1	(Addr: 0x5b) */
#define _MTSC_ENABLE            0x01
#define _SCANSTOPONBUFFULL      0x02
#define _MFRC_RUNSCANSTATE      0x04
#define _MFRC_BY_XSTEP          0x08

/* Register RegMotorDriverType (Addr: 0x64) */
#define _MOTORS_MASK            0x33
#define _MOTORR_MASK            0xf3
#define _MOTORR_WEAK            0x04
#define _MOTORR_MEDIUM          0x08
#define _MOTORR_STRONG          0x0c
#define _MOTORT_WEAK            0x40
#define _MOTORT_MEDIUM          0x80
#define _MOTORT_STRONG          0xc0
#define _BUTTON_SELECT1         0x40
#define _BUTTON_SELECT2         0x80
#define _BUTTON_DISABLE         0xc0

/** Register RegStatus2	(Addr: 0x66) */
#define _REFLECTIONLAMP_ON      0x01
#define _TPALAMP_ON             0x02
#define _STILL_FREE_RUNNING     0x04
#define _BUFFER_IS_FULL         0x08

/** Register RegTestMode	(Addr: 0xf0) */
#define _SW_TESTMODE            0x20

/** buffer sizes */
#define _BYTES_PER_CHANNEL    5500UL
#define _SIZE_DATA_BUF        (u_long)(_BYTES_PER_CHANNEL * 3 * 2)
#define _SIZE_TPA_DATA_BUF    (u_long)(_BYTES_PER_CHANNEL * 3 * 2)
#define _SIZE_SHADING_SUM_BUF (u_long)(_BYTES_PER_CHANNEL * 3 * 4)
#define _SIZE_TOTAL_BUF       (u_long)(_SIZE_DATA_BUF + _SIZE_SHADING_SUM_BUF)
#define _SIZE_TOTAL_BUF_TPA   (u_long)(_SIZE_TOTAL_BUF + _SIZE_TPA_DATA_BUF)

/** internal FIFO buffers */
#define _SIZE_REDFIFO   196608UL    /* 192k */
#define _SIZE_GREENFIFO 147456UL    /* 144k */
#define _SIZE_BLUEFIFO  114688UL    /* 112k */

#define _SIZE_GRAYFIFO  (_SIZE_REDFIFO + _SIZE_GREENFIFO + _SIZE_BLUEFIFO)

/* Scan State Definitions */
#define _SS_STEP   0x08
#define _SS_RED    0x04
#define _SS_GREEN  0x02
#define _SS_BLUE   0x01

#define _SS_MONO   _SS_GREEN
#define _SS_COLOR  (_SS_RED | _SS_GREEN | _SS_BLUE)

/** some positioning stuff
 */
#define _RFT_SCANNING_ORG  380U
#define _POS_SCANNING_ORG  2840U
#define _NEG_SCANNING_ORG  3000U
#define _TPA_SHADINGORG    2172U

#define _DATA_ORIGIN_X     72
#define _YOFFSET           300

#define _POS_PAGEWIDTH     450U
#define _POS_ORG_OFFSETX   0x41C

#define _NEG_PAGEWIDTH     464U
#define _NEG_PAGEWIDTH600  992U
#define _NEG_ORG_OFFSETX   0x430
#define _NEG_EDGE_VALUE    0x800
#define _NEG_SHADING_OFFS  1500U

#define _SHADING_BEGINX    4U


#define _DEFAULT_LINESCANTIME   96

#define _LINE_TIMEOUT   (_SECOND * 5 ) 


/** for mirroring parts of the 98003 ASIC register set
 */
typedef struct {
	SANE_Byte RD_Motor1Control;     /* 0x0b           */
	SANE_Byte RD_StepControl;       /* 0x14           */
	SANE_Byte RD_Motor0Control;     /* 0x15           */
	SANE_Byte RD_XStepTime;         /* 0x16           */
	SANE_Byte RD_ModeControl;       /* 0x1b           */
	SANE_Byte RD_LineControl;       /* 0x1c           */
	SANE_Byte RD_ScanControl;       /* 0x1d, init = 5 */
	SANE_Byte RD_ModelControl;      /* 0x1f           */
	SANE_Byte RD_Model1Control;     /* 0x20           */
	u_short   RD_Dpi;               /* 0x21           */
	u_short   RD_Origin;            /* 0x23           */
	u_short   RD_Pixels;            /* 0x25           */
	u_short   RD_ThresholdControl;  /* 0x27           */
	SANE_Byte RD_ThresholdGapCtrl;  /* 0x29           */
	u_short   RD_RedDarkOff;        /* 0x33           */
	u_short   RD_GreenDarkOff;      /* 0x35           */
	u_short   RD_BlueDarkOff;       /* 0x37           */

	u_long    RD_BufFullSize;       /* 0x54   */
	u_short   RD_MotorTotalSteps;   /* 0x57   */

	SANE_Byte RD_ScanControl1;      /* 0x5b   */
	SANE_Byte RD_MotorDriverType;   /* 0x64   */
	SANE_Byte RD_ExtLineControl;    /* 0x6d   */
	SANE_Byte RD_ExtXStepTime;      /* 0x6e   */

} ShadowRegs;

/** to hold all the shading stuff for calibrating a scanner
 */
typedef struct svd
{
	ColorWord   GainResize;
	ColorWord   DarkCmpHi;
	ColorWord   DarkCmpLo;
	ColorWord   DarkOffSub;
	ColorByte   DarkDAC;
	SANE_Byte   Reserved;
} ShadingVarDef;

typedef struct 
{
	ShadingVarDef *pCcdDac;
	ColorByte      DarkDAC;
	ColorWord      DarkOffset;
	u_short        wDarkLevels;
	SANE_Byte      intermediate;

	u_long         dwDiv;
	SANE_Byte      skipShadow;

	SANE_Byte      skipHilight;
	ColorByte      Hilight;
	RGBUShortDef  *pHilight;
	ColorByte      Gain;
	SANE_Byte      bGainDouble;
	SANE_Byte      bUniGain;
	SANE_Byte      bMinGain;
	SANE_Byte      bMaxGain;
	SANE_Byte      bGainHigh;
	SANE_Byte      bGainLow;

	SANE_Bool      fStop;

	u_short        wExposure;
	u_short        wXStep;

} ShadingDef;

#endif	/* guard __U12_HWDEF_H__ */

/* END U12-HWDEF.H ..........................................................*/
