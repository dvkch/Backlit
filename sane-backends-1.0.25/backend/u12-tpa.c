/* @file u12-pp_tpa.c
 * @brief Here we find some adjustments according to the scan source.
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - cleanup
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

/** this function does some reshading, when scanning negatives on an ASIC 98003
 * based scanner
 */
static void u12tpa_Reshading( U12_Device *dev )
{
	SANE_Byte   bHi[3], bHiLeft[3], bHiRight[3];
	u_long      i, dwR, dwG, dwB, dwSum;
	u_long      dwIndex, dwIndexRight, dwIndexLeft;
	DataPointer RedPtr, GreenPtr, BluePtr;
	TimerDef    timer;

	DBG( _DBG_INFO, "u12tpa_Reshading()\n" );

	bHi[0] = bHi[1] = bHi[2] = 0;

	dev->scan.negScan[1].exposureTime = 144;
	dev->scan.negScan[1].xStepTime    = 18;
	dev->scan.negScan[2].exposureTime = 144;
	dev->scan.negScan[2].xStepTime    = 36;
	dev->scan.negScan[3].exposureTime = 144;
	dev->scan.negScan[3].xStepTime    = 72;
	dev->scan.negScan[4].exposureTime = 144;
	dev->scan.negScan[4].xStepTime    = 144;

	dev->shade.wExposure = dev->scan.negScan[dev->scan.dpiIdx].exposureTime;
	dev->shade.wXStep    = dev->scan.negScan[dev->scan.dpiIdx].xStepTime;

	u12io_StartTimer( &timer, _SECOND );

	u12io_ResetFifoLen();
	while(!(u12io_GetScanState( dev ) & _SCANSTATE_STOP) &&
	                                              (!u12io_CheckTimer(&timer)));
	u12io_DataToRegister( dev, REG_XSTEPTIME,
	                               (SANE_Byte)(dev->regs.RD_LineControl >> 4));
	_DODELAY( 12 );
	u12motor_PositionYProc( dev, _NEG_SHADING_OFFS );

	u12io_DataToRegister( dev, REG_XSTEPTIME, dev->regs.RD_XStepTime );

	dev->regs.RD_ScanControl = _SCAN_BYTEMODE;
	u12hw_SelectLampSource( dev );

	u12io_DataToRegister( dev, REG_LINECONTROL, _LOBYTE(dev->shade.wExposure));
	u12io_DataToRegister( dev, REG_XSTEPTIME,   _LOBYTE(dev->shade.wXStep));

	dev->regs.RD_LineControl    = _LOBYTE(dev->shade.wExposure);
	dev->regs.RD_ExtLineControl = _HIBYTE(dev->shade.wExposure);
	dev->regs.RD_XStepTime      = (SANE_Byte)(dev->shade.wExposure);
	dev->regs.RD_ModeControl    = _ModeScan;
	dev->regs.RD_Motor0Control  = _FORWARD_MOTOR;

	dev->regs.RD_Origin = (u_short)dev->scan.negBegin;
	dev->regs.RD_Pixels = _NEG_PAGEWIDTH600;

	memset( dev->scanStates, 0, _SCANSTATE_BYTES );

	/* put 9 scan states to make sure there are 8 lines available at least */
	for( i = 0; i <= 12; i++)
		dev->scanStates[i] = 0x8f;

	u12io_PutOnAllRegisters( dev );
	_DODELAY( 70 );

	/* prepare the buffers... */
	memset( dev->bufs.TpaBuf.pb, 0, _SIZE_TPA_DATA_BUF );

	RedPtr.pb   = dev->bufs.b1.pShadingMap;
	GreenPtr.pb = RedPtr.pb   + _NEG_PAGEWIDTH600;
	BluePtr.pb  = GreenPtr.pb + _NEG_PAGEWIDTH600;

	for( dwSum = 8; dwSum--; ) {

		u12io_ReadOneShadingLine( dev, dev->bufs.b1.pShadingMap, _NEG_PAGEWIDTH600 );

		for( i = 0; i < _NEG_PAGEWIDTH600; i++) {

			dev->bufs.TpaBuf.pusrgb[i].Red   += RedPtr.pb[i];
			dev->bufs.TpaBuf.pusrgb[i].Green += GreenPtr.pb[i];
			dev->bufs.TpaBuf.pusrgb[i].Blue  += BluePtr.pb[i];
		}
	}

	for( i = 0; i < (_NEG_PAGEWIDTH600 * 3UL); i++ )
		dev->bufs.TpaBuf.pb[i] = dev->bufs.TpaBuf.pw[i] >> 3;

	RedPtr.pb = dev->bufs.TpaBuf.pb;

	/* Convert RGB to gray scale (Brightness), and average 16 pixels */
	for( bHiRight[1] = 0, i = dwIndexRight = 0;
	                                     i < _NEG_PAGEWIDTH600 / 2; i += 16 ) {
		bHiRight [0] =
	       (SANE_Byte)(((((u_long) RedPtr.pbrgb [i].Red +
		     (u_long) RedPtr.pbrgb[i + 1].Red +
		     (u_long) RedPtr.pbrgb[i + 2].Red +
		     (u_long) RedPtr.pbrgb[i + 3].Red +
		     (u_long) RedPtr.pbrgb[i + 4].Red +
		     (u_long) RedPtr.pbrgb[i + 5].Red +
		     (u_long) RedPtr.pbrgb[i + 6].Red +
		     (u_long) RedPtr.pbrgb[i + 7].Red +
		     (u_long) RedPtr.pbrgb[i + 8].Red +
		     (u_long) RedPtr.pbrgb[i + 9].Red +
		     (u_long) RedPtr.pbrgb[i + 10].Red +
		     (u_long) RedPtr.pbrgb[i + 11].Red +
		     (u_long) RedPtr.pbrgb[i + 12].Red +
		     (u_long) RedPtr.pbrgb[i + 13].Red +
		     (u_long) RedPtr.pbrgb[i + 14].Red +
		     (u_long) RedPtr.pbrgb[i + 15].Red) >> 4) * 30UL +
		   (((u_long) RedPtr.pbrgb[i].Green +
		     (u_long) RedPtr.pbrgb[i + 1].Green +
		     (u_long) RedPtr.pbrgb[i + 2].Green +
		     (u_long) RedPtr.pbrgb[i + 3].Green +
		     (u_long) RedPtr.pbrgb[i + 4].Green +
		     (u_long) RedPtr.pbrgb[i + 5].Green +
		     (u_long) RedPtr.pbrgb[i + 6].Green +
		     (u_long) RedPtr.pbrgb[i + 7].Green +
		     (u_long) RedPtr.pbrgb[i + 8].Green +
		     (u_long) RedPtr.pbrgb[i + 9].Green +
		     (u_long) RedPtr.pbrgb[i + 10].Green +
		     (u_long) RedPtr.pbrgb[i + 11].Green +
		     (u_long) RedPtr.pbrgb[i + 12].Green +
		     (u_long) RedPtr.pbrgb[i + 13].Green +
		     (u_long) RedPtr.pbrgb[i + 14].Green +
		     (u_long) RedPtr.pbrgb[i + 15].Green) >> 4) * 59UL +
		   (((u_long) RedPtr.pbrgb[i].Blue +
		     (u_long) RedPtr.pbrgb[i + 1].Blue +
		     (u_long) RedPtr.pbrgb[i + 2].Blue +
		     (u_long) RedPtr.pbrgb[i + 3].Blue +
		     (u_long) RedPtr.pbrgb[i + 4].Blue +
		     (u_long) RedPtr.pbrgb[i + 5].Blue +
		     (u_long) RedPtr.pbrgb[i + 6].Blue +
		     (u_long) RedPtr.pbrgb[i + 7].Blue +
		     (u_long) RedPtr.pbrgb[i + 8].Blue +
		     (u_long) RedPtr.pbrgb[i + 9].Blue +
		     (u_long) RedPtr.pbrgb[i + 10].Blue +
		     (u_long) RedPtr.pbrgb[i + 11].Blue +
		     (u_long) RedPtr.pbrgb[i + 12].Blue +
		     (u_long) RedPtr.pbrgb[i + 13].Blue +
		     (u_long) RedPtr.pbrgb[i + 14].Blue +
		     (u_long) RedPtr.pbrgb[i + 15].Blue) >> 4) * 11UL) / 100UL);

		if( bHiRight[1] < bHiRight[0] ) {
			 bHiRight[1] = bHiRight[0];
			 dwIndexRight = i;
		}
	}

	/* Convert RGB to gray scale (Brightness), and average 16 pixels */
	for( bHiLeft[1] = 0, i = dwIndexLeft = _NEG_PAGEWIDTH / 2;
	                                         i < _NEG_PAGEWIDTH600; i += 16 ) {
		bHiLeft [0] =
	       (SANE_Byte)(((((u_long) RedPtr.pbrgb[i].Red +
		     (u_long) RedPtr.pbrgb[i + 1].Red +
		     (u_long) RedPtr.pbrgb[i + 2].Red +
		     (u_long) RedPtr.pbrgb[i + 3].Red +
		     (u_long) RedPtr.pbrgb[i + 4].Red +
		     (u_long) RedPtr.pbrgb[i + 5].Red +
		     (u_long) RedPtr.pbrgb[i + 6].Red +
		     (u_long) RedPtr.pbrgb[i + 7].Red +
		     (u_long) RedPtr.pbrgb[i + 8].Red +
		     (u_long) RedPtr.pbrgb[i + 9].Red +
		     (u_long) RedPtr.pbrgb[i + 10].Red +
		     (u_long) RedPtr.pbrgb[i + 11].Red +
		     (u_long) RedPtr.pbrgb[i + 12].Red +
		     (u_long) RedPtr.pbrgb[i + 13].Red +
		     (u_long) RedPtr.pbrgb[i + 14].Red +
		     (u_long) RedPtr.pbrgb[i + 15].Red) >> 4) * 30UL +
		   (((u_long) RedPtr.pbrgb[i].Green +
		     (u_long) RedPtr.pbrgb[i + 1].Green +
		     (u_long) RedPtr.pbrgb[i + 2].Green +
		     (u_long) RedPtr.pbrgb[i + 3].Green +
		     (u_long) RedPtr.pbrgb[i + 4].Green +
		     (u_long) RedPtr.pbrgb[i + 5].Green +
		     (u_long) RedPtr.pbrgb[i + 6].Green +
		     (u_long) RedPtr.pbrgb[i + 7].Green +
		     (u_long) RedPtr.pbrgb[i + 8].Green +
		     (u_long) RedPtr.pbrgb[i + 9].Green +
		     (u_long) RedPtr.pbrgb[i + 10].Green +
		     (u_long) RedPtr.pbrgb[i + 11].Green +
		     (u_long) RedPtr.pbrgb[i + 12].Green +
		     (u_long) RedPtr.pbrgb[i + 13].Green +
		     (u_long) RedPtr.pbrgb[i + 14].Green +
		     (u_long) RedPtr.pbrgb[i + 15].Green) >> 4) * 59UL +
		   (((u_long) RedPtr.pbrgb[i].Blue +
		     (u_long) RedPtr.pbrgb[i + 1].Blue +
		     (u_long) RedPtr.pbrgb[i + 2].Blue +
		     (u_long) RedPtr.pbrgb[i + 3].Blue +
		     (u_long) RedPtr.pbrgb[i + 4].Blue +
		     (u_long) RedPtr.pbrgb[i + 5].Blue +
		     (u_long) RedPtr.pbrgb[i + 6].Blue +
		     (u_long) RedPtr.pbrgb[i + 7].Blue +
		     (u_long) RedPtr.pbrgb[i + 8].Blue +
		     (u_long) RedPtr.pbrgb[i + 9].Blue +
		     (u_long) RedPtr.pbrgb[i + 10].Blue +
		     (u_long) RedPtr.pbrgb[i + 11].Blue +
		     (u_long) RedPtr.pbrgb[i + 12].Blue +
		     (u_long) RedPtr.pbrgb[i + 13].Blue +
		     (u_long) RedPtr.pbrgb[i + 14].Blue +
		     (u_long) RedPtr.pbrgb[i + 15].Blue) >> 4) * 11UL) / 100UL);

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
	RedPtr.pusrgb = dev->bufs.b2.pSumRGB + dwIndex +
					dev->regs.RD_Origin + _SHADING_BEGINX;

	for( dwR = dwG = dwB = 0, i = 16; i--; RedPtr.pusrgb++ ) {
		dwR += RedPtr.pusrgb->Red;
		dwG += RedPtr.pusrgb->Green;
		dwB += RedPtr.pusrgb->Blue;
	}

	dwR >>= 8;
	dwG >>= 8;
	dwB >>= 8;

	if( dwR > dwG && dwR > dwB )
		dev->shade.bGainHigh = (SANE_Byte)dwR;     /* >> 4 for average, >> 4 to 8-bit */
	else {
		if( dwG > dwR && dwG > dwB )
			dev->shade.bGainHigh = (SANE_Byte)dwG;
		else
			dev->shade.bGainHigh = (SANE_Byte)dwB;
	}

	dev->shade.bGainHigh = (SANE_Byte)(dev->shade.bGainHigh - 0x18);
	dev->shade.bGainLow  = (SANE_Byte)(dev->shade.bGainHigh - 0x10);

	/* Reshading to get the new gain */
	dev->shade.Hilight.Colors.Red   = 0;
	dev->shade.Hilight.Colors.Green = 0;
	dev->shade.Hilight.Colors.Blue  = 0;
	dev->shade.Gain.Colors.Red++;
	dev->shade.Gain.Colors.Green++;
	dev->shade.Gain.Colors.Blue++;
	dev->shade.fStop = SANE_FALSE;

	RedPtr.pb   = dev->bufs.b1.pShadingMap + dwIndex;
	GreenPtr.pb = RedPtr.pb   + _NEG_PAGEWIDTH600;
	BluePtr.pb  = GreenPtr.pb + _NEG_PAGEWIDTH600;

	for( i = 16; i-- && !dev->shade.fStop;) {

		dev->shade.fStop = SANE_TRUE;

		u12shading_FillToDAC( dev, &dev->RegDACGain, &dev->shade.Gain );

		u12io_DataToRegister( dev, REG_MODECONTROL, _ModeIdle );

		dev->regs.RD_ScanControl = _SCAN_BYTEMODE;
		u12hw_SelectLampSource( dev );

		dev->regs.RD_ModeControl   = _ModeScan;
		dev->regs.RD_StepControl   = _MOTOR0_SCANSTATE;
		dev->regs.RD_Motor0Control = _FORWARD_MOTOR;

		memset( dev->scanStates, 0, _SCANSTATE_BYTES );
		dev->scanStates[1] = 0x77;

		u12io_PutOnAllRegisters( dev );
		_DODELAY( 50 );

		if(u12io_ReadOneShadingLine( dev,
		                         dev->bufs.b1.pShadingMap,_NEG_PAGEWIDTH600)) {

			bHi[0] = u12shading_SumGains( RedPtr.pb,   32 );
			bHi[1] = u12shading_SumGains( GreenPtr.pb, 32 );
			bHi[2] = u12shading_SumGains( BluePtr.pb,  32 );

			if( !bHi[0] || !bHi[1] || !bHi[2]) {
				dev->shade.fStop = SANE_FALSE;
			} else {

				u12shading_AdjustGain( dev, _CHANNEL_RED,   bHi[0] );
				u12shading_AdjustGain( dev, _CHANNEL_GREEN, bHi[1] );
				u12shading_AdjustGain( dev, _CHANNEL_BLUE,  bHi[2] );
			}
		} else {
			dev->shade.fStop = SANE_FALSE;
		}
	}

	u12shading_FillToDAC( dev, &dev->RegDACGain, &dev->shade.Gain );

	/* Set RGB Gain */
	if( dwR && dwG && dwB ) {

		if(dev->CCDID == _CCD_3797 || dev->DACType == _DA_ESIC ) {
			dev->shade.pCcdDac->GainResize.Colors.Red =
			                           (u_short)((u_long)bHi[0] * 100UL / dwR);
			dev->shade.pCcdDac->GainResize.Colors.Green =
			                           (u_short)((u_long)bHi[1] * 100UL / dwG);
			dev->shade.pCcdDac->GainResize.Colors.Blue =
			                           (u_short)((u_long)bHi[2] * 100UL / dwB);
		} else {
			dev->shade.pCcdDac->GainResize.Colors.Red =
			                            (u_short)((u_long)bHi[0] * 90UL / dwR);
			dev->shade.pCcdDac->GainResize.Colors.Green =
			                            (u_short)((u_long)bHi[1] * 77UL / dwG);
			dev->shade.pCcdDac->GainResize.Colors.Blue =
			                            (u_short)((u_long)bHi[2] * 73UL / dwB);
		}

		dev->shade.DarkOffset.Colors.Red +=
		                          (u_short)((dwR > bHi[0]) ? dwR - bHi[0] : 0);
		dev->shade.DarkOffset.Colors.Green +=
		                          (u_short)((dwG > bHi[1]) ? dwG - bHi[1] : 0);
		dev->shade.DarkOffset.Colors.Blue +=
		                          (u_short)((dwB > bHi[2]) ? dwB - bHi[2] : 0);

		if( dev->DACType != _DA_ESIC && dev->CCDID != _CCD_3799 ) {
			dev->shade.DarkOffset.Colors.Red =
			                       (u_short)(dev->shade.DarkOffset.Colors.Red *
			                dev->shade.pCcdDac->GainResize.Colors.Red / 100UL);
			dev->shade.DarkOffset.Colors.Green =
			                     (u_short)(dev->shade.DarkOffset.Colors.Green *
			              dev->shade.pCcdDac->GainResize.Colors.Green / 100UL);
			dev->shade.DarkOffset.Colors.Blue =
			                      (u_short)(dev->shade.DarkOffset.Colors.Blue *
			               dev->shade.pCcdDac->GainResize.Colors.Blue / 100UL);
		}
	}

	/* AdjustDark () */
	dev->regs.RD_Origin = _SHADING_BEGINX;
	dev->regs.RD_Pixels = 5400;
}

/** perform some adjustments according to the source (normal, transparency etc)
 */
static void u12tpa_FindCenterPointer( U12_Device *dev )
{
	u_long        i;
	u_long        width;
	u_long        left;
	u_long        right;
	RGBUShortDef *pwSum = dev->bufs.b2.pSumRGB;

	if( dev->DataInf.dwScanFlag & _SCANDEF_Negative )
		width = _NEG_PAGEWIDTH600;
	else
		width = _NEG_PAGEWIDTH600 - 94;

	/* 2.54 cm tolerance */
	left  = _DATA_ORIGIN_X + _NEG_ORG_OFFSETX * 2 - 600;
	right = _DATA_ORIGIN_X + _NEG_ORG_OFFSETX * 2 +
                                          _NEG_PAGEWIDTH600 + 600;

	for( i = 5400UL - left, pwSum = dev->bufs.b2.pSumRGB; i--; left++)
		if( pwSum[left].Red   > _NEG_EDGE_VALUE &&
			pwSum[left].Green > _NEG_EDGE_VALUE &&
			pwSum[left].Blue  > _NEG_EDGE_VALUE)
			break;

	for( i = 5400UL - left, pwSum = dev->bufs.b2.pSumRGB; i--; right--)
		if( pwSum[right].Red   > _NEG_EDGE_VALUE &&
			pwSum[right].Green > _NEG_EDGE_VALUE &&
			pwSum[right].Blue  > _NEG_EDGE_VALUE)
			break;

	if((right <= left) || ((right - left) < width)) {
		if( dev->DataInf.dwScanFlag & _SCANDEF_Negative )
			dev->scan.negBegin = _DATA_ORIGIN_X + _NEG_ORG_OFFSETX * 2;
		else
			dev->scan.posBegin = _DATA_ORIGIN_X + _POS_ORG_OFFSETX * 2;
	} else {
		if( dev->DataInf.dwScanFlag & _SCANDEF_Negative )
			dev->scan.negBegin = (right + left) / 2UL - _NEG_PAGEWIDTH;
		else
			dev->scan.posBegin = (right + left) / 2UL - _POS_PAGEWIDTH;
	}
}

/* END U12_TPA.C ............................................................*/
