/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Mustek.
   Originally maintained by Mustek
   Author:Roy 2005.5.24

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

   This file implements a SANE backend for the Mustek BearPaw 2448 TA Pro 
   and similar USB2 scanners. */

#ifndef MUSTEK_USB2_ASIC_H
#define MUSTEK_USB2_ASIC_H

#include "../include/sane/sanei_usb.h"

/* ---------------------- low level asic defines -------------------------- */

#define TRUE 1
#define FALSE 0

#define _MAX(a,b) ((a)>(b)?(a):(b))
#define _MIN(a,b) ((a)<(b)?(a):(b))

#ifndef LOBYTE
#define LOBYTE(w) (SANE_Byte)((unsigned short)(w) & 0x00ff)
#endif

#ifndef HIBYTE
#define HIBYTE(w) (SANE_Byte)((unsigned short)(w)>>8 & 0x00ff)
#endif


typedef enum tagFIRMWARESTATE
{
  FS_NULL = 0,
  FS_ATTACHED = 1,
  FS_OPENED = 2,
  FS_SCANNING = 3
} FIRMWARESTATE, *LPFIRMWARESTATE;

typedef enum tagMOTORSTATE
{
  MS_STILL = 0,
  MS_MOVED = 1
} MOTORSTATE, *LPMOTORSTATE;

typedef enum tagUSBHOST
{
  HT_USB10 = 0,
  HT_USB20 = 1
} USBHOST;

typedef enum tagLIGHTSOURCE
{
  LS_REFLECTIVE = 1,
  LS_POSITIVE = 2,
  LS_NEGATIVE = 4
} LIGHTSOURCE;

typedef struct
{
  unsigned int LongX;
  unsigned int PicWidth;
  unsigned int PicHeight;

  unsigned int Top;
  unsigned int Bottom;
  unsigned int Left;
  unsigned int Right;
  unsigned int ScanMode;
  unsigned int Dpi;
  unsigned int TotalMotorSteps;

  unsigned int CCD_Pixel_Length;
  SANE_Byte LineGap;
  SANE_Byte TG_Pulse_Width_Pixel;
  SANE_Byte TG_Wait_Width_Pixel;
  unsigned short Multi_TG_Dummy_Pixel;
  unsigned short CCD_Dummy_Pixel;
  SANE_Byte Dummy_Cycle;
  SANE_Byte TG_Times;

  double LineTime;

  unsigned short StartPixel;
  unsigned short StartLine;
}
ScanParam;

typedef struct
{
  unsigned int Shading_Table_Size;
  unsigned int Image_Buffer_Size;
  unsigned int Full_Bank;
  unsigned int Line_Pixel;
  double Line_Time;
  SANE_Byte LineGap;
}
Temps;

typedef struct
{
  /* AFE */
  unsigned int AFE_ADCCLK_Timing;
  unsigned int AFE_ADCVS_Timing;
  unsigned int AFE_ADCRS_Timing;
  unsigned short AFE_ChannelA_LatchPos;
  unsigned short AFE_ChannelB_LatchPos;
  unsigned short AFE_ChannelC_LatchPos;
  unsigned short AFE_ChannelD_LatchPos;
  SANE_Byte AFE_Secondary_FF_LatchPos;
  /* Sensor */
  unsigned int CCD_DummyCycleTiming;
  SANE_Byte PHTG_PluseWidth;
  SANE_Byte PHTG_WaitWidth;
  unsigned short ChannelR_StartPixel;
  unsigned short ChannelR_EndPixel;
  unsigned short ChannelG_StartPixel;
  unsigned short ChannelG_EndPixel;
  unsigned short ChannelB_StartPixel;
  unsigned short ChannelB_EndPixel;
  SANE_Byte PHTG_TimingAdj;
  SANE_Byte PHTG_TimingSetup;

  /*1200dpi */
  unsigned int CCD_PHRS_Timing_1200;
  unsigned int CCD_PHCP_Timing_1200;
  unsigned int CCD_PH1_Timing_1200;
  unsigned int CCD_PH2_Timing_1200;
  SANE_Byte DE_CCD_SETUP_REGISTER_1200;
  unsigned short wCCDPixelNumber_1200;

  /*600dpi */
  unsigned int CCD_PHRS_Timing_600;
  unsigned int CCD_PHCP_Timing_600;
  unsigned int CCD_PH1_Timing_600;
  unsigned int CCD_PH2_Timing_600;
  SANE_Byte DE_CCD_SETUP_REGISTER_600;
  unsigned short wCCDPixelNumber_600;
} Timings;


typedef struct tagADConverter
{
  SANE_Byte GainR;
  SANE_Byte GainG;
  SANE_Byte GainB;
  SANE_Byte OffsetR;
  SANE_Byte OffsetG;
  SANE_Byte OffsetB;
  SANE_Bool DirectionR;
  SANE_Bool DirectionG;
  SANE_Bool DirectionB;
} ADConverter, LPADConverter;



typedef struct
{
  unsigned int Shading;
  SANE_Byte Shading_0;
  SANE_Byte Shading_1;
  SANE_Byte Shading_2;

  unsigned int Motor;
  SANE_Byte Motor_0;
  SANE_Byte Motor_1;
  SANE_Byte Motor_2;

  SANE_Byte ImageEndAddr_0;
  SANE_Byte ImageEndAddr_1;
  SANE_Byte ImageEndAddr_2;

  SANE_Byte ImageFullBank_0;
  SANE_Byte ImageFullBank_1;
}
RamPosition;

typedef enum tagTASSTATUS
{
  TA_NOT_PLUGIN = 0,
  TA_PLUGIN = 1,
  TA_UNKNOW = 2
} TASTATUS;

typedef struct
{
  int fd;			/* File Description of Scanner */

  FIRMWARESTATE firmwarestate;	/* record firmware state */
  MOTORSTATE motorstate;	/* record motor status */
  SANE_Bool isFirstOpenChip;		/* If first open chip, is TRUE */
  USBHOST UsbHost;		/* The type of USB port */
  LIGHTSOURCE lsLightSource;	/* light source of scanner */
  ScanParam Scan;		/* The parameters of Scan */

  unsigned int dwBytesCountPerRow;
  unsigned int dwCalibrationBytesCountPerRow;

  Temps Temp;
  Timings Timing;
  ADConverter AD;

  SANE_Bool isHardwareShading;

  RamPosition RamPositions;

  unsigned short * lpGammaTable;
  SANE_Byte isMotorMove;

  unsigned int ibase1;
  unsigned int ibase2;

  unsigned short SWWidth;

  TASTATUS TA_Status;

  SANE_Byte isMotorGoToFirstLine;	/*Roy add */
  SANE_Byte * lpShadingTable;	/*Roy add */
  SANE_Byte isUniformSpeedToScan;
}
Asic, *PAsic;

typedef enum
{
  STATUS_GOOD = 0,
  STATUS_CANCELLED,
  STATUS_EOF,
  STATUS_DEVICE_BUSY,
  STATUS_INVAL,
  STATUS_MEM_ERROR,
  STATUS_IO_ERROR,
  STATUS_ACCESS_ERROR
}
STATUS;


/* For ScanObj */
typedef struct Point
{
  unsigned int x;
  unsigned int y;
}
Point;

typedef struct Rect
{
  unsigned int left;
  unsigned int right;
  unsigned int top;
  unsigned int bottom;
}
Rect;

typedef struct RGBColor
{
  unsigned short Red;
  unsigned short Green;
  unsigned short Blue;
}
RGBColor;

/* debug levels */
#define DBG_CRIT 	0	/* Critical errors thatshould be printed even
				   if user hasn't enabled debugging -- use 
				   with care and only after sane_open has been 
				   called */
#define DBG_ERR 	1	/* Other errors */
#define DBG_WARN 	2	/* unusual conditions that may not be fatal */
#define DBG_INFO 	3	/* information useful for the deucated user */
#define DBG_DET 	4	/* more detailed information */
#define DBG_FUNC 	5	/* start and exits of high level functions */
#define DBG_ASIC 	6	/* starts and exits of low level functions */
#define DBG_DBG 	10	/* usefull only for tracing bugs */


#define		DPI_2400		0x8000
#define		DPI_1200		0x8000
#define		DPI_600			0x8000
#define		DPI_300			0x4000
#define		DPI_200			0x2aaa
#define		DPI_150			0x2000
#define		DPI_100			0x1555
#define		DPI_75			0x1000
#define     DPI_50          0xaaa
#define		PIXEL_TIME		333	/*unit : ms */
#define DRAM_1Mx16_SIZE				(1024*1024)	/*unit : word */
#define PackAreaStartAddress ((DRAM_1Mx16_SIZE/4)*3)

#define TEMP_MEMORY_SIZE_64K 64*1024

#define CALIBRATION_PIXEL_WIDTH			10240	/*need 512x */
#define	CALIBRATE_WHITE_LINECOUNT	40
#define	CALIBRATE_DARK_LINECOUNT	2

#define ACTION_MODE_ACCDEC_MOVE 0
#define ACTION_MODE_UNIFORM_SPEED_MOVE 1

#define ACTION_TYPE_BACKWARD 0
#define ACTION_TYPE_FORWARD  1
#define ACTION_TYPE_BACKTOHOME 2
#define ACTION_TYPE_TEST_MODE 3


#define SENSOR0_DETECTED 0x10
#define SENSOR1_DETECTED 0x20
#define SENSOR0AND1_DETECTED 0x30

#define READ_RAM	0
#define WRITE_RAM	1

#define EXTERNAL_RAM	0
#define ON_CHIP_PRE_GAMMA	1
#define ON_CHIP_FINAL_GAMMA	2

#define MOTOR_TABLE_SIZE	512*8

#define ValidPixelNumberFor600DPI 5100 + 50 + 250
#define ValidPixelNumberFor1200DPI 10200 + 100 + 500

#define OverLapPixelNumber600 0
#define OverLapPixelNumber1200 0
#define SegmentGap 0
#define	BANK_SIZE	(64)

#define WaitBufferOneLineSize 11000*6

#define CCD_PIXEL_NUMBER   21600
#define CCD_Line_Spacing   24
#define CCD_EvneOdd_Spacing 2

#define	ShadingTableSize(x)			( ((x + 10)*6) + ( ((x + 10)*6)/240)*16 )
#define ACC_DEC_STEP_TABLE_SIZE		(512)	/*unit : word */
#define TableBase(x)				((((x)+((1<<TABLE_OFFSET_BASE)-1))>>TABLE_OFFSET_BASE)<<TABLE_OFFSET_BASE)
#define NUM_OF_ACC_DEC_STEP_TABLE	(8)	/*unit : word */
#define TABLE_BASE_SIZE				(1024*2*2*2*2)	/*unit : word */
#define LAMP0_PWM_DEFAULT            255
#define LAMP1_PWM_DEFAULT            255

#define TABLE_OFFSET_BASE			(14)	/*unit : word */
#define CHECK_HOME_SLEEP_TIME 100

#define _MOTOR_MOVE_TYPE _4_TABLE_SPACE_FOR_FULL_STEP


#define TA_CAL_PIXELNUMBER 50000

#define SENSOR_DPI  1200
#define TA_IMAGE_PIXELNUMBER 61000
#define MAX_PATH 256


/**************************** ASIC registers ***********************/

/*ES01_XX = Easy Scan_01 register hex xx*/
#define		ES01_00_AFEReg0				0x00
#define		ES01_01_AFEReg1				0x01
#define		ES01_02_AFEReg2				0x02
#define		ES01_03_AFEReg3				0x03
#define		ES01_04_AFEReset			0x04
#define		ES01_05_AFEReg4				0x05

#define		ES01_20_DACRed				0x20
#define		ES01_21_DACGreen			0x21
#define		ES01_22_DACBlue				0x22
#define		ES01_24_SignRed				0x24
#define		ES01_25_SignGreen			0x25
#define		ES01_26_SignBlue			0x26
#define		ES01_28_PGARed				0x28
#define		ES01_29_PGAGreen			0x29
#define		ES01_2A_PGABlue				0x2A

#define		ES01_00_ADAFEConfiguration	0x00
#define		ES01_02_ADAFEMuxConfig		0x02
#define		ES01_04_ADAFEPGACH1			0x04
#define		ES01_06_ADAFEPGACH2			0x06
#define		ES01_08_ADAFEPGACH3			0x08

#define		ES01_0C_ADAFEOffsetCH1P		0x0C
#define		ES01_0D_ADAFEOffsetCH1N		0x0D
#define		ES01_0E_ADAFEOffsetCH2P		0x0E
#define		ES01_0F_ADAFEOffsetCH2N		0x0F
#define		ES01_10_ADAFEOffsetCH3P		0x10
#define		ES01_11_ADAFEOffsetCH3N		0x11

#define		ES01_00_AD9826Configuration	0x00
#define		ES01_02_AD9826MuxConfig		0x02
#define		ES01_04_AD9826PGARed		0x04
#define		ES01_06_AD9826PGAGreen		0x06
#define		ES01_08_AD9826PGABlue		0x08
#define		ES01_0A_AD9826OffsetRedP	0x0a
#define		ES01_0B_AD9826OffsetRedN	0x0b
#define		ES01_0C_AD9826OffsetGreenP	0x0c
#define		ES01_0D_AD9826OffsetGreenN	0x0d
#define		ES01_0E_AD9826OffsetBlueP	0x0e
#define		ES01_0F_AD9826OffsetBlueN	0x0f

#define		ES02_50_MOTOR_CURRENT_CONTORL		0x50
			/* bit[0] */
#define	DOWN_LOAD_MOTOR_TABLE_DISABLE	0x00
#define	DOWN_LOAD_MOTOR_TABLE_ENABLE	0x01
			/* bit[3:1] */
#define	_4_TABLE_SPACE_FOR_FULL_STEP	0x00
#define	_8_TABLE_SPACE_FOR_1_DIV_2_STEP	0x02
#define	_16_TABLE_SPACE_FOR_1_DIV_4_STEP	0x06
#define	_32_TABLE_SPACE_FOR_1_DIV_8_STEP	0x0E
			/* bit[4] */
#define	MOTOR_TABLE_ADDR_SHOW_IN_FIRST_PIXEL_OF_LINE_DISABLE	0x00
#define	MOTOR_TABLE_ADDR_SHOW_IN_FIRST_PIXEL_OF_LINE_ENABLE		0x10
			/* bit[5] */
#define	MOTOR_CURRENT_TABLE_ADDRESS_BIT4_TO_BIT0_DISABLE	0x00
#define	MOTOR_CURRENT_TABLE_ADDRESS_BIT4_TO_BIT0_ENABLE		0x20

#define		ES02_51_MOTOR_PHASE_TABLE_1			0x51
#define		ES02_52_MOTOR_CURRENT_TABLE_A		0x52
#define		ES02_53_MOTOR_CURRENT_TABLE_B		0x53

#define		ES01_5F_REGISTER_BANK_SELECT	0x5F
		/* bit[1:0] */
#define		SELECT_REGISTER_BANK0	0x00
#define		SELECT_REGISTER_BANK1	0x01
#define		SELECT_REGISTER_BANK2	0x02

/* AFE Auto Configuration Gain & Offset Register */
#define		ES01_60_AFE_AUTO_GAIN_OFFSET_RED_LB		0x60
#define		ES01_61_AFE_AUTO_GAIN_OFFSET_RED_HB		0x61
#define		ES01_62_AFE_AUTO_GAIN_OFFSET_GREEN_LB	0x62
#define		ES01_63_AFE_AUTO_GAIN_OFFSET_GREEN_HB	0x63
#define		ES01_64_AFE_AUTO_GAIN_OFFSET_BLUE_LB	0x64
#define		ES01_65_AFE_AUTO_GAIN_OFFSET_BLUE_HB	0x65

#define	ES01_74_HARDWARE_SETTING	0x74
/* bit[0] */
#define	MOTOR1_SERIAL_INTERFACE_G10_8_ENABLE	0x01
#define	MOTOR1_SERIAL_INTERFACE_G10_8_DISABLE	0x00
/* bit[1]*/
#define LED_OUT_G11_ENABLE	0x02
#define LED_OUT_G11_DISABLE	0x00
		  /* bit[2] */
#define	SLAVE_SERIAL_INTERFACE_G15_14_ENABLE	0x04
#define	SLAVE_SERIAL_INTERFACE_G15_14_DISABLE	0x00
		  /* bit[3] */
#define	SHUTTLE_CCD_ENABLE	0x08
#define	SHUTTLE_CCD_DISABLE	0x00
		  /* bit[4] */
#define	HARDWARE_RESET_ESIC_AFE_ENABLE	0x10
#define	HARDWARE_RESET_ESIC_AFE_DISABLE	0x00

#define		ES01_79_AFEMCLK_SDRAMCLK_DELAY_CONTROL		0x79
			/* bit[3:0] */
#define	AFEMCLK_DELAY_0_ns		0x00
#define	AFEMCLK_DELAY_2_ns		0x01
#define	AFEMCLK_DELAY_4_ns		0x02
#define	AFEMCLK_DELAY_6_ns		0x03
#define	AFEMCLK_DELAY_8_ns		0x04
#define	AFEMCLK_DELAY_10_ns		0x05
#define	AFEMCLK_DELAY_12_ns		0x06
#define	AFEMCLK_DELAY_14_ns		0x07
#define	AFEMCLK_DELAY_16_ns		0x08
#define	AFEMCLK_DELAY_18_ns		0x09
#define	AFEMCLK_DELAY_20_ns		0x0A
/* bit[7:4]*/
#define	SDRAMCLK_DELAY_0_ns		0x00
#define	SDRAMCLK_DELAY_2_ns		0x10
#define	SDRAMCLK_DELAY_4_ns		0x20
#define	SDRAMCLK_DELAY_6_ns		0x30
#define	SDRAMCLK_DELAY_8_ns		0x40
#define	SDRAMCLK_DELAY_10_ns	0x50
#define	SDRAMCLK_DELAY_12_ns	0x60
#define	SDRAMCLK_DELAY_14_ns	0x70
#define	SDRAMCLK_DELAY_16_ns	0x80
#define	SDRAMCLK_DELAY_18_ns	0x90
#define	SDRAMCLK_DELAY_20_ns	0xA0

#define		ES01_82_AFE_ADCCLK_TIMING_ADJ_BYTE0		0x82
#define		ES01_83_AFE_ADCCLK_TIMING_ADJ_BYTE1		0x83
#define		ES01_84_AFE_ADCCLK_TIMING_ADJ_BYTE2		0x84
#define		ES01_85_AFE_ADCCLK_TIMING_ADJ_BYTE3		0x85
#define		ES01_86_DisableAllClockWhenIdle		0x86
			/* bit[0] */
#define		CLOSE_ALL_CLOCK_ENABLE	0x01
#define		CLOSE_ALL_CLOCK_DISABLE	0x00
#define		ES01_87_SDRAM_Timing				0x87

#define		ES01_88_LINE_ART_THRESHOLD_HIGH_VALUE	0x88
#define		ES01_89_LINE_ART_THRESHOLD_LOW_VALUE	0x89

#define ES01_8A_FixScanStepMSB			0x8a
#define ES01_8B_Status					0x8b
		/* bit[4:0] */
#define		H1H0L1L0_PS_MJ				0x00
#define		SCAN_STATE					0x01
#define		GPIO0_7						0x02
#define		GPIO8_15					0x03
#define		AVAILABLE_BANK_COUNT0_7		0x04
#define		AVAILABLE_BANK_COUNT8_15	0x05
#define		RAM_ADDRESS_POINTER0_7		0x06
#define		RAM_ADDRESS_POINTER8_15		0x07
#define		RAM_ADDRESS_POINTER16_19	0x08
#define		CARRIAGE_POS_DURING_SCAN0_7	0x09
#define		CARRIAGE_POS_DURING_SCAN8_15	0x0a
#define		CARRIAGE_POS_DURING_SCAN16_19	0x0b
#define		LINE_TIME0_7				0x0C
#define		LINE_TIME8_15				0x0d
#define		LINE_TIME16_19				0x0e
#define		LAST_COMMAND_ADDRESS		0x0f
#define		LAST_COMMAND_DATA			0x10
#define		SERIAL_READ_REGISTER_0		0x11
#define		SERIAL_READ_REGISTER_1		0x12
#define		SERIAL_READ_REGISTER_2		0x13
#define		SERIAL_READ_REGISTER_3		0x14
#define		MOTOR_STEP_TRIGER_POSITION7_0	0x15
#define		MOTOR_STEP_TRIGER_POSITION15_8	0x16
#define		MOTOR_STEP_TRIGER_POSITION23_16	0x17
#define		CHIP_STATUS_A				0x18	/*reserve */
#define		CHIP_STRING_0				0x19	/*0x45'E' */
#define		CHIP_STRING_1				0x1a	/*0x53'S' */
#define		CHIP_STRING_2				0x1b	/*0x43'C' */
#define		CHIP_STRING_3				0x1c	/*0x41'A' */
#define		CHIP_STRING_4				0x1d	/*0x4E'N' */
#define		CHIP_STRING_5				0x1e	/*0x30'0' */
#define		CHIP_STRING_6				0x1f	/*0x31'1' */
#define ES01_8C_RestartMotorSynPixelNumberM16LSB	0x8c
#define ES01_8D_RestartMotorSynPixelNumberM16MSB	0x8d
#define ES01_90_Lamp0PWM				0x90
#define ES01_91_Lamp1PWM				0x91
#define ES01_92_TimerPowerSaveTime		0x92
#define ES01_93_MotorWatchDogTime		0x93
#define ES01_94_PowerSaveControl		0x94
		/* bit[0] */
#define		TIMER_POWER_SAVE_ENABLE		0x01
#define		TIMER_POWER_SAVE_DISABLE	0x00

/* bit[1]*/
#define		USB_POWER_SAVE_ENABLE		0x02
#define		USB_POWER_SAVE_DISABLE		0x00
/* bit[2]*/
#define		USB_REMOTE_WAKEUP_ENABLE	0x04
#define		USB_REMOTE_WAKEUP_DISABLE	0x00
/* bit[5:4]*/
#define		LED_MODE_ON					0x00
#define		LED_MODE_OFF				0x10
#define		LED_MODE_FLASH_SLOWLY		0x20
#define		LED_MODE_FLASH_QUICKLY		0x30


#define ES01_95_GPIOValue0_7			0x95
#define ES01_96_GPIOValue8_15			0x96
#define ES01_97_GPIOControl0_7			0x97
#define ES01_98_GPIOControl8_15			0x98
#define	ES01_99_LAMP_PWM_FREQ_CONTROL	0x99
#define ES01_9A_AFEControl				0x9a
		/*bit[0] */
#define ADAFE_AFE		0x00
#define AD9826_AFE		0x01
/*bit[1]*/
#define AUTO_CHANGE_AFE_GAIN_OFFSET_ENABLE 0x02
#define AUTO_CHANGE_AFE_GAIN_OFFSET_DISABLE 0x00

#define	ES01_9B_ShadingTableAddrA14_A21		0x9B
#define ES01_9C_ShadingTableAddrODDA12_A19 0x9c
#define ES01_9D_MotorTableAddrA14_A21	0x9d
#define ES01_9E_HorizontalRatio1to15LSB 0x9e
#define ES01_9F_HorizontalRatio1to15MSB 0x9f
#define ES01_A0_HostStartAddr0_7		0xa0
#define ES01_A1_HostStartAddr8_15		0xa1
		/* bit[3] */
#define		ES01_ACCESS_PRE_GAMMA			0x08
#define		ES01_ACCESS_FINAL_GAMMA			0x00
#define ES01_A2_HostStartAddr16_21		0xa2
		/* bit[7] */
#define		ACCESS_DRAM	       			0x00
#define		ACCESS_GAMMA_RAM    		0x80
#define ES01_A3_HostEndAddr0_7			0xa3
#define ES01_A4_HostEndAddr8_15			0xa4
#define ES01_A5_HostEndAddr16_21		0xa5

#define		ES01_A6_MotorOption				0xA6
		/* bit[0] */
#define		MOTOR_0_ENABLE				0x01
#define		MOTOR_0_DISABLE				0x00
/*	bit[1]*/
#define		MOTOR_1_ENABLE				0x02
#define		MOTOR_1_DISABLE				0x00
/* bit[3:2]*/
#define		HOME_SENSOR_0_ENABLE		0x00
#define		HOME_SENSOR_1_ENABLE		0x04
#define		HOME_SENSOR_BOTH_ENABLE		0x08
#define		HOME_SENSOR_0_INVERT_ENABLE	0x0c
/* bit[7:4]*/
#define		ES03_UNIPOLAR_FULL_STEP_2003	0x00
#define		ES03_BIPOLAR_FULL_2916			0x10
#define		ES03_BIPOLAR_FULL_3955_3966		0x20
#define		ES03_UNIPOLAR_PWM_2003			0x30
#define		ES03_BIPOLAR_3967				0x40
#define		ES03_TABLE_DEFINE				0x50
#define ES01_A7_MotorPWMOnTimePhasA		0xa7
#define ES01_A8_MotorPWMOnTimePhasB		0xa8
#define ES01_A9_MotorPWMOffTimePhasA	0xa9
#define ES01_AA_MotorPWMOffTimePhasB	0xaa
#define ES01_AB_PWM_CURRENT_CONTROL		0xab

		/* bit[1:0] */
#define	MOTOR_PWM_CURRENT_0	0x00
#define	MOTOR_PWM_CURRENT_1	0x01
#define	MOTOR_PWM_CURRENT_2	0x02
#define	MOTOR_PWM_CURRENT_3	0x03
/* bit[3:2]*/
#define	MOTOR1_GPO_VALUE_0	0x00
#define	MOTOR1_GPO_VALUE_1	0x04
#define	MOTOR1_GPO_VALUE_2	0x08
#define	MOTOR1_GPO_VALUE_3	0x0c
/* bit[4]*/
#define	GPO_OUTPUT_ENABLE	0x10
#define	GPO_OUTPUT_DISABLE	0x00
/* bit[5]*/
#define	SERIAL_PORT_CONTINUOUS_OUTPUT_ENABLE	0x20
#define	SERIAL_PORT_CONTINUOUS_OUTPUT_DISABLE	0x00
#define ES01_AC_MotorPWMJamRangeLSB		0xac
#define ES01_AD_MotorPWMJamRangeMSB		0xad
#define ES01_AE_MotorSyncPixelNumberM16LSB	0xae
#define ES01_AF_MotorSyncPixelNumberM16MSB	0xaf
#define	ES01_B0_CCDPixelLSB					0xb0
#define	ES01_B1_CCDPixelMSB					0xb1
#define ES01_B2_PHTGPulseWidth				0xb2
#define ES01_B3_PHTGWaitWidth				0xb3
#define ES01_B4_StartPixelLSB				0xb4
#define ES01_B5_StartPixelMSB				0xb5
#define ES01_B6_LineWidthPixelLSB			0xb6
#define ES01_B7_LineWidthPixelMSB			0xb7

#define ES01_B8_ChannelRedExpStartPixelLSB		0xb8
#define ES01_B9_ChannelRedExpStartPixelMSB		0xb9
#define ES01_BA_ChannelRedExpEndPixelLSB		0xba
#define ES01_BB_ChannelRedExpEndPixelMSB		0xbb
#define ES01_BC_ChannelGreenExpStartPixelLSB	0xbc
#define ES01_BD_ChannelGreenExpStartPixelMSB	0xbd
#define ES01_BE_ChannelGreenExpEndPixelLSB		0xbe
#define ES01_BF_ChannelGreenExpEndPixelMSB		0xbf
#define ES01_C0_ChannelBlueExpStartPixelLSB		0xc0
#define ES01_C1_ChannelBlueExpStartPixelMSB		0xc1
#define ES01_C2_ChannelBlueExpEndPixelLSB		0xc2
#define ES01_C3_ChannelBlueExpEndPixelMSB		0xc3


#define ES01_C4_MultiTGTimesRed				0xc4
#define ES01_C5_MultiTGTimesGreen			0xc5
#define ES01_C6_MultiTGTimesBlue			0xc6
#define ES01_C7_MultiTGDummyPixelNumberLSB	0xc7
#define ES01_C8_MultiTGDummyPixelNumberMSB	0xc8
#define ES01_C9_CCDDummyPixelNumberLSB		0xc9
#define ES01_CA_CCDDummyPixelNumberMSB		0xca
#define ES01_CB_CCDDummyCycleNumber			0xcb
#define ES01_CC_PHTGTimingAdjust			0xcc
		/*      bit[0] */
#define		PHTG_INVERT_OUTPUT_ENABLE			0x01
#define		PHTG_INVERT_OUTPUT_DISABLE			0x00
/*	bit[1]*/
#define		TWO_TG								0x01
#define		MULTI_TG							0x00
		  /*    bit[3:2] */
#define		CCD_PIXEL_MODE_RED					0x0c
#define		CCD_LINE_MOE_RED_00					0x00
#define		CCD_LINE_MOE_RED_01					0x04
#define		CCD_LINE_MOE_RED_10					0x08
/*	bit[5:4]*/
#define		CCD_PIXEL_MODE_GREEN				0x30
#define		CCD_LINE_MOE_GREEN_00				0x00
#define		CCD_LINE_MOE_GREEN_01				0x40
#define		CCD_LINE_MOE_GREEN_10				0x80
/*	bit[7:6]*/
#define		CCD_PIXEL_MODE_BLUE					0xc0
#define		CCD_LINE_MOE_BLUE_00				0x00
#define		CCD_LINE_MOE_BLUE_01				0x40
#define		CCD_LINE_MOE_BLUE_10				0x80

#define		ES01_CD_TG_R_CONTROL					0xCD
#define		ES01_CE_TG_G_CONTROL					0xCE
#define		ES01_CF_TG_B_CONTROL					0xCF

#define		ES01_D9_CLEAR_PULSE_WIDTH				0xD9
#define		ES01_DA_CLEAR_SIGNAL_INVERTING_OUTPUT	0xDA
#define		ES01_DB_PH_RESET_EDGE_TIMING_ADJUST		0xDB
#define		ES01_DC_CLEAR_EDGE_TO_PH_TG_EDGE_WIDTH	0xDC
#define		ES01_D0_PH1_0						0xD0
#define		ES01_D1_PH2_0						0xD1
#define		ES01_D2_PH1B_0						0xD2
#define		ES01_D4_PHRS_0						0xD4
#define		ES01_D5_PHCP_0						0xD5
#define		ES01_D6_AFE_VSAMP_0					0xD6
#define		ES01_D7_AFE_RSAMP_0					0xD7

#define		ES01_D8_PHTG_EDGE_TIMING_ADJUST		0xD8

#define ES01_CD_PH1_0						0xcd
#define ES01_CE_PH1_1						0xce
#define ES01_CF_PH2_0						0xcf
#define ES01_D0_PH2_1						0xd0
#define ES01_D1_PH1B_0						0xd1
#define ES01_D2_PH1B_1						0xd2
#define ES01_D3_PH2B_0						0xd3
#define ES01_D4_PH2B_1						0xd4
#define ES01_D5_PHRS_0						0xd5
#define ES01_D6_PHRS_1						0xd6
#define ES01_D7_PHCP_0						0xd7
#define ES01_D8_PHCP_1						0xd8
#define ES01_D9_AFE_VSAMP_0					0xd9
#define ES01_DA_AFE_VSAMP_1					0xda
#define ES01_DB_AFE_RSAMP_0					0xdb
#define ES01_DC_AFE_RSAMP_1					0xdc
#define ES01_DD_PH1234_IN_DUMMY_TG			0xdd
#define		ES01_DE_CCD_SETUP_REGISTER				0xDE
		/* bit[0] */
#define		ES01_LINE_SCAN_MODE_DISABLE		0x00
#define		ES01_LINE_SCAN_MODE_ENABLE		0x01
		/* bit[1] */
#define		ES01_CIS_SENSOR_MODE_DISABLE	0x00
#define		ES01_CIS_SENSOR_MODE_ENABLE		0x02
		/* bit[2] */
#define		ES01_CIS_LED_OUTPUT_RGB			0x00
#define		ES01_CIS_LED_OUTPUT_RtoGtoB		0x04
		/* bit[3] */
#define		ES01_CIS_LED_NORMAL_OUTPUT		0x00
#define		ES01_CIS_LED_INVERT_OUTPUT		0x08
		/* bit[4] */
#define		ES01_ACC_IN_IDLE_DISABLE		0x00
#define		ES01_ACC_IN_IDLE_ENABLE			0x10
		/* bit[5] */
#define		ES01_EVEN_ODD_DISABLE			0x00
#define		ES01_EVEN_ODD_ENABLE			0x20
		/* bit[6] */
#define		ES01_ALTERNATE_EVEN_ODD_DISABLE	0x00
#define		ES01_ALTERNATE_EVEN_ODD_ENABLE	0x40
		/* bit[7] */
#define		ES01_RESET_CCD_STATE_DISABLE	0x00
#define		ES01_RESET_CCD_STATE_ENABLE		0x80

#define ES01_DF_ICG_CONTROL					0xdf
		/*      bit[2:0] */
#define	BEFORE_PHRS_0_ns		0x00
#define	BEFORE_PHRS_416_7t_ns	0x01
#define	BEFORE_PHRS_416_6t_ns	0x02
#define	BEFORE_PHRS_416_5t_ns	0x03
#define	BEFORE_PHRS_416_4t_ns	0x04
#define	BEFORE_PHRS_416_3t_ns	0x05
#define	BEFORE_PHRS_416_2t_ns	0x06
#define	BEFORE_PHRS_416_1t_ns	0x07
/*	bit[2:0]*/
#define	ICG_UNIT_1_PIXEL_TIME	0x00
#define	ICG_UNIT_4_PIXEL_TIME	0x10
#define	ICG_UNIT_8_PIXEL_TIME	0x20
#define	ICG_UNIT_16_PIXEL_TIME	0x30
#define	ICG_UNIT_32_PIXEL_TIME	0x40
#define	ICG_UNIT_64_PIXEL_TIME	0x50
#define	ICG_UNIT_128_PIXEL_TIME	0x60
#define	ICG_UNIT_256_PIXEL_TIME	0x70

#define ES01_E0_MotorAccStep0_7				0xe0
#define ES01_E1_MotorAccStep8_8				0xe1
#define ES01_E2_MotorStepOfMaxSpeed0_7		0xe2
#define ES01_E3_MotorStepOfMaxSpeed8_15		0xe3
#define ES01_E4_MotorStepOfMaxSpeed16_19	0xe4
#define ES01_E5_MotorDecStep				0xe5
#define ES01_E6_ScanBackTrackingStepLSB		0xe6
#define ES01_E7_ScanBackTrackingStepMSB		0xe7
#define ES01_E8_ScanRestartStepLSB			0xe8
#define ES01_E9_ScanRestartStepMSB			0xe9
#define ES01_EA_ScanBackHomeExtStepLSB		0xea
#define ES01_EB_ScanBackHomeExtStepMSB		0xeb
#define ES01_EC_ScanAccStep0_7				0xec
#define ES01_ED_ScanAccStep8_8				0xed
#define ES01_EE_FixScanStepLSB				0xee
#define ES01_EF_ScanDecStep					0xef
#define ES01_F0_ScanImageStep0_7			0xf0
#define ES01_F1_ScanImageStep8_15			0xf1
#define ES01_F2_ScanImageStep16_19			0xf2

#define ES01_F3_ActionOption				0xf3
		/*      bit[0] */
#define		MOTOR_MOVE_TO_FIRST_LINE_ENABLE			0x01
#define		MOTOR_MOVE_TO_FIRST_LINE_DISABLE		0x00
/*	bit[1]*/
#define		MOTOR_BACK_HOME_AFTER_SCAN_ENABLE		0x02
#define		MOTOR_BACK_HOME_AFTER_SCAN_DISABLE		0x00
/*	bit[2]*/
#define		SCAN_ENABLE								0x04
#define		SCAN_DISABLE							0x00
/*	bit[3]*/
#define		SCAN_BACK_TRACKING_ENABLE				0x08
#define		SCAN_BACK_TRACKING_DISABLE				0x00
/*	bit[4]*/
#define		INVERT_MOTOR_DIRECTION_ENABLE			0x10
#define		INVERT_MOTOR_DIRECTION_DISABLE			0x00
/*	bit[5]*/
#define		UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE		0x20
#define		UNIFORM_MOTOR_AND_SCAN_SPEED_DISABLE	0x00

	 /* bit[6] */
#define     ES01_STATIC_SCAN_ENABLE                 0x40
#define     ES01_STATIC_SCAN_DISABLE                0x00

/*	bit[7]*/
#define		MOTOR_TEST_LOOP_ENABLE					0x80
#define		MOTOR_TEST_LOOP_DISABLE					0x00
#define ES01_F4_ActiveTriger				0xf4
		/* bit[0] */
#define		ACTION_TRIGER_ENABLE			0x01
#define		ACTION_TRIGER_DISABLE			0x00

#define		ES01_F5_ScanDataFormat				0xf5
			/* bit[0] */
#define		COLOR_ES02							0x00
#define		GRAY_ES02							0x01
/* bit[2:1]*/
#define		_8_BITS_ES02						0x00
#define		_16_BITS_ES02						0x02
#define		_1_BIT_ES02							0x04
/* bit[5:4]*/
#define		GRAY_RED_ES02						0x00
#define		GRAY_GREEN_ES02						0x10
#define		GRAY_BLUE_ES02						0x20
#define		GRAY_GREEN_BLUE_ES02				0x30

#define ES01_F6_MorotControl1				0xf6
		/* bit[2:0] */
#define		SPEED_UNIT_1_PIXEL_TIME			0x00
#define		SPEED_UNIT_4_PIXEL_TIME			0x01
#define		SPEED_UNIT_8_PIXEL_TIME			0x02
#define		SPEED_UNIT_16_PIXEL_TIME		0x03
#define		SPEED_UNIT_32_PIXEL_TIME		0x04
#define		SPEED_UNIT_64_PIXEL_TIME		0x05
#define		SPEED_UNIT_128_PIXEL_TIME		0x06
#define		SPEED_UNIT_256_PIXEL_TIME		0x07
/* bit[5:4]*/
#define		MOTOR_SYNC_UNIT_1_PIXEL_TIME		0x00
#define		MOTOR_SYNC_UNIT_16_PIXEL_TIME		0x10
#define		MOTOR_SYNC_UNIT_64_PIXEL_TIME		0x20
#define		MOTOR_SYNC_UNIT_256_PIXEL_TIME		0x30
#define ES01_F7_DigitalControl							0xf7
		/* bit[0] */
#define		DIGITAL_REDUCE_ENABLE	0x01
#define		DIGITAL_REDUCE_DISABLE	0x00
/* bit[3:1]*/
#define		DIGITAL_REDUCE_1_1		0x00
#define		DIGITAL_REDUCE_1_2		0x02
#define		DIGITAL_REDUCE_1_4		0x04
#define		DIGITAL_REDUCE_1_8		0x06
#define		DIGITAL_REDUCE_1_16		0x08

#define		ES01_F8_WHITE_SHADING_DATA_FORMAT		0xF8
			/* bit[1:0] */
#define		ES01_SHADING_2_INT_14_DEC		0x00
#define		ES01_SHADING_3_INT_13_DEC		0x01
#define		ES01_SHADING_4_INT_12_DEC		0x02
#define		ES01_SHADING_5_INT_11_DEC		0x03
#define ES01_F9_BufferFullSize16WordLSB		0xf9
#define ES01_FA_BufferFullSize16WordMSB		0xfa
#define ES01_FB_BufferEmptySize16WordLSB	0xfb
#define ES01_FC_BufferEmptySize16WordMSB	0xfc
#define ES01_FD_MotorFixedspeedLSB			0xfd
#define ES01_FE_MotorFixedspeedMSB			0xfe
#define		ES01_FF_TestControl				0xff
		/* bit[0] */
#define		OUTPUT_HORIZONTAL_PATTERN_ENABLE	0x01
#define		OUTPUT_HORIZONTAL_PATTERN_DISABLE	0x00
/* bit[1]*/
#define		OUTPUT_VERTICAL_PATTERN_ENABLE	0x02
#define		OUTPUT_VERTICAL_PATTERN_DISABLE	0x00
/* bit[2]*/
#define		BYPASS_DARK_SHADING_ENABLE	0x04
#define		BYPASS_DARK_SHADING_DISABLE	0x00
/* bit[3]*/
#define		BYPASS_WHITE_SHADING_ENABLE		0x08
#define		BYPASS_WHITE_SHADING_DISABLE	0x00
/* bit[4]*/
#define		BYPASS_PRE_GAMMA_ENABLE		0x10
#define		BYPASS_PRE_GAMMA_DISABLE	0x00
/* bit[5]*/
#define		BYPASS_CONVOLUTION_ENABLE	0x20
#define		BYPASS_CONVOLUTION_DISABLE	0x00
/* bit[6]*/
#define		BYPASS_MATRIX_ENABLE	0x40
#define		BYPASS_MATRIX_DISABLE	0x00
/* bit[7]*/
#define		BYPASS_GAMMA_ENABLE		0x80
#define		BYPASS_GAMMA_DISABLE	0x00
#define		ES01_FF_SCAN_IMAGE_OPTION				0xFF
		/* bit[0] */
#define		OUTPUT_HORIZONTAL_PATTERN_ENABLE	0x01
#define		OUTPUT_HORIZONTAL_PATTERN_DISABLE	0x00
/* bit[1]*/
#define		OUTPUT_VERTICAL_PATTERN_ENABLE	0x02
#define		OUTPUT_VERTICAL_PATTERN_DISABLE	0x00
/* bit[2]*/
#define		BYPASS_DARK_SHADING_ENABLE	0x04
#define		BYPASS_DARK_SHADING_DISABLE	0x00
/* bit[3]*/
#define		BYPASS_WHITE_SHADING_ENABLE		0x08
#define		BYPASS_WHITE_SHADING_DISABLE	0x00
/* bit[4]*/
#define		BYPASS_PRE_GAMMA_ENABLE		0x10
#define		BYPASS_PRE_GAMMA_DISABLE	0x00
/* bit[5]*/
#define		BYPASS_CONVOLUTION_ENABLE	0x20
#define		BYPASS_CONVOLUTION_DISABLE	0x00
/* bit[6]*/
#define		BYPASS_MATRIX_ENABLE	0x40
#define		BYPASS_MATRIX_DISABLE	0x00
/* bit[7]*/
#define		BYPASS_GAMMA_ENABLE		0x80
#define		BYPASS_GAMMA_DISABLE	0x00

/*******************************************************************/
#define		ES01_160_CHANNEL_A_LATCH_POSITION_HB	0x160
#define		ES01_161_CHANNEL_A_LATCH_POSITION_LB	0x161
#define		ES01_162_CHANNEL_B_LATCH_POSITION_HB	0x162
#define		ES01_163_CHANNEL_B_LATCH_POSITION_LB	0x163
#define		ES01_164_CHANNEL_C_LATCH_POSITION_HB	0x164
#define		ES01_165_CHANNEL_C_LATCH_POSITION_LB	0x165
#define		ES01_166_CHANNEL_D_LATCH_POSITION_HB	0x166
#define		ES01_167_CHANNEL_D_LATCH_POSITION_LB	0x167

#define		ES01_168_SECONDARY_FF_LATCH_POSITION	0x168

#define		ES01_169_NUMBER_OF_SEGMENT_PIXEL_LB		0x169
#define		ES01_16A_NUMBER_OF_SEGMENT_PIXEL_HB		0x16A

#define		ES01_16B_BETWEEN_SEGMENT_INVALID_PIXEL	0x16B
#define		ES01_16C_LINE_SHIFT_OUT_TIMES_DIRECTION	0x16C	/* bit[3:0] */

/* segment start address */
#define		ES01_16D_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE0	0x16D
#define		ES01_16E_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE1	0x16E
#define		ES01_16F_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE2	0x16F	/* bit[3:0] */

#define		ES01_170_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE0	0x170
#define		ES01_171_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE1	0x171
#define		ES01_172_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE2	0x172	/* bit[3:0] */

#define		ES01_173_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE0	0x173
#define		ES01_174_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE1	0x174
#define		ES01_175_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE2	0x175	/* bit[3:0] */

#define		ES01_176_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE0	0x176
#define		ES01_177_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE1	0x177
#define		ES01_178_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE2	0x178	/* bit[3:0] */

#define		ES01_179_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE0	0x179
#define		ES01_17A_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE1	0x17A
#define		ES01_17B_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE2	0x17B	/* bit[3:0] */

#define		ES01_17C_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE0	0x17C
#define		ES01_17D_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE1	0x17D
#define		ES01_17E_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE2	0x17E	/* bit[3:0] */

#define		ES01_17F_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE0	0x17F
#define		ES01_180_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE1	0x180
#define		ES01_181_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE2	0x181	/* bit[3:0] */

#define		ES01_182_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE0	0x182
#define		ES01_183_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE1	0x183
#define		ES01_184_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE2	0x184	/* bit[3:0] */

#define		ES01_185_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE0	0x185
#define		ES01_186_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE1	0x186
#define		ES01_187_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE2	0x187	/* bit[3:0] */

#define		ES01_188_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE0	0x188
#define		ES01_189_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE1	0x189
#define		ES01_18A_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE2	0x18A	/* bit[3:0] */

#define		ES01_18B_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE0	0x18B
#define		ES01_18C_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE1	0x18C
#define		ES01_18D_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE2	0x18D	/* bit[3:0] */

#define		ES01_18E_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE0	0x18E
#define		ES01_18F_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE1	0x18F
#define		ES01_190_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE2	0x190	/* bit[3:0] */

/* channel gap */
#define		ES01_191_CHANNEL_GAP1_BYTE0	0x191
#define		ES01_192_CHANNEL_GAP1_BYTE1	0x192
#define		ES01_193_CHANNEL_GAP1_BYTE2	0x193	/* bit[3:0] */

#define		ES01_194_CHANNEL_GAP2_BYTE0	0x194
#define		ES01_195_CHANNEL_GAP2_BYTE1	0x195
#define		ES01_196_CHANNEL_GAP2_BYTE2	0x196	/* bit[3:0] */

#define		ES01_197_CHANNEL_GAP3_BYTE0	0x197
#define		ES01_198_CHANNEL_GAP3_BYTE1	0x198
#define		ES01_199_CHANNEL_GAP3_BYTE2	0x199	/* bit[3:0] */

/* channel line gap */
#define		ES01_19A_CHANNEL_LINE_GAP_LB	0x19A
#define		ES01_19B_CHANNEL_LINE_GAP_HB	0x19B

/* max pack line */
#define		ES01_19C_MAX_PACK_LINE	0x19C	/* bit[5:0] */

/*pack threshold */
#define		ES01_19D_PACK_THRESHOLD_LINE	0x19D

/* pack area start address */
#define		ES01_19E_PACK_AREA_R_START_ADDR_BYTE0	0x19E
#define		ES01_19F_PACK_AREA_R_START_ADDR_BYTE1	0x19F
#define		ES01_1A0_PACK_AREA_R_START_ADDR_BYTE2	0x1A0	/* bit[3:0] */

#define		ES01_1A1_PACK_AREA_G_START_ADDR_BYTE0	0x1A1
#define		ES01_1A2_PACK_AREA_G_START_ADDR_BYTE1	0x1A2
#define		ES01_1A3_PACK_AREA_G_START_ADDR_BYTE2	0x1A3	/* bit[3:0] */

#define		ES01_1A4_PACK_AREA_B_START_ADDR_BYTE0	0x1A4
#define		ES01_1A5_PACK_AREA_B_START_ADDR_BYTE1	0x1A5
#define		ES01_1A6_PACK_AREA_B_START_ADDR_BYTE2	0x1A6	/* bit[3:0] */

/* pack area end address */
#define		ES01_1A7_PACK_AREA_R_END_ADDR_BYTE0	0x1A7
#define		ES01_1A8_PACK_AREA_R_END_ADDR_BYTE1	0x1A8
#define		ES01_1A9_PACK_AREA_R_END_ADDR_BYTE2	0x1A9	/* bit[3:0] */

#define		ES01_1AA_PACK_AREA_G_END_ADDR_BYTE0	0x1AA
#define		ES01_1AB_PACK_AREA_G_END_ADDR_BYTE1	0x1AB
#define		ES01_1AC_PACK_AREA_G_END_ADDR_BYTE2	0x1AC	/* bit[3:0] */

#define		ES01_1AD_PACK_AREA_B_END_ADDR_BYTE0	0x1AD
#define		ES01_1AE_PACK_AREA_B_END_ADDR_BYTE1	0x1AE
#define		ES01_1AF_PACK_AREA_B_END_ADDR_BYTE2	0x1AF	/* bit[3:0] */

/* segment pixel number */
#define		ES01_1B0_SEGMENT_PIXEL_NUMBER_LB	0x1B0
#define		ES01_1B1_SEGMENT_PIXEL_NUMBER_HB	0x1B1

/*overlap pixel number and hold pixel number */
#define		ES01_1B2_OVERLAP_AND_HOLD_PIXEL_NUMBER	0x1B2

/*convolution parameter */
#define		ES01_1B3_CONVOLUTION_A	0x1B3
#define		ES01_1B4_CONVOLUTION_B	0x1B4
#define		ES01_1B5_CONVOLUTION_C	0x1B5
#define		ES01_1B6_CONVOLUTION_D	0x1B6
#define		ES01_1B7_CONVOLUTION_E	0x1B7
#define		ES01_1B8_CONVOLUTION_F	0x1B8	/* bit[2:0] */

/* line pixel number */
#define		ES01_1B9_LINE_PIXEL_NUMBER_LB	0x1B9
#define		ES01_1BA_LINE_PIXEL_NUMBER_HB	0x1BA

/* matrix parameter */
#define		ES01_1BB_MATRIX_A_LB	0x1BB
#define		ES01_1BC_MATRIX_A_HB	0x1BC	/* bit[3:0] */

#define		ES01_1BD_MATRIX_B_LB	0x1BD
#define		ES01_1BE_MATRIX_B_HB	0x1BE	/* bit[3:0] */

#define		ES01_1BF_MATRIX_C_LB	0x1BF
#define		ES01_1C0_MATRIX_C_HB	0x1C0	/* bit[3:0] */

#define		ES01_1C1_MATRIX_D_LB	0x1C1
#define		ES01_1C2_MATRIX_D_HB	0x1C2	/* bit[3:0] */

#define		ES01_1C3_MATRIX_E_LB	0x1C3
#define		ES01_1C4_MATRIX_E_HB	0x1C4	/* bit[3:0] */

#define		ES01_1C5_MATRIX_F_LB	0x1C5
#define		ES01_1C6_MATRIX_F_HB	0x1C6	/* bit[3:0] */

#define		ES01_1C7_MATRIX_G_LB	0x1C7
#define		ES01_1C8_MATRIX_G_HB	0x1C8	/* bit[3:0] */

#define		ES01_1C9_MATRIX_H_LB	0x1C9
#define		ES01_1CA_MATRIX_H_HB	0x1CA	/* bit[3:0] */

#define		ES01_1CB_MATRIX_I_LB	0x1CB
#define		ES01_1CC_MATRIX_I_HB	0x1CC	/* bit[3:0] */

/*dummy clock number */
#define		ES01_1CD_DUMMY_CLOCK_NUMBER	0x1CD	/* bit[3:0] */

/* line segment number */
#define		ES01_1CE_LINE_SEGMENT_NUMBER 0x1CE

/* dummy cycle timing */
#define		ES01_1D0_DUMMY_CYCLE_TIMING_B0	0x1D0
#define		ES01_1D1_DUMMY_CYCLE_TIMING_B1	0x1D1
#define		ES01_1D2_DUMMY_CYCLE_TIMING_B2	0x1D2
#define		ES01_1D3_DUMMY_CYCLE_TIMING_B3	0x1D3

/* PH1 timing adjust register */
#define		ES01_1D4_PH1_TIMING_ADJ_B0	0x1D4
#define		ES01_1D5_PH1_TIMING_ADJ_B1	0x1D5
#define		ES01_1D6_PH1_TIMING_ADJ_B2	0x1D6
#define		ES01_1D7_PH1_TIMING_ADJ_B3	0x1D7

/* PH2 timing adjust register */
#define		ES01_1D8_PH2_TIMING_ADJ_B0	0x1D8
#define		ES01_1D9_PH2_TIMING_ADJ_B1	0x1D9
#define		ES01_1DA_PH2_TIMING_ADJ_B2	0x1DA
#define		ES01_1DB_PH2_TIMING_ADJ_B3	0x1DB

/* PH3 timing adjust register */
#define		ES01_1DC_PH3_TIMING_ADJ_B0	0x1DC
#define		ES01_1DD_PH3_TIMING_ADJ_B1	0x1DD
#define		ES01_1DE_PH3_TIMING_ADJ_B2	0x1DE
#define		ES01_1DF_PH3_TIMING_ADJ_B3	0x1DF

/* PH4 timing adjust register */
#define		ES01_1E0_PH4_TIMING_ADJ_B0	0x1E0
#define		ES01_1E1_PH4_TIMING_ADJ_B1	0x1E1
#define		ES01_1E2_PH4_TIMING_ADJ_B2	0x1E2
#define		ES01_1E3_PH4_TIMING_ADJ_B3	0x1E3

/*PHRS timing adjust register */
#define		ES01_1E4_PHRS_TIMING_ADJ_B0	0x1E4
#define		ES01_1E5_PHRS_TIMING_ADJ_B1	0x1E5
#define		ES01_1E6_PHRS_TIMING_ADJ_B2	0x1E6
#define		ES01_1E7_PHRS_TIMING_ADJ_B3	0x1E7

/* PHCP timing adjust register */
#define		ES01_1E8_PHCP_TIMING_ADJ_B0	0x1E8
#define		ES01_1E9_PHCP_TIMING_ADJ_B1	0x1E9
#define		ES01_1EA_PHCP_TIMING_ADJ_B2	0x1EA
#define		ES01_1EB_PHCP_TIMING_ADJ_B3	0x1EB

/*AFEVS timing adjust register */
#define		ES01_1EC_AFEVS_TIMING_ADJ_B0	0x1EC
#define		ES01_1ED_AFEVS_TIMING_ADJ_B1	0x1ED
#define		ES01_1EE_AFEVS_TIMING_ADJ_B2	0x1EE
#define		ES01_1EF_AFEVS_TIMING_ADJ_B3	0x1EF

/*AFERS timing adjust register */
#define		ES01_1F0_AFERS_TIMING_ADJ_B0	0x1F0
#define		ES01_1F1_AFERS_TIMING_ADJ_B1	0x1F1
#define		ES01_1F2_AFERS_TIMING_ADJ_B2	0x1F2
#define		ES01_1F3_AFERS_TIMING_ADJ_B3	0x1F3

/* read out pixel */
#define		ES01_1F4_START_READ_OUT_PIXEL_LB	0x1F4
#define		ES01_1F5_START_READ_OUT_PIXEL_HB	0x1F5
#define		ES01_1F6_READ_OUT_PIXEL_LENGTH_LB	0x1F6
#define		ES01_1F7_READ_OUT_PIXEL_LENGTH_HB	0x1F7

/* pack channel setting */
#define		ES01_1F8_PACK_CHANNEL_SELECT_B0	0x1F8
#define		ES01_1F9_PACK_CHANNEL_SELECT_B1	0x1F9
#define		ES01_1FA_PACK_CHANNEL_SELECT_B2	0x1FA
#define		ES01_1FB_PACK_CHANNEL_SIZE_B0	0x1FB
#define		ES01_1FC_PACK_CHANNEL_SIZE_B1	0x1FC
#define		ES01_1FD_PACK_CHANNEL_SIZE_B2	0x1FD

/* AFE gain offset control*/
  /* rom code ver 0.10  */
#define		ES01_2A0_AFE_GAIN_OFFSET_CONTROL	0x2A0
#define		ES01_2A1_AFE_AUTO_CONFIG_GAIN		0x2A1
#define		ES01_2A2_AFE_AUTO_CONFIG_OFFSET		0x2A2

#define		ES01_2B0_SEGMENT0_OVERLAP_SEGMENT1	0x2B0
#define		ES01_2B1_SEGMENT1_OVERLAP_SEGMENT2	0x2B1
#define		ES01_2B2_SEGMENT2_OVERLAP_SEGMENT3	0x2B2

/* valid pixel parameter */
#define		ES01_2C0_VALID_PIXEL_PARAMETER_OF_SEGMENT1	0x2C0
#define		ES01_2C1_VALID_PIXEL_PARAMETER_OF_SEGMENT2	0x2C1
#define		ES01_2C2_VALID_PIXEL_PARAMETER_OF_SEGMENT3	0x2C2
#define		ES01_2C3_VALID_PIXEL_PARAMETER_OF_SEGMENT4	0x2C3
#define		ES01_2C4_VALID_PIXEL_PARAMETER_OF_SEGMENT5	0x2C4
#define		ES01_2C5_VALID_PIXEL_PARAMETER_OF_SEGMENT6	0x2C5
#define		ES01_2C6_VALID_PIXEL_PARAMETER_OF_SEGMENT7	0x2C6
#define		ES01_2C7_VALID_PIXEL_PARAMETER_OF_SEGMENT8	0x2C7
#define		ES01_2C8_VALID_PIXEL_PARAMETER_OF_SEGMENT9	0x2C8
#define		ES01_2C9_VALID_PIXEL_PARAMETER_OF_SEGMENT10	0x2C9
#define		ES01_2CA_VALID_PIXEL_PARAMETER_OF_SEGMENT11	0x2CA
#define		ES01_2CB_VALID_PIXEL_PARAMETER_OF_SEGMENT12	0x2CB
#define		ES01_2CC_VALID_PIXEL_PARAMETER_OF_SEGMENT13	0x2CC
#define		ES01_2CD_VALID_PIXEL_PARAMETER_OF_SEGMENT14	0x2CD
#define		ES01_2CE_VALID_PIXEL_PARAMETER_OF_SEGMENT15	0x2CE
#define		ES01_2CF_VALID_PIXEL_PARAMETER_OF_SEGMENT16	0x2CF

/* forward declarations */
static STATUS OpenScanChip (PAsic chip);
static STATUS CloseScanChip (PAsic chip);
static STATUS SafeInitialChip (PAsic chip);
static STATUS DRAM_Test (PAsic chip);
#if SANE_UNUSED
static STATUS SetPowerSave (PAsic chip);
#endif
static STATUS SetLineTimeAndExposure (PAsic chip);
static STATUS CCDTiming (PAsic chip);
static STATUS IsCarriageHome (PAsic chip, SANE_Bool * LampHome, SANE_Bool * TAHome);
static STATUS InitTiming (PAsic chip);
static STATUS GetChipStatus (PAsic chip, SANE_Byte Selector, SANE_Byte * ChipStatus);
static STATUS SetAFEGainOffset (PAsic chip);
static STATUS SetLEDTime (PAsic chip);
static STATUS SetScanMode (PAsic chip, SANE_Byte bScanBits);
static STATUS SetPackAddress (PAsic chip, unsigned short wXResolution,
			      unsigned short wWidth, unsigned short wX, double XRatioAdderDouble,
			      double XRatioTypeDouble,
			      SANE_Byte byClear_Pulse_Width,
			      unsigned short * PValidPixelNumber);
static STATUS SetExtraSetting (PAsic chip, unsigned short wXResolution,
			       unsigned short wCCD_PixelNumber, SANE_Bool isCaribrate);


/* Forward declarations */

static STATUS Mustek_SendData (PAsic chip, unsigned short reg, SANE_Byte data);
static STATUS Mustek_SendData2Byte (PAsic chip, unsigned short reg, SANE_Byte data);
static STATUS Mustek_ReceiveData (PAsic chip, SANE_Byte * reg);
static STATUS Mustek_WriteAddressLineForRegister (PAsic chip, SANE_Byte x);
static STATUS WriteIOControl (PAsic chip, unsigned short wValue, unsigned short wIndex,
			      unsigned short wLength, SANE_Byte * lpbuf);
static STATUS ReadIOControl (PAsic chip, unsigned short wValue, unsigned short wIndex,
			     unsigned short wLength, SANE_Byte * lpbuf);
static STATUS Mustek_DMARead (PAsic chip, unsigned int size, SANE_Byte * lpdata);
static STATUS Mustek_DMAWrite (PAsic chip, unsigned int size, SANE_Byte * lpdata);
static STATUS Mustek_ClearFIFO (PAsic chip);
static STATUS SetRWSize (PAsic chip, SANE_Byte ReadWrite, unsigned int size);

/* Open Scanner by Scanner Name and return Chip Information */
static STATUS Asic_Open (PAsic chip, SANE_Byte *pDeviceName);
/* Close Scanner */
static STATUS Asic_Close (PAsic chip);
#if SANE_UNUSED
/* Release Scanner Resource */
static STATUS Asic_Release (PAsic chip);
#endif
/* Initialize Scanner Parameters */
static STATUS Asic_Initialize (PAsic chip);
/* Set Scan Window */
static STATUS Asic_SetWindow (PAsic chip, SANE_Byte bScanBits,
			      unsigned short wXResolution, unsigned short wYResolution,
			      unsigned short wX, unsigned short wY, unsigned short wWidth, unsigned short wLength);
/* Turn Lamp  ON or OFF */
static STATUS Asic_TurnLamp (PAsic chip, SANE_Bool isLampOn);
/* Turn TA ON or OFF */
static STATUS Asic_TurnTA (PAsic chip, SANE_Bool isTAOn);
/* Reset some parameter of asic */
static STATUS Asic_Reset (PAsic chip);
/* Set scan source */
static STATUS Asic_SetSource (PAsic chip, LIGHTSOURCE lsLightSource);
/* Start scanner to scan */
static STATUS Asic_ScanStart (PAsic chip);
/* Stop scanner to scan */
static STATUS Asic_ScanStop (PAsic chip);
/* Read One Scan Line When Scanning */
static STATUS Asic_ReadImage (PAsic chip, SANE_Byte * pBuffer, unsigned short LinesCount);
#if SANE_UNUSED
/* To Check Hard Key */
static STATUS Asic_CheckFunctionKey (PAsic chip, SANE_Byte * key);
#endif
/* To Check if TA id connected */
static STATUS Asic_IsTAConnected (PAsic chip, SANE_Bool *hasTA);
#if SANE_UNUSED
/* Download GammaTable to Scanner */
static STATUS Asic_DownloadGammaTable (PAsic chip, void * lpBuffer);
#endif
/* For AdjustAD Calculate Scanner*/
static STATUS Asic_ReadCalibrationData (PAsic chip, void * pBuffer,
					unsigned int dwXferBytes, SANE_Byte bScanBits);
/* Set motor move or not */
static STATUS Asic_SetMotorType (PAsic chip, SANE_Bool isMotorMove, SANE_Bool isUniformSpeed);
/* Move Motor Forward or Backword */
static STATUS Asic_MotorMove (PAsic chip, SANE_Bool isForward, unsigned int dwTotalSteps);
/* Move Motor to Home. */
/* If isTA is TRUE, move TA to home, else move Lamp to home */
static STATUS Asic_CarriageHome (PAsic chip, SANE_Bool isTA);
/* For ShadingTable */
static STATUS Asic_SetShadingTable (PAsic chip, unsigned short * lpWhiteShading,
				    unsigned short * lpDarkShading,
				    unsigned short wXResolution, unsigned short wWidth, unsigned short wX);
/* Wait motor move to home. isTA no used */
static STATUS Asic_WaitCarriageHome (PAsic chip, SANE_Bool isTA);
/* Wait until asic idle */
static STATUS Asic_WaitUnitReady (PAsic chip);
/* Set Scan Parameter to Scanner */
static STATUS Asic_SetCalibrate (PAsic chip, SANE_Byte bScanBits, unsigned short wXResolution,
				 unsigned short wYResolution, unsigned short wX, unsigned short wY,
				 unsigned short wWidth, unsigned short wLength, SANE_Bool isShading);
/* Set AFE  Parameter to Scanner */
static STATUS Asic_SetAFEGainOffset (PAsic chip);

/* ---------------------- asic motor defines -------------------------- */


#define ACTION_MODE_ACCDEC_MOVE 0
#define ACTION_MODE_UNIFORM_SPEED_MOVE 1


typedef struct tagMOTOR_CURRENT_AND_PHASE
{
  SANE_Byte MoveType;
  SANE_Byte FillPhase;
  SANE_Byte MotorDriverIs3967;
  SANE_Byte MotorCurrentTableA[32];
  SANE_Byte MotorCurrentTableB[32];
  SANE_Byte MotorPhaseTable[32];
} MOTOR_CURRENT_AND_PHASE, LPMOTOR_CURRENT_AND_PHASE;

typedef struct tagLLF_RAMACCESS
{
  SANE_Byte ReadWrite;
  SANE_Byte IsOnChipGamma;
  unsigned short LoStartAddress;
  unsigned short HiStartAddress;
  int RwSize;
  SANE_Byte DramDelayTime;
  SANE_Byte *BufferPtr;
} LLF_RAMACCESS;

typedef struct tagLLF_MOTOR_CURRENT_AND_PHASE
{
  SANE_Byte MoveType;
  SANE_Byte FillPhase;
  SANE_Byte MotorDriverIs3967;
  SANE_Byte MotorCurrentTableA[32];
  SANE_Byte MotorCurrentTableB[32];
  SANE_Byte MotorPhaseTable[32];
} LLF_MOTOR_CURRENT_AND_PHASE;

typedef struct tagLLF_CALCULATEMOTORTABLE
{
  unsigned short StartSpeed;
  unsigned short EndSpeed;
  unsigned short AccStepBeforeScan;
  SANE_Byte DecStepAfterScan;
  unsigned short * lpMotorTable;
} LLF_CALCULATEMOTORTABLE;

typedef struct tagLLF_SETMOTORTABLE
{
  unsigned int TableSize;
  SANE_Byte MotorTableAddress;
  unsigned short *MotorTablePtr;
} LLF_SETMOTORTABLE;

typedef struct tagLLF_MOTORMOVE
{
  SANE_Byte ActionMode;		/* 0: AccDec Mode, 1: Uniform Speed Mode, 2: Test Mode */
  SANE_Byte ActionType;		/* 0: Forward, 1: Backward, 2:Back To Home */
  SANE_Byte MotorSelect;		/* 0: Motor 0 only, 1: Motor 1 only, 2: Motor 0 & 1 */
  SANE_Byte HomeSensorSelect;	/* 0: Sensor 0, 1: Sensor 1, 2: Sensor 0 & 1, 3:Invert Sensor 1 */
  unsigned short FixMoveSpeed;
  unsigned int FixMoveSteps;		/* 3 bytes */
  SANE_Byte MotorSpeedUnit;
  SANE_Byte MotorSyncUnit;
  unsigned short AccStep;			/*Max = 511 */
  SANE_Byte DecStep;			/* Max = 255 */
  SANE_Byte MotorMoveUnit;
  SANE_Byte WaitOrNoWait;		/* 0: no wait, 1: wait */
  SANE_Byte Lamp0PwmFreq;		/* Lamp0 PWM freq */
  SANE_Byte Lamp1PwmFreq;		/* Lamp1 PWM freq */

  unsigned short wForwardSteps;
  unsigned short wScanAccSteps;
  SANE_Byte bScanDecSteps;
  unsigned short wFixScanSteps;
  unsigned short wScanBackTrackingSteps;
  unsigned short wScanRestartSteps;
  unsigned short wScanBackHomeExtSteps;
} LLF_MOTORMOVE;

static STATUS CalculateMotorTable (LLF_CALCULATEMOTORTABLE *
				   lpCalculateMotorTable, unsigned short wYResolution);
static STATUS LLFCalculateMotorTable (LLF_CALCULATEMOTORTABLE *
				      lpCalculateMotorTable);
static STATUS LLFSetMotorCurrentAndPhase (PAsic chip,
					  LLF_MOTOR_CURRENT_AND_PHASE *
					  MotorCurrentAndPhase);
static STATUS SetMotorStepTable (PAsic chip, LLF_MOTORMOVE * MotorStepsTable,
				 unsigned short wStartY, unsigned int dwScanImageSteps,
				 unsigned short wYResolution);
static STATUS LLFSetMotorTable (PAsic chip,
				LLF_SETMOTORTABLE * LLF_SetMotorTable);
static STATUS SetMotorCurrent (PAsic chip, unsigned short dwMotorSpeed,
			       LLF_MOTOR_CURRENT_AND_PHASE * CurrentPhase);
static STATUS LLFMotorMove (PAsic chip, LLF_MOTORMOVE * LLF_MotorMove);
#if SANE_UNUSED
static STATUS LLFStopMotorMove (PAsic chip);
#endif
static STATUS LLFSetRamAddress (PAsic chip, unsigned int dwStartAddr,
				unsigned int dwEndAddr, SANE_Byte byAccessTarget);
static STATUS LLFRamAccess (PAsic chip, LLF_RAMACCESS * RamAccess);
static STATUS MotorBackHome (PAsic chip, SANE_Byte WaitOrNoWait);

#endif
