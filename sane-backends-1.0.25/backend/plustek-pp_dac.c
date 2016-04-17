/* @file plustek-pp_dac.c
 * @brief all the shading function formerly found in shading.c.
 *        don't ask me why I called this file dac.c...
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - no changes
 * - 0.33 - added some comments
 * - 0.34 - slight changes
 * - 0.35 - removed SetInitialGainRAM from structure pScanData
 * - 0.36 - added dacP96001WaitForShading and changed dacP96WaitForShading to
 *          dacP96003WaitForShading
 *        - changes, due to define renaming
 * - 0.37 - removed dacP98FillShadingDarkToShadingRegister()
 *        - removed // comments
 *        - some code cleanup
 * - 0.38 - added P12 stuff
 * - 0.39 - no changes
 * - 0.40 - disabled the A3I stuff
 * - 0.41 - no changes
 * - 0.42 - changed include names
 * - 0.43 - no changes
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

/************************** local definitions ********************************/

/***************************** global vars ***********************************/

static const Byte a_bCorrectTimesTable[4] = {0, 1, 2, 4};

static ULong dwADCPipeLine = 4 * 4;
static ULong dwReadyLen;

/*************************** local functions *********************************/

/**
 */
static void dacP98AdjustGainAverage( pScanData ps )
{
	pUChar pDest, pSrce;
    ULong  dw, dw1;
    UShort wSum;

    pDest = pSrce = ps->pScanBuffer1;

    for (dw1 = 0; dw1 < (2560 * 3) / 16; dw1++, pDest++) {
		for (dw = 0, wSum = 0; dw < 16; dw++, pSrce++)
		    wSum += *pSrce;

		*pDest = wSum / 16;
    }
}

/**
 */
static void dacP98FillDarkDAC( pScanData ps )
{
    IODataRegisterToDAC( ps, 0x20, ps->bRedDAC   );
    IODataRegisterToDAC( ps, 0x21, ps->bGreenDAC );
    IODataRegisterToDAC( ps, 0x22, ps->bBlueDAC  );
}

/**
 */
static void dacP98SetReadFBKRegister( pScanData ps )
{
    IODataToRegister( ps, ps->RegModeControl, _ModeIdle );

    ps->AsicReg.RD_ScanControl = _SCAN_12BITMODE + _SCAN_1ST_AVERAGE;

    IOSelectLampSource( ps );
    IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl );

	ps->AsicReg.RD_Motor0Control = _MotorOn;
	ps->AsicReg.RD_StepControl 	 = _MOTOR0_SCANSTATE;
	ps->AsicReg.RD_Origin 		 = 4;
	ps->AsicReg.RD_Pixels 		 = 512;
	ps->AsicReg.RD_Motor1Control = 0;
	ps->AsicReg.RD_Motor0Control = 0;

	ps->AsicReg.RD_ModelControl  = _LED_CONTROL + _LED_ACTIVITY;

    if( ps->bSetScanModeFlag & _ScanMode_AverageOut ) {
		ps->AsicReg.RD_Dpi = 300;
		ps->AsicReg.RD_ModelControl += _ModelDpi300;

    } else {
		ps->AsicReg.RD_Dpi = 600;
		ps->AsicReg.RD_ModelControl += _ModelDpi600;
    }
}

/**
 */
static UShort dacP98CalDarkOff( pScanData ps, UShort wChDarkOff,
							    UShort wDACCompareHigh, UShort wDACOffset )
{
    UShort wTemp;

    if ((_CCD_518 == ps->Device.bCCDID) || (_CCD_535 == ps->Device.bCCDID)) {
		wTemp = wChDarkOff + wDACOffset;
	} else  {

		if (_CCD_3797 == ps->Device.bCCDID) {
		    if (wChDarkOff > wDACOffset) {
				wTemp = wChDarkOff - wDACOffset;
			} else {
				wTemp = 0;
			}
		} else {
		    if (wChDarkOff > wDACCompareHigh) {
				wTemp = wChDarkOff - wDACCompareHigh;
			} else {
				wTemp = 0;
			}
		}
    }
    return wTemp;
}

/**
 */
static Bool dacP98AdjustDAC( UShort DarkOff, UShort wHigh,
							 UShort wLow, pUChar pbReg, Bool *fDACStopFlag )
{
    if (DarkOff > wHigh) {
		if ((DarkOff - wHigh) > 10) {
		    if ((DarkOff - wHigh) > 2550)
				*pbReg += ((DarkOff - wHigh) / 20);
		    else
			*pbReg += ((DarkOff - wHigh) / 10);
		} else
	    	*pbReg += 1;

		if (!(*pbReg))
		    *pbReg = 0xff;

		*fDACStopFlag = _FALSE;
		return _FALSE;

    } else {
		if (DarkOff < wLow) {
		    if (DarkOff > 0)
				*pbReg -= 2;
		    else
				*pbReg -= 10;

		    *fDACStopFlag = _FALSE;
		    return _FALSE;
		} else
		    return _TRUE;
	}
}

/**
 */
static Bool dacP98CheckChannelDarkLevel( pScanData ps )
{
	Bool fDACStopFlag = _TRUE;

    dacP98AdjustDAC( ps->Shade.DarkOffset.Colors.Red,
                     ps->Shade.pCcdDac->DarkCmpHi.Colors.Red,
                     ps->Shade.pCcdDac->DarkCmpLo.Colors.Red,
			         &ps->bRedDAC, &fDACStopFlag );
    dacP98AdjustDAC( ps->Shade.DarkOffset.Colors.Green,
                     ps->Shade.pCcdDac->DarkCmpHi.Colors.Green,
                     ps->Shade.pCcdDac->DarkCmpLo.Colors.Green,
			         &ps->bGreenDAC, &fDACStopFlag );
    dacP98AdjustDAC( ps->Shade.DarkOffset.Colors.Blue,
                     ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue,
                     ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue,
			         &ps->bBlueDAC, &fDACStopFlag );

    return  fDACStopFlag;
}

/** Average left offset 30, 16 pixels as each color's dark level
 */
static void dacP98FillChannelDarkLevelControl( pScanData ps )
{
    DataPointer p;
	ULong	    dwPos, dw, dwSum;

    if( ps->bSetScanModeFlag & _ScanMode_AverageOut ) {
		dwPos = 0x10 * 3;
	} else {
		dwPos = 0x20 * 2;
	}

    for (p.pw = (pUShort)(ps->pScanBuffer1 + dwPos), dwSum = 0, dw = 16;
	     dw; dw--, p.pw++) {
		dwSum += (ULong)(*p.pw);
	}

	ps->Shade.DarkOffset.Colors.Red = (UShort)(dwSum / 16);

    for (p.pw = (pUShort)(ps->pScanBuffer1 + dwPos + 1024), dwSum = 0, dw = 16;
	    dw; dw--, p.pw++) {
		dwSum += (ULong)(*p.pw);
	}

    ps->Shade.DarkOffset.Colors.Green = (UShort)(dwSum / 16);

    for (p.pw = (pUShort)(ps->pScanBuffer1 + dwPos + 1024 * 2), dwSum = 0, dw = 16;
	    dw; dw--, p.pw++) {
		dwSum += (ULong)(*p.pw);
	}

	ps->Shade.DarkOffset.Colors.Blue = (UShort)(dwSum / 16);
}

/**
 */
static void dacP98AdjustGain( pScanData ps )
{
	DataPointer	p;
	ULong		dw;
	UShort		w;
    Byte		b[3];
    pUChar		pbReg[3];

    dacP98AdjustGainAverage( ps );

    pbReg[0] = &ps->bRedGainIndex;
    pbReg[1] = &ps->bGreenGainIndex;
    pbReg[2] = &ps->bBlueGainIndex;

    for (w = 0, p.pb = ps->pScanBuffer1; w < 3; w++) {

		for (dw = 2560 / 16, b [w] = 0; dw; dw--, p.pb++) {
		    if (b [w] < *p.pb)
				b [w] = *p.pb;
		}
		if (b[w] < _GAIN_LOW) {
		    if ((_GAIN_P98_HIGH - b[w]) < b[w])
				*(pbReg[w]) += 1;
		    else
				*(pbReg[w]) += 4;
		} else {
		    if (b[w] > _GAIN_P98_HIGH)
				*(pbReg[w]) -= 1;
		}
    }
}

/**
 */
static void dacP98CheckLastGain( pScanData ps )
{
    DataPointer p;
 	ULong	    dw;
    UShort	    w;
    Byte	    b[3];
    pUChar	    pbReg[3];

    dacP98AdjustGainAverage( ps );

    pbReg[0] = &ps->bRedGainIndex;
    pbReg[1] = &ps->bGreenGainIndex;
    pbReg[2] = &ps->bBlueGainIndex;

    for (w = 0, p.pb = ps->pScanBuffer1; w < 3; w++) {
		for (dw = 2560 / 16, b [w] = 0; dw; dw--, p.pb++) {
		    if (b[w] < *p.pb)
				b[w] = *p.pb;
		}
	
		if (b[w] > _GAIN_P98_HIGH) {
		    *(pbReg [w]) -= 1;
		}
    }
}

/**
 */
static void dacP98FillGainInitialRestRegister( pScanData ps )
{
	ps->OpenScanPath( ps );

	IODataToRegister( ps, ps->RegThresholdGapControl, ps->AsicReg.RD_ThresholdGapCtrl );
	IODataToRegister( ps, ps->RegModelControl, ps->AsicReg.RD_ModelControl );

	ps->CloseScanPath( ps );
}

/**
 */
static void dacP98SetInitialGainRegister( pScanData ps )
{
	DacP98FillGainOutDirectPort( ps );	   	/* R/G/B GainOut to scanner		*/
    dacP98FillGainInitialRestRegister( ps );/* Model Control2, LED, Correct.*/
}

/** Find the most ideal intensity for each color (RGB)
 */
static void dacP98SetRGBGainRegister( pScanData ps )
{
    IOCmdRegisterToScanner( ps, ps->RegModeControl, _ModeIdle );

	ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE;
    IOSelectLampSource( ps );

    IOCmdRegisterToScanner( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);
    dacP98SetInitialGainRegister( ps );

    ps->AsicReg.RD_ModeControl   = _ModeScan;
    ps->AsicReg.RD_StepControl   = _MOTOR0_SCANSTATE;
    ps->AsicReg.RD_Motor0Control = _MotorOn + _MotorDirForward + _MotorHEightStep;
    ps->AsicReg.RD_XStepTime 	 = ps->bSpeed4;

    if( ps->bSetScanModeFlag & _ScanMode_AverageOut ) {
		ps->AsicReg.RD_ModelControl = _LED_CONTROL + _ModelDpi300;
		ps->AsicReg.RD_Origin = 32 +  60 + 4;
    } else {
		ps->AsicReg.RD_ModelControl = _LED_CONTROL + _ModelDpi600;
		ps->AsicReg.RD_Origin = 64 + 120 + 4;
    }
    ps->AsicReg.RD_Dpi    = 300;
    ps->AsicReg.RD_Pixels = 2560;

	IOPutOnAllRegisters( ps );
}

/**
 */
static void dacP98FillRGBMap( pUChar pBuffer )
{
    ULong  dw, dw1;
	pULong pdw = (pULong)(pBuffer);

    for( dw = 256, dw1 = 0; dw; dw--, dw1 += 0x01010101 ) {
		*pdw++ = dw1;
		*pdw++ = dw1;
		*pdw++ = dw1;
		*pdw++ = dw1;
    }
}

/** here we download the current mapping table
 */
static void dacP98DownloadMapTable( pScanData ps, pUChar pBuffer )
{
    Byte  bAddr;
    ULong i;

    IODataToRegister( ps, ps->RegScanControl,
	   			(Byte)((ps->AsicReg.RD_ScanControl & 0xfc) | _SCAN_BYTEMODE));

    for( i = 3, bAddr = _MAP_ADDR_RED; i--; bAddr += _MAP_ADDR_SIZE ) {

        IODataToRegister( ps, ps->RegModeControl, _ModeMappingMem );
        IODataToRegister( ps, ps->RegMemoryLow, 0);
        IODataToRegister( ps, ps->RegMemoryHigh, bAddr );

    	IOMoveDataToScanner( ps, pBuffer, 4096 );
    	pBuffer += 4096;
    }

    IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl );
}

/**
 */
static void dacP98DownloadShadingTable( pScanData ps,
                                        pUChar pBuffer, ULong size )
{
    IODataToRegister( ps, ps->RegModeControl, _ModeShadingMem );
	IODataToRegister( ps, ps->RegMemoryLow,  0 );
    IODataToRegister( ps, ps->RegMemoryHigh, 0 );

    /* set 12 bits output color */
    IODataToRegister( ps, ps->RegScanControl,
					  (Byte)(ps->AsicReg.RD_ScanControl | _SCAN_12BITMODE));

	/* MoveDataToShadingRam() */
    IOMoveDataToScanner( ps ,pBuffer, size );

    if( _ASIC_IS_98003 == ps->sCaps.AsicID )
        IODataToRegister( ps, ps->RegModeControl, _ModeScan );
    else
        IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl );

    DacP98FillShadingDarkToShadingRegister( ps );
}

/** Build a linear map for asic (this model is 12-bit scanner, there are 4096
 *  map entries but just generate 256 level output, so the content must be 0 to
 *  255), then fill the shading buffer (the first 3k), and map table (the last
 *  1k) to asic for R & G & B channels.
 *
 *  I need pScanBuffer2 size = 5400 * 2 * 3
 *  pScanBuffer1 size = 256 * 16 * 2
 */
static void dacP98SetInitialGainRAM( pScanData ps )
{
    memset( ps->pScanBuffer2, 0xff, (5400 * 2 * 3));

	dacP98DownloadShadingTable( ps, ps->pScanBuffer2, (5400 * 2 * 3));

    dacP98FillRGBMap( ps->pScanBuffer1 );		   	/* Fill 12 Bits R Map */
    dacP98FillRGBMap( ps->pScanBuffer1 + 4096 );    /* Fill 12 Bits G Map */
    dacP98FillRGBMap( ps->pScanBuffer1 + 8192 );    /* Fill 12 Bits B Map */

	dacP98DownloadMapTable( ps, ps->pScanBuffer1 );
}

/** Find the most ideal intensity for each color (RGB)
 */
static void dacP98AdjustRGBGain( pScanData ps )
{
	int bCorrectTimes;

	DBG( DBG_LOW, "dacP98AdjustRGBGain()\n" );

	ps->OpenScanPath( ps );
	dacP98SetInitialGainRAM( ps );	/* set shading ram and read out data to */
    ps->CloseScanPath( ps );

    ps->bRedGainIndex   = 0x02;
	ps->bGreenGainIndex = 0x02;
	ps->bBlueGainIndex  = 0x02;

    for (bCorrectTimes = 10; bCorrectTimes; bCorrectTimes-- ) {

		dacP98SetRGBGainRegister( ps );	   	 /* shading the most brightness &*/
		ps->PauseColorMotorRunStates( ps );	 /* stop scan states			 */
		IOReadOneShadingLine( ps, ps->pScanBuffer1, 2560UL );
		dacP98AdjustGain( ps );
    }

    dacP98SetRGBGainRegister( ps );		/* shading the most brightness &	*/
	ps->PauseColorMotorRunStates( ps ); /* stop scan states                */

    IOReadOneShadingLine( ps, ps->pScanBuffer1, 2560UL );

    dacP98CheckLastGain( ps );
	DacP98FillGainOutDirectPort( ps );
}

/**
 */
static void dacP98SetAdjustShadingRegister( pScanData ps )
{
	DBG( DBG_LOW, "dacP98SetAdjustShadingRegister()\n" );

	IOCmdRegisterToScanner( ps, ps->RegModeControl, _ModeIdle );

	ps->AsicReg.RD_ScanControl = _SCAN_12BITMODE + _SCAN_1ST_AVERAGE;
	IOSelectLampSource( ps );

    IOCmdRegisterToScanner( ps, ps->RegScanControl,
							ps->AsicReg.RD_ScanControl );

	ps->AsicReg.RD_ModeControl 	 = _ModeScan;
	ps->AsicReg.RD_Motor0Control = _MotorOn + _MotorHEightStep + _MotorDirForward;
	ps->AsicReg.RD_XStepTime 	 = ps->bSpeed1;
	ps->AsicReg.RD_ModelControl  = _LED_ACTIVITY + _LED_CONTROL;

    if (ps->bSetScanModeFlag & _ScanMode_AverageOut) {
		ps->AsicReg.RD_Dpi 	  = 300;
		ps->AsicReg.RD_Pixels = 2700;
		ps->AsicReg.RD_ModelControl += _ModelDpi300;
    } else {
		ps->AsicReg.RD_Dpi 	  = 600;
		ps->AsicReg.RD_Pixels = 5400;
		ps->AsicReg.RD_ModelControl += _ModelDpi600;
    }
	ps->AsicReg.RD_Origin = 4;

	IOPutOnAllRegisters( ps );
}

/**
 */
static void dacP98ReadShadingScanLine( pScanData ps )
{
	TimerDef timer;

	MiscStartTimer( &timer, _SECOND );

    ps->Scan.bFifoSelect = ps->RegGFifoOffset;

    while((IOReadFifoLength( ps ) < dwReadyLen) &&
		   !MiscCheckTimer(&timer)) {
		_DO_UDELAY( 1 );
	}

    IOReadColorData( ps, ps->pScanBuffer2, ps->dwShadingLen );
}

/**
 */
static void dacP98GainResize( pUShort pValue, UShort wResize)
{
	DataType Data;
    Byte	 bTemp;

    Data.dwValue = ((ULong) *pValue) * (ULong) wResize / 100;

    if (0x1000 <= Data.dwValue)
		Data.wValue = 0xfff;

    Data.wValue *= 16;

    bTemp = Data.wOverlap.b1st;
    Data.wOverlap.b1st = Data.wOverlap.b2nd;
    Data.wOverlap.b2nd = bTemp;

    *pValue = Data.wValue;
}

/**
 */
static void dacP98SortHilightShadow( pScanData ps, pUShort pwData,
					 			     ULong dwHilightOff, ULong dwShadowOff )
{
	ULong   dwLines, dwPixels;
    UShort  wVCmp, wTmp;
    pUShort pw;

    for (dwPixels = 0; dwPixels < (ps->dwShadingPixels - 4); dwPixels++) {

		pw    = (pUShort)ps->Shade.pHilight + dwHilightOff + dwPixels;
		wVCmp = pwData[dwPixels] & 0xfffU;

		for (dwLines = _DEF_BRIGHTEST_SKIP; dwLines--; pw += 5400UL) {
		    if (wVCmp > *pw) {
				wTmp  = wVCmp;
				wVCmp = *pw;
				*pw   = wTmp;
		    }
		}
    }
    for (dwPixels = 0; dwPixels < (ps->dwShadingPixels - 4); dwPixels++) {

		pw    = ps->pwShadow + dwShadowOff + dwPixels;
		wVCmp = pwData [dwPixels] & 0xfffU;

		for (dwLines = _DEF_DARKEST_SKIP; dwLines--; pw += 5400UL) {

		    if (wVCmp < *pw) {
				wTmp  = wVCmp;
				wVCmp = *pw;
				*pw   = wTmp;
	    	}
		}
    }
}

/**
 */
static void dacP98WriteBackToShadingRAM( pScanData ps )
{
	ULong   dw;

	pUShort	pBuffer = (pUShort)ps->pScanBuffer2;

	DBG( DBG_LOW, "dacP98WriteBackToShadingRAM()\n" );

    if (ps->DataInf.wPhyDataType >= COLOR_TRUE24) {

		for (dw = 0; dw < 5400; dw++) {

		    *pBuffer = ((pUShort)ps->pScanBuffer1)[dw] -
                                            ps->Shade.DarkOffset.Colors.Red;
			dacP98GainResize( pBuffer,
                              ps->Shade.pCcdDac->GainResize.Colors.Red );
		    pBuffer ++;

		    *pBuffer = ((pUShort)ps->pScanBuffer1)[dw + 5400] -
                                            ps->Shade.DarkOffset.Colors.Green;
			dacP98GainResize( pBuffer,
                              ps->Shade.pCcdDac->GainResize.Colors.Green );
		    pBuffer ++;

		    *pBuffer = ((pUShort)ps->pScanBuffer1)[dw + 5400 * 2] -
                                             ps->Shade.DarkOffset.Colors.Blue;
			dacP98GainResize( pBuffer,
                              ps->Shade.pCcdDac->GainResize.Colors.Blue );
		    pBuffer ++;
		}

    } else {
		for (dw = 0; dw < 5400; dw++) {

			DataType Data;
			Byte	 bTemp;

		    *pBuffer = ((pUShort)ps->pScanBuffer1)[dw + 5400] -
                                            ps->Shade.DarkOffset.Colors.Green;

		    Data.wValue = (*pBuffer) * 16;
		    bTemp = Data.wOverlap.b1st;
	    	Data.wOverlap.b1st = Data.wOverlap.b2nd;
		    Data.wOverlap.b2nd = bTemp;
		    *pBuffer = Data.wValue;
		    pBuffer++;
		}
		
    }
	dacP98DownloadShadingTable( ps, ps->pScanBuffer2, (5400 * 2 * 3));
}

/**
 */
static void dacP98ShadingRunLoop( pScanData ps )
{
	int		    i;
	DataPointer p;

    p.pb = ps->a_nbNewAdrPointer;

    switch( ps->IO.portMode ) {
	case _PORT_SPP:
	case _PORT_BIDI:
	    *p.pw++ = 0;
	    for (i = 0; i < 7; i++)
			*p.pdw++ = 0x00800700;
	    *p.pw = 0;
	    break;

	default:
	    *p.pb++ = 0;
	    for (i = 0; i < 15; i++)
			*p.pw++ = 0xf888;
	    *p.pb = 0;
    }

	IOSetToMotorRegister( ps );
}

/**
 */
static void dacP98Adjust12BitShading( pScanData ps )
{
	DataPointer pd, pt;
    ULong		dw, dw1, dwLoop;

	DBG( DBG_LOW, "dacP98Adjust12BitShading()\n" );

    memset( ps->pScanBuffer1, 0, (5400 * 4 * 3));

    if( ps->Shade.pHilight && (_Shading_32Times == ps->bShadingTimeFlag)) {

		memset( ps->Shade.pHilight, 0, (ps->dwHilight * 2UL));

		for (dw = 0; dw < ps->dwShadow; dw++)
		    ps->pwShadow[dw] = 0xfff;
    }

	/*
	 * in the original code this function does not exist !
	 * (of course the code behind the function does ;-)
	 */
	dacP98SetAdjustShadingRegister( ps );

    dacP98ShadingRunLoop( ps );
    _DODELAY( 24 );

    if ((ps->DataInf.dwScanFlag & SCANDEF_TPA ) ||
	  	(_Shading_32Times == ps->bShadingTimeFlag)) {
		dwLoop = 32;
	} else {
		if (_Shading_16Times == ps->bShadingTimeFlag) {
		   dwLoop = 16;
		} else {
		   dwLoop = 4;
		}
    }

    for (dw1 = 0; dw1 < dwLoop; dw1++) {

        ps->Scan.bFifoSelect = ps->RegGFifoOffset;

		dacP98ReadShadingScanLine( ps );

		if((_Shading_32Times == ps->bShadingTimeFlag) && ps->Shade.pHilight ) {

		    dacP98SortHilightShadow( ps, (pUShort)ps->pScanBuffer2, 0, 0 );
	    	dacP98SortHilightShadow( ps, (pUShort)ps->pScanBuffer2 +
												  ps->dwShadingPixels,
						       	     ps->dwHilightCh, ps->dwShadowCh );

		    dacP98SortHilightShadow( ps, (pUShort)ps->pScanBuffer2 +
												  ps->dwShadingPixels * 2,
							       	  ps->dwHilightCh * 2, ps->dwShadowCh * 2);
		}

	    /* SumAdd12BitShadingR */
		pd.pw  = (pUShort)ps->pScanBuffer2;
		pt.pdw = (pULong)(ps->pScanBuffer1 + dwADCPipeLine);

		for (dw = 5400 - 4; dw; dw--, pd.pw++, pt.pdw++)
		    *pt.pdw += (ULong)(*pd.pw & 0x0fff);

	    /* SumAdd10BitShadingG */
		if( ps->bSetScanModeFlag & _ScanMode_AverageOut )
		    pd.pw = (pUShort)(ps->pScanBuffer2 + 2700 * 2);
		else
		    pd.pw = (pUShort)(ps->pScanBuffer2 + 5400 * 2);

		pt.pdw = (pULong)(ps->pScanBuffer1 + 5400 * 4 + dwADCPipeLine);

		for (dw = 5400 - 4; dw; dw--, pd.pw++, pt.pdw++)
	    	*pt.pdw += (ULong)(*pd.pw & 0x0fff);

	    /* SumAdd12BitShadingB */
		if( ps->bSetScanModeFlag & _ScanMode_AverageOut )
		    pd.pw = (pUShort)(ps->pScanBuffer2 + 2700 * 4);
		else
	    	pd.pw = (pUShort)(ps->pScanBuffer2 + 5400 * 4);

		pt.pdw = (pULong)(ps->pScanBuffer1 + 5400 * 8 + dwADCPipeLine);

		for (dw = 5400 - 4; dw; dw--, pd.pw++, pt.pdw++)
		    *pt.pdw += (ULong)(*pd.pw * 94 / 100 & 0x0fff);

		/* one line */
		if (IOReadFifoLength( ps ) <= 2500)	
	    	IORegisterDirectToScanner( ps, ps->RegRefreshScanState );
    }

    TPAP98001AverageShadingData( ps );

	ps->OpenScanPath( ps );
	dacP98WriteBackToShadingRAM( ps );
	ps->CloseScanPath( ps );
}

/**
 */
static Bool dacP98WaitForShading( pScanData ps )
{
    Byte oldLineControl;

	DBG( DBG_LOW, "dacP98WaitForShading()\n" );

	/*	
	 * before getting the shading data, (re)init the ASIC
	 */
    ps->InitialSetCurrentSpeed( ps );
	ps->ReInitAsic( ps, _TRUE );

	IOCmdRegisterToScanner( ps, ps->RegLineControl,
							ps->AsicReg.RD_LineControl );

    ps->Shade.DarkOffset.Colors.Red   = 0;
	ps->Shade.DarkOffset.Colors.Green = 0;
	ps->Shade.DarkOffset.Colors.Blue  = 0;

	/*
	 * according to the scan mode, switch on the lamp
	 */
    IOSelectLampSource( ps );
    IOCmdRegisterToScanner(ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);

    if( ps->bSetScanModeFlag & _ScanMode_AverageOut ) {
		ps->AsicReg.RD_ModelControl = _LED_CONTROL + _ModelDpi300;
	} else {
		ps->AsicReg.RD_ModelControl = _LED_CONTROL + _ModelDpi600;
	}
    IOCmdRegisterToScanner( ps, ps->RegModelControl,
							ps->AsicReg.RD_ModelControl );

    IOCmdRegisterToScanner( ps, ps->RegModeControl, _ModeScan);

    oldLineControl = ps->AsicReg.RD_LineControl;
	IOSetXStepLineScanTime( ps, _DEFAULT_LINESCANTIME );

	/* set line control */
	IOCmdRegisterToScanner( ps, ps->RegLineControl,
							ps->AsicReg.RD_LineControl );

	/* Wait for Sensor to position */
	if( !ps->GotoShadingPosition( ps ))
		return _FALSE;

    ps->AsicReg.RD_LineControl = oldLineControl;
    IOCmdRegisterToScanner( ps, ps->RegLineControl,
							ps->AsicReg.RD_LineControl );

    dwADCPipeLine = 4 * 4;

    if( ps->bSetScanModeFlag & _ScanMode_AverageOut ) {
		dwReadyLen			= 2500;
		ps->dwShadingLen	= 2700 * 2;
		ps->dwShadingPixels	= 2700;
    } else {
		dwReadyLen			= 5000;
		ps->dwShadingLen	= 5400 * 2;
		ps->dwShadingPixels	= 5400;
    }

	dacP98AdjustRGBGain		( ps );
	DacP98AdjustDark		( ps );
	dacP98Adjust12BitShading( ps );

    ps->OpenScanPath( ps );
	DacP98FillShadingDarkToShadingRegister( ps );

	if ( COLOR_BW != ps->DataInf.wPhyDataType )
		dacP98DownloadMapTable( ps, ps->a_bMapTable );

    ps->CloseScanPath( ps );

    return _TRUE;
}

/** Set RAM bank and size, then write the data to it
 */
static void dacP96FillWhole4kRAM( pScanData ps, pUChar pBuf )
{
    ps->OpenScanPath( ps );

	IODataToRegister( ps, ps->RegMemAccessControl,
		    			  ps->Asic96Reg.RD_MemAccessControl );

    ps->AsicReg.RD_ModeControl = _ModeProgram;
    IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl );

    IOMoveDataToScanner( ps, pBuf, ps->ShadingBankSize );

    ps->AsicReg.RD_ModeControl = _ModeScan;
    IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl );

    ps->CloseScanPath( ps );
}

/**
 */
static void	dacP96FillChannelDarkOffset( pScanData ps )
{
    ps->OpenScanPath( ps );

	IODataToRegister( ps, ps->RegRedChDarkOffset,
					  ps->Asic96Reg.RD_RedChDarkOff );
    IODataToRegister( ps, ps->RegGreenChDarkOffset,
					  ps->Asic96Reg.RD_GreenChDarkOff );
    IODataToRegister( ps, ps->RegBlueChDarkOffset,
					  ps->Asic96Reg.RD_BlueChDarkOff );

    ps->CloseScanPath( ps );
}

/**
 */
static void dacP96FillEvenOddControl( pScanData ps )
{
    ps->OpenScanPath( ps );

	IODataToRegister( ps, ps->RegRedChEvenOffset,
					  ps->Asic96Reg.RD_RedChEvenOff );
	IODataToRegister( ps, ps->RegGreenChEvenOffset,
					  ps->Asic96Reg.RD_GreenChEvenOff );
	IODataToRegister( ps, ps->RegBlueChEvenOffset,
					  ps->Asic96Reg.RD_BlueChEvenOff );
	IODataToRegister( ps, ps->RegRedChOddOffset,
					  ps->Asic96Reg.RD_RedChOddOff );
	IODataToRegister( ps, ps->RegGreenChOddOffset,
					  ps->Asic96Reg.RD_GreenChOddOff );
	IODataToRegister( ps, ps->RegBlueChOddOffset,
					  ps->Asic96Reg.RD_BlueChOddOff );

    ps->CloseScanPath( ps );
}

/** Used for capture data to pScanData->pPrescan16.
 *  Used to replace:
 *	1) ReadFBKScanLine (3 * 1024, 5)
 *	2) ReadOneScanLine (3 * 2560, 11)
 *	4) ReadShadingScanLine (3 * 1280, 6)
 */
static void dacP96ReadDataWithinOneSecond( pScanData ps,
										   ULong dwLen, Byte bBlks )
{
	TimerDef timer;

    MiscStartTimer( &timer, _SECOND );

    while((IODataRegisterFromScanner( ps, ps->RegFifoOffset) < bBlks) &&
	   		!MiscCheckTimer(&timer));

    IOReadScannerImageData( ps, ps->pPrescan16, dwLen );
}

/**
 */
static void dacP96GetEvenOddOffset( pUChar pb, pWordVal pwv )
{
	ULong	dw;
    UShort	we, wo;

    for (dw = 8, we = wo = 0; dw; dw-- ) {
		we += *pb++;
		wo += *pb++;
    }

    pwv->b1st = (Byte)(we >> 3);
    pwv->b2nd = (Byte)(wo >> 3);
}

/**
 */
static void dacP96SumAverageShading( pScanData ps, pUChar pDest, pUChar pSrce )
{
	ULong  dw;
	UShort wLeft[6];
	UShort wSumL, wSumR;

    pSrce += ps->Offset70 + ps->Device.DataOriginX;
    pDest += ps->Offset70 + ps->Device.DataOriginX;

    wLeft[0] = wLeft[1] = wLeft[2] =
	wLeft[3] = wLeft[4] = wLeft[5] = (UShort)*pSrce;

    wSumL = wLeft[0] * 6;
    wSumR = (UShort)pSrce[1] + pSrce[2] + pSrce[3] + pSrce[4] +
		            pSrce[5] + pSrce[6];

/*  for (dw = 2772; dw; dw--, pSrce++, pDest++) */
    for (dw = ps->BufferSizePerModel - 6; dw; dw--, pSrce++, pDest++) {

		*pDest = (Byte)(((UShort)*pSrce * 4 + wSumL + wSumR) / 16);
		wSumL  = wSumL - wLeft [0] + (UShort)*pSrce;

		wLeft[0] = wLeft[1];
		wLeft[1] = wLeft[2];
		wLeft[2] = wLeft[3];
		wLeft[3] = wLeft[4];
		wLeft[4] = wLeft[5];
		wLeft[5] = (UShort)*pSrce;
		wSumR = wSumR - (UShort) *(pSrce + 1) + (UShort) *(pSrce + 7);
    }
}

/**
 */
static void dacP96WriteLinearGamma( pScanData ps,
								    pUChar pBuf, ULong dwEntries, Byte bBank )
{
	ULong   dw;
    pULong	pdw = (pULong)(pBuf + ps->ShadingBufferSize);

    for (dw = 0; dwEntries; pdw++, dwEntries--, dw += 0x01010101)
		*pdw = dw;

    ps->Asic96Reg.RD_MemAccessControl = bBank;

    dacP96FillWhole4kRAM( ps, pBuf );
}

/**
 */
static void dacP96FillChannelShadingOffset( pScanData ps )
{
    ps->OpenScanPath( ps );

	IODataToRegister( ps, ps->RegRedChShadingOffset,
					  ps->Asic96Reg.u28.RD_RedChShadingOff);
	IODataToRegister( ps, ps->RegGreenChShadingOffset,
					  ps->Asic96Reg.u29.RD_GreenChShadingOff);
	IODataToRegister( ps, ps->RegBlueChShadingOffset,
					  ps->Asic96Reg.RD_BlueChShadingOff);

    ps->CloseScanPath( ps );
}

/**
 */
static void dacP96GetHilightShadow( pScanData ps,
									pUChar pBuf, pUChar pChOff, pUChar pHigh )
{
 	ULong dw;

	/* GetShadingImageExtent (ps) */
	if (ps->DataInf.wAppDataType < COLOR_256GRAY)
		dw = (ULong)(ps->DataInf.crImage.cx & 0xfff8);
	else
		dw = (ULong)ps->DataInf.crImage.cx;

    for( pBuf += ps->DataInf.crImage.x, *pChOff = 0xff, *pHigh = 0;
		 dw; dw--, pBuf++ ) {

		if (*pChOff < *pBuf) {
		    if (*pHigh < *pBuf)
				*pHigh = *pBuf; /* brightest */
		} else
		    *pChOff = *pBuf;	/* darkest */
    }
}

/**
 */
static void dacP96ReadColorShadingLine( pScanData ps )
{
    Byte	 b2ndDiscard, b3rdDiscard, b1stReadLines, b2ndReadLines;
    Byte	 b3rdReadLines;
	ULong	 dw;
    DataType Data;

    /* ClearScanBuffer1 (ps) */
	/* buffer to keep sum of data */
    memset( ps->pScanBuffer1, 0, ps->BufferForDataRead1 );

    b2ndDiscard   = ps->b2ndLinesOffset;
    b3rdDiscard   = ps->b1stLinesOffset;
    b1stReadLines = b2ndReadLines = b3rdReadLines = 8;

    while( _TRUE ) {

		/* ReadShadingScanLine(ps) */
		dacP96ReadDataWithinOneSecond( ps, ps->ShadingScanLineLen,
					 				   ps->ShadingScanLineBlks );

		if (b1stReadLines) {

		    b1stReadLines--;

	    	/* SumAdd10BitShadingR */
		    for (dw = 0; dw < ps->BufferSizeBase; dw++)
    			((pUShort)ps->pScanBuffer1)[dw] += (UShort)ps->pPrescan16 [dw];
		}

		if (!b2ndDiscard) {
		    if (b2ndReadLines) {
				b2ndReadLines--;
				/* SumAdd10BitShadingG */
				for (dw = ps->BufferSizeBase;dw < (ULong)ps->BufferSizeBase * 2;dw++) {
				    ((pUShort)ps->pScanBuffer1)[dw] +=
													(UShort)ps->pPrescan16[dw];
				}
		    }
		} else
		    b2ndDiscard--;
	
		if (!b3rdDiscard) {
		    if (b3rdReadLines) {
				b3rdReadLines--;
				/* SumAdd10BitShadingB */
				for (dw = ps->BufferSizeBase * 2;
						dw < (ULong)ps->BufferSizeBase * 3; dw++) {
				    ((pUShort)ps->pScanBuffer1)[dw] +=
													(UShort)ps->pPrescan16[dw];
				}
		    } else
				break;
		} else
		    b3rdDiscard--;

		IORegisterDirectToScanner( ps, ps->RegRefreshScanState );
    }

    for (dw = 0; dw < (ULong)ps->BufferSizeBase * 3; dw++) {

		Data.wOverlap.b1st =
		Data.wOverlap.b2nd = (Byte)(((pUShort)ps->pScanBuffer1)[dw] / 8);
		((pUShort)ps->pPrescan16)[dw] = Data.wValue;
    }
}

/**
 */
static void dacP96SetShadingGainProc( pScanData ps, Byte bHigh, ULong dwCh )
{
	Byte	bDark, bGain, bGainX2, bGainX4, bMask;
	pUChar	pbChDark, pbSrce, pbDest;
	ULong	dw;

    pbChDark = NULL, pbSrce = NULL, pbDest = NULL;
    bDark = 0, bGain = 0, bGainX2 = 0, bGainX4 = 0, bMask = 0;

    switch( dwCh ) {

	case 0: 	/* red */
	    pbChDark = &ps->Asic96Reg.u28.RD_RedChShadingOff;
	    pbSrce   = ps->pPrescan16;
	    pbDest   = ps->pScanBuffer1 + ps->Offset70 + ps->Device.DataOriginX;
	    bGainX2  = 1;
	    bGainX4  = 3;
	    bMask    = 0x3c;
	    break;

	case 1: 	/* green */
	    pbChDark = &ps->Asic96Reg.u29.RD_GreenChShadingOff;
	    pbSrce   = ps->pPrescan16 + ps->BufferSizePerModel;
	    pbDest   = ps->pScanBuffer1 + ps->Offset70 + ps->ShadingBankSize +
				   ps->Device.DataOriginX;
	    bGainX2  = 4;
	    bGainX4  = 0x0c;
	    bMask    = 0x33;
	    break;

	case 2:
	    pbChDark = &ps->Asic96Reg.RD_BlueChShadingOff;
	    pbSrce   = ps->pPrescan16 + ps->BufferSizePerModel * 2;
	    pbDest   = ps->pScanBuffer1 + ps->Offset70 + ps->ShadingBankSize * 2 +
				   ps->Device.DataOriginX;
	    bGainX2  = 0x10;
	    bGainX4  = 0x30;
	    bMask    = 0x0f;
    }

    bDark = *pbChDark;
    if ((bHigh -= bDark) > 60U) {
		/* Hilight - Shadow > 60, Quality not so good */
		bGain = bGainX2;		/* Gain x 2 */
		if (bHigh > 120U)
		    bGain = bGainX4;	/* Poor quality, Gain x 4 */
    } else
		bGain = 0;

    ps->Asic96Reg.RD_ShadingCorrectCtrl &= bMask;
    ps->Asic96Reg.RD_ShadingCorrectCtrl |= bGain;

    if (!bGain) {	
		/* GammaGain1 (ps) */
		for (dw = ps->BufferSizePerModel; dw; dw--, pbSrce++, pbDest++)
		    if (*pbSrce > bDark)
				*pbDest = (*pbSrce - bDark) * 4;
	    	else
				*pbDest = 0;
    } else
		if (bGain == bGainX2) {
			/* GammaGain2 */
		    for (dw = ps->BufferSizePerModel; dw; dw--, pbSrce++, pbDest++)
				if (*pbSrce > bDark)
				    *pbDest = (*pbSrce - bDark) * 2;
				else
				    *pbDest = 0;
		} else {
			/* GammaGain4 (ps) */
			memcpy( pbDest, pbSrce, ps->BufferSizePerModel );
		    *pbChDark = 0;
		}
}

/**
 */
static void dacP96FillShadingAndGammaTable( pScanData ps )
{
    ps->Asic96Reg.RD_MemAccessControl = ps->ShadingBankRed;		/* R */
	dacP96FillWhole4kRAM( ps, ps->pPrescan16 );

    ps->Asic96Reg.RD_MemAccessControl = ps->ShadingBankGreen;	/* G */
	dacP96FillWhole4kRAM( ps, ps->pPrescan16 );

    ps->Asic96Reg.RD_MemAccessControl = ps->ShadingBankBlue;	/* B */
	dacP96FillWhole4kRAM( ps, ps->pPrescan16 );
}

/**
 */
static void dacP96SetInitialGainRAM( pScanData ps )
{
    ULong  dw, dw1;
	pULong pdw = (pULong)(ps->pPrescan16 + ps->ShadingBufferSize);

	memset( ps->pPrescan16, 0xff, ps->ShadingBufferSize );

    for (dw = 256, dw1 = 0; dw; dw--, dw1 += 0x01010101, pdw++)
		*pdw = dw1;

	dacP96FillShadingAndGammaTable( ps );
}

/**
 */
static void dacP96Adjust10BitShading( pScanData ps )
{
	pULong	pdw;
	ULong	dw;
	Byte	bRedHigh, bGreenHigh, bBlueHigh;

    /* ShadingMotorRunLoop(ps)
	 * set scan states as:
     *	 40h, 00, 00, 00, 40h, 01, 03, 02, ... (repeat)
     * so, read a R/G/B line every 2 steps
	 */
    pdw = (pULong)ps->a_nbNewAdrPointer;
    for (dw = 0; dw < 4; dw++) {
		*pdw++ = 0x40;
		*pdw++ = 0x02030140;
	}

    dacP96SetInitialGainRAM( ps);	/* initiates the shading buffers and maps */

    /* SetAdjustShadingRegister(ps) */
    /* Prepare Physical/2 dpi, 8.5" scanning condition for reading the shading area */
    ps->AsicReg.RD_ScanControl    = ps->bLampOn | _SCAN_BYTEMODE;
    ps->Asic96Reg.RD_MotorControl = ps->IgnorePF | ps->MotorOn |
									_MotorDirForward;
    ps->AsicReg.RD_ModelControl   = ps->Device.ModelCtrl | _ModelWhiteIs0;
    ps->AsicReg.RD_Dpi    		  = ps->PhysicalDpi / 2;
    ps->AsicReg.RD_Origin 		  = (UShort)(ps->Offset70 +
                                             ps->Device.DataOriginX);
    ps->AsicReg.RD_Pixels 		  = ps->BufferSizeBase;
    IOPutOnAllRegisters( ps );

    ps->Asic96Reg.RD_ShadingCorrectCtrl = _ShadingRCorrectX4|_ShadingGCorrectX4|
									      _ShadingBCorrectX4;
	IOCmdRegisterToScanner( ps, ps->RegShadingCorrectCtrl,
						 			    ps->Asic96Reg.RD_ShadingCorrectCtrl );

    /* Read shaded data and do average */
	dacP96ReadColorShadingLine( ps );

    /* ExpandAndAverage (ps) ------------------------------------------------*/
/*
	 for (dw = 0; dw < 1280 * 3; dw++)
   {
	Data.wOverlap.b1st =
	Data.wOverlap.b2nd = (Byte)(((pUShort) ps->pScanBuffer1)[dw] / 8);
	((pULong) ps->pPrescan16)[dw] = Data.wValue;
  }
*/
	/* CalculateShadingOffset (ps); */
    dacP96GetHilightShadow( ps, ps->pPrescan16,
						&ps->Asic96Reg.u28.RD_RedChShadingOff, &bRedHigh );

    dacP96GetHilightShadow( ps, ps->pPrescan16 + ps->BufferSizePerModel,
			      &ps->Asic96Reg.u29.RD_GreenChShadingOff, &bGreenHigh );

    dacP96GetHilightShadow( ps, ps->pPrescan16 + ps->BufferSizePerModel * 2,
				      &ps->Asic96Reg.RD_BlueChShadingOff, &bBlueHigh );

	/* SubTheImageDataBase (ps) */
	dacP96SetShadingGainProc( ps, bRedHigh,   0 );	/* red	 */
	dacP96SetShadingGainProc( ps, bGreenHigh, 1 );	/* green */
	dacP96SetShadingGainProc( ps, bBlueHigh,  2 );	/* blue	 */
	dacP96FillChannelShadingOffset( ps );

	IOCmdRegisterToScanner( ps, ps->RegShadingCorrectCtrl,
						    ps->Asic96Reg.RD_ShadingCorrectCtrl );

	dacP96SumAverageShading( ps, ps->pPrescan16, ps->pScanBuffer1 );
	dacP96SumAverageShading( ps, ps->pPrescan16 + ps->ShadingBankSize,
			  			     ps->pScanBuffer1 + ps->ShadingBankSize );
	dacP96SumAverageShading( ps, ps->pPrescan16 + ps->ShadingBankSize * 2,
					         ps->pScanBuffer1 + ps->ShadingBankSize * 2 );

    /* --------------------------------------------------------------------- */

    /* PrewriteBackToGammaShadingRAM( ps) */
    dacP96WriteLinearGamma( ps, ps->pPrescan16, 256, ps->ShadingBankRed );
   	dacP96WriteLinearGamma( ps, ps->pPrescan16 + ps->ShadingBankSize, 256,
							ps->ShadingBankGreen );
   	dacP96WriteLinearGamma( ps, ps->pPrescan16 + ps->ShadingBankSize * 2,
    					    256, ps->ShadingBankBlue );
}

/**
 */
static Bool dacP96003WaitForShading( pScanData ps )
{
	int 		bCorrectTimes;
	DataPointer	p;
    ULong		dw;
    UShort		w;
    WordVal 	wv;
	pUChar		pbReg[3];
    Byte		b[3];

	DBG( DBG_LOW, "dacP96003WaitForShading()\n" );

    /* TurnOnLamp () */
	ps->AsicReg.RD_ScanControl |= ps->bLampOn;
	IOCmdRegisterToScanner(ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);

	ps->Asic96Reg.RD_LedControl = _LedActControl | _LedMotorActEnable;
	IOCmdRegisterToScanner(ps, ps->RegLedControl, ps->Asic96Reg.RD_LedControl);

    if ( ps->GotoShadingPosition( ps )) {

    	/* AdjustRGBGain () =================================================*/
		ps->Asic96Reg.RD_RedGainOut  = ps->Asic96Reg.RD_GreenGainOut =
		ps->Asic96Reg.RD_BlueGainOut = 8;
		ps->Asic96Reg.u26.RD_ModelControl2 = _Model2DirectOutPort;

		IOCmdRegisterToScanner(ps, ps->RegModelControl2, _Model2DirectOutPort);

		for ( bCorrectTimes = 4; bCorrectTimes >= 1; bCorrectTimes-- ) {

			ps->PauseColorMotorRunStates( ps );

			/* SetRGBGainRegister () ----------------------------------------*/
		    ps->AsicReg.RD_ScanControl = ps->bLampOn | _SCAN_BYTEMODE;
		    dacP96SetInitialGainRAM( ps );

		    /* SetInitialGainRegister () ++++++++++++++++++++++++++++++++++++*/
		    ps->Asic96Reg.u28.RD_RedChShadingOff	= 		
			ps->Asic96Reg.u29.RD_GreenChShadingOff	=
		    ps->Asic96Reg.RD_BlueChShadingOff		=
		    ps->Asic96Reg.RD_RedChDarkOff		 	=
		    ps->Asic96Reg.RD_GreenChDarkOff			=
		    ps->Asic96Reg.RD_BlueChDarkOff			=
		    ps->Asic96Reg.RD_RedChEvenOff			=
		    ps->Asic96Reg.RD_GreenChEvenOff			=
		    ps->Asic96Reg.RD_BlueChEvenOff			=
		    ps->Asic96Reg.RD_RedChOddOff			=
		    ps->Asic96Reg.RD_GreenChOddOff			=
		    ps->Asic96Reg.RD_BlueChOddOff			= 0;
		    ps->Asic96Reg.RD_ShadingCorrectCtrl = _ShadingRCorrectX4 |
					 					    	  _ShadingGCorrectX4 |
											      _ShadingBCorrectX4;

			dacP96FillChannelShadingOffset( ps );
		    dacP96FillChannelDarkOffset( ps );
			dacP96FillEvenOddControl( ps );

		    /* FillGainOutDirectPort (); */
			ps->OpenScanPath( ps );
			IODataToRegister( ps, ps->RegRedGainOutDirect,
							  ps->Asic96Reg.RD_RedGainOut );
			IODataToRegister( ps, ps->RegGreenGainOutDirect,
							  ps->Asic96Reg.RD_GreenGainOut );
			IODataToRegister( ps, ps->RegBlueGainOutDirect,
							  ps->Asic96Reg.RD_BlueGainOut );

		    /* FillGainInitialRestRegister (); */
		    IODataToRegister( ps, ps->RegModelControl2,
							  ps->Asic96Reg.u26.RD_ModelControl2 );
			IODataToRegister( ps, ps->RegThresholdGapControl,
			 			      ps->AsicReg.RD_ThresholdGapCtrl );
			IODataToRegister( ps, ps->RegLedControl,
							  ps->Asic96Reg.RD_LedControl );

		    IODataToRegister( ps, ps->RegShadingCorrectCtrl,
						      ps->Asic96Reg.RD_ShadingCorrectCtrl );

			ps->CloseScanPath( ps );

		    ps->Asic96Reg.RD_MotorControl = 0;
			IOCmdRegisterToScanner( ps, ps->RegMotorControl, 0 );

			ps->AsicReg.RD_ModeControl    = _ModeScan;
			ps->AsicReg.RD_ScanControl    = ps->bLampOn | _SCAN_BYTEMODE;
			ps->Asic96Reg.RD_MotorControl = (ps->IgnorePF |
										     ps->MotorOn | _MotorDirForward);

			ps->AsicReg.RD_Origin       = 142;
			ps->AsicReg.RD_ModelControl = ps->Device.ModelCtrl | _ModelWhiteIs0;
            ps->AsicReg.RD_Dpi          = ps->PhysicalDpi;
			ps->AsicReg.RD_Pixels       = ps->BufferSizePerModel;

			IOPutOnAllRegisters( ps );

			/*---------------------------------------------------------------*/

		    /* ReadOneScanLine (); */
		    dacP96ReadDataWithinOneSecond( ps, ps->OneScanLineLen, 11 );

			/* AdjustGain () */
			/*	FindTheMaxGain (), AdjustGainOutData (); */

		    pbReg[0] = &ps->Asic96Reg.RD_RedGainOut;
		    pbReg[1] = &ps->Asic96Reg.RD_GreenGainOut;
	    	pbReg[2] = &ps->Asic96Reg.RD_BlueGainOut;

		    for (w = 0, p.pb = ps->pPrescan16; w < 3; w++) {
/* CHANGE: org was:	for (dw = 2560, b[w] = 0; dw; dw--, p.pb++) { */
				for (dw = (ps->OneScanLineLen/3), b[w] = 0; dw; dw--, p.pb++) {

				    if (b[w] < *p.pb)
						b[w] = *p.pb;
				}

				if (b[w] < _GAIN_LOW)
				    *(pbReg[w]) += a_bCorrectTimesTable[bCorrectTimes - 1];
				else
				    if (b[w] > _GAIN_P96_HIGH)
						*(pbReg[w]) -= a_bCorrectTimesTable[bCorrectTimes - 1];
		    }
		}

    	/*===================================================================*/

	    /* SonyFBK ()/ToshibaFBK ()====================================*/
		/*FillRGBDarkLevel0Table (); */
		memset( ps->pPrescan16, 0xff, ps->ShadingBankSize );

		for( dw = 0, p.pb = ps->pPrescan16 + ps->ShadingBufferSize;
                                    		    dw <=255; dw++, p.pb++ ) {
		    *p.pb = (Byte) dw;
		}

		dacP96FillShadingAndGammaTable( ps );

		ps->PauseColorMotorRunStates( ps );

		/* SetReadFBKRegister () */
		ps->Asic96Reg.RD_MotorControl = 0;
		IOCmdRegisterToScanner( ps, ps->RegMotorControl, 0 );

		ps->AsicReg.RD_ModeControl    = _ModeScan;
		ps->AsicReg.RD_ScanControl    = ps->bLampOn | _SCAN_BYTEMODE;
		ps->Asic96Reg.RD_MotorControl = ps->IgnorePF | ps->MotorOn |
								        _MotorDirForward;

		ps->AsicReg.RD_Origin       = 22;
		ps->AsicReg.RD_ModelControl = ps->Device.ModelCtrl | _ModelWhiteIs0;
        ps->AsicReg.RD_Dpi          = ps->PhysicalDpi;
		ps->AsicReg.RD_Pixels       = ps->FBKScanLineLenBase;

		IOPutOnAllRegisters( ps );

		/* ReadFBKScanLine () */
		dacP96ReadDataWithinOneSecond( ps, ps->FBKScanLineLen,
													   ps->FBKScanLineBlks );

	    /*===================================================================*/
		if ( ps->fSonyCCD ) {

		    /* FillChannelDarkLevelControl (); */
		    for ( p.pb = ps->pPrescan16 + 32, w = 0, dw = 16;
				  dw; dw--, p.pb++) {
				  w += (UShort) *p.pb;
			}

		    ps->Asic96Reg.RD_RedChDarkOff = (Byte)(w / 16);

		    for ( p.pb = ps->pPrescan16 + 32 + ps->FBKScanLineLenBase, w = 0, dw = 16;
				  dw; dw--, p.pb++ ) {
				w += (UShort)*p.pb;
			}

			ps->Asic96Reg.RD_GreenChDarkOff = (Byte)(w / 16);

	    	for ( p.pb = ps->pPrescan16 + 32 + ps->FBKScanLineLenBase * 2, w = 0, dw = 16;
				  dw; dw--,	p.pb++) {
				w += (UShort)*p.pb;
			}

		    ps->Asic96Reg.RD_BlueChDarkOff = (Byte)(w / 16);

		    dacP96FillChannelDarkOffset( ps );

		} else {

		    /* FillToshibaDarkLevelControl (); */
			dacP96GetEvenOddOffset( ps->pPrescan16 + 32, &wv );
		    ps->Asic96Reg.RD_RedChEvenOff = wv.b1st;
		    ps->Asic96Reg.RD_RedChOddOff  = wv.b2nd;

			dacP96GetEvenOddOffset( ps->pPrescan16 + 32 + ps->FBKScanLineLenBase, &wv );
		    ps->Asic96Reg.RD_GreenChEvenOff = wv.b1st;
		    ps->Asic96Reg.RD_GreenChOddOff = wv.b2nd;

			dacP96GetEvenOddOffset( ps->pPrescan16 + 32 + ps->FBKScanLineLenBase * 2, &wv );
			ps->Asic96Reg.RD_BlueChEvenOff = wv.b1st;
			ps->Asic96Reg.RD_BlueChOddOff  = wv.b2nd;

			dacP96FillEvenOddControl( ps );
		}

		/*	SetInitialGainRAM (); */
		dacP96Adjust10BitShading( ps );

		return _TRUE;
    }

    return _FALSE;
}

/**
 */
static void dacP96001ToSetShadingAddress( pScanData ps, pUChar pData )
{
    ps->OpenScanPath( ps );

    IODataToRegister( ps, ps->RegMemAccessControl,
											ps->Asic96Reg.RD_MemAccessControl);

    ps->AsicReg.RD_ModeControl = _ModeProgram;
    IODataToRegister( ps, ps->RegModeControl, _ModeProgram );

    ps->Asic96Reg.RD_MotorControl = ps->FullStep | _MotorDirForward;
    IODataToRegister( ps, ps->RegMotorControl, ps->Asic96Reg.RD_MotorControl);

    memset( ps->pScanBuffer1, 0, (64 + 8 + ps->Offset70));
  	memcpy( ps->pScanBuffer1 + 64 + 8 + ps->Offset70, pData,
    												_BUF_SIZE_BASE_CONST * 2 );

    IOMoveDataToScanner( ps, ps->pScanBuffer1,
							_BUF_SIZE_BASE_CONST * 2 + 64 + 8 + ps->Offset70 );

    ps->AsicReg.RD_ModeControl = _ModeScan;
    IODataToRegister( ps, ps->RegModeControl, _ModeScan );

    ps->CloseScanPath( ps );
}

/**
 */
static void dacP96001WriteBackToColorShadingRam( pScanData ps )
{
    ps->Asic96Reg.RD_MemAccessControl = (_MemBankSize4k96001 | 0x3a);
    dacP96001ToSetShadingAddress( ps, ps->pPrescan16 );

    ps->Asic96Reg.RD_MemAccessControl = (_MemBankSize4k96001 | 0x3e);
    dacP96001ToSetShadingAddress( ps, ps->pPrescan16 + _BUF_SIZE_BASE_CONST*2);

    ps->Asic96Reg.RD_MemAccessControl = (_MemBankSize4k96001 | 0x3c);
    dacP96001ToSetShadingAddress( ps, ps->pPrescan16 + _BUF_SIZE_BASE_CONST*4);
}

/**
 */
static void dacP96001ModifyShadingColor( pByte pData, Byte bMul )
{
    UShort w;
	ULong  dw;

    for ( dw = 0; dw < _BUF_SIZE_BASE_CONST * 2; dw++ ) {

		w = (UShort)(Byte)(~pData[dw]) * bMul / 100U;

		if (w >= 255U)
	    	pData[dw] = 0;
		else
		    pData[dw] = (Byte)~w;
    }
}

/**
 */
static Byte dacP96001FBKReading( pScanData ps, Byte bValue,
								 Byte bReg, pByte pbSave, Bool bFBKModify )
{
    TimerDef timer;
    UShort	 w, wSum;
    Byte	 addrSeq[8] = { 0x40, 0x20, 0x10, 0x08, 4, 2, 1, 0 };
    Byte	 bTemp, bFBKTemp, bFBKIndex;

    if( bFBKModify ) {
		bFBKIndex = 3;
		bFBKTemp = *pbSave;
    } else {
		bFBKTemp = 0x80;
		bFBKIndex = 0;
    }

    while( _TRUE ) {

		*pbSave = bFBKTemp;
		IOCmdRegisterToScanner( ps, bReg, bFBKTemp );

		/* SetColorRunTable (BYTE) */
		memset( ps->a_nbNewAdrPointer, bValue, _SCANSTATE_BYTES );
		MotorSetConstantMove( ps, 0 );

		/* SetReadFBK (pScanData) */
		ps->Asic96Reg.RD_MotorControl = ps->FullStep | _MotorDirForward;
		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
										ps->Asic96Reg.RD_MotorControl );

		ps->AsicReg.RD_ModeControl  = _ModeScan;
		ps->AsicReg.RD_ScanControl  = _SCAN_BYTEMODE | ps->bLampOn;
		ps->AsicReg.RD_ModelControl =
					(_ModelMemSize32k96001 | _ModelDpi300 | _ModelWhiteIs0 );

		ps->AsicReg.RD_Dpi    = 300;
		ps->AsicReg.RD_Origin = 22;
		ps->AsicReg.RD_Pixels = 1024;
		IOPutOnAllRegisters( ps );

		ps->Asic96Reg.RD_MotorControl =
						  	   (ps->FullStep | ps->MotorOn | _MotorDirForward);
		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
											   ps->Asic96Reg.RD_MotorControl );

		MiscStartTimer( &timer, _SECOND );
		while((IODataRegisterFromScanner( ps, ps->RegFifoOffset) < 1) &&
													!MiscCheckTimer( &timer ));

		IOCmdRegisterToScanner( ps, ps->RegMotorControl, 0 );
		IOReadScannerImageData( ps, ps->pScanBuffer1, 64 );

		for( w = 26, wSum = 0; w < 42; w++)
		    wSum += ps->pScanBuffer1[w];

		wSum >>= 4;
		bTemp = addrSeq[bFBKIndex++];

		if( bTemp ) {
		    if( wSum < 0xfe )
				bFBKTemp += bTemp;
		    else
				bFBKTemp -= bTemp;
		} else {
		    return bFBKTemp;
		}
    }
}

/**
 */
static Bool dacP96001WaitForShading( pScanData ps )
{
	Bool   bFBKModify;
    Byte   bRSave;
    Byte   bGSave;
    Byte   bBSave;
    ULong  dw;
    pULong pdw;

	DBG( DBG_LOW, "dacP96001WaitForShading()\n" );

    ps->AsicReg.RD_ScanControl |= ps->bLampOn;
    IOCmdRegisterToScanner(ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);

    if ( ps->GotoShadingPosition( ps )) {

		_DODELAY( 250 );

	    /* AdjustMostWideOffset70 (pScanData) -------------------------------*/
		/* FillABitGray (pScanData)*/
		memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
		ps->a_nbNewAdrPointer[8]  =
		ps->a_nbNewAdrPointer[24] = 0x30;

		MotorSetConstantMove( ps, 32 );

		/* SetMaxWideRegister (pScanData) */
		ps->AsicReg.RD_ModeControl = _ModeScan;
		ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE | ps->bLampOn;

		ps->Asic96Reg.RD_MotorControl =
 							  (ps->MotorOn | ps->IgnorePF | _MotorDirForward);
		ps->AsicReg.RD_ModelControl =
					  (_ModelMemSize32k96001 | _ModelDpi300 | _ModelWhiteIs0);

		ps->AsicReg.RD_Dpi    = 300;
		ps->AsicReg.RD_Origin = 64 + 8;
		ps->AsicReg.RD_Pixels = 2700;
		IOPutOnAllRegisters( ps );

		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
                                ps->Asic96Reg.RD_MotorControl );

		/* ReadMaxWideLine */
		dacP96ReadDataWithinOneSecond( ps, 2700, 5 );

	    /* AdjustOffset70Proc (pScanData)------------------------------------*/
		{
		    ULong dwSum, dw, dwLeft, dwCenter;

		    /* AverageWideBank (pScanData) */
		    for( dwSum = 0, dw = 0; dw < 2700; dw++ )
				dwSum += (ULong)ps->pPrescan16[dw];
	
			dwSum /= 2700UL;

		    if( dwSum <= 0x80 ) {

				memcpy( ps->pScanBuffer1, ps->pPrescan16, 140 );
				memcpy( ps->pScanBuffer1 + 140,
       						ps->pPrescan16 + _BUF_SIZE_BASE_CONST * 2, 140);

				/* BlackOffsetCheck (pScanData) */
				for( dw = dwLeft = 0; dw < 140; dw++ ) {
				    if( ps->pScanBuffer1[dw] >= 0xe0 )
						dwLeft++;
				    else
						break;
				}

				/* WhiteOffsetCheck (pScanData) */
				for( dw = 140, dwCenter = 0; dw < 280; dw++ ) {
				    if( ps->pScanBuffer1[dw] < 0xe0 )
						dwCenter++;
				    else
						break;
				}

				if (dwLeft) {
				    ps->Offset70 = (dwLeft + dwCenter) / 2 + 14;
				} else {
				    if (dwCenter == 140)
						ps->Offset70 = 70;
		    		else
						ps->Offset70 = dwCenter / 2 + 2;
				}
	    	}
		}

		memset( ps->pPrescan16, 0, ps->BufferSizePerModel * 3 );
		dacP96001WriteBackToColorShadingRam( ps );

		/* SetFBK */
		if((IODataRegisterFromScanner(ps, ps->RegReadIOBufBus) & 0x0f) == 0x0f)
	    	bFBKModify = 0;
		else
		    bFBKModify = 1;

		dacP96001FBKReading(ps, 0x10, ps->RegRedDCAdjust,  &bRSave,bFBKModify);
		dacP96001FBKReading(ps, 0x30, ps->RegGreenDCAdjust,&bGSave,bFBKModify);
		dacP96001FBKReading(ps, 0x20, ps->RegBlueDCAdjust, &bBSave,bFBKModify);

		ps->OpenScanPath( ps );
		IODataToRegister( ps, ps->RegRedDCAdjust,   (Byte)(bRSave + 2));
		IODataToRegister( ps, ps->RegGreenDCAdjust, (Byte)(bGSave + 2));
		IODataToRegister( ps, ps->RegBlueDCAdjust,  bBSave);
		ps->CloseScanPath( ps );

		/* Turn off and then turn on motor */
		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
				         (Byte)(ps->Asic96Reg.RD_MotorControl & ~ps->MotorOn));
		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
											   ps->Asic96Reg.RD_MotorControl );

		/* FillABitColor (pScanData) */
		pdw = (pULong)ps->a_nbNewAdrPointer;
		for( dw = 0; dw < 4; dw++) {
		    *pdw++ = 0x40;
		    *pdw++ = 0x2030140;
		}

		IOSetToMotorRegister( ps );

		/* SetShadingRegister (pScanData) */
		ps->Asic96Reg.RD_MotorControl = ps->FullStep | _MotorDirForward;
		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
												ps->Asic96Reg.RD_MotorControl );
		ps->AsicReg.RD_ModeControl    = _ModeScan;
		ps->AsicReg.RD_LineControl    = ps->TimePerLine;
		ps->AsicReg.RD_ScanControl    = _SCAN_BYTEMODE | ps->bLampOn;
		ps->Asic96Reg.RD_MotorControl = ps->FullStep | _MotorDirForward;

		ps->AsicReg.RD_ModelControl =
						(_ModelMemSize32k96001 | _ModelDpi300 | _ModelWhiteIs0);
		ps->AsicReg.RD_Dpi    = 150;
		ps->AsicReg.RD_Origin = (UShort)(64 + 8 + ps->Offset70);
		ps->AsicReg.RD_Pixels = ps->BufferSizeBase;
		IOPutOnAllRegisters( ps );

		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
					    (Byte)(ps->MotorOn | ps->IgnorePF | _MotorDirForward));

		dacP96ReadColorShadingLine( ps );

		/* ModifyShadingColor (pScanData) */
		dacP96001ModifyShadingColor( ps->pPrescan16, 103 );
		dacP96001ModifyShadingColor( ps->pPrescan16 +
											_BUF_SIZE_BASE_CONST * 2 * 2, 97);
		dacP96001WriteBackToColorShadingRam( ps );
		return _TRUE;
    }
    return _FALSE;
}

/**
 */
static void dacP98003GainOffsetToDAC( pScanData ps, Byte ch, Byte reg, Byte d )
{
    if( ps->Device.bDACType == _DA_SAMSUNG8531 ) {

    	IODataToRegister( ps, ps->RegADCAddress, 0 );
    	IODataToRegister( ps, ps->RegADCData, ch );
    	IODataToRegister( ps, ps->RegADCSerialOutStr, ch);
    }

    IODataToRegister( ps, ps->RegADCAddress, reg );
    IODataToRegister( ps, ps->RegADCData, d );
    IODataToRegister( ps, ps->RegADCSerialOutStr, d );
}

/**
 */
static void dacP98003AdjustRGBGain( pScanData ps )
{
    ULong i;
    Byte  bHi[3];

    DBG( DBG_LOW, "dacP98003AdjustRGBGain()\n" );

    ps->Shade.Gain.Colors.Red   =
    ps->Shade.Gain.Colors.Green =
    ps->Shade.Gain.Colors.Blue  =  ps->Shade.bUniGain;

    ps->Shade.Hilight.Colors.Red   =
    ps->Shade.Hilight.Colors.Green =
    ps->Shade.Hilight.Colors.Blue  = 0;

    ps->Shade.bGainHigh = _GAIN_P98003_HIGH;
    ps->Shade.bGainLow  = _GAIN_P98003_LOW;

    ps->Shade.fStop = _FALSE;

    for( i = 10; i-- && !ps->Shade.fStop; ) {

        ps->Shade.fStop = _TRUE;

        /* SetRGBGainRegister () */
        IODataToRegister( ps, ps->RegModeControl, _ModeIdle );

        ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE;
        IOSelectLampSource( ps );
    	IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl );

        DacP98003FillToDAC( ps, &ps->Device.RegDACGain, &ps->Shade.Gain );

        ps->AsicReg.RD_ModeControl   = _ModeScan;
        ps->AsicReg.RD_StepControl   = _MOTOR0_SCANSTATE;
        ps->AsicReg.RD_Motor0Control = _FORWARD_MOTOR;

    	if( ps->Shade.bIntermediate & _ScanMode_AverageOut )
	        ps->AsicReg.RD_Origin = (UShort)ps->Device.DataOriginX >> 1;
    	else
	        ps->AsicReg.RD_Origin = (UShort)ps->Device.DataOriginX;

        ps->AsicReg.RD_Dpi    = 300;
        ps->AsicReg.RD_Pixels = 2560;

    	/* PauseColorMotorRunStates () */
    	memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
	    ps->a_nbNewAdrPointer[1] = 0x77;	

	    IOPutOnAllRegisters( ps );

    	_DODELAY( 70 );

        /* read one shading line and work on it */
	    if( IOReadOneShadingLine( ps, (pUChar)ps->Bufs.b1.pShadingRam, 2560)) {

        	if ( ps->DataInf.wPhyDataType <= COLOR_256GRAY ) {

        		bHi[1] = DacP98003SumGains(
                                 (pUChar)ps->Bufs.b1.pShadingRam + 2560, 2560);
		        if( bHi[1] )
        		    DacP98003AdjustGain( ps, _CHANNEL_GREEN, bHi[1] );
		        else {
        		    ps->Shade.fStop = _FALSE;
		        }
    	    } else {
        		bHi[0] = DacP98003SumGains((pUChar)ps->Bufs.b1.pShadingRam, 2560);
		        bHi[1] = DacP98003SumGains((pUChar)ps->Bufs.b1.pShadingRam + 2560, 2560);
        		bHi[2] = DacP98003SumGains((pUChar)ps->Bufs.b1.pShadingRam + 5120, 2560);

		        if (!bHi[0] || !bHi[1] || !bHi[2] ) {
        		    ps->Shade.fStop = _FALSE;
		        } else {
        		    DacP98003AdjustGain( ps, _CHANNEL_RED,   bHi[0] );
        		    DacP98003AdjustGain( ps, _CHANNEL_GREEN, bHi[1] );
        		    DacP98003AdjustGain( ps, _CHANNEL_BLUE,  bHi[2] );
		        }
    	    }
	    } else
    	    ps->Shade.fStop = _FALSE;
    }

#ifdef DEBUG
    if( !ps->Shade.fStop )
        DBG( DBG_LOW, "dacP98003AdjustRGBGain() - all loops done!!!\n" );
#endif

    DacP98003FillToDAC( ps, &ps->Device.RegDACGain, &ps->Shade.Gain );
}

/**
 */
static UShort dacP98003SumDarks( pScanData ps, pUShort data )
{
    UShort i, loop;

    if( ps->Device.bCCDID == _CCD_3799 ) {
    	if( ps->Shade.bIntermediate & _ScanMode_AverageOut )
    	    data += 0x18;
	    else
	        data += 0x30;
    } else {
    	if( ps->Shade.bIntermediate & _ScanMode_AverageOut )
	        data += 0x18;
    	else
	        data += 0x20;
    }

    for( i = 0, loop = 16; loop--; data++ )
    	i += *data;
    i >>= 4;

    return i;
}

/**
 */
static void dacP98003AdjustShadingWaveform( pScanData ps )
{
    Byte	       b;
    UShort         count, wR, wG, wB, tmp;
    DataType       var;
    DataPointer    pvar, psum;
    RBGPtrDef      cp;
    pRGBUShortDef  pRGB, pwsum;

    DBG( DBG_LOW, "dacP98003AdjustShadingWaveForm()\n" );

    memset( &cp, 0, sizeof(RBGPtrDef));
    memset( ps->Bufs.b2.pSumBuf, 0, (5400 * 3 * 2));

    /* SetAdjustShadingRegister () */
    IODataToRegister( ps, ps->RegModeControl, _ModeIdle );

    ps->AsicReg.RD_LineControl    = _LOBYTE(ps->Shade.wExposure);
    ps->AsicReg.RD_ExtLineControl = _HIBYTE(ps->Shade.wExposure);
    IODataToRegister( ps, ps->RegExtendedLineControl,
                      ps->AsicReg.RD_ExtLineControl );
    IODataToRegister( ps, ps->RegLineControl, ps->AsicReg.RD_LineControl );

    ps->AsicReg.RD_XStepTime    = _LOBYTE(ps->Shade.wExposure);
    ps->AsicReg.RD_ExtXStepTime = _HIBYTE(ps->Shade.wExposure);
    IODataToRegister( ps, ps->RegExtendedXStep, ps->AsicReg.RD_ExtXStepTime );
    IODataToRegister( ps, ps->RegXStepTime, ps->AsicReg.RD_XStepTime );

    ps->AsicReg.RD_ModeControl   = _ModeScan;
    ps->AsicReg.RD_StepControl   = _MOTOR0_SCANSTATE;
    ps->AsicReg.RD_Motor0Control = _FORWARD_MOTOR;

   	if( ps->Shade.bIntermediate & _ScanMode_AverageOut ) {

    	ps->AsicReg.RD_Dpi     = 300;
    	ps->AsicReg.RD_Pixels  = 2700;
    	ps->Shade.shadingBytes = 2700 * 2;
    } else {
    	ps->AsicReg.RD_Dpi     = 600;
    	ps->AsicReg.RD_Pixels  = 5400;
    	ps->Shade.shadingBytes = 5400 * 2;
    }
    ps->AsicReg.RD_Origin = _SHADING_BEGINX;

    for( pvar.pdw = (pULong)ps->a_nbNewAdrPointer,
         var.dwValue = _SCANSTATE_BYTES >> 2; var.dwValue--; pvar.pdw++) {
    	*pvar.pdw = 0x00f00080;
    }

    ps->Scan.fRefreshState = _FALSE;
    IOPutOnAllRegisters( ps );
    _DODELAY( 55 );

    /* SetupHilightShadow () */
    if( ps->Shade.pHilight ) {

        memset( ps->Shade.pHilight, 0,
    	        ps->Shade.shadingBytes * ps->Shade.skipHilight * 3 );

        memset((pUChar)ps->Shade.pHilight +
                ps->Shade.shadingBytes * ps->Shade.skipHilight * 3, 0xff,
			    ps->Shade.shadingBytes * ps->Shade.skipShadow * 3 );
    }


    for( count = 32; count--;) {

        IOReadOneShadingLine( ps,
                             ((pUChar)ps->Bufs.b1.pShadingRam)+_SHADING_BEGINX,
	                                                     ps->Shade.shadingBytes );

	    /* SaveHilightShadow() */
    	if( ps->Shade.pHilight ) {

           	if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

        		cp.red.usp   = ps->Bufs.b1.pShadingRam + _SHADING_BEGINX;
		        cp.green.usp = cp.red.usp   + ps->AsicReg.RD_Pixels;
        		cp.blue.usp  = cp.green.usp + ps->AsicReg.RD_Pixels;
		        pvar.pusrgb = (pRGBUShortDef)ps->Shade.pHilight +
                                                         _SHADING_BEGINX;

        		for( var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                              var.dwValue--;) {
        		    pRGB = pvar.pusrgb++;
		            wR = *cp.red.usp;
        		    wG = *cp.green.usp;
		            wB = *cp.blue.usp;

        		    for( b = ps->Shade.skipHilight; b--;
                                               pRGB += ps->AsicReg.RD_Pixels) {

            			if( wR > pRGB->Red ) {
            			    tmp = wR;
			                wR  = pRGB->Red;
            			    pRGB->Red = tmp;
			            }
            			if( wG > pRGB->Green ) {
            			    tmp = wG;
			                wG  = pRGB->Green;
            			    pRGB->Green = tmp;
			            }
            			if( wB > pRGB->Blue ) {
            			    tmp = wB;
			                wB  = pRGB->Blue;
            			    pRGB->Blue = tmp;
			            }
        		    }

        		    wR = *cp.red.usp++;
		            wG = *cp.green.usp++;
        		    wB = *cp.blue.usp++;
		            for(b = ps->Shade.skipShadow; b--;
                                               pRGB += ps->AsicReg.RD_Pixels) {

            			if (wR < pRGB->Red) {
            			    tmp = wR;
			                wR  = pRGB->Red;
            			    pRGB->Red = tmp;
			            }
            			if (wG < pRGB->Green) {
            			    tmp = wG;
			                wG  = pRGB->Green;
            			    pRGB->Green = tmp;
			            }
            			if (wB < pRGB->Blue) {
            			    tmp = wB;
			                wB  = pRGB->Blue;
            			    pRGB->Blue = tmp;
			            }
        		    }
    	        }

    	    } else {

        		cp.green.usp = ps->Bufs.b1.pShadingRam +
                               ps->AsicReg.RD_Pixels + _SHADING_BEGINX;
        		cp.blue.usp  = (pUShort) ps->Shade.pHilight + _SHADING_BEGINX;

        		for (var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                              var.dwValue--;) {
        		    cp.red.usp = cp.blue.usp++;
		            wG = *cp.green.usp;
        		    for( b = ps->Shade.skipHilight; b--;
	                                    cp.red.usp += ps->AsicReg.RD_Pixels) {
            			if( wG > *cp.red.usp ) {
            			    tmp = wG;
			                wG  = *cp.red.usp;
            			    *cp.red.usp = tmp;
			            }
        		    }
		            wG = *cp.green.usp++;
        		    for (b = ps->Shade.skipShadow; b--;
                  		                cp.red.usp += ps->AsicReg.RD_Pixels) {
            			if( wG < *cp.red.usp ) {
            			    tmp = wG;
			                wG  = *cp.red.usp;
            			    *cp.red.usp = tmp;
			            }
        		    }
		        }
    	    }
	    }

    	/* AddToSumBuffer() */
       	if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

    	    cp.red.usp   = ps->Bufs.b1.pShadingRam + _SHADING_BEGINX;
	        cp.green.usp = cp.red.usp   + ps->AsicReg.RD_Pixels;
	        cp.blue.usp  = cp.green.usp + ps->AsicReg.RD_Pixels;

    	    pvar.pulrgb = (pRGBULongDef)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;

	        for( var.dwValue = (ULong)ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
     	         var.dwValue--;
                 pvar.pulrgb++, cp.red.usp++, cp.green.usp++, cp.blue.usp++) {
        		pvar.pulrgb->Red   += (ULong)*cp.red.usp;
		        pvar.pulrgb->Green += (ULong)*cp.green.usp;
        		pvar.pulrgb->Blue  += (ULong)*cp.blue.usp;
	        }

    	} else {

    	    cp.green.usp = ps->Bufs.b1.pShadingRam + ps->AsicReg.RD_Pixels +
                                                               _SHADING_BEGINX;
	        pvar.pdw  = (pULong)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;
	        for(var.dwValue = (ULong)ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                            	    var.dwValue--; pvar.pdw++, cp.green.usp++) {
		        *pvar.pdw += (ULong)*cp.green.usp;
    	    }
        }

   	    if( IOReadFifoLength( ps ) < ps->AsicReg.RD_Pixels )
           IORegisterToScanner( ps, ps->RegRefreshScanState );
    }

    /* AverageAfterSubHilightShadow() */
    if( ps->Shade.pHilight ) {

        if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

      	    psum.pulrgb = (pRGBULongDef)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;
	        pwsum       = (pRGBUShortDef)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;
	        pvar.pusrgb = (pRGBUShortDef)ps->Shade.pHilight + _SHADING_BEGINX;

      	    for( var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                              var.dwValue--;) {
           		pRGB = pvar.pusrgb++;

	            for( b = ps->Shade.skipHilight + ps->Shade.skipShadow;
                                        b--; pRGB += ps->AsicReg.RD_Pixels ) {

           		    psum.pulrgb->Red   -= (ULong)pRGB->Red;
    	            psum.pulrgb->Green -= (ULong)pRGB->Green;
           		    psum.pulrgb->Blue  -= (ULong)pRGB->Blue;
	            }

           		pwsum->Red   = (UShort)(psum.pulrgb->Red   / ps->Shade.dwDiv);
    	        pwsum->Green = (UShort)(psum.pulrgb->Green / ps->Shade.dwDiv);
           		pwsum->Blue  = (UShort)(psum.pulrgb->Blue  / ps->Shade.dwDiv);
       	    	psum.pulrgb++;
	            pwsum++;
   	        }
        } else {
       	    cp.green.ulp = (pULong)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;
            cp.blue.usp  = (pUShort)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;
   	        pvar.pw      = (pUShort)ps->Shade.pHilight  + _SHADING_BEGINX;

       	    for( var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                              var.dwValue--;) {
           		cp.red.usp = pvar.pw++;

    	        for( b = ps->Shade.skipHilight + ps->Shade.skipShadow;
                                     b--; cp.red.usp += ps->AsicReg.RD_Pixels )
   		            *cp.green.ulp -= *cp.red.usp;

           		*cp.blue.usp = (UShort)(*cp.green.ulp / ps->Shade.dwDiv);
           		cp.blue.usp++;
	            cp.green.ulp++;
       	    }
        }
    } else {
       	if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

   	        psum.pulrgb = (pRGBULongDef)ps->Bufs.b2.pSumBuf  + _SHADING_BEGINX;
            pwsum       = (pRGBUShortDef)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;

   	        for (var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                              var.dwValue--;) {
           		pwsum->Red   = (UShort)(psum.pulrgb->Red   >> 5);
    	        pwsum->Green = (UShort)(psum.pulrgb->Green >> 5);
           		pwsum->Blue  = (UShort)(psum.pulrgb->Blue  >> 5);
	            psum.pulrgb++;
       		    pwsum++;
       	    }
        } else {
   	        cp.green.ulp = (pULong)ps->Bufs.b2.pSumBuf  + _SHADING_BEGINX;
            cp.blue.usp  = (pUShort)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;

   	        for (var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                              var.dwValue--;) {
           		*cp.blue.usp = (UShort)(*cp.green.ulp >> 5);
           		cp.blue.usp++;
       	    	cp.green.ulp++;
       	    }
        }
    }

    /* Process negative & transparency here */
    if( ps->DataInf.dwScanFlag & SCANDEF_TPA )
   	    TPAP98003FindCenterPointer( ps );

    if( ps->DataInf.dwScanFlag & SCANDEF_Negative )
		TPAP98003Reshading( ps );

    pRGB = (pRGBUShortDef)&ps->Shade.pCcdDac->GainResize;

   	if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

        	pwsum = (pRGBUShortDef)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;

       	    for( var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                            var.dwValue--;) {

  	        if ((short)(pwsum->Red -= ps->Shade.DarkOffset.Colors.Red) > 0) {
       	    	pwsum->Red = pwsum->Red * pRGB->Red / 100U;
	            if( pwsum->Red > 0xfff )
       		        pwsum->Red = 0xfff;
       	    } else
               	pwsum->Red = 0;

           	if((short)(pwsum->Green -= ps->Shade.DarkOffset.Colors.Green) > 0) {
               	pwsum->Green = pwsum->Green * pRGB->Green / 100U;
           	    if( pwsum->Green > 0xfff )
               	    pwsum->Green = 0xfff;
           	} else
               	pwsum->Green = 0;

       	    if ((short)(pwsum->Blue -= ps->Shade.DarkOffset.Colors.Blue) > 0) {
           	    pwsum->Blue = pwsum->Blue * pRGB->Blue / 100U;
               	if( pwsum->Blue > 0xfff )
    	            pwsum->Blue = 0xfff;
           	} else
               	pwsum->Blue = 0;

           	wR = (UShort)(pwsum->Red >> 4);
         	pwsum->Red <<= 12;
   	        pwsum->Red |= wR;
            wR = (UShort)(pwsum->Green >> 4);
       	    pwsum->Green <<= 12;
   	        pwsum->Green |= wR;
       	    wR = (UShort)(pwsum->Blue>> 4);
       	    pwsum->Blue <<= 12;
   	        pwsum->Blue |= wR;
       	    pwsum++;
       	}
   } else {

       	cp.green.usp = (pUShort)ps->Bufs.b2.pSumBuf + _SHADING_BEGINX;

   	    for( var.dwValue = ps->AsicReg.RD_Pixels - _SHADING_BEGINX;
                                                            var.dwValue--;) {

   	        if((short)(*cp.green.usp -= ps->Shade.DarkOffset.Colors.Green) > 0) {

           		*cp.green.usp = *cp.green.usp * pRGB->Green / 100U;
           		if( *cp.green.usp > 0xfff )
           		    *cp.green.usp = 0xfff;
            } else
           		*cp.green.usp = 0;

       	    wR = (UShort)(*cp.green.usp >> 4);
   	        *cp.green.usp <<= 12;
       	    *cp.green.usp |= wR;

       	    cp.green.usp++;
        }
    }

    /* DownloadShadingAndSetDark() */
    dacP98DownloadShadingTable( ps, ps->Bufs.b2.pSumBuf, (5400 * 3 * 2));
}

/**
 */
static void dacP98003AdjustDark( pScanData ps )
{
    ULong  i;
    UShort wDarks[3];

    DBG( DBG_LOW, "dacP98003AdjustDark()\n" );

    ps->Shade.DarkDAC.Colors = ps->Shade.pCcdDac->DarkDAC.Colors;
    ps->Shade.fStop = _FALSE;

    for( i = 16; i-- && !ps->Shade.fStop;) {

    	ps->Shade.fStop = _TRUE;

    	/* FillDarkToDAC() */
        DacP98003FillToDAC( ps, &ps->Device.RegDACOffset, &ps->Shade.DarkDAC );

        IODataToRegister( ps, ps->RegModeControl, _ModeIdle );

        ps->AsicReg.RD_ScanControl = (_SCAN_12BITMODE + _SCAN_1ST_AVERAGE);
        IOSelectLampSource( ps );
    	IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl );

        ps->AsicReg.RD_StepControl   = _MOTOR0_SCANSTATE;
        ps->AsicReg.RD_Motor0Control = _FORWARD_MOTOR;

        ps->AsicReg.RD_Origin = _SHADING_BEGINX;
        ps->AsicReg.RD_Pixels = 512;

    	if( ps->Shade.bIntermediate & _ScanMode_AverageOut )
	        ps->AsicReg.RD_Dpi = 300;
    	else
	        ps->AsicReg.RD_Dpi = 600;


    	/* PauseColorMotorRunStates () */
    	memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
	    ps->a_nbNewAdrPointer[1] = 0x77;	

	    IOPutOnAllRegisters( ps );
    	_DODELAY( 70 );

        /* read one shading line and work on it */
	    if( IOReadOneShadingLine(ps, (pUChar)ps->Bufs.b1.pShadingRam, 512*2)) {

        	if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

            	wDarks[0] = dacP98003SumDarks( ps, ps->Bufs.b1.pShadingRam );
                wDarks[1] = dacP98003SumDarks( ps, ps->Bufs.b1.pShadingRam +
                                               ps->AsicReg.RD_Pixels );
		        wDarks[2] = dacP98003SumDarks( ps, ps->Bufs.b1.pShadingRam +
                                               ps->AsicReg.RD_Pixels * 2UL);

                if( !wDarks[0] || !wDarks[1] || !wDarks[2] ) {
		            ps->Shade.fStop = _FALSE;
		        } else {
		            ps->Shade.DarkOffset.wColors[0] = wDarks[0];
		            ps->Shade.DarkOffset.wColors[1] = wDarks[1];
		            ps->Shade.DarkOffset.wColors[2] = wDarks[2];
		            (*(ps->Device).fnDACDark)( ps, ps->Shade.pCcdDac,
                                               _CHANNEL_RED, wDarks[0] );
		            (*(ps->Device).fnDACDark)( ps, ps->Shade.pCcdDac,
                                               _CHANNEL_GREEN, wDarks[1] );
        		    (*(ps->Device).fnDACDark)( ps, ps->Shade.pCcdDac,
                                               _CHANNEL_BLUE, wDarks[2] );
	        	}
	        } else {
        		wDarks[1] = dacP98003SumDarks( ps, ps->Bufs.b1.pShadingRam +
                                               ps->AsicReg.RD_Pixels );
        		if (!wDarks[1] )
        		    ps->Shade.fStop = _FALSE;
        		else {
        		    ps->Shade.DarkOffset.wColors[1] = wDarks[1];
		            (*(ps->Device).fnDACDark)( ps, ps->Shade.pCcdDac,
                                               _CHANNEL_GREEN, wDarks[1] );
        		}
	        }
    	} else
            ps->Shade.fStop = _FALSE;
    }

    /* CalculateDarkDependOnCCD() */
   	if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {
    	(*(ps->Device).fnDarkOffset)( ps, ps->Shade.pCcdDac, _CHANNEL_RED   );
    	(*(ps->Device).fnDarkOffset)( ps, ps->Shade.pCcdDac, _CHANNEL_GREEN );
    	(*(ps->Device).fnDarkOffset)( ps, ps->Shade.pCcdDac, _CHANNEL_BLUE  );
    } else
    	(*(ps->Device).fnDarkOffset)( ps, ps->Shade.pCcdDac, _CHANNEL_GREEN );
}

/**
 */
static Bool dacP98003WaitForShading( pScanData ps )
{
    ULong i, tmp;
    Byte  bScanControl;

	DBG( DBG_LOW, "dacP98003WaitForShading()\n" );

	/*	
	 * before getting the shading data, (re)init the ASIC
	 */
	ps->ReInitAsic( ps, _TRUE );

    ps->Shade.DarkOffset.Colors.Red   = 0;
    ps->Shade.DarkOffset.Colors.Green = 0;
    ps->Shade.DarkOffset.Colors.Blue  = 0;

    IORegisterToScanner( ps, ps->RegResetMTSC );

    IODataToRegister( ps, ps->RegModelControl, ps->AsicReg.RD_ModelControl);
    IODataToRegister( ps, ps->RegMotorDriverType,
                                            ps->AsicReg.RD_MotorDriverType );
    IODataToRegister( ps, ps->RegScanControl1,
                                        (_SCANSTOPONBUFFULL| _MFRC_BY_XSTEP));
    ps->GotoShadingPosition( ps );

    bScanControl = ps->AsicReg.RD_ScanControl;

    /* SetShadingMapForGainDark */
    memset( ps->Bufs.b2.pSumBuf, 0xff, (5400 * 3 * 2));

    /* DownloadShadingAndSetDark() */
    dacP98DownloadShadingTable( ps, ps->Bufs.b2.pSumBuf, (5400 * 3 * 2));

    for( i = 0, tmp = 0; i < 1024; tmp += 0x01010101, i += 4 ) {
    	ps->Bufs.b1.Buf.pdw[i]   =
    	ps->Bufs.b1.Buf.pdw[i+1] =
    	ps->Bufs.b1.Buf.pdw[i+2] =
    	ps->Bufs.b1.Buf.pdw[i+3] = tmp;
    }

    memcpy( ps->Bufs.b1.pShadingMap + 4096, ps->Bufs.b1.pShadingMap, 4096 );
    memcpy( ps->Bufs.b1.pShadingMap + 8192, ps->Bufs.b1.pShadingMap, 4096 );
    dacP98DownloadMapTable( ps, ps->Bufs.b1.pShadingMap );

    DBG( DBG_LOW, "wExposure = %u\n", ps->Shade.wExposure);
    DBG( DBG_LOW, "wXStep    = %u\n", ps->Shade.wXStep);

    ps->AsicReg.RD_LineControl    = (_LOBYTE(ps->Shade.wExposure));
    ps->AsicReg.RD_ExtLineControl = (_HIBYTE(ps->Shade.wExposure));
    IODataToRegister(ps, ps->RegExtendedLineControl,
                                            ps->AsicReg.RD_ExtLineControl );
    IODataToRegister(ps, ps->RegLineControl, ps->AsicReg.RD_LineControl );

    dacP98003AdjustRGBGain( ps );
    dacP98003AdjustDark( ps );
    dacP98003AdjustShadingWaveform( ps );

    ps->AsicReg.RD_ScanControl = bScanControl;

	/* here we have to download the table in any case...*/
	dacP98DownloadMapTable( ps, ps->a_bMapTable );

    MotorP98003BackToHomeSensor( ps );

    return _TRUE;
}

/************************ exported functions *********************************/

/**
 */
_LOC int DacInitialize( pScanData ps )
{
	DBG( DBG_HIGH, "DacInitialize()\n" );

	if( NULL == ps )
		return _E_NULLPTR;

	/*
	 * depending on the asic, we set some functions
	 */
	if( _ASIC_IS_98003 == ps->sCaps.AsicID ) {

		ps->WaitForShading = dacP98003WaitForShading;

    } else if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

		ps->WaitForShading = dacP98WaitForShading;

	} else if( _ASIC_IS_96003 == ps->sCaps.AsicID ) {

		ps->WaitForShading = dacP96003WaitForShading;

	} else if( _ASIC_IS_96001 == ps->sCaps.AsicID ) {

		ps->WaitForShading = dacP96001WaitForShading;

	} else {

		DBG( DBG_HIGH , "NOT SUPPORTED ASIC !!!\n" );
		return _E_NOSUPP;
	}
	return _OK;
}

/** Fill out the R/G/B GainOut value
 */
_LOC void DacP98FillGainOutDirectPort( pScanData ps )
{
	ps->OpenScanPath( ps );

    IODataRegisterToDAC( ps, 0x28, ps->bRedGainIndex   );
    IODataRegisterToDAC( ps, 0x29, ps->bGreenGainIndex );
    IODataRegisterToDAC( ps, 0x2a, ps->bBlueGainIndex  );

	ps->CloseScanPath( ps );
}

/**
 */
_LOC void DacP98FillShadingDarkToShadingRegister( pScanData ps )
{
	pUChar pValue;
	Byte   bReg;

	DBG( DBG_LOW, "DacP98FillShadingDarkToShadingRegister()\n" );

	ps->AsicReg.RD_RedDarkOff   = ps->Shade.DarkOffset.Colors.Red;
    ps->AsicReg.RD_GreenDarkOff = ps->Shade.DarkOffset.Colors.Green;
    ps->AsicReg.RD_BlueDarkOff  = ps->Shade.DarkOffset.Colors.Blue;

    pValue = (pUChar)&ps->AsicReg.RD_RedDarkOff;
    for (bReg = ps->RegRedChDarkOffsetLow; bReg <= ps->RegBlueChDarkOffsetHigh;
	 	 bReg++, pValue++) {
		
		IODataToRegister( ps, bReg, *pValue );
	}
}

/**
 */
_LOC void DacP98AdjustDark( pScanData ps )
{
	Byte bCorrectTimes;		/* used to be a global var !*/

	DBG( DBG_LOW, "DacP98AdjustDark()\n" );

    ps->Shade.pCcdDac->DarkDAC.Colors.Red   = ps->bsPreRedDAC;
    ps->Shade.pCcdDac->DarkDAC.Colors.Green = ps->bsPreGreenDAC;
    ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = ps->bsPreBlueDAC;

    if( ps->DataInf.dwScanFlag & SCANDEF_Negative ) {
		bCorrectTimes = 6;
	} else {
		bCorrectTimes = 5;
	}

/* CHANGE
** original code seems to be buggy : for (bCorrectTimes ; bCorrectTimes--;) {
*/
	for (;bCorrectTimes ; bCorrectTimes-- ) {

    	ps->OpenScanPath( ps );
  		dacP98FillDarkDAC( ps );
		dacP98SetReadFBKRegister( ps );
       	ps->CloseScanPath( ps );

    	IOPutOnAllRegisters( ps );

		ps->PauseColorMotorRunStates( ps );		/* stop scan states */

		IOReadOneShadingLine( ps, ps->pScanBuffer1, 512*2 );

		dacP98FillChannelDarkLevelControl( ps );

		if(dacP98CheckChannelDarkLevel( ps ))
	    	break;
    }

	ps->Shade.DarkOffset.Colors.Red=
                dacP98CalDarkOff( ps, ps->Shade.DarkOffset.Colors.Red,
                                  ps->Shade.pCcdDac->DarkCmpHi.Colors.Red,
                                  ps->Shade.pCcdDac->DarkOffSub.Colors.Red );

    ps->Shade.DarkOffset.Colors.Green =
                dacP98CalDarkOff( ps, ps->Shade.DarkOffset.Colors.Green,
                                  ps->Shade.pCcdDac->DarkCmpHi.Colors.Green,
                                  ps->Shade.pCcdDac->DarkOffSub.Colors.Green );

    ps->Shade.DarkOffset.Colors.Blue  =
                dacP98CalDarkOff( ps, ps->Shade.DarkOffset.Colors.Blue,
                                  ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue,
                                  ps->Shade.pCcdDac->DarkOffSub.Colors.Blue );
}

/**
 */
_LOC void DacP96WriteBackToGammaShadingRAM( pScanData ps )
{
	/* ModifyGammaShadingOffset(ps) */
    ps->OpenScanPath( ps);

    IODataToRegister( ps, ps->RegRedChShadingOffset,
					  ps->Asic96Reg.u28.RD_RedChShadingOff );
	IODataToRegister( ps, ps->RegGreenChShadingOffset,
	     (Byte)((ULong)ps->Asic96Reg.u29.RD_GreenChShadingOff * 96UL/100UL));

	IODataToRegister( ps, ps->RegBlueChShadingOffset,
			    (Byte)((ULong)ps->Asic96Reg.RD_BlueChShadingOff * 91UL/100UL));

    ps->CloseScanPath( ps );

	dacP96WriteLinearGamma( ps, ps->pPrescan16, 256, ps->ShadingBankRed);
	dacP96WriteLinearGamma( ps, ps->pPrescan16 + ps->ShadingBankSize,
           										    256, ps->ShadingBankGreen);
	dacP96WriteLinearGamma( ps, ps->pPrescan16 + ps->ShadingBankSize * 2,
					  					            256, ps->ShadingBankBlue);
}

/**
 */
_LOC void DacP98003FillToDAC( pScanData ps, pRGBByteDef regs, pColorByte data )
{
   	if ( ps->DataInf.wPhyDataType > COLOR_256GRAY ) {

        dacP98003GainOffsetToDAC( ps, _DAC_RED, regs->Red, data->Colors.Red );
        dacP98003GainOffsetToDAC( ps, _DAC_GREENCOLOR,
                                            regs->Green, data->Colors.Green );
        dacP98003GainOffsetToDAC( ps, _DAC_BLUE,
                                            regs->Blue, data->Colors.Blue );
    } else {
        dacP98003GainOffsetToDAC( ps, _DAC_GREENMONO, regs->Green,
                                          			      data->Colors.Green );
    }
}

/**
 */
_LOC void DacP98003AdjustGain( pScanData ps, ULong color, Byte hilight )
{
    if( hilight < ps->Shade.bGainLow ) {

        if( ps->Shade.Hilight.bColors[color] < ps->Shade.bGainHigh ) {

    	    ps->Shade.fStop = _FALSE;
    	    ps->Shade.Hilight.bColors[color] = hilight;

    	    if( hilight <= (Byte)(ps->Shade.bGainLow - hilight))
        		ps->Shade.Gain.bColors[color] += ps->Shade.bGainDouble;
	        else
    		    ps->Shade.Gain.bColors[color]++;
	    }
    } else {
    	if( hilight > ps->Shade.bGainHigh ) {
    	    ps->Shade.fStop = _FALSE;
	        ps->Shade.Hilight.bColors[color] = hilight;
    	    ps->Shade.Gain.bColors[color]--;
    	} else
    	    ps->Shade.Hilight.bColors[color] = hilight;
	}

    if( ps->Shade.Gain.bColors[color] > ps->Shade.bMaxGain ) {
        ps->Shade.Gain.bColors[color] = ps->Shade.bMaxGain;
    }
}

/**
 */
_LOC Byte DacP98003SumGains( pUChar pb, ULong pixelsLine )
{
    Byte   bHilight, tmp;
    ULong  dwPixels, dwAve;
    UShort sum;

    for( bHilight = 0, dwPixels = pixelsLine >> 4; dwPixels--; ) {

    	for( sum = 0, dwAve = 16; dwAve--; pb++)
	        sum += (UShort)*pb;

    	sum >>= 4;
        tmp = (Byte)sum;

	    if( tmp > bHilight )
	        bHilight = tmp;
    }
    return bHilight;
}

/* END PLUSTEK-PP_DAC.C .....................................................*/
