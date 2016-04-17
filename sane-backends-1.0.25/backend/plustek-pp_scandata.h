/* @file plustek-pp_scandata.h
 * @brief here we define the ScanData structure...
 *        and a lot of register settings
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson <rick@efn.org>
 *.............................................................................
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - added fWarmupNeeded to struct ScanData
 *      - removed function FillDataToColorTable from struct ScanData
 *      - removed dwLampDelay from struct ScanData
 * 0.33 - cosmetic changes
 *      - removed PositionLamp from structure
 *      - added dwLastPortMode to struct ScanData
 * 0.34 - removed WaitBack() function from pScanData structure
 *      - removed wStayMaxStep from pScanData structure
 * 0.35 - removed SetInitialGainRAM from pScanData structure
 *      - changed ModelStr list
 * 0.36 - added some defines for the ASIC 96001 (model 4800)
 *      - added wDither to DataInfo structure
 *      - removed dwPreferSize from struct ScannerCaps
 *      - cleanup
 *      - moved all stuff that is used by the backend and the driver
 *        to plustek-share.h which is in the backend directory
 *      - added ModelOverride parameter to struct
 *      - added strcut pardevice to struct
 * 0.37 - added bIODelay for SPP/BIDI port operations
 *      - added ReadData to struct
 *      - added ProcDirDef
 *      - added ButtonCount
 *      - removed RegisterToScanner from struct
 *      - removed MaxDpiByInterpolation from struct
 * 0.38 - added function PutToIdleMode() to struct
 *      - added function Calibration() to struct
 *      - changed interface of the ReInitAsic() function
 *      - major changes: moved a lot of stuff to hwdefs.h
 *      - added IO, Device, Shade, Scan and Bufs to struct
 * 0.39 - added forceMode to struct
 *      - added f97003, b97003DarkR, b97003DarkB, b97003DarkG to struct
 * 0.40 - no changes
 * 0.41 - no changes
 * 0.42 - no changes
 * 0.43 - changed type of XYRatio from double to long
 *      - cleanup
 * 0.44 - changes as Long defaults now to int32_t
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
#ifndef __SCANDATA_H__
#define __SCANDATA_H__

/*
 *Directory information for the /proc interface
 */
typedef struct {
	struct proc_dir_entry *entry;				/* Directory /proc/pt_drv/X  */
	struct proc_dir_entry *info;				/*		.../info             */
	struct proc_dir_entry *buttons[_MAX_BTNS];  /*		.../buttons          */
} ProcDirDef, *pProcDirDef;

/*
 * here we have some structs internally used
 */
typedef struct {
    ULong	    	dwVxdFlag;
    ULong	    	dwScanFlag;

/*
 * CHECK: why there are dups ?
 */
    ULong	    	dwAppLinesPerArea;
    ULong	    	dwAppPixelsPerLine;
    ULong	    	dwAppPhyBytesPerLine;
    ULong	    	dwAppBytesPerLine;
    ULong	    	dwAsicPixelsPerPlane;
    ULong	    	dwAsicBytesPerPlane;
    ULong	    	dwAsicBytesPerLine;
    CropRect    	crImage;
    XY		    	xyAppDpi;
    XY		    	xyPhyDpi;
    pUChar	    	pCurrentBuffer;
    UShort		    wPhyDataType;
    UShort			wAppDataType;
    UShort			wYSum;

    short	    	siBrightness;

/* CHANGE added these vars for scaling
 */
	Long    XYRatio;
	ULong   dwPhysBytesPerLine;

/*
 * CHANGE added this for selecting dither method
 */
    UShort  wDither;

} DataInfo, *pDataInfo;


/*
 * here it is, the great structure
 */
typedef struct scandata
{
#ifdef __KERNEL__
	UInt	flags;          	/* as follows:  */
#define	_PTDRV_INITALIZED	0x00000001
#define	_PTDRV_OPEN		    0x00000002

	struct pardevice *pardev;	/* for accessing parport... */
	struct parport   *pp;
	ProcDirDef		  procDir;
#else
	int pardev;                 /* parport handle in user-space */
#endif

	/*
	 * device control
	 */
	ULong devno;
	int   lampoff;
	int   warmup;	
	int   lOffonEnd;

	/*	
	 * CHECK for controlling the ECP-mode (not used now)
	 */
#if 0
	Byte	bOldECR;
	Bool	fECPReadWriteTest;
	Bool	fSkipEcpFlag;
	Bool	fECPFlag;
	Bool	fECPtoEPP;
	Bool	fECRFIFO;
#endif

    /*
     * the following stuff gets changed on a per model basis
     */
	UShort ModelOverride;	/* for non-auto detection stuff		*/

    UShort Offset70;            /* CHECK: --> to Device */
    UShort BufferSizeBase;      /* --> to Device */
    UShort BufferSizePerModel;  /* --> to Device */
	UShort TimePerLine;         /* --> to Device */

    /*
     * scanner properties 
     */
   	RegData     AsicReg;  		/* here we have the 98001/3 register set	*/
	Reg96		Asic96Reg;		/* here we hold the 96001/3 specific regs	*/

	LensInfo	LensInf;
    ScannerCaps sCaps;
	ULong 		dwScannerSize;
	Byte 		bCurrentSpeed;
	pUChar 		pbMapRed;
	pUChar 		pbMapGreen;
	pUChar 		pbMapBlue;

    ULong		TotalBufferRequire;
	ULong 		BufferForColorRunTable;
    UShort		PhysicalDpi;
	UShort      RdPix;          /* for ASIC 96003 devices */

	Byte 		a_bMapTable[4096 * 3];  	/* pre 98001 was 256 * 3 */
	Byte  		a_nbNewAdrPointer[_SCANSTATE_BYTES];

	/*
	 * for P9600x ASIC based scanners
	 */
	Bool 		fColorMoreRedFlag;
	Bool		fColorMoreBlueFlag;
	Bool		fSonyCCD;
    Bool        f97003;
	Byte		AsicRedColor;
	Byte		AsicGreenColor;
	Byte		RedDataReady;
	Byte		GreenDataReady;
	Byte 		b1stColorByte;
	Byte 		b1stColor;
	Byte 		b1stMask;
	Byte 		b2ndColorByte;
	Byte 		b2ndColor;
	Byte 		b2ndMask;
	Byte 		b3rdColorByte;
	Byte 		b3rdColor;
	Byte 		b3rdMask;
	Byte		b1stLinesOffset;
	Byte		b2ndLinesOffset;
	Byte		bLampOn;
	Byte 		bExtraAdd;
	Byte		bFifoCount;
	Byte		bMinReadFifo;
    Byte		FullStep;
    Byte 		StepMask;
    Byte		MotorOn;
	Byte		MotorFreeRun;
    Byte		IgnorePF;
	Byte		bMotorStepTableNo;

	/* for ASIC 97003... */
	Byte        b97003DarkR;
	Byte        b97003DarkG;
	Byte        b97003DarkB;

/* CHECK: to Scan!!!!  */
	pUChar		pGetBufR;  /* NOTE: these aren't actually Red/Green buffer  */
	pUChar		pGetBufG;  /*		pointers but instead are used 			*/
	pUChar		pPutBufR;  /*		generically to point to the first 2		*/
	pUChar		pPutBufG;  /*		color buffers as temp storage 			*/

	pUChar		pCurrentColorRunTable;
    UShort		a_wGrayInitTime[3];
	UShort		a_wColorInitTime[3];
	UShort		BackwardSteps;
	UShort		wLinesPer64kTime;
    UShort		ShadingBufferSize;
    UShort		ShadingBankSize;
	UShort		ShadingBankRed;
	UShort		ShadingBankGreen;
	UShort		ShadingBankBlue;
	UShort		ShadingScanLineBlks;
	UShort		ShadingScanLineLen;
	UShort		wOverBlue;
	UShort		FBKScanLineBlks;
	UShort		FBKScanLineLenBase;
	UShort		FBKScanLineLen;
	UShort		OneScanLineLen;

	/*
	 * the DAC part - to Shade !!!
	 */
	UShort		wsDACCompareHighRed, wsDACCompareLowRed;
	UShort		wsDACCompareHighGreen, wsDACCompareLowGreen;
	UShort		wsDACCompareHighBlue, wsDACCompareLowBlue;
	UShort		wsDACOffsetRed, wsDACOffsetGreen, wsDACOffsetBlue;
	Byte		bsPreRedDAC, bsPreGreenDAC, bsPreBlueDAC;
	Byte  		bRedDAC, bGreenDAC,	bBlueDAC;
	Byte  		bRedGainIndex, bGreenGainIndex, bBlueGainIndex;

	/*
	 * for image description
	 */
	DataInfo	DataInf;
	Bool		fReshaded;
	ULong 		dwDitherIndex;  	
	Bool		fDoFilter, fFilterFirstLine;
	ULong		dwDivFilter;
	ULong		dwMul;
	Byte		bOffsetFilter;
	ULong		dwLinesFilter;
	pUChar		pFilterBuf, pEndBuf;
	pUChar		pGet1, pGet2, pGet3;

	Byte 		bSetScanModeFlag;	/* see Section 5 - Scanmodes --> ps->Shade.bIntermediate*/

	/*
	 * some admin vals (they used to be global vars in the original driver)
	 */
	Bool	fScanningStatus;
	Byte	bLastLampStatus;
	Bool	fWarmupNeeded;
	ULong	dwOffset70;
	ULong   dwMaxReadFifoData;

	/*
 	 *
	 */
	pUChar pColorRunTable;
	pUChar pPrescan16;
	pUChar pPrescan8;
    UShort BufferForDataRead1;
    ULong  BufferFor1stColor;
    ULong  BufferFor2ndColor;
	pUChar driverbuf;
	pUChar pEndBufR;
	pUChar pEndBufG;
	pUChar pProcessingBuf;

	/*
	 * formerly used as global vars in ioproc.c, now in genericio.c
	 */
	pUChar 		 pScanBuffer1;
	pUChar		 pScanBuffer2;

	pModeTypeVar lpEppColorHomePos;
	pModeTypeVar lpEppColorExposure;
	pModeTypeVar lpBppColorHomePos;
	pModeTypeVar lpSppColorHomePos;
	UShort 		 wMinCmpDpi;
	pModeTypeVar a_ColorSettings;
	pDiffModeVar a_tabDiffParam;

	Byte		 bSpeed48;
	Byte		 bSpeed32;
	Byte		 bSpeed24;
	Byte		 bSpeed16;
	Byte	 	 bSpeed12;
	Byte	 	 bSpeed8;
	Byte		 bSpeed6;
	Byte		 bSpeed4;
	Byte		 bSpeed3;
	Byte		 bSpeed2;
	Byte		 bSpeed1;

	Byte 		 bHpMotor;
	Byte		 bStepSpeed;
	ULong		 dwFullStateSpeed;

	/*
	 * reference to globals from motor.c
	 */
	Bool		fHalfStepTableFlag;
	Bool		fFullLength;
	Byte 		bMoveDataOutFlag;
	Byte 		bExtraMotorCtrl;
	Byte 		bFastMoveFlag;
	Byte		bOldStateCount;
	Byte		bMotorSpeedData;
	Byte		bCurrentLineCount;
	Byte		bNewGap;
	Byte		bNewCurrentLineCountGap;
	UShort		wMaxMoveStep;
	ULong		dwScanStateCount;
	ULong		dwColorRunIndex;
	pByte		a_bColorByteTable;
	pUChar		pScanState;
	pUShort		a_wMoveStepTable;

	/*
	 * for shading - dac.c
     * CHECK: move to ps->Shade
	 */
	Byte 	bShadingTimeFlag;
	ULong   dwShadow, dwShadowCh;
	ULong	dwHilight, dwHilightCh;
	ULong	dwShadingLen, dwShadingPixels;
	pUShort pwShadow;

	/*
	 * from transform.c
	 */
	Byte	bRedHigh, bGreenHigh, bBlueHigh;
	UShort	wPosAdjustX;
	UShort	wNegAdjustX;
	UShort	wReduceRedFactor;
	UShort	wReduceGreenFactor;
	UShort	wReduceBlueFactor;
	ULong	dwOffsetNegative;

	/*
	 * reference to globals from map.c
	 */
#define _DITHERSIZE 64
	Byte	a_bDitherPattern[_DITHERSIZE];
	Short	wBrightness;
	Short	wContrast;
	UShort 	wInitialStep;
	ULong	dwSizeMustProcess;

	/*
	 * here we have pointers to the functions to call
	 */
	Bool (*OpenScanPath) 	 	  	 (pScanData);
	void (*CloseScanPath)	 	  	 (pScanData);
	int  (*ReadWriteTest)	 	  	 (pScanData);
	void (*PutToIdleMode)	 	  	 (pScanData);
	int  (*Calibration) 	 	  	 (pScanData);
	void (*SetupScannerVariables) 	 (pScanData);
	int  (*SetupScanSettings)     	 (pScanData, pScanInfo pInf );
	void (*GetImageInfo)    	  	 (pScanData, pImgDef pInf );
	Bool (*WaitForShading)		  	 (pScanData);
	void (*WaitForPositionY) 	  	 (pScanData);
	void (*InitialSetCurrentSpeed)	 (pScanData);
	Bool (*GotoShadingPosition)   	 (pScanData);
	void (*SetupScanningCondition)	 (pScanData);
	void (*SetMotorSpeed) 		  	 (pScanData,Byte bSpeed,Bool fSetRunState);
	void (*FillRunNewAdrPointer)  	 (pScanData);
	void (*SetupMotorRunTable)       (pScanData);
	void (*PauseColorMotorRunStates) (pScanData);
	void (*UpdateDataCurrentReadLine)(pScanData);
	Bool (*ReadOneImageLine)		 (pScanData);

	/* used only by ASIC9800x Part of the driver ! */
	void (*ReInitAsic)				 (pScanData, Bool shading);

	/* value used to read nibble's */
	Byte CtrlReadHighNibble;
	Byte CtrlReadLowNibble;

  	/*
	 * asic register offset values
	 */
	Byte RegSwitchBus;
	Byte RegEPPEnable;
	Byte RegECPEnable;
	Byte RegReadDataMode;
	Byte RegWriteDataMode;
	Byte RegInitDataFifo;
	Byte RegForceStep;
	Byte RegInitScanState;
	Byte RegRefreshScanState;
	Byte RegThresholdGapControl;
	Byte RegADCAddress;
	Byte RegADCData;
	Byte RegADCPixelOffset;
	Byte RegADCSerialOutStr;
	Byte RegResetConfig;
	Byte RegLensPosition;
	Byte RegStatus;
	Byte RegWaitStateInsert;
	Byte RegFifoOffset;
	Byte RegRFifoOffset;
	Byte RegGFifoOffset;
	Byte RegBFifoOffset;
	Byte RegBitDepth;
	Byte RegStepControl;
	Byte RegMotor0Control;
	Byte RegXStepTime;
	Byte RegGetScanState;
	Byte RegAsicID;
	Byte RegReadIOBufBus;
	Byte RegMemoryLow;
	Byte RegMemoryHigh;
	Byte RegModeControl;
	Byte RegLineControl;
	Byte RegScanControl;
    Byte RegMotorControl;
#define _MotorDirForward	 0x01	/* go forward                */
#define _MotorOn		     0x02	/* turn on motor             */
#define _MotorIgnorePF	     0x04	/* motor rolling don't care  */
						            /* paper define flag         */
#define _MotorFreeRun	     0x80	/*ScanState count don't stop */
	    /* Following bits (bit 3 & 4 are depended on StatusPort  */
	    /* bit-7:MotorType when it is 1:                         */
#define _Motor1FullStep	     0x08	/* bit 4 is ignored          */
	                                /* When it is 0:             */
#define _Motor0FullStepWeak      0  /* Full step (driving weak)  */
#define _Motor0HalfStep	      0x10	/* 1/2 step                  */
#define _Motor0QuarterStep	  0x08	/* 1/4 step                  */
#define _Motor0FullStepStrong 0x18	/* Full step (driving strong)*/
#define _MotorStepMask	      0xe7
	/* for 96001 */
#define _MotorFullStep96001   0x02
#define _MotorOn96001	      0x04
#define _MotorIgnorePF96001   0x08

  Byte RegConfiguration;
  Byte RegModelControl;
  Byte RegModel1Control;
  Byte RegMemAccessControl;
#define     _MemBanks   	     64  /* the number of banks, 5 ls bits */
#define     _MemBankMask	     (_MemBanks - 1)
#define     _MemBankSize1k	     0
#define     _MemBankSize2k	     0x40
#define     _MemBankSize4k	     0x80
#define     _MemBankSize8k	     0xc0
	/* 96001 specific */
#define     _MemBankSize2k96001      0x00
#define     _MemBankSize4k96001      0x40
#define     _MemBankSize8k96001      0x80

  Byte RegDpiLow;
  Byte RegDpiHigh;
  Byte RegScanPosLow;
  Byte RegScanPosHigh;
  Byte RegWidthPixelsLow;
  Byte RegWidthPixelsHigh;
  Byte RegThresholdLow;
  Byte RegThresholdHigh;
  Byte RegThresholdControl;
  Byte RegWatchDogControl;
#define     _WDOnIntervalMask	     0x0f	/* WD * 8192 scan lines to turn
						                       off Lamp */
#define     _WDMotorLongInterval     0x40	/* short = 8192 lines time
						                        long = 32768 lines time */
#define     _WDEnable		     0x80
  Byte RegModelControl2;
#define     _Model2ChannelSlct	     0
#define     _Model2ChannelMult	     0x01	/* bit on/off accords to JONES */
#define     _Model2CCSInvert	     0x02
#define     _Model2DirectOutPort     0x04
#define     _Model2PipeLineDelayN    0x08
#define     _Model2ShiftGapTiming10  0x10
#define     _Model2BtnKeyPassThrough 0x20
  Byte RegRedDCAdjust;
  Byte RegGreenDCAdjust;
  Byte RegBlueDCAdjust;
  Byte RegRedChShadingOffset;
  Byte RegGreenChShadingOffset;
  Byte RegBlueChShadingOffset;
  Byte RegRedChDarkOffset;
  Byte RegGreenChDarkOffset;
  Byte RegBlueChDarkOffset;
  Byte RegWriteIOBusDecode1;
  Byte RegWriteIOBusDecode2;
  Byte RegScanStateControl;
#define     _ScanStateEvenMask	     0x0f
#define     _ScanStateOddMask	     0xf0
  Byte RegRedChEvenOffset;
  Byte RegGreenChEvenOffset;
  Byte RegBlueChEvenOffset;
  Byte RegRedChOddOffset;
  Byte RegGreenChOddOffset;
  Byte RegBlueChOddOffset;
  Byte RegRedGainOutDirect;
  Byte RegGreenGainOutDirect;
  Byte RegBlueGainOutDirect;
  Byte RegLedControl;
#define     _LedCmdActEnable	     0x04
#define     _LedMotorActEnable	     0x08
#define     _LedClrChActEnable	     0x10	/* Color Channel Action  */
#define     _LedLightOnActEnable     0x20
#define     _LedHostTurnOnEnable     0x40
#define     _LedActControl	     0x80
    Byte RegShadingCorrectCtrl;
#define     _ShadingRCorrectX1	     0
#define     _ShadingRCorrectX2	     0x01
#define     _ShadingRCorrectX3	     0x02
#define     _ShadingRCorrectX4	     0x03
#define     _ShadingGCorrectX1	     0
#define     _ShadingGCorrectX2	     0x04
#define     _ShadingGCorrectX3	     0x08
#define     _ShadingGCorrectX4	     0x0c
#define     _ShadingBCorrectX1	     0
#define     _ShadingBCorrectX2	     0x10
#define     _ShadingBCorrectX3	     0x20
#define     _ShadingBCorrectX4	     0x30
    Byte RegScanStateBegin;
    Byte RegRedChDarkOffsetLow;
    Byte RegRedChDarkOffsetHigh;
    Byte RegGreenChDarkOffsetLow;
    Byte RegGreenChDarkOffsetHigh;
    Byte RegBlueChDarkOffsetLow;
    Byte RegBlueChDarkOffsetHigh;
    Byte RegResetPulse0;
    Byte RegResetPulse1;
    Byte RegCCDClampTiming0;
    Byte RegCCDClampTiming1;
    Byte RegVSMPTiming0;
    Byte RegVSMPTiming1;
    Byte RegCCDQ1Timing0;
    Byte RegCCDQ1Timing1;
    Byte RegCCDQ1Timing2;
    Byte RegCCDQ1Timing3;
    Byte RegCCDQ2Timing0;
    Byte RegCCDQ2Timing1;
    Byte RegCCDQ2Timing2;
    Byte RegCCDQ2Timing3;
    Byte RegADCclockTiming0;
    Byte RegADCclockTiming1;
    Byte RegADCclockTiming2;
    Byte RegADCclockTiming3;
    Byte RegADCDVTiming0;
    Byte RegADCDVTiming1;
    Byte RegADCDVTiming2;
    Byte RegADCDVTiming3;
    Byte RegScanStateEnd;

    /* ASIC 98003 specific*/
    Byte RegFifoFullLength0;
    Byte RegFifoFullLength1;
    Byte RegFifoFullLength2;

    Byte RegMotorTotalStep0;	
    Byte RegMotorTotalStep1;
    Byte RegMotorFreeRunCount0;	
    Byte RegMotorFreeRunCount1;
    Byte RegScanControl1;
    Byte RegMotorFreeRunTrigger;

    Byte RegResetMTSC;

    Byte RegMotor1Control;
    Byte RegMotor2Control;
    Byte RegMotorDriverType;
    Byte RegStatus2;
    Byte RegExtendedLineControl;
    Byte RegExtendedXStep;

    Byte RegPllPredivider;
    Byte RegPllMaindivider;
    Byte RegPllPostdivider;
    Byte RegClockSelector;
    Byte RegTestMode;

/* CHECK: subject to change */
    IODef      IO;
    DeviceDef  Device;
    ShadingDef Shade;
    ScanDef    Scan;
    BufferDef  Bufs;

} ScanData;

#endif  /* guard __SCANDATA_H__ */

/* END PLUTSEK-PP_SCANDATA.H ................................................*/
