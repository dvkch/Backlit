/* @file plustek-pp_p12.c
 * @brief p12 and pt12 specific stuff
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2000 Plustek Inc.
 * Copyright (C) 2001-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.38 - initial version
 * - 0.39 - added Genius Colorpage Vivid III V2 stuff
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - removed setting of ps->sCaps.dwFlag in p12InitiateComponentModel()
 * - 0.43 - no changes
 * - 0.44 - fix format string issues, as Long types default to int32_t
 *          now
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
#include "plustek-pp_scan.h"

/*************************** some local vars *********************************/

static RegDef p12CcdStop[] = {
    {0x41, 0xff}, {0x42, 0xff}, {0x60, 0xff}, {0x61, 0xff},
    {0x4b, 0xff}, {0x4c, 0xff}, {0x4d, 0xff}, {0x4e, 0xff},
    {0x2a, 0x01}, {0x2b, 0x00}, {0x2d, 0x00}, {0x1b, 0x19}, {0x15, 0x00}
};

/*************************** local functions *********************************/

/** init the stuff according to the buttons
 */
static void p12ButtonSetup( pScanData ps, Byte nrOfButtons )
{
    ps->Device.buttons = nrOfButtons;

    ps->Device.Model1Mono  &= ~_BUTTON_MODE;
    ps->Device.Model1Color &= ~_BUTTON_MODE;

    ps->AsicReg.RD_MotorDriverType |= _BUTTON_DISABLE;
    ps->Scan.motorPower            |= _BUTTON_DISABLE;
}

/** According to what we have detected, set the other stuff
 */
static void p12InitiateComponentModel( pScanData ps )
{
    /* preset some stuff and do the differences later */
    ps->Device.buttons        = 0;
    ps->Device.Model1Mono     = _BUTTON_MODE + _CCD_SHIFT_GATE + _SCAN_GRAYTYPE;
    ps->Device.Model1Color    = _BUTTON_MODE + _CCD_SHIFT_GATE;
    ps->Device.dwModelOriginY = 64;
    ps->Device.fTpa           = _FALSE;
    ps->Device.ModelCtrl      = (_LED_ACTIVITY | _LED_CONTROL);

	/* ps->sCaps.dwFlag should have been set correctly in models.c */

    switch( ps->Device.bPCBID ) {

	case _PLUSTEK_SCANNER:
    	DBG( DBG_LOW, "We have a Plustek Scanner\n" );
        ps->sCaps.Model = MODEL_OP_P12;
	    break;

	case _SCANNER_WITH_TPA:
    	DBG( DBG_LOW, "Scanner has TPA\n" );
	    ps->Device.fTpa   = _TRUE;
	    ps->sCaps.dwFlag |= SFLAG_TPA;
	    break;

	case _SCANNER4Button:
    	DBG( DBG_LOW, "Scanner has 4 Buttons\n" );
	    p12ButtonSetup( ps, 4 );
	    break;

	case _SCANNER4ButtonTPA:
    	DBG( DBG_LOW, "Scanner has 4 Buttons & TPA\n" );
	    ps->Device.fTpa   = _TRUE;
	    ps->sCaps.dwFlag |= SFLAG_TPA;
	    p12ButtonSetup( ps, 4 );
	    break;

	case _SCANNER5Button:
    	DBG( DBG_LOW, "Scanner has 5 Buttons\n" );
	    ps->Device.dwModelOriginY = 64 + 20;	
	    p12ButtonSetup( ps, 5 );
	    break;

	case _SCANNER5ButtonTPA:
    	DBG( DBG_LOW, "Scanner has 5 Buttons & TPA\n" );
	    ps->Device.dwModelOriginY = 64 + 20;
	    ps->Device.fTpa           = _TRUE;
	    ps->sCaps.dwFlag         |= SFLAG_TPA;
	    p12ButtonSetup( ps, 5 );
	    break;

	case _SCANNER1Button:
    	DBG( DBG_LOW, "Scanner has 1 Button\n" );
	    p12ButtonSetup( ps, 1 );
	    break;

	case _SCANNER1ButtonTPA:
    	DBG( DBG_LOW, "Scanner has 1 Button & TPA\n" );
        ps-> Device.fTpa  = _TRUE;
	    ps->sCaps.dwFlag |= SFLAG_TPA;
	    p12ButtonSetup( ps, 1 );
	    break;

	case _AGFA_SCANNER:
    	DBG( DBG_LOW, "Agfa Scanner\n" );
	    ps->Device.dwModelOriginY = 24; 	/* 1200 dpi */
	    break;

	case _SCANNER2Button:
    	DBG( DBG_LOW, "Scanner has 2 Buttons\n" );
    	DBG( DBG_LOW, "Seems we have a Genius Colorpage Vivid III V2\n" );
	    ps->Device.dwModelOriginY = 64 - 33;	
	    p12ButtonSetup( ps, 2 );
        ps->sCaps.Model = MODEL_GEN_CPV2;
	    break;

    default:
    	DBG( DBG_LOW, "Default Model: P12\n" );
        ps->sCaps.Model = MODEL_OP_P12;
        break;
    }

    if( _MOTOR0_2003 == ps->Device.bMotorID ) {
    	ps->Device.f2003      = _TRUE;
    	ps->Device.XStepMono  = 10;
	    ps->Device.XStepColor = 6;
    	ps->Device.XStepBack  = 5;
	    ps->AsicReg.RD_MotorDriverType |= _MOTORR_STRONG;
    } else {
    	ps->Device.f2003      = _FALSE;
    	ps->Device.XStepMono  = 8;
	    ps->Device.XStepColor = 4;
    	ps->Device.XStepBack  = 5;
	    ps->AsicReg.RD_MotorDriverType |= _MOTORR_WEAK;
    }
}

/*.............................................................................
 * prepare all the necessary variables -
 */
static void p12SetupScannerVariables( pScanData ps )
{
	DBG( DBG_LOW, "p12SetupScannerVariables()\n" );

    /*
     * these values were originally altered by registry entries (NT-driver)
     * and used to adjust the picture position...
     */
    ps->Device.lUpNormal   = 0;
    ps->Device.lUpNegative = 20;
    ps->Device.lUpPositive = -30;

    ps->Device.lLeftNormal = 51;

    ps->OpenScanPath( ps );
	ps->ReInitAsic( ps, _FALSE );
    ps->CloseScanPath( ps );
}

/*.............................................................................
 *
 */
static void p12SetupScanningCondition( pScanData ps )
{
    TimerDef timer;
    ULong    channel;
    Byte	 bState;
    pUChar	 pState = ps->Bufs.b1.pReadBuf;

	DBG( DBG_LOW, "p12SetupScanningCondition()\n" );

    P12SetGeneralRegister( ps );

    IORegisterToScanner( ps, ps->RegResetMTSC );

    /* ------- Setup MinRead/MaxRead Fifo size ------- */
    if( ps->DataInf.wPhyDataType <= COLOR_TRUE24 ) {
    	ps->Scan.dwMaxReadFifo =
	    ps->Scan.dwMinReadFifo = ps->DataInf.dwAsicBytesPerPlane * 2;
    } else {
    	ps->Scan.dwMaxReadFifo =
	    ps->Scan.dwMinReadFifo = ps->DataInf.dwAppPixelsPerLine << 1;
    }

    if( ps->Scan.dwMinReadFifo < 1024)
    	ps->Scan.dwMinReadFifo = ps->Scan.dwMaxReadFifo = 1024;

    ps->Scan.dwMaxReadFifo += (ps->DataInf.dwAsicBytesPerPlane / 2);


	DBG( DBG_LOW, "MinReadFifo=%u, MaxReadFifo=%u\n",	
         ps->Scan.dwMinReadFifo, ps->Scan.dwMaxReadFifo );

    /* ------- Set the max. read fifo to asic ------- */
    if( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

    	ps->Scan.bFifoSelect = ps->RegBFifoOffset;

    	if( !ps->Scan.p48BitBuf.pb ) {

    	    Long lRed, lGreen;

    	    lRed = (_SIZE_REDFIFO - _SIZE_BLUEFIFO) /
                    ps->DataInf.dwAsicBytesPerPlane - ps->Scan.bd_rk.wRedKeep;

    	    lGreen = (_SIZE_GREENFIFO - _SIZE_BLUEFIFO) /
                    ps->DataInf.dwAsicBytesPerPlane - ps->Scan.gd_gk.wGreenKeep;

	        if((lRed < 0) || (lGreen < 0)) {

        		if( lRed < lGreen ) {
        		    channel = _RED_FULLSIZE << 16;
		            ps->AsicReg.RD_BufFullSize = _SIZE_REDFIFO;
        		    lGreen = lRed;
		        } else {
        		    channel = _GREEN_FULLSIZE << 16;
		            ps->AsicReg.RD_BufFullSize = _SIZE_GREENFIFO;
        		}
		
                lGreen = (ULong)(-lGreen * ps->DataInf.dwAsicBytesPerPlane);

        		if(  ps->DataInf.wPhyDataType > COLOR_TRUE24 )
		            lGreen >>= 1;

                ps->Scan.dwMinReadFifo += (ULong)lGreen;
		        ps->Scan.dwMaxReadFifo += (ULong)lGreen;

    	    } else {
       		    channel = _BLUE_FULLSIZE << 16;
	            ps->AsicReg.RD_BufFullSize = _SIZE_BLUEFIFO;
            }
    	} else {
   		    channel = _BLUE_FULLSIZE << 16;
            ps->AsicReg.RD_BufFullSize = _SIZE_BLUEFIFO;
       	}
    } else {
    	ps->Scan.bFifoSelect = ps->RegGFifoOffset;
	    channel = _GREEN_FULLSIZE << 16;
        ps->AsicReg.RD_BufFullSize = _SIZE_GRAYFIFO;
    }

    ps->AsicReg.RD_BufFullSize -= (ps->DataInf.dwAsicBytesPerPlane << 1);

    if( ps->DataInf.wPhyDataType > COLOR_TRUE24 )
    	ps->AsicReg.RD_BufFullSize >>= 1;

    ps->AsicReg.RD_BufFullSize |= channel;

    ps->Scan.bRefresh = (Byte)(ps->Scan.dwInterval << 1);
    ps->AsicReg.RD_LineControl    = (_LOBYTE (ps->Shade.wExposure));
    ps->AsicReg.RD_ExtLineControl = (_HIBYTE (ps->Shade.wExposure));
    ps->AsicReg.RD_XStepTime      = (_LOBYTE (ps->Shade.wXStep));
    ps->AsicReg.RD_ExtXStepTime   = (_HIBYTE (ps->Shade.wXStep));
    ps->AsicReg.RD_Motor0Control  = _FORWARD_MOTOR;
    ps->AsicReg.RD_StepControl    = _MOTOR0_SCANSTATE;
    ps->AsicReg.RD_ModeControl    = (_ModeScan | _ModeFifoGSel);

    DBG( DBG_LOW, "bRefresh = %i\n", ps->Scan.bRefresh );

    if( ps->DataInf.wPhyDataType == COLOR_BW ) {
    	ps->AsicReg.RD_ScanControl = _SCAN_BITMODE;

		if( !(ps->DataInf.dwScanFlag & SCANDEF_Inverse))
			ps->AsicReg.RD_ScanControl |= _P98_SCANDATA_INVERT;

    } else if( ps->DataInf.wPhyDataType <= COLOR_TRUE24 )
	    ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE;
	else {
	    ps->AsicReg.RD_ScanControl = _SCAN_12BITMODE;

        if(!(ps->DataInf.dwScanFlag & SCANDEF_RightAlign))
	    	ps->AsicReg.RD_ScanControl |= _BITALIGN_LEFT;

    	if( ps->DataInf.dwScanFlag & SCANDEF_Inverse)
    		ps->AsicReg.RD_ScanControl |= _P98_SCANDATA_INVERT;
    }

    ps->AsicReg.RD_ScanControl |= _SCAN_1ST_AVERAGE;
    IOSelectLampSource( ps );

	DBG( DBG_LOW, "RD_ScanControl = 0x%02x\n", ps->AsicReg.RD_ScanControl );

    ps->AsicReg.RD_MotorTotalSteps = (ULong)ps->DataInf.crImage.cy * 4 +
                                 	 ((ps->Device.f0_8_16) ? 32 : 16) +
                                     ((ps->Scan.bDiscardAll) ? 32 : 0);

    ps->AsicReg.RD_ScanControl1 = (_MTSC_ENABLE | _SCANSTOPONBUFFULL |
                               	   _MFRC_RUNSCANSTATE | _MFRC_BY_XSTEP);

    ps->AsicReg.RD_Dpi = ps->DataInf.xyPhyDpi.x;

    if(!(ps->DataInf.dwScanFlag & SCANDEF_TPA )) {

    	ps->AsicReg.RD_Origin = (UShort)(ps->Device.lLeftNormal * 2 +
    	                        		 ps->Device.DataOriginX +
                    				     ps->DataInf.crImage.x );

    } else if( ps->DataInf.dwScanFlag & SCANDEF_Transparency ) {
	    ps->AsicReg.RD_Origin =
                            (UShort)(ps->Scan.posBegin + ps->DataInf.crImage.x);
	} else {
	    ps->AsicReg.RD_Origin =
                            (UShort)(ps->Scan.negBegin + ps->DataInf.crImage.x);
    }

    if( ps->Shade.bIntermediate & _ScanMode_AverageOut )
    	ps->AsicReg.RD_Origin >>= 1;

    if( ps->DataInf.wPhyDataType == COLOR_BW )
    	ps->AsicReg.RD_Pixels = (UShort)ps->DataInf.dwAsicBytesPerPlane;
    else
	    ps->AsicReg.RD_Pixels = (UShort)ps->DataInf.dwAppPixelsPerLine;

	DBG( DBG_LOW, "RD_Origin = %u, RD_Pixels = %u\n",
					ps->AsicReg.RD_Origin, ps->AsicReg.RD_Pixels );

    /* ------- Prepare scan states ------- */
    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
    memset( ps->Bufs.b1.pReadBuf,  0, _NUMBER_OF_SCANSTEPS );

    if( ps->DataInf.wPhyDataType <= COLOR_256GRAY )
    	bState = (_SS_MONO | _SS_STEP);
    else
	    bState = (_SS_COLOR | _SS_STEP);

    for( channel = _NUMBER_OF_SCANSTEPS;
                                    channel; channel -= ps->Scan.dwInterval ) {
    	*pState = bState;
	    if( ps->Scan.dwInterlace )
	        pState[ ps->Scan.dwInterlace] = _SS_STEP;
    	pState += ps->Scan.dwInterval;
    }
    for( channel = 0, pState = ps->Bufs.b1.pReadBuf;
                                      channel < _SCANSTATE_BYTES; channel++)  {
    	ps->a_nbNewAdrPointer[channel] = pState [0] | (pState [1] << 4);
	    pState += 2;
    }

    /* ------- Wait for scan state stop ------- */
    MiscStartTimer( &timer, _SECOND * 2 );

    while(!(IOGetScanState( ps, _FALSE ) & _SCANSTATE_STOP) &&
                                                    !MiscCheckTimer(&timer));

/* CHECK: Replace by IOPutAll.... */
    IODownloadScanStates( ps );
    IODataToRegister( ps, ps->RegLineControl, ps->AsicReg.RD_LineControl);
    IODataToRegister( ps, ps->RegExtendedLineControl,
                          ps->AsicReg.RD_ExtLineControl);
    IODataToRegister( ps, ps->RegXStepTime, ps->AsicReg.RD_XStepTime);
    IODataToRegister( ps, ps->RegExtendedXStep, ps->AsicReg.RD_ExtXStepTime);
    IODataToRegister( ps, ps->RegMotorDriverType,
                          ps->AsicReg.RD_MotorDriverType);
    IODataToRegister( ps, ps->RegStepControl, ps->AsicReg.RD_StepControl);
    IODataToRegister( ps, ps->RegMotor0Control, ps->AsicReg.RD_Motor0Control);
    IODataToRegister( ps, ps->RegModelControl,ps->AsicReg.RD_ModelControl);
    IODataToRegister( ps, ps->RegDpiLow,  (_LOBYTE(ps->AsicReg.RD_Dpi)));
    IODataToRegister( ps, ps->RegDpiHigh, (_HIBYTE(ps->AsicReg.RD_Dpi)));
    IODataToRegister( ps, ps->RegScanPosLow, (_LOBYTE(ps->AsicReg.RD_Origin)));
    IODataToRegister( ps, ps->RegScanPosHigh,(_HIBYTE(ps->AsicReg.RD_Origin)));
    IODataToRegister( ps, ps->RegWidthPixelsLow,
                                             (_LOBYTE(ps->AsicReg.RD_Pixels)));
    IODataToRegister( ps, ps->RegWidthPixelsHigh,
                                             (_HIBYTE(ps->AsicReg.RD_Pixels)));
    IODataToRegister( ps, ps->RegThresholdLow,
                                   (_LOBYTE(ps->AsicReg.RD_ThresholdControl)));
    IODataToRegister( ps, ps->RegThresholdHigh,
                                   (_HIBYTE(ps->AsicReg.RD_ThresholdControl)));
    IODataToRegister( ps, ps->RegMotorTotalStep0,
                                    (_LOBYTE(ps->AsicReg.RD_MotorTotalSteps)));
    IODataToRegister( ps, ps->RegMotorTotalStep1,
                                    (_HIBYTE(ps->AsicReg.RD_MotorTotalSteps)));
    IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);

    IORegisterToScanner( ps, ps->RegInitDataFifo);
}

/*.............................................................................
 * program the CCD relevant stuff
 */
static void p12ProgramCCD( pScanData ps)
{
    UShort  w;
    pRegDef rp;

    DBG( DBG_IO, "p12ProgramCCD: 0x%08lx[%lu]\n",
            (unsigned long)ps->Device.pCCDRegisters,
            ((unsigned long)ps->Device.wNumCCDRegs * ps->Shade.bIntermediate));

    DBG( DBG_IO, " %u regs * %u (intermediate)\n",
                    ps->Device.wNumCCDRegs, ps->Shade.bIntermediate );

    rp = ps->Device.pCCDRegisters +	
         (ULong)ps->Device.wNumCCDRegs * ps->Shade.bIntermediate;

    for( w = ps->Device.wNumCCDRegs; w--; rp++ ) {

        DBG( DBG_IO, "[0x%02x] = 0x%02x\n", rp->bReg, rp->bParam );
        IODataToRegister( ps, rp->bReg, rp->bParam );
    }
}

/*.............................................................................
 * this initializes the ASIC and prepares the different functions for shading
 * and scanning
 */
static void p12Init98003( pScanData ps, Bool shading )
{
	DBG( DBG_LOW, "p12InitP98003(%d)\n", shading );

    /* get DAC and motor stuff */
    ps->Device.bDACType = IODataFromRegister( ps, ps->RegResetConfig );
    ps->Device.bMotorID = (Byte)(ps->Device.bDACType & _MOTOR0_MASK);

    ps->AsicReg.RD_MotorDriverType  =
                            (Byte)((ps->Device.bDACType & _MOTOR0_MASK) >> 3);
    ps->AsicReg.RD_MotorDriverType |=
                            (Byte)((ps->Device.bDACType & _MOTOR1_MASK) >> 1);


    ps->Scan.motorPower = ps->AsicReg.RD_MotorDriverType | _MOTORR_STRONG;

    ps->Device.bDACType &= _ADC_MASK;

    /*get CCD and PCB ID */
    ps->Device.bPCBID = IODataFromRegister( ps, ps->RegConfiguration );
    ps->Device.bCCDID = ps->Device.bPCBID & 0x07;
    ps->Device.bPCBID &= 0xf0;

    if( _AGFA_SCANNER == ps->Device.bPCBID )
        ps->Device.bDACType = _DA_WOLFSON8141;

	DBG( DBG_LOW, "PCB-ID=0x%02x, CCD-ID=0x%02x, DAC-TYPE=0x%02x\n",
                   ps->Device.bPCBID, ps->Device.bCCDID, ps->Device.bDACType );

    p12InitiateComponentModel( ps );

	/* encode the CCD-id into the flag parameter */
    ps->sCaps.dwFlag |= ((ULong)(ps->Device.bCCDID | ps->Device.bPCBID) << 16);

    P12InitCCDandDAC( ps, shading );

    if( ps->Shade.bIntermediate & _ScanMode_Mono )
    	ps->AsicReg.RD_Model1Control = ps->Device.Model1Mono;
    else
	    ps->AsicReg.RD_Model1Control = ps->Device.Model1Color;

    IODataToRegister( ps, ps->RegPllPredivider,  1 );
    IODataToRegister( ps, ps->RegPllMaindivider, 0x20 );
    IODataToRegister( ps, ps->RegPllPostdivider, 2 );
    IODataToRegister( ps, ps->RegClockSelector,	 3 );		/* 2 */
    IODataToRegister( ps, ps->RegMotorDriverType,
                          ps->AsicReg.RD_MotorDriverType );

    /* this might be changed, def value is 11 */
    IODataToRegister( ps, ps->RegWaitStateInsert, 11 );
    IODataToRegister( ps, ps->RegModel1Control, ps->AsicReg.RD_Model1Control );

    p12ProgramCCD( ps );
}

/*.............................................................................
 * initialize the register values for the 98003 asic and preset other stuff
 */
static void p12InitializeAsicRegister( pScanData ps )
{
    memset( &ps->AsicReg, 0, sizeof(RegData));
}

/*.............................................................................
 * as the function name says
 */
static void p12PutToIdleMode( pScanData ps )
{
    ULong i;

    ps->OpenScanPath( ps );

    DBG( DBG_IO, "CCD-Stop\n" );

   for( i = 0; i < 13; i++ ) {

        DBG( DBG_IO, "[0x%02x] = 0x%02x\n",
                        p12CcdStop[i].bReg, p12CcdStop[i].bParam );

        IODataToRegister( ps, p12CcdStop[i].bReg, p12CcdStop[i].bParam );
    }

    ps->CloseScanPath( ps );
}

/*.............................................................................
 * here we simply call the WaitForShading function which performs this topic
 */
static int p12Calibration( pScanData ps )
{
    Bool result;

	DBG( DBG_LOW, "p12Calibration()\n" );

	/*
	 * wait for shading to be done
	 */
    ps->OpenScanPath( ps );

	_ASSERT(ps->WaitForShading);
	result = ps->WaitForShading( ps );
    ps->CloseScanPath( ps );

	if( !result )
		return _E_TIMEOUT;

    return _OK;
}

/************************ exported functions *********************************/

/*.............................................................................
 * initialize the register values and function calls for the 98003 asic
 */
_LOC int P12InitAsic( pScanData ps )
{
	int result;

	DBG( DBG_LOW, "P12InitAsic()\n" );

    /*
     * preset the asic shadow registers
     */
    p12InitializeAsicRegister( ps );

	ps->IO.bOpenCount = 0;

	/*
	 * setup the register values
	 */
	ps->RegSwitchBus 			= 0;
  	ps->RegEPPEnable 			= 1;
	ps->RegECPEnable 			= 2;
	ps->RegReadDataMode 		= 3;
	ps->RegWriteDataMode 		= 4;
	ps->RegInitDataFifo 		= 5;
	ps->RegForceStep 			= 6;
	ps->RegInitScanState 		= 7;
	ps->RegRefreshScanState 	= 8;
	ps->RegWaitStateInsert 		= 0x0a;
	ps->RegRFifoOffset 			= 0x0a;
	ps->RegGFifoOffset 			= 0x0b;
	ps->RegBFifoOffset 			= 0x0c;
	ps->RegBitDepth 			= 0x13;
	ps->RegStepControl 			= 0x14;
	ps->RegMotor0Control 		= 0x15;
	ps->RegXStepTime 			= 0x16;
	ps->RegGetScanState 		= 0x17;
	ps->RegAsicID 				= 0x18;
	ps->RegMemoryLow 			= 0x19;
	ps->RegMemoryHigh 			= 0x1a;
	ps->RegModeControl 			= 0x1b;
	ps->RegLineControl 			= 0x1c;
	ps->RegScanControl 			= 0x1d;
	ps->RegConfiguration 		= 0x1e;
	ps->RegModelControl 		= 0x1f;
	ps->RegModel1Control 		= 0x20;
	ps->RegDpiLow 				= 0x21;
	ps->RegDpiHigh 				= 0x22;
	ps->RegScanPosLow 			= 0x23;
	ps->RegScanPosHigh 			= 0x24;
	ps->RegWidthPixelsLow 		= 0x25;
	ps->RegWidthPixelsHigh 		= 0x26;
	ps->RegThresholdLow 		= 0x27;
	ps->RegThresholdHigh 		= 0x28;
	ps->RegThresholdGapControl 	= 0x29;
	ps->RegADCAddress 			= 0x2a;
	ps->RegADCData 				= 0x2b;
	ps->RegADCPixelOffset 		= 0x2c;
	ps->RegADCSerialOutStr 		= 0x2d;
	ps->RegResetConfig 			= 0x2e;
	ps->RegLensPosition			= 0x2f;
	ps->RegStatus 				= 0x30;
	ps->RegScanStateControl 	= 0x31;
	ps->RegRedChDarkOffsetLow 	= 0x33;
	ps->RegRedChDarkOffsetHigh 	= 0x34;
	ps->RegGreenChDarkOffsetLow = 0x35;
	ps->RegGreenChDarkOffsetHigh= 0x36;
	ps->RegBlueChDarkOffsetLow 	= 0x37;
	ps->RegBlueChDarkOffsetHigh = 0x38;
	ps->RegResetPulse0 			= 0x39;
	ps->RegResetPulse1 			= 0x3a;
	ps->RegCCDClampTiming0 		= 0x3b;
	ps->RegCCDClampTiming1 		= 0x3c;
	ps->RegVSMPTiming0 			= 0x41;
	ps->RegVSMPTiming1 			= 0x42;
	ps->RegCCDQ1Timing0 		= 0x43;
	ps->RegCCDQ1Timing1 		= 0x44;
	ps->RegCCDQ1Timing2 		= 0x45;
	ps->RegCCDQ1Timing3 		= 0x46;
	ps->RegCCDQ2Timing0 		= 0x47;
	ps->RegCCDQ2Timing1 		= 0x48;
	ps->RegCCDQ2Timing2 		= 0x49;
	ps->RegCCDQ2Timing3 		= 0x4a;
	ps->RegADCclockTiming0 		= 0x4b;
	ps->RegADCclockTiming1		= 0x4c;
	ps->RegADCclockTiming2 		= 0x4d;
	ps->RegADCclockTiming3 		= 0x4e;
	ps->RegADCDVTiming0 		= 0x50;
	ps->RegADCDVTiming1 		= 0x51;
	ps->RegADCDVTiming2 		= 0x52;
	ps->RegADCDVTiming3 		= 0x53;

    ps->RegFifoFullLength0	    = 0x54;
    ps->RegFifoFullLength1	    = 0x55;
    ps->RegFifoFullLength2	    = 0x56;

    ps->RegMotorTotalStep0	    = 0x57;
    ps->RegMotorTotalStep1	    = 0x58;
    ps->RegMotorFreeRunCount0	= 0x59;
    ps->RegMotorFreeRunCount1	= 0x5a;
    ps->RegScanControl1	        = 0x5b;
    ps->RegMotorFreeRunTrigger  = 0x5c;

    ps->RegResetMTSC		    = 0x5d;

    ps->RegMotor1Control	    = 0x62;
    ps->RegMotor2Control	    = 0x63;
    ps->RegMotorDriverType	    = 0x64;

    ps->RegStatus2		        = 0x66;

    ps->RegExtendedLineControl  = 0x6d;
    ps->RegExtendedXStep	    = 0x6e;

    ps->RegPllPredivider	    = 0x71;
    ps->RegPllMaindivider	    = 0x72;
    ps->RegPllPostdivider	    = 0x73;
    ps->RegClockSelector	    = 0x74;
    ps->RegTestMode		        = 0xf0;

	/*
	 * setup function calls
	 */
	ps->SetupScannerVariables  = p12SetupScannerVariables;
	ps->SetupScanningCondition = p12SetupScanningCondition;
    ps->Calibration            = p12Calibration;
    ps->PutToIdleMode          = p12PutToIdleMode;
	ps->ReInitAsic			   = p12Init98003;

	ps->CtrlReadHighNibble  = _CTRL_GENSIGNAL + _CTRL_AUTOLF + _CTRL_STROBE;
	ps->CtrlReadLowNibble   = _CTRL_GENSIGNAL + _CTRL_AUTOLF;

	ps->IO.useEPPCmdMode = _FALSE;

	/*
	 * initialize the other modules and set some
	 * function pointer
	 */
	result = DacInitialize( ps );
	if( _OK != result )
		return result;

	result = ImageInitialize( ps );
	if( _OK != result )
		return result;

	result = IOFuncInitialize( ps );
	if( _OK != result )
		return result;

	result = IOInitialize( ps );
	if( _OK != result )
		return result;

	result = MotorInitialize( ps );
	if( _OK != result )
		return result;

    if( _FALSE == ps->OpenScanPath( ps )) {
    	DBG( DBG_LOW, "P12InitAsic() failed.\n" );
        return _E_NO_DEV;
    }

    /*get CCD and PCB ID */
    ps->Device.bPCBID = IODataFromRegister( ps, ps->RegConfiguration );
    ps->Device.bCCDID = ps->Device.bPCBID & 0x07;
    ps->Device.bPCBID &= 0xf0;

	DBG( DBG_LOW, "PCB-ID=0x%02x, CCD-ID=0x%02x\n", ps->Device.bPCBID, ps->Device.bCCDID );

    /* get a more closer model description...*/
    p12InitiateComponentModel( ps );

    ps->CloseScanPath( ps );

    /* here we check for the OpticWorks 2000, which is not supported */
    if( _OPTICWORKS2000 == ps->Device.bPCBID ) {
    	DBG( DBG_LOW, "OpticWorks 2000 not supported!\n" );
        return _E_NOSUPP;
    }

	DBG( DBG_LOW, "P12InitAsic() done.\n" );
	return _OK;
}

/*.............................................................................
 * set all necessary register contents
 */
_LOC void P12SetGeneralRegister( pScanData ps )
{
	DBG( DBG_LOW, "P12SetGeneralRegister()\n" );

    ps->Scan.fMotorBackward = _FALSE;
    ps->Scan.fRefreshState  = _FALSE;

    if( COLOR_BW == ps->DataInf.wPhyDataType )
    	ps->AsicReg.RD_ScanControl = _SCAN_BITMODE;
    else {
        if( ps->DataInf.wPhyDataType <= COLOR_TRUE24 )
	        ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE;
    	else
	        ps->AsicReg.RD_ScanControl = _SCAN_12BITMODE;
    }

	IOSelectLampSource( ps );

   	if( ps->Shade.bIntermediate & _ScanMode_AverageOut )
    	ps->AsicReg.RD_ModelControl = ps->Device.ModelCtrl | _ModelDpi300;
    else
	    ps->AsicReg.RD_ModelControl = ps->Device.ModelCtrl | _ModelDpi600;

    ps->AsicReg.RD_Motor0Control = _MotorOn | _MotorHQuarterStep | _MotorPowerEnable;
    ps->AsicReg.RD_ScanControl1  = _SCANSTOPONBUFFULL | _MFRC_BY_XSTEP;
    ps->AsicReg.RD_StepControl   = _MOTOR0_SCANSTATE;
}

/* END PLUSTEK-PP_P12.C .....................................................*/
