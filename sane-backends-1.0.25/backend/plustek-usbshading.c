/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbshading.c
 *  @brief Calibration routines.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - minor fixes
 *        - added workaround stuff for EPSON1250
 * - 0.42 - added workaround stuff for UMAX 3400
 * - 0.43 - added call to usb_switchLamp before reading dark data
 * - 0.44 - added more debug output and bypass calibration
 *        - added dump of shading data
 * - 0.45 - added coarse calibration for CIS devices
 *        - added _WAF_SKIP_FINE to skip the results of fine calibration
 *        - CanoScan fixes and fine-tuning
 * - 0.46 - CanoScan will now be calibrated by code in plustek-usbcal.c
 *        - added functions to save and restore calibration data from a file
 *        - fixed some TPA issues
 * - 0.47 - made calibration work on big-endian machines
 * - 0.48 - added more debug output
 *        - added usb_AutoWarmup()
 * - 0.49 - a_bRegs is now part of the device structure
 *        - using now PhyDpi.y as selector for the motor MCLK range
 * - 0.50 - readded kCIS670 to add 5% extra to LiDE20 fine calibration
 *        - fixed line statistics and added data output
 * - 0.51 - added fine calibration cache
 * - 0.52 - added get_ptrs to let various sensororders work
 *          correctly
 *        - fixed warning condition
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

/************** global stuff - I hate it, but we need it... ******************/

#define _MAX_AUTO_WARMUP 60  /**< number of loops                            */
#define _AUTO_SLEEP       1  /**< time to sleep, when lamp is not stable     */
#define _AUTO_THRESH     60  /**< threshold for stop waiting (normal lamp)   */
#define _AUTO_TPA_THRESH 40  /**< threshold for stop waiting (TPA lamp)      */

#define _MAX_GAIN_LOOPS  10  /**< max number of loops for coarse calibration */
#define _TLOOPS           3  /**< test loops for transfer rate measurement   */

#define SWAP_COARSE
#define SWAP_FINE

/************************** static variables *********************************/

static RGBUShortDef Gain_Hilight;
static RGBUShortDef Gain_NegHilight;
static RGBByteDef   Gain_Reg;

static u_long     m_dwPixels;
static ScanParam  m_ScanParam;
static u_long     m_dwIdealGain;

static double dMCLK, dExpect, dMax;
static double dMCLK_ADF;

/** do some statistics...
 */
static void usb_line_statistics( char *cmt, u_short* buf,
                                 u_long dim_x, SANE_Bool color )
{
	char         fn[50];
	int          i, channel;
	u_long       dw, imad, imid, alld, cld, cud;
	u_short      mid, mad, aved, lbd, ubd, tmp;
	MonoWordDef *pvd, *pvd2;
	FILE        *fp;
	SANE_Bool    swap = usb_HostSwap();

	pvd = pvd2 = (MonoWordDef*)buf;

	if( color )
		channel = 3;
	else
		channel = 1;

	for( i = 0; i < channel; i++ ) {

		mid  = 0xFFFF;
		mad  = 0;
		imid = 0;
		imad = 0;
		alld = 0;

		fp = NULL;
		if( DBG_LEVEL >= _DBG_DCALDATA ) {
			sprintf( fn, "%scal%u.dat", cmt, i );
			fp = fopen( fn, "w+b" );
			if( fp == NULL )
				DBG( _DBG_ERROR, "Could not open %s\n", fn );
		}

		/* do the standard min/max stuff */
		for( dw = 0; dw < dim_x; pvd++, dw++ ) {

			if( swap )
				tmp = _LOBYTE(pvd->Mono) * 256 + _HIBYTE(pvd->Mono);
			else
				tmp = pvd->Mono;

			if( tmp > mad ) {
				mad  = tmp;
				imad = dw;
			}

			if( tmp < mid ) {
				mid  = tmp;
				imid = dw;
			}

			if( fp )
				fprintf(fp, "%u\n", tmp );

			alld += tmp;
		}

		if( fp )
			fclose(fp);

		/* calculate average and 5% limit */
		aved = (u_short)(alld/dim_x);
		lbd  = aved - 0.05*aved;
		ubd  = aved + 0.05*aved;
		cld  = 0;
		cud  = 0;

		/* find the number of values beyond the 5% limits */
		for( dw = 0; dw < dim_x; pvd2++, dw++ ) {

			if( swap )
				tmp = _LOBYTE(pvd2->Mono) * 256 + _HIBYTE(pvd2->Mono);
			else
				tmp = pvd2->Mono;

			if( tmp > ubd ) cud++;
			if( tmp < lbd ) cld++;
		}

		DBG( _DBG_INFO2, "Color[%u] (%s): %lu all "
		                 "min=%u(%lu) max=%u(%lu) ave=%u\n",
		                 i, cmt, dim_x, mid, imid, mad, imad, aved);
		DBG( _DBG_INFO2, "5%%: low@%u (count=%lu), upper@%u (count=%lu)\n",
		                 lbd, cld, ubd, cud);
	}
}

/**
 */
static void
usb_PrepareFineCal( Plustek_Device *dev, ScanParam *tmp_sp, u_short cal_dpi )
{
	ScanParam *sp    = &dev->scanning.sParam;
	DCapsDef  *scaps = &dev->usbDev.Caps;

	*tmp_sp = *sp;

	if( dev->adj.cacheCalData ) {

		DBG( _DBG_INFO2, "* Cal-cache active, tweaking scanparams"
		                 " - DPI=%u!\n", cal_dpi );

		tmp_sp->UserDpi.x = usb_SetAsicDpiX(dev, sp->UserDpi.x );
		if( cal_dpi != 0 )
			tmp_sp->UserDpi.x = cal_dpi;

		tmp_sp->PhyDpi        = scaps->OpticDpi;
		tmp_sp->Origin.x      = 0;
		tmp_sp->Size.dwPixels = scaps->Normal.Size.x *
		                        usb_SetAsicDpiX(dev, tmp_sp->UserDpi.x)/ 300UL;
	}

#if 0
	if( tmp_sp->PhyDpi.x > 75)
		tmp_sp->Size.dwLines = 64;
	else
#endif
		tmp_sp->Size.dwLines = 32;

	tmp_sp->Origin.y  = 0;
	tmp_sp->bBitDepth = 16;

	tmp_sp->UserDpi.y    = scaps->OpticDpi.y;
	tmp_sp->Size.dwBytes = tmp_sp->Size.dwPixels * 2 * tmp_sp->bChannels;

	if( usb_IsCISDevice(dev) && (tmp_sp->bDataType == SCANDATATYPE_Color)) {
		tmp_sp->Size.dwBytes *= 3;
	}

	tmp_sp->dMCLK = dMCLK;
}

/**
 */
static double usb_GetMCLK( Plustek_Device *dev, ScanParam *param )
{
	int          idx, i;
	double       mclk;
	ClkMotorDef *clk;
	HWDef       *hw = &dev->usbDev.HwSetting;

	clk = usb_GetMotorSet( hw->motorModel );
	idx = 0;
	for( i = 0; i < _MAX_CLK; i++ ) {
		if( param->PhyDpi.y <= dpi_ranges[i] )
			break;
		idx++;
	}
	if( idx >= _MAX_CLK )
		idx = _MAX_CLK - 1;

	if( param->bDataType != SCANDATATYPE_Color ) {

		if( param->bBitDepth > 8 )
			mclk = clk->gray_mclk_16[idx];
		else
			mclk = clk->gray_mclk_8[idx];
	} else {
		if( param->bBitDepth > 8 )
			mclk = clk->color_mclk_16[idx];
		else
			mclk = clk->color_mclk_8[idx];
	}

	DBG( _DBG_INFO, "GETMCLK[%u/%u], using entry %u: %.3f, %u\n",
	     hw->motorModel, param->bDataType, idx, mclk, param->PhyDpi.x );
	return mclk;
}

/** usb_SetMCLK
 * get the MCLK out of our table
 */
static void usb_SetMCLK( Plustek_Device *dev, ScanParam *param )
{
	HWDef *hw = &dev->usbDev.HwSetting;

	dMCLK = usb_GetMCLK( dev, param );
	param->dMCLK = dMCLK; 

	DBG( _DBG_INFO, "SETMCLK[%u/%u]: %.3f\n", 
	     hw->motorModel, param->bDataType, dMCLK );
}

/** usb_SetDarkShading
 * download the dark shading data to Merlins' DRAM
 */
static SANE_Bool usb_SetDarkShading( Plustek_Device *dev, u_char channel,
                                     void *coeff_buffer, u_short wCount )
{
	int     res;
	u_char *regs = dev->usbDev.a_bRegs;

	regs[0x03] = 0;
	if( channel == CHANNEL_green )
		regs[0x03] |= 4;
	else
		if( channel == CHANNEL_blue )
			regs[0x03] |= 8;

	if( usbio_WriteReg( dev->fd, 0x03, regs[0x03] )) {

		/* Dataport address is always 0 for setting offset coefficient */
		regs[0x04] = 0;
		regs[0x05] = 0;

		res = sanei_lm983x_write( dev->fd, 0x04, &regs[0x04], 2, SANE_TRUE );

		/* Download offset coefficients */
		if( SANE_STATUS_GOOD == res ) {
			
			res = sanei_lm983x_write( dev->fd, 0x06,
			                          (u_char*)coeff_buffer, wCount, SANE_FALSE );
			if( SANE_STATUS_GOOD == res )
				return SANE_TRUE;
		}
	}
	
	DBG( _DBG_ERROR, "usb_SetDarkShading() failed\n" );
	return SANE_FALSE;
}

/** usb_SetWhiteShading
 * download the white shading data to Merlins' DRAM
 */
static SANE_Bool usb_SetWhiteShading( Plustek_Device *dev, u_char channel,
                                      void *data_buffer, u_short wCount )
{
	int     res;
	u_char *regs = dev->usbDev.a_bRegs;

	regs[0x03] = 1;
	if (channel == CHANNEL_green)
		regs [0x03] |= 4;
	else
		if (channel == CHANNEL_blue)
			regs[0x03] |= 8;

	if( usbio_WriteReg( dev->fd, 0x03, regs[0x03] )) {

		/* Dataport address is always 0 for setting offset coefficient */
		regs[0x04] = 0;
		regs[0x05] = 0;

		res = sanei_lm983x_write( dev->fd, 0x04, &regs[0x04], 2, SANE_TRUE );

		/* Download offset coefficients */
		if( SANE_STATUS_GOOD == res ) {
			res = sanei_lm983x_write(dev->fd, 0x06,
			                        (u_char*)data_buffer, wCount, SANE_FALSE );
			if( SANE_STATUS_GOOD == res )
				return SANE_TRUE;
		}
	}
	
	DBG( _DBG_ERROR, "usb_SetWhiteShading() failed\n" );
	return SANE_FALSE;
}

/** usb_GetSWOffsetGain4TPA
 *  preset the offset and gain parameter for fine calibration for
 *  TPA/ADF scanning
 */
static void usb_GetSWOffsetGain4TPA( Plustek_Device *dev )
{
	ScanParam *param = &dev->scanning.sParam;
	DCapsDef  *sCaps = &dev->usbDev.Caps;

	switch( sCaps->bCCD ) {
		case kEPSON:
			DBG( _DBG_INFO2, "kEPSON TPA adjustments\n" );
			param->swGain[0] = 1000;
			param->swGain[1] = 1000;
			param->swGain[2] = 1000;
		break;
	}
}

/** usb_GetSWOffsetGain
 * preset the offset and gain parameter for fine calibration for normal
 * scanning
 */
static void usb_GetSWOffsetGain( Plustek_Device *dev )
{
	ScanParam *param = &dev->scanning.sParam;
	DCapsDef  *sCaps  = &dev->usbDev.Caps;
	HWDef     *hw     = &dev->usbDev.HwSetting;

	param->swOffset[0] = 0;
	param->swOffset[1] = 0;
	param->swOffset[2] = 0;

	param->swGain[0] = 1000;
	param->swGain[1] = 1000;
	param->swGain[2] = 1000;

	if( param->bSource != SOURCE_Reflection ) {
		usb_GetSWOffsetGain4TPA( dev );
		return;
	}

	/* only valid for normal scanning... */
	switch( sCaps->bCCD ) {

	case kEPSON:
		DBG( _DBG_INFO2, "kEPSON adjustments\n" );
#if 0
		param->swGain[0] = 800;
		param->swGain[1] = 800;
		param->swGain[2] = 800;
#endif
		break;
	
	case kNECSLIM:
		DBG( _DBG_INFO2, "kNECSLIM adjustments\n" );
		if( param->PhyDpi.x <= 150 ) {
			param->swOffset[0] = 600;
			param->swOffset[1] = 500;
			param->swOffset[2] = 300;
			param->swGain[0] = 960;
			param->swGain[1] = 970;
			param->swGain[2] = 1000;
		} else if (param->PhyDpi.x <= 300) {
			param->swOffset[0] = 700;
			param->swOffset[1] = 600;
			param->swOffset[2] = 400;
			param->swGain[0] = 967;
			param->swGain[1] = 980;
			param->swGain[2] = 1000;
		} else {
			param->swOffset[0] = 900;
			param->swOffset[1] = 850;
			param->swOffset[2] = 620;
			param->swGain[0] = 965;
			param->swGain[1] = 980;
			param->swGain[2] = 1000;
		}
		break;

	case kNEC8861:
		DBG( _DBG_INFO2, "kNEC8861 adjustments\n" );
		break;

	case kCIS670:
		DBG( _DBG_INFO2, "kCIS670 adjustments\n" );
		if(param->bDataType == SCANDATATYPE_Color) {

			param->swGain[0] =
			param->swGain[1] =
			param->swGain[2] = 952;

			param->swOffset[0] =
			param->swOffset[1] =
			param->swOffset[2] = 1000;
		}
		break;
#if 0
	case kCIS650:
	case kCIS1220:

		DBG( _DBG_INFO2, "kCIS adjustments\n" );
		if(param->bDataType == SCANDATATYPE_Color) {

			param->swGain[0] =
			param->swGain[1] =
			param->swGain[2] = 952;

			param->swOffset[0] =
			param->swOffset[1] =
			param->swOffset[2] = 1000;
		}
		break;
	case kCIS1240:
		DBG( _DBG_INFO2, "kCIS1240 adjustments\n" );
		if(param->bDataType == SCANDATATYPE_Color) {

			param->swGain[0] = 950;
			param->swGain[1] = 950;
			param->swGain[2] = 900;

			param->swOffset[0] =
			param->swOffset[1] =
			param->swOffset[2] = 0; /*1000;*/
		}
		break;
#endif

	case kNEC3799:
		DBG( _DBG_INFO2, "kNEC3799 adjustments\n" );
		if( sCaps->bPCB == 2 ) {
			if( param->PhyDpi.x <= 150 ) {
				param->swOffset[0] = 600;
				param->swOffset[1] = 500;
				param->swOffset[2] = 300;
				param->swGain[0] = 960;
				param->swGain[1] = 970;
				param->swGain[2] = 1000;
			} else if (param->PhyDpi.x <= 300) {
				param->swOffset[0] = 700;
				param->swOffset[1] = 600;
				param->swOffset[2] = 400;
				param->swGain[0] = 967;
				param->swGain[1] = 980;
				param->swGain[2] = 1000;
			} else {
				param->swOffset[0] = 900;
				param->swOffset[1] = 850;
				param->swOffset[2] = 620;
				param->swGain[0] = 965;
				param->swGain[1] = 980;
				param->swGain[2] = 1000;
			}
		} else if( hw->motorModel == MODEL_KaoHsiung ) {
			param->swOffset[0] = 1950;
			param->swOffset[1] = 1700;
			param->swOffset[2] = 1250;
			param->swGain[0] = 955;
			param->swGain[1] = 950;
			param->swGain[2] = 1000;
		} else { /* MODEL_Hualien */
			if( param->PhyDpi.x <= 300 ) {
				if( param->bBitDepth > 8 ) {
					param->swOffset[0] = 0;
					param->swOffset[1] = 0;
					param->swOffset[2] = -300;
					param->swGain[0] = 970;
					param->swGain[1] = 985;
					param->swGain[2] = 1050;
				} else {
					param->swOffset[0] = -485;
					param->swOffset[1] = -375;
					param->swOffset[2] = -628;
					param->swGain[0] = 970;
					param->swGain[1] = 980;
					param->swGain[2] = 1050;
				}
			} else {
				if( param->bBitDepth > 8 ) {
					param->swOffset[0] = 1150;
					param->swOffset[1] = 1000;
					param->swOffset[2] = 700;
					param->swGain[0] = 990;
					param->swGain[1] = 1000;
					param->swGain[2] = 1050;
				} else {
					param->swOffset[0] = -30;
					param->swOffset[1] = 0;
					param->swOffset[2] = -250;
					param->swGain[0] = 985;
					param->swGain[1] = 995;
					param->swGain[2] = 1050;
				}
			}
		}
		break;

	case kSONY548:
		DBG( _DBG_INFO2, "kSONY548 adjustments\n" );
		if(param->bDataType == SCANDATATYPE_Color)
		{
			if(param->PhyDpi.x <= 75)
			{
				param->swOffset[0] = 650;
				param->swOffset[1] = 850;
				param->swOffset[2] = 500;
				param->swGain[0] = 980;
				param->swGain[1] = 1004;
				param->swGain[2] = 1036;
			}
			else if(param->PhyDpi.x <= 300)
			{
				param->swOffset[0] = 700;
				param->swOffset[1] = 900;
				param->swOffset[2] = 550;
				param->swGain[0] = 970;
				param->swGain[1] = 995;
				param->swGain[2] = 1020;
			}
			else if(param->PhyDpi.x <= 400)
			{
				param->swOffset[0] = 770;
				param->swOffset[1] = 1010;
				param->swOffset[2] = 600;
				param->swGain[0] = 970;
				param->swGain[1] = 993;
				param->swGain[2] = 1023;
			}
			else
			{
				param->swOffset[0] = 380;
				param->swOffset[1] = 920;
				param->swOffset[2] = 450;
				param->swGain[0] = 957;
				param->swGain[1] = 980;
				param->swGain[2] = 1008;
			}
		}
		else
		{
			if(param->PhyDpi.x <= 75)
			{
				param->swOffset[1] = 1250;
				param->swGain[1] = 950;
			}
			else if(param->PhyDpi.x <= 300)
			{
				param->swOffset[1] = 1250;
				param->swGain[1] = 950;
			}
			else if(param->PhyDpi.x <= 400)
			{
				param->swOffset[1] = 1250;
				param->swGain[1] = 950;
			}
			else
			{
				param->swOffset[1] = 1250;
				param->swGain[1] = 950;
			}
		}
		break;

	case kNEC3778:
		DBG( _DBG_INFO2, "kNEC3778 adjustments\n" );
		if((_LM9831 == hw->chip) && (param->PhyDpi.x <= 300))
		{
			param->swOffset[0] = 0;
			param->swOffset[1] = 0;
			param->swOffset[2] = 0;
			param->swGain[0] = 900;
			param->swGain[1] = 920;
			param->swGain[2] = 980;
		}
		else if( hw->motorModel == MODEL_HuaLien && param->PhyDpi.x > 800)
		{
			param->swOffset[0] = 0;
			param->swOffset[1] = 0;
			param->swOffset[2] = -200;
			param->swGain[0] = 980;
			param->swGain[1] = 930;
			param->swGain[2] = 1080;
		}
		else
		{
			param->swOffset[0] = -304;
			param->swOffset[1] = -304;
			param->swOffset[2] = -304;
			param->swGain[0] = 910;	
			param->swGain[1] = 920;	
			param->swGain[2] = 975;
		}
		
		if(param->bDataType == SCANDATATYPE_BW && param->PhyDpi.x <= 300)
		{
			param->swOffset[1] = 1000;
			param->swGain[1] = 1000;
		}
		break;
	}
}

/** according to the pixel values,
 */
static u_char usb_GetNewGain( Plustek_Device *dev, u_short wMax, int channel )
{
	double dRatio, dAmp;
	u_long dwInc, dwDec;
	u_char bGain;

	if( !wMax )
		wMax = 1;

	dAmp = 0.93 + 0.067 * dev->usbDev.a_bRegs[0x3b+channel];

	if((m_dwIdealGain / (wMax / dAmp)) < 3) {

		dRatio = ((double) m_dwIdealGain * dAmp / wMax - 0.93) / 0.067;
		if(ceil(dRatio) > 0x3f)
			return 0x3f;

		dwInc = (u_long)((0.93 + ceil (dRatio) * 0.067) * wMax / dAmp);
		dwDec = (u_long)((0.93 + floor (dRatio) * 0.067) * wMax / dAmp);
		if((dwInc >= 0xff00) ||
		   (labs (dwInc - m_dwIdealGain) > labs(dwDec - m_dwIdealGain))) {
			bGain = (u_char)floor(dRatio);
		} else {
			bGain = (u_char)ceil(dRatio);
		}

	} else {

		dRatio = m_dwIdealGain / (wMax / dAmp);
		dAmp   = floor((dRatio / 3 - 0.93)/0.067);

		if( dAmp > 31 )
			dAmp = 31;

		bGain = (u_char)dAmp + 32;
	}
	
	if( bGain > 0x3f ) {
		DBG( _DBG_INFO, "* GAIN Overflow!!!\n" );
		bGain = 0x3f;
	}
	return bGain;
}

/** limit and set register given by address
 * @param gain -
 * @param reg  -
 */
static void setAdjGain( int gain, u_char *reg )
{
	if( gain >= 0 ) {

		if( gain > 0x3f )
			*reg = 0x3f;
		else
			*reg = gain;
	}
}

/**
 * @param channel - 0 = red, 1 = green, 2 = blue
 * @param max     -
 * @param ideal   -
 * @param l_on    -
 * @param l_off   -
 * @return
 */
static SANE_Bool adjLampSetting( Plustek_Device *dev, int channel, u_long max,  
                                 u_long ideal, u_short l_on, u_short *l_off )
{
	SANE_Bool adj = SANE_FALSE;
	u_long    lamp_on;

	/* so if the image was too bright, we dim the lamps by 3% */
	if( max > ideal ) {

		lamp_on = (*l_off) - l_on;
	    lamp_on = (lamp_on * 97)/100;
		*l_off  = l_on + lamp_on;
        DBG( _DBG_INFO2,
	                "lamp(%u) adjust (-3%%): %i %i\n", channel, l_on, *l_off );
		adj = SANE_TRUE;
	}

    /* if the image was too dull, increase lamp by 1% */
	if( dev->usbDev.a_bRegs[0x3b + channel] == 63 ) {

		lamp_on  = (*l_off) - l_on;
	    lamp_on += (lamp_on/100);
		*l_off   = l_on + lamp_on;
        DBG( _DBG_INFO2,
	                "lamp(%u) adjust (+1%%): %i %i\n", channel, l_on, *l_off );
		adj = SANE_TRUE;
    }

    return adj;
}

/** usb_AdjustGain
 * function to perform the "coarse calibration step" part 1.
 * We scan reference image pixels to determine the optimum coarse gain settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * The scanned line should contain a white strip with some black at the
 * beginning. The function searches for the maximum value which corresponds to
 * the maximum white value.
 * Affects register 0x3b, 0x3c and 0x3d
 *
 */
static SANE_Bool usb_AdjustGain( Plustek_Device *dev, int fNegative )
{
	char      tmp[40];
	int       i;
	double    min_mclk;
	ScanDef  *scanning = &dev->scanning;
	DCapsDef *scaps    = &dev->usbDev.Caps;
	HWDef    *hw       = &dev->usbDev.HwSetting;
	u_long   *scanbuf  = scanning->pScanBuffer;
	u_char   *regs     = dev->usbDev.a_bRegs;
	u_long    dw, start, end, len;
	SANE_Bool fRepeatITA = SANE_TRUE;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	bMaxITA = 0xff;

	DBG( _DBG_INFO, "#########################\n" );
	DBG( _DBG_INFO, "usb_AdjustGain()\n" );
	if((dev->adj.rgain != -1) && 
	   (dev->adj.ggain != -1) && (dev->adj.bgain != -1)) {
		setAdjGain( dev->adj.rgain, &regs[0x3b] );
		setAdjGain( dev->adj.ggain, &regs[0x3c] );
		setAdjGain( dev->adj.bgain, &regs[0x3d] );
		DBG( _DBG_INFO, "- function skipped, using frontend values!\n" );
		return SANE_TRUE;
	}

	min_mclk = usb_GetMCLK( dev, &scanning->sParam );

	/* define the strip to scan for coarse calibration */
	m_ScanParam.Size.dwLines  = 1;
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
	                            scaps->OpticDpi.x / 300UL;
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
	                            2 * m_ScanParam.bChannels;

	if( usb_IsCISDevice(dev) && m_ScanParam.bDataType == SCANDATATYPE_Color)
		m_ScanParam.Size.dwBytes *= 3;

	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
                                                    300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Gain;

	start = 0;
	len   = m_ScanParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_Transparency ) {
		start = scaps->Positive.DataOrigin.x * scaps->OpticDpi.x / 300UL;
		len   = scaps->Positive.Size.x * scaps->OpticDpi.x / 300UL;
	}
	else if( scanning->sParam.bSource == SOURCE_Negative ) {
		start = scaps->Negative.DataOrigin.x * scaps->OpticDpi.x / 300UL;
		len   = scaps->Negative.Size.x * scaps->OpticDpi.x / 300UL;
	}
	end = start + len;

	start = ((u_long)dev->usbDev.pSource->DataOrigin.x*scaps->OpticDpi.x/300UL);
	len   = ((u_long)dev->usbDev.pSource->Size.x * scaps->OpticDpi.x / 300UL);

	DBG( _DBG_INFO2, "Coarse Calibration Strip:\n" );
	DBG( _DBG_INFO2, "Lines    = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels   = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes    = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X = %u\n",  m_ScanParam.Origin.x );
	DBG( _DBG_INFO2, "Start    = %lu\n", start );
	DBG( _DBG_INFO2, "Len      = %lu\n", len   );
	DBG( _DBG_INFO2, "End      = %lu\n", end   );
	DBG( _DBG_INFO,  "MIN MCLK = %.2f\n", min_mclk );

	i = 0;
TOGAIN:
	m_ScanParam.dMCLK = dMCLK;

	if( !usb_SetScanParameters( dev, &m_ScanParam ))
		return SANE_FALSE;

	if( !usb_ScanBegin( dev, SANE_FALSE) ||
		!usb_ScanReadImage( dev, scanbuf, m_ScanParam.Size.dwPhyBytes ) ||
		!usb_ScanEnd( dev )) {
		DBG( _DBG_ERROR, "usb_AdjustGain() failed\n" );
		return SANE_FALSE;
	}

	DBG( _DBG_INFO2, "PhyBytes  = %lu\n",  m_ScanParam.Size.dwPhyBytes  );
	DBG( _DBG_INFO2, "PhyPixels = %lu\n",  m_ScanParam.Size.dwPhyPixels );

	if( end > m_ScanParam.Size.dwPhyPixels )
		end = m_ScanParam.Size.dwPhyPixels;

	sprintf( tmp, "coarse-gain-%u.raw", i++ );

	dumpPicInit(&m_ScanParam, tmp);
	dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwPhyBytes, 0);
		
#ifdef SWAP_COARSE
	if(usb_HostSwap())
#endif
		usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwPhyBytes );

	if( fNegative ) {

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			RGBULongDef rgb, rgbSum;
			u_long      dwLoop = len / 20 * 20;
			u_long      dw10, dwGray, dwGrayMax;

			rgb.Red = rgb.Green = rgb.Blue = dwGrayMax = 0;

			for( dw = start; dwLoop; dwLoop-- ) {

				rgbSum.Red = rgbSum.Green = rgbSum.Blue = 0;
				for( dw10 = 20; dw10--; dw++ ) {
					rgbSum.Red   += (u_long)(((RGBULongDef*)scanbuf)[dw].Red);
					rgbSum.Green += (u_long)(((RGBULongDef*)scanbuf)[dw].Green);
					rgbSum.Blue  += (u_long)(((RGBULongDef*)scanbuf)[dw].Blue);
				}

				/* do some weighting of the color planes for negatives */
				dwGray = (rgbSum.Red * 30UL + rgbSum.Green * 59UL + rgbSum.Blue * 11UL) / 100UL;

				if( fNegative == 1 || rgbSum.Red > rgbSum.Green) {
					if( dwGray > dwGrayMax ) {
						dwGrayMax = dwGray;
						rgb.Red   = rgbSum.Red;
						rgb.Green = rgbSum.Green;
						rgb.Blue  = rgbSum.Blue;
					}
				}
			}

			Gain_Hilight.Red   = (u_short)(rgb.Red / 20UL);
			Gain_Hilight.Green = (u_short)(rgb.Green / 20UL);
			Gain_Hilight.Blue  = (u_short)(rgb.Blue / 20UL);
			DBG(_DBG_INFO2, "MAX(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
			    Gain_Hilight.Red, Gain_Hilight.Red, Gain_Hilight.Green, 
			    Gain_Hilight.Green, Gain_Hilight.Blue, Gain_Hilight.Blue );

			regs[0x3b] = usb_GetNewGain(dev,Gain_Hilight.Red,   0 );
			regs[0x3c] = usb_GetNewGain(dev,Gain_Hilight.Green, 1 );
			regs[0x3d] = usb_GetNewGain(dev,Gain_Hilight.Blue,  2 );

		} else {

			u_long dwMax  = 0, dwSum;
			u_long dwLoop = len / 20 * 20;
			u_long dw10;

			for( dw = start; dwLoop; dwLoop-- ) {

				dwSum = 0;
				for( dw10 = 20; dw10--; dw++ )
					dwSum += (u_long)((u_short*)scanbuf)[dw];

				if((fNegative == 1) || (dwSum < 0x6000 * 20)) {
					if( dwMax < dwSum )
						dwMax = dwSum;
				}
			}
			Gain_Hilight.Red  = Gain_Hilight.Green =
			Gain_Hilight.Blue = (u_short)(dwMax / 20UL);

			Gain_Reg.Red  = Gain_Reg.Green =
			Gain_Reg.Blue = regs[0x3b] =
			regs[0x3c] = regs[0x3d] = usb_GetNewGain(dev,Gain_Hilight.Green,1);
		}
	} else {
	
		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			RGBUShortDef max_rgb, min_rgb, tmp_rgb;
			u_long       dwR, dwG, dwB;
			u_long       dwDiv   = 10;
			u_long       dwLoop1 = len / dwDiv, dwLoop2;

			max_rgb.Red = max_rgb.Green = max_rgb.Blue = 0;
			min_rgb.Red = min_rgb.Green = min_rgb.Blue = 0xffff;

			/* find out the max pixel value for R, G, B */
			for( dw = start; dwLoop1; dwLoop1-- ) {

				/* do some averaging... */
				for (dwLoop2 = dwDiv, dwR = dwG = dwB = 0;
				                                    dwLoop2; dwLoop2--, dw++) {
					if( usb_IsCISDevice(dev)) {
						dwR += ((u_short*)scanbuf)[dw];
						dwG += ((u_short*)scanbuf)[dw+m_ScanParam.Size.dwPhyPixels+1];
						dwB += ((u_short*)scanbuf)[dw+(m_ScanParam.Size.dwPhyPixels+1)*2];
            		} else {
						dwR += ((RGBUShortDef*)scanbuf)[dw].Red;
						dwG += ((RGBUShortDef*)scanbuf)[dw].Green;
						dwB += ((RGBUShortDef*)scanbuf)[dw].Blue;
					}
				}
				dwR = dwR / dwDiv;
				dwG = dwG / dwDiv;
				dwB = dwB / dwDiv;

				if(max_rgb.Red < dwR)
					max_rgb.Red = dwR;
				if(max_rgb.Green < dwG)
					max_rgb.Green = dwG;
				if(max_rgb.Blue < dwB)
					max_rgb.Blue = dwB;

				if(min_rgb.Red > dwR)
					min_rgb.Red = dwR;
				if(min_rgb.Green > dwG)
					min_rgb.Green = dwG;
				if(min_rgb.Blue > dwB)
					min_rgb.Blue = dwB;
			}

			DBG(_DBG_INFO2, "MAX(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
				  	max_rgb.Red, max_rgb.Red, max_rgb.Green,
					max_rgb.Green, max_rgb.Blue, max_rgb.Blue );
			DBG(_DBG_INFO2, "MIN(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
				  	min_rgb.Red, min_rgb.Red, min_rgb.Green,
					min_rgb.Green, min_rgb.Blue, min_rgb.Blue );

			/* on CIS scanner, we use the min value, on CCD the max value
			 * for adjusting the gain
			 */
			tmp_rgb = max_rgb;
			if( usb_IsCISDevice(dev))
				tmp_rgb = min_rgb;

			DBG(_DBG_INFO2, "CUR(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
				  	tmp_rgb.Red, tmp_rgb.Red, tmp_rgb.Green,
				    tmp_rgb.Green, tmp_rgb.Blue, tmp_rgb.Blue);

/*			m_dwIdealGain = IDEAL_GainNormal;
*/						 /* min(min(rgb.wRed, rgb.wGreen), rgb.wBlue) */

			regs[0x3b] = usb_GetNewGain( dev, tmp_rgb.Red,   0 );
			regs[0x3c] = usb_GetNewGain( dev, tmp_rgb.Green, 1 );
			regs[0x3d] = usb_GetNewGain( dev, tmp_rgb.Blue,  2 );

			if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {

				SANE_Bool adj = SANE_FALSE;

				/* on CIS devices, we can control the lamp off settings */
				if( usb_IsCISDevice(dev)) {

/*					m_dwIdealGain = IDEAL_GainNormal;
 */
					if( adjLampSetting( dev, CHANNEL_red, tmp_rgb.Red, m_dwIdealGain,
					                    hw->red_lamp_on, &hw->red_lamp_off )) {
						adj = SANE_TRUE;
					}

					if( adjLampSetting( dev, CHANNEL_green, tmp_rgb.Green, m_dwIdealGain,
								    hw->green_lamp_on, &hw->green_lamp_off )) {
						adj = SANE_TRUE;
					}

					if( adjLampSetting( dev, CHANNEL_blue, tmp_rgb.Blue, m_dwIdealGain,
									    hw->blue_lamp_on, &hw->blue_lamp_off)){
						adj = SANE_TRUE;
					}

					/* on any adjustment, set the registers... */
					if( adj ) {
						usb_AdjustLamps( dev, SANE_TRUE );

						if( i < _MAX_GAIN_LOOPS )
							goto TOGAIN;
					}

				} else {

					if((!regs[0x3b] ||
					    !regs[0x3c] || !regs[0x3d]) && dMCLK > min_mclk) {

						scanning->sParam.dMCLK = dMCLK = dMCLK - 0.5;
						regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;
						
						adj = SANE_TRUE;

					} else if(((regs[0x3b] == 63) || (regs[0x3c] == 63) ||
									  (regs[0x3d] == 63)) && (dMCLK < 10)) {

						scanning->sParam.dMCLK = dMCLK = dMCLK + 0.5;
						regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

						adj = SANE_TRUE;
					}

					if( adj ) {
						if( i < _MAX_GAIN_LOOPS )
							goto TOGAIN;
					}
				}
				
			} else {

				/* for MODEL KaoHsiung 1200 scanner multi-straight-line bug at
				 * 1200 dpi color mode
				 */
				if( hw->motorModel == MODEL_KaoHsiung &&
				    scaps->bCCD == kNEC3778 && dMCLK>= 5.5 && !regs[0x3c]){

					regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;
					scanning->sParam.dMCLK = dMCLK = dMCLK - 1.5;
					goto TOGAIN;

				} else if( hw->motorModel == MODEL_HuaLien &&
					       scaps->bCCD == kNEC3799 && fRepeatITA ) {

					if((!regs[0x3b] ||
					    !regs[0x3c] || !regs[0x3d]) && dMCLK > 3.0) {

						scanning->sParam.dMCLK = dMCLK = dMCLK - 0.5;
						regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;
						goto TOGAIN;

					} else if(((regs[0x3b] == 63) || (regs[0x3c] == 63) ||
										  (regs[0x3d] == 63)) && (dMCLK < 10)) {

						scanning->sParam.dMCLK = dMCLK = dMCLK + 0.5;
						regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;
						goto TOGAIN;
					}
					bMaxITA = (u_char)floor((dMCLK + 1) / 2);
					fRepeatITA = SANE_FALSE;
				}
			}

		} else {

			u_short	w_max = 0, w_min = 0xffff, w_tmp;

			for( dw = start; dw < end; dw++ ) {
				if( w_max < ((u_short*)scanbuf)[dw])
					w_max = ((u_short*)scanbuf)[dw];
				if( w_min > ((u_short*)scanbuf)[dw])
					w_min = ((u_short*)scanbuf)[dw];
			}

			w_tmp = w_max;
			if( usb_IsCISDevice(dev))
				w_tmp = w_min;

			regs[0x3b] =
			regs[0x3c] =
			regs[0x3d] = usb_GetNewGain(dev, w_tmp, 0);

			DBG(_DBG_INFO2, "MAX(G)= 0x%04x(%u)\n", w_max, w_max );
			DBG(_DBG_INFO2, "MIN(G)= 0x%04x(%u)\n", w_min, w_min );
			DBG(_DBG_INFO2, "CUR(G)= 0x%04x(%u)\n", w_tmp, w_tmp );

/*			m_dwIdealGain = IDEAL_GainNormal;
 */
			if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {

				SANE_Bool adj = SANE_FALSE;

				/* on CIS devices, we can control the lamp off settings */
				if( usb_IsCISDevice(dev)) {

					if( adjLampSetting( dev, CHANNEL_green, w_tmp, m_dwIdealGain,
					                    hw->green_lamp_on, &hw->green_lamp_off )) {
						adj = SANE_TRUE;
					}

					/* on any adjustment, set the registers... */
					if( adj ) {
						usb_AdjustLamps( dev, SANE_TRUE );

						if( i < _MAX_GAIN_LOOPS )
							goto TOGAIN;
					}

				} else {

					if( !regs[0x3b] && (dMCLK > min_mclk)) {

						scanning->sParam.dMCLK = dMCLK = dMCLK - 0.5;
						regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

						adj = SANE_TRUE;

					} else if((regs[0x3b] == 63) && (dMCLK < 20)) {

						scanning->sParam.dMCLK = dMCLK = dMCLK + 0.5;
						regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

						adj = SANE_TRUE;
					}

					if( adj ) {
						if( i < _MAX_GAIN_LOOPS )
							goto TOGAIN;
					}
				}
			}
		}
	}

	DBG( _DBG_INFO2, "REG[0x3b] = %u\n", regs[0x3b] );
	DBG( _DBG_INFO2, "REG[0x3c] = %u\n", regs[0x3c] );
	DBG( _DBG_INFO2, "REG[0x3d] = %u\n", regs[0x3d] );

	DBG( _DBG_INFO2, "red_lamp_on    = %u\n", hw->red_lamp_on  );
	DBG( _DBG_INFO2, "red_lamp_off   = %u\n", hw->red_lamp_off );
	DBG( _DBG_INFO2, "green_lamp_on  = %u\n", hw->green_lamp_on  );
	DBG( _DBG_INFO2, "green_lamp_off = %u\n", hw->green_lamp_off );
	DBG( _DBG_INFO2, "blue_lamp_on   = %u\n", hw->blue_lamp_on   );
	DBG( _DBG_INFO2, "blue_lamp_off  = %u\n", hw->blue_lamp_off  );

	DBG( _DBG_INFO, "usb_AdjustGain() done.\n" );
	return SANE_TRUE;
}

/** usb_GetNewOffset
 * @param pdwSum   -
 * @param pdwDiff  -
 * @param pcOffset -
 * @param pIdeal   -
 * @param channel  -
 * @param cAdjust  -
 */
static void usb_GetNewOffset( Plustek_Device *dev, u_long *pdwSum, u_long *pdwDiff,
                              signed char *pcOffset, u_char *pIdeal,
                              u_long channel, signed char cAdjust )
{
	/* IDEAL_Offset is currently set to 0x1000 = 4096 */
	u_long dwIdealOffset = IDEAL_Offset;

	if( pdwSum[channel] > dwIdealOffset ) {

		/* Over ideal value */
		pdwSum[channel] -= dwIdealOffset;
		if( pdwSum[channel] < pdwDiff[channel] ) {
			/* New offset is better than old one */
			pdwDiff[channel] = pdwSum[channel];
			pIdeal[channel]  = dev->usbDev.a_bRegs[0x38 + channel];
		}
		pcOffset[channel] -= cAdjust;

	} else 	{

		/* Below than ideal value */
		pdwSum[channel] = dwIdealOffset - pdwSum [channel];
		if( pdwSum[channel] < pdwDiff[channel] ) {
			/* New offset is better than old one */
			pdwDiff[channel] = pdwSum[channel];
			pIdeal[channel]  = dev->usbDev.a_bRegs[0x38 + channel];
		}
		pcOffset[channel] += cAdjust;
	}

	if( pcOffset[channel] >= 0 )
		dev->usbDev.a_bRegs[0x38 + channel] = pcOffset[channel];
	else
		dev->usbDev.a_bRegs[0x38 + channel] = (u_char)(32 - pcOffset[channel]);
}

/** usb_AdjustOffset
 * function to perform the "coarse calibration step" part 2.
 * We scan reference image pixels to determine the optimum coarse offset settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * On CIS based devices, we switch the light off, on CCD devices, we use the optical
 * black pixels.
 * Affects register 0x38, 0x39 and 0x3a
 */
static SANE_Bool usb_AdjustOffset( Plustek_Device *dev )
{
	char          tmp[40];
	signed char   cAdjust = 16;
	signed char   cOffset[3];
	u_char        bExpect[3];
	int           i;
	u_long        dw, dwPixels;
    u_long        dwDiff[3], dwSum[3];

	HWDef  *hw      = &dev->usbDev.HwSetting;
	u_char *regs    = dev->usbDev.a_bRegs;
	u_long *scanbuf = dev->scanning.pScanBuffer;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	DBG( _DBG_INFO, "#########################\n" );
	DBG( _DBG_INFO, "usb_AdjustOffset()\n" );
	if((dev->adj.rofs != -1) && 
	   (dev->adj.gofs != -1) && (dev->adj.bofs != -1)) {
		regs[0x38] = (dev->adj.rofs & 0x3f);
		regs[0x39] = (dev->adj.gofs & 0x3f);
		regs[0x3a] = (dev->adj.bofs & 0x3f);
		DBG( _DBG_INFO, "- function skipped, using frontend values!\n" );
		return SANE_TRUE;
	}

	m_ScanParam.Size.dwLines  = 1;
	m_ScanParam.Size.dwPixels = 2550;

	if( usb_IsCISDevice(dev))
		dwPixels = m_ScanParam.Size.dwPixels;
	else
		dwPixels = (u_long)(hw->bOpticBlackEnd - hw->bOpticBlackStart );

	m_ScanParam.Size.dwPixels = 2550;
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2 *
	                                                     m_ScanParam.bChannels;
	if( usb_IsCISDevice(dev) && m_ScanParam.bDataType == SCANDATATYPE_Color )
		m_ScanParam.Size.dwBytes *= 3;

	m_ScanParam.Origin.x = (u_short)((u_long)hw->bOpticBlackStart * 300UL /
												  dev->usbDev.Caps.OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Offset;
 	m_ScanParam.dMCLK        = dMCLK;

	dwDiff[0]  = dwDiff[1]  = dwDiff[2]  = 0xffff;
	cOffset[0] = cOffset[1] = cOffset[2] = 0;
	bExpect[0] = bExpect[1] = bExpect[2] = 0;

	regs[0x38] = regs[0x39] = regs[0x3a] = 0;

	if( usb_IsCISDevice(dev)) {
	    /*
		 * if we have dark shading strip, there's no need to switch
	     * the lamp off
		 */
		if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

			usb_ModuleToHome( dev, SANE_TRUE );
			usb_ModuleMove  ( dev, MOVE_Forward,
								(u_long)dev->usbDev.pSource->DarkShadOrgY );

			regs[0x45] &= ~0x10;

		} else {

		 	/* switch lamp off to read dark data... */
			regs[0x29] = 0;
			usb_switchLamp( dev, SANE_FALSE );
		}
	}

	if( 0 == dwPixels ) {
		DBG( _DBG_ERROR, "OpticBlackEnd = OpticBlackStart!!!\n" );
		return SANE_FALSE;
	}

	if( !usb_SetScanParameters( dev, &m_ScanParam )) {
		DBG( _DBG_ERROR, "usb_AdjustOffset() failed\n" );
		return SANE_FALSE;
	}
		
	i = 0;

	DBG( _DBG_INFO2, "S.dwPixels  = %lu\n", m_ScanParam.Size.dwPixels );		
	DBG( _DBG_INFO2, "dwPixels    = %lu\n", dwPixels );
	DBG( _DBG_INFO2, "dwPhyBytes  = %lu\n", m_ScanParam.Size.dwPhyBytes );
	DBG( _DBG_INFO2, "dwPhyPixels = %lu\n", m_ScanParam.Size.dwPhyPixels );

	while( cAdjust ) {

		/*
		 * read data (a white calibration strip - hopefully ;-)
		 */
		if((!usb_ScanBegin(dev, SANE_FALSE)) ||
		   (!usb_ScanReadImage(dev,scanbuf,m_ScanParam.Size.dwPhyBytes)) ||
			!usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "usb_AdjustOffset() failed\n" );
			return SANE_FALSE;
		}

		sprintf( tmp, "coarse-off-%u.raw", i++ );
		
#ifdef SWAP_COARSE
		if(usb_HostSwap())
			usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwPhyBytes );
#endif
		dumpPicInit(&m_ScanParam, tmp);
		dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwPhyBytes, 0);

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			dwSum[0] = dwSum[1] = dwSum[2] = 0;

			for (dw = 0; dw < dwPixels; dw++) {
#ifndef SWAP_COARSE
				dwSum[0] += (u_long)_HILO2WORD(((ColorWordDef*)scanbuf)[dw].HiLo[0]);
				dwSum[1] += (u_long)_HILO2WORD(((ColorWordDef*)scanbuf)[dw].HiLo[1]);
				dwSum[2] += (u_long)_HILO2WORD(((ColorWordDef*)scanbuf)[dw].HiLo[2]);
#else
				dwSum[0] += ((RGBUShortDef*)scanbuf)[dw].Red;
				dwSum[1] += ((RGBUShortDef*)scanbuf)[dw].Green;
				dwSum[2] += ((RGBUShortDef*)scanbuf)[dw].Blue;
#endif
			}

            DBG( _DBG_INFO2, "RedSum   = %lu, ave = %lu\n",
												dwSum[0], dwSum[0] /dwPixels );
            DBG( _DBG_INFO2, "GreenSum = %lu, ave = %lu\n",
												dwSum[1], dwSum[1] /dwPixels );
            DBG( _DBG_INFO2, "BlueSum  = %lu, ave = %lu\n",
												dwSum[2], dwSum[2] /dwPixels );
			
			/* do averaging for each channel */
			dwSum[0] /= dwPixels;
			dwSum[1] /= dwPixels;
			dwSum[2] /= dwPixels;

			usb_GetNewOffset( dev, dwSum, dwDiff, cOffset, bExpect, 0, cAdjust );
			usb_GetNewOffset( dev, dwSum, dwDiff, cOffset, bExpect, 1, cAdjust );
			usb_GetNewOffset( dev, dwSum, dwDiff, cOffset, bExpect, 2, cAdjust );

            DBG( _DBG_INFO2, "RedExpect   = %u\n", bExpect[0] );
            DBG( _DBG_INFO2, "GreenExpect = %u\n", bExpect[1] );
            DBG( _DBG_INFO2, "BlueExpect  = %u\n", bExpect[2] );

		} else {
			dwSum[0] = 0;

			for( dw = 0; dw < dwPixels; dw++ ) {
#ifndef SWAP_COARSE
				dwSum[0] += (u_long)_HILO2WORD(((HiLoDef*)scanbuf)[dw]);
#else
				dwSum[0] += ((u_short*)scanbuf)[dw];
#endif
			}
			dwSum [0] /= dwPixels;
			usb_GetNewOffset( dev, dwSum, dwDiff, cOffset, bExpect, 0, cAdjust );
			regs[0x3a] = regs[0x39] = regs[0x38];

			DBG(_DBG_INFO2,"Sum = %lu, ave = %lu\n",dwSum[0],dwSum[0]/dwPixels);
			DBG(_DBG_INFO2,"Expect = %u\n", bExpect[0]);
		}

		_UIO(sanei_lm983x_write(dev->fd, 0x38, &regs[0x38], 3, SANE_TRUE));
		cAdjust >>= 1;
	}

	if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		regs[0x38] = bExpect[0];
		regs[0x39] = bExpect[1];
		regs[0x3a] = bExpect[2];
	} else {

		regs[0x38] = regs[0x39] = regs[0x3a] = bExpect[0];	
	}

	DBG( _DBG_INFO2, "REG[0x38] = %u\n", regs[0x38] );
	DBG( _DBG_INFO2, "REG[0x39] = %u\n", regs[0x39] );
	DBG( _DBG_INFO2, "REG[0x3a] = %u\n", regs[0x3a] );
	DBG( _DBG_INFO, "usb_AdjustOffset() done.\n" );

	/* switch it on again on CIS based scanners */
	if( usb_IsCISDevice(dev)) {

		if( dev->usbDev.pSource->DarkShadOrgY < 0 ) {
			regs[0x29] = hw->bReg_0x29;
			usb_switchLamp( dev, SANE_TRUE );
			usbio_WriteReg( dev->fd, 0x29, regs[0x29]);
		}
	}

	return SANE_TRUE;
}

/** this function tries to find out some suitable values for the dark
 *  fine calibration. If the device owns a black calibration strip
 *  the data is simply copied. If not, then the white strip is read 
 *  with the lamp switched off...
 */
static void usb_GetDarkShading( Plustek_Device *dev, u_short *pwDest,
                                HiLoDef *pSrce, u_long dwPixels,
                                u_long dwAdd, int iOffset )
{
	u_long    dw;
	u_long    dwSum[2];
	DCapsDef *scaps = &dev->usbDev.Caps;
	HWDef    *hw    = &dev->usbDev.HwSetting;

	if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

		u_short w;
		int     wtmp;

		/* here we use the source  buffer + a static offset */
		for (dw = 0; dw < dwPixels; dw++, pSrce += dwAdd)
		{
#ifndef SWAP_FINE
			wtmp = ((int)_PHILO2WORD(pSrce) + iOffset);
#else
			wtmp = ((int)_PLOHI2WORD(pSrce) + iOffset);
#endif
			if( wtmp < 0 )
				wtmp = 0;

			if( wtmp > 0xffff )
				wtmp = 0xffff;

			w = (u_short)wtmp;

#ifndef SWAP_FINE
			pwDest[dw] = _LOBYTE(w) * 256 + _HIBYTE(w);
#else
			pwDest[dw] = w;
#endif
		}
	}
	else
	{
		dwSum[0] = dwSum[1] = 0;
		if( hw->bSensorConfiguration & 0x04 ) {

			/* Even/Odd CCD */
			for( dw = 0; dw < dwPixels; dw++, pSrce += dwAdd ) {
#ifndef SWAP_FINE
				dwSum[dw & 1] += (u_long)_PHILO2WORD(pSrce);
#else
				dwSum[dw & 1] += (u_long)_PLOHI2WORD(pSrce);
#endif
			}
			dwSum[0] /= ((dwPixels + 1UL) >> 1);
			dwSum[1] /= (dwPixels >> 1);

			if( /*Registry.GetEvenOdd() == 1 ||*/ scaps->bPCB == 2)
			{
				dwSum[0] = dwSum[1] = (dwSum[0] + dwSum[1]) / 2;
			}

			dwSum[0] = (int)dwSum[0] + iOffset;
			dwSum[1] = (int)dwSum[1] + iOffset;

			if((int)dwSum[0] < 0)
				dwSum[0] = 0;

			if((int)dwSum[1] < 0)
				dwSum[1] = 0;
#ifndef SWAP_FINE
			dwSum[0] = (u_long)_LOBYTE(_LOWORD(dwSum[0])) * 256UL +
			                             _HIBYTE(_LOWORD(dwSum[0]));
			dwSum[1] = (u_long)_LOBYTE(_LOWORD(dwSum[1])) * 256UL +
			                             _HIBYTE(_LOWORD(dwSum[1]));
#else
			dwSum[0] = (u_long)_LOWORD(dwSum[0]);
			dwSum[1] = (u_long)_LOWORD(dwSum[1]);
#endif

			for( dw = 0; dw < dwPixels; dw++ )
				pwDest[dw] = (u_short)dwSum[dw & 1];
		} else {
			
			/* Standard CCD */

			/* do some averaging on the line */
			for( dw = 0; dw < dwPixels; dw++, pSrce += dwAdd ) {
#ifndef SWAP_FINE
				dwSum[0] += (u_long)_PHILO2WORD(pSrce);
#else
				dwSum[0] += (u_long)_PLOHI2WORD(pSrce);
#endif
			}

			dwSum[0] /= dwPixels;

			/* add our offset... */
			dwSum[0] = (int)dwSum[0] + iOffset;
			if((int)dwSum[0] < 0)
				dwSum[0] = 0;
#ifndef SWAP_FINE
			dwSum[0] = (u_long)_LOBYTE(_LOWORD(dwSum[0])) * 256UL +
			                             _HIBYTE(_LOWORD(dwSum[0]));
#else
			dwSum[0] = (u_long)_LOWORD(dwSum[0]);
#endif

			/* fill the shading data */
			for( dw = 0; dw < dwPixels; dw++ )
				pwDest[dw] = (u_short)dwSum[0];
		}
	}
#ifdef SWAP_FINE
	if(usb_HostSwap())
		usb_Swap( pwDest, dwPixels *2 );
#endif
}

/** usb_AdjustDarkShading
 * fine calibration part 1 - read the black calibration area and write
 * the black line data to the offset coefficient data in Merlins' DRAM
 * If there's no black line available, we can use the min pixel value
 * from coarse calibration...
 */
static SANE_Bool usb_AdjustDarkShading( Plustek_Device *dev )
{
	char      tmp[40];
	ScanDef  *scanning = &dev->scanning;
	DCapsDef *scaps    = &dev->usbDev.Caps;
	HWDef    *hw       = &dev->usbDev.HwSetting;
	u_long   *scanbuf  = scanning->pScanBuffer;
	u_char   *regs     = dev->usbDev.a_bRegs;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	if( scaps->workaroundFlag & _WAF_SKIP_FINE )
		return SANE_TRUE;

	DBG( _DBG_INFO, "#########################\n" );
	DBG( _DBG_INFO, "usb_AdjustDarkShading()\n" );
	DBG( _DBG_INFO2, "* MCLK = %f (scanparam-MCLK=%f)\n",
	                    dMCLK, scanning->sParam.dMCLK );

	usb_PrepareFineCal( dev, &m_ScanParam, 0 );

	m_ScanParam.Size.dwLines = 1;				/* for gain */
	m_ScanParam.bCalibration = PARAM_DarkShading;

	if( _LM9831 == hw->chip ) {

		m_ScanParam.UserDpi.x = usb_SetAsicDpiX( dev, m_ScanParam.UserDpi.x);
		if( m_ScanParam.UserDpi.x < 100)
			m_ScanParam.UserDpi.x = 150;

		/* Now DPI X is physical */
		m_ScanParam.Origin.x      = m_ScanParam.Origin.x %
		                            (u_short)m_dHDPIDivider;
		m_ScanParam.Size.dwPixels = (u_long)scaps->Normal.Size.x *
		                                    m_ScanParam.UserDpi.x / 300UL;
		m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
		                            2UL * m_ScanParam.bChannels;
		m_dwPixels = scanning->sParam.Size.dwPixels *
		             m_ScanParam.UserDpi.x / scanning->sParam.UserDpi.x;

		if( usb_IsCISDevice(dev) && m_ScanParam.bDataType == SCANDATATYPE_Color )
			m_ScanParam.Size.dwBytes *= 3;
    }

	/* if we have dark shading strip, there's no need to switch
	 * the lamp off
	 */
	if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

		usb_ModuleToHome( dev, SANE_TRUE );
		usb_ModuleMove  ( dev, MOVE_Forward,
		                  (u_long)dev->usbDev.pSource->DarkShadOrgY );
	} else {

	 	/* switch lamp off to read dark data... */
		regs[0x29] = 0;
		usb_switchLamp( dev, SANE_FALSE );
	}

	usb_SetScanParameters( dev, &m_ScanParam );

	if((!usb_ScanBegin(dev, SANE_FALSE)) ||
	   (!usb_ScanReadImage(dev,scanbuf, m_ScanParam.Size.dwPhyBytes)) ||
	   (!usb_ScanEnd( dev ))) {

		/* on error, reset the lamp settings*/
		regs[0x29] = hw->bReg_0x29;
 		usb_switchLamp( dev, SANE_TRUE );
		usbio_WriteReg( dev->fd, 0x29, regs[0x29] );

		DBG( _DBG_ERROR, "usb_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}
	
	/* set illumination mode and switch lamp on again
	 */
	regs[0x29] = hw->bReg_0x29;
	usb_switchLamp( dev, SANE_TRUE );

	if( !usbio_WriteReg( dev->fd, 0x29, regs[0x29])) {
		DBG( _DBG_ERROR, "usb_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}

#ifdef SWAP_FINE
	if(usb_HostSwap())
		usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwPhyBytes );
#endif

	sprintf( tmp, "fine-black.raw" );

	dumpPicInit(&m_ScanParam, tmp);
	dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwPhyBytes, 0);

	usleep(500 * 1000);    /* Warm up lamp again */

	if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

		if( usb_IsCISDevice(dev)) {

			usb_GetDarkShading( dev, a_wDarkShading, (HiLoDef*)scanbuf,
			                    m_ScanParam.Size.dwPhyPixels, 1,
			                    scanning->sParam.swOffset[0]);

			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
			                    (HiLoDef*)scanbuf + m_ScanParam.Size.dwPhyPixels,
			                     m_ScanParam.Size.dwPhyPixels, 1, scanning->sParam.swOffset[1]);

			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
			               (HiLoDef*)scanbuf + m_ScanParam.Size.dwPhyPixels * 2,
			               m_ScanParam.Size.dwPhyPixels, 1, scanning->sParam.swOffset[2]);

		} else {

			usb_GetDarkShading( dev, a_wDarkShading, (HiLoDef*)scanbuf,
			                    m_ScanParam.Size.dwPhyPixels, 3,
			                    scanning->sParam.swOffset[0]);
			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
			                (HiLoDef*)scanbuf + 1, m_ScanParam.Size.dwPhyPixels,
			                3, scanning->sParam.swOffset[1]);
			usb_GetDarkShading( dev, a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
			               (HiLoDef*)scanbuf + 2, m_ScanParam.Size.dwPhyPixels,
			               3, scanning->sParam.swOffset[2]);
		}
	} else {

		usb_GetDarkShading( dev, a_wDarkShading, (HiLoDef*)scanbuf,
		                    m_ScanParam.Size.dwPhyPixels, 1,
		                    scanning->sParam.swOffset[1]);

		memcpy( a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
		        a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2 );
		memcpy( a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
		        a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2 );
	}

	regs[0x45] |= 0x10;

	usb_line_statistics( "Dark", a_wDarkShading, m_ScanParam.Size.dwPhyPixels,
                          scanning->sParam.bDataType == SCANDATATYPE_Color?1:0);
	return SANE_TRUE;
}

/** function to remove the brightest values out of each row
 * @param dev           - the almighty device structure.
 * @param sp            - is a pointer to the scanparam structure used for
 *                        scanning the shading lines.
 * @param hilight       - defines the number of values to skip.
 * @param shading_lines - defines the overall number of shading lines.
 */
static void usb_CalSortHighlight( Plustek_Device *dev, ScanParam *sp, 
                                  u_long hilight, u_long shading_lines )
{
    ScanDef      *scan = &dev->scanning;
	u_short       r, g, b;
	u_long        lines, w, x;
	RGBUShortDef *pw, *rgb;

	if( hilight == 0 )
		return;

	rgb = (RGBUShortDef*)scan->pScanBuffer;

	/* do it for all relevant lines */
	for( lines = hilight,  rgb = rgb + sp->Size.dwPhyPixels * lines;
	     lines < shading_lines; lines++, rgb += sp->Size.dwPhyPixels ) {

		/* scan the complete line */
		for( x = 0; x < sp->Size.dwPhyPixels; x++ ) {

			/* reference is the first scanline */
			pw = (RGBUShortDef*)scan->pScanBuffer;
			r = rgb[x].Red;
			g = rgb[x].Green;
			b = rgb[x].Blue;

			for( w = 0; w < hilight; w++, pw += sp->Size.dwPhyPixels ) {

				if( r > pw[x].Red )
					_SWAP( r, pw[x].Red );

				if( g > pw[x].Green )
					_SWAP( g, pw[x].Green );

				if( b > pw[x].Blue )
					_SWAP( b, pw[x].Blue );
			}
			rgb[x].Red   = r;
			rgb[x].Green = g;
			rgb[x].Blue  = b;
		}
	}
}

/** function to remove the brightest values out of each row
 * @param dev           - the almighty device structure.
 * @param sp            - is a pointer to the scanparam structure used for 
 *                        scanning the shading lines.
 * @param hilight       - defines the number of values to skip.
 * @param shading_lines - defines the overall number of shading lines.
 */
static void usb_CalSortShadow( Plustek_Device *dev, ScanParam *sp,
                               u_long hilight, u_long shadow, u_long shading_lines )
{
	ScanDef      *scan = &dev->scanning;
	u_short       r, g, b;
	u_long        lines, w, x;
	RGBUShortDef *pw, *rgb;

	if( shadow == 0 )
		return;

	rgb = (RGBUShortDef*)scan->pScanBuffer;

	for( lines = hilight,  rgb = rgb + sp->Size.dwPhyPixels * lines;
	     lines < shading_lines-shadow; lines++, rgb += sp->Size.dwPhyPixels ) {

		for (x = 0; x < sp->Size.dwPhyPixels; x++) {

			pw = ((RGBUShortDef*)scan->pScanBuffer) + (shading_lines - shadow) *
			                                           sp->Size.dwPhyPixels;
			r = rgb[x].Red;
			g = rgb[x].Green;
			b = rgb[x].Blue;

			for( w = 0; w < shadow; w++, pw += sp->Size.dwPhyPixels ) {
				if( r < pw[x].Red )
					_SWAP( r, pw[x].Red );
				if( g < pw[x].Green )
					_SWAP( g, pw [x].Green );
				if( b > pw[x].Blue )
					_SWAP( b, pw[x].Blue );
			}
			rgb[x].Red   = r;
			rgb[x].Green = g;
			rgb[x].Blue  = b;
		}
	}
}

static void usb_procHighlightAndShadow( Plustek_Device *dev, ScanParam *sp,
                                        u_long hilight, u_long shadow, u_long shading_lines )
{
	ScanDef      *scan = &dev->scanning;
	u_long        lines, x;
	u_long       *pr, *pg, *pb;
	RGBUShortDef *rgb;

	pr = (u_long*)((u_char*)scan->pScanBuffer + sp->Size.dwPhyBytes * shading_lines);
	pg = pr + sp->Size.dwPhyPixels;
	pb = pg + sp->Size.dwPhyPixels;

	memset(pr, 0, sp->Size.dwPhyPixels * 4UL * 3UL);

	/* Sort hilight */
	usb_CalSortHighlight(dev, sp, hilight, shading_lines);

	/* Sort shadow */
	usb_CalSortShadow(dev, sp, hilight, shadow, shading_lines);

	rgb  = (RGBUShortDef*)scan->pScanBuffer;
	rgb += sp->Size.dwPhyPixels * hilight;

	/* Sum */
	for( lines = hilight; lines < (shading_lines-shadow); lines++ ) {

		for( x = 0; x < sp->Size.dwPhyPixels; x++ ) {
			pr[x] += rgb[x].Red;
			pg[x] += rgb[x].Green;
			pb[x] += rgb[x].Blue;
		}

		rgb += sp->Size.dwPhyPixels;
	}
}

/** usb_AdjustWhiteShading
 * fine calibration part 2 - read the white calibration area and calculate
 * the gain coefficient for each pixel
 */
static SANE_Bool usb_AdjustWhiteShading( Plustek_Device *dev )
{
	char          tmp[40];
	ScanDef      *scan  = &dev->scanning;
	DCapsDef     *scaps = &dev->usbDev.Caps;
	HWDef        *hw    = &dev->usbDev.HwSetting;
	u_long       *pBuf  = scan->pScanBuffer;
	u_long        dw, dwLines, dwRead;
	u_long        shading_lines;
	MonoWordDef  *pValue;
	u_short      *m_pAvMono;
	u_long       *pdw, *m_pSum;
	u_short       hilight, shadow;
	int           i;
	SANE_Bool     swap = usb_HostSwap();
	
	if( scaps->workaroundFlag & _WAF_SKIP_FINE )
		return SANE_TRUE;

	DBG( _DBG_INFO, "#########################\n" );
	DBG( _DBG_INFO, "usb_AdjustWhiteShading()\n" );
	
	m_pAvMono = (u_short*)scan->pScanBuffer;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	usb_PrepareFineCal( dev, &m_ScanParam, 0 );

	if( m_ScanParam.PhyDpi.x > 75)
		shading_lines = 64;
	else
		shading_lines = 32;

	/* NOTE: hilight + shadow < shading_lines */
	hilight = 4;
	shadow  = 4;

	m_ScanParam.bCalibration = PARAM_WhiteShading;
	m_ScanParam.Size.dwLines = shading_lines;

	if( _LM9831 == hw->chip ) {

		m_ScanParam.UserDpi.x = usb_SetAsicDpiX( dev, m_ScanParam.UserDpi.x);
		if( m_ScanParam.UserDpi.x < 100 )
			m_ScanParam.UserDpi.x = 150;

		/* Now DPI X is physical */
		m_ScanParam.Origin.x      = m_ScanParam.Origin.x % (u_short)m_dHDPIDivider;
		m_ScanParam.Size.dwPixels = (u_long)scaps->Normal.Size.x * m_ScanParam.UserDpi.x / 300UL;
		m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2UL * m_ScanParam.bChannels;
		if( usb_IsCISDevice(dev) && m_ScanParam.bDataType == SCANDATATYPE_Color )
			m_ScanParam.Size.dwBytes *= 3;

		m_dwPixels = scan->sParam.Size.dwPixels * m_ScanParam.UserDpi.x / 
		             scan->sParam.UserDpi.x;

		dw = (u_long)(hw->wDRAMSize - 196 /*192 KiB*/) * 1024UL;
		for( dwLines = dw / m_ScanParam.Size.dwBytes;
			 dwLines < m_ScanParam.Size.dwLines; m_ScanParam.Size.dwLines>>=1);
	}

	/* goto the correct position again... */
	if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

		usb_ModuleToHome( dev, SANE_TRUE );
		usb_ModuleMove  ( dev, MOVE_Forward,
		                  (u_long)dev->usbDev.pSource->ShadingOriginY );
	}

	sprintf( tmp, "fine-white.raw" );
	DBG( _DBG_INFO2, "FINE WHITE Calibration Strip: %s\n", tmp );
	DBG( _DBG_INFO2, "Shad.-Lines = %lu\n", shading_lines );
	DBG( _DBG_INFO2, "Lines       = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels      = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes       = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X    = %u\n",  m_ScanParam.Origin.x );

	for( dw = shading_lines, dwRead = 0; dw; dw -= m_ScanParam.Size.dwLines ) {

		if( usb_SetScanParameters( dev, &m_ScanParam ) &&
		    usb_ScanBegin( dev, SANE_FALSE )) {

			DBG(_DBG_INFO2,"TotalBytes = %lu\n",m_ScanParam.Size.dwTotalBytes);
			if( _LM9831 == hw->chip ) {
				/* Delay for white shading hold for 9831-1200 scanner */
				usleep(900000);
			}

			if( usb_ScanReadImage( dev, (u_char*)pBuf + dwRead,
			                       m_ScanParam.Size.dwTotalBytes)) {

				if( _LM9831 == hw->chip ) {
					/* Delay for white shading hold for 9831-1200 scanner */
					usleep(10000);
				}

				if( 0 == dwRead ) {
					dumpPicInit(&m_ScanParam, tmp);
				}
				
				dumpPic(tmp, (u_char*)pBuf + dwRead, m_ScanParam.Size.dwTotalBytes, 0);

				if( usb_ScanEnd( dev )) {
					dwRead += m_ScanParam.Size.dwTotalBytes;
					continue;
				}
			}
		}

		DBG( _DBG_ERROR, "usb_AdjustWhiteShading() failed\n" );
		return SANE_FALSE;
	}

	m_pSum = (u_long*)((u_char*)pBuf + m_ScanParam.Size.dwPhyBytes * shading_lines);
	
	/*
	 * do some reordering on CIS based devices:
	 * from RRRRRRR.... GGGGGGGG.... BBBBBBBBB, create RGB RGB RGB ...
	 * to use the following code, originally written for CCD devices...
	 */
	if( usb_IsCISDevice(dev)) {

		u_short *dest, *src;
		u_long   dww;

		src = (u_short*)pBuf;

		DBG( _DBG_INFO2, "PhyBytes  = %lu\n", m_ScanParam.Size.dwPhyBytes );
		DBG( _DBG_INFO2, "PhyPixels = %lu\n", m_ScanParam.Size.dwPhyPixels );
		DBG( _DBG_INFO2, "Pixels    = %lu\n", m_ScanParam.Size.dwPixels );
		DBG( _DBG_INFO2, "Bytes     = %lu\n", m_ScanParam.Size.dwBytes  );
		DBG( _DBG_INFO2, "Channels  = %u\n",  m_ScanParam.bChannels );

		for( dwLines = shading_lines; dwLines; dwLines-- ) {

			dest = a_wWhiteShading;

			for( dw=dww=0; dw < m_ScanParam.Size.dwPhyPixels; dw++, dww+=3 ) {

				dest[dww]     = src[dw];
				dest[dww + 1] = src[m_ScanParam.Size.dwPhyPixels + dw];
				dest[dww + 2] = src[m_ScanParam.Size.dwPhyPixels * 2 + dw];
			}

			/* copy line back ... */
			memcpy( src, dest, m_ScanParam.Size.dwPhyPixels * 3 * 2 );
			src = &src[m_ScanParam.Size.dwPhyPixels * 3];
		}

		m_ScanParam.bChannels = 3;
	}

	if( _LM9831 == hw->chip ) {

		u_short *pwDest = (u_short*)pBuf;
		HiLoDef *pwSrce = (HiLoDef*)pBuf;

		pwSrce  += ((u_long)(scan->sParam.Origin.x-m_ScanParam.Origin.x) /
		           (u_short)m_dHDPIDivider) *
		                   (scaps->OpticDpi.x / 300UL) * m_ScanParam.bChannels;

		for( dwLines = shading_lines; dwLines; dwLines--) {

#ifdef SWAP_FINE
			if(usb_HostSwap()) {
#endif
				for( dw = 0; dw < m_dwPixels * m_ScanParam.bChannels; dw++ ) 
					pwDest[dw] = _HILO2WORD(pwSrce[dw]);
#ifdef SWAP_FINE
			} else {
				for( dw = 0; dw < m_dwPixels * m_ScanParam.bChannels; dw++ )
					pwDest[dw] = _LOHI2WORD(pwSrce[dw]);
			}
#endif
			pwDest += (u_long)m_dwPixels * m_ScanParam.bChannels;
			pwSrce  = (HiLoDef*)((u_char*)pwSrce + m_ScanParam.Size.dwPhyBytes);
		}

		_SWAP(m_ScanParam.Size.dwPhyPixels, m_dwPixels);
	} else {
		/* Discard the status word and conv. the hi-lo order to intel format */
		u_short *pwDest = (u_short*)pBuf;
		HiLoDef *pwSrce = (HiLoDef*)pBuf;

		for( dwLines = shading_lines; dwLines; dwLines-- ) {

#ifdef SWAP_FINE
			if(usb_HostSwap()) {
#endif
				for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels *
				                                 m_ScanParam.bChannels; dw++) {
					pwDest[dw] = _HILO2WORD(pwSrce[dw]);
				}
#ifdef SWAP_FINE
			} else {
				for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels *
				                                 m_ScanParam.bChannels; dw++) {
					pwDest[dw] = _LOHI2WORD(pwSrce[dw]);
				}
			}
#endif
			pwDest += m_ScanParam.Size.dwPhyPixels * m_ScanParam.bChannels;
			pwSrce = (HiLoDef*)((u_char*)pwSrce + m_ScanParam.Size.dwPhyBytes);
		}
	}

	if( scan->sParam.bDataType == SCANDATATYPE_Color ) {

		usb_procHighlightAndShadow(dev, &m_ScanParam, hilight, shadow, shading_lines);

		pValue = (MonoWordDef*)a_wWhiteShading;
		pdw    = (u_long*)m_pSum;

		/* Software gain */
		if( scan->sParam.bSource != SOURCE_Negative ) {

			for( i = 0; i < 3; i++ ) {

				for(dw=m_ScanParam.Size.dwPhyPixels; dw; dw--,pValue++,pdw++) {

					*pdw = *pdw * 1000 / ((shading_lines - hilight - shadow) *
					                               scan->sParam.swGain[i]);
					if(*pdw > 65535U)
						pValue->Mono = 65535U;
					else
						pValue->Mono = (u_short)*pdw;
					
					if (pValue->Mono > 16384U)
						pValue->Mono = (u_short)(GAIN_Target * 16384U / pValue->Mono);
					else
						pValue->Mono = GAIN_Target;

#ifdef SWAP_FINE
					if( swap )
#endif
						_SWAP(pValue->HiLo.bHi, pValue->HiLo.bLo);
				}
			}
		} else {
			for( dw = m_ScanParam.Size.dwPhyPixels*3; dw; dw--,pValue++,pdw++)
				pValue->Mono=(u_short)(*pdw/(shading_lines-hilight-shadow));

				/* swapping will be done later in usb_ResizeWhiteShading() */
		}
	} else {

		/* gray mode */
		u_short *pwAv, *pw;
		u_short  w, wV;

		memset( m_pSum, 0, m_ScanParam.Size.dwPhyPixels << 2 );
		if( hilight ) {
			for( dwLines = hilight,
				 pwAv = m_pAvMono + m_ScanParam.Size.dwPhyPixels * dwLines;
				 dwLines < shading_lines;
              				 dwLines++, pwAv += m_ScanParam.Size.dwPhyPixels) {

				for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {

					pw = m_pAvMono;
					wV = pwAv [dw];
					for( w = 0; w < hilight; w++,
						 pw += m_ScanParam.Size.dwPhyPixels ) {
						if( wV > pw[dw] )
							_SWAP( wV, pw[dw] );
					}
					pwAv[dw] = wV;
				}
			}
		}

		/* Sort shadow */
		if (shadow) {
			for (dwLines = hilight, pwAv = m_pAvMono + m_ScanParam.Size.dwPhyPixels * dwLines;
			 	 dwLines < (shading_lines - shadow); dwLines++, pwAv += m_ScanParam.Size.dwPhyPixels)
				for (dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++)
				{
					pw = m_pAvMono + (shading_lines - shadow) * m_ScanParam.Size.dwPhyPixels;
					wV = pwAv [dw];
					for (w = 0; w < shadow; w++, pw += m_ScanParam.Size.dwPhyPixels)
						if (wV < pw [dw])
							_SWAP (wV, pw[dw]);
					pwAv [dw] = wV;
				}
		}

		/* Sum */
		pdw = (u_long*)m_pSum;

		for (dwLines = hilight,
			 pwAv = m_pAvMono + m_ScanParam.Size.dwPhyPixels * dwLines;
			 dwLines < (shading_lines - shadow);
			 dwLines++, pwAv += m_ScanParam.Size.dwPhyPixels) {
			for (dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++)
				pdw[dw] += pwAv[dw];
		}

		/* Software gain */
		pValue = (MonoWordDef*)a_wWhiteShading;
		if( scan->sParam.bSource != SOURCE_Negative ) {

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++) {
				
				pdw[dw] = pdw[dw] * 1000 /((shading_lines-hilight-shadow) *
				                                 scan->sParam.swGain[1]);
				if( pdw[dw] > 65535U )
					pValue[dw].Mono = 65535;
				else
					pValue[dw].Mono = (u_short)pdw[dw];

				if( pValue[dw].Mono > 16384U ) {
					pValue[dw].Mono = (u_short)(GAIN_Target * 16384U / pValue[dw].Mono);
				} else {
					pValue[dw].Mono = GAIN_Target;
				}

#ifdef SWAP_FINE
				if( swap )
#endif
					_SWAP(pValue[dw].HiLo.bHi, pValue[dw].HiLo.bLo);
			}
			
		} else{

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
				pValue[dw].Mono = (u_short)(pdw[dw] /
				                  (shading_lines - hilight - shadow));

				/* swapping will be done later in usb_ResizeWhiteShading() */
			}
		}
	}

	usb_SaveCalSetShading( dev, &m_ScanParam );

	if( scan->sParam.bSource != SOURCE_Negative ) {
		usb_line_statistics( "White", a_wWhiteShading, m_ScanParam.Size.dwPhyPixels,
	                         scan->sParam.bDataType == SCANDATATYPE_Color?1:0);
	}
	return SANE_TRUE;
}

/** for negative film only
 * we need to resize the gain to obtain bright white...
 */
static void usb_ResizeWhiteShading( double dAmp, u_short *pwShading, int iGain )
{
	u_long  dw, dwAmp;
	u_short w;
	
	DBG( _DBG_INFO2, "ResizeWhiteShading: dAmp=%.3f, iGain=%i\n", dAmp, iGain );

	for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
		
		dwAmp = (u_long)(GAIN_Target * 0x4000 /
		                            (pwShading[dw] + 1) * dAmp) * iGain / 1000;

		if( dwAmp <= GAIN_Target)
			w = (u_short)dwAmp;
		else 
			w = GAIN_Target;

#ifndef SWAP_FINE
		pwShading[dw] = (u_short)_LOBYTE(w) * 256 + _HIBYTE(w);
#else
		pwShading[dw] = w;
#endif
	}

#ifdef SWAP_FINE
	if( usb_HostSwap())
		usb_Swap( pwShading, m_ScanParam.Size.dwPhyPixels );
#endif
}

/** do the base settings for calibration scans
 */
static void
usb_PrepareCalibration( Plustek_Device *dev )
{
	ScanDef  *scan  = &dev->scanning;
	DCapsDef *scaps = &dev->usbDev.Caps;
	u_char   *regs  = dev->usbDev.a_bRegs;

	usb_GetSWOffsetGain( dev );

	memset( &m_ScanParam, 0, sizeof(ScanParam));

	m_ScanParam.UserDpi   = scaps->OpticDpi;
	m_ScanParam.PhyDpi    = scaps->OpticDpi;
	m_ScanParam.bChannels = scan->sParam.bChannels;
	m_ScanParam.bBitDepth = 16;
	m_ScanParam.bSource   = scan->sParam.bSource;
	m_ScanParam.Origin.y  = 0;

	if( scan->sParam.bDataType == SCANDATATYPE_Color )
		m_ScanParam.bDataType = SCANDATATYPE_Color;
	else
		m_ScanParam.bDataType = SCANDATATYPE_Gray;

	usb_SetMCLK( dev, &m_ScanParam );
 
	/* preset these registers offset/gain */
	regs[0x38] = regs[0x39] = regs[0x3a] = 0;
	regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;
	regs[0x45] &= ~0x10;

	memset( a_wWhiteShading, 0, _SHADING_BUF );
	memset( a_wDarkShading,  0, _SHADING_BUF );

	scan->skipCoarseCalib = SANE_FALSE;

	if( dev->adj.cacheCalData )
		if( usb_ReadAndSetCalData( dev ))
			scan->skipCoarseCalib = SANE_TRUE;

	/* as sheet-fed device we use the cached values, or
	 * perform the calibration upon request
	 */
	if( usb_IsSheetFedDevice(dev)) {
		if( !scan->skipCoarseCalib && !usb_InCalibrationMode(dev)) {

			DBG(_DBG_INFO2,"SHEET-FED device, skip coarse calibration!\n");
			scan->skipCoarseCalib = SANE_TRUE;

			regs[0x3b] = 0x0a;
			regs[0x3c] = 0x0a;
			regs[0x3d] = 0x0a;

			/* use frontend values... */
			if((dev->adj.rofs != -1) &&
			   (dev->adj.gofs != -1) && (dev->adj.bofs != -1)) {
				regs[0x38] = (dev->adj.rofs & 0x3f);
				regs[0x39] = (dev->adj.gofs & 0x3f);
				regs[0x3a] = (dev->adj.bofs & 0x3f);
			}

			if((dev->adj.rgain != -1) &&
			   (dev->adj.ggain != -1) && (dev->adj.bgain != -1)) {
				setAdjGain( dev->adj.rgain, &regs[0x3b] );
				setAdjGain( dev->adj.ggain, &regs[0x3c] );
				setAdjGain( dev->adj.bgain, &regs[0x3d] );
			}
		}
	}
}

/**
 */
static SANE_Bool
usb_SpeedTest( Plustek_Device *dev )
{
	int       i;
	double    s, e, r, tr;
	struct timeval start, end;
	DCapsDef *scaps   = &dev->usbDev.Caps;
	HWDef    *hw      = &dev->usbDev.HwSetting;
	u_char   *regs    = dev->usbDev.a_bRegs;
	u_long   *scanbuf = dev->scanning.pScanBuffer;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	bMaxITA = 0xff;

	DBG( 1, "#########################\n" );
	DBG( 1, "usb_SpeedTest(%d,%lu)\n", dev->initialized, dev->transferRate );
	if( dev->transferRate != DEFAULT_RATE ) {
		DBG( 1, "* skipped, using already detected speed: %lu Bytes/s\n", 
                dev->transferRate );
		return SANE_TRUE;
	}

	usb_PrepareCalibration( dev );
	regs[0x38] = regs[0x39] = regs[0x3a] = 0;
	regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

	/* define the strip to scan for warming up the lamp, in the end
	 * we always scan the full line, even for TPA
	 */
	m_ScanParam.bDataType     = SCANDATATYPE_Color;
	m_ScanParam.bCalibration  = PARAM_Gain;
	m_ScanParam.dMCLK         = dMCLK;
	m_ScanParam.bBitDepth     = 8;
	m_ScanParam.Size.dwLines  = 1;
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
	                            scaps->OpticDpi.x / 300UL;
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
	                            2 * m_ScanParam.bChannels;

	if( usb_IsCISDevice(dev))
		m_ScanParam.Size.dwBytes *= 3;

	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
	                                                300UL / scaps->OpticDpi.x);
	r = 0.0;
	dev->transferRate = 2000000;

	for( i = 0; i < _TLOOPS ; i++ ) {

		if( !usb_SetScanParameters( dev, &m_ScanParam ))
			return SANE_FALSE;

		if( !usb_ScanBegin( dev, SANE_FALSE )) {
			DBG( _DBG_ERROR, "usb_SpeedTest() failed\n" );
			return SANE_FALSE;
		}
		if (!usb_IsDataAvailableInDRAM( dev )) 
			return SANE_FALSE;

		m_fFirst = SANE_FALSE;
		gettimeofday( &start, NULL );
		usb_ScanReadImage( dev, scanbuf, m_ScanParam.Size.dwPhyBytes );
		gettimeofday( &end, NULL );
		usb_ScanEnd( dev );
		s = (double)start.tv_sec * 1000000.0 + (double)start.tv_usec;
		e = (double)end.tv_sec   * 1000000.0 + (double)end.tv_usec;

		if( e > s )
			r += (e - s);
		else
			r += (s - e);
	}

	tr = ((double)m_ScanParam.Size.dwPhyBytes * _TLOOPS * 1000000.0)/r;
	dev->transferRate = (u_long)tr;
	DBG( 1, "usb_SpeedTest() done - %u loops, %.4fus --> %.4f B/s, %lu\n", 
	        _TLOOPS, r, tr, dev->transferRate );
	return SANE_TRUE;
}

/** read the white calibration strip until the lamp seems to be stable
 * the timed warmup will be used, when the warmup time is set to -1
 */
static SANE_Bool
usb_AutoWarmup( Plustek_Device *dev )
{
	int       i, stable_count;
	ScanDef  *scanning = &dev->scanning;
	DCapsDef *scaps    = &dev->usbDev.Caps;
	HWDef    *hw       = &dev->usbDev.HwSetting;
	u_long   *scanbuf  = scanning->pScanBuffer;
	u_char   *regs     = dev->usbDev.a_bRegs;
	u_long    dw, start, end, len;
	u_long    curR,   curG,  curB;
	u_long    lastR, lastG, lastB;
	long      diffR, diffG, diffB;
	long      thresh = _AUTO_THRESH;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	bMaxITA = 0xff;

	DBG( _DBG_INFO, "#########################\n" );
	DBG( _DBG_INFO, "usb_AutoWarmup()\n" );

	if( usb_IsCISDevice(dev)) {
		DBG( _DBG_INFO, "- function skipped, CIS device!\n" );
		return SANE_TRUE;
	}

	if( dev->adj.warmup >= 0 ) {
		DBG( _DBG_INFO, "- using timed warmup: %ds\n", dev->adj.warmup );
		if( !usb_Wait4Warmup( dev )) {
			DBG( _DBG_ERROR, "- CANCEL detected\n" );
			return SANE_FALSE;
		}
		return SANE_TRUE;
	}

	usb_PrepareCalibration( dev );
	regs[0x38] = regs[0x39] = regs[0x3a] = 0;
	regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

	/* define the strip to scan for warming up the lamp, in the end
	 * we always scan the full line, even for TPA
	 */
	m_ScanParam.bDataType     = SCANDATATYPE_Color;
	m_ScanParam.bCalibration  = PARAM_Gain;
	m_ScanParam.dMCLK         = dMCLK;
	m_ScanParam.Size.dwLines  = 1;
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
	                            scaps->OpticDpi.x / 300UL;
	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels *
	                            2 * m_ScanParam.bChannels;

	if( usb_IsCISDevice(dev))
		m_ScanParam.Size.dwBytes *= 3;

	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
	                                                300UL / scaps->OpticDpi.x);

	stable_count = 0;
	start = 500;
	len   = m_ScanParam.Size.dwPixels;

	if( scanning->sParam.bSource == SOURCE_Transparency ) {
		start  = scaps->Positive.DataOrigin.x * scaps->OpticDpi.x / 300UL;
		len    = scaps->Positive.Size.x * scaps->OpticDpi.x / 300UL;
		thresh = _AUTO_TPA_THRESH;
	}
	else if( scanning->sParam.bSource == SOURCE_Negative ) {
		start  = scaps->Negative.DataOrigin.x * scaps->OpticDpi.x / 300UL;
		len    = scaps->Negative.Size.x * scaps->OpticDpi.x / 300UL;
		thresh = _AUTO_TPA_THRESH;
	}
	end = start + len;
	DBG( _DBG_INFO2, "Start=%lu, End=%lu, Len=%lu, Thresh=%li\n", 
	                 start, end, len, thresh );

	lastR = lastG = lastB = 0;
	for( i = 0; i < _MAX_AUTO_WARMUP + 1 ; i++ ) {

		if( !usb_SetScanParameters( dev, &m_ScanParam ))
			return SANE_FALSE;

		if( !usb_ScanBegin( dev, SANE_FALSE ) ||
		    !usb_ScanReadImage( dev, scanbuf, m_ScanParam.Size.dwPhyBytes ) ||
		    !usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "usb_AutoWarmup() failed\n" );
			return SANE_FALSE;
		}

#ifdef SWAP_COARSE
		if(usb_HostSwap())
#endif
			usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwPhyBytes );

		if( end > m_ScanParam.Size.dwPhyPixels )
			end = m_ScanParam.Size.dwPhyPixels;

		curR = curG = curB = 0;
		for( dw = start; dw < end; dw++ ) {
			
			if( usb_IsCISDevice(dev)) {
				curR += ((u_short*)scanbuf)[dw];
				curG += ((u_short*)scanbuf)[dw+m_ScanParam.Size.dwPhyPixels+1];
				curB += ((u_short*)scanbuf)[dw+(m_ScanParam.Size.dwPhyPixels+1)*2];
			} else {
				curR += ((RGBUShortDef*)scanbuf)[dw].Red;
				curG += ((RGBUShortDef*)scanbuf)[dw].Green;
				curB += ((RGBUShortDef*)scanbuf)[dw].Blue;
			}
		}
		curR /= len;
		curG /= len;
		curB /= len;

		diffR = curR - lastR; lastR = curR;
		diffG = curG - lastG; lastG = curG;
		diffB = curB - lastB; lastB = curB;
		DBG( _DBG_INFO2, "%i/%i-AVE(R,G,B)= %lu(%ld), %lu(%ld), %lu(%ld)\n", 
		              i, stable_count, curR, diffR, curG, diffG, curB, diffB );

		/* we consider the lamp to be stable,
		 * when the diffs are less than thresh for at least 3 loops
		 */
		if((diffR < thresh) && (diffG < thresh) && (diffB < thresh)) {
			if( stable_count > 3 )
				break;
			stable_count++;
		} else {
			stable_count = 0;
		}

		/* no need to sleep in the first loop */
		if((i != 0) && (stable_count == 0))
			sleep( _AUTO_SLEEP );
	}

	DBG( _DBG_INFO, "usb_AutoWarmup() done - %u loops\n", i+1 );
	DBG( _DBG_INFO, "* AVE(R,G,B)= %lu(%ld), %lu(%ld), %lu(%ld)\n", 
	                       curR, diffR, curG, diffG, curB, diffB );
	return SANE_TRUE;
}

/**
 */
static int
usb_DoIt( Plustek_Device *dev )
{
	SANE_Bool skip_fine;
	ScanDef  *scan = &dev->scanning;

	DBG( _DBG_INFO, "Settings done, so start...\n" );
	if( !scan->skipCoarseCalib ) {
		DBG( _DBG_INFO2, "###### ADJUST GAIN (COARSE)#######\n" );
		if( !usb_AdjustGain(dev, 0)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return _E_INTERNAL;
		}
		DBG( _DBG_INFO2, "###### ADJUST OFFSET (COARSE) ####\n" );
		if( !usb_AdjustOffset(dev)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return _E_INTERNAL;
		}
	} else {
		DBG( _DBG_INFO2, "Coarse Calibration skipped, using saved data\n" );
	}

	skip_fine = SANE_FALSE;
	if( dev->adj.cacheCalData ) {
		skip_fine = usb_FineShadingFromFile(dev);
	}

	if( !skip_fine ) {
		DBG( _DBG_INFO2, "###### ADJUST DARK (FINE) ########\n" );
		if( !usb_AdjustDarkShading(dev)) {
			DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
			return _E_INTERNAL;
		}
		DBG( _DBG_INFO2, "###### ADJUST WHITE (FINE) #######\n" );
		if( !usb_AdjustWhiteShading(dev)) {
			DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
			return _E_INTERNAL;
		}
	} else {
		DBG( _DBG_INFO2, "###### FINE calibration skipped #######\n" );

		m_ScanParam = scan->sParam;
		usb_GetPhyPixels( dev, &m_ScanParam );

		usb_line_statistics( "Dark", a_wDarkShading, m_ScanParam.Size.dwPhyPixels,
		                      m_ScanParam.bDataType == SCANDATATYPE_Color?1:0);
		usb_line_statistics( "White", a_wWhiteShading, m_ScanParam.Size.dwPhyPixels,
		                      m_ScanParam.bDataType == SCANDATATYPE_Color?1:0);

/*		dev->usbDev.a_bRegs[0x45] &= ~0x10;*/
	}
	return 0;
}

/** usb_DoCalibration
 */
static int
usb_DoCalibration( Plustek_Device *dev )
{
	int       result;
	ScanDef  *scanning = &dev->scanning;
	DCapsDef *scaps    = &dev->usbDev.Caps;
	HWDef    *hw       = &dev->usbDev.HwSetting;
	u_char   *regs     = dev->usbDev.a_bRegs;
	double    dRed, dGreen, dBlue;

	DBG( _DBG_INFO, "usb_DoCalibration()\n" );

	if( SANE_TRUE == scanning->fCalibrated )
		return SANE_TRUE;

	/* Go to shading position
     */
	DBG( _DBG_INFO, "...goto shading position\n" );

	/* HEINER: Currently not clear why Plustek didn't use the ShadingOriginY
	 *         for all modes
	 * It should be okay to remove this and reference to the ShadingOriginY
	 */	
#if 0	
	if( scanning->sParam.bSource == SOURCE_Negative ) {

		DBG( _DBG_INFO, "DataOrigin.x=%u, DataOrigin.y=%u\n",
		 dev->usbDev.pSource->DataOrigin.x, dev->usbDev.pSource->DataOrigin.y);
		if(!usb_ModuleMove( dev, MOVE_Forward,
		                                  (dev->usbDev.pSource->DataOrigin.y +
  			                               dev->usbDev.pSource->Size.y / 2))) {
			return _E_LAMP_NOT_IN_POS;
		}

	} else {
#endif
		DBG( _DBG_INFO, "ShadingOriginY=%lu\n",
				(u_long)dev->usbDev.pSource->ShadingOriginY );

		if((hw->motorModel == MODEL_HuaLien) && (scaps->OpticDpi.x==600)) {
			if (!usb_ModuleMove(dev, MOVE_ToShading,
			                    (u_long)dev->usbDev.pSource->ShadingOriginY)) {
				return _E_LAMP_NOT_IN_POS;
			}
		} else {
			if( !usb_ModuleMove(dev, MOVE_Forward,
			                    (u_long)dev->usbDev.pSource->ShadingOriginY)) {
				return _E_LAMP_NOT_IN_POS;
			}
		}
/*	}*/

	DBG( _DBG_INFO, "shading position reached\n" );

	usb_SpeedTest( dev );

	if( !usb_AutoWarmup( dev ))
		return SANE_FALSE;

	usb_PrepareCalibration( dev );

	/** this won't work for Plustek devices!!!
	 */
#if 0
	if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ||
		!(SCANDEF_QualityScan & dev->scanning.dwFlag)) {
#else
	if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ) {
#endif

		DBG( _DBG_INFO, "--> BYPASS\n" );
		regs[0x38] = regs[0x39] = regs[0x3a] = 0;
		regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

		setAdjGain( dev->adj.rgain, &regs[0x3b] );
		setAdjGain( dev->adj.ggain, &regs[0x3c] );
		setAdjGain( dev->adj.bgain, &regs[0x3d] );

		regs[0x45] |= 0x10;
		usb_SetMCLK( dev, &scanning->sParam );

	    dumpregs( dev->fd, regs );
		DBG( _DBG_INFO, "<-- BYPASS\n" );

	} else {

		switch( scanning->sParam.bSource ) {

			case SOURCE_Negative:
				DBG( _DBG_INFO, "NEGATIVE Shading\n" );
				m_dwIdealGain = IDEAL_GainNormal;

				if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {
					DBG( _DBG_INFO, "No Plustek model: %udpi\n",
						            scanning->sParam.PhyDpi.x );
					usb_SetMCLK( dev, &scanning->sParam );
				} else {

					if( dev->usbDev.Caps.OpticDpi.x == 600 )
						dMCLK = 7;
					else
						dMCLK = 8;
				}

				for(;;) {
					if( usb_AdjustGain( dev, 2)) {
						if( regs[0x3b] && regs[0x3c] && regs[0x3d]) {
							break;
						} else {
							regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;
							dMCLK--;
						}
					} else {
						return _E_LAMP_NOT_STABLE;
					}
				}
				scanning->sParam.dMCLK = dMCLK;
				Gain_Reg.Red    = regs[0x3b];
				Gain_Reg.Green  = regs[0x3c];
				Gain_Reg.Blue   = regs[0x3d];
				Gain_NegHilight = Gain_Hilight;
			
				DBG( _DBG_INFO, "MCLK      = %.3f\n", dMCLK );
				DBG( _DBG_INFO, "GainRed   = %u\n", regs[0x3b] );
				DBG( _DBG_INFO, "GainGreen = %u\n", regs[0x3c] );
				DBG( _DBG_INFO, "GainBlue  = %u\n", regs[0x3d] );

#if 0
    			if( !usb_ModuleMove( dev, MOVE_Backward,
    								 dev->usbDev.pSource->DataOrigin.y +
    								 dev->usbDev.pSource->Size.y / 2 -
    								 dev->usbDev.pSource->ShadingOriginY)) {
    				return _E_LAMP_NOT_IN_POS;
    			}
#endif
				regs[0x45] &= ~0x10;

				regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

				if(!usb_AdjustGain( dev, 1 ))
					return _E_INTERNAL;
				
				regs[0x3b] = regs[0x3c] = regs[0x3d] = 1;

				DBG( _DBG_INFO, "Settings done, so start...\n" );
				if( !usb_AdjustOffset(dev) || !usb_AdjustDarkShading(dev) ||
				                              !usb_AdjustWhiteShading(dev)) {
					return _E_INTERNAL;
				}

				dRed    = 0.93 + 0.067 * Gain_Reg.Red;
				dGreen  = 0.93 + 0.067 * Gain_Reg.Green;
				dBlue   = 0.93 + 0.067 * Gain_Reg.Blue;
				dExpect = 2.85;
				if( dBlue >= dGreen && dBlue >= dRed )
					dMax = dBlue;
				else
					if( dGreen >= dRed && dGreen >= dBlue )
						dMax = dGreen;
					else
						dMax = dRed;

				dMax = dExpect / dMax;
				dRed   *= dMax;
				dGreen *= dMax;
				dBlue  *= dMax;

				if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
					usb_ResizeWhiteShading( dRed,  a_wWhiteShading,
					                                scanning->sParam.swGain[0]);
					usb_ResizeWhiteShading( dGreen, a_wWhiteShading +
					                                m_ScanParam.Size.dwPhyPixels,
					                                scanning->sParam.swGain[1]);
					usb_ResizeWhiteShading( dBlue,  a_wWhiteShading +
					                                m_ScanParam.Size.dwPhyPixels*2,
					                                scanning->sParam.swGain[2]);
				}
				usb_line_statistics( "White", a_wWhiteShading, 
				                     m_ScanParam.Size.dwPhyPixels, SANE_TRUE);
				break;

			case SOURCE_ADF:
				DBG( _DBG_INFO, "ADF Shading\n" );
				m_dwIdealGain = IDEAL_GainPositive;
				if( scanning->sParam.bDataType == SCANDATATYPE_BW ) {
					if( scanning->sParam.PhyDpi.x <= 200 ) {
						scanning->sParam.dMCLK = 4.5;
						dMCLK = 4.0;
					} else if ( scanning->sParam.PhyDpi.x <= 300 ) {
						scanning->sParam.dMCLK = 4.0;
						dMCLK = 3.5;
					} else if( scanning->sParam.PhyDpi.x <= 400 ) {
						scanning->sParam.dMCLK = 5.0;
						dMCLK = 4.0;
					} else {
						scanning->sParam.dMCLK = 6.0;
						dMCLK = 4.0;
					}
				} else { /* Gray */

					if( scanning->sParam.PhyDpi.x <= 400 ) {
						scanning->sParam.dMCLK = 6.0;
						dMCLK = 4.5;
					} else {
						scanning->sParam.dMCLK = 9.0;
						dMCLK = 7.0;
					}
				}
				dMCLK_ADF = dMCLK;

				result = usb_DoIt( dev );
				if( result != 0 )
					return result;
				break;

			case SOURCE_Transparency:
				DBG( _DBG_INFO, "TRANSPARENCY Shading\n" );
				m_dwIdealGain = IDEAL_GainPositive;

				if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {
					DBG( _DBG_INFO, "No Plustek model: %udpi\n",
						            scanning->sParam.PhyDpi.x );
					usb_SetMCLK( dev, &scanning->sParam );

				} else {
					if( dev->usbDev.Caps.OpticDpi.x == 600 ) {
						scanning->sParam.dMCLK = 8;
						dMCLK = 4;
					} else {
						scanning->sParam.dMCLK = 8;
						dMCLK = 6;
					}
				}
				result = usb_DoIt( dev );
				if( result != 0 )
					return result;
				break;

			default:
				if( !_IS_PLUSTEKMOTOR(hw->motorModel)) {
    				DBG( _DBG_INFO, "No Plustek model: %udpi\n",
    												scanning->sParam.PhyDpi.x );
    				usb_SetMCLK( dev, &scanning->sParam );

    			} else 	if( dev->usbDev.Caps.OpticDpi.x == 600 ) {

    				DBG( _DBG_INFO, "Default Shading (600dpi)\n" );

    				if( dev->usbDev.Caps.bCCD == kSONY548 )	{

    					DBG( _DBG_INFO, "CCD - SONY548\n" );
    					if( scanning->sParam.PhyDpi.x <= 75 ) {
    						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
    							scanning->sParam.dMCLK = dMCLK = 2.5;
    						else if(scanning->sParam.bDataType == SCANDATATYPE_Gray)
    							scanning->sParam.dMCLK = dMCLK = 7.0;
    						else
    							scanning->sParam.dMCLK = dMCLK = 7.0;

    					} else if( scanning->sParam.PhyDpi.x <= 300 ) {
    						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
    							scanning->sParam.dMCLK = dMCLK = 3.0;
    						else if(scanning->sParam.bDataType == SCANDATATYPE_Gray)
    							scanning->sParam.dMCLK = dMCLK = 6.0;
    						else  {
    							if( scanning->sParam.PhyDpi.x <= 100 )
    								scanning->sParam.dMCLK = dMCLK = 6.0;
    							else if( scanning->sParam.PhyDpi.x <= 200 )
    								scanning->sParam.dMCLK = dMCLK = 5.0;
    							else
    								scanning->sParam.dMCLK = dMCLK = 4.5;
    						}
    					} else if( scanning->sParam.PhyDpi.x <= 400 ) {
    						if( scanning->sParam.bDataType  == SCANDATATYPE_Color )
    							scanning->sParam.dMCLK = dMCLK = 4.0;
    						else if( scanning->sParam.bDataType == SCANDATATYPE_Gray )
    							scanning->sParam.dMCLK = dMCLK = 6.0;
    						else
    							scanning->sParam.dMCLK = dMCLK = 4.0;
    					} else {
    						if(scanning->sParam.bDataType  == SCANDATATYPE_Color)
    							scanning->sParam.dMCLK = dMCLK = 6.0;
    						else if(scanning->sParam.bDataType == SCANDATATYPE_Gray)
    							scanning->sParam.dMCLK = dMCLK = 7.0;
    						else
    							scanning->sParam.dMCLK = dMCLK = 6.0;
    					}

    				} else if( dev->usbDev.Caps.bPCB == 0x02 ) {
    					DBG( _DBG_INFO, "PCB - 0x02\n" );
    					if( scanning->sParam.PhyDpi.x > 300 )
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 6: 16);
    					else if( scanning->sParam.PhyDpi.x > 150 )
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?4.5: 13.5);
    					else
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 8);

    				}  else if( dev->usbDev.Caps.bButtons ) { /* with lens Shading piece (with gobo) */

    					DBG( _DBG_INFO, "CAPS - Buttons\n" );
    					scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
    					if( dev->usbDev.HwSetting.motorModel == MODEL_KaoHsiung )	{
    						if( dev->usbDev.Caps.bCCD == kNEC3799 ) {
    							if( scanning->sParam.PhyDpi.x > 300 )
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 6: 13);
    							else if(scanning->sParam.PhyDpi.x > 150)
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?4.5:13.5);
    							else
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
    						} else {
    							scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
    						}
    					} else { /* motorModel == MODEL_Hualien */
    						/*  IMPORTANT !!!!
    						 * for Hualien 600 dpi scanner big noise
                             */
    						hw->wLineEnd = 5384;
    						if(scanning->sParam.bDataType  == SCANDATATYPE_Color &&
    							((scanning->sParam.bBitDepth == 8 && 
								 (scanning->sParam.PhyDpi.x == 200 ||scanning->sParam.PhyDpi.x == 300))))
    							hw->wLineEnd = 7000;
    						regs[0x20] = _HIBYTE(hw->wLineEnd);
    						regs[0x21] = _LOBYTE(hw->wLineEnd);

    						if( scanning->sParam.PhyDpi.x > 300 ) {
    							if (scanning->sParam.bBitDepth > 8)
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType  == SCANDATATYPE_Color)? 5: 13);
    							else
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 6: 13);
    						} else {
    							if( scanning->sParam.bBitDepth > 8 )
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 5: 13);
    							else
    								scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)?3: 6);
    						}
    					}
    				} else  { /* without lens Shading piece (without gobo) - Model U12 only */
    					DBG( _DBG_INFO, "Default trunc (U12)\n" );
    					if( scanning->sParam.PhyDpi.x > 300 )
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 3: 9);
    					else
    						scanning->sParam.dMCLK = dMCLK = ((scanning->sParam.bDataType == SCANDATATYPE_Color)? 2: 6);
    				}
    			} else {	/* Device.Caps.OpticDpi.x == 1200 */

    				DBG( _DBG_INFO, "Default Shading (1200dpi)\n" );
    				if( scanning->sParam.bDataType != SCANDATATYPE_Color ) {
    					if( scanning->sParam.PhyDpi.x > 300 )
    						scanning->sParam.dMCLK = dMCLK = 6.0;
    					else {
    						scanning->sParam.dMCLK = dMCLK = 5.0;
    						regs[0x0a] = 1;
    					}
    				} else {
    					if( scanning->sParam.PhyDpi.x <= 300)
    						scanning->sParam.dMCLK = dMCLK = 2.0;
    					else if( scanning->sParam.PhyDpi.x <= 800 )
    						scanning->sParam.dMCLK = dMCLK = 4.0;
    					else
    						scanning->sParam.dMCLK = dMCLK = 5.5;
    				}
    			}

				if (m_ScanParam.bSource == SOURCE_ADF)
					m_dwIdealGain = IDEAL_GainPositive;
				else
					m_dwIdealGain = IDEAL_GainNormal;

				result = usb_DoIt( dev );
				if( result != 0 )
					return result;
				break;
		}
	}

	/* home the sensor after calibration */
	if( _IS_PLUSTEKMOTOR(hw->motorModel)) {
		if( hw->motorModel != MODEL_Tokyo600 ) {
			usb_ModuleMove  ( dev, MOVE_Forward, hw->wMotorDpi / 5 );
			usb_ModuleToHome( dev, SANE_TRUE );
		}
	} else {
		usb_ModuleMove( dev, MOVE_Forward, 10 );
		usleep( 1500 );
		usb_ModuleToHome( dev, SANE_TRUE );
	}

	if( scanning->sParam.bSource == SOURCE_ADF ) {

		if( scaps->bCCD == kNEC3778 )
			usb_ModuleMove( dev, MOVE_Forward, 1000 );

		else /* if( scaps->bCCD == kNEC3799) */
			usb_ModuleMove( dev, MOVE_Forward, 3 * 300 + 38 );

		usb_MotorOn( dev, SANE_FALSE );
	}

	scanning->fCalibrated = SANE_TRUE;
	DBG( _DBG_INFO, "Calibration done\n" );
	DBG( _DBG_INFO, "-----------------------\n" );
	DBG( _DBG_INFO, "Static Gain:\n" );
	DBG( _DBG_INFO, "REG[0x3b] = %u\n", regs[0x3b] );
	DBG( _DBG_INFO, "REG[0x3c] = %u\n", regs[0x3c] );
	DBG( _DBG_INFO, "REG[0x3d] = %u\n", regs[0x3d] );
	DBG( _DBG_INFO, "Static Offset:\n" );
	DBG( _DBG_INFO, "REG[0x38] = %i\n", regs[0x38] );
	DBG( _DBG_INFO, "REG[0x39] = %i\n", regs[0x39] );
	DBG( _DBG_INFO, "REG[0x3a] = %i\n", regs[0x3a] );
	DBG( _DBG_INFO, "MCLK      = %.2f\n", scanning->sParam.dMCLK );
	DBG( _DBG_INFO, "-----------------------\n" );
	return SANE_TRUE;
}

/* on different sensor orders, we need to adjust the shading buffer
 * pointer, otherwise we correct the wrong channels
 */
static void
get_ptrs(Plustek_Device *dev, u_short *buf, u_long offs,
         u_short **r, u_short **g, u_short **b)
{
	ScanDef  *scan  = &dev->scanning;
	DCapsDef *scaps = &dev->usbDev.Caps;
	u_char so = scaps->bSensorOrder;

	if (_WAF_RESET_SO_TO_RGB & scaps->workaroundFlag) {
		if (scaps->bPCB != 0) {
			if (scan->sParam.PhyDpi.x > scaps->bPCB)
				so = SENSORORDER_rgb;
		}
	}

	switch( so ) {
		case SENSORORDER_gbr:
			*g = buf;
			*b = buf + offs;
			*r = buf + offs * 2;
			break;

		case SENSORORDER_bgr:
			*b = buf;
			*g = buf + offs;
			*r = buf + offs * 2;
			break;

		case SENSORORDER_rgb:
		default:
			*r = buf;
			*g = buf + offs;
			*b = buf + offs * 2;
			break;
	}
}

/** usb_DownloadShadingData
 * according to the job id, different registers or DRAM areas are set
 * in preparation for calibration or scanning
 */
static SANE_Bool
usb_DownloadShadingData( Plustek_Device *dev, u_char what )
{
	u_char    channel;
	u_short   *r, *g, *b;
	DCapsDef  *scaps = &dev->usbDev.Caps;
	ScanDef   *scan  = &dev->scanning;
	HWDef     *hw    = &dev->usbDev.HwSetting;
	ScanParam *param = &dev->scanning.sParam;
	u_char    *regs  = dev->usbDev.a_bRegs;

	DBG( _DBG_INFO, "usb_DownloadShadingData(%u)\n", what );

	channel = CHANNEL_green;
	if( usb_IsCISDevice(dev))
		channel = CHANNEL_blue;

	switch( what ) {

		case PARAM_WhiteShading:
			if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

				usb_SetDarkShading( dev, CHANNEL_red, a_wDarkShading,
				                    (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				usb_SetDarkShading( dev, CHANNEL_green, a_wDarkShading +
				                    m_ScanParam.Size.dwPhyPixels,
				                    (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				usb_SetDarkShading( dev, CHANNEL_blue, a_wDarkShading +
				                    m_ScanParam.Size.dwPhyPixels * 2,
				                    (u_short)m_ScanParam.Size.dwPhyPixels * 2);
			} else {
				usb_SetDarkShading( dev, channel, a_wDarkShading +
				                    m_ScanParam.Size.dwPhyPixels,
				                    (u_short)m_ScanParam.Size.dwPhyPixels * 2);
			}
			regs[0x40] = 0x40;
			regs[0x41] = 0x00;

			/* set RAM configuration AND
			 * Gain = Multiplier Coefficient/16384
			 * CFG Register 0x40/0x41 for Multiplier Coefficient Source
			 * External DRAM for Offset Coefficient Source
			 */
			regs[0x42] = (u_char)(( hw->wDRAMSize > 512)? 0x64: 0x24);
			_UIO(sanei_lm983x_write( dev->fd, 0x40,
			                        &regs[0x40], 0x42-0x40+1, SANE_TRUE ));
			break;

		case PARAM_Scan:
			{
#if 0
				if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ||
					!(SCANDEF_QualityScan & dev->scanning.dwFlag)) {
#else
				if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ) {
#endif
					DBG( _DBG_INFO, "--> BYPASS\n" );
					/* set RAM configuration AND
					 * Bypass Multiplier
					 * CFG Register 0x40/0x41 for Multiplier Coefficient Source
					 * CFG Register 0x3e/0x3f for Offset Coefficient Source
					 */
					regs[0x03] = 0;
					regs[0x40] = 0x40;
					regs[0x41] = 0x00;
					regs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x61:0x21);
					if( !usbio_WriteReg( dev->fd, 0x03, regs[0x03]))
						return SANE_FALSE;

					_UIO(sanei_lm983x_write( dev->fd, 0x40,
					                        &regs[0x40], 3, SANE_TRUE));
					break;
				}

				if( _LM9831 != hw->chip )
					m_dwPixels = m_ScanParam.Size.dwPhyPixels;

				if( scaps->workaroundFlag & _WAF_SKIP_FINE ) {
					DBG( _DBG_INFO, "Skipping fine calibration\n" );
					regs[0x42] = (u_char)(( hw->wDRAMSize > 512)? 0x60: 0x20);
					if (scan->skipCoarseCalib) {

						DBG( _DBG_INFO, "...cleaning shading buffer\n" );
						memset( a_wWhiteShading, 0, _SHADING_BUF );
						memset( a_wDarkShading,  0, _SHADING_BUF );

						regs[0x40] = 0x3f;
						regs[0x41] = 0xff;

						_UIO(sanei_lm983x_write( dev->fd, 0x40,
						                        &regs[0x40], 3, SANE_TRUE));
					} else {
						if( !usbio_WriteReg( dev->fd, 0x42, regs[0x42]))
							return SANE_FALSE;
					}
					break;
				}

				DBG( _DBG_INFO, "Downloading %lu pixels\n", m_dwPixels );
				/* Download the dark & white shadings to LM983x */
				if( param->bDataType == SCANDATATYPE_Color ) {

					get_ptrs(dev, a_wDarkShading, m_dwPixels, &r, &g, &b);

					usb_SetDarkShading( dev, CHANNEL_red, r,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetDarkShading( dev, CHANNEL_green, g,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetDarkShading( dev, CHANNEL_blue, b,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				} else {
					usb_SetDarkShading( dev, channel,
					                    a_wDarkShading + m_dwPixels,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				}
				if( param->bDataType == SCANDATATYPE_Color ) {

					get_ptrs(dev, a_wWhiteShading, 
					         m_ScanParam.Size.dwPhyPixels, &r, &g, &b);

					usb_SetWhiteShading( dev, CHANNEL_red, r,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetWhiteShading( dev, CHANNEL_green, g,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
					usb_SetWhiteShading( dev, CHANNEL_blue, b,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				} else {
					usb_SetWhiteShading( dev, channel, a_wWhiteShading,
					                (u_short)m_ScanParam.Size.dwPhyPixels * 2);
				}

				/* set RAM configuration AND
				 * Gain = Multiplier Coefficient/16384
				 * External DRAM for Multiplier Coefficient Source
				 * External DRAM for Offset Coefficient Source
				 */
				regs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x66: 0x26);

				if( scaps->workaroundFlag & _WAF_SKIP_WHITEFINE ) {
					DBG( _DBG_INFO,"Skipping fine white calibration result\n");
					regs[0x42] = (u_char)(( hw->wDRAMSize > 512)? 0x64: 0x24);
				}

				if( !usbio_WriteReg( dev->fd, 0x42, regs[0x42]))
					return SANE_FALSE;
			}
			break;

		default:
			/* for coarse calibration and "black fine" */
			regs[0x3e] = 0;
			regs[0x3f] = 0;
			regs[0x40] = 0x40;
			regs[0x41] = 0x00;

			/* set RAM configuration AND
			 * GAIN = Multiplier Coefficient/16384
			 * CFG Register 0x40/0x41 for Multiplier Coefficient Source
			 * CFG Register 0x3e/0x3f for Offset Coefficient Source
			 */
			regs[0x42] = (u_char)((hw->wDRAMSize > 512)? 0x60: 0x20);
			_UIO(sanei_lm983x_write( dev->fd, 0x3e, &regs[0x3e],
			                         0x42 - 0x3e + 1, SANE_TRUE ));
			break;
	}
	return SANE_TRUE;
}

/* END PLUSTEK-USBSHADING.C .................................................*/
