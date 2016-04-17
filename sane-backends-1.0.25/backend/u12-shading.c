/* @file u12-shading.c -
 * @brief all the shading functions
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 -
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

#define _GAIN_HIGH 240 /* Volt. max. value */
#define _GAIN_LOW  220 /* Volt. min. value */

#define _CHANNEL_RED   0
#define _CHANNEL_GREEN 1
#define _CHANNEL_BLUE  2

/* for DAC programming */
#define _VALUE_CONFIG   0x51
#define _DAC_RED        (SANE_Byte)(_VALUE_CONFIG | 0x00)
#define _DAC_GREENCOLOR (SANE_Byte)(_VALUE_CONFIG | 0x04)
#define _DAC_GREENMONO  (SANE_Byte)(_VALUE_CONFIG | 0x06)
#define _DAC_BLUE       (SANE_Byte)(_VALUE_CONFIG | 0x08)


/* forward declarations ... */
static void u12tpa_Reshading( U12_Device * );
static void u12tpa_FindCenterPointer( U12_Device * );

/**
 */
static void
u12shading_DownloadShadingTable( U12_Device *dev, SANE_Byte *buf, u_long len )
{
	SANE_Byte *val, *rb;
	SANE_Byte  reg, regs[20];
	int        c;

	DBG( _DBG_INFO, "u12shading_DownloadShadingTable()\n" );

	u12io_DataToRegister( dev, REG_MODECONTROL, _ModeShadingMem );
	u12io_DataToRegister( dev, REG_MEMORYLO,  0 );
	u12io_DataToRegister( dev, REG_MEMORYHI, 0 );

	/* set 12 bits output color */
	u12io_DataToRegister( dev, REG_SCANCONTROL,
	                  (SANE_Byte)(dev->regs.RD_ScanControl | _SCAN_12BITMODE));

	u12io_MoveDataToScanner( dev, buf, len );

	regs[0] = REG_MODECONTROL;
	regs[1] = _ModeScan;

	/* FillShadingDarkToShadingRegister() */
	dev->regs.RD_RedDarkOff   = dev->shade.DarkOffset.Colors.Red;
	dev->regs.RD_GreenDarkOff = dev->shade.DarkOffset.Colors.Green;
	dev->regs.RD_BlueDarkOff  = dev->shade.DarkOffset.Colors.Blue;

	val = (SANE_Byte*)&dev->regs.RD_RedDarkOff;
	rb  = &regs[2];
	c   = 1;
	for( reg = REG_REDCHDARKOFFSETLO;
	     reg <= REG_BLUECHDARKOFFSETHI; reg++, val++) {

		*(rb++) = reg;
		*(rb++) = *val;
		c++;
	}
	u12io_DataToRegs( dev, regs, c );
}

/**
 */
static SANE_Status u12shadingAdjustShadingWaveform( U12_Device *dev )
{
	SANE_Byte     b;
	u_short       count, wR, wG, wB, tmp;
	DataType      var;
	DataPointer   pvar, psum;
	RBGPtrDef     cp;
	RGBUShortDef *pRGB, *pwsum;
	u_long        shadingBytes;

	DBG( _DBG_INFO, "u12shading_AdjustShadingWaveForm()\n" );

	memset( &cp, 0, sizeof(RBGPtrDef));
	memset( dev->bufs.b2.pSumBuf, 0, (5400 * 3 * 2));

	u12io_DataToRegister( dev, REG_MODECONTROL, _ModeIdle );

	dev->regs.RD_LineControl    = _LOBYTE(dev->shade.wExposure);
	dev->regs.RD_ExtLineControl = _HIBYTE(dev->shade.wExposure);
	u12io_DataToRegister( dev, REG_EXTENDEDLINECONTROL,
	                      dev->regs.RD_ExtLineControl );
	u12io_DataToRegister( dev, REG_LINECONTROL, dev->regs.RD_LineControl );

	dev->regs.RD_XStepTime    = _LOBYTE(dev->shade.wExposure);
	dev->regs.RD_ExtXStepTime = _HIBYTE(dev->shade.wExposure);
	u12io_DataToRegister( dev, REG_EXTENDEDXSTEP, dev->regs.RD_ExtXStepTime );
	u12io_DataToRegister( dev, REG_XSTEPTIME, dev->regs.RD_XStepTime );

	dev->regs.RD_ModeControl   = _ModeScan;
	dev->regs.RD_StepControl   = _MOTOR0_SCANSTATE;
	dev->regs.RD_Motor0Control = _FORWARD_MOTOR;

	if( dev->shade.intermediate & _ScanMode_AverageOut ) {

		dev->regs.RD_Dpi    = 300;
		dev->regs.RD_Pixels = 2700;
		shadingBytes        = 2700 * 2;
	} else {
		dev->regs.RD_Dpi    = 600;
		dev->regs.RD_Pixels = 5400;
		shadingBytes        = 5400 * 2;
	}
	dev->regs.RD_Origin = _SHADING_BEGINX;

	for( pvar.pdw = (u_long*)dev->scanStates,
		var.dwValue = _SCANSTATE_BYTES >> 2; var.dwValue--; pvar.pdw++) {
		*pvar.pdw = 0x00f00080;
	}

	dev->scan.refreshState = SANE_FALSE;
	u12io_PutOnAllRegisters( dev );
/*	_DODELAY( 100 ); */

	if( dev->shade.pHilight ) {

		memset( dev->shade.pHilight, 0,
		        shadingBytes * dev->shade.skipHilight * 3 );

		memset((SANE_Byte*)dev->shade.pHilight +
		        shadingBytes * dev->shade.skipHilight * 3, 0xff,
		        shadingBytes * dev->shade.skipShadow * 3 );
	}

	for( count = 32; count--; ) {

		if( u12io_IsEscPressed()) {
			DBG( _DBG_INFO, "* CANCEL detected!\n" );
			return SANE_STATUS_CANCELLED;
		}

		u12io_ReadOneShadingLine( dev, ((SANE_Byte*)dev->bufs.b1.pShadingRam)+
		                           _SHADING_BEGINX, shadingBytes );

		if( dev->shade.pHilight ) {

			if ( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {

				cp.red.usp   = dev->bufs.b1.pShadingRam + _SHADING_BEGINX;
				cp.green.usp = cp.red.usp   + dev->regs.RD_Pixels;
				cp.blue.usp  = cp.green.usp + dev->regs.RD_Pixels;
				pvar.pusrgb  = (RGBUShortDef*)dev->shade.pHilight +
				                                              _SHADING_BEGINX;

				for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
				                                              var.dwValue--;) {
					pRGB = pvar.pusrgb++;
					wR = *cp.red.usp;
					wG = *cp.green.usp;
					wB = *cp.blue.usp;

					for( b = dev->shade.skipHilight; b--;
					                            pRGB += dev->regs.RD_Pixels ) {
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

					for( b = dev->shade.skipShadow; b--;
					                            pRGB += dev->regs.RD_Pixels ) {
						if( wR < pRGB->Red ) {
							tmp = wR;
							wR  = pRGB->Red;
							pRGB->Red = tmp;
						}
						if( wG < pRGB->Green ) {
							tmp = wG;
							wG  = pRGB->Green;
							pRGB->Green = tmp;
						}
						if( wB < pRGB->Blue ) {
							tmp = wB;
							wB  = pRGB->Blue;
							pRGB->Blue = tmp;
						}
					}
				}
			} else {

				cp.green.usp = dev->bufs.b1.pShadingRam +
				               dev->regs.RD_Pixels + _SHADING_BEGINX;
				cp.blue.usp  = (u_short*)dev->shade.pHilight + _SHADING_BEGINX;

				for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
				                                              var.dwValue--;) {
					cp.red.usp = cp.blue.usp++;
					wG = *cp.green.usp;
					for( b = dev->shade.skipHilight; b--;
					                       cp.red.usp += dev->regs.RD_Pixels) {
						if( wG > *cp.red.usp ) {
							tmp = wG;
							wG  = *cp.red.usp;
							*cp.red.usp = tmp;
						}
					}
					wG = *cp.green.usp++;
					for( b = dev->shade.skipShadow; b--;
					                      cp.red.usp += dev->regs.RD_Pixels ) {
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
		if( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {

			cp.red.usp   = dev->bufs.b1.pShadingRam + _SHADING_BEGINX;
			cp.green.usp = cp.red.usp   + dev->regs.RD_Pixels;
			cp.blue.usp  = cp.green.usp + dev->regs.RD_Pixels;

			pvar.pulrgb = (RGBULongDef*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;

			for( var.dwValue = (u_long)dev->regs.RD_Pixels - _SHADING_BEGINX;
			     var.dwValue--;
			     pvar.pulrgb++, cp.red.usp++, cp.green.usp++, cp.blue.usp++) {
				pvar.pulrgb->Red   += (u_long)*cp.red.usp;
				pvar.pulrgb->Green += (u_long)*cp.green.usp;
				pvar.pulrgb->Blue  += (u_long)*cp.blue.usp;
			}

		} else {

			cp.green.usp = dev->bufs.b1.pShadingRam +
			               dev->regs.RD_Pixels +  _SHADING_BEGINX;
			pvar.pdw  = (u_long*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;
			for( var.dwValue = (u_long)dev->regs.RD_Pixels - _SHADING_BEGINX;
			     var.dwValue--; pvar.pdw++, cp.green.usp++) {
				*pvar.pdw += (u_long)*cp.green.usp;
			}
		}

		u12io_ResetFifoLen();
		if( u12io_GetFifoLength( dev ) < dev->regs.RD_Pixels )
			u12io_RegisterToScanner( dev, REG_REFRESHSCANSTATE );
	}

	/* AverageAfterSubHilightShadow() */
	if( dev->shade.pHilight ) {
		if( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {

			psum.pulrgb = (RGBULongDef*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;
			pwsum       = (RGBUShortDef*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;
			pvar.pusrgb = (RGBUShortDef*)dev->shade.pHilight + _SHADING_BEGINX;

			for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
                                                              var.dwValue--;) {
				pRGB = pvar.pusrgb++;

				for( b = dev->shade.skipHilight + dev->shade.skipShadow;
				                           b--; pRGB += dev->regs.RD_Pixels ) {

					psum.pulrgb->Red   -= (u_long)pRGB->Red;
					psum.pulrgb->Green -= (u_long)pRGB->Green;
					psum.pulrgb->Blue  -= (u_long)pRGB->Blue;
				}

				pwsum->Red   = (u_short)(psum.pulrgb->Red   / dev->shade.dwDiv);
				pwsum->Green = (u_short)(psum.pulrgb->Green / dev->shade.dwDiv);
				pwsum->Blue  = (u_short)(psum.pulrgb->Blue  / dev->shade.dwDiv);
				psum.pulrgb++;
				pwsum++;
			}
		} else {
			cp.green.ulp = (u_long*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;
			cp.blue.usp  = (u_short*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;
			pvar.pw      = (u_short*)dev->shade.pHilight  + _SHADING_BEGINX;

			for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
			                                                  var.dwValue--;) {
				cp.red.usp = pvar.pw++;

				for( b = dev->shade.skipHilight + dev->shade.skipShadow;
				                       b--; cp.red.usp += dev->regs.RD_Pixels )
					*cp.green.ulp -= *cp.red.usp;

				*cp.blue.usp = (u_short)(*cp.green.ulp / dev->shade.dwDiv);
				cp.blue.usp++;
				cp.green.ulp++;
			}
		}
	} else {

		if( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {

			psum.pulrgb = (RGBULongDef*)dev->bufs.b2.pSumBuf  + _SHADING_BEGINX;
			pwsum       = (RGBUShortDef*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;

			for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
			                                                  var.dwValue--;) {
				pwsum->Red   = (u_short)(psum.pulrgb->Red   >> 5);
				pwsum->Green = (u_short)(psum.pulrgb->Green >> 5);
				pwsum->Blue  = (u_short)(psum.pulrgb->Blue  >> 5);
				psum.pulrgb++;
				pwsum++;
			}
		} else {
			cp.green.ulp = (u_long*)dev->bufs.b2.pSumBuf  + _SHADING_BEGINX;
			cp.blue.usp  = (u_short*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;

			for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
			                                                  var.dwValue--;) {
				*cp.blue.usp = (u_short)(*cp.green.ulp >> 5);
				cp.blue.usp++;
				cp.green.ulp++;
			}
		}
	}

	/* Process negative & transparency here */
	if( dev->DataInf.dwScanFlag & _SCANDEF_TPA )
		u12tpa_FindCenterPointer( dev );

	if( dev->DataInf.dwScanFlag & _SCANDEF_Negative )
		u12tpa_Reshading( dev );

	pRGB = (RGBUShortDef*)&dev->shade.pCcdDac->GainResize;

	if ( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {

		pwsum = (RGBUShortDef*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;

		for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
		                                                      var.dwValue--;) {

			if ((short)(pwsum->Red -= dev->shade.DarkOffset.Colors.Red) > 0) {
				pwsum->Red = pwsum->Red * pRGB->Red / 100U;
				if( pwsum->Red > 0xfff )
					pwsum->Red = 0xfff;
			} else
				pwsum->Red = 0;

			if((short)(pwsum->Green -= dev->shade.DarkOffset.Colors.Green) > 0) {
				pwsum->Green = pwsum->Green * pRGB->Green / 100U;
				if( pwsum->Green > 0xfff )
					pwsum->Green = 0xfff;
			} else
				pwsum->Green = 0;

			if ((short)(pwsum->Blue -= dev->shade.DarkOffset.Colors.Blue) > 0) {
				pwsum->Blue = pwsum->Blue * pRGB->Blue / 100U;
				if( pwsum->Blue > 0xfff )
					pwsum->Blue = 0xfff;
			} else
				pwsum->Blue = 0;

			wR = (u_short)(pwsum->Red >> 4);
			pwsum->Red <<= 12;
			pwsum->Red |= wR;
			wR = (u_short)(pwsum->Green >> 4);
			pwsum->Green <<= 12;
			pwsum->Green |= wR;
			wR = (u_short)(pwsum->Blue>> 4);
			pwsum->Blue <<= 12;
			pwsum->Blue |= wR;
			pwsum++;
		}
	} else {

		cp.green.usp = (u_short*)dev->bufs.b2.pSumBuf + _SHADING_BEGINX;

		for( var.dwValue = dev->regs.RD_Pixels - _SHADING_BEGINX;
		                                                      var.dwValue--;) {

			if((short)(*cp.green.usp -= dev->shade.DarkOffset.Colors.Green) > 0) {

				*cp.green.usp = *cp.green.usp * pRGB->Green / 100U;
				if( *cp.green.usp > 0xfff )
					*cp.green.usp = 0xfff;
			} else
				*cp.green.usp = 0;

			wR = (u_short)(*cp.green.usp >> 4);
			*cp.green.usp <<= 12;
			*cp.green.usp |= wR;

			cp.green.usp++;
		}
	}

	u12shading_DownloadShadingTable(dev, dev->bufs.b2.pSumBuf, (5400 * 3 * 2));
	return SANE_STATUS_GOOD;
}

/**
 */
static void u12shading_GainOffsetToDAC( U12_Device *dev, SANE_Byte ch,
                                        SANE_Byte reg, SANE_Byte d )
{
	if( dev->DACType == _DA_SAMSUNG8531 ) {
		u12io_DataRegisterToDAC( dev, 0, ch );
	}
	u12io_DataRegisterToDAC( dev, reg, d );
}

/**
 */
static void u12shading_FillToDAC( U12_Device *dev,
                                  RGBByteDef *regs, ColorByte *data )
{
	if( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {

		u12shading_GainOffsetToDAC(dev, _DAC_RED, regs->Red, data->Colors.Red);
		u12shading_GainOffsetToDAC(dev, _DAC_GREENCOLOR,
                                              regs->Green, data->Colors.Green);
		u12shading_GainOffsetToDAC(dev, _DAC_BLUE,
                                                regs->Blue, data->Colors.Blue);
	} else {
		u12shading_GainOffsetToDAC(dev, _DAC_GREENMONO, regs->Green,
		                                                   data->Colors.Green);
    }
}

/**
 */
static SANE_Byte u12shading_SumGains( SANE_Byte *pb, u_long pixelsLine )
{
	SANE_Byte hilight, tmp;
	u_long    dwPixels, dwAve;
	u_short   sum;

	hilight = 0;
	for( dwPixels = pixelsLine >> 4; dwPixels--; ) {

		for( sum = 0, dwAve = 16; dwAve--; pb++ )
			sum += (u_short)*pb;

		sum >>= 4;
		tmp = (SANE_Byte)sum;

		if( tmp > hilight )
			hilight = tmp;
	}
	return hilight;
}

/**
 */
static void
u12shading_AdjustGain( U12_Device *dev, u_long color, SANE_Byte hilight )
{
	if( hilight < dev->shade.bGainLow ) {

		if( dev->shade.Hilight.bColors[color] < dev->shade.bGainHigh ) {

			dev->shade.fStop = SANE_FALSE;
			dev->shade.Hilight.bColors[color] = hilight;

			if( hilight <= (SANE_Byte)(dev->shade.bGainLow - hilight))
				dev->shade.Gain.bColors[color] += dev->shade.bGainDouble;
			else
				dev->shade.Gain.bColors[color]++;
		}
	} else {
		if( hilight > dev->shade.bGainHigh ) {
			dev->shade.fStop = SANE_FALSE;
			dev->shade.Hilight.bColors[color] = hilight;
			dev->shade.Gain.bColors[color]--;
		} else {
			dev->shade.Hilight.bColors[color] = hilight;
		}
	}

	if( dev->shade.Gain.bColors[color] > dev->shade.bMaxGain ) {
		dev->shade.Gain.bColors[color] = dev->shade.bMaxGain;
	}
}

/**
 */
static SANE_Status u12shading_AdjustRGBGain( U12_Device *dev )
{
	int       i;
	SANE_Byte hi[3];

	DBG( _DBG_INFO, "u12shading_AdjustRGBGain()\n" );

	dev->shade.Gain.Colors.Red   =
	dev->shade.Gain.Colors.Green =
	dev->shade.Gain.Colors.Blue  =  dev->shade.bUniGain;

	dev->shade.Hilight.Colors.Red   =
	dev->shade.Hilight.Colors.Green =
	dev->shade.Hilight.Colors.Blue  = 0;

	dev->shade.bGainHigh = _GAIN_HIGH;
	dev->shade.bGainLow  = _GAIN_LOW;

	dev->shade.fStop = SANE_FALSE;

	for( i = 10; i-- && !dev->shade.fStop; ) {

		if( u12io_IsEscPressed()) {
			DBG( _DBG_INFO, "* CANCEL detected!\n" );
			return SANE_STATUS_CANCELLED;
		}

		dev->shade.fStop = SANE_TRUE;

		u12io_DataToRegister( dev, REG_MODECONTROL, _ModeIdle );

		dev->regs.RD_ScanControl = _SCAN_BYTEMODE;
		u12hw_SelectLampSource( dev );
		u12io_DataToRegister( dev, REG_SCANCONTROL, dev->regs.RD_ScanControl );

		u12shading_FillToDAC( dev, &dev->RegDACGain, &dev->shade.Gain );

		dev->regs.RD_ModeControl   = _ModeScan;
		dev->regs.RD_StepControl   = _MOTOR0_SCANSTATE;
		dev->regs.RD_Motor0Control = _FORWARD_MOTOR;

		if( dev->shade.intermediate & _ScanMode_AverageOut )
			dev->regs.RD_Origin = (u_short)_DATA_ORIGIN_X >> 1;
		else
			dev->regs.RD_Origin = (u_short)_DATA_ORIGIN_X;

		dev->regs.RD_Dpi    = 300;
		dev->regs.RD_Pixels = 2560;

		memset( dev->scanStates, 0, _SCANSTATE_BYTES );
		dev->scanStates[1] = 0x77;

		u12io_PutOnAllRegisters( dev );
/*		_DODELAY( 100 ); */

		/* read one shading line and work on it */
		if( u12io_ReadOneShadingLine( dev,
		                         (SANE_Byte*)dev->bufs.b1.pShadingRam, 2560)) {

			if( dev->DataInf.wPhyDataType <= COLOR_256GRAY ) {

				hi[1] = u12shading_SumGains(
				           (SANE_Byte*)dev->bufs.b1.pShadingRam + 2560, 2560);
				if( hi[1] ) {
					u12shading_AdjustGain( dev, _CHANNEL_GREEN, hi[1] );
				} else {
					dev->shade.fStop = SANE_FALSE;
				}
			} else {
				hi[0] = u12shading_SumGains(
				                   (SANE_Byte*)dev->bufs.b1.pShadingRam, 2560);
				hi[1] = u12shading_SumGains(
				            (SANE_Byte*)dev->bufs.b1.pShadingRam + 2560, 2560);
				hi[2] = u12shading_SumGains(
				            (SANE_Byte*)dev->bufs.b1.pShadingRam + 5120, 2560);

				if (!hi[0] || !hi[1] || !hi[2] ) {
					dev->shade.fStop = SANE_FALSE;
				} else {
					u12shading_AdjustGain( dev, _CHANNEL_RED,   hi[0] );
					u12shading_AdjustGain( dev, _CHANNEL_GREEN, hi[1] );
					u12shading_AdjustGain( dev, _CHANNEL_BLUE,  hi[2] );
				}
			}
		} else
			dev->shade.fStop = SANE_FALSE;
	}

	if( !dev->shade.fStop )
		DBG( _DBG_INFO, "u12shading_AdjustRGBGain() - all loops done!!!\n" );

	u12shading_FillToDAC( dev, &dev->RegDACGain, &dev->shade.Gain );
	return SANE_STATUS_GOOD;
}

/**
 */
static u_short u12shading_SumDarks( U12_Device *dev, u_short *data )
{
	u_short i, loop;

	if( dev->CCDID == _CCD_3799 ) {
		if( dev->shade.intermediate & _ScanMode_AverageOut )
			data += 0x18;
		else
			data += 0x30;
	} else {
		if( dev->shade.intermediate & _ScanMode_AverageOut )
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
static SANE_Status u12shadingAdjustDark( U12_Device *dev )
{
	u_long  i;
	u_short wDarks[3];

	DBG( _DBG_INFO, "u12shadingAdjustDark()\n" );
	dev->shade.DarkDAC.Colors = dev->shade.pCcdDac->DarkDAC.Colors;
	dev->shade.fStop = SANE_FALSE;

	for( i = 16; i-- && !dev->shade.fStop;) {

		if( u12io_IsEscPressed()) {
			DBG( _DBG_INFO, "* CANCEL detected!\n" );
			return SANE_STATUS_CANCELLED;
		}

		dev->shade.fStop = SANE_TRUE;

		u12shading_FillToDAC( dev, &dev->RegDACOffset, &dev->shade.DarkDAC );
		u12io_DataToRegister( dev, REG_MODECONTROL, _ModeIdle );

		dev->regs.RD_ScanControl = (_SCAN_12BITMODE + _SCAN_1ST_AVERAGE);
		u12hw_SelectLampSource( dev );
		u12io_DataToRegister( dev, REG_SCANCONTROL, dev->regs.RD_ScanControl );

		dev->regs.RD_StepControl   = _MOTOR0_SCANSTATE;
		dev->regs.RD_Motor0Control = _FORWARD_MOTOR;

		dev->regs.RD_Origin = _SHADING_BEGINX;
		dev->regs.RD_Pixels = 512;

		if( dev->shade.intermediate & _ScanMode_AverageOut )
			dev->regs.RD_Dpi = 300;
		else
			dev->regs.RD_Dpi = 600;

		memset( dev->scanStates, 0, _SCANSTATE_BYTES );
		dev->scanStates[1] = 0x77;

		u12io_PutOnAllRegisters( dev );
/*		_DODELAY( 100 ); */

		/* read one shading line and work on it */
		if( u12io_ReadOneShadingLine(dev,
		                        (SANE_Byte*)dev->bufs.b1.pShadingRam, 512*2)) {

			if ( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {

				wDarks[0] = u12shading_SumDarks(dev, dev->bufs.b1.pShadingRam);
				wDarks[1] = u12shading_SumDarks(dev, dev->bufs.b1.pShadingRam +
                                                dev->regs.RD_Pixels );
				wDarks[2] = u12shading_SumDarks(dev, dev->bufs.b1.pShadingRam +
				                                dev->regs.RD_Pixels * 2UL);

				if( !wDarks[0] || !wDarks[1] || !wDarks[2] ) {
					dev->shade.fStop = SANE_FALSE;
				} else {
					dev->shade.DarkOffset.wColors[0] = wDarks[0];
					dev->shade.DarkOffset.wColors[1] = wDarks[1];
					dev->shade.DarkOffset.wColors[2] = wDarks[2];
					(*dev->fnDACDark)( dev,dev->shade.pCcdDac,
					                           _CHANNEL_RED, wDarks[0] );
					(*dev->fnDACDark)( dev, dev->shade.pCcdDac,
					                           _CHANNEL_GREEN, wDarks[1] );
					(*dev->fnDACDark)( dev, dev->shade.pCcdDac,
					                           _CHANNEL_BLUE, wDarks[2] );
				}
			} else {
				wDarks[1] = u12shading_SumDarks(dev, dev->bufs.b1.pShadingRam +
                                                dev->regs.RD_Pixels );
				if(!wDarks[1] ) {
					dev->shade.fStop = SANE_FALSE;
				} else {
					dev->shade.DarkOffset.wColors[1] = wDarks[1];
					(*dev->fnDACDark)( dev, dev->shade.pCcdDac,
				                                   _CHANNEL_GREEN, wDarks[1] );
				}
			}
		} else {
			dev->shade.fStop = SANE_FALSE;
		}
	}

	/* CalculateDarkDependOnCCD() */
	if ( dev->DataInf.wPhyDataType > COLOR_256GRAY ) {
		(*dev->fnDarkOffset)( dev, dev->shade.pCcdDac, _CHANNEL_RED   );
		(*dev->fnDarkOffset)( dev, dev->shade.pCcdDac, _CHANNEL_GREEN );
		(*dev->fnDarkOffset)( dev, dev->shade.pCcdDac, _CHANNEL_BLUE  );
	} else {
		(*dev->fnDarkOffset)( dev, dev->shade.pCcdDac, _CHANNEL_GREEN );
	}
	return SANE_STATUS_GOOD;
}

/** here we download the current mapping table
 */
static void u12shading_DownloadMapTable( U12_Device *dev, SANE_Byte *buf )
{
	SANE_Byte addr, regs[6];
	int       i;

	u12io_DataToRegister( dev, REG_SCANCONTROL,
	          (SANE_Byte)((dev->regs.RD_ScanControl & 0xfc) | _SCAN_BYTEMODE));

	/* prepare register settings... */
	regs[0] = REG_MODECONTROL;
	regs[1] = _ModeMappingMem;
	regs[2] = REG_MEMORYLO;
	regs[3] = 0;
	regs[4] = REG_MEMORYHI;

	for( i = 3, addr = _MAP_ADDR_RED; i--; addr += _MAP_ADDR_SIZE ) {

		regs[5] = addr;
		u12io_DataToRegs( dev, regs, 3 );

		u12io_MoveDataToScanner( dev, buf, 4096 );
		buf += 4096;
	}

	u12io_DataToRegister( dev, REG_SCANCONTROL, dev->regs.RD_ScanControl );
}

/**
 */
static SANE_Status u12shading_DoCalibration( U12_Device *dev )
{
	SANE_Byte   tb[4096*3];
	u_long      i, tmp;
	SANE_Byte   bScanControl, rb[20];
	SANE_Status res;
	int         c;
    
	DBG( _DBG_INFO, "u12shading_DoCalibration()\n" );

	/** before getting the shading data, (re)init the ASIC
	 */
	u12hw_InitAsic( dev, SANE_TRUE );

	dev->shade.DarkOffset.Colors.Red   = 0;
	dev->shade.DarkOffset.Colors.Green = 0;
	dev->shade.DarkOffset.Colors.Blue  = 0;

	c = 0;
	_SET_REG( rb, c, REG_RESETMTSC, 0 );
	_SET_REG( rb, c, REG_MODELCONTROL, dev->regs.RD_ModelControl);
	_SET_REG( rb, c, REG_MOTORDRVTYPE, dev->regs.RD_MotorDriverType );
	_SET_REG( rb, c, REG_SCANCONTROL1, (_SCANSTOPONBUFFULL| _MFRC_BY_XSTEP));

	u12io_DataToRegs( dev, rb, c );

	res = u12motor_GotoShadingPosition( dev );
	if( SANE_STATUS_GOOD != res )
		return res;

	bScanControl = dev->regs.RD_ScanControl;

    /* SetShadingMapForGainDark */
	memset( dev->bufs.b2.pSumBuf, 0xff, (5400 * 3 * 2));
	u12shading_DownloadShadingTable( dev, dev->bufs.b2.pSumBuf, (5400*3*2));

	for( i = 0, tmp = 0; i < 1024; tmp += 0x01010101, i += 4 ) {
		dev->bufs.b1.Buf.pdw[i]   =
		dev->bufs.b1.Buf.pdw[i+1] =
		dev->bufs.b1.Buf.pdw[i+2] =
		dev->bufs.b1.Buf.pdw[i+3] = tmp;
	}

	memcpy( dev->bufs.b1.pShadingMap + 4096, dev->bufs.b1.pShadingMap, 4096 );
	memcpy( dev->bufs.b1.pShadingMap + 8192, dev->bufs.b1.pShadingMap, 4096 );
	u12shading_DownloadMapTable( dev, dev->bufs.b1.pShadingMap );

	DBG( _DBG_INFO, "* wExposure = %u\n", dev->shade.wExposure);
	DBG( _DBG_INFO, "* wXStep    = %u\n", dev->shade.wXStep);

	dev->regs.RD_LineControl    = (_LOBYTE(dev->shade.wExposure));
	dev->regs.RD_ExtLineControl = (_HIBYTE(dev->shade.wExposure));
	u12io_DataToRegister( dev, REG_EXTENDEDLINECONTROL,
	                                            dev->regs.RD_ExtLineControl );
	u12io_DataToRegister( dev, REG_LINECONTROL, dev->regs.RD_LineControl );

	res = u12shading_AdjustRGBGain( dev );
	if( SANE_STATUS_GOOD != res )
		return res;
	
	res = u12shadingAdjustDark( dev );
	if( SANE_STATUS_GOOD != res )
		return res;

	res = u12shadingAdjustShadingWaveform( dev );
	if( SANE_STATUS_GOOD != res )
		return res;

	dev->regs.RD_ScanControl = bScanControl;

	/* here we have to prepare and download the table in any case...*/
	if( dev->DataInf.wPhyDataType <= COLOR_256GRAY ) {
		u12map_Adjust( dev, _MAP_MASTER, tb );
	} else {
		u12map_Adjust( dev, _MAP_RED, tb   );
		u12map_Adjust( dev, _MAP_GREEN, tb );
		u12map_Adjust( dev, _MAP_BLUE, tb  );
	}

	u12shading_DownloadMapTable( dev, tb );

	u12motor_BackToHomeSensor( dev );
	DBG( _DBG_INFO, "u12shading_DoCalibration() - done.\n" );
	return SANE_STATUS_GOOD;
}

/* END U12-SHADING ..........................................................*/
