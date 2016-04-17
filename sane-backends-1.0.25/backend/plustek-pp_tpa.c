/* @file plustek-pp_tpa.c
 * @brief Here we find some adjustments according to the scan source.
 *        This file is ASIC P9800x specific
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - Added some comments
 * - 0.32 - no changes
 * - 0.33 - new header
 * - 0.34 - no changes
 * - 0.35 - no changes
 * - 0.36 - no changes
 * - 0.37 - cosmetic changes
 * - 0.38 - Replaced AllPointer by DataPointer
 *        - renamed this file from transform.c tpa.c
 * - 0.39 - no changes
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - changed include names
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

/***************************** local vars ************************************/

static UShort a_wGainString [] = {
	 50,  75, 100, 125, 150, 175, 200, 225,
	250, 275, 300, 325, 350, 375, 400, 425,
	450, 475, 500, 525, 550, 575, 600, 625,
	650, 675, 700, 725, 750, 775, 800, 825
};

/*************************** local functions *********************************/

/*.............................................................................
 *
 */
static void tpaP98SubNoise( pScanData ps, pULong pdwSum, pUShort pwShading,
					        ULong dwHilightOff, ULong dwShadowOff )
{
	ULong   dwPixels, dwLines, dwSum;
    pUShort pw;

    for (dwPixels = 4; dwPixels--; pdwSum++, pwShading++)
		*pwShading = (UShort)(*pdwSum >> 5);

    for (dwPixels = 0; dwPixels < (ps->dwShadingPixels - 4); dwPixels++,
						 pdwSum++, pwShading++) {

		pw = (pUShort)ps->Shade.pHilight + dwHilightOff + dwPixels;
		dwSum = 0;

		for (dwLines = _DEF_BRIGHTEST_SKIP; dwLines--; pw += 5400UL)
		    dwSum += (ULong) *pw;

		pw = ps->pwShadow + dwShadowOff + dwPixels;

		for (dwLines = _DEF_DARKEST_SKIP; dwLines--; pw += 5400UL)
		    dwSum += (ULong) *pw;
	
		*pwShading = (UShort)((*pdwSum - dwSum) / ps->Shade.dwDiv);
    }
    if (ps->dwShadingPixels != 5400UL) {
		for (dwPixels = 2700UL; dwPixels--; pdwSum++, pwShading++)
		    *pwShading = (UShort)(*pdwSum >> 5);
	}
}

/*.............................................................................
 *
 */
static void tpaP98ShadingWaveformSum( pScanData ps )
{
	DataPointer	pd, pt;
    ULong		dw;

    pd.pdw = (pULong)ps->pScanBuffer1;
    pt.pw  = (pUShort)ps->pScanBuffer1;

    if ((ps->DataInf.dwScanFlag & SCANDEF_TPA ) ||
		(0 == ps->bShadingTimeFlag)) {

		if( ps->Shade.pHilight ) {

		    tpaP98SubNoise( ps, (pULong)ps->pScanBuffer1,
						      (pUShort)ps->pScanBuffer1, 0, 0);

		    tpaP98SubNoise( ps ,(pULong)ps->pScanBuffer1 + 5400UL,
						      (pUShort)ps->pScanBuffer1 + 5400UL,
						      ps->dwHilightCh, ps->dwShadowCh);

		    tpaP98SubNoise( ps, (pULong)ps->pScanBuffer1 + 5400UL * 2UL,
						      (pUShort)ps->pScanBuffer1 + 5400UL * 2UL,
				      	      ps->dwHilightCh * 2, ps->dwShadowCh * 2);
		} else {

		    for (dw = 5400 * 3; dw; dw--, pt.pw++, pd.pdw++)
				*pt.pw = (UShort)(*pd.pdw / 32);	/* shift 5 bits */
		}

    } else {

		if (02 == ps->bShadingTimeFlag ) {
		    for (dw = 5400 * 3; dw; dw--, pt.pw++, pd.pdw++)
				*pt.pw = (UShort)(*pd.pdw / 16);	/* shift 4 bits */
		} else {
		    for (dw = 5400 * 3; dw; dw--, pt.pw++, pd.pdw++)
				*pt.pw = (UShort)(*pd.pdw / 4);	    /* shift 2 bits */
		}
	}
}

/*.............................................................................
 * get wReduceRedFactor, wReduceGreenFactor, wReduceBlueFactor
 */
static void tpaP98GetNegativeTempRamData( pScanData ps )
{
	UShort		wRedTemp, wGreenTemp, wBlueTemp;
	UShort		wRedShadingTemp, wGreenShadingTemp, wBlueShadingTemp;
    ULong		dw, dw1;
	DataPointer	p;
	pULong		pdwNegativeSumTemp;
    pUShort		pNegativeTempRam, pNegativeTempRam2;

	ps->bFastMoveFlag = _FastMove_Low_C75_G150;

	MotorP98GoFullStep( ps ,80 );

    pNegativeTempRam   = (pUShort)(ps->pScanBuffer1 + 5400 * 6);
    pdwNegativeSumTemp = (pULong)(pNegativeTempRam + 960 * 3 * 2);
    pNegativeTempRam2  = (pUShort)(pdwNegativeSumTemp + 960 * 3 * 4);

	/* ClearNegativeSumBuffer() */
	memset( pdwNegativeSumTemp, 0, (960 * 3 * 4));

    /* SetReadNegativeTempRegister() */
	ps->AsicReg.RD_Motor0Control = 0;

	IOCmdRegisterToScanner( ps, ps->RegMotor0Control,
							ps->AsicReg.RD_Motor0Control );

	ps->AsicReg.RD_ModeControl   = _ModeScan;
	ps->AsicReg.RD_Motor0Control = _MotorOn + _MotorHEightStep + _MotorDirForward;
	ps->AsicReg.RD_ModelControl  = _ModelDpi600 + _LED_CONTROL + _LED_ACTIVITY;
	ps->AsicReg.RD_Dpi 			 = ps->PhysicalDpi;

    if (!ps->wNegAdjustX) {
		ps->AsicReg.RD_Origin = (UShort)(ps->dwOffset70 + ps->Device.DataOriginX +
							             _Negative96OriginOffsetX * 2);
	} else {
		ps->AsicReg.RD_Origin = (UShort)(ps->dwOffset70 + ps->Device.DataOriginX +
										 ps->wNegAdjustX);
	}

	ps->AsicReg.RD_Pixels    = 960;
	ps->AsicReg.RD_XStepTime = 32;

	IOPutOnAllRegisters( ps );

    /* NegativeMotorRunLoop() */
    p.pb = ps->a_nbNewAdrPointer;
    for (dw = _NUMBER_OF_SCANSTEPS / 8; dw; dw--, p.pdw++)
		*p.pdw = 0x87808780;

	IOSetToMotorRegister( ps );

    for (dw1 = 16; dw1; dw1--) {

		TimerDef timer;

		MiscStartTimer( &timer, _SECOND );

		while((IOReadFifoLength( ps ) < 960) && !MiscCheckTimer( &timer )) {

	    	_DO_UDELAY(1);
		}

	    /* ReadColorDataIn() - Read 1 RGB line */
		ps->AsicReg.RD_ModeControl = _ModeFifoRSel;
		IOReadScannerImageData( ps, (pUChar)pNegativeTempRam, 960 );

		ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
		IOReadScannerImageData( ps, (pUChar)(pNegativeTempRam + 960), 960 );

		ps->AsicReg.RD_ModeControl = _ModeFifoBSel;
		IOReadScannerImageData( ps, (pUChar)(pNegativeTempRam + 960 * 2), 960 );

		/* fillNegativeSum() */
		for (dw = 0; dw < 960 * 3; dw++)
		    pdwNegativeSumTemp[dw] += ((pUShort) pNegativeTempRam)[dw];

		/* one line */
		if (IOReadFifoLength( ps ) <= (960 * 2))
		    IORegisterDirectToScanner( ps, ps->RegRefreshScanState );
    }

    /* AverageAndShift() */
    for( dw = 0, dw1 = 0; dw < 240 * 3; dw++, dw1+=4 ) {

		pNegativeTempRam[dw] = (UShort)((pdwNegativeSumTemp[dw1] +
										 pdwNegativeSumTemp[dw1+1] +
										 pdwNegativeSumTemp[dw1+2] +
										 pdwNegativeSumTemp[dw1+3]) / 128);	
				/* shift 6 bits */
	}

	/* NegativeAdd1() */
    if (!ps->wNegAdjustX) {
		dw1 = (ps->dwOffsetNegative + _Negative96OriginOffsetX * 2 * 2) / 2;
    } else {
		dw1 = (ps->dwOffsetNegative + ps->wNegAdjustX * 2) / 2;
	}

	/* do R shading average */
    for (dw = 0; dw < 240; dw++, dw1 += 4) {	
		pNegativeTempRam2[dw] = (UShort)(
								 (((pUShort)ps->pScanBuffer1)[dw1] +
								  ((pUShort)ps->pScanBuffer1)[dw1+1] +
								  ((pUShort)ps->pScanBuffer1)[dw1+2] +
					 			  ((pUShort)ps->pScanBuffer1)[dw1+3]) / 4);
	}

	/* NegativeAdd1() */
    if (!ps->wNegAdjustX)
		dw1 = (ps->dwOffsetNegative + 5400 * 2 + _Negative96OriginOffsetX * 2 * 2) / 2;
    else
		dw1 = (ps->dwOffsetNegative + 5400 * 2 + ps->wNegAdjustX * 2) / 2;

	/* do G shading average */
    for (; dw < 240 * 2; dw++, dw1 += 4) {
		pNegativeTempRam2[dw] = (UShort)(
									(((pUShort)ps->pScanBuffer1)[dw1] +
									 ((pUShort)ps->pScanBuffer1)[dw1+1] +
									 ((pUShort)ps->pScanBuffer1)[dw1+2] +
									 ((pUShort)ps->pScanBuffer1)[dw1+3]) / 4);
	}

	/* NegativeAdd1() */
    if (!ps->wNegAdjustX)
		dw1 = (ps->dwOffsetNegative + 5400 * 4 + _Negative96OriginOffsetX * 2 * 2) / 2;
    else
		dw1 = (ps->dwOffsetNegative + 5400 * 4 + ps->wNegAdjustX * 2) / 2;

	/* do B shading average */
    for (; dw < 240 * 3; dw++, dw1 += 4) {
		pNegativeTempRam2 [dw] = (UShort)(
									(((pUShort)ps->pScanBuffer1)[dw1] +
									 ((pUShort)ps->pScanBuffer1)[dw1+1] +
									 ((pUShort)ps->pScanBuffer1)[dw1+2] +
									 ((pUShort)ps->pScanBuffer1)[dw1+3]) / 4);
	}

    wRedTemp = wGreenTemp = wBlueTemp = 0;
    wRedShadingTemp = wGreenShadingTemp = wBlueShadingTemp = 0;

    /* FindMaxNegValue -- find R */
    for (dw = 0; dw < 240; dw++) {
		if (pNegativeTempRam[dw] >= wRedTemp &&
		    pNegativeTempRam[dw + 240] >= wGreenTemp  &&
	    	pNegativeTempRam[dw + 480] > wBlueTemp)	{

		    wRedTemp   = pNegativeTempRam[dw];
		    wGreenTemp = pNegativeTempRam[dw + 240];
	    	wBlueTemp  = pNegativeTempRam[dw + 480];

		    wRedShadingTemp   = pNegativeTempRam2[dw];
		    wGreenShadingTemp = pNegativeTempRam2[dw + 240];
	    	wBlueShadingTemp  = pNegativeTempRam2[dw + 480];
		}
	}

    /* GainAddX = (1/4)*DoubleX + 1/ 2 */
    if ((ps->bRedGainIndex += (Byte)((wRedShadingTemp / wRedTemp) * 100 - 50) / 25) > 32)
		ps->bRedGainIndex = 31;

    if ((ps->bGreenGainIndex += (Byte)((wGreenShadingTemp / wGreenTemp) * 100 - 50) / 25) > 32)
		ps->bGreenGainIndex = 31;

    if ((ps->bBlueGainIndex += (Byte)((wBlueShadingTemp / wBlueTemp) * 100 - 50) / 25) > 32)
		ps->bBlueGainIndex = 31;

}

/*.............................................................................
 *
 */
static void tpaP98RecalculateNegativeShadingGain( pScanData ps )
{
    Byte		b[3];
	UShort		wSum, counter;
	UShort		w, w1, w2;
	ULong		dw, dw1;
	pUChar		pDest, pSrce, pNegativeTempRam;
    pUChar		pbReg[3];
	TimerDef	timer;
    DataPointer	p;

    pNegativeTempRam = (pUChar)(ps->pScanBuffer1 + 5400 * 6);

    /* AdjustDarkCondition () */
    ps->Shade.pCcdDac->DarkDAC.Colors.Red   = ps->bsPreRedDAC;
    ps->Shade.pCcdDac->DarkDAC.Colors.Green = ps->bsPreGreenDAC;
    ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = ps->bsPreBlueDAC;

    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red   = ps->wsDACCompareHighRed;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red   = ps->wsDACCompareLowRed;
    ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = ps->wsDACCompareHighGreen;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = ps->wsDACCompareLowGreen;
    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue  = ps->wsDACCompareHighBlue;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue  = ps->wsDACCompareLowBlue;

    DacP98FillGainOutDirectPort( ps );

    /* ClearNegativeTempBuffer () */
	memset( pNegativeTempRam, 0, (960 * 3 * 4));

    /* GetNegGainValue () */
	ps->PauseColorMotorRunStates( ps );

    /* SetScanMode () set scan mode to Byte mode */
	ps->AsicReg.RD_ScanControl |= _SCAN_BYTEMODE;
	ps->AsicReg.RD_ScanControl &= 0xfd;
	IOCmdRegisterToScanner(ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);
    DacP98FillGainOutDirectPort( ps );

    /* SetReadNegativeTempRegister() */
	ps->AsicReg.RD_Motor0Control = 0;
    IOCmdRegisterToScanner( ps, ps->RegMotor0Control,
							ps->AsicReg.RD_Motor0Control );

	ps->AsicReg.RD_ModeControl   = _ModeScan;
	ps->AsicReg.RD_Motor0Control = _MotorOn + _MotorHEightStep + _MotorDirForward;
	ps->AsicReg.RD_ModelControl  = _ModelDpi600 + _LED_CONTROL + _LED_ACTIVITY;
	ps->AsicReg.RD_Dpi 			 = ps->PhysicalDpi;

    if (!ps->wNegAdjustX) {
		ps->AsicReg.RD_Origin = (UShort)(ps->dwOffset70 +
                                         ps->Device.DataOriginX +
					 				     _Negative96OriginOffsetX * 2);
	} else {
		ps->AsicReg.RD_Origin = (UShort)(ps->dwOffset70 + ps->Device.DataOriginX +
										 ps->wNegAdjustX);
	}
	ps->AsicReg.RD_Pixels = 960;
	ps->AsicReg.RD_XStepTime = 32;
    IOPutOnAllRegisters( ps );

    /* ReReadNegativeTemp */
	MiscStartTimer( &timer, _SECOND );

    while((IOReadFifoLength( ps) < 960) && !MiscCheckTimer( &timer )) {

		_DO_UDELAY(1);	/* 1 us delay */
	}

    /* ReadColorDataIn() - Read 1 RGB line */
	ps->AsicReg.RD_ModeControl = _ModeFifoRSel;
	IOReadScannerImageData( ps, pNegativeTempRam, 960);

	ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
    IOReadScannerImageData( ps, pNegativeTempRam + 960, 960);

	ps->AsicReg.RD_ModeControl = _ModeFifoBSel;
	IOReadScannerImageData( ps, pNegativeTempRam + 960 * 2, 960);

    /* FindRGBGainValue(); */
    pDest = pSrce = pNegativeTempRam;

    /* ReAdjustGainAverage() */
    for (dw1 = 0; dw1 < (960 * 3) / 16; dw1++, pDest++) {
		for (dw = 0, wSum = 0; dw < 16; dw++, pSrce++)
		    wSum += *pSrce;

		*pDest = wSum / 16;
    }

    /* FindTheMaxGainValue */
    for (w = 0, p.pb = pNegativeTempRam; w < 3; w++) {
		for (dw = 960 / 16, b[w] = 0; dw; dw--, p.pb++) {
		    if (b[w] < *p.pb)
				b[w] = *p.pb;
		}
    }
	ps->bRedHigh   = b[0];
	ps->bGreenHigh = b[1];
    ps->bBlueHigh  = b[2];

    /* ModifyExposureTime () */
    if ((ps->bRedHigh < _GAIN_LOW) ||
		(ps->bGreenHigh < _GAIN_LOW) || (ps->bBlueHigh <  _GAIN_LOW)) {
		ps->AsicReg.RD_LineControl = 192;
	}

	IOCmdRegisterToScanner( ps, ps->RegLineControl,
							ps->AsicReg.RD_LineControl );
    counter = 0;

    /* ReAdjustRGBGain () */
    for (w1 = 0; w1 < 16; w1++) {

		ps->PauseColorMotorRunStates( ps );
		DacP98FillGainOutDirectPort( ps );

		/* SetReadNegativeTempRegister () */
		ps->AsicReg.RD_Motor0Control = 0;
		IOCmdRegisterToScanner( ps, ps->RegMotor0Control,
								ps->AsicReg.RD_Motor0Control );

		ps->AsicReg.RD_ModeControl   = _ModeScan;
		ps->AsicReg.RD_Motor0Control = _MotorOn + _MotorHEightStep + _MotorDirForward;
		ps->AsicReg.RD_ModelControl  = _ModelDpi600 + _LED_CONTROL + _LED_ACTIVITY;
		ps->AsicReg.RD_Dpi 			 = ps->PhysicalDpi;

		if (!ps->wNegAdjustX) {
		    ps->AsicReg.RD_Origin = (UShort)(ps->dwOffset70 +
                                             ps->Device.DataOriginX +
										     _Negative96OriginOffsetX * 2);
		} else {
			ps->AsicReg.RD_Origin = (UShort)(ps->dwOffset70 +
                                             ps->Device.DataOriginX +
											 ps->wNegAdjustX);
		}

		ps->AsicReg.RD_Pixels    = 960;
		ps->AsicReg.RD_XStepTime = 32;
		IOPutOnAllRegisters( ps );

		/* ReReadNegativeTemp () */
		MiscStartTimer( &timer, _SECOND );
		while((IOReadFifoLength( ps ) < 960) && !MiscCheckTimer( &timer)) {

			_DO_UDELAY(1);
		}

		/* ReadColorDataIn() - Read 1 RGB line */
		ps->AsicReg.RD_ModeControl = _ModeFifoRSel;
		IOReadScannerImageData( ps, pNegativeTempRam, 960 );

		ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
		IOReadScannerImageData( ps, pNegativeTempRam + 960, 960);

		ps->AsicReg.RD_ModeControl = _ModeFifoBSel;
		IOReadScannerImageData( ps ,pNegativeTempRam + 960 * 2, 960);


		/* ReAdjustGainAverage() */
		pDest = pSrce = pNegativeTempRam;
		for( dw1 = 0; dw1 < (960 * 3) / 16; dw1++, pDest++ ) {
		    for( dw = 0, wSum = 0; dw < 16; dw++, pSrce++ )
				wSum += *pSrce;
		    *pDest = wSum / 16;
		}

		/* FindTheMaxGainValue */
		pbReg[0] = &ps->bRedGainIndex;
		pbReg[1] = &ps->bGreenGainIndex;
		pbReg[2] = &ps->bBlueGainIndex;
		for (w = 0, p.pb = pNegativeTempRam; w < 3; w++) {

		    for (dw = 960 / 16, b [w] = 0; dw; dw--, p.pb++) {
				if (b[w] < *p.pb)
				    b[w] = *p.pb;
		    }
		    if (b [w] < _GAIN_LOW) {
				if (( _GAIN_P98_HIGH - b [w]) < b [w])
				    *(pbReg [w]) += 1;
				else
			    	*(pbReg [w]) += 4;
	    	} else {

				if (b [w] > _GAIN_P98_HIGH)
			    	*(pbReg [w]) -= 1;
		    }
		}

		for (w2 = 0; w2 < 3; w2++) {
		    if (*(pbReg[w2]) > 31)
			(*(pbReg[w2])) = 31;
		}
		if ((b[0] == 0) || (b[1] == 0) || (b[2] == 0)) {
	    	counter++;

		    if (counter < 16) {
				w1--;
				ps->bRedGainIndex   -= 4;
				ps->bGreenGainIndex -= 4;
				ps->bBlueGainIndex  -= 4;
		    }
		}
    }

	DacP98FillGainOutDirectPort( ps );

    ps->Shade.DarkOffset.Colors.Red   = 0;
	ps->Shade.DarkOffset.Colors.Green = 0;
	ps->Shade.DarkOffset.Colors.Blue  = 0;

    ps->OpenScanPath( ps );
	DacP98FillShadingDarkToShadingRegister( ps );
    ps->CloseScanPath( ps );

	DacP98AdjustDark( ps );
}

/*.............................................................................
 *
 */
static void tpaP98RecalculateShadingGainandData( pScanData ps )
{
	DataPointer	p;
	ULong		dw;
    UShort      filmAdjustX;
	UShort		wOldRedGain, wOldGreenGain, wOldBlueGain;
	UShort		wNewRedGain, wNewGreenGain, wNewBlueGain;

    /* AdjustDarkCondition () */
    ps->Shade.pCcdDac->DarkDAC.Colors.Red   = ps->bsPreRedDAC;
    ps->Shade.pCcdDac->DarkDAC.Colors.Green = ps->bsPreGreenDAC;
    ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = ps->bsPreBlueDAC;

    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red   = ps->wsDACCompareHighRed;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red   = ps->wsDACCompareLowRed;
    ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = ps->wsDACCompareHighGreen;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = ps->wsDACCompareLowGreen;
    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue  = ps->wsDACCompareHighBlue;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue  = ps->wsDACCompareLowBlue;

    wOldRedGain = a_wGainString[ps->bRedGainIndex] * 100/ps->wReduceRedFactor;

	/* SearchNewGain() */
    for (ps->bRedGainIndex = 0; ps->bRedGainIndex < 32; ps->bRedGainIndex++) {
		if (wOldRedGain < a_wGainString[ps->bRedGainIndex])
		    break;
	}

    if (0 == ps->bRedGainIndex)
		ps->bRedGainIndex ++;

    wNewRedGain = a_wGainString[--ps->bRedGainIndex];

    wOldGreenGain = a_wGainString[ps->bGreenGainIndex]*100/
														ps->wReduceGreenFactor;

	/* SearchNewGain() */
    for (ps->bGreenGainIndex = 0;
		 ps->bGreenGainIndex < 32; ps->bGreenGainIndex++) {

		if (wOldGreenGain < a_wGainString[ps->bGreenGainIndex])
		    break;
	}

    if (0 == ps->bGreenGainIndex)
		ps->bGreenGainIndex ++;

    wNewGreenGain = a_wGainString[--ps->bGreenGainIndex];

    wOldBlueGain = a_wGainString[ps->bBlueGainIndex]*100/ps->wReduceBlueFactor;

	/* SearchNewGain() */
    for (ps->bBlueGainIndex = 0;ps->bBlueGainIndex < 32;ps->bBlueGainIndex++) {
		if (wOldBlueGain < a_wGainString[ps->bBlueGainIndex])
		    break;
	}
    if (0 == ps->bBlueGainIndex)
		ps->bBlueGainIndex ++;

    wNewBlueGain = a_wGainString[--ps->bBlueGainIndex];

	DacP98FillGainOutDirectPort( ps );

	ps->Shade.DarkOffset.Colors.Red   = 0;
    ps->Shade.DarkOffset.Colors.Green = 0;
    ps->Shade.DarkOffset.Colors.Blue  = 0;

    ps->OpenScanPath( ps );
	DacP98FillShadingDarkToShadingRegister( ps );
    ps->CloseScanPath( ps );

	DacP98AdjustDark( ps );

    /* RecalculateTransparencyImage() */
    if (ps->DataInf.dwScanFlag & SCANDEF_Transparency) {
		filmAdjustX = ps->wPosAdjustX;
    } else {
		filmAdjustX = ps->wNegAdjustX;
	}

    if (!filmAdjustX) {
		p.pw = (pUShort)(ps->pScanBuffer1 + ps->dwOffsetNegative +
			  		       _Negative96OriginOffsetX * 2);
	} else {
		p.pw = (pUShort)(ps->pScanBuffer1 +
							  ps->dwOffsetNegative + filmAdjustX);
	}

	/* RecalculateData() */
    for (dw= 0; dw < _NegativePageWidth * 2 + 132; dw++, p.pw++)
		*p.pw = *p.pw * wNewRedGain / wOldRedGain;

    if (!ps->wNegAdjustX) {
		p.pw = (pUShort)(ps->pScanBuffer1 + 5400 * 2 +
							ps->dwOffsetNegative + _Negative96OriginOffsetX * 2);
	} else {
		p.pw = (pUShort)(ps->pScanBuffer1 + 5400 * 2 +
									ps->dwOffsetNegative + ps->wNegAdjustX);
	}

	/* RecalculateData() */
    for (dw= 0; dw < _NegativePageWidth * 2 + 132; dw++, p.pw++)
		*p.pw = *p.pw * wNewGreenGain / wOldGreenGain;

    if (!ps->wNegAdjustX) {
		p.pw = (pUShort)(ps->pScanBuffer1 + 5400 * 4 +
							ps->dwOffsetNegative + _Negative96OriginOffsetX * 2);
    } else {
		p.pw = (pUShort)(ps->pScanBuffer1 + 5400 * 4 +
									ps->dwOffsetNegative + ps->wNegAdjustX);
	}

	/* RecalculateData() - 64 + dwoffset70 */
    for (dw= 0; dw < _NegativePageWidth * 2 + 132; dw++, p.pw++)
		*p.pw = *p.pw * wNewBlueGain / wOldBlueGain;
}

/************************ exported functions *********************************/

/*.............................................................................
 * perform some adjustments according to the source (normal, transparency etc)
 */
_LOC void TPAP98001AverageShadingData( pScanData ps )
{
	DBG( DBG_LOW, "TPAP98001AverageShadingData()\n" );

	ps->wNegAdjustX 	 = 0;
	ps->wPosAdjustX 	 = 0;
	ps->dwOffsetNegative = 0;

	tpaP98ShadingWaveformSum( ps );

	/*
	 * CHANGE: to support Grayscale images in transparency and negative mode
	 * original code: if ((ps->DataInf.wPhyDataType >= COLOR_TRUE24) &&
	 */
	if((ps->DataInf.wPhyDataType >= COLOR_256GRAY) &&
       (ps->DataInf.dwScanFlag & SCANDEF_TPA)) {

		if (((ps->DataInf.dwScanFlag & SCANDEF_Negative) && !ps->wNegAdjustX) ||
		    ((ps->DataInf.dwScanFlag & SCANDEF_Transparency) && !ps->wPosAdjustX)) {

		    Long	dwLeft, dwRight;
		    pUShort	pw = (pUShort)ps->pScanBuffer1;

		    for (dwLeft = 0; dwLeft < 5400; dwLeft++)
				if (pw[dwLeft] >= 600)
		    		break;

		    for (dwRight = 4600; dwRight; dwRight--)
				if (pw[dwRight] >= 600)
		    		break;

			DBG( DBG_LOW, "_TPAPageWidth = %u, _NegativePageWidth = %u\n"
						  "right = %d, left = %d --> right = %d\n",
						  _TPAPageWidth, _NegativePageWidth,
						  dwRight, dwLeft, (((Long)dwRight-(Long)dwLeft)/2));

		    dwRight = (dwRight - dwLeft) / 2;

		    if (ps->DataInf.dwScanFlag & SCANDEF_Negative) {

				if (dwRight >= (Long)_NegativePageWidth) {

				    ps->wNegAdjustX = (UShort)(dwRight - _NegativePageWidth +
										 	   dwLeft - ps->dwOffset70 -
                                               ps->Device.DataOriginX + 4U);
					if( ps->wNegAdjustX > (_Negative96OriginOffsetX * 2U))
						ps->wNegAdjustX = (_Negative96OriginOffsetX * 2U);

				    ps->DataInf.crImage.x += ps->wNegAdjustX;
				} else {
				    ps->DataInf.crImage.x += (_Negative96OriginOffsetX * 2U);
				}
		    }  else {
				if (dwRight >= (Long)_TPAPageWidth) {

				    ps->wPosAdjustX = (UShort)(dwRight - _TPAPageWidth +
									 		   dwLeft - ps->dwOffset70 -
											   ps->Device.DataOriginX + 4U);

					if( ps->wPosAdjustX > (_Transparency96OriginOffsetX * 2U))
						ps->wPosAdjustX = (_Transparency96OriginOffsetX * 2U);

		    		ps->DataInf.crImage.x += ps->wPosAdjustX;

				} else {
				    ps->DataInf.crImage.x += (_Transparency96OriginOffsetX * 2U);
				}
	    	}
		}
#if 0
		else {
			/* CHANGE: as we always reset the values, we can ignore this code..*/

		    if( ps->DataInf.dwScanFlag & SCANDEF_Negative )
				ps->DataInf.crImage.x += ps->wNegAdjustX;
		    else
				ps->DataInf.crImage.x += ps->wPosAdjustX;
		}
#endif

		if( ps->DataInf.dwScanFlag & SCANDEF_Negative ) {

	    	ps->dwOffsetNegative = (ps->dwOffset70 + 64+4) * 2;

			tpaP98GetNegativeTempRamData( ps );
		    tpaP98RecalculateNegativeShadingGain( ps );

		} else {
		    ps->wReduceRedFactor   = 0x3e;
		    ps->wReduceGreenFactor = 0x39;
	    	ps->wReduceBlueFactor  = 0x42;

		    if( ps->Device.bCCDID == _CCD_518 ) {
				ps->wReduceRedFactor   = 55;
				ps->wReduceGreenFactor = 55;
				ps->wReduceBlueFactor  = 55;
		    }
		    if( ps->Device.bCCDID == _CCD_3797 ) {
				ps->wReduceRedFactor   = 42;
				ps->wReduceGreenFactor = 50;
				ps->wReduceBlueFactor  = 50;
		    }

			tpaP98RecalculateShadingGainandData( ps );
		}
    }
}

/*.............................................................................
 * perform some adjustments according to the source (normal, transparency etc)
 */
_LOC void TPAP98003FindCenterPointer( pScanData ps )
{
    ULong         i;
    ULong         width;
    ULong         left;
    ULong         right;
    pRGBUShortDef pwSum = ps->Bufs.b2.pSumRGB;

    if( ps->DataInf.dwScanFlag & SCANDEF_Negative )
        width = _NEG_PAGEWIDTH600;
    else
	    width = _NEG_PAGEWIDTH600 - 94;

    /* 2.54 cm tolerance */
    left  = ps->Device.DataOriginX + _NEG_ORG_OFFSETX * 2 - 600;
    right = ps->Device.DataOriginX + _NEG_ORG_OFFSETX * 2 +
                                          _NEG_PAGEWIDTH600 + 600;

    for( i = 5400UL - left, pwSum = ps->Bufs.b2.pSumRGB; i--; left++)
    	if( pwSum[left].Red   > _NEG_EDGE_VALUE &&
	        pwSum[left].Green > _NEG_EDGE_VALUE &&
	        pwSum[left].Blue  > _NEG_EDGE_VALUE)
    	    break;

    for( i = 5400UL - left, pwSum = ps->Bufs.b2.pSumRGB; i--; right--)
    	if( pwSum[right].Red   > _NEG_EDGE_VALUE &&
	        pwSum[right].Green > _NEG_EDGE_VALUE &&
	        pwSum[right].Blue  > _NEG_EDGE_VALUE)
	        break;

    if((right <= left) || ((right - left) < width)) {
        if( ps->DataInf.dwScanFlag & SCANDEF_Negative )
	        ps->Scan.negBegin = ps->Device.DataOriginX + _NEG_ORG_OFFSETX * 2;
    	else
	        ps->Scan.posBegin = ps->Device.DataOriginX + _POS_ORG_OFFSETX * 2;
    } else {
        if( ps->DataInf.dwScanFlag & SCANDEF_Negative )
	        ps->Scan.negBegin = (right + left) / 2UL - _NEG_PAGEWIDTH;
    	else
            ps->Scan.posBegin = (right + left) / 2UL - _POS_PAGEWIDTH;
    }
}

/*.............................................................................
 * this function does some reshading, when scanning negatives on an ASIC 98003
 * based scanner
 */
_LOC void TPAP98003Reshading( pScanData ps )
{
    Byte        bHi[3], bHiLeft[3], bHiRight[3];
    ULong       i, dwR, dwG, dwB, dwSum;
    ULong       dwIndex, dwIndexRight, dwIndexLeft;
    DataPointer RedPtr, GreenPtr, BluePtr;
    TimerDef    timer;

	bHi[0] = bHi[1] = bHi[2] = 0;

/* CHECK: Why this ??? */
#if 1
    ps->Scan.negScan[1].exposureTime = 144;
    ps->Scan.negScan[1].xStepTime    = 18;
    ps->Scan.negScan[2].exposureTime = 144;
    ps->Scan.negScan[2].xStepTime    = 36;
    ps->Scan.negScan[3].exposureTime = 144;
    ps->Scan.negScan[3].xStepTime    = 72;
    ps->Scan.negScan[4].exposureTime = 144;
    ps->Scan.negScan[4].xStepTime    = 144;
#endif

    ps->Shade.wExposure = ps->Scan.negScan[ps->Scan.dpiIdx].exposureTime;
    ps->Shade.wXStep    = ps->Scan.negScan[ps->Scan.dpiIdx].xStepTime;

    MiscStartTimer( &timer, _SECOND );

    while(!(IOGetScanState(ps, _TRUE) & _SCANSTATE_STOP) &&
                                              (_OK == MiscCheckTimer(&timer)));

    IODataToRegister( ps, ps->RegXStepTime,
                                    (Byte)(ps->AsicReg.RD_LineControl >> 4));
     _DODELAY( 12 );
    MotorP98003PositionYProc( ps, _NEG_SHADING_OFFS );

    IODataToRegister( ps, ps->RegXStepTime, ps->AsicReg.RD_XStepTime );

    ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE;
    IOSelectLampSource( ps );

    IODataToRegister( ps, ps->RegLineControl, _LOBYTE(ps->Shade.wExposure));
    IODataToRegister( ps, ps->RegXStepTime,   _LOBYTE(ps->Shade.wXStep));

    ps->AsicReg.RD_LineControl    = _LOBYTE(ps->Shade.wExposure);
    ps->AsicReg.RD_ExtLineControl = _HIBYTE(ps->Shade.wExposure);
    ps->AsicReg.RD_XStepTime      = (Byte)(ps->Shade.wExposure);
    ps->AsicReg.RD_ModeControl    = _ModeScan;
    ps->AsicReg.RD_Motor0Control  = _FORWARD_MOTOR;

    ps->AsicReg.RD_Origin = (UShort)ps->Scan.negBegin;
    ps->AsicReg.RD_Pixels = _NEG_PAGEWIDTH600;

    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

    /* put 9 scan states to make sure there are 8 lines available at least */
    for( i = 0; i <= 12; i++)
    	ps->a_nbNewAdrPointer[i] = 0x8f;

    IOPutOnAllRegisters( ps );
    _DODELAY( 70 );

	/* prepare the buffers... */
    memset( ps->Bufs.TpaBuf.pb, 0, _SizeTpaDataBuf );

    RedPtr.pb   = ps->Bufs.b1.pShadingMap;
    GreenPtr.pb = RedPtr.pb   + _NEG_PAGEWIDTH600;
    BluePtr.pb  = GreenPtr.pb + _NEG_PAGEWIDTH600;

    for( dwSum = 8; dwSum--; ) {

        IOReadOneShadingLine( ps, ps->Bufs.b1.pShadingMap, _NEG_PAGEWIDTH600 );

	    for( i = 0; i < _NEG_PAGEWIDTH600; i++) {

	        ps->Bufs.TpaBuf.pusrgb[i].Red   += RedPtr.pb[i];
    	    ps->Bufs.TpaBuf.pusrgb[i].Green += GreenPtr.pb[i];
	        ps->Bufs.TpaBuf.pusrgb[i].Blue  += BluePtr.pb[i];
    	}
    }

    for( i = 0; i < (_NEG_PAGEWIDTH600 * 3UL); i++ )
    	ps->Bufs.TpaBuf.pb[i] = ps->Bufs.TpaBuf.pw[i] >> 3;

    RedPtr.pb = ps->Bufs.TpaBuf.pb;

	/* Convert RGB to gray scale (Brightness), and average 16 pixels */
    for( bHiRight[1] = 0, i = dwIndexRight = 0;
                                         i < _NEG_PAGEWIDTH600 / 2; i += 16 ) {
    	bHiRight [0] =
	       (Byte)(((((ULong) RedPtr.pbrgb [i].Red +
		     (ULong) RedPtr.pbrgb[i + 1].Red +
		     (ULong) RedPtr.pbrgb[i + 2].Red +
		     (ULong) RedPtr.pbrgb[i + 3].Red +
		     (ULong) RedPtr.pbrgb[i + 4].Red +
		     (ULong) RedPtr.pbrgb[i + 5].Red +
		     (ULong) RedPtr.pbrgb[i + 6].Red +
		     (ULong) RedPtr.pbrgb[i + 7].Red +
		     (ULong) RedPtr.pbrgb[i + 8].Red +
		     (ULong) RedPtr.pbrgb[i + 9].Red +
		     (ULong) RedPtr.pbrgb[i + 10].Red +
		     (ULong) RedPtr.pbrgb[i + 11].Red +
		     (ULong) RedPtr.pbrgb[i + 12].Red +
		     (ULong) RedPtr.pbrgb[i + 13].Red +
		     (ULong) RedPtr.pbrgb[i + 14].Red +
		     (ULong) RedPtr.pbrgb[i + 15].Red) >> 4) * 30UL +
		   (((ULong) RedPtr.pbrgb[i].Green +
		     (ULong) RedPtr.pbrgb[i + 1].Green +
		     (ULong) RedPtr.pbrgb[i + 2].Green +
		     (ULong) RedPtr.pbrgb[i + 3].Green +
		     (ULong) RedPtr.pbrgb[i + 4].Green +
		     (ULong) RedPtr.pbrgb[i + 5].Green +
		     (ULong) RedPtr.pbrgb[i + 6].Green +
		     (ULong) RedPtr.pbrgb[i + 7].Green +
		     (ULong) RedPtr.pbrgb[i + 8].Green +
		     (ULong) RedPtr.pbrgb[i + 9].Green +
		     (ULong) RedPtr.pbrgb[i + 10].Green +
		     (ULong) RedPtr.pbrgb[i + 11].Green +
		     (ULong) RedPtr.pbrgb[i + 12].Green +
		     (ULong) RedPtr.pbrgb[i + 13].Green +
		     (ULong) RedPtr.pbrgb[i + 14].Green +
		     (ULong) RedPtr.pbrgb[i + 15].Green) >> 4) * 59UL +
		   (((ULong) RedPtr.pbrgb[i].Blue +
		     (ULong) RedPtr.pbrgb[i + 1].Blue +
		     (ULong) RedPtr.pbrgb[i + 2].Blue +
		     (ULong) RedPtr.pbrgb[i + 3].Blue +
		     (ULong) RedPtr.pbrgb[i + 4].Blue +
		     (ULong) RedPtr.pbrgb[i + 5].Blue +
		     (ULong) RedPtr.pbrgb[i + 6].Blue +
		     (ULong) RedPtr.pbrgb[i + 7].Blue +
		     (ULong) RedPtr.pbrgb[i + 8].Blue +
		     (ULong) RedPtr.pbrgb[i + 9].Blue +
		     (ULong) RedPtr.pbrgb[i + 10].Blue +
		     (ULong) RedPtr.pbrgb[i + 11].Blue +
		     (ULong) RedPtr.pbrgb[i + 12].Blue +
		     (ULong) RedPtr.pbrgb[i + 13].Blue +
		     (ULong) RedPtr.pbrgb[i + 14].Blue +
		     (ULong) RedPtr.pbrgb[i + 15].Blue) >> 4) * 11UL) / 100UL);

    	if( bHiRight[1] < bHiRight[0] ) {
    	    bHiRight[1] = bHiRight[0];
	        dwIndexRight = i;
    	}
    }

	/* Convert RGB to gray scale (Brightness), and average 16 pixels */
    for( bHiLeft[1] = 0, i = dwIndexLeft = _NEG_PAGEWIDTH / 2;
                                        	 i < _NEG_PAGEWIDTH600; i += 16 ) {
    	bHiLeft [0] =
	       (Byte)(((((ULong) RedPtr.pbrgb[i].Red +
		     (ULong) RedPtr.pbrgb[i + 1].Red +
		     (ULong) RedPtr.pbrgb[i + 2].Red +
		     (ULong) RedPtr.pbrgb[i + 3].Red +
		     (ULong) RedPtr.pbrgb[i + 4].Red +
		     (ULong) RedPtr.pbrgb[i + 5].Red +
		     (ULong) RedPtr.pbrgb[i + 6].Red +
		     (ULong) RedPtr.pbrgb[i + 7].Red +
		     (ULong) RedPtr.pbrgb[i + 8].Red +
		     (ULong) RedPtr.pbrgb[i + 9].Red +
		     (ULong) RedPtr.pbrgb[i + 10].Red +
		     (ULong) RedPtr.pbrgb[i + 11].Red +
		     (ULong) RedPtr.pbrgb[i + 12].Red +
		     (ULong) RedPtr.pbrgb[i + 13].Red +
		     (ULong) RedPtr.pbrgb[i + 14].Red +
		     (ULong) RedPtr.pbrgb[i + 15].Red) >> 4) * 30UL +
		   (((ULong) RedPtr.pbrgb[i].Green +
		     (ULong) RedPtr.pbrgb[i + 1].Green +
		     (ULong) RedPtr.pbrgb[i + 2].Green +
		     (ULong) RedPtr.pbrgb[i + 3].Green +
		     (ULong) RedPtr.pbrgb[i + 4].Green +
		     (ULong) RedPtr.pbrgb[i + 5].Green +
		     (ULong) RedPtr.pbrgb[i + 6].Green +
		     (ULong) RedPtr.pbrgb[i + 7].Green +
		     (ULong) RedPtr.pbrgb[i + 8].Green +
		     (ULong) RedPtr.pbrgb[i + 9].Green +
		     (ULong) RedPtr.pbrgb[i + 10].Green +
		     (ULong) RedPtr.pbrgb[i + 11].Green +
		     (ULong) RedPtr.pbrgb[i + 12].Green +
		     (ULong) RedPtr.pbrgb[i + 13].Green +
		     (ULong) RedPtr.pbrgb[i + 14].Green +
		     (ULong) RedPtr.pbrgb[i + 15].Green) >> 4) * 59UL +
		   (((ULong) RedPtr.pbrgb[i].Blue +
		     (ULong) RedPtr.pbrgb[i + 1].Blue +
		     (ULong) RedPtr.pbrgb[i + 2].Blue +
		     (ULong) RedPtr.pbrgb[i + 3].Blue +
		     (ULong) RedPtr.pbrgb[i + 4].Blue +
		     (ULong) RedPtr.pbrgb[i + 5].Blue +
		     (ULong) RedPtr.pbrgb[i + 6].Blue +
		     (ULong) RedPtr.pbrgb[i + 7].Blue +
		     (ULong) RedPtr.pbrgb[i + 8].Blue +
		     (ULong) RedPtr.pbrgb[i + 9].Blue +
		     (ULong) RedPtr.pbrgb[i + 10].Blue +
		     (ULong) RedPtr.pbrgb[i + 11].Blue +
		     (ULong) RedPtr.pbrgb[i + 12].Blue +
		     (ULong) RedPtr.pbrgb[i + 13].Blue +
		     (ULong) RedPtr.pbrgb[i + 14].Blue +
		     (ULong) RedPtr.pbrgb[i + 15].Blue) >> 4) * 11UL) / 100UL);

        if( bHiLeft[1] < bHiLeft[0] ) {
            bHiLeft[1] = bHiLeft[0];
	        dwIndexLeft = i;
    	}
    }

    if((bHiLeft[1] < 200) && (bHiRight[1] < 200)) {

    	if( bHiLeft[1] < bHiRight[1] )
    	    dwIndex = dwIndexRight;
	    else
	        dwIndex = dwIndexLeft;
    } else {
    	if( bHiLeft[1] > 200 )
    	    dwIndex = dwIndexRight;
	    else
	        dwIndex = dwIndexLeft;
    }

    /* Get the hilight */
    RedPtr.pusrgb = ps->Bufs.b2.pSumRGB + dwIndex +
                    ps->AsicReg.RD_Origin + _SHADING_BEGINX;

    for( dwR = dwG = dwB = 0, i = 16; i--; RedPtr.pusrgb++ ) {
    	dwR += RedPtr.pusrgb->Red;
	    dwG += RedPtr.pusrgb->Green;
    	dwB += RedPtr.pusrgb->Blue;
    }

    dwR >>= 8;
    dwG >>= 8;
    dwB >>= 8;

    if( dwR > dwG && dwR > dwB )
    	ps->Shade.bGainHigh = (Byte)dwR;     /* >> 4 for average, >> 4 to 8-bit */
    else {
    	if( dwG > dwR && dwG > dwB )
	        ps->Shade.bGainHigh = (Byte)dwG;
    	else
	        ps->Shade.bGainHigh = (Byte)dwB;
    }

    ps->Shade.bGainHigh = (Byte)(ps->Shade.bGainHigh - 0x18);
    ps->Shade.bGainLow  = (Byte)(ps->Shade.bGainHigh - 0x10);

    /* Reshading to get the new gain */
    ps->Shade.Hilight.Colors.Red   = 0;
    ps->Shade.Hilight.Colors.Green = 0;
    ps->Shade.Hilight.Colors.Blue  = 0;
    ps->Shade.Gain.Colors.Red++;
    ps->Shade.Gain.Colors.Green++;
    ps->Shade.Gain.Colors.Blue++;
    ps->Shade.fStop = _FALSE;

    RedPtr.pb   = ps->Bufs.b1.pShadingMap + dwIndex;
    GreenPtr.pb = RedPtr.pb   + _NEG_PAGEWIDTH600;
    BluePtr.pb  = GreenPtr.pb + _NEG_PAGEWIDTH600;

    for( i = 16; i-- && !ps->Shade.fStop;) {

    	ps->Shade.fStop = _TRUE;

        DacP98003FillToDAC( ps, &ps->Device.RegDACGain, &ps->Shade.Gain );

    	IODataToRegister( ps, ps->RegModeControl, _ModeIdle );

        ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE;
        IOSelectLampSource( ps );

    	ps->AsicReg.RD_ModeControl   = _ModeScan;
	    ps->AsicReg.RD_StepControl   = _MOTOR0_SCANSTATE;
    	ps->AsicReg.RD_Motor0Control = _FORWARD_MOTOR;

        memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
        ps->a_nbNewAdrPointer[1] = 0x77;

    	IOPutOnAllRegisters( ps );
	    _DODELAY( 50 );

    	if(IOReadOneShadingLine(ps,ps->Bufs.b1.pShadingMap,_NEG_PAGEWIDTH600)) {

	        bHi[0] = DacP98003SumGains( RedPtr.pb,   32 );
    	    bHi[1] = DacP98003SumGains( GreenPtr.pb, 32 );
	        bHi[2] = DacP98003SumGains( BluePtr.pb,  32 );

	        if( !bHi[0] || !bHi[1] || !bHi[2]) {
    	    	ps->Shade.fStop = _FALSE;
    	    } else {

        		DacP98003AdjustGain( ps, _CHANNEL_RED,   bHi[0] );
	        	DacP98003AdjustGain( ps, _CHANNEL_GREEN, bHi[1] );
		        DacP98003AdjustGain( ps, _CHANNEL_BLUE,  bHi[2] );
    	    }
	    } else
	        ps->Shade.fStop = _FALSE;
    }

    DacP98003FillToDAC( ps, &ps->Device.RegDACGain, &ps->Shade.Gain );

    /* Set RGB Gain */
    if( dwR && dwG && dwB ) {

    	if(ps->Device.bCCDID == _CCD_3797 || ps->Device.bDACType == _DA_ESIC) {
    	    ps->Shade.pCcdDac->GainResize.Colors.Red =
                                         (UShort)((ULong)bHi[0] * 100UL / dwR);
	        ps->Shade.pCcdDac->GainResize.Colors.Green =
                                         (UShort)((ULong)bHi[1] * 100UL / dwG);
	        ps->Shade.pCcdDac->GainResize.Colors.Blue =
                                         (UShort)((ULong)bHi[2] * 100UL / dwB);
    	} else {
    	    ps->Shade.pCcdDac->GainResize.Colors.Red =
                                         (UShort)((ULong)bHi[0] * 90UL / dwR);
	        ps->Shade.pCcdDac->GainResize.Colors.Green =
                                         (UShort)((ULong)bHi[1] * 77UL / dwG);
	        ps->Shade.pCcdDac->GainResize.Colors.Blue =
                                         (UShort)((ULong)bHi[2] * 73UL / dwB);
		}
    	ps->Shade.DarkOffset.Colors.Red +=
                                   (UShort)((dwR > bHi[0]) ? dwR - bHi[0] : 0);
    	ps->Shade.DarkOffset.Colors.Green +=
                                   (UShort)((dwG > bHi[1]) ? dwG - bHi[1] : 0);
	    ps->Shade.DarkOffset.Colors.Blue +=
                                   (UShort)((dwB > bHi[2]) ? dwB - bHi[2] : 0);

	    if( ps->Device.bDACType != _DA_ESIC && ps->Device.bCCDID != _CCD_3799 ) {
            ps->Shade.DarkOffset.Colors.Red =
                		(UShort)(ps->Shade.DarkOffset.Colors.Red *
                    		 ps->Shade.pCcdDac->GainResize.Colors.Red / 100UL);
    	    ps->Shade.DarkOffset.Colors.Green =
                		(UShort)(ps->Shade.DarkOffset.Colors.Green *
                      	   ps->Shade.pCcdDac->GainResize.Colors.Green / 100UL);
	        ps->Shade.DarkOffset.Colors.Blue =
                		(UShort)(ps->Shade.DarkOffset.Colors.Blue *
                 	        ps->Shade.pCcdDac->GainResize.Colors.Blue / 100UL);
    	}
    }

    /* AdjustDark () */
    ps->AsicReg.RD_Origin = _SHADING_BEGINX;
    ps->AsicReg.RD_Pixels = 5400;
}

/* END PLUSTEK-PP_TPA.C .....................................................*/
