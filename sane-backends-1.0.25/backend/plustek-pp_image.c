/* @file plustek-pp_image.c
 * @brief functions to convert scanner data into image data
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - no changes
 * - 0.33 - no changes
 * - 0.34 - reactivated code in imageP96WaitLineData() to recover from
 *          loosing data
 * - 0.35 - no changes
 * - 0.36 - removed comment
 *        - added wDither exchange to imageP9xSetupScanSettings
 *        - added fnHalftoneDirect1 which provides dithering by using random
 *          thresholds
 *        - removed the swapping behaviour for model OP_600 in
 *          fnP96ColorDirect() according to the Primax 4800 Direct tests
 *        - changes, due to define renaming
 *        - removed _ASIC_96001 specific stuff to invert colors
 * - 0.37 - removed // comments
 *        - corrected output of 12bit/pixel
 * - 0.38 - added P12 stuff
 *        - renamed WaitLineData functions to ReadOneImageLine
 * - 0.39 - fixed a problem in imageP98003ReadOneImageLine, that causes
 *          these I/O timeouts...
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - fixed a problem for the 12bit modes fo ASIC9800x based devices
 *        - changed include names
 * - 0.43 - removed floating point stuff
 *        - cleanup
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

/************************ local definitions **********************************/

#define _LINE_TIMEOUT   (_SECOND * 5 )      /* max 5 second per line !  */

/*************************** local vars **************************************/

static UShort wPreviewScanned = 0;

static ExpXStepDef posScan[5] = {{128, 8}, {96, 12},
                                 {96, 24}, {96, 48}, {96, 96}};
static ExpXStepDef negScan[5] = {{128, 8}, {96, 12},
                                 {96, 24}, {96, 48}, {96, 96}};

static ExpXStepDef nmlScan[4][5] = {
    {{160, 10}, {96, 12},  {96, 24},  {96, 48},  {96, 96}},     /* EPP */
    {{160, 10}, {128, 16}, {128, 32}, {192, 96}, {192, 96}},    /* SPP */
    {{160, 10}, {96, 12},  {96, 24},  {160, 80}, {160, 160}},   /* BPP */
    {{160, 10}, {96, 12},  {96, 24},  {96, 48},  {96, 96}}      /* ECP */
};

static ThreshDef xferSpeed[4] = {
    {0, 3200, 2500}, {0, 1200, 800}, {0, 800, 1250}, {0, 3200, 2500}
};

/*************************** local functions *********************************/

/** return the correct DPI-value
 * The ASIC 96001/3 models are limited to an optical resolution of 300 Dpi
 * so it´s necessary to scale in X and Y direction (see scale.c)!
 */
static UShort imageGetPhysDPI( pScanData ps, pImgDef pImgInf, Bool fDpiX )
{
	if( _IS_ASIC98(ps->sCaps.AsicID)) {

	    if (fDpiX) {

			if (pImgInf->xyDpi.x > ps->LensInf.rDpiX.wPhyMax)
	    		return ps->LensInf.rDpiX.wPhyMax;
			else
	    		return pImgInf->xyDpi.x;

	    } else {
			if (pImgInf->xyDpi.y > ps->LensInf.rDpiY.wPhyMax)
	    		return ps->LensInf.rDpiY.wPhyMax;
			else
	    		return pImgInf->xyDpi.y;
	    }
	} else {

	    if (fDpiX) {

			if (pImgInf->wDataType >= COLOR_TRUE24) {
	    		if (pImgInf->xyDpi.x > ps->LensInf.rDpiX.wPhyMax)
					return ps->LensInf.rDpiX.wPhyMax;
			    else
					return pImgInf->xyDpi.x;
			} else {
	    		if (pImgInf->xyDpi.x > (ps->LensInf.rDpiX.wPhyMax * 2))
					return (ps->LensInf.rDpiX.wPhyMax * 2);
			    else
					return pImgInf->xyDpi.x;
			}
    	} else {

			if (pImgInf->wDataType >= COLOR_TRUE24 ) {
	    		if (pImgInf->xyDpi.y > (ps->LensInf.rDpiY.wPhyMax / 2))
					return (ps->LensInf.rDpiY.wPhyMax / 2);
			    else
					return pImgInf->xyDpi.y;
			} else {
	    		if (pImgInf->xyDpi.y > ps->LensInf.rDpiY.wPhyMax)
					return ps->LensInf.rDpiY.wPhyMax;
			    else
					return pImgInf->xyDpi.y;
			}
	    }
	}
}

/*****************************************************************************
 *			Sampling stuff for ASIC 98003      						         *
 *****************************************************************************/

static Bool fnEveryLines( pScanData ps )
{
	_VAR_NOT_USED( ps );
    return _TRUE;
}

static Bool fnSampleLines( pScanData ps )
{
    ps->DataInf.wYSum += ps->DataInf.xyAppDpi.y;

    if( ps->DataInf.wYSum >= ps->DataInf.xyPhyDpi.y ) {

        ps->DataInf.wYSum -= ps->DataInf.xyPhyDpi.y;
    	return	_TRUE;
    }

	return _FALSE;
}

static Bool fnSamplePreview( pScanData ps )
{
    ps->DataInf.wYSum += wPreviewScanned;
    if( ps->DataInf.wYSum >= 150 ) {

        ps->DataInf.wYSum -= 150;
    	return	_TRUE;
    }

	return _FALSE;
}

/*****************************************************************************
 *			Data Processing Routines			     						 *
 *****************************************************************************/

static Bool fnReadToDriver( pScanData ps )
{
    ps->AsicReg.RD_ModeControl = _ModeFifoBSel;
    IOReadScannerImageData( ps, ps->Scan.BufPut.blue.bp,
							ps->DataInf.dwAsicBytesPerPlane );

	ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
    IOReadScannerImageData( ps, ps->Scan.BufPut.green.bp,
							ps->DataInf.dwAsicBytesPerPlane );

    if( ps->Scan.gd_gk.wGreenKeep )
		ps->Scan.gd_gk.wGreenKeep--;
    else {
		ps->Scan.BufPut.green.bp += ps->DataInf.dwAsicBytesPerPlane;

		if( ps->Scan.BufPut.green.bp >= ps->Scan.BufEnd.green.bp )
		    ps->Scan.BufPut.green.bp = ps->Scan.BufBegin.green.bp;
    }

    ps->AsicReg.RD_ModeControl = _ModeFifoRSel;
    IOReadScannerImageData( ps, ps->Scan.BufPut.red.bp,
							ps->DataInf.dwAsicBytesPerPlane );

    ps->Scan.BufPut.red.bp += ps->DataInf.dwAsicBytesPerPlane;
    if( ps->Scan.BufPut.red.bp >= ps->Scan.BufEnd.red.bp )
		ps->Scan.BufPut.red.bp = ps->Scan.BufBegin.red.bp;

    if( ps->Scan.bd_rk.wRedKeep ) {
		ps->Scan.bd_rk.wRedKeep--;
		return _FALSE;

    } else {

		ps->Scan.BufData.green.bp = ps->Scan.BufGet.green.bp;

		if( ps->DataInf.dwScanFlag & SCANDEF_ColorBGROrder ) {
		    ps->Scan.BufData.red.bp  = ps->Scan.BufGet.blue.bp;
		    ps->Scan.BufData.blue.bp = ps->Scan.BufGet.red.bp;
		} else {
		    ps->Scan.BufData.red.bp = ps->Scan.BufGet.red.bp;
		    ps->Scan.BufData.blue.bp = ps->Scan.BufGet.blue.bp;
		}

		ps->Scan.BufGet.red.bp   += ps->DataInf.dwAsicBytesPerPlane;
		ps->Scan.BufGet.green.bp += ps->DataInf.dwAsicBytesPerPlane;

		if( ps->Scan.BufGet.red.bp >= ps->Scan.BufEnd.red.bp )
		    ps->Scan.BufGet.red.bp = ps->Scan.BufBegin.red.bp;

		if( ps->Scan.BufGet.green.bp >= ps->Scan.BufEnd.green.bp )
		    ps->Scan.BufGet.green.bp = ps->Scan.BufBegin.green.bp;

		return _TRUE;
    }
}

static Bool fnReadOutScanner( pScanData ps )
{
    if( ps->Scan.bd_rk.wBlueDiscard ) {

        ps->Scan.bd_rk.wBlueDiscard--;
    	ps->AsicReg.RD_ModeControl = _ModeFifoBSel;

	    IOReadScannerImageData( ps, ps->Bufs.b1.pReadBuf,
                                    ps->DataInf.dwAsicBytesPerPlane );

    	if( ps->Scan.gd_gk.wGreenDiscard ) {
            ps->Scan.gd_gk.wGreenDiscard--;

	        ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
    	    IOReadScannerImageData( ps, ps->Bufs.b1.pReadBuf,
                                        ps->DataInf.dwAsicBytesPerPlane);
    	}
	    return _FALSE;

    } else {
        IOReadColorData( ps, ps->Bufs.b1.pReadBuf,
                             ps->DataInf.dwAsicBytesPerPlane );
	    return _TRUE;
    }
}

/** Interpolates the gray data by using averaged the continuous pixels
 */
static void fnP96GrayDirect( pScanData ps, pVoid pBuf, pVoid pImg, ULong bl )
{
	pUChar src, dest;

	src  = (pUChar)pImg;
	dest = (pUChar)pBuf;
	
    for (; bl; bl--, src++, dest++ )
		*dest = ps->pbMapRed [*src];
}

/** This routine used in the condition:
 * 1) The data type is B/W or GrayScale.
 * 2) The required horizontal resolution doesn't exceed the optic spec.
 * 3) The required vertical resolution exceeds the optic spec.
 * So, the vertcal lines have to average with previous line to smooth the
 * image.
 */
static void fnDataDirect( pScanData ps, pVoid pBuf, pVoid pImg, ULong bl )
{
	_VAR_NOT_USED( ps );
	memcpy( pBuf, pImg, bl );
}

/** According to dither matrix to convert the input gray scale data into
 * one-bit data.
 */
static void fnHalftoneDirect0( pScanData ps, pVoid pb, pVoid pImg, ULong bL )
{
	pUChar pDither, src, dest;
	ULong  dw;

	src  = (pUChar)pImg;
	dest = (pUChar)pb;
   
	pDither = &ps->a_bDitherPattern[ps->dwDitherIndex];

    for( ; bL; bL--, dest++, pDither -= 8 ) {

		for( dw = 8; dw; dw--, src++, pDither++ ) {

		    if( *src < *pDither ) {
				*dest = (*dest << 1) | 0x01;
			} else {
				*dest <<= 1;
			}
		}
	}
	ps->dwDitherIndex = (ps->dwDitherIndex + 8) & 0x3f;
}

/** use random generator to make halftoning
 */
static void fnHalftoneDirect1( pScanData ps, pVoid pb, pVoid pImg, ULong bL )
{
	pUChar src, dest;
    UChar  threshold;
    ULong  dw;

	_VAR_NOT_USED( ps );
	src  = (pUChar)pImg;
	dest = (pUChar)pb;

	for (; bL; bL--, dest++ ) {

		for (dw = 8; dw; dw--, src++ ) {

		    threshold = (UChar)MiscLongRand();

		    if (*src < threshold ) {
				*dest = (*dest << 1) | 0x01;
			} else {
				*dest <<= 1;
			}
		}
	}
}

/** Merges the color planes to pixels style without enlarge operation.
 */
static void fnP98ColorDirect( pScanData ps, pVoid pb, pVoid pImg, ULong bL )
{
	pUChar      src;
	pRGBByteDef dest;
	
	src  = (pUChar)pImg;
	dest = (pRGBByteDef)pb;
	
	for ( bL = ps->DataInf.dwAsicPixelsPerPlane; bL; bL--, src++, dest++) {

		dest->Red   = *src;
		dest->Green = src[ps->DataInf.dwAsicPixelsPerPlane];
		dest->Blue  = src[ps->DataInf.dwAsicPixelsPerPlane*2];
	}
}

static void fnP96ColorDirect( pScanData ps, pVoid pb, pVoid pImg, ULong bL )
{
	pUChar      src;
	pRGBByteDef dest;

	src  = (pUChar)pImg;
	dest = (pRGBByteDef)pb;
		
	for ( bL = ps->DataInf.dwAsicPixelsPerPlane; bL; bL--, dest++, src++) {

			dest->Red  =ps->pbMapRed[*src];
			dest->Green=ps->pbMapGreen[src[ps->DataInf.dwAsicPixelsPerPlane]];
			dest->Blue =ps->pbMapBlue[src[ps->DataInf.dwAsicPixelsPerPlane*2]];
    }
}

/** Merges the color planes to pixels style without enlarge operation.
 * The scanner returns the pixel data in Motorola-Format, so we have to swap
 */
static void fnP98Color48( pScanData ps, pVoid pb, pVoid pImg, ULong bL )
{
	pUShort       src;
	pRGBUShortDef dest;
	
	register ULong i;

	_VAR_NOT_USED( bL );
	src  = (pUShort)pImg;
	dest = (pRGBUShortDef)pb;
	
	for ( i = ps->DataInf.dwAsicPixelsPerPlane; i;	i--, src++, dest++) {

		dest->Red   = *src;
		dest->Green = src[ps->DataInf.dwAsicPixelsPerPlane];
		dest->Blue  = src[ps->DataInf.dwAsicPixelsPerPlane * 2];
    }
}

/** prepare for scanning
 */
static int imageP98SetupScanSettings( pScanData ps, pScanInfo pInf )
{
	UShort brightness;

	DBG( DBG_LOW, "imageP98SetupScanSettings()\n" );

    ps->DataInf.dwScanFlag = pInf->ImgDef.dwFlag;
    ps->DataInf.dwVxdFlag  = 0;
    ps->DataInf.crImage    = pInf->ImgDef.crArea;

    /* AdjustOriginXByLens
     * [NOTE]
     *	Here we just simply adjust it to double (600 DPI is two times of
     *	300 DPI), but if this model is a multi-lens scanner, we should adjust
     *	it according to different lens.
	 */
    ps->DataInf.crImage.x <<= 1;

    ps->DataInf.xyAppDpi 	 = pInf->ImgDef.xyDpi;
    ps->DataInf.siBrightness = pInf->siBrightness;
    ps->DataInf.wDither      = pInf->wDither;
    ps->DataInf.wAppDataType = pInf->ImgDef.wDataType;

    ps->GetImageInfo( ps, &pInf->ImgDef );

    if (ps->DataInf.dwVxdFlag & _VF_DATATOUSERBUFFER) {
		ps->Scan.DataProcess = fnDataDirect;
	}
    if (ps->DataInf.dwScanFlag & SCANDEF_BmpStyle) {
		ps->Scan.lBufferAdjust = -(Long)ps->DataInf.dwAppBytesPerLine;
	} else {
		ps->Scan.lBufferAdjust = (Long)ps->DataInf.dwAppBytesPerLine;
	}

	DBG( DBG_LOW, "Scan settings:\n" );
	DBG( DBG_LOW, "ImageInfo: (x=%u,y=%u,dx=%u,dy=%u)\n",
			 ps->DataInf.crImage.x,  ps->DataInf.crImage.y,
			 ps->DataInf.crImage.cx, ps->DataInf.crImage.cy );

    /*
	 * SetBwBrightness
     * [NOTE]
     *
     *	 0                   _DEF_BW_THRESHOLD					   255
     *	 +-------------------------+--------------------------------+
     *	 |<------- Black --------->|<----------- White ------------>|
     *	 So, if user wish to make image darker, the threshold value should be
     *	 higher than _defBwThreshold, otherwise it should lower than the
     *	 _DefBwThreshold.
     *	 Darker = _DefBwThreshold + White * Input / 127;
     *		  Input < 0, and White = 255 - _DefBwThreshold, so
     *		= _DefBwThreshold - (255 - _DefBwThreshold) * Input / 127;
     *	 The brighter is the same idea.
	 *
 	 * CHECK: it seems that the brightness only works for the binary mode !
	 */
	if( ps->DataInf.wPhyDataType != COLOR_BW ) {/* if not line art 			*/
		ps->wBrightness = pInf->siBrightness;   /* use internal tables for 	*/
		ps->wContrast   = pInf->siContrast;		/* brightness and contrast	*/

		pInf->siBrightness = 0;				/* don't use asic for threshold */
    }

/* CHECK: We have now two methods for setting the brightness...
*/
	DBG( DBG_LOW, "brightness = %i\n", pInf->siBrightness );

    if (ps->DataInf.siBrightness < 0) {
		brightness = (UShort)(_DEF_BW_THRESHOLD -
			       (255 - _DEF_BW_THRESHOLD) * ps->DataInf.siBrightness /127);
	} else {
		brightness = (UShort)(_DEF_BW_THRESHOLD -
					       _DEF_BW_THRESHOLD * ps->DataInf.siBrightness /127);
	}
    ps->AsicReg.RD_ThresholdControl = brightness;

	DBG( DBG_LOW, "1. brightness = %i\n", brightness );

    if( ps->DataInf.siBrightness >= 0 ) {
	    brightness = (short)((long)(-(255 - _DEF_BW_THRESHOLD) *
						 ps->DataInf.siBrightness) / 127 + _DEF_BW_THRESHOLD);
	} else {
	    brightness = (short)((long)(_DEF_BW_THRESHOLD *
 						  ps->DataInf.siBrightness) / 127 + _DEF_BW_THRESHOLD);
	}

	brightness = (brightness ^ 0xff) & 0xff;

	if( _ASIC_IS_98003 == ps->sCaps.AsicID ) {
	    ps->AsicReg.RD_ThresholdControl = brightness;
		DBG( DBG_LOW, "2. brightness = %i\n", brightness );
	}

	ps->DataInf.pCurrentBuffer = ps->pScanBuffer1;

	return _OK;
}

/**
 */
static void imageP98DoCopyBuffer( pScanData ps, pUChar pImage )
{
	memcpy( ps->pFilterBuf, pImage, ps->DataInf.dwAsicBytesPerPlane );

	ps->pFilterBuf += 5120;
    if (ps->pFilterBuf >= ps->pEndBuf)
		ps->pFilterBuf = ps->pProcessingBuf;
}

/**
 */
static Bool imageP98CopyToFilterBuffer( pScanData ps, pUChar pImage )
{
    if (ps->fDoFilter) {

		if (ps->fFilterFirstLine) {

		    imageP98DoCopyBuffer( ps, pImage );
		    imageP98DoCopyBuffer( ps, pImage );
		    ps->dwLinesFilter--;
		    return _FALSE;
	} else {

		    imageP98DoCopyBuffer( ps, pImage );
		    if ((ps->dwLinesFilter--) == 0)
				imageP98DoCopyBuffer( ps, pImage);
		}
    }
    return _TRUE;
}

/**
 */
static void imageP98UnSharpCompare( pScanData ps, Byte Center,
                                    Byte Neighbour, pLong pdwNewValue )
{
    Byte b;

    b = (Center >= Neighbour) ? Center - Neighbour : Neighbour - Center ;

    if (b > ps->bOffsetFilter) {

		*pdwNewValue -= (Long)Neighbour;
		ps->dwDivFilter--;
    }
}

/**
 */
static void imageP98DoFilter( pScanData ps, pUChar pPut )
{
	ULong dw;
    Long  dwNewValue;

    if (ps->fDoFilter && (ps->DataInf.xyAppDpi.x) >= 600UL) {

		/* DoUnsharpMask(); */
		for (dw = 0; dw < ps->DataInf.dwAsicBytesPerPlane - 2; dw++, pPut++) {

			ps->dwDivFilter = ps->dwMul;

		    dwNewValue = ((ULong)ps->pGet2[dw+1]) * ps->dwMul;
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet1[dw], &dwNewValue);
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet1[dw+1], &dwNewValue);
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet1[dw+2], &dwNewValue);
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet2[dw], &dwNewValue);
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet2[dw+2], &dwNewValue);
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet3[dw], &dwNewValue);
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet3[dw+1], &dwNewValue);
			imageP98UnSharpCompare( ps, ps->pGet2[dw+1], ps->pGet3[dw+2], &dwNewValue);

    	    if( dwNewValue > 0 ) {
				if((dwNewValue /= ps->dwDivFilter) < 255) {
		    		*pPut = (Byte) dwNewValue;
				} else {
				    *pPut = 255;
				}
		    } else {
				*pPut = 0;
			}
		}
		pPut = ps->pGet1;
		ps->pGet1 = ps->pGet2;
		ps->pGet2 = ps->pGet3;
		ps->pGet3 = pPut;
    }
}

/**
 */
static Bool imageP98DataIsReady( pScanData ps )
{
	Byte b;

	ps->Scan.fMotorBackward = _FALSE;
	ps->bMoveDataOutFlag    = _DataAfterRefreshState;

    b = (ps->DataInf.wPhyDataType >= COLOR_TRUE24) ?
	                                      _BLUE_DATA_READY : _GREEN_DATA_READY;
    while( _TRUE ) {

		ps->dwColorRunIndex ++;

		if(ps->pColorRunTable[ps->dwColorRunIndex] & b)
	    	break;
    }

    if (b == _GREEN_DATA_READY) {

		ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
		IOReadScannerImageData( ps, ps->DataInf.pCurrentBuffer,
			 	    		        ps->DataInf.dwAsicBytesPerPlane );

		imageP98CopyToFilterBuffer( ps, ps->DataInf.pCurrentBuffer );
    } else  {

		/* ReadColorImageData() */
		if( ps->DataInf.dwScanFlag & SCANDEF_ColorBGROrder ) {

			ps->AsicReg.RD_ModeControl = _ModeFifoRSel;
			IOReadScannerImageData( ps, ps->pScanBuffer1 +
				         			    ps->DataInf.dwAsicBytesPerPlane * 2,
									    ps->DataInf.dwAsicBytesPerPlane );

			ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
			IOReadScannerImageData( ps, ps->pScanBuffer1 +
										ps->DataInf.dwAsicBytesPerPlane,
										ps->DataInf.dwAsicBytesPerPlane );

			ps->AsicReg.RD_ModeControl = _ModeFifoBSel;
			IOReadScannerImageData( ps, ps->pScanBuffer1,
				 					    ps->DataInf.dwAsicBytesPerPlane );
		} else {

            IOReadColorData( ps, ps->pScanBuffer1,
                                 ps->DataInf.dwAsicBytesPerPlane );
		}
    }

	if (ps->fFilterFirstLine) {
		ps->fFilterFirstLine = _FALSE;
	    return _TRUE;
	}

	imageP98DoFilter( ps, ps->DataInf.pCurrentBuffer );

    (*ps->Scan.DataProcess)( ps, ps->Scan.bp.pMonoBuf,
                             ps->DataInf.pCurrentBuffer,
                             ps->DataInf.dwAppPhyBytesPerLine );

    return _TRUE;
}

/** here we wait for one data-line
 */
static Bool imageP98001ReadOneImageLine( pScanData ps )
{
	ULong    dwFifoCounter;
	TimerDef timer;

	MiscStartTimer( &timer, _LINE_TIMEOUT );
    do {

		ps->Scan.bNowScanState = IOGetScanState( ps, _FALSE );
		dwFifoCounter = IOReadFifoLength( ps );

		if (!(ps->Scan.bNowScanState & _SCANSTATE_STOP) &&
								    (dwFifoCounter < ps->dwMaxReadFifoData)) {

		    if( ps->Scan.bOldScanState != ps->Scan.bNowScanState )
				ps->UpdateDataCurrentReadLine( ps );

		    if( dwFifoCounter >= ps->Scan.dwMinReadFifo )
				return imageP98DataIsReady( ps );

		} else {	/* ScanStateIsStop */

		    if (dwFifoCounter >= ps->dwSizeMustProcess)
				return imageP98DataIsReady( ps );

			ps->UpdateDataCurrentReadLine( ps );

		    if( dwFifoCounter >= ps->Scan.dwMinReadFifo )
				return imageP98DataIsReady( ps );
		}

		_DODELAY(10);			     /* delay 10 ms */

    } while (!MiscCheckTimer( &timer ));

	DBG( DBG_HIGH, "Timeout - Scanner malfunction !!\n" );
	MotorToHomePosition(ps);

	/* timed out, scanner malfunction */
    return _FALSE;
}

/** calculate the image properties according to the scanmode
 */
static void imageP98GetInfo( pScanData ps, pImgDef pImgInf )
{
	DBG( DBG_LOW, "imageP98GetInfo()\n" );

	ps->DataInf.xyPhyDpi.x = imageGetPhysDPI( ps, pImgInf, _TRUE  );
	ps->DataInf.xyPhyDpi.y = imageGetPhysDPI( ps, pImgInf, _FALSE );

	DBG( DBG_LOW, "xyPhyDpi.x = %u, xyPhyDpi.y = %u\n",
		 ps->DataInf.xyPhyDpi.x, ps->DataInf.xyPhyDpi.y );

	DBG( DBG_LOW, "crArea.x = %u, crArea.y = %u\n",
		 pImgInf->crArea.x, pImgInf->crArea.y );

	DBG( DBG_LOW, "crArea.cx = %u, crArea.cy = %u\n",
		 pImgInf->crArea.cx, pImgInf->crArea.cy );

	ps->DataInf.XYRatio = 1000 * ps->DataInf.xyPhyDpi.y/ps->DataInf.xyPhyDpi.x;
	DBG( DBG_LOW, "xyDpi.x = %u, xyDpi.y = %u, XYRatio = %u\n",
	               pImgInf->xyDpi.x, pImgInf->xyDpi.y, ps->DataInf.XYRatio );

	ps->DataInf.dwAppLinesPerArea = (ULong)pImgInf->crArea.cy *
									  pImgInf->xyDpi.y / _MEASURE_BASE;

    ps->DataInf.dwAppPixelsPerLine = (ULong)pImgInf->crArea.cx *
									   pImgInf->xyDpi.x / _MEASURE_BASE;

	ps->DataInf.dwPhysBytesPerLine = (ULong)pImgInf->crArea.cx *
									   ps->DataInf.xyPhyDpi.x / _MEASURE_BASE;

    if( pImgInf->wDataType <= COLOR_HALFTONE ) {
		ps->DataInf.dwAsicPixelsPerPlane = (ps->DataInf.dwAppPixelsPerLine+7UL)&
											 0xfffffff8UL;
		ps->DataInf.dwAppPhyBytesPerLine =
		ps->DataInf.dwAppBytesPerLine 	 =
		ps->DataInf.dwAsicBytesPerLine   =
		ps->DataInf.dwAsicBytesPerPlane  = ps->DataInf.dwAsicPixelsPerPlane>>3;
    } else {
		ps->DataInf.dwAsicBytesPerPlane  =
		ps->DataInf.dwAsicPixelsPerPlane = ps->DataInf.dwAppPixelsPerLine;
	}

    if( COLOR_TRUE48 == pImgInf->wDataType ) {
		ps->DataInf.dwAsicBytesPerPlane *= 2;
	}

    switch( pImgInf->wDataType ) {

	case COLOR_BW:
	    ps->DataInf.dwVxdFlag |= _VF_DATATOUSERBUFFER;
	    ps->DataInf.wPhyDataType = COLOR_BW;
        ps->Shade.bIntermediate  = _ScanMode_Mono;
	    break;

	case COLOR_HALFTONE:
		if( ps->DataInf.wDither == 2 ) {
            ps->Scan.DataProcess = fnHalftoneDirect1;
		} else {
            ps->Scan.DataProcess = fnHalftoneDirect0;
		}
/*
 * CHANGE: it seems, that we have to use the same settings as for 256GRAY
 */
		ps->DataInf.dwAsicBytesPerPlane =
		ps->DataInf.dwAsicPixelsPerPlane = ps->DataInf.dwAppPixelsPerLine;
	    ps->DataInf.wPhyDataType = COLOR_256GRAY;
        ps->Shade.bIntermediate  = _ScanMode_Mono;
	    break;

	case COLOR_256GRAY:
	    ps->DataInf.dwVxdFlag |= _VF_DATATOUSERBUFFER;
	    ps->DataInf.dwAsicBytesPerLine =
	    ps->DataInf.dwAppPhyBytesPerLine = ps->DataInf.dwAppPixelsPerLine;
	    ps->DataInf.wPhyDataType = COLOR_256GRAY;
        ps->Shade.bIntermediate  = _ScanMode_Mono;
	    break;

	case COLOR_TRUE24:
        ps->Scan.DataProcess = fnP98ColorDirect;
	    ps->DataInf.dwAsicBytesPerLine =
	    ps->DataInf.dwAppPhyBytesPerLine = ps->DataInf.dwAppPixelsPerLine * 3;
	    ps->DataInf.wPhyDataType = COLOR_TRUE24;
        ps->Shade.bIntermediate  = _ScanMode_Color;
	    break;

	case COLOR_TRUE48:
        ps->Scan.DataProcess = fnP98Color48;
	    ps->DataInf.dwAsicBytesPerLine =
	    ps->DataInf.dwAppPhyBytesPerLine = ps->DataInf.dwAppPixelsPerLine * 6;
	    ps->DataInf.wPhyDataType = COLOR_TRUE48;
        ps->Shade.bIntermediate  = _ScanMode_Color;
	    break;

    }

    if (pImgInf->dwFlag & SCANDEF_BoundaryDWORD) {
		ps->DataInf.dwAppBytesPerLine = (ps->DataInf.dwAppPhyBytesPerLine + 3) &
									      0xfffffffc;
	} else {
		if (pImgInf->dwFlag & SCANDEF_BoundaryWORD) {
		    ps->DataInf.dwAppBytesPerLine = (ps->DataInf.dwAppPhyBytesPerLine + 1) &
											  0xfffffffe;
		} else {
		    ps->DataInf.dwAppBytesPerLine = ps->DataInf.dwAppPhyBytesPerLine;
		}
	}

	DBG( DBG_LOW, "AppLinesPerArea    = %u\n", ps->DataInf.dwAppLinesPerArea    );
	DBG( DBG_LOW, "AppPixelsPerLine   = %u\n", ps->DataInf.dwAppPixelsPerLine   );
	DBG( DBG_LOW, "AppPhyBytesPerLine = %u\n", ps->DataInf.dwAppPhyBytesPerLine );
	DBG( DBG_LOW, "AppBytesPerLine    = %u\n", ps->DataInf.dwAppBytesPerLine    );
	DBG( DBG_LOW, "AsicPixelsPerPlane = %u\n", ps->DataInf.dwAsicPixelsPerPlane );
	DBG( DBG_LOW, "AsicBytesPerPlane  = %u\n", ps->DataInf.dwAsicBytesPerPlane  );
	DBG( DBG_LOW, "AsicBytesPerLine   = %u\n", ps->DataInf.dwAsicBytesPerLine   );
	DBG( DBG_LOW, "Physical Bytes     = %u\n", ps->DataInf.dwPhysBytesPerLine   );
}

/**
 */
static void imageP96GetInfo( pScanData ps, pImgDef pImgInf )
{
	DBG( DBG_LOW, "imageP96GetInfo()\n" );

	ps->DataInf.xyPhyDpi.x = imageGetPhysDPI( ps, pImgInf, _TRUE  );
	ps->DataInf.xyPhyDpi.y = imageGetPhysDPI( ps, pImgInf, _FALSE );

	DBG( DBG_LOW, "xyPhyDpi.x = %u, xyPhyDpi.y = %u\n",
		 ps->DataInf.xyPhyDpi.x, ps->DataInf.xyPhyDpi.y );

	DBG( DBG_LOW, "crArea.x = %u, crArea.y = %u\n",
		 pImgInf->crArea.x, pImgInf->crArea.y );

	DBG( DBG_LOW, "crArea.cx = %u, crArea.cy = %u\n",
		 pImgInf->crArea.cx, pImgInf->crArea.cy );

	ps->DataInf.XYRatio = 1000 * ps->DataInf.xyPhyDpi.y/ps->DataInf.xyPhyDpi.x;
	DBG( DBG_LOW, "xyDpi.x = %u, xyDpi.y = %u, XYRatio = %u\n",
	               pImgInf->xyDpi.x, pImgInf->xyDpi.y, ps->DataInf.XYRatio );

	ps->DataInf.dwAppLinesPerArea = (ULong)pImgInf->crArea.cy *
					  					   pImgInf->xyDpi.y / _MEASURE_BASE;
    ps->DataInf.dwAsicBytesPerPlane  =
	ps->DataInf.dwAsicPixelsPerPlane = (ULong)ps->DataInf.xyPhyDpi.x *
				 					       pImgInf->crArea.cx / _MEASURE_BASE;

    ps->DataInf.dwAppPixelsPerLine = (ULong)pImgInf->crArea.cx *
										    pImgInf->xyDpi.x / _MEASURE_BASE;

	ps->DataInf.dwPhysBytesPerLine = (ULong)pImgInf->crArea.cx *
									   ps->DataInf.xyPhyDpi.x / _MEASURE_BASE;

    ps->DataInf.wPhyDataType = ps->DataInf.wAppDataType;

    switch( pImgInf->wDataType ) {

	case COLOR_BW:
	    ps->DataInf.dwAsicBytesPerPlane =
								 (ps->DataInf.dwAsicPixelsPerPlane + 7) >> 3;
	    ps->DataInf.dwAppPhyBytesPerLine =
									 (ps->DataInf.dwAppPixelsPerLine + 7) >> 3;
	    ps->DataInf.dwVxdFlag |= _VF_DATATOUSERBUFFER;
        ps->Scan.DataProcess = fnDataDirect;
	    break;

	case COLOR_HALFTONE:
	    ps->DataInf.dwAppPhyBytesPerLine =
								 (ps->DataInf.dwAsicPixelsPerPlane + 7) >> 3;
		if( ps->DataInf.wDither == 2 ) {
            ps->Scan.DataProcess = fnHalftoneDirect1;
		} else {
            ps->Scan.DataProcess = fnHalftoneDirect0;
		}
	    ps->DataInf.wPhyDataType = COLOR_256GRAY;
	    break;

	case COLOR_256GRAY:
	    ps->DataInf.dwAppPhyBytesPerLine = ps->DataInf.dwAppPixelsPerLine;
        ps->Scan.DataProcess = fnP96GrayDirect;
	    break;

	case COLOR_TRUE24:
#ifdef _A3I_EN
        ps->Scan.DataProcess = fnP98ColorDirect;
#else
        ps->Scan.DataProcess = fnP96ColorDirect;
#endif
	    ps->DataInf.dwAppPhyBytesPerLine = ps->DataInf.dwAppPixelsPerLine * 3;
    }

    if( pImgInf->dwFlag & SCANDEF_BoundaryDWORD ) {
		ps->DataInf.dwAppBytesPerLine =
							(ps->DataInf.dwAppPhyBytesPerLine + 3) & 0xfffffffc;
	} else {
		if ( pImgInf->dwFlag & SCANDEF_BoundaryWORD ) {
		    ps->DataInf.dwAppBytesPerLine =
							(ps->DataInf.dwAppPhyBytesPerLine + 1) & 0xfffffffe;
		} else {
	    	ps->DataInf.dwAppBytesPerLine = ps->DataInf.dwAppPhyBytesPerLine;
		}
	}

    if (ps->DataInf.wPhyDataType == COLOR_TRUE24)
		ps->DataInf.dwAsicBytesPerLine = ps->DataInf.dwAsicBytesPerPlane * 3;
    else
		ps->DataInf.dwAsicBytesPerLine = ps->DataInf.dwAsicBytesPerPlane;

/* WORK: AsicBytesPerLine only used for ASIC_98001 based scanners - try to remove
**		 that, also try to remove redundant info
*/
	DBG( DBG_LOW, "AppLinesPerArea    = %u\n", ps->DataInf.dwAppLinesPerArea    );
	DBG( DBG_LOW, "AppPixelsPerLine   = %u\n", ps->DataInf.dwAppPixelsPerLine   );
	DBG( DBG_LOW, "AppPhyBytesPerLine = %u\n", ps->DataInf.dwAppPhyBytesPerLine );
	DBG( DBG_LOW, "AppBytesPerLine    = %u\n", ps->DataInf.dwAppBytesPerLine    );
	DBG( DBG_LOW, "AsicPixelsPerPlane = %u\n", ps->DataInf.dwAsicPixelsPerPlane );
	DBG( DBG_LOW, "AsicBytesPerPlane  = %u\n", ps->DataInf.dwAsicBytesPerPlane  );
	DBG( DBG_LOW, "AsicBytesPerLine   = %u\n", ps->DataInf.dwAsicBytesPerLine   );
	DBG( DBG_LOW, "Physical Bytes     = %u\n", ps->DataInf.dwPhysBytesPerLine   );
}

/** here we wait for one data-line
 */
static Bool imageP96ReadOneImageLine( pScanData ps )
{
    Bool     result = _FALSE;
	Byte	 bData, bFifoCount;
    TimerDef timer;

	MiscStartTimer( &timer, _LINE_TIMEOUT);
    do {

		bFifoCount = IODataRegisterFromScanner( ps, ps->RegFifoOffset );

/* CHECK ps->bMoveDataOutFlag will never be set to _DataFromStopState !!!*/
#if 1
		if ((bFifoCount < ps->bMinReadFifo) &&
					            (ps->bMoveDataOutFlag == _DataFromStopState)) {

		    bData = IOGetScanState( ps, _FALSE);

		    if (!(bData & _SCANSTATE_STOP)) {
				if (bData < ps->bCurrentLineCount)
				    bData += _NUMBER_OF_SCANSTEPS;
				if ((bData - ps->bCurrentLineCount) < _SCANSTATE_BYTES)
				    continue;
		    }
	
		    ps->bMoveDataOutFlag = _DataAfterRefreshState;
		}
#endif

/*
// HEINER:A3I
//		if( ps->bMoveDataOutFlag != _DataFromStopState )
//	    	ps->UpdateDataCurrentReadLine( ps );
*/
		if( bFifoCount >= ps->bMinReadFifo ) {

		    /* data is ready */
		    for (; !(*ps->pCurrentColorRunTable &
				(ps->RedDataReady | ps->GreenDataReady | _BLUE_DATA_READY));
												 ps->pCurrentColorRunTable++);

#ifdef DEBUG			
			if( ps->pCurrentColorRunTable >
							(ps->pColorRunTable+ps->BufferForColorRunTable))
				DBG( DBG_LOW, "WARNING: pCurrentColorRunTab>pColorRunTable\n");
#endif

		    if (ps->DataInf.wPhyDataType == COLOR_TRUE24) {

				/* read color planes (either R/G/B or R/B/G sequence that
				 * depends on COLOR CCD, see below
				 */
				if (*ps->pCurrentColorRunTable & ps->b1stColor) {
				    *ps->pCurrentColorRunTable &= ps->b1stMask;
				    IOReadScannerImageData (ps, ps->pPutBufR,
						  					    ps->DataInf.dwAsicBytesPerPlane);
				    ps->pPutBufR += ps->BufferSizePerModel;
					if (ps->pPutBufR == ps->pEndBufR)
						ps->pPutBufR = ps->pPrescan16;
				} else
					if (*ps->pCurrentColorRunTable & ps->b2ndColor) {
						*ps->pCurrentColorRunTable &= ps->b2ndMask;
						IOReadScannerImageData( ps, ps->pPutBufG,
										        ps->DataInf.dwAsicBytesPerPlane);
						ps->pPutBufG += ps->BufferSizePerModel;
						if (ps->pPutBufG == ps->pEndBufG)
						    ps->pPutBufG = ps->pPrescan8;
		    	} else {
					*ps->pCurrentColorRunTable &= ps->b3rdMask;
					ps->pCurrentColorRunTable++;    /* processed this step */

					/* according to CCD type & image placement method to
					 * read third color into corresponding location.
					 * SONY CCD: Red, Green and Blue.
					 * TOSHIBA CCD: Red, Blue and Green.
					 * SCANDEF_BmpStyle: Blue, Green and Red, Otherwise
					 *  Red, Green and Blue.
					 */
					if (ps->b3rdColor & ps->GreenDataReady) {
					    /* Green always in middle */
					    IOReadScannerImageData (ps,
                                                 ps->DataInf.pCurrentBuffer +
											  ps->DataInf.dwAsicBytesPerPlane,
											  ps->DataInf.dwAsicBytesPerPlane);
					} else {
					    /* Blue depends the request style from caller */
					    if (ps->DataInf.dwScanFlag & SCANDEF_BmpStyle) {
							/* BMP style, blue is the first one */
							IOReadScannerImageData (ps,
                                                   ps->DataInf.pCurrentBuffer,
											  ps->DataInf.dwAsicBytesPerPlane);
						} else {
							/* Blue is the last one */
							IOReadScannerImageData (ps, ps->DataInf.pCurrentBuffer +
											ps->DataInf.dwAsicBytesPerPlane * 2,
											  ps->DataInf.dwAsicBytesPerPlane);
						}
					}

					/* reassemble 3 color lines for separated RGB value */
    				if (ps->DataInf.dwScanFlag & SCANDEF_BmpStyle) {
					    /* BMP style, red is last one */
					    memcpy( ps->DataInf.pCurrentBuffer +
								ps->DataInf.dwAsicBytesPerPlane * 2,
								ps->pGetBufR, ps->DataInf.dwAsicBytesPerPlane);
					} else {
					    /* Red is first one */
					    memcpy( ps->DataInf.pCurrentBuffer,
		 					    ps->pGetBufR, ps->DataInf.dwAsicBytesPerPlane );
					}

					if (ps->b2ndColor & ps->GreenDataReady) {
					    /* Green always in middle */
						memcpy( ps->DataInf.pCurrentBuffer +
							   ps->DataInf.dwAsicBytesPerPlane,
							   ps->pGetBufG, ps->DataInf.dwAsicBytesPerPlane);
					} else {
				    	/* Blue depends the request style from caller */
					    if (ps->DataInf.dwScanFlag & SCANDEF_BmpStyle) {
							/* BMP style, blue is the first one */
							memcpy( ps->DataInf.pCurrentBuffer,
							        ps->pGetBufG,
							        ps->DataInf.dwAsicBytesPerPlane);
						} else {
							/* BMP style, blue is the last one */
							memcpy( ps->DataInf.pCurrentBuffer +
							        ps->DataInf.dwAsicBytesPerPlane * 2,
							        ps->pGetBufG,
							        ps->DataInf.dwAsicBytesPerPlane );
						}
					}

					/* Adjust the get pointers */
					ps->pGetBufR += ps->BufferSizePerModel;
					ps->pGetBufG += ps->BufferSizePerModel;
					if (ps->pGetBufR == ps->pEndBufR)
					    ps->pGetBufR = ps->pPrescan16;

					if (ps->pGetBufG == ps->pEndBufG)
					    ps->pGetBufG = ps->pPrescan8;

    				result = _TRUE; /* Line data in buffer */
                    break;
			    }

				/* reset timer for new 10-second interval */
				MiscStartTimer( &timer, (10 * _SECOND));

		    } else  {
				/* Gray Image */
				*ps->pCurrentColorRunTable &= 0xf0; /* leave high nibble for debug  */
				ps->pCurrentColorRunTable++;		/* this step has been processed */
				IOReadScannerImageData( ps, ps->DataInf.pCurrentBuffer,
									        ps->DataInf.dwAsicBytesPerPlane );

				result = _TRUE;
                break;
		    }
		}

/* HEINER:A3I */
		if( ps->bMoveDataOutFlag != _DataFromStopState )
	    	ps->UpdateDataCurrentReadLine( ps );

    } while (!MiscCheckTimer( &timer));

    if( _TRUE == result ) {
        (*ps->Scan.DataProcess)( ps, ps->Scan.bp.pMonoBuf,
                                 ps->DataInf.pCurrentBuffer,
                                 ps->DataInf.dwAppPhyBytesPerLine );
        return _TRUE;
    }

	DBG( DBG_HIGH, "Timeout - Scanner malfunction !!\n" );
	MotorToHomePosition(ps);

    return _FALSE;
}

/** prepare for scanning
 */
static int imageP96SetupScanSettings( pScanData ps, pScanInfo pInf )
{
	DBG( DBG_LOW, "imageSetupP96ScanSettings()\n" );

	ps->DataInf.dwVxdFlag = 0;
	if (pInf->ImgDef.dwFlag & SCANDEF_BuildBwMap)
		ps->DataInf.dwVxdFlag |= _VF_BUILDMAP;

    ps->DataInf.dwScanFlag = pInf->ImgDef.dwFlag;

    ps->DataInf.crImage = pInf->ImgDef.crArea;

	/* scale according to DPI */
    ps->DataInf.crImage.x  *= ps->PhysicalDpi / _MEASURE_BASE;
    ps->DataInf.crImage.cx *= ps->PhysicalDpi / _MEASURE_BASE;

    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {
		ps->DataInf.crImage.x += _Transparency48OriginOffsetX;
		ps->DataInf.crImage.y += _Transparency48OriginOffsetY;
    }

    ps->DataInf.xyAppDpi     = pInf->ImgDef.xyDpi;
    ps->DataInf.wAppDataType = pInf->ImgDef.wDataType;
    ps->DataInf.wDither      = pInf->wDither;

    ps->GetImageInfo( ps, &pInf->ImgDef );

	/* try to get brightness to work */
	if (ps->DataInf.wPhyDataType != COLOR_BW) { /* if not line art 			*/
		ps->wBrightness = pInf->siBrightness;   /* use internal tables for 	*/
		ps->wContrast   = pInf->siContrast;		/* brightness and contrast	*/

		pInf->siBrightness = 0;				/* don't use asic for threshold */
    }
    ps->DataInf.siBrightness = pInf->siBrightness;

    if (ps->DataInf.dwScanFlag & SCANDEF_BmpStyle)
		ps->Scan.lBufferAdjust = -(Long)ps->DataInf.dwAppBytesPerLine;
    else
		ps->Scan.lBufferAdjust = (Long)ps->DataInf.dwAppBytesPerLine;

    if (ps->DataInf.siBrightness < 0)
		ps->DataInf.siBrightness = 255 - (_DEF_BW_THRESHOLD *
							ps->DataInf.siBrightness / 127 + _DEF_BW_THRESHOLD);
    else
		ps->DataInf.siBrightness = 255 - ((255 - _DEF_BW_THRESHOLD) *
							 ps->DataInf.siBrightness / 127 + _DEF_BW_THRESHOLD);

    ps->AsicReg.RD_ThresholdControl = (Byte)ps->DataInf.siBrightness;

    ps->DataInf.pCurrentBuffer = ps->pScanBuffer1;

	return _OK;
}

/**
 */
static Bool imageP98003DataIsReady( pScanData ps )
{
	pUChar pb;

    if( ps->Scan.bDiscardAll ) {
    	ps->Scan.bDiscardAll--;

    	if( ps->DataInf.wPhyDataType <= COLOR_256GRAY ) {
    	    ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
    	    IOReadScannerImageData( ps, ps->Bufs.b1.pReadBuf,
                                        ps->DataInf.dwAsicBytesPerPlane );
    	} else {
            IOReadColorData( ps, ps->Bufs.b1.pReadBuf,
                                 ps->DataInf.dwAsicBytesPerPlane );
        }
	    return _FALSE;
    }

   	if( ps->DataInf.wPhyDataType <= COLOR_256GRAY ) {

    	ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
		pb = ps->Scan.bp.pMonoBuf;

		/* use a larger buffer during halftone reads...*/
		if( ps->DataInf.wAppDataType == COLOR_HALFTONE )
			pb = ps->Scan.BufPut.red.bp;

	    IOReadScannerImageData( ps, pb, ps->DataInf.dwAsicBytesPerPlane );

    } else {
    	if( !ps->Scan.DataRead( ps )) {
    	    return _FALSE;
        }
    }

    if( ps->Scan.DoSample( ps )) {

    	if( ps->Scan.dwLinesToRead == 1 &&
           !(IOGetScanState( ps, _TRUE ) & _SCANSTATE_STOP))
	        IORegisterToScanner( ps, ps->RegRefreshScanState );

		/* direct is done here without copying...*/
		if( fnDataDirect != ps->Scan.DataProcess ) {
			(*ps->Scan.DataProcess)(ps, (pVoid)(ps->Scan.bp.pMonoBuf ),
                                        (pVoid)(ps->Scan.BufPut.red.bp),
                                        ps->DataInf.dwAppPhyBytesPerLine);
		}
    	return _TRUE;
    }

    return _FALSE;
}

/**
 */
static Bool imageP98003ReadOneImageLine( pScanData ps )
{
    Byte     b, state;
	TimerDef timer, t2;

	MiscStartTimer( &timer, _LINE_TIMEOUT );
	MiscStartTimer( &t2, _SECOND*2 );
    do {

        state = IOGetScanState( ps, _TRUE );
        ps->Scan.bNowScanState = (state & _SCANSTATE_MASK);

    	if( state & _SCANSTATE_STOP ) {

	        MotorP98003ModuleForwardBackward( ps );

    	    if( IOReadFifoLength( ps ) >= ps->Scan.dwMinReadFifo )
	        	if( imageP98003DataIsReady( ps ))
		            return _TRUE;
    	} else {

    	    ps->Scan.bModuleState = _MotorInNormalState;
    	    b = ps->Scan.bNowScanState - ps->Scan.bOldScanState;

    	    if((char) b < 0)
        		b += _NUMBER_OF_SCANSTEPS;

    	    if( b >= ps->Scan.bRefresh ) {
        		IORegisterToScanner( ps, ps->RegRefreshScanState );

                ps->Scan.bOldScanState = IOGetScanState( ps, _TRUE );
                ps->Scan.bOldScanState &= _SCANSTATE_MASK;
    	    }

	        if( IOReadFifoLength( ps ) >= ps->Scan.dwMaxReadFifo ) {
            	if( imageP98003DataIsReady( ps ))
	                return _TRUE;
    	    } else {

        	    b = ps->Scan.bNowScanState - ps->Scan.bOldScanState;

    	        if((char) b < 0)
        	    	b += _NUMBER_OF_SCANSTEPS;

        	    if( b >= ps->Scan.bRefresh ) {
            		IORegisterToScanner( ps, ps->RegRefreshScanState );

                    ps->Scan.bOldScanState = IOGetScanState( ps, _TRUE );
                    ps->Scan.bOldScanState &= _SCANSTATE_MASK;
        	    }

        	    if( IOReadFifoLength( ps ) >= ps->Scan.dwMinReadFifo ) {
                	if( imageP98003DataIsReady( ps ))
	                    return _TRUE;
                }
        	}
        }
		_DODELAY(5);			     /* delay 5 ms */

    } while( !MiscCheckTimer( &timer ));

#ifdef __KERNEL__    
	_PRINT(
#else
	DBG( DBG_HIGH,
#endif
	"Timeout - Scanner malfunction !!\n" );
	MotorToHomePosition(ps);

	/* timed out, scanner malfunction */
    return _FALSE;
}

/**
 */
static void imageP98003SetupScanStateVariables( pScanData ps, ULong index )
{
    DataType var;

    ps->Scan.dpiIdx = index;

    if(!(ps->DataInf.dwScanFlag & SCANDEF_TPA)) {

        if(((ps->IO.portMode == _PORT_BIDI) ||
            (ps->IO.portMode == _PORT_SPP)) &&
            (ps->DataInf.wPhyDataType > COLOR_TRUE24) &&
                                             (ps->DataInf.xyAppDpi.y >= 600)) {

       		ps->Shade.wExposure = nmlScan[ps->IO.portMode][index].exposureTime;
	        ps->Shade.wXStep    = nmlScan[ps->IO.portMode][index].xStepTime;
   	    } else {
		    ps->Shade.wExposure = nmlScan[_PORT_EPP][index].exposureTime;
    		ps->Shade.wXStep    = nmlScan[_PORT_EPP][index].xStepTime;
	    }

	    if( ps->Shade.bIntermediate & _ScanMode_AverageOut ) {
    	    ps->Shade.wExposure >>= 1;
	        ps->Shade.wXStep    >>= 1;
    	}
    } else {
    	if( ps->DataInf.dwScanFlag & SCANDEF_Transparency ) {
    	    ps->Shade.wExposure = posScan[index].exposureTime;
	        ps->Shade.wXStep    = posScan[index].xStepTime;
    	} else {
    	    ps->Shade.wExposure = ps->Scan.negScan[index].exposureTime;
	        ps->Shade.wXStep    = ps->Scan.negScan[index].xStepTime;
    	}
    }

    ps->Scan.dwInterlace = 0;
    ps->Scan.dwInterval  = 1;

    if( ps->DataInf.wPhyDataType == COLOR_BW )
	    var.dwValue = xferSpeed[ps->IO.portMode].thresholdBW;
	else {
        if( ps->DataInf.wPhyDataType == COLOR_256GRAY )
    		var.dwValue = xferSpeed[ps->IO.portMode].thresholdGray;
	    else
		    var.dwValue = xferSpeed[ps->IO.portMode].thresholdColor;
    }

    /* for small size/descreen */
    if((ps->DataInf.xyAppDpi.y >= 300) && var.dwValue &&
       (ps->DataInf.dwAsicBytesPerPlane <= var.dwValue)) {
    	ps->Scan.dwInterval <<= 1;
    }

    if( var.dwValue && ps->DataInf.dwAsicBytesPerPlane > var.dwValue ) {
    	if((var.dwValue << 1) > ps->DataInf.dwAsicBytesPerPlane)
	        ps->Scan.dwInterval <<= 1;			
    	else
        	if((var.dwValue << 2) > ps->DataInf.dwAsicBytesPerPlane)
		        ps->Scan.dwInterval <<= 2;
    	    else
	        	ps->Scan.dwInterval <<= 3;
    }

    /* 48 bit/600 dpi/Bpp mode will scan failed */
    if(((ps->IO.portMode == _PORT_BIDI) ||
        (ps->IO.portMode == _PORT_SPP)) &&
        (ps->DataInf.wPhyDataType > COLOR_TRUE24) &&
                                             (ps->DataInf.xyAppDpi.y >= 600)) {
    	ps->Scan.dwInterval <<= 1;
    }

    if( ps->DataInf.wPhyDataType >= COLOR_TRUE24 ) {

    	if( ps->DataInf.xyPhyDpi.y > 75U ) { 
    	    if( ps->Device.f0_8_16 ) {
	        	ps->Scan.gd_gk.wGreenDiscard = ps->DataInf.xyPhyDpi.y / 75U;
    	    } else {
	        	ps->Scan.gd_gk.wGreenDiscard = ps->DataInf.xyPhyDpi.y / 150U;
	        }
    	} else {
           	ps->Scan.gd_gk.wGreenDiscard = 1;
		}

       	ps->Scan.bd_rk.wBlueDiscard = ps->Scan.gd_gk.wGreenDiscard << 1;
    } else {
       	ps->Scan.bd_rk.wBlueDiscard = ps->Scan.gd_gk.wGreenDiscard = 0;
    }
}

/** PrepareScanningVariables() !!!
 */
static int imageP98003SetupScanSettings( pScanData ps, pScanInfo pInf )
{
	DBG( DBG_LOW, "imageP98003SetupScanSettings()\n" );

    /* call the one for ASIC 98001 first */
    imageP98SetupScanSettings( ps, pInf );

    if( !(ps->DataInf.dwScanFlag & SCANDEF_TPA )) {

        ps->Scan.dwScanOrigin = ps->Device.lUpNormal * 4 + _RFT_SCANNING_ORG;

    } else if( ps->DataInf.dwScanFlag & SCANDEF_Transparency) {

	    ps->Scan.dwScanOrigin = ps->Device.lUpPositive * 4 +
            							   _POS_SCANNING_ORG;
	} else {
	    ps->Scan.dwScanOrigin = ps->Device.lUpNegative * 4 +
            							   _NEG_SCANNING_ORG;
    }
    ps->Scan.dwScanOrigin += ps->Device.dwModelOriginY;

    /* ------- Setup CCD Offset variables ------- */
    if( ps->DataInf.xyAppDpi.y <= 75 ) {

    	if( ps->DataInf.dwVxdFlag & _VF_PREVIEW ) {

            ps->Scan.bDiscardAll   = 0;
    	    ps->DataInf.xyPhyDpi.y = 150;
            ps->Shade.bIntermediate |= _ScanMode_AverageOut;
    	    imageP98003SetupScanStateVariables( ps, 1 );
	        ps->Scan.gd_gk.wGreenDiscard = 0;

	        if( ps->DataInf.xyAppDpi.y >= 38 )
        		ps->Scan.bd_rk.wBlueDiscard = 1;
	        else
        		ps->Scan.bd_rk.wBlueDiscard = 0;

	        if( ps->DataInf.wPhyDataType >= COLOR_256GRAY ) {
        		ps->Shade.wXStep    = 6;
		        ps->Shade.wExposure = 8 * ps->Shade.wXStep;
    	    }
	    } else {
    	    if(!(ps->DataInf.dwScanFlag & SCANDEF_TPA) &&
                (ps->DataInf.xyAppDpi.y <= 50) &&
				(ps->DataInf.wPhyDataType >= COLOR_TRUE24)) {
        		ps->Shade.bIntermediate |= _ScanMode_AverageOut;
            }

    	    if((ps->DataInf.wPhyDataType<COLOR_TRUE24) || ps->Device.f0_8_16 ||
           			         (ps->Shade.bIntermediate & _ScanMode_AverageOut)) {

        		ps->Scan.bDiscardAll   = 1;
        		ps->DataInf.xyPhyDpi.y = 75;
		        imageP98003SetupScanStateVariables( ps, 0 );
    	    } else {
        		ps->Scan.bDiscardAll   = 2;
        		ps->DataInf.xyPhyDpi.y = 150;
		        imageP98003SetupScanStateVariables( ps, 1 );
    	    }
    	}
    } else {
    	if( ps->DataInf.xyAppDpi.y <= 150 ) {

    	    ps->Scan.bDiscardAll   = 2;
	        ps->DataInf.xyPhyDpi.y = 150;
	        imageP98003SetupScanStateVariables( ps, 1 );

    	} else if( ps->DataInf.xyAppDpi.y <= 300 ) {

    	    ps->Scan.bDiscardAll   = 4;
	        ps->DataInf.xyPhyDpi.y = 300;
	        imageP98003SetupScanStateVariables( ps, 2 );

	    } else if( ps->DataInf.xyAppDpi.y <= 600 ) {

    	    ps->Scan.bDiscardAll   = 8;
	        ps->DataInf.xyPhyDpi.y = 600;
            imageP98003SetupScanStateVariables( ps, 3 );

		} else {

    	    ps->Scan.bDiscardAll   = 16;
	        ps->DataInf.xyPhyDpi.y = 1200;
	        imageP98003SetupScanStateVariables( ps, 4 );
		}
    }

    /* ------- Lines have to sample or not? ------- */
    if( ps->DataInf.xyAppDpi.y == ps->DataInf.xyPhyDpi.y ) {
		DBG( DBG_LOW, "Sample every line\n" );
    	ps->Scan.DoSample = fnEveryLines;
    } else {
    	if( ps->DataInf.dwVxdFlag & _VF_PREVIEW ) {

			DBG( DBG_LOW, "Sample preview\n" );
            ps->Scan.DoSample = fnSamplePreview;
	        ps->DataInf.wYSum = 150;

	        if( ps->DataInf.xyAppDpi.y >= 38 )
        		wPreviewScanned = ps->DataInf.xyAppDpi.y * 2;
    	    else if( ps->DataInf.xyAppDpi.y >= 19 )
		            wPreviewScanned = ps->DataInf.xyAppDpi.y * 4;
        		else
            		wPreviewScanned = ps->DataInf.xyAppDpi.y * 8;
    	} else {

			DBG( DBG_LOW, "Sample lines (%u - %u)...\n",
						  ps->DataInf.xyPhyDpi.y, ps->DataInf.xyAppDpi.y );
            ps->Scan.DoSample = fnSampleLines;
	        ps->DataInf.wYSum = ps->DataInf.xyPhyDpi.y - ps->DataInf.xyAppDpi.y;
    	}
    }

    /*
     * now assign the buffer pointers for image aquisition
     */
	ps->Scan.p48BitBuf.pb = NULL;

    if( ps->DataInf.wPhyDataType >= COLOR_TRUE24 ) {

        ULong r,g,b;

        r = (ULong)_SIZE_REDFIFO /
			ps->DataInf.dwAsicBytesPerPlane - ps->Scan.bd_rk.wRedKeep;
        g = (ULong)_SIZE_GREENFIFO /
			ps->DataInf.dwAsicBytesPerPlane - ps->Scan.gd_gk.wGreenKeep;

    	if((int)r < 16 || (int)g < 16) {

    	    b = (ULong)(ps->Scan.bd_rk.wRedKeep +
						ps->Scan.gd_gk.wGreenKeep + 2U) *
			            ps->DataInf.dwAsicBytesPerPlane;

			DBG( DBG_LOW, "48Bit buffer request: len=%u bytes, available=%u\n",
						  b, ps->TotalBufferRequire );

			if( b > ps->TotalBufferRequire )
				return _E_NORESOURCE;

			ps->Scan.p48BitBuf.pb = ps->Bufs.b1.pReadBuf;
		}
    }	

    if( ps->Scan.p48BitBuf.pb ){
    	ps->Scan.DataRead          = fnReadToDriver;
	    ps->Scan.BufGet.red.bp     =
    	ps->Scan.BufPut.red.bp     =
	    ps->Scan.BufBegin.red.bp   = ps->Scan.p48BitBuf.pb;
    	ps->Scan.BufEnd.red.bp     =
	    ps->Scan.BufBegin.green.bp =
    	ps->Scan.BufGet.green.bp   =
	    ps->Scan.BufPut.green.bp   = ps->Scan.p48BitBuf.pb +
	                			     ps->DataInf.dwAsicBytesPerLine *
                                     (ps->Scan.bd_rk.wRedKeep + 1U);

    	ps->Scan.BufEnd.green.bp = ps->Scan.BufBegin.green.bp +
	                			   ps->DataInf.dwAsicBytesPerLine *
                				   (ps->Scan.gd_gk.wGreenKeep + 1U);
        ps->Scan.BufPut.blue.bp =
    	ps->Scan.BufGet.blue.bp = ps->Bufs.b1.pReadBuf +
                                  ps->DataInf.dwAsicBytesPerLine * 2;
    } else {
    	ps->Scan.DataRead         = fnReadOutScanner;
    	ps->Scan.BufPut.red.bp    = ps->Bufs.b1.pReadBuf;
    	ps->Scan.BufData.green.bp =
	    ps->Scan.BufPut.green.bp  = ps->Scan.BufPut.red.bp +
                			        ps->DataInf.dwAsicBytesPerLine;
    	ps->Scan.BufPut.blue.bp   = ps->Scan.BufPut.green.bp +
                			        ps->DataInf.dwAsicBytesPerLine;

        if( ps->DataInf.dwScanFlag & SCANDEF_ColorBGROrder ) {
    	    ps->Scan.BufData.red.bp  = ps->Scan.BufPut.blue.bp;
	        ps->Scan.BufData.blue.bp = ps->Scan.BufPut.red.bp;
    	} else {
    	    ps->Scan.BufData.red.bp  = ps->Scan.BufPut.red.bp;
	        ps->Scan.BufData.blue.bp = ps->Scan.BufPut.blue.bp;
    	}
    }

/* CHECK: maybe remove this stuff */
	if( ps->DataInf.dwScanFlag & SCANDEF_Transparency) {
	    posScan[1].exposureTime = 96;
	    posScan[1].xStepTime    = 12;
	    posScan[2].exposureTime = 96;
	    posScan[2].xStepTime    = 24;
	    posScan[3].exposureTime = 96;
	    posScan[3].xStepTime    = 48;
	    posScan[4].exposureTime = 96;
	    posScan[4].xStepTime    = 96;

	    /* Reset shading Exposure Time & xStep Time */
	    ps->Shade.wExposure = posScan[ps->Scan.dpiIdx].exposureTime;
	    ps->Shade.wXStep    = posScan[ps->Scan.dpiIdx].xStepTime;
	}
	else if( ps->DataInf.dwScanFlag & SCANDEF_Negative) {
        ps->Scan.negScan[1].exposureTime = 96;
        ps->Scan.negScan[1].xStepTime    = 12;
        ps->Scan.negScan[2].exposureTime = 96;
        ps->Scan.negScan[2].xStepTime    = 24;
        ps->Scan.negScan[3].exposureTime = 96;
        ps->Scan.negScan[3].xStepTime    = 48;
        ps->Scan.negScan[4].exposureTime = 96;
        ps->Scan.negScan[4].xStepTime    = 96;

	    /* Reset shading Exposure Time & xStep Time */
    	ps->Shade.wExposure = ps->Scan.negScan[ps->Scan.dpiIdx].exposureTime;
		ps->Shade.wXStep    = ps->Scan.negScan[ps->Scan.dpiIdx].xStepTime;
    }

	return _OK;
}

/************************ exported functions *********************************/

/**
 */
_LOC int ImageInitialize( pScanData ps )
{
	DBG( DBG_HIGH, "ImageInitialize()\n" );

	if( NULL == ps )
		return _E_NULLPTR;

    ps->Scan.dpiIdx  = 0;
    ps->Scan.negScan = negScan;

	/*
	 * depending on the asic, we set some functions
	 */
	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

		ps->GetImageInfo 	  = imageP98GetInfo;
		ps->SetupScanSettings = imageP98SetupScanSettings;
		ps->ReadOneImageLine  = imageP98001ReadOneImageLine;

    } else if( _ASIC_IS_98003 == ps->sCaps.AsicID ) {

		ps->GetImageInfo 	  = imageP98GetInfo;
		ps->SetupScanSettings = imageP98003SetupScanSettings;
		ps->ReadOneImageLine  = imageP98003ReadOneImageLine;

	} else if( _IS_ASIC96(ps->sCaps.AsicID)) {

		ps->GetImageInfo 	  = imageP96GetInfo;
		ps->SetupScanSettings = imageP96SetupScanSettings;
		ps->ReadOneImageLine  = imageP96ReadOneImageLine;

	} else {

		DBG( DBG_HIGH , "NOT SUPPORTED ASIC !!!\n" );
		return _E_NOSUPP;
	}
	return _OK;
}

/* END PLUSTEK-PP_IMAGE.C ...................................................*/
