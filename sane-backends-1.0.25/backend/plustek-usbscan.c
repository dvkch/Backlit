/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbscan.c
 *  @brief Scanning...
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - minor fixes
 * - 0.42 - added some stuff for CIS devices
 * - 0.43 - no changes
 * - 0.44 - added CIS specific settings and calculations
 *        - removed usb_IsEscPressed
 * - 0.45 - fixed the special setting stuff for CanoScan
 *        - fixed pixel calculation for binary modes
 *        - fixed cancel hang problem
 *        - fixed CIS PhyBytes adjustment
 *        - removed CanoScan specific setting stuff
 * - 0.46 - fixed problem in usb_SetScanParameters()
 * - 0.47 - no changes
 * - 0.48 - minor fixes
 * - 0.49 - a_bRegs is now part of the device structure
 *        - using now PhyDpi.y as selector for the motor MCLK range
 *        - changed usb_MapDownload prototype
 * - 0.50 - cleanup
 * - 0.51 - added usb_get_res() and usb_GetPhyPixels()
 * - 0.52 - removed stuff, that will most probably never be used
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

#define DIVIDER 8

/** array used to get motor-settings and mclk-settings
 */
static int dpi_ranges[] = { 75,100,150,200,300,400,600,800,1200,2400 };

static u_char     bMaxITA;

static SANE_Bool  m_fAutoPark;
static SANE_Bool  m_fFirst;
static double     m_dHDPIDivider;
static double     m_dMCLKDivider;
static ScanParam *m_pParam;
static u_char     m_bLineRateColor;
static u_char     m_bCM;
static u_char	  m_bIntTimeAdjust;
static u_char	  m_bOldScanData;
static u_short    m_wFastFeedStepSize;
static u_short    m_wLineLength;
static u_short	  m_wStepSize;
static u_long     m_dwPauseLimit;

static SANE_Bool  m_fStart = SANE_FALSE;

/* Prototype... */
static SANE_Bool usb_DownloadShadingData( Plustek_Device*, u_char );

/** returns the min of the two values val1 and val2
 * @param val1 - first parameter
 * @param val2 - second parameter
 * @return val1 if val1 < val2, else val1
 */
static u_long
usb_min( u_long val1, u_long val2 )
{
	if( val1 > val2 )
		return val2;
	return val1;
}

/** returns the max of the two values val1 and val2
 * @param val1 - first parameter
 * @param val2 - second parameter
 * @return val1 if val1 > val2, else val2
 */
static u_long
usb_max( u_long val1, u_long val2 )
{
	if( val1 > val2 )
		return val1;
	return val2;
}

/** function to get the possible resolution for a specific base resolution,
 * according to the dividers a LM983x offers.
 * @param base - scanners optical resolution
 * @param idx  - which divider to use
 */
static u_short
usb_get_res( u_short base, u_short idx )
{
	double div_list[DIVIDER] = { 12.0, 8.0, 6.0, 4.0, 3.0, 2.0, 1.5, 1.0 };

	if( idx == 0 || idx > DIVIDER )
		return 0;

	return (u_short)((double)base/div_list[idx-1]);
}

/** Set the horizontal DPI divider.
 * Affected registers:<br>
 * 0x09 - Horizontal DPI divider HDPI_DIV<br>
 *
 * @param dev  - pointer to our device structure,
 *               it should contain all we need
 * @param xdpi - user specified horizontal resolution
 * @return - the function returns the "normalized" horizontal resolution.
 */
static u_short
usb_SetAsicDpiX( Plustek_Device *dev, u_short xdpi )
{
	u_short   res;
	ScanDef  *scanning = &dev->scanning;
	DCapsDef *scaps    = &dev->usbDev.Caps;
	u_char   *regs     = dev->usbDev.a_bRegs;

	/* limit xdpi to lower value for certain devices...
	 */
	if( scaps->OpticDpi.x == 1200 &&
		scanning->sParam.bDataType != SCANDATATYPE_Color &&
		xdpi < 150 &&
		scanning->sParam.bDataType == SCANDATATYPE_BW ) {
		xdpi = 150;
		DBG( _DBG_INFO2, "* LIMIT XDPI to %udpi\n", xdpi );
	}

	m_dHDPIDivider = (double)scaps->OpticDpi.x / xdpi;

	if (m_dHDPIDivider < 1.5)
	{
		m_dHDPIDivider = 1.0;
		regs[0x09]  = 0;
	}
	else if (m_dHDPIDivider < 2.0)
	{
		m_dHDPIDivider = 1.5;
		regs[0x09]  = 1;
	}
	else if (m_dHDPIDivider < 3.0)
	{
		m_dHDPIDivider = 2.0;
		regs[0x09]  = 2;
	}
	else if (m_dHDPIDivider < 4.0)
	{
		m_dHDPIDivider = 3.0;
		regs[0x09]  = 3;
	}
	else if (m_dHDPIDivider < 6.0)
	{
		m_dHDPIDivider = 4.0;
		regs[0x09]  = 4;
	}
	else if (m_dHDPIDivider < 8.0)
	{
		m_dHDPIDivider = 6.0;
		regs[0x09]  = 5;
	}
	else if (m_dHDPIDivider < 12.0)
	{
		m_dHDPIDivider = 8.0;
		regs[0x09]  = 6;
	}
	else
	{
		m_dHDPIDivider = 12.0;
		regs[0x09]  = 7;
	}

	/* adjust, if any turbo/preview mode is set, should be 0 here... */
	if( regs[0x0a] )
		regs[0x09] -= ((regs[0x0a] >> 2) + 2);

	DBG( _DBG_INFO2, "* HDPI: %.3f\n", m_dHDPIDivider );
	res = (u_short)((double)scaps->OpticDpi.x / m_dHDPIDivider);

	DBG( _DBG_INFO2, "* XDPI=%u, HDPI=%.3f\n", res, m_dHDPIDivider );
	return res;
}

/**
 * @param dev  - pointer to our device structure,
 *               it should contain all we need
 * @param ydpi - user specified vertical resolution
 * @return -
 */
static u_short
usb_SetAsicDpiY( Plustek_Device *dev, u_short ydpi )
{
	ScanDef  *scanning = &dev->scanning;
	DCapsDef *sCaps    = &dev->usbDev.Caps;
	HWDef    *hw       = &dev->usbDev.HwSetting;

	u_short wMinDpi, wDpi;

	if(0 != sCaps->bSensorDistance )
		wMinDpi = sCaps->OpticDpi.y / sCaps->bSensorDistance;
	else
		wMinDpi = 75;

	/* Here we might have to check against the MinDpi value ! */
	wDpi = (ydpi + wMinDpi - 1) / wMinDpi * wMinDpi;

	/*
	 * HEINER: added '*2'
	 */
	if( wDpi > sCaps->OpticDpi.y * 2 )
		wDpi = sCaps->OpticDpi.y * 2;

	if( (hw->motorModel == MODEL_Tokyo600) ||
		!_IS_PLUSTEKMOTOR(hw->motorModel)) {
		/* return wDpi; */
	} else if( sCaps->wFlags & DEVCAPSFLAG_Adf && sCaps->OpticDpi.x == 600 ) {
		/* for ADF scanner color mode 300 dpi big noise */
		if( scanning->sParam.bDataType == SCANDATATYPE_Color &&
			scanning->sParam.bBitDepth > 8 && wDpi < 300 ) {
			wDpi = 300;
		}
	} else if( sCaps->OpticDpi.x == 1200 ) {
		if( scanning->sParam.bDataType != SCANDATATYPE_Color && wDpi < 200) {
			wDpi = 200;
		}
	}

	DBG( _DBG_INFO2, "* YDPI=%u, MinDPIY=%u\n", wDpi, wMinDpi );
	return wDpi;
}

/** set color mode and sensor configuration stuff, according to the data mode
 * Affected registers:<br>
 * 0x26 - 0x27 - Color Mode settings<br>
 * 0x0f - 0x18 - Sensor Configuration - directly from the HwDefs<br>
 * 0x09        - add Data Mode and Pixel Packing<br>
 *
 * @param dev    - pointer to our device structure,
 *                 it should contain all we need
 * @param pParam - pointer to the current scan parameters
 * @return - Nothing
 */
static void
usb_SetColorAndBits( Plustek_Device *dev, ScanParam *pParam )
{
	HWDef    *hw   = &dev->usbDev.HwSetting;
	u_char   *regs = dev->usbDev.a_bRegs;

	/* Set pixel packing, data mode and AFE operation */
	switch( pParam->bDataType ) {
		case SCANDATATYPE_Color:
			m_bCM = 3;
			regs[0x26] = hw->bReg_0x26 & 0x7;

			/* if set to one channel color, we select the blue channel
			 * as input source, this is the default, but I don't know
			 * what happens, if we deselect this
			 */
			if( regs[0x26] & _ONE_CH_COLOR )
				regs[0x26] |= (_BLUE_CH | 0x01);

			memcpy( &regs[0x0f], hw->bReg_0x0f_Color, 10 );
			break;

		case SCANDATATYPE_Gray:
			m_bCM = 1;
			regs[0x26] = (hw->bReg_0x26 & 0x18) | 0x04;
			memcpy( &regs[0x0f], hw->bReg_0x0f_Mono, 10 );
			break;

		case SCANDATATYPE_BW:
			m_bCM = 1;
			regs[0x26] = (hw->bReg_0x26 & 0x18) | 0x04;
			memcpy( &regs[0x0f], hw->bReg_0x0f_Mono, 10 );
			break;
	}

	regs[0x27] = hw->bReg_0x27;

	if( pParam->bBitDepth > 8 ) {
		regs[0x09] |= 0x20;         /* 14/16bit image data */

	} else if( pParam->bBitDepth == 8 ) {
		regs[0x09] |= 0x18;        /* 8bits/per pixel */
	}
}

/**
 * Data output from NS983X should be times of 2-byte and every line
 * will append 2 status bytes
 */
static void
usb_GetPhyPixels( Plustek_Device *dev, ScanParam *sp )
{
	sp->Size.dwValidPixels = sp->Size.dwPixels * sp->PhyDpi.x / sp->UserDpi.x;

	if (sp->bBitDepth == 1) {

		/* Pixels should be times of 16 */
		sp->Size.dwPhyPixels =
		            (sp->Size.dwValidPixels + 15UL) & 0xfffffff0UL;
		sp->Size.dwPhyBytes = sp->Size.dwPhyPixels / 8UL + 2UL;

	} else if (sp->bBitDepth == 8) {

		/* Pixels should be times of 2 */
		sp->Size.dwPhyPixels = (sp->Size.dwValidPixels + 1UL) & 0xfffffffeUL;
		sp->Size.dwPhyBytes  = sp->Size.dwPhyPixels * sp->bChannels + 2UL;

		/* need to be adjusted fo CIS devices in color mode */
		if(usb_IsCISDevice( dev ) && (sp->bDataType == SCANDATATYPE_Color)) {
			sp->Size.dwPhyBytes *= 3;
		}
	}
	else /* sp->bBitDepth == 16 */
	{
		sp->Size.dwPhyPixels = sp->Size.dwValidPixels;
		sp->Size.dwPhyBytes  = sp->Size.dwPhyPixels * 2 * sp->bChannels + 2UL;

		/* need to be adjusted fo CIS devices in color mode */
		if(usb_IsCISDevice( dev ) && (sp->bDataType == SCANDATATYPE_Color)) {
			sp->Size.dwPhyBytes *= 3;
		}
	}
}

/** Calculate basic image settings like the number of physical bytes per line
 * etc...
 * Affected registers:<br>
 * 0x22/0x23 - Data Pixels Start<br>
 * 0x24/0x25 - Data Pixels End<br>
 * 0x4a/0x4b - Full Steps to Skip at Start of Scan
 *
 * @param dev    - pointer to our device structure,
 *                 it should contain all we need
 * @param pParam - pointer to the current scan parameters
 * @return - Nothing
 */
static void
usb_GetScanRect( Plustek_Device *dev, ScanParam *sp )
{
	u_short   wDataPixelStart, wLineEnd;

	DCapsDef *sCaps = &dev->usbDev.Caps;
	HWDef    *hw    = &dev->usbDev.HwSetting;
	u_char   *regs  = dev->usbDev.a_bRegs;

	/* Convert pixels to physical dpi based */
	usb_GetPhyPixels( dev, sp );

	/* Compute data start pixel */
#if 0
/* HEINER: check ADF stuff... */
	if(sp->bCalibration != PARAM_Gain &&
		sp->bCalibration != PARAM_Offset && ScanInf.m_fADF)
		wDataPixelStart = 2550 * sCaps->OpticDpi.x / 300UL -
				(u_short)(m_dHDPIDivider * sp->Size.dwValidPixels + 0.5);
	else
#endif
		wDataPixelStart = (u_short)((u_long)sp->Origin.x *
		                                            sCaps->OpticDpi.x / 300UL);

	/* during the calibration steps, we read the entire CCD data
	 */
	if((sp->bCalibration != PARAM_Gain)&&(sp->bCalibration != PARAM_Offset)) {
/* HEINER: check ADF stuff... */
#if 0
		if(ScanInf.m_fADF) {
			wDataPixelStart = 2550 * sCaps->OpticDpi.x / 300UL -
			    (u_short)(m_dHDPIDivider * sp->Size.dwValidPixels + 0.5);
		}
#endif
		wDataPixelStart += hw->wActivePixelsStart;
	}

	wLineEnd = wDataPixelStart + (u_short)(m_dHDPIDivider *
	                                               sp->Size.dwPhyPixels + 0.5);

	DBG( _DBG_INFO2, "* DataPixelStart=%u, LineEnd=%u\n",
	                                               wDataPixelStart, wLineEnd );
	if( wDataPixelStart & 1 ) {

		wDataPixelStart++;
		wLineEnd++;

		DBG( _DBG_INFO2, "* DataPixelStart=%u, LineEnd=%u (ADJ)\n",
		                                           wDataPixelStart, wLineEnd );
	}

	regs[0x22] = _HIBYTE( wDataPixelStart );
	regs[0x23] = _LOBYTE( wDataPixelStart );

	/* should match: wLineEnd-wDataPixelStart%(m_dHDPIDivider*2) = 0!! */
	regs[0x24] = _HIBYTE( wLineEnd );
	regs[0x25] = _LOBYTE( wLineEnd );

	DBG( _DBG_INFO2, ">> End-Start=%u, HDPI=%.2f\n",
	                                 wLineEnd-wDataPixelStart, m_dHDPIDivider);

	/* Y origin */
	if( sp->bCalibration == PARAM_Scan ) {

		if( hw->motorModel == MODEL_Tokyo600 ) {

			if(sp->PhyDpi.x <= 75)
				sp->Origin.y += 20;
			else if(sp->PhyDpi.x <= 100)
			{
				if (sp->bDataType == SCANDATATYPE_Color)
					sp->Origin.y += 0;
				else
					sp->Origin.y -= 6;
			}
			else if(sp->PhyDpi.x <= 150)
			{
				if (sp->bDataType == SCANDATATYPE_Color)
					sp->Origin.y -= 0;
			}
			else if(sp->PhyDpi.x <= 200)
			{
				if (sp->bDataType == SCANDATATYPE_Color)
					sp->Origin.y -= 10;
				else
					sp->Origin.y -= 4;
			}
			else if(sp->PhyDpi.x <= 300)
			{
				if (sp->bDataType == SCANDATATYPE_Color)
					sp->Origin.y += 16;
				else
					sp->Origin.y -= 18;
			}
			else if(sp->PhyDpi.x <= 400)
			{
				if (sp->bDataType == SCANDATATYPE_Color)
					sp->Origin.y += 15;
				else if(sp->bDataType == SCANDATATYPE_BW)
					sp->Origin.y += 4;
			}
			else /* if(sp->PhyDpi.x <= 600) */
			{
				if (sp->bDataType == SCANDATATYPE_Gray)
					sp->Origin.y += 4;
			}
		}

		/* Add gray mode offset (Green offset, we assume the CCD are
		 * always be RGB or BGR order).
		 */
		if (sp->bDataType != SCANDATATYPE_Color)
			sp->Origin.y += (u_long)(300UL *
			           sCaps->bSensorDistance / sCaps->OpticDpi.y);
	}

	sp->Origin.y=(u_short)((u_long)sp->Origin.y * hw->wMotorDpi/300UL);

	/* Something wrong, but I can not find it. */
	if( hw->motorModel == MODEL_HuaLien && sCaps->OpticDpi.x == 600)
		sp->Origin.y = sp->Origin.y * 297 / 298;

	DBG(_DBG_INFO2,"* Full Steps to Skip at Start = 0x%04x\n",
	                sp->Origin.y);

	regs[0x4a] = _HIBYTE( sp->Origin.y );
	regs[0x4b] = _LOBYTE( sp->Origin.y );
}

/** preset scan stepsize and fastfeed stepsize
 */
static void
usb_PresetStepSize( Plustek_Device *dev, ScanParam *pParam )
{
	u_short ssize;
	double  mclkdiv = pParam->dMCLK;
	HWDef  *hw      = &dev->usbDev.HwSetting;
	u_char *regs    = dev->usbDev.a_bRegs;

	ssize = (u_short)((double)CRYSTAL_FREQ / ( mclkdiv * 8.0 *
            (double)m_bCM * hw->dMaxMotorSpeed * 4.0 * (double)hw->wMotorDpi));

	regs[0x46] = _HIBYTE( ssize );
	regs[0x47] = _LOBYTE( ssize );
	regs[0x48] = _HIBYTE( ssize );
	regs[0x49] = _LOBYTE( ssize );

	DBG( _DBG_INFO2, "* StepSize(Preset) = %u (0x%04x)\n", ssize, ssize );
}

/** calculate default phase difference DPD
 */
static void
usb_GetDPD( Plustek_Device *dev  )
{
	int    qtcnt;	/* quarter speed count count reg 51 b2..3 */
	int    hfcnt;	/* half speed count reg 51 b0..1          */
	int    strev;   /* steps to reverse reg 50                */
	int    dpd;     /* calculated dpd reg 52:53               */
	int    st;      /* step size reg 46:47                    */

	HWDef  *hw    = &dev->usbDev.HwSetting;
	u_char *regs  = dev->usbDev.a_bRegs;

	qtcnt = (regs[0x51] & 0x30) >> 4;  /* quarter speed count */
	hfcnt = (regs[0x51] & 0xc0) >> 6;  /* half speed count    */

	if( _LM9831 == hw->chip )
		strev = regs[0x50] & 0x3f;    /* steps to reverse */
	else /* LM9832/3 */
	{
		if (qtcnt == 3)
			qtcnt = 8;
		if (hfcnt == 3)
			hfcnt = 8;
		strev = regs[0x50];           /* steps to reverse */
	}

	st = regs[0x46] * 256 + regs[0x47];     /* scan step size */

	if (m_wLineLength == 0)
		dpd = 0;
	else
	{
		dpd = (((qtcnt * 4) + (hfcnt * 2) + strev) * 4 * st) %
		                            (m_wLineLength * m_bLineRateColor);
		DBG( _DBG_INFO2, "* DPD =%u (0x%04x)\n", dpd, dpd );
		dpd = m_wLineLength * m_bLineRateColor - dpd;
	}

	DBG( _DBG_INFO2, "* DPD =%u (0x%04x), step size=%u, steps2rev=%u\n",
	                  dpd, dpd, st, strev);
	DBG( _DBG_INFO2, "* llen=%u, lineRateColor=%u, qtcnt=%u, hfcnt=%u\n",
	                  m_wLineLength, m_bLineRateColor, qtcnt, hfcnt );

	regs[0x51] |= (u_char)((dpd >> 16) & 0x03);
	regs[0x52] = (u_char)(dpd >> 8);
	regs[0x53] = (u_char)(dpd & 0xFF);
}

#define MCLKDIV_SCALING 2
#define _MIN(a,b) ((a) < (b) ? (a) : (b))
#define _MAX(a,b) ((a) > (b) ? (a) : (b))

/**
 */
static int
usb_GetMCLKDiv( Plustek_Device *dev )
{
	int     j, pixelbits, pixelsperline, r;
	int     minmclk, maxmclk, mclkdiv;
	double  hdpi, min_int_time;
	u_char *regs = dev->usbDev.a_bRegs;
	HWDef  *hw   = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "usb_GetMCLKDiv()\n" );

	r = 8; /* line rate */
	if ((regs[0x26] & 7) == 0) 
		r = 24; /* pixel rate */

	/* use high or low res min integration time */
	min_int_time = ((regs[0x9]&7) > 2 ? hw->dMinIntegrationTimeLowres:
	                                    hw->dMinIntegrationTimeHighres);

	minmclk = (int)ceil((double) MCLKDIV_SCALING * CRYSTAL_FREQ *
	                       min_int_time /((double)1000. * r * m_wLineLength));
	minmclk = _MAX(minmclk,MCLKDIV_SCALING);

	maxmclk = (int)(32.5*MCLKDIV_SCALING + .5); 

	DBG(_DBG_INFO2,"- lower mclkdiv limit=%f\n",(double)minmclk/MCLKDIV_SCALING);
	DBG(_DBG_INFO2,"- upper mclkdiv limit=%f\n",(double)maxmclk/MCLKDIV_SCALING);

	/* get the bits per pixel */
	switch (regs[0x9] & 0x38) {
		case 0:	    pixelbits=1; break;
		case 0x8:   pixelbits=2; break;
		case 0x10:  pixelbits=4; break;
		case 0x18:  pixelbits=8; break;
		default:	pixelbits=16;break;
	}

	/* compute the horizontal dpi (pixels per inch) */
	j    = regs[0x9] & 0x7;
	hdpi = ((j&1)*.5+1)*(j&2?2:1)*(j&4?4:1);	

	pixelsperline = (int)((256*regs[0x24]+regs[0x25]-256*regs[0x22]-regs[0x23])
	                       *pixelbits/(hdpi * 8));
	mclkdiv = (int)ceil((double)MCLKDIV_SCALING * pixelsperline * CRYSTAL_FREQ
	                /((double) 8. * m_wLineLength * dev->transferRate));

	DBG( _DBG_INFO2, "- hdpi          = %.3f\n", hdpi );
	DBG( _DBG_INFO2, "- pixelbits     = %u\n", pixelbits );
	DBG( _DBG_INFO2, "- pixelsperline = %u\n", pixelsperline );
	DBG( _DBG_INFO2, "- linelen       = %u\n", m_wLineLength );
	DBG( _DBG_INFO2, "- transferrate  = %lu\n", dev->transferRate );
	DBG( _DBG_INFO2, "- MCLK Divider  = %u\n", mclkdiv/MCLKDIV_SCALING );

	mclkdiv = _MAX(mclkdiv,minmclk);
	mclkdiv = _MIN(mclkdiv,maxmclk);
	DBG( _DBG_INFO2, "- Current MCLK Divider = %u\n", mclkdiv/MCLKDIV_SCALING );

	if( dev->transferRate == 2000000 ) {
		while (mclkdiv * hdpi < 6.*MCLKDIV_SCALING) {
			mclkdiv++;
		}
		DBG( _DBG_INFO2, "- HIGHSPEED MCLK Divider = %u\n", 
		     mclkdiv/MCLKDIV_SCALING );
	}

	return mclkdiv;
}

/** Plusteks' poor-man MCLK calculation...
 * at least we give the master clock divider and adjust the step size
 * and integration time (for 14/16 bit modes)
 */
static double
usb_GetMCLKDivider( Plustek_Device *dev, ScanParam *pParam )
{
	double dMaxIntegrationTime;
	double dMaxMCLKDivider;

	DCapsDef *sCaps = &dev->usbDev.Caps;
	HWDef    *hw    = &dev->usbDev.HwSetting;
	u_char   *regs  = dev->usbDev.a_bRegs;

	DBG( _DBG_INFO, "usb_GetMCLKDivider()\n" );

	if( dev->transferRate == 2000000 ) {
		int mclkdiv = usb_GetMCLKDiv(dev);
		pParam->dMCLK = (double)mclkdiv/MCLKDIV_SCALING;
	}

	m_dMCLKDivider = pParam->dMCLK;

	if (m_dHDPIDivider*m_dMCLKDivider >= 5.3/*6*/)
		m_bIntTimeAdjust = 0;
	else
		m_bIntTimeAdjust = ceil( 5.3/*6.0*/ / (m_dHDPIDivider*m_dMCLKDivider));

	if( pParam->bCalibration == PARAM_Scan ) {

		usb_GetMCLKDiv(dev);

		/*  Compare Integration with USB speed to find the best ITA value */
		if( pParam->bBitDepth > 8 )	{

			while( pParam->Size.dwPhyBytes >
			       (m_dMCLKDivider * m_bCM * m_wLineLength / 6 * 9 / 10) *
 			       (1 + m_bIntTimeAdjust)) {
				m_bIntTimeAdjust++;
			}	

			if( hw->motorModel == MODEL_HuaLien &&
				sCaps->bCCD == kNEC3799 && m_bIntTimeAdjust > bMaxITA) {
				m_bIntTimeAdjust = bMaxITA;
			}

			if(((hw->motorModel == MODEL_HP) && (sCaps->bCCD == kNECSLIM))/* ||
			 	( regs[0x26] & _ONE_CH_COLOR )*/) {

				bMaxITA = (u_char)floor((m_dMCLKDivider + 1) / 2.0);

				DBG( _DBG_INFO2, "* MaxITA (HP) = %u\n", bMaxITA );
				if( m_bIntTimeAdjust > bMaxITA ) {
					DBG( _DBG_INFO, "* ITA (%u) limited\n", m_bIntTimeAdjust );
					m_bIntTimeAdjust = bMaxITA;
				}
			}
		}
	}
	DBG( _DBG_INFO2, "* Integration Time Adjust = %u (HDPI=%.3f,MCLKD=%.3f)\n",
	                    m_bIntTimeAdjust, m_dHDPIDivider, m_dMCLKDivider );

	regs[0x08] = (u_char)((m_dMCLKDivider - 1) * 2);
	regs[0x19] = m_bIntTimeAdjust;

	if( m_bIntTimeAdjust != 0 ) {

		m_wStepSize = (u_short)((u_long) m_wStepSize *
		                    (m_bIntTimeAdjust + 1) / m_bIntTimeAdjust);
		if( m_wStepSize < 2 )
			m_wStepSize = 2;

		regs[0x46] = _HIBYTE(m_wStepSize);
		regs[0x47] = _LOBYTE(m_wStepSize);

		DBG( _DBG_INFO2, "* Stepsize = %u, 0x46=0x%02x 0x47=0x%02x\n",
		                   m_wStepSize, regs[0x46], regs[0x47] );
	    usb_GetDPD( dev );
	}
	
	/* Compute maximum MCLK divider base on maximum integration time for
	 * high lamp PWM, use equation 4
	 */
	dMaxIntegrationTime = hw->dIntegrationTimeHighLamp;
	dMaxMCLKDivider = (double)CRYSTAL_FREQ * dMaxIntegrationTime /
	                                    (1000 * 8 * m_bCM * m_wLineLength);

	/* Determine lamp PWM setting */
	if( m_dMCLKDivider > dMaxMCLKDivider ) {

		DBG( _DBG_INFO2, "* Setting GreenPWMDutyCycleLow\n" );
		regs[0x2a] = _HIBYTE( hw->wGreenPWMDutyCycleLow );
		regs[0x2b] = _LOBYTE( hw->wGreenPWMDutyCycleLow );

	} else {

		DBG( _DBG_INFO2, "* Setting GreenPWMDutyCycleHigh\n" );
		regs[0x2a] = _HIBYTE( hw->wGreenPWMDutyCycleHigh );
		regs[0x2b] = _LOBYTE( hw->wGreenPWMDutyCycleHigh );
	}

	DBG( _DBG_INFO2, "* Current MCLK Divider = %f\n", m_dMCLKDivider );
	return m_dMCLKDivider;
}

/** calculate the step size of each scan step
 */
static void
usb_GetStepSize( Plustek_Device *dev, ScanParam *pParam )
{
	HWDef  *hw  = &dev->usbDev.HwSetting;
	u_char *regs = dev->usbDev.a_bRegs;

	/* Compute step size using equation 1 */
	if (m_bIntTimeAdjust != 0) {
		m_wStepSize = (u_short)(((u_long) pParam->PhyDpi.y * m_wLineLength *
		              m_bLineRateColor * (m_bIntTimeAdjust + 1)) /
		              (4 * hw->wMotorDpi * m_bIntTimeAdjust));
	} else {
		m_wStepSize = (u_short)(((u_long) pParam->PhyDpi.y * m_wLineLength *
		               m_bLineRateColor) / (4 * hw->wMotorDpi));
	}

	if (m_wStepSize < 2)
		m_wStepSize = 2;

	m_wStepSize = m_wStepSize * 298 / 297;

	regs[0x46] = _HIBYTE( m_wStepSize );
	regs[0x47] = _LOBYTE( m_wStepSize );

	DBG( _DBG_INFO2, "* Stepsize = %u, 0x46=0x%02x 0x47=0x%02x\n",
	                  m_wStepSize, regs[0x46], regs[0x47] );
}

/**
 */
static void
usb_GetLineLength( Plustek_Device *dev, ScanParam *param )
{
/* [note]
 *  The ITA in this moment is always 0, it will be changed later when we
 *  calculate MCLK. This is very strange why this routine will not call
 *  again to get all new value after ITA was changed? If this routine
 *  never call again, maybe we remove all factor with ITA here.
 */
	int tr;
	int tpspd;  /* turbo/preview mode speed reg 0a b2..3                 */
	int tpsel;  /* turbo/preview mode select reg 0a b0..1                */
	int gbnd;   /* guardband duration reg 0e b4..7                       */
	int dur;    /* pulse duration reg 0e b0..3                           */
	int ntr;    /* number of tr pulses reg 0d b7                         */
	int afeop;  /* scan mode, 0=pixel rate, 1=line rate,                 */
	            /* 4=1 channel mode a, 5=1 channel mode b, reg 26 b0..2  */
	int ctmode; /* CIS tr timing mode reg 19 b0..1                       */
	int tp;     /* tpspd or 1 if tpsel=0                                 */
	int b;      /* if ctmode=0, (ntr+1)*((2*gbnd)+dur+1), otherwise 1    */
	int tradj;  /* ITA                                                   */
	int en_tradj;
	u_short      le;
	HWDef       *hw    = &dev->usbDev.HwSetting;
	ClkMotorDef *motor = usb_GetMotorSet( hw->motorModel );
	u_char      *regs  = dev->usbDev.a_bRegs;

	tpspd = (regs[0x0a] & 0x0c) >> 2;       /* turbo/preview mode speed  */
	tpsel = regs[0x0a] & 3;                 /* turbo/preview mode select */

	gbnd = (regs[0x0e] & 0xf0) >> 4;        /* TR fi1 guardband duration */
	dur = (regs[0x0e] & 0xf);               /* TR pulse duration         */

	ntr = regs[0x0d] / 128;                 /* number of tr pulses       */

	afeop = regs[0x26] & 7;           /* afe op - 3 channel or 1 channel */

	tradj = regs[0x19] & 0x7f;                /* integration time adjust */
	en_tradj = (tradj) ? 1 : 0;

	ctmode = (regs[0x0b] >> 3) & 3;                /* cis tr timing mode */

	m_bLineRateColor = 1;	
	if (afeop == 1 || afeop == 5) /* if 3 channel line or 1 channel mode b */
		m_bLineRateColor = 3;

	/* according to turbo/preview mode to set value */
	if( tpsel == 0 ) {
		tp = 1;
	} else {
		tp = tpspd + 2;
		if( tp == 5 )
			tp++;
	}

	b = 1;
	if( ctmode == 0 ) { /* CCD mode scanner*/
	
		b  = (ntr + 1) * ((2 * gbnd) + dur + 1);
		b += (1 - ntr) * en_tradj;
	}
	if( ctmode == 2 )   /* CIS mode scanner */
	    b = 3;

	/* it might be necessary to tweak this value, to keep the
	 * MCLK constant - see some sheet-fed devices
	 */
	le = hw->wLineEnd;
	if (motor->dpi_thresh != 0) {
		if( param->PhyDpi.y <= motor->dpi_thresh) {
			le = motor->lineend;
			DBG( _DBG_INFO2, "* Adjusting lineend: %u\n", le);
		}
		regs[0x20] = _HIBYTE( le );
		regs[0x21] = _LOBYTE( le );
	}

	tr = m_bLineRateColor * (le + tp * (b + 3 - ntr));

	if( tradj == 0 ) {
		if( ctmode == 0 )
			tr += m_bLineRateColor;
	} else {
	
		int le_phi, num_byteclk, num_mclkf, tr_fast_pix, extra_pix;
			
		/* Line color or gray mode */
		if( afeop != 0 ) {
		
			le_phi      = (tradj + 1) / 2 + 1 + 6;
			num_byteclk = ((le_phi + 8 * le + 8 * b + 4) /
						   (8 * tradj)) + 1;
			num_mclkf   = 8 * tradj * num_byteclk;
			tr_fast_pix = num_byteclk;
			extra_pix   = (num_mclkf - le_phi) % 8;
		}
		else /* 3 channel pixel rate color */
		{
			le_phi      = (tradj + 1) / 2 + 1 + 10 + 12;
			num_byteclk = ((le_phi + 3 * 8 * le + 3 * 8 * b + 3 * 4) /
						   (3 * 8 * tradj)) + 1;
			num_mclkf   = 3 * 8 * tradj * num_byteclk;
			tr_fast_pix = num_byteclk;
			extra_pix   = (num_mclkf - le_phi) % (3 * 8);
		}
		
		tr = b + le + 4 + tr_fast_pix;
		if (extra_pix == 0)
			tr++;
		tr *= m_bLineRateColor;
	}
	m_wLineLength = tr / m_bLineRateColor;

	DBG( _DBG_INFO2, "* LineLength=%d, LineRateColor=%u\n",
	                    m_wLineLength, m_bLineRateColor );
}

/** usb_GetMotorParam
 * registers 0x56, 0x57
 */
static void
usb_GetMotorParam( Plustek_Device *dev, ScanParam *pParam )
{
	int          idx, i;
	ClkMotorDef *clk;
	MDef        *md;
	DCapsDef    *sCaps = &dev->usbDev.Caps;
	HWDef       *hw    = &dev->usbDev.HwSetting;
	u_char      *regs  = dev->usbDev.a_bRegs;

	if (!_IS_PLUSTEKMOTOR(hw->motorModel)) {

		clk = usb_GetMotorSet( hw->motorModel );
		md  = clk->motor_sets;
		idx = 0;
		for( i = 0; i < _MAX_CLK; i++ ) {
			if( pParam->PhyDpi.y <= dpi_ranges[i] )
				break;
			idx++;
		}
		if( idx >= _MAX_CLK )
			idx = _MAX_CLK - 1;

		regs[0x56] = md[idx].pwm;
		regs[0x57] = md[idx].pwm_duty;

		regs[0x43] = 0;
		regs[0x44] = 0;

		if( md[idx].scan_lines_per_line > 1 ) {

			if((pParam->bBitDepth > 8) &&
				(pParam->bDataType == SCANDATATYPE_Color)) {

				regs[0x43] = 0xff;
				regs[0x44] = md[idx].scan_lines_per_line;

				DBG( _DBG_INFO2, "* Line Skipping : 0x43=0x%02x, 0x44=0x%02x\n",
				                  regs[0x43], regs[0x44] );
			}
		}
	} else {

		if( sCaps->OpticDpi.x == 1200 ) {

			switch( hw->motorModel ) {

			case MODEL_HuaLien:
			case MODEL_KaoHsiung:
			default:
				if(pParam->PhyDpi.x <= 200)
				{
					regs[0x56] = 1;
					regs[0x57] = 48;  /* 63; */
				}
				else if(pParam->PhyDpi.x <= 300)
				{
					regs[0x56] = 2;   /* 8;  */
					regs[0x57] = 48;  /* 56; */
				}
				else if(pParam->PhyDpi.x <= 400)
				{
					regs[0x56] = 8;	
					regs[0x57] = 48;	
				}
				else if(pParam->PhyDpi.x <= 600)
				{
					regs[0x56] = 2;   /* 10; */
					regs[0x57] = 48;  /* 56; */
				}
				else /* pParam->PhyDpi.x == 1200) */
				{
					regs[0x56] = 1;   /* 8;  */
					regs[0x57] = 48;  /* 56; */
				}
				break;
        	}
        } else {
        	switch ( hw->motorModel ) {

        	case MODEL_Tokyo600:
        		regs[0x56] = 16;
        		regs[0x57] = 4;	/* 2; */
        		break;
        	case MODEL_HuaLien:
        		{
        			if(pParam->PhyDpi.x <= 200)
        			{
        				regs[0x56] = 64;	/* 24; */
        				regs[0x57] = 4;	/* 16; */
        			}
        			else if(pParam->PhyDpi.x <= 300)
        			{
        				regs[0x56] = 64;	/* 16; */
        				regs[0x57] = 4;	/* 16; */
        			}
        			else if(pParam->PhyDpi.x <= 400)
        			{
        				regs[0x56] = 64;	/* 16; */
        				regs[0x57] = 4;	/* 16; */
        			}
        			else /* if(pParam->PhyDpi.x <= 600) */
        			{
/* HEINER: check ADF stuff... */
#if 0
        				if(ScanInf.m_fADF)
        				{
        					regs[0x56] = 8;
        					regs[0x57] = 48;
        				}
        				else
#endif
        				{
        					regs[0x56] = 64;	/* 2;  */
        					regs[0x57] = 4;	/* 48; */
        				}
        			}
        		}
        		break;
        	case MODEL_KaoHsiung:
        	default:
        		if(pParam->PhyDpi.x <= 200)
        		{
        			regs[0x56] = 24;
        			regs[0x57] = 16;
        		}
        		else if(pParam->PhyDpi.x <= 300)
        		{
        			regs[0x56] = 16;
        			regs[0x57] = 16;
        		}
        		else if(pParam->PhyDpi.x <= 400)
        		{
        			regs[0x56] = 16;
        			regs[0x57] = 16;
        		}
        		else /* if(pParam->PhyDpi.x <= 600) */
        		{
        			regs[0x56] = 2;
        			regs[0x57] = 48;
        		}
        		break;
        	}
        }
	}

	DBG( _DBG_INFO2, "* MOTOR-Settings: PWM=0x%02x, PWM_DUTY=0x%02x\n",
	                 regs[0x56], regs[0x57] );
}

/**
 */
static void
usb_GetPauseLimit( Plustek_Device *dev, ScanParam *pParam )
{
	int     coeffsize, scaler;
	HWDef  *hw   = &dev->usbDev.HwSetting;
	u_char *regs = dev->usbDev.a_bRegs;

	scaler = 1;
	if( hw->bReg_0x26 & _ONE_CH_COLOR ) {
		if( pParam->bDataType == SCANDATATYPE_Color ) {
			scaler = 3;
		}
	}

	/* compute size of coefficient ram */
	coeffsize = 4 + 16 + 16;	/* gamma and shading and offset */

	/* if 16 bit, then not all is used */
	if( regs[0x09] & 0x20 ) {
		coeffsize = 16 + 16;	/* no gamma */
	}
	coeffsize *= (2*3); /* 3 colors and 2 bytes/word */


	/* Get available buffer size in KB
	 * for 512kb this will be 296
	 * for 2Mb   this will be 1832
	 */
	m_dwPauseLimit = (u_long)(hw->wDRAMSize - (u_long)(coeffsize));
	m_dwPauseLimit -= ((pParam->Size.dwPhyBytes*scaler) / 1024 + 1);

	/* If not reversing, take into account the steps to reverse */
	if( regs[0x50] == 0 )
		m_dwPauseLimit -= ((regs[0x54] & 7) *
		                  (pParam->Size.dwPhyBytes * scaler) + 1023) / 1024;

	DBG( _DBG_INFO2, "* PL=%lu, coeffsize=%u, scaler=%u\n",
	                  m_dwPauseLimit, coeffsize, scaler );

	m_dwPauseLimit = usb_max( usb_min(m_dwPauseLimit,
	                  (u_long)ceil(pParam->Size.dwTotalBytes / 1024.0)), 2);

	regs[0x4e] = (u_char)floor((m_dwPauseLimit*512.0)/(2.0*hw->wDRAMSize));

	if( regs[0x4e] > 1 ) {
		regs[0x4e]--;
		if(regs[0x4e] > 1)
			regs[0x4e]--;
	} else
		regs[0x4e] = 1;

	/* resume, when buffer is 2/8 kbytes full (512k/2M memory)
	 */
	regs[0x4f] = 1;

	DBG( _DBG_INFO2, "* PauseLimit = %lu, [0x4e] = 0x%02x, [0x4f] = 0x%02x\n",
	                  m_dwPauseLimit, regs[0x4e], regs[0x4f] );
}

/** usb_GetScanLinesAndSize
 */
static void
usb_GetScanLinesAndSize( Plustek_Device *dev, ScanParam *pParam )
{
	DCapsDef *sCaps = &dev->usbDev.Caps;

	pParam->Size.dwPhyLines = (u_long)ceil((double) pParam->Size.dwLines *
	                                     pParam->PhyDpi.y / pParam->UserDpi.y);

	/* Calculate color offset */
	if (pParam->bCalibration == PARAM_Scan && pParam->bChannels == 3) {

		dev->scanning.bLineDistance = sCaps->bSensorDistance *
		                                  pParam->PhyDpi.y / sCaps->OpticDpi.x;
		pParam->Size.dwPhyLines += (dev->scanning.bLineDistance << 1);
	}
	else
		dev->scanning.bLineDistance = 0;

	pParam->Size.dwTotalBytes = pParam->Size.dwPhyBytes * pParam->Size.dwPhyLines;

	DBG( _DBG_INFO, "* PhyBytes   = %lu\n", pParam->Size.dwPhyBytes );
	DBG( _DBG_INFO, "* PhyLines   = %lu\n", pParam->Size.dwPhyLines );
	DBG( _DBG_INFO, "* TotalBytes = %lu\n", pParam->Size.dwTotalBytes );
}

/** function to preset/reset the merlin registers
 */
static SANE_Bool
usb_SetScanParameters( Plustek_Device *dev, ScanParam *pParam )
{
	static u_char reg8, reg38[6], reg48[2];

	ScanDef   *scan    = &dev->scanning;
	ScanParam *pdParam = &dev->scanning.sParam;
	HWDef     *hw      = &dev->usbDev.HwSetting;
	u_char    *regs    = dev->usbDev.a_bRegs;

	m_pParam = pParam;

	DBG( _DBG_INFO, "usb_SetScanParameters()\n" );

	if( !usb_IsScannerReady(dev))
		return SANE_FALSE;

	if(pParam->bCalibration == PARAM_Scan && pParam->bSource == SOURCE_ADF) {
/* HEINER: dSaveMoveSpeed is only used in func EjectPaper!!!
		dSaveMoveSpeed = hw->dMaxMoveSpeed;
*/
		hw->dMaxMoveSpeed = 1.0;
		usb_MotorSelect( dev, SANE_TRUE );
		usb_MotorOn( dev, SANE_TRUE );
	}

	/*
	 * calculate the basic settings...
	 */
	pParam->PhyDpi.x = usb_SetAsicDpiX( dev, pParam->UserDpi.x );
	pParam->PhyDpi.y = usb_SetAsicDpiY( dev, pParam->UserDpi.y );

	usb_SetColorAndBits( dev, pParam );
	usb_GetScanRect    ( dev, pParam );

	usb_PresetStepSize( dev, pParam );
	
	if( dev->caps.dwFlag & SFLAG_ADF ) {

		if( pParam->bCalibration == PARAM_Scan ) {

			if( pdParam->bSource == SOURCE_ADF ) {
				regs[0x50] = 0;
				regs[0x51] = 0x40;
				if( pParam->PhyDpi.x <= 300)
					regs[0x54] = (regs[0x54] & ~7) | 4;	/* 3; */
				else
					regs[0x54] = (regs[0x54] & ~7) | 5;	/* 4; */
			} else {
				regs[0x50] = hw->bStepsToReverse;
				regs[0x51] = hw->bReg_0x51;
				regs[0x54] &= ~7;
			}
		} else
			regs[0x50] = 0;
	} else {
		if( pParam->bCalibration == PARAM_Scan )
			regs[0x50] = hw->bStepsToReverse;
		else
			regs[0x50] = 0;
	}

	/* Assume we will not use ITA */
	regs[0x19] = m_bIntTimeAdjust = 0;

	/* Get variables from calculation algorithms */
	if(!(pParam->bCalibration == PARAM_Scan &&
          pParam->bSource == SOURCE_ADF && dev->usbDev.fLastScanIsAdf )) {

		DBG( _DBG_INFO2, "* Scan calculations...\n" );
		usb_GetLineLength ( dev, pParam );
		usb_GetStepSize   ( dev, pParam );
		usb_GetDPD        ( dev );
		usb_GetMCLKDivider( dev, pParam );
		usb_GetMotorParam ( dev, pParam );
	}

	/* Compute fast feed step size, use equation 3 and equation 8 */
	if( m_dMCLKDivider < 1.0)
		m_dMCLKDivider = 1.0;

	m_wFastFeedStepSize = (u_short)(CRYSTAL_FREQ /
	                          (m_dMCLKDivider * 8 * m_bCM * hw->dMaxMoveSpeed *
	                           4 * hw->wMotorDpi));
	/* CIS special ;-) */
	if((hw->bReg_0x26 & _ONE_CH_COLOR) && (m_bCM == 1)) {
		DBG( _DBG_INFO2, "* CIS FFStep-Speedup\n" );
		m_wFastFeedStepSize /= 3;
	}

	if( m_bIntTimeAdjust != 0 )
		m_wFastFeedStepSize /= m_bIntTimeAdjust;

	if(regs[0x0a])
		m_wFastFeedStepSize *= ((regs[0x0a] >> 2) + 2);
	regs[0x48] = _HIBYTE( m_wFastFeedStepSize );
	regs[0x49] = _LOBYTE( m_wFastFeedStepSize );

	DBG( _DBG_INFO2, "* FFStepSize = %u, [0x48] = 0x%02x, [0x49] = 0x%02x\n",
	                       m_wFastFeedStepSize, regs[0x48], regs[0x49] );

	/* Compute the number of lines to scan using actual Y resolution */
	usb_GetScanLinesAndSize( dev, pParam );
	
	/* Pause limit should be bounded by total bytes to read
	 * so that the chassis will not move too far.
	 */
	usb_GetPauseLimit( dev, pParam );

	/* For ADF .... */
	if(pParam->bCalibration == PARAM_Scan && pParam->bSource == SOURCE_ADF) {

		if( dev->usbDev.fLastScanIsAdf ) {

			regs[0x08] = reg8;
			memcpy( &regs[0x38], reg38, sizeof(reg38));
			memcpy( &regs[0x48], reg48, sizeof(reg48));

		} else {

			reg8 = regs[0x08];
			memcpy( reg38, &regs[0x38], sizeof(reg38));
			memcpy( reg48, &regs[0x48], sizeof(reg48));
		}
		usb_MotorSelect( dev, SANE_TRUE );
	}

	/* Reset LM983x's state machine before setting register values */
	if( !usbio_WriteReg( dev->fd, 0x18, 0x18 ))
		return SANE_FALSE;

	usleep(200 * 1000); /* Need to delay at least xxx microseconds */

	if( !usbio_WriteReg( dev->fd, 0x07, 0x20 ))
		return SANE_FALSE;

	if( !usbio_WriteReg( dev->fd, 0x19, 6 ))
		return SANE_FALSE;

	regs[0x07] = 0;
	regs[0x28] = 0;

	/* set unused registers to 0 */
	memset( &regs[0x03], 0, 3 );
	memset( &regs[0x5f], 0, 0x7f-0x5f+1 );

	if( !usb_IsSheetFedDevice(dev)) {
		/* we limit the possible scansteps to avoid, that the sensors bumps
		 * against the scanbed - not necessary for sheet-fed devices
		 */
		if(pParam->bCalibration==PARAM_Scan && pParam->bSource!=SOURCE_ADF) {

			u_long  lines     = pParam->Size.dwPhyLines + scan->bLinesToSkip +
			                                          scan->dwLinesDiscard + 5;
			u_short scansteps = (u_short)ceil((double)lines*
			                                 hw->wMotorDpi / pParam->PhyDpi.y);
			DBG( _DBG_INFO, "* Scansteps=%u (%lu*%u/%u)\n", scansteps,  lines,
			                hw->wMotorDpi, pParam->PhyDpi.y );
			regs[0x4c] = _HIBYTE(scansteps);
			regs[0x4d] = _LOBYTE(scansteps);
		}
	}

	/* set the merlin registers */
	_UIO(sanei_lm983x_write( dev->fd, 0x03, &regs[0x03], 3, SANE_TRUE));
	_UIO(sanei_lm983x_write( dev->fd, 0x08, &regs[0x08], 0x7f - 0x08+1, SANE_TRUE));

	usleep(100);
	
	if( !usbio_WriteReg( dev->fd, 0x07, 0 ))
		return SANE_FALSE;

	DBG( _DBG_INFO, "usb_SetScanParameters() done.\n" );
	return SANE_TRUE;
}

/**
 */
static SANE_Bool
usb_ScanBegin( Plustek_Device *dev, SANE_Bool auto_park )
{
	u_char  value;
	u_short inches;
	HWDef       *hw   = &dev->usbDev.HwSetting;
	DCapsDef    *sc   = &dev->usbDev.Caps;
	u_char      *regs = dev->usbDev.a_bRegs;

	DBG( _DBG_INFO, "usb_ScanBegin()\n" );

	if( !usb_Wait4ScanSample( dev ))
		return SANE_FALSE;

	/* save the request for usb_ScanEnd () */
	m_fAutoPark = auto_park;

	/* Disable home sensor during scan, or the chassis cannot move */
	value = ((m_pParam->bCalibration == PARAM_Scan &&
	        m_pParam->bSource == SOURCE_ADF)? (regs[0x58] & ~7): 0);

	if(!usbio_WriteReg( dev->fd, 0x58, value ))
		return SANE_FALSE;

	/* Check if scanner is ready for receiving command */
	if( !usb_IsScannerReady(dev))
		return SANE_FALSE;

	/* Flush cache - only LM9831 (Source: National Semiconductors) */
	if( _LM9831 == hw->chip ) {

		for(;;) {

			if( SANE_TRUE == cancelRead ) {
				DBG( _DBG_INFO, "ScanBegin() - Cancel detected...\n" );
				return SANE_FALSE;
			}
			
			_UIO(usbio_ReadReg( dev->fd, 0x01, &m_bOldScanData ));
			if( m_bOldScanData ) {

				u_long dwBytesToRead = m_bOldScanData * hw->wDRAMSize * 4;
				u_char *pBuffer      = malloc( sizeof(u_char) * dwBytesToRead );

				DBG(_DBG_INFO,"Flushing cache - %lu bytes (bOldScanData=%u)\n",
				                                dwBytesToRead, m_bOldScanData );

				_UIO(sanei_lm983x_read( dev->fd, 0x00, pBuffer, 
				                                  dwBytesToRead, SANE_FALSE ));
				free( pBuffer );

			} else
				break;
		}
	}

	/* Download map & Shading data */
	if(( m_pParam->bCalibration == PARAM_Scan && !usb_MapDownload( dev )) ||
	    !usb_DownloadShadingData( dev, m_pParam->bCalibration )) {
		return SANE_FALSE;
	}

	/* Move chassis and start to read image data */
	if (!usbio_WriteReg( dev->fd, 0x07, 3 ))
		return SANE_FALSE;

	usbio_ReadReg( dev->fd, 0x01, &m_bOldScanData );
	m_bOldScanData = 0;                     /* No data at all  */

	m_fStart = m_fFirst = SANE_TRUE;        /* Prepare to read */

	DBG( _DBG_DREGS, "Register Dump before reading data:\n" );
	dumpregs( dev->fd, NULL );

	inches = (u_short)((m_pParam->Origin.y *300UL)/hw->wMotorDpi);
	DBG( _DBG_INFO2, ">>> INCH=%u, DOY=%u\n", inches, sc->Normal.DataOrigin.y );
	if( inches > sc->Normal.DataOrigin.y )
		usb_WaitPos( dev, 150, SANE_FALSE );

	DBG( _DBG_INFO, "usb_ScanBegin() done.\n" );
	return SANE_TRUE;
}

/** usb_ScanEnd
 * stop all the processing stuff and reposition sensor back home
 */
static SANE_Bool
usb_ScanEnd( Plustek_Device *dev )
{
	u_char value;

	DBG( _DBG_INFO, "usbDev_ScanEnd(), start=%u, park=%u\n",
	                                                   m_fStart, m_fAutoPark );
	usbio_ReadReg( dev->fd, 0x07, &value );
	if( value == 3 || value != 2 )
		usbio_WriteReg( dev->fd, 0x07, 0 );

	if( m_fStart ) {
		m_fStart = SANE_FALSE;

		if( m_fAutoPark )
			usb_ModuleToHome( dev, SANE_FALSE );
	}
	else if( SANE_TRUE == cancelRead ) {
	
		usb_ModuleToHome( dev, SANE_FALSE );
	}
	return SANE_TRUE;
}

/**
 */
static SANE_Bool
usb_IsDataAvailableInDRAM( Plustek_Device *dev )
{
	/* Compute polling timeout
	 *	Height (Inches) / MaxScanSpeed (Inches/Second) = Seconds to move the
     *  module from top to bottom. Then convert the seconds to miliseconds
     *  by multiply 1000. We add extra 2 seconds to get some tolerance.
     */
	u_char         a_bBand[3];
	long           dwTicks;
    struct timeval t;
	u_char         *regs = dev->usbDev.a_bRegs;

	DBG( _DBG_INFO, "usb_IsDataAvailableInDRAM()\n" );

	gettimeofday( &t, NULL);	
	dwTicks = t.tv_sec + 30;

	for(;;)	{

		_UIO( sanei_lm983x_read( dev->fd, 0x01, a_bBand, 3, SANE_FALSE ));

		gettimeofday( &t, NULL);	
	    if( t.tv_sec > dwTicks )
			break;

		if( usb_IsEscPressed()) {
			DBG(_DBG_INFO,"usb_IsDataAvailableInDRAM() - Cancel detected...\n");
			return SANE_FALSE;
		}
			
		/* It is not stable for read */
		if((a_bBand[0] != a_bBand[1]) && (a_bBand[1] != a_bBand[2]))
			continue;

		if( a_bBand[0] > m_bOldScanData ) {

			if( m_pParam->bSource != SOURCE_Reflection )

				usleep(1000*(30 * regs[0x08] * dev->usbDev.Caps.OpticDpi.x / 600));
			else
				usleep(1000*(20 * regs[0x08] * dev->usbDev.Caps.OpticDpi.x / 600));

			DBG( _DBG_INFO, "Data is available\n" );
			return SANE_TRUE;
		}
	}

	DBG( _DBG_INFO, "NO Data available\n" );
	return SANE_FALSE;
}

/**
 */
static SANE_Bool
usb_ScanReadImage( Plustek_Device *dev, void *pBuf, u_long dwSize )
{
	u_char       *regs = dev->usbDev.a_bRegs;
	SANE_Status   res;

	DBG( _DBG_READ, "usb_ScanReadImage(%lu)\n", dwSize );

	if( m_fFirst ) {

		m_fFirst = SANE_FALSE;

		/* Wait for data band ready */
		if (!usb_IsDataAvailableInDRAM( dev )) {
			DBG( _DBG_ERROR, "Nothing to read...\n" );
			return SANE_FALSE;
		}

		/* restore the fast forward stepsize...*/
		sanei_lm983x_write(dev->fd, 0x48, &regs[0x48], 2, SANE_TRUE);
	}
	res = sanei_lm983x_read(dev->fd, 0x00, (u_char *)pBuf, dwSize, SANE_FALSE);

	/* check for pressed ESC button, as sanei_lm983x_read() may take some time
	 */
	if( usb_IsEscPressed()) {
		DBG(_DBG_INFO,"usb_ScanReadImage() - Cancel detected...\n");
		return SANE_FALSE;
	}

	DBG( _DBG_READ, "usb_ScanReadImage() done, result: %d\n", res );
	if( SANE_STATUS_GOOD == res ) {
		return SANE_TRUE;
	}

	DBG( _DBG_ERROR, "usb_ScanReadImage() failed\n" );
	return SANE_FALSE;
}

/** calculate the number of pixels per line and lines out of a given
 * crop-area. The size of the area is given on a 300dpi base!
 */
static void
usb_GetImageInfo( Plustek_Device *dev, ImgDef *pInfo, WinInfo *pSize )
{
	DBG( _DBG_INFO, "usb_GetImageInfo()\n" );

	pSize->dwPixels = (u_long)pInfo->crArea.cx * pInfo->xyDpi.x / 300UL;
	pSize->dwLines  = (u_long)pInfo->crArea.cy * pInfo->xyDpi.y / 300UL;

	DBG( _DBG_INFO2,"Area: cx=%u, cy=%u\n",pInfo->crArea.cx,pInfo->crArea.cy);

	switch( pInfo->wDataType ) {

		case COLOR_TRUE48:
			pSize->dwBytes = pSize->dwPixels * 6UL;
			break;
			
		case COLOR_TRUE24:
			if( dev->scanning.fGrayFromColor > 7 ){
				pSize->dwBytes  = (pSize->dwPixels + 7UL) >> 3;
				pSize->dwPixels = pSize->dwBytes * 8;
			} else {
				pSize->dwBytes = pSize->dwPixels * 3UL;
			}
			break;

		case COLOR_GRAY16:
			pSize->dwBytes = pSize->dwPixels << 1;
			break;

		case COLOR_256GRAY:
			pSize->dwBytes = pSize->dwPixels;
			break;

		default:
			pSize->dwBytes  = (pSize->dwPixels + 7UL) >> 3;
			pSize->dwPixels = pSize->dwBytes * 8;
			break;
	}
}

/**
 */
static void
usb_SaveImageInfo( Plustek_Device *dev, ImgDef *pInfo )
{
	HWDef     *hw     = &dev->usbDev.HwSetting;
	ScanParam *pParam = &dev->scanning.sParam;

	DBG( _DBG_INFO, "usb_SaveImageInfo()\n" );

	/* Dpi & Origins */
	pParam->UserDpi  = pInfo->xyDpi;
	pParam->Origin.x = pInfo->crArea.x;
	pParam->Origin.y = pInfo->crArea.y;

	/* Source & Bits */
	pParam->bBitDepth = 8;

	switch( pInfo->wDataType ) {

		case COLOR_TRUE48:
			pParam->bBitDepth = 16;
			/* fall through... */
			
		case COLOR_TRUE24:
			pParam->bDataType = SCANDATATYPE_Color;

			/* AFE operation: one or 3 channels ! */
			if( hw->bReg_0x26 & _ONE_CH_COLOR )
				pParam->bChannels = 1;
			else
				pParam->bChannels = 3;
			break;

		case COLOR_GRAY16:
			pParam->bBitDepth = 16;
			/* fall through... */

		case COLOR_256GRAY:
			pParam->bDataType = SCANDATATYPE_Gray;
			pParam->bChannels = 1;
			break;

		default:
			pParam->bBitDepth = 1;
			pParam->bDataType = SCANDATATYPE_BW;
			pParam->bChannels = 1;
	}

	DBG( _DBG_INFO, "* dwFlag = 0x%08lx\n", pInfo->dwFlag );

	if( pInfo->dwFlag & SCANDEF_Transparency )
		pParam->bSource = SOURCE_Transparency;
	else if( pInfo->dwFlag & SCANDEF_Negative )
		pParam->bSource = SOURCE_Negative;
	else if( pInfo->dwFlag & SCANDEF_Adf )
		pParam->bSource = SOURCE_ADF;
	else
		pParam->bSource = SOURCE_Reflection;

	/* it seems, that we need to adjust the Origin.x when we have a
	 * sheetfed device to avoid stripes in the resulting pictures
	 */
	if( usb_IsSheetFedDevice(dev)) {
	
		int step, div, org, xdpi;

		xdpi = usb_SetAsicDpiX( dev, pParam->UserDpi.x );

		if ((xdpi * 2) <= 300)
			div = 300;
		else if ((xdpi * 2) <= 600)
			div = 600;
		else if ((xdpi * 2) <= 1200)
			div = 1200;
		else
			div = 2400;

		step = div / xdpi;
		org  = pParam->Origin.x;

		pParam->Origin.x = (pParam->Origin.x / step) * step;

		if (org != pParam->Origin.x)
			DBG(_DBG_INFO, "* Origin.x adjusted: %i -> %i\n",
			               org, pParam->Origin.x);
	}
}

/* END PLUSTEK-USBSCAN.C ....................................................*/
