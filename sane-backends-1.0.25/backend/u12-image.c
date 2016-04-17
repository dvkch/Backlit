/* @file u12_image.c
 * @brief functions to convert scanner data into image data
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (c) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - fixed fnColor42() to return 16bit values instead of
 *          only 12bit (this is the maximum the scanner can)
 *        - added scaling function u12image_ScaleX()
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

/*************************** local vars **************************************/

static u_short wPreviewScanned = 0;

static ExpXStepDef negScan[5] = {
	{128, 8}, {96, 12}, {96, 24}, {96, 48}, {96, 96}
};

static ExpXStepDef posScan[5] = {
	{128, 8}, {96, 12}, {96, 24}, {96, 48}, {96, 96}
};

static ExpXStepDef nmlScan[5] = {
	{160, 10}, {96, 12}, {96, 24}, {96, 48}, {96, 96},
};

/*************************** local functions *********************************/

/**
 */
static SANE_Bool fnReadToDriver( U12_Device *dev )
{
	dev->regs.RD_ModeControl = _ModeFifoBSel;
	u12io_ReadMonoData( dev, dev->scan.BufPut.blue.bp,
	                    dev->DataInf.dwAsicBytesPerPlane );

	dev->regs.RD_ModeControl = _ModeFifoGSel;
	u12io_ReadMonoData( dev, dev->scan.BufPut.green.bp,
	                    dev->DataInf.dwAsicBytesPerPlane );

	if( dev->scan.gd_gk.wGreenKeep )
		dev->scan.gd_gk.wGreenKeep--;
	else {
		dev->scan.BufPut.green.bp += dev->DataInf.dwAsicBytesPerPlane;

		if( dev->scan.BufPut.green.bp >= dev->scan.BufEnd.green.bp )
		    dev->scan.BufPut.green.bp = dev->scan.BufBegin.green.bp;
	}

	dev->regs.RD_ModeControl = _ModeFifoRSel;
	u12io_ReadMonoData( dev, dev->scan.BufPut.red.bp,
	                    dev->DataInf.dwAsicBytesPerPlane );

	dev->scan.BufPut.red.bp += dev->DataInf.dwAsicBytesPerPlane;
	if( dev->scan.BufPut.red.bp >= dev->scan.BufEnd.red.bp )
		dev->scan.BufPut.red.bp = dev->scan.BufBegin.red.bp;

	if( dev->scan.bd_rk.wRedKeep ) {
		dev->scan.bd_rk.wRedKeep--;
		return SANE_FALSE;

	} else {

		dev->scan.BufData.green.bp = dev->scan.BufGet.green.bp;
		dev->scan.BufData.red.bp   = dev->scan.BufGet.red.bp;
		dev->scan.BufData.blue.bp  = dev->scan.BufGet.blue.bp;

		dev->scan.BufGet.red.bp   += dev->DataInf.dwAsicBytesPerPlane;
		dev->scan.BufGet.green.bp += dev->DataInf.dwAsicBytesPerPlane;

		if( dev->scan.BufGet.red.bp >= dev->scan.BufEnd.red.bp )
			dev->scan.BufGet.red.bp = dev->scan.BufBegin.red.bp;

		if( dev->scan.BufGet.green.bp >= dev->scan.BufEnd.green.bp )
			dev->scan.BufGet.green.bp = dev->scan.BufBegin.green.bp;

		return SANE_TRUE;
	}
}

/**
 */
static SANE_Bool fnReadOutScanner( U12_Device *dev )
{
	if( dev->scan.bd_rk.wBlueDiscard ) {

		dev->scan.bd_rk.wBlueDiscard--;
		dev->regs.RD_ModeControl = _ModeFifoBSel;

		u12io_ReadMonoData( dev, dev->bufs.b1.pReadBuf,
		                dev->DataInf.dwAsicBytesPerPlane );

		if( dev->scan.gd_gk.wGreenDiscard ) {
			dev->scan.gd_gk.wGreenDiscard--;

			dev->regs.RD_ModeControl = _ModeFifoGSel;
			u12io_ReadMonoData( dev, dev->bufs.b1.pReadBuf,
			                    dev->DataInf.dwAsicBytesPerPlane );
		}
		return SANE_FALSE;

	} else {
		u12io_ReadColorData( dev, dev->bufs.b1.pReadBuf,
		                     dev->DataInf.dwAsicBytesPerPlane );
		return SANE_TRUE;
	}
}

/** some sampling functions
 */
static SANE_Bool fnEveryLine( U12_Device *dev )
{
	_VAR_NOT_USED( dev );
	return SANE_TRUE;
}

static SANE_Bool fnSampleLines( U12_Device *dev )
{
	dev->DataInf.wYSum += dev->DataInf.xyAppDpi.y;

	if( dev->DataInf.wYSum >= dev->DataInf.xyPhyDpi.y ) {
		dev->DataInf.wYSum -= dev->DataInf.xyPhyDpi.y;
		return	SANE_TRUE;
	}
	return SANE_FALSE;
}

static SANE_Bool fnSamplePreview( U12_Device *dev )
{
	dev->DataInf.wYSum += wPreviewScanned;
	if( dev->DataInf.wYSum >= 150 ) {

		dev->DataInf.wYSum -= 150;
		return SANE_TRUE;
	}

	return SANE_FALSE;
}

/** this function is used when
 * - the data type is B/W or GrayScale.
 * - the required horizontal resolution doesn't exceed the optic spec.
 * - the required vertical resolution exceeds the optic spec.
 */
static void fnDataDirect( U12_Device *dev, void *src, void *dest, u_long len )
{
	_VAR_NOT_USED( dev );
	memcpy( dest, src, len );
}

/** merges the color planes to pixels style without enlarge operation.
 */
static void fnColorDirect( U12_Device *dev, void *pb, void *img, u_long len )
{
	SANE_Byte  *src;
	RGBByteDef *dest;

	src  = (SANE_Byte*)img;
	dest = (RGBByteDef*)pb;

	for ( len = dev->DataInf.dwAsicPixelsPerPlane; len; len--, src++, dest++) {

		dest->Red   = *src;
		dest->Green = src[dev->DataInf.dwAsicPixelsPerPlane];
		dest->Blue  = src[dev->DataInf.dwAsicPixelsPerPlane*2];
	}
}

/** merges the color planes to pixels style without enlarge operation.
 *  The scanner returns the pixel data in Motorola-Format, so we have to swap
 *  (at least on x86)
 */
static void fnColor42( U12_Device *dev, void *pb, void *img, u_long len )
{
	u_short      *src;
	RGBUShortDef *dest;
    
	register u_long i;

	_VAR_NOT_USED( len );
	src  = (u_short*)img;
	dest = (RGBUShortDef*)pb;

	for ( i = dev->DataInf.dwAsicPixelsPerPlane; i; i--, src++, dest++) {

		dest->Red   = (*src) << 4;
		dest->Green = (src[dev->DataInf.dwAsicPixelsPerPlane]) << 4;
		dest->Blue  = (src[dev->DataInf.dwAsicPixelsPerPlane * 2]) << 4;
	}
}

/**
 */
static void u12image_SetupScanStateVariables( U12_Device *dev, u_long index )
{
	DataType var;

	DBG( _DBG_INFO, "u12image_SetupScanStateVariables(%lu)\n", index );
	dev->scan.dpiIdx = index;

	if(!(dev->DataInf.dwScanFlag & _SCANDEF_TPA)) {

		dev->shade.wExposure = nmlScan[index].exposureTime;
		dev->shade.wXStep     = nmlScan[index].xStepTime;

		if( dev->shade.intermediate & _ScanMode_AverageOut ) {
			dev->shade.wExposure >>= 1;
			dev->shade.wXStep    >>= 1;
		}
	} else {
		if( dev->DataInf.dwScanFlag & _SCANDEF_Transparency ) {
			dev->shade.wExposure = posScan[index].exposureTime;
			dev->shade.wXStep    = posScan[index].xStepTime;
		} else {
			dev->shade.wExposure = dev->scan.negScan[index].exposureTime;
			dev->shade.wXStep    = dev->scan.negScan[index].xStepTime;
		}
	}
	dev->scan.dwInterval = 1;

	if( dev->DataInf.wPhyDataType == COLOR_BW )
		var.dwValue = 0;
	else {
		if( dev->DataInf.wPhyDataType == COLOR_256GRAY )
    		var.dwValue = 2500;
		else
			var.dwValue = 3200;
	}

	/* for small size/descreen */
	if((dev->DataInf.xyAppDpi.y >= 300) && var.dwValue &&
	   (dev->DataInf.dwAsicBytesPerPlane <= var.dwValue)) {
		dev->scan.dwInterval <<= 1;
	}

	if( var.dwValue && dev->DataInf.dwAsicBytesPerPlane > var.dwValue ) {
		if((var.dwValue << 1) > dev->DataInf.dwAsicBytesPerPlane)
			dev->scan.dwInterval <<= 1;
		else
			if((var.dwValue << 2) > dev->DataInf.dwAsicBytesPerPlane)
				dev->scan.dwInterval <<= 2;
			else
				dev->scan.dwInterval <<= 3;
	}

	if( dev->DataInf.wPhyDataType >= COLOR_TRUE24 ) {

		if( dev->DataInf.xyPhyDpi.y > 75U ) {
			if( dev->f0_8_16 ) {
				dev->scan.gd_gk.wGreenDiscard = dev->DataInf.xyPhyDpi.y / 75U;
			} else {
				dev->scan.gd_gk.wGreenDiscard = dev->DataInf.xyPhyDpi.y / 150U;
			}
		} else {
			dev->scan.gd_gk.wGreenDiscard = 1;
		}

		dev->scan.bd_rk.wBlueDiscard = dev->scan.gd_gk.wGreenDiscard << 1;
	} else {
		dev->scan.bd_rk.wBlueDiscard = dev->scan.gd_gk.wGreenDiscard = 0;
	}
}

/** limit the resolution
 */
static u_short
u12image_GetPhysDPI( U12_Device *dev, ImgDef *img, SANE_Bool fDpiX )
{
	if( fDpiX ) {

		if( img->xyDpi.x > dev->dpi_max_x )
			return dev->dpi_max_x;
		else
			return img->xyDpi.x;

	} else {
		
		if( img->xyDpi.y > dev->dpi_max_y )
				return dev->dpi_max_y;
		else
			return img->xyDpi.y;
	}
}

/** calculate the image properties according to the scanmode
 *  set all internally needed information
 */
static void u12image_GetImageInfo( U12_Device *dev, ImgDef *image )
{
	DBG( _DBG_INFO, "u12image_GetImageInfo()\n" );

	dev->DataInf.xyPhyDpi.x = u12image_GetPhysDPI(dev, image, SANE_TRUE );
	dev->DataInf.xyPhyDpi.y = u12image_GetPhysDPI(dev, image, SANE_FALSE);

	DBG( _DBG_INFO, "* xyPhyDpi.x = %u, xyPhyDpi.y = %u\n",
	                dev->DataInf.xyPhyDpi.x, dev->DataInf.xyPhyDpi.y );

	DBG( _DBG_INFO, "* crArea.x = %u, crArea.y = %u\n",
	                image->crArea.x, image->crArea.y );

	DBG( _DBG_INFO, "* crArea.cx = %u, crArea.cy = %u\n",
	                image->crArea.cx, image->crArea.cy );

	dev->DataInf.xyRatio = (double)dev->DataInf.xyPhyDpi.y/
	                       (double)dev->DataInf.xyPhyDpi.x;

	dev->DataInf.dwAppLinesPerArea = (u_long)image->crArea.cy *
									  image->xyDpi.y / _MEASURE_BASE;

    dev->DataInf.dwAppPixelsPerLine = (u_long)image->crArea.cx *
									   image->xyDpi.x / _MEASURE_BASE;

	dev->DataInf.dwPhysBytesPerLine = (u_long)image->crArea.cx *
									   dev->DataInf.xyPhyDpi.x / _MEASURE_BASE;

    if( image->wDataType <= COLOR_BW ) {
		dev->DataInf.dwAsicPixelsPerPlane =
		                  (dev->DataInf.dwAppPixelsPerLine+7UL) & 0xfffffff8UL;
		dev->DataInf.dwAppPhyBytesPerLine =
		dev->DataInf.dwAppBytesPerLine 	  =
		dev->DataInf.dwAsicBytesPerLine   =
		dev->DataInf.dwAsicBytesPerPlane  = dev->DataInf.dwAsicPixelsPerPlane>>3;
    } else {
		dev->DataInf.dwAsicBytesPerPlane  =
		dev->DataInf.dwAsicPixelsPerPlane = dev->DataInf.dwAppPixelsPerLine;
	}

	if( COLOR_TRUE42 == image->wDataType ) {
		dev->DataInf.dwAsicBytesPerPlane *= 2;
	}

	switch( image->wDataType ) {

	case COLOR_BW:                            
		dev->scan.DataProcess     = fnDataDirect;
		dev->DataInf.wPhyDataType = COLOR_BW;
		dev->shade.intermediate   = _ScanMode_Mono;
		break;

	case COLOR_256GRAY:
		dev->scan.DataProcess     = fnDataDirect;
		dev->DataInf.dwAsicBytesPerLine =
		dev->DataInf.dwAppPhyBytesPerLine = dev->DataInf.dwAppPixelsPerLine;
		dev->DataInf.wPhyDataType = COLOR_256GRAY;
		dev->shade.intermediate   = _ScanMode_Mono;
		break;

	case COLOR_TRUE24:
		dev->scan.DataProcess = fnColorDirect;
		dev->DataInf.dwAsicBytesPerLine =
		dev->DataInf.dwAppPhyBytesPerLine = dev->DataInf.dwAppPixelsPerLine * 3;
		dev->DataInf.wPhyDataType = COLOR_TRUE24;
		dev->shade.intermediate  = _ScanMode_Color;
		break;

	case COLOR_TRUE42:
		dev->scan.DataProcess = fnColor42;
		dev->DataInf.dwAsicBytesPerLine =
		dev->DataInf.dwAppPhyBytesPerLine = dev->DataInf.dwAppPixelsPerLine * 6;
		dev->DataInf.wPhyDataType = COLOR_TRUE42;
		dev->shade.intermediate  = _ScanMode_Color;
		break;
	}

	/* raus mit einem von beiden!!!!*/
	dev->DataInf.dwAppBytesPerLine = dev->DataInf.dwAppPhyBytesPerLine;
	
	DBG( _DBG_INFO, "AppLinesPerArea    = %lu\n", dev->DataInf.dwAppLinesPerArea    );
	DBG( _DBG_INFO, "AppPixelsPerLine   = %lu\n", dev->DataInf.dwAppPixelsPerLine   );
	DBG( _DBG_INFO, "AppPhyBytesPerLine = %lu\n", dev->DataInf.dwAppPhyBytesPerLine );
	DBG( _DBG_INFO, "AppBytesPerLine    = %lu\n", dev->DataInf.dwAppBytesPerLine    );
	DBG( _DBG_INFO, "AsicPixelsPerPlane = %lu\n", dev->DataInf.dwAsicPixelsPerPlane );
	DBG( _DBG_INFO, "AsicBytesPerPlane  = %lu\n", dev->DataInf.dwAsicBytesPerPlane  );
	DBG( _DBG_INFO, "AsicBytesPerLine   = %lu\n", dev->DataInf.dwAsicBytesPerLine   );
	DBG( _DBG_INFO, "Physical Bytes     = %lu\n", dev->DataInf.dwPhysBytesPerLine   );
}

/** 
 */
static int imageSetupScanSettings( U12_Device *dev, ImgDef *img )
{
	u_short brightness;

	DBG( _DBG_INFO, "imageSetupScanSettings()\n" );

	dev->DataInf.dwScanFlag = img->dwFlag;
	dev->DataInf.crImage    = img->crArea;

	DBG( _DBG_INFO,"* DataInf.dwScanFlag = 0x%08lx\n",dev->DataInf.dwScanFlag);

	dev->DataInf.crImage.x <<= 1;

	dev->DataInf.xyAppDpi     = img->xyDpi;
	dev->DataInf.wAppDataType = img->wDataType;

	u12image_GetImageInfo( dev, img );

	dev->scan.lBufferAdjust = (long)dev->DataInf.dwAppBytesPerLine;

	DBG( _DBG_INFO, "* Scan settings:\n" );
	DBG( _DBG_INFO, "* ImageInfo: (x=%u,y=%u,dx=%u,dy=%u)\n",
			 dev->DataInf.crImage.x,  dev->DataInf.crImage.y,
			 dev->DataInf.crImage.cx, dev->DataInf.crImage.cy );

	/*
	 * 0                   _DEF_BW_THRESHOLD                     255
	 * +-------------------------+--------------------------------+
	 * |<------- Black --------->|<----------- White ------------>|
	 *  So, if user wish to make image darker, the threshold value should be
	 *  higher than _defBwThreshold, otherwise it should lower than the
	 *  _DefBwThreshold.
	 *  Darker = _DEF_BW_THRESHOLD + White * Input / 127;
	 *             Input < 0, and White = 255 - _DEF_BW_THRESHOLD, so
	 *           = _DEF_BW_THRESHOLD - (255 - _DEF_BW_THRESHOLD) * Input / 127;
	 *  The brighter is the same idea.
	 */

/* CHECK: We have now two methods for setting the brightness...
*/
	DBG( _DBG_INFO, "* brightness = %i\n", dev->DataInf.siBrightness );
	if ( dev->DataInf.siBrightness < 0) {
		brightness = (u_short)(_DEF_BW_THRESHOLD -
			       (255 - _DEF_BW_THRESHOLD) * dev->DataInf.siBrightness /127);
	} else {
		brightness = (u_short)(_DEF_BW_THRESHOLD -
					       _DEF_BW_THRESHOLD * dev->DataInf.siBrightness /127);
	}

	dev->regs.RD_ThresholdControl = brightness;
	DBG( _DBG_INFO, "* RD_ThresholdControl = %i\n", brightness );
	return 0;
}

/** PrepareScanningVariables() !!!
 */
static SANE_Status u12image_SetupScanSettings( U12_Device *dev, ImgDef *img )
{
	DBG( _DBG_INFO, "u12image_SetupScanSettings()\n" );

	wPreviewScanned   = 0;
	dev->scan.dpiIdx  = 0;
	dev->scan.negScan = negScan;
 
	imageSetupScanSettings( dev, img );

	if( !(dev->DataInf.dwScanFlag & _SCANDEF_TPA )) {

		dev->scan.dwScanOrigin = dev->adj.upNormal * 4 + _RFT_SCANNING_ORG;

	} else if( dev->DataInf.dwScanFlag & _SCANDEF_Transparency) {

		dev->scan.dwScanOrigin = dev->adj.upPositive * 4 + _POS_SCANNING_ORG;
	} else {
		dev->scan.dwScanOrigin = dev->adj.upNegative * 4 + _NEG_SCANNING_ORG;
	}
	dev->scan.dwScanOrigin += 64 /*dev->dwModelOriginY*/;

	if( dev->DataInf.xyAppDpi.y <= 75 ) {

		if( dev->DataInf.dwScanFlag & _SCANDEF_PREVIEW ) {

			dev->scan.bDiscardAll   = 0;
			dev->DataInf.xyPhyDpi.y = 150;
			dev->shade.intermediate |= _ScanMode_AverageOut;
			u12image_SetupScanStateVariables( dev, 1 );
	        dev->scan.gd_gk.wGreenDiscard = 0;

			if( dev->DataInf.xyAppDpi.y >= 38 )
				dev->scan.bd_rk.wBlueDiscard = 1;
			else
				dev->scan.bd_rk.wBlueDiscard = 0;

			if( dev->DataInf.wPhyDataType >= COLOR_256GRAY ) {
				dev->shade.wXStep    = 6;
				dev->shade.wExposure = 8 * dev->shade.wXStep;
			}
		} else {
			if(!(dev->DataInf.dwScanFlag & _SCANDEF_TPA) &&
			    (dev->DataInf.xyAppDpi.y <= 50) &&
			    (dev->DataInf.wPhyDataType >= COLOR_TRUE24)) {
				dev->shade.intermediate |= _ScanMode_AverageOut;
			}

			if((dev->DataInf.wPhyDataType<COLOR_TRUE24) || dev->f0_8_16 ||
			         (dev->shade.intermediate & _ScanMode_AverageOut)) {

				dev->scan.bDiscardAll   = 1;
				dev->DataInf.xyPhyDpi.y = 75;
				u12image_SetupScanStateVariables( dev, 0 );
    		} else {
				dev->scan.bDiscardAll   = 2;
				dev->DataInf.xyPhyDpi.y = 150;
				u12image_SetupScanStateVariables( dev, 1 );
			}
		}
	} else {
		if( dev->DataInf.xyAppDpi.y <= 150 ) {

			dev->scan.bDiscardAll   = 2;
			dev->DataInf.xyPhyDpi.y = 150;
			u12image_SetupScanStateVariables( dev, 1 );

		} else if( dev->DataInf.xyAppDpi.y <= 300 ) {

			dev->scan.bDiscardAll   = 4;
			dev->DataInf.xyPhyDpi.y = 300;
			u12image_SetupScanStateVariables( dev, 2 );

		} else if( dev->DataInf.xyAppDpi.y <= 600 ) {

			dev->scan.bDiscardAll   = 8;
			dev->DataInf.xyPhyDpi.y = 600;
			u12image_SetupScanStateVariables( dev, 3 );

		} else {

			dev->scan.bDiscardAll   = 16;
			dev->DataInf.xyPhyDpi.y = 1200;
			u12image_SetupScanStateVariables( dev, 4 );
		}
	}

	/* ------- lines to sample or not? ------- */
	if( dev->DataInf.xyAppDpi.y == dev->DataInf.xyPhyDpi.y ) {
		DBG( _DBG_INFO, "* Sample every line\n" );
		dev->scan.DoSample = fnEveryLine;
	} else {
		if( dev->DataInf.dwScanFlag & _SCANDEF_PREVIEW ) {

			DBG( _DBG_INFO, "* Sample preview\n" );
			dev->scan.DoSample = fnSamplePreview;
			dev->DataInf.wYSum = 150;

			if( dev->DataInf.xyAppDpi.y >= 38 )
				wPreviewScanned = dev->DataInf.xyAppDpi.y * 2;
			else if( dev->DataInf.xyAppDpi.y >= 19 )
				wPreviewScanned = dev->DataInf.xyAppDpi.y * 4;
			else
				wPreviewScanned = dev->DataInf.xyAppDpi.y * 8;
		} else {

			DBG( _DBG_INFO, "* Sample lines (%u - %u)...\n",
						  dev->DataInf.xyPhyDpi.y, dev->DataInf.xyAppDpi.y );
			dev->scan.DoSample = fnSampleLines;
			dev->DataInf.wYSum = dev->DataInf.xyPhyDpi.y - dev->DataInf.xyAppDpi.y;
		}
	}

	/* now assign the buffer pointers for image aquisition
	 */
	dev->scan.p48BitBuf.pb = NULL;

	if( dev->DataInf.wPhyDataType >= COLOR_TRUE24 ) {

		u_long r, g, b;

		r = (u_long)_SIZE_REDFIFO /
			dev->DataInf.dwAsicBytesPerPlane - dev->scan.bd_rk.wRedKeep;
		g = (u_long)_SIZE_GREENFIFO /
			dev->DataInf.dwAsicBytesPerPlane - dev->scan.gd_gk.wGreenKeep;

		if((int)r < 16 || (int)g < 16) {

			b = (u_long)(dev->scan.bd_rk.wRedKeep +
			             dev->scan.gd_gk.wGreenKeep + 2U) *
			             dev->DataInf.dwAsicBytesPerPlane;

			DBG( _DBG_INFO, "48Bit buffer request: "
			     "len=%lu bytes, available=%lu\n", b, _SIZE_TOTAL_BUF_TPA );

			if( b > _SIZE_TOTAL_BUF_TPA ) {
				DBG( _DBG_ERROR, "Not that much FIFO memory available!\n" );
				return SANE_STATUS_NO_MEM;
			}

			dev->scan.p48BitBuf.pb = dev->bufs.b1.pReadBuf;
		}
	}

	if( dev->scan.p48BitBuf.pb ){
		dev->scan.DataRead          = fnReadToDriver;
		dev->scan.BufGet.red.bp     =
		dev->scan.BufPut.red.bp     =
		dev->scan.BufBegin.red.bp   = dev->scan.p48BitBuf.pb;
		dev->scan.BufEnd.red.bp     =
		dev->scan.BufBegin.green.bp =
		dev->scan.BufGet.green.bp   =
		dev->scan.BufPut.green.bp   = dev->scan.p48BitBuf.pb +
		                              dev->DataInf.dwAsicBytesPerLine *
		                              (dev->scan.bd_rk.wRedKeep + 1U);

		dev->scan.BufEnd.green.bp = dev->scan.BufBegin.green.bp +
		                            dev->DataInf.dwAsicBytesPerLine *
		                            (dev->scan.gd_gk.wGreenKeep + 1U);
		dev->scan.BufPut.blue.bp =
		dev->scan.BufGet.blue.bp = dev->bufs.b1.pReadBuf +
		                           dev->DataInf.dwAsicBytesPerLine * 2;
	} else {
		dev->scan.DataRead         = fnReadOutScanner;
		dev->scan.BufPut.red.bp    = dev->bufs.b1.pReadBuf;
		dev->scan.BufData.green.bp =
		dev->scan.BufPut.green.bp  = dev->scan.BufPut.red.bp +
		                             dev->DataInf.dwAsicBytesPerLine;
		dev->scan.BufPut.blue.bp   = dev->scan.BufPut.green.bp +
		                             dev->DataInf.dwAsicBytesPerLine;

		dev->scan.BufData.red.bp  = dev->scan.BufPut.red.bp;
		dev->scan.BufData.blue.bp = dev->scan.BufPut.blue.bp;
	}

/* CHECK: maybe remove this stuff */
#if 0
	if( ps->DataInf.dwScanFlag & _SCANDEF_Transparency) {
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
	else if( ps->DataInf.dwScanFlag & _SCANDEF_Negative) {
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
#endif
	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Bool u12image_DataIsReady( U12_Device *dev, void* buf )
{
	DBG( _DBG_READ, "* DataIsReady()\n" );

	if( dev->scan.bDiscardAll ) {
		dev->scan.bDiscardAll--;

		if( dev->DataInf.wPhyDataType <= COLOR_256GRAY ) {
			dev->regs.RD_ModeControl = _ModeFifoGSel;
			u12io_ReadMonoData( dev, dev->bufs.b1.pReadBuf,
			                            dev->DataInf.dwAsicBytesPerPlane );
		} else {
			u12io_ReadColorData( dev, dev->bufs.b1.pReadBuf,
			                     dev->DataInf.dwAsicBytesPerPlane );
		}
		return SANE_FALSE;
	}

	if( dev->DataInf.wPhyDataType <= COLOR_256GRAY ) {

		dev->regs.RD_ModeControl = _ModeFifoGSel;
		u12io_ReadMonoData( dev, buf, dev->DataInf.dwAsicBytesPerPlane );

	} else {

		if( !dev->scan.DataRead( dev )) {
			return SANE_FALSE;
		}
	}

	if( dev->scan.DoSample( dev )) {

		/* direct is done here without copying...*/
		if( fnDataDirect != dev->scan.DataProcess ) {
			(*dev->scan.DataProcess)(dev, buf, (void*)(dev->scan.BufPut.red.bp),
                                        dev->DataInf.dwAppPhyBytesPerLine);
		}
		return SANE_TRUE;
	}
	return SANE_FALSE;
}

/**
 */
static SANE_Status u12image_ReadOneImageLine( U12_Device *dev, void* buf )
{
	SANE_Byte  b, state;
	TimerDef  timer, t2;

	DBG( _DBG_READ, "u12image_ReadOneImageLine()\n" );

	u12io_StartTimer( &timer, _LINE_TIMEOUT );
	u12io_StartTimer( &t2, _SECOND*2 );
	do {

		state = u12io_GetScanState( dev );
		dev->scan.bNowScanState = (state & _SCANSTATE_MASK);

		if( state & _SCANSTATE_STOP ) {

			DBG( _DBG_READ, "* SCANSTATE_STOP\n" );
			u12motor_ModuleForwardBackward( dev );

			if( u12io_GetFifoLength( dev ) >= dev->scan.dwMinReadFifo )
				if( u12image_DataIsReady( dev, buf ))
					return SANE_STATUS_GOOD;

		} else {

			dev->scan.bModuleState = _MotorInNormalState;
			b = dev->scan.bNowScanState - dev->scan.oldScanState;

			if((char) b < 0)
				b += _NUMBER_OF_SCANSTEPS;

			if( b >= dev->scan.bRefresh ) {

				u12io_RegisterToScanner( dev, REG_REFRESHSCANSTATE );
				dev->scan.oldScanState = u12io_GetScanState( dev );
				dev->scan.oldScanState &= _SCANSTATE_MASK;
			}

			if( u12io_GetFifoLength( dev ) >= dev->scan.dwMaxReadFifo ) {

				if( u12image_DataIsReady( dev, buf ))
					return SANE_STATUS_GOOD;
			}
			else {

				b = dev->scan.bNowScanState - dev->scan.oldScanState;

				if((char) b < 0)
					b += _NUMBER_OF_SCANSTEPS;

				if( b >= dev->scan.bRefresh ) {

					u12io_RegisterToScanner( dev, REG_REFRESHSCANSTATE );
					dev->scan.oldScanState = u12io_GetScanState( dev );
					dev->scan.oldScanState &= _SCANSTATE_MASK;
				}

				if( u12io_GetFifoLength( dev ) >= dev->scan.dwMinReadFifo ) {
					if( u12image_DataIsReady( dev, buf ))
						return SANE_STATUS_GOOD;
				}
			}
		}

	} while( !u12io_CheckTimer( &timer ));

	DBG( _DBG_ERROR, "Timeout - Scanner malfunction !!\n" );
	u12motor_ToHomePosition( dev, SANE_TRUE );

	/* timed out, scanner malfunction */
	return SANE_STATUS_IO_ERROR;
}

/**
 */
static void u12image_PrepareScaling( U12_Device *dev )
{
	int    step;
	double ratio;

	dev->scaleBuf = NULL;
	DBG( _DBG_INFO, "APP-DPIX=%u, MAX-DPIX=%u\n",
		             dev->DataInf.xyAppDpi.x, dev->dpi_max_x );

	if( dev->DataInf.xyAppDpi.x > dev->dpi_max_x ) {
		
		dev->scaleBuf = malloc( dev->DataInf.dwAppBytesPerLine );

		ratio = (double)dev->DataInf.xyAppDpi.x/(double)dev->dpi_max_x;
		dev->scaleIzoom = (int)(1.0/ratio * 1000);

		switch( dev->DataInf.wAppDataType ) {

			case COLOR_BW      : step = 0;  break;
			case COLOR_256GRAY : step = 1;  break;
			case COLOR_TRUE24  : step = 3;  break;
			case COLOR_TRUE42  : step = 6;  break;
			default			   : step = 99; break;
		}
		dev->scaleStep = step;

		DBG( _DBG_INFO, "u12image_PrepareScaling: izoom=%i, step=%u\n",
		                dev->scaleIzoom, step );
	} else {

		DBG( _DBG_INFO, "u12image_PrepareScaling: DISABLED\n" );
	} 
}

/** scaling picture data in x-direction, using a DDA algorithm
 *  (digital differential analyzer).
 */
static void u12image_ScaleX( U12_Device *dev, SANE_Byte *ib, SANE_Byte *ob )
{
	SANE_Byte tmp;
	int       ddax;
	u_long    i, j, x;

	/* when not supported, only copy the data */
	if( 99 == dev->scaleStep ) {
		memcpy( ob, ib, dev->DataInf.dwAppBytesPerLine );
		return;
	}

	/* now scale... */
	if( 0 == dev->scaleStep ) {

		/* binary scaling */
		ddax = 0;
		x    = 0;
		memset( ob, 0, dev->DataInf.dwAppBytesPerLine );

		for( i = 0; i < dev->DataInf.dwPhysBytesPerLine*8; i++ ) {

			ddax -= 1000;

			while( ddax < 0 ) {

				tmp = ib[(i>>3)];

				if((x>>3) < dev->DataInf.dwAppBytesPerLine ) {
					if( 0 != (tmp &= (1 << ((~(i & 0x7))&0x7))))
						ob[x>>3] |= (1 << ((~(x & 0x7))&0x7));
				}
				x++;
				ddax += dev->scaleIzoom;
			}
		}

	} else {

		/* color and gray scaling */
		ddax = 0;
		x    = 0;
		for( i = 0; i < dev->DataInf.dwPhysBytesPerLine*dev->scaleStep;
		                                                  i+=dev->scaleStep ) {

			ddax -= 1000;

			while( ddax < 0 ) {

				for( j = 0; j < (u_long)dev->scaleStep; j++ ) {

					if((x+j) < dev->DataInf.dwAppBytesPerLine ) {
						ob[x+j] = ib[i+j];
					}
				}
				x    += dev->scaleStep;
				ddax += dev->scaleIzoom;
			}
		}
	}
}

/* END U12_IMAGE.C ..........................................................*/
