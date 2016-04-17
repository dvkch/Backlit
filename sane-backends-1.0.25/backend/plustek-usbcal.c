/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners; canoscan calibration
 *.............................................................................
 */

/** @file plustek-usbcal.c
 *  @brief Calibration routines for CanoScan CIS devices.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2007 Gerhard Jaeger <gerhard@gjaeger.de><br>
 * Large parts Copyright (C) 2003 Christopher Montgomery <monty@xiph.org>
 *
 * Montys' comment:
 * The basic premise: The stock Plustek-usbshading.c in the plustek
 * driver is effectively nonfunctional for Canon CanoScan scanners.
 * These scanners rely heavily on all calibration steps, especially
 * fine white, to produce acceptible scan results.  However, to make
 * autocalibration work and make it work well involves some
 * substantial mucking aobut in code that supports thirty other
 * scanners with widely varying characteristics... none of which I own
 * or can test.
 *
 * Therefore, I'm splitting out a few calibration functions I need
 * to modify for the CanoScan which allows me to simplify things 
 * greatly for the CanoScan without worrying about breaking other
 * scanners, as well as reuse the vast majority of the Plustek
 * driver infrastructure without forking.
 *
 * History:
 * - 0.45m - birth of the file; tested extensively with the LiDE 20
 * - 0.46  - renamed to plustek-usbcal.c
 *         - fixed problems with LiDE30, works now with 650, 1220, 670, 1240
 *         - cleanup
 *         - added CCD calibration capability
 *         - added the usage of the swGain and swOffset values, to allow
 *           tweaking the calibration results on a sensor base
 * - 0.47  - moved usb_HostSwap() to plustek_usbhw.c
 *         - fixed problem in cano_AdjustLightsource(), so that it won't
 *           stop too early.
 * - 0.48  - cleanup
 * - 0.49  - a_bRegs is now part of the device structure
 *         - fixed lampsetting in cano_AdjustLightsource()
 * - 0.50  - tried to use the settings from SANE-1.0.13
 *         - added _TWEAK_GAIN to allow increasing GAIN during
 *           lamp coarse calibration
 *         - added also speedtest
 *         - fixed segfault in fine calibration
 * - 0.51  - added fine calibration cache
 *         - usb_SwitchLamp() now really switches off the sensor
 * - 0.52  - fixed setting for frontend values (gain/offset)
 *         - added 0 pixel detection for offset calculation
 *
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

/* un-/comment the following to en-/disable lamp coarse calibration to tweak
 * the initial AFE gain settings
 */
#define _TWEAK_GAIN 1

/* set the threshold for 0 pixels (in percent if pixels per line) */
#define _DARK_TGT_THRESH 1

/** 0 for not ready,  1 pos white lamp on,  2 lamp off */
static int strip_state = 0;

/** depending on the strip state, the sensor is moved to the shading position
 *  and the lamp ist switched on
 */
static int
cano_PrepareToReadWhiteCal( Plustek_Device *dev, SANE_Bool mv2shading_pos )
{
	SANE_Bool goto_shading_pos = SANE_TRUE;
	HWDef     *hw = &dev->usbDev.HwSetting;
	
	switch (strip_state) {
		case 0:
			if( !usb_IsSheetFedDevice(dev)) {
				if(!usb_ModuleToHome( dev, SANE_TRUE )) {
					DBG( _DBG_ERROR, "cano_PrepareToReadWhiteCal() failed\n" );
					return _E_LAMP_NOT_IN_POS;
				}
			} else {
				goto_shading_pos = mv2shading_pos;
			}

			if( goto_shading_pos ) {
				if( !usb_ModuleMove(dev, MOVE_Forward,
					(u_long)dev->usbDev.pSource->ShadingOriginY)) {
					DBG( _DBG_ERROR, "cano_PrepareToReadWhiteCal() failed\n" );
					return _E_LAMP_NOT_IN_POS;
				}
			}
	    	break;
		case 2:
			dev->usbDev.a_bRegs[0x29] = hw->bReg_0x29;
			usb_switchLamp( dev, SANE_TRUE );
			if( !usbio_WriteReg( dev->fd, 0x29, dev->usbDev.a_bRegs[0x29])) {
				DBG( _DBG_ERROR, "cano_PrepareToReadWhiteCal() failed\n" );
				return _E_LAMP_NOT_IN_POS;
			}
		break;
	}

	strip_state = 1;
	return 0;
}

/** also here, depending on the strip state, the sensor will be moved to
 * the shading position and the lamp will be switched off
 */
static int
cano_PrepareToReadBlackCal( Plustek_Device *dev )
{
	if( strip_state == 0 )
		if(cano_PrepareToReadWhiteCal(dev, SANE_FALSE))
			return SANE_FALSE;
			
	if( strip_state != 2 ) {
	    /*
		 * if we have a dark shading strip, there's no need to switch
	     * the lamp off, leave in on a go to that strip
		 */
		if( dev->usbDev.pSource->DarkShadOrgY >= 0 ) {

			if( !usb_IsSheetFedDevice(dev))
				usb_ModuleToHome( dev, SANE_TRUE );
			usb_ModuleMove  ( dev, MOVE_Forward,
			                       (u_long)dev->usbDev.pSource->DarkShadOrgY );
			dev->usbDev.a_bRegs[0x45] &= ~0x10;
			strip_state = 0;

		} else {
		 	/* switch lamp off to read dark data... */
			dev->usbDev.a_bRegs[0x29] = 0;
			usb_switchLamp( dev, SANE_FALSE );
			strip_state = 2;
		}
	}
	return 0;
}

/** according to the strip-state we switch the lamp on
 */
static int
cano_LampOnAfterCalibration( Plustek_Device *dev )
{
	HWDef *hw = &dev->usbDev.HwSetting;

	switch (strip_state) {
		case 2:
			dev->usbDev.a_bRegs[0x29] = hw->bReg_0x29;
			usb_switchLamp( dev, SANE_TRUE );
			if( !usbio_WriteReg( dev->fd, 0x29, dev->usbDev.a_bRegs[0x29])) {
				DBG( _DBG_ERROR, "cano_LampOnAfterCalibration() failed\n" );
				return _E_LAMP_NOT_IN_POS;
			}
			strip_state = 1;
			break;
	}
	return 0;
}

/** function to adjust the CIS lamp-off setting for a given channel.
 * @param min - pointer to the min OFF point for the CIS-channel
 * @param max - pointer to the max OFF point for the CIS-channel
 * @param off - pointer to the current OFF point of the CIS-channel
 * @param val - current value to check
 * @return returns 0 if the value is fine, 1, if we need to adjust 
 */
static int
cano_adjLampSetting( u_short *min, u_short *max, u_short *off, u_short val )
{
	u_long newoff = *off;

	/* perfect value, no need to adjust
	 * val  [53440..61440] is perfect
	 */
	if((val < (IDEAL_GainNormal)) && (val > (IDEAL_GainNormal-8000)))
		return 0;

	if(val >= (IDEAL_GainNormal-4000)) {
		DBG(_DBG_INFO2, "* TOO BRIGHT --> reduce\n" );
		*max   = newoff;
		*off = ((newoff + *min)>>1);

	} else {

		u_short bisect = (newoff + *max)>>1;
		u_short twice  =  newoff*2;

		DBG(_DBG_INFO2, "* TOO DARK --> up\n" );
		*min = newoff;
		*off = twice<bisect?twice:bisect;

		/* as we have already set the maximum value, there's no need
		 * for this channel to recalibrate.
		 */
		if( *off > 0x3FFF ) {
			DBG( _DBG_INFO, "* lamp off limited (0x%04x --> 0x3FFF)\n", *off);
			*off = 0x3FFF;
			return 10;
		}
	}
	if((*min+1) >= *max )
		return 0;

	return 1;
}

/** cano_AdjustLightsource
 * coarse calibration step 0
 * [Monty changes]: On the CanoScan at least, the default lamp
 * settings are several *hundred* percent too high and vary from
 * scanner-to-scanner by 20-50%. This is only for CIS devices 
 * where the lamp_off parameter is adjustable; I'd make it more general,
 * but I only have the CIS hardware to test.
 */
static int
cano_AdjustLightsource( Plustek_Device *dev )
{
	char         tmp[40];
	int          i;
	int          res_r, res_g, res_b;
	u_long       dw, dwR, dwG, dwB, dwDiv, dwLoop1, dwLoop2;
	RGBUShortDef max_rgb, min_rgb, tmp_rgb;
	u_long      *scanbuf = dev->scanning.pScanBuffer;
	DCapsDef    *scaps   = &dev->usbDev.Caps;
	HWDef       *hw      = &dev->usbDev.HwSetting;

	if( usb_IsEscPressed())
		return SANE_FALSE;

	DBG( _DBG_INFO, "cano_AdjustLightsource()\n" );

	if( !usb_IsCISDevice(dev)) {
		DBG( _DBG_INFO, "- function skipped, CCD device!\n" );

		/* HEINER: we might have to tweak the PWM for the lamps */
		return SANE_TRUE;
	}

	/* define the strip to scan for coarse calibration
	 * done at optical resolution.
	 */
	m_ScanParam.Size.dwLines  = 1;
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
	                            scaps->OpticDpi.x / 300UL;

	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2;

	if( m_ScanParam.bDataType == SCANDATATYPE_Color )
		m_ScanParam.Size.dwBytes *=3;

	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
	                                          300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Gain;

	DBG( _DBG_INFO2, "* Coarse Calibration Strip:\n" );
	DBG( _DBG_INFO2, "* Lines    = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "* Pixels   = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "* Bytes    = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "* Origin.X = %u\n",  m_ScanParam.Origin.x );

	/* init... */
	max_rgb.Red   = max_rgb.Green = max_rgb.Blue = 0x3fff;
	min_rgb.Red   = hw->red_lamp_on;
	min_rgb.Green = hw->green_lamp_on;
	min_rgb.Blue  = hw->blue_lamp_on;

	if((dev->adj.rlampoff != -1) && 
	   (dev->adj.glampoff != -1) && (dev->adj.rlampoff != -1)) {
		DBG( _DBG_INFO, "- function skipped, using frontend values!\n" );
		return SANE_TRUE;
	}

	/* we probably should preset gain to some reasonably good value
	 * i.e. 0x0a as it's done by Canon within their Windoze driver!
	 */
#ifdef _TWEAK_GAIN
	for( i=0x3b; i<0x3e; i++ )
		dev->usbDev.a_bRegs[i] = 0x0a;
#endif
	for( i = 0; ; i++ ) {

		m_ScanParam.dMCLK = dMCLK;
		if( !usb_SetScanParameters( dev, &m_ScanParam )) {
			return SANE_FALSE;
		}

		if( !usb_ScanBegin( dev, SANE_FALSE) ||
		    !usb_ScanReadImage( dev, scanbuf, m_ScanParam.Size.dwPhyBytes ) ||
		    !usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "* cano_AdjustLightsource() failed\n" );
			return SANE_FALSE;
		}

		DBG( _DBG_INFO2, "* PhyBytes  = %lu\n",  m_ScanParam.Size.dwPhyBytes );
		DBG( _DBG_INFO2, "* PhyPixels = %lu\n",  m_ScanParam.Size.dwPhyPixels);

		sprintf( tmp, "coarse-lamp-%u.raw", i );

		dumpPicInit(&m_ScanParam, tmp);
		dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwPhyBytes, 0);

		if(usb_HostSwap())
			usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwPhyBytes );

		sprintf( tmp, "coarse-lamp-swap%u.raw", i );

		dumpPicInit(&m_ScanParam, tmp);
		dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwPhyBytes, 0);

		dwDiv   = 10;
		dwLoop1 = m_ScanParam.Size.dwPhyPixels/dwDiv;

		tmp_rgb.Red = tmp_rgb.Green = tmp_rgb.Blue = 0;

		/* find out the max pixel value for R, G, B */
		for( dw = 0; dwLoop1; dwLoop1-- ) {

			/* do some averaging... */
			for (dwLoop2 = dwDiv, dwR = dwG = dwB = 0; dwLoop2; dwLoop2--, dw++) {

				if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

					if( usb_IsCISDevice(dev)) {
						dwR += ((u_short*)scanbuf)[dw];
						dwG += ((u_short*)scanbuf)
						       [dw+m_ScanParam.Size.dwPhyPixels+1];
						dwB += ((u_short*)scanbuf)
						       [dw+(m_ScanParam.Size.dwPhyPixels+1)*2];
            		} else {
						dwR += ((RGBUShortDef*)scanbuf)[dw].Red;
						dwG += ((RGBUShortDef*)scanbuf)[dw].Green;
						dwB += ((RGBUShortDef*)scanbuf)[dw].Blue;
					}
				} else {
					dwG += ((u_short*)scanbuf)[dw];
				}
			}

			dwR = dwR / dwDiv;
			dwG = dwG / dwDiv;
			dwB = dwB / dwDiv;

			if( tmp_rgb.Red < dwR )
				tmp_rgb.Red = dwR;
			if( tmp_rgb.Green < dwG )
				tmp_rgb.Green = dwG;
			if( tmp_rgb.Blue < dwB )
				tmp_rgb.Blue = dwB;
		}

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			DBG( _DBG_INFO2, "red_lamp_off  = %u/%u/%u\n",
			                  min_rgb.Red ,hw->red_lamp_off, max_rgb.Red );
		}
			
		DBG( _DBG_INFO2, "green_lamp_off = %u/%u/%u\n",
		                  min_rgb.Green, hw->green_lamp_off, max_rgb.Green );
							
		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			DBG( _DBG_INFO2, "blue_lamp_off = %u/%u/%u\n",
			                  min_rgb.Blue, hw->blue_lamp_off, max_rgb.Blue );
		}

		DBG(_DBG_INFO2, "CUR(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
		                     tmp_rgb.Red, tmp_rgb.Red, tmp_rgb.Green,
		                     tmp_rgb.Green, tmp_rgb.Blue, tmp_rgb.Blue );
		res_r = 0;
		res_g = 0;
		res_b = 0;

		/* bisect */
		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
			res_r = cano_adjLampSetting( &min_rgb.Red, &max_rgb.Red,
			                             &hw->red_lamp_off, tmp_rgb.Red );
			res_b = cano_adjLampSetting( &min_rgb.Blue, &max_rgb.Blue,
			                             &hw->blue_lamp_off,tmp_rgb.Blue );
		}

		res_g = cano_adjLampSetting( &min_rgb.Green, &max_rgb.Green,
		                             &hw->green_lamp_off, tmp_rgb.Green );

		/* nothing adjusted, so stop here */
		if((res_r == 0) && (res_g == 0) && (res_b == 0))
			break;

		/* no need to adjust more, we have already reached the limit
		 * without tweaking the gain.
		 */
		if((res_r == 10) && (res_g == 10) && (res_b == 10))
			break;

		/* we raise the gain for channels, that have been limited */
#ifdef _TWEAK_GAIN
		if( res_r == 10 ) {
			if( dev->usbDev.a_bRegs[0x3b] < 0xf)
				dev->usbDev.a_bRegs[0x3b]++;
		}
		if( res_g == 10 ) {
			if( dev->usbDev.a_bRegs[0x3c] < 0x0f)
				dev->usbDev.a_bRegs[0x3c]++;
		}
		if( res_b == 10 ) {
			if( dev->usbDev.a_bRegs[0x3d] < 0x0f)
				dev->usbDev.a_bRegs[0x3d]++;
		}
#endif

		/* now decide what to do:
		 * if we were too bright, we have to rerun the loop in any
		 * case
		 * if we're too dark, we should rerun it too, but we can
		 * compensate that using higher gain values later
		 */
		if( i >= 10 ) {
			DBG(_DBG_INFO, "* 10 times limit reached, still too dark!!!\n");
			break;
		}
		usb_AdjustLamps(dev, SANE_TRUE);
	}

	DBG( _DBG_INFO, "* red_lamp_on    = %u\n", hw->red_lamp_on  );
	DBG( _DBG_INFO, "* red_lamp_off   = %u\n", hw->red_lamp_off );
	DBG( _DBG_INFO, "* green_lamp_on  = %u\n", hw->green_lamp_on  );
	DBG( _DBG_INFO, "* green_lamp_off = %u\n", hw->green_lamp_off );
	DBG( _DBG_INFO, "* blue_lamp_on   = %u\n", hw->blue_lamp_on   );
	DBG( _DBG_INFO, "* blue_lamp_off  = %u\n", hw->blue_lamp_off  );

	DBG( _DBG_INFO, "cano_AdjustLightsource() done.\n" );
	return SANE_TRUE;
}

/**
 */
static int
cano_adjGainSetting( u_char *min, u_char *max, u_char *gain,u_long val )
{
	u_long newgain = *gain;

	if((val < IDEAL_GainNormal) && (val > (IDEAL_GainNormal-8000)))
		return 0;

	if(val > (IDEAL_GainNormal-4000)) {
		*max   = newgain;
		*gain  = (newgain + *min)>>1;
	} else {
		*min   = newgain;
		*gain  = (newgain + *max)>>1;
	}

	if((*min+1) >= *max)
		return 0;

	return 1;
}

/** cano_AdjustGain
 * function to perform the "coarse calibration step" part 1.
 * We scan reference image pixels to determine the optimum coarse gain settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * The scanned line should contain a white strip with some black at the
 * beginning. The function searches for the maximum value which corresponds to
 * the maximum white value.
 * Affects register 0x3b, 0x3c and 0x3d
 *
 * adjLightsource, above, steals most of this function's thunder.
 */
static SANE_Bool
cano_AdjustGain( Plustek_Device *dev )
{
	char      tmp[40];
	int       i = 0, adj = 1;
	u_long    dw;
	u_long   *scanbuf = dev->scanning.pScanBuffer;
	DCapsDef *scaps   = &dev->usbDev.Caps;
	HWDef    *hw      = &dev->usbDev.HwSetting;

	unsigned char max[3], min[3];

	if( usb_IsEscPressed())
		return SANE_FALSE;

	bMaxITA = 0xff;

	max[0] = max[1] = max[2] = 0x3f;
	min[0] = min[1] = min[2] = 1;

	DBG( _DBG_INFO, "cano_AdjustGain()\n" );
	if( !usb_InCalibrationMode(dev)) {
		if((dev->adj.rgain != -1) && 
		   (dev->adj.ggain != -1) && (dev->adj.bgain != -1)) {
			setAdjGain( dev->adj.rgain, &dev->usbDev.a_bRegs[0x3b] );
			setAdjGain( dev->adj.ggain, &dev->usbDev.a_bRegs[0x3c] );
			setAdjGain( dev->adj.bgain, &dev->usbDev.a_bRegs[0x3d] );
			DBG( _DBG_INFO, "- function skipped, using frontend values!\n" );
			return SANE_TRUE;
		}
	}

	/* define the strip to scan for coarse calibration
	 * done at 300dpi
	 */
	m_ScanParam.Size.dwLines  = 1;                       /* for gain */
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x *
	                                                 scaps->OpticDpi.x / 300UL;

	m_ScanParam.Size.dwBytes  = m_ScanParam.Size.dwPixels * 2;

	if( usb_IsCISDevice(dev) && m_ScanParam.bDataType == SCANDATATYPE_Color)
		m_ScanParam.Size.dwBytes *=3;

	m_ScanParam.Origin.x = (u_short)((u_long) hw->wActivePixelsStart *
	                                                300UL / scaps->OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Gain;

	DBG( _DBG_INFO2, "Coarse Calibration Strip:\n" );
	DBG( _DBG_INFO2, "Lines    = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels   = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes    = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X = %u\n",  m_ScanParam.Origin.x );
  
	while( adj ) {

		m_ScanParam.dMCLK = dMCLK;
 
		if( !usb_SetScanParameters( dev, &m_ScanParam ))
			return SANE_FALSE;

		if( !usb_ScanBegin( dev, SANE_FALSE) ||
		    !usb_ScanReadImage(dev,scanbuf,m_ScanParam.Size.dwPhyBytes) ||
		    !usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "cano_AdjustGain() failed\n" );
			return SANE_FALSE;
		}

		DBG( _DBG_INFO2, "PhyBytes  = %lu\n",  m_ScanParam.Size.dwPhyBytes  );
		DBG( _DBG_INFO2, "PhyPixels = %lu\n",  m_ScanParam.Size.dwPhyPixels );

		sprintf( tmp, "coarse-gain-%u.raw", i++ );

		dumpPicInit(&m_ScanParam, tmp);
		dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwPhyBytes, 0);

		if(usb_HostSwap())
			usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwPhyBytes );

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			RGBUShortDef max_rgb;
			u_long       dwR, dwG, dwB;
			u_long       dwDiv = 10;
			u_long       dwLoop1 = m_ScanParam.Size.dwPhyPixels/dwDiv, dwLoop2;

			max_rgb.Red = max_rgb.Green = max_rgb.Blue = 0;

			/* find out the max pixel value for R, G, B */
			for( dw = 0; dwLoop1; dwLoop1-- ) {

				/* do some averaging... */
				for (dwLoop2 = dwDiv, dwR=dwG=dwB=0; dwLoop2; dwLoop2--, dw++) {

					if( usb_IsCISDevice(dev)) {
						dwR += ((u_short*)scanbuf)[dw];
						dwG += ((u_short*)scanbuf)
						       [dw+m_ScanParam.Size.dwPhyPixels+1];
						dwB += ((u_short*)scanbuf)
						       [dw+(m_ScanParam.Size.dwPhyPixels+1)*2];
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
			}

			DBG(_DBG_INFO2, "MAX(R,G,B)= 0x%04x(%u), 0x%04x(%u), 0x%04x(%u)\n",
			                 max_rgb.Red, max_rgb.Red, max_rgb.Green,
			                 max_rgb.Green, max_rgb.Blue, max_rgb.Blue );

			adj  = cano_adjGainSetting(min  , max  ,dev->usbDev.a_bRegs+0x3b,max_rgb.Red  );
			adj += cano_adjGainSetting(min+1, max+1,dev->usbDev.a_bRegs+0x3c,max_rgb.Green);
			adj += cano_adjGainSetting(min+2, max+2,dev->usbDev.a_bRegs+0x3d,max_rgb.Blue );

	    } else {

			u_short w_max = 0;

			for( dw = 0; dw < m_ScanParam.Size.dwPhyPixels; dw++ ) {
				if( w_max < ((u_short*)scanbuf)[dw])
					w_max = ((u_short*)scanbuf)[dw];
			}

			adj = cano_adjGainSetting(min,max,dev->usbDev.a_bRegs+0x3c,w_max);
			dev->usbDev.a_bRegs[0x3b] = (dev->usbDev.a_bRegs[0x3d] = dev->usbDev.a_bRegs[0x3c]);

			DBG(_DBG_INFO2, "MAX(G)= 0x%04x(%u)\n", w_max, w_max );

		}
		DBG( _DBG_INFO2, "REG[0x3b] = %u\n", dev->usbDev.a_bRegs[0x3b] );
		DBG( _DBG_INFO2, "REG[0x3c] = %u\n", dev->usbDev.a_bRegs[0x3c] );
		DBG( _DBG_INFO2, "REG[0x3d] = %u\n", dev->usbDev.a_bRegs[0x3d] );
	}
	DBG( _DBG_INFO, "cano_AdjustGain() done.\n" );
	return SANE_TRUE;
}

static int tweak_offset[3];

/**
 */
static int
cano_GetNewOffset(Plustek_Device *dev, u_long *val, int channel, signed char *low,
                  signed char *now, signed char *high, u_long *zc)
{
	DCapsDef *scaps = &dev->usbDev.Caps;

	if (tweak_offset[channel]) {

		/* if we're too black, we're likely off the low end */
		if( val[channel] <= 16 ) {
			low[channel] =  now[channel];
			now[channel] = (now[channel]+high[channel])/2;

			dev->usbDev.a_bRegs[0x38+channel]= (now[channel]&0x3f);

			if( low[channel]+1 >= high[channel] )
				return 0;
			return 1;

		} else if ( val[channel]>=2048 ) {
			high[channel]=now[channel];
			now[channel]=(now[channel]+low[channel])/2;

			dev->usbDev.a_bRegs[0x38+channel]= (now[channel]&0x3f);

			if(low[channel]+1>=high[channel])
				return 0;
			return 1;
		}
	}

	if (!(scaps->workaroundFlag & _WAF_INC_DARKTGT)) {
		DBG( _DBG_INFO, "0 Pixel adjustment not active!\n");
		return 0;
	}

	/* reaching this point, our black level should be okay, but
	 * we also should check the percentage of 0 level pixels.
	 * It turned out, that when having a lot of 0 level pixels,
	 * the calibration will be bad and the resulting scans show up
	 * stripes...
	 */
	if (zc[channel] > _DARK_TGT_THRESH) {
		DBG( _DBG_INFO2, "More than %u%% 0 pixels detected, raise offset!\n",
		                 _DARK_TGT_THRESH);
		high[channel]=now[channel];
		now[channel]=(now[channel]+low[channel])/2;

		/* no more value checks, the goal to set the black level < 2048
		 * will cause stripes...
		 */
		tweak_offset[channel] = 0;

		dev->usbDev.a_bRegs[0x38+channel]= (now[channel]&0x3f);

		if( low[channel]+1 >= high[channel] )
			return 0;
		return 1;

	}
#if 0
	else if ( val[channel]>=4096 ) {
		low[channel] =  now[channel];
		now[channel] = (now[channel]+high[channel])/2;

		dev->usbDev.a_bRegs[0x38+channel]= (now[channel]&0x3f);

		if( low[channel]+1 >= high[channel] )
			return 0;
		return 1;
	}
#endif
	return 0;
}

/** cano_AdjustOffset
 * function to perform the "coarse calibration step" part 2.
 * We scan reference image pixels to determine the optimum coarse offset settings
 * for R, G, B. (Analog gain and offset prior to ADC). These coefficients are
 * applied at the line rate during normal scanning.
 * On CIS based devices, we switch the light off, on CCD devices, we use the optical
 * black pixels.
 * Affects register 0x38, 0x39 and 0x3a
 */

/* Move this to a bisection-based algo and correct some fenceposts;
   Plustek's example code disagrees with NatSemi's docs; going by the
   docs works better, I will assume the docs are correct. --Monty */

static int
cano_AdjustOffset( Plustek_Device *dev )
{
	char    tmp[40];
	int     i, adj;
	u_short r, g, b;
	u_long  dw, dwPixels;
	u_long  dwSum[3], zCount[3];

	signed char low[3]  = {-32,-32,-32 };
	signed char now[3]  = {  0,  0,  0 };
	signed char high[3] = { 31, 31, 31 };

	u_long   *scanbuf = dev->scanning.pScanBuffer;
	HWDef    *hw      = &dev->usbDev.HwSetting;
	DCapsDef *scaps   = &dev->usbDev.Caps;
	
	if( usb_IsEscPressed())
		return SANE_FALSE;

	DBG( _DBG_INFO, "cano_AdjustOffset()\n" );
	if( !usb_InCalibrationMode(dev)) {
		if((dev->adj.rofs != -1) &&
		   (dev->adj.gofs != -1) && (dev->adj.bofs != -1)) {
			dev->usbDev.a_bRegs[0x38] = (dev->adj.rofs & 0x3f);
			dev->usbDev.a_bRegs[0x39] = (dev->adj.gofs & 0x3f);
			dev->usbDev.a_bRegs[0x3a] = (dev->adj.bofs & 0x3f);
			DBG( _DBG_INFO, "- function skipped, using frontend values!\n" );
			return SANE_TRUE;
		}
	}

	m_ScanParam.Size.dwLines  = 1;
	m_ScanParam.Size.dwPixels = scaps->Normal.Size.x*scaps->OpticDpi.x/300UL;

	if( usb_IsCISDevice(dev))
		dwPixels = m_ScanParam.Size.dwPixels;
	else
		dwPixels = (u_long)(hw->bOpticBlackEnd - hw->bOpticBlackStart);

	m_ScanParam.Size.dwBytes = m_ScanParam.Size.dwPixels * 2;

	if( usb_IsCISDevice(dev) && m_ScanParam.bDataType == SCANDATATYPE_Color)
		m_ScanParam.Size.dwBytes *= 3;

	m_ScanParam.Origin.x = (u_short)((u_long)hw->bOpticBlackStart * 300UL /
	                                              dev->usbDev.Caps.OpticDpi.x);
	m_ScanParam.bCalibration = PARAM_Offset;
	m_ScanParam.dMCLK        = dMCLK;

	if( !usb_SetScanParameters( dev, &m_ScanParam )) {
		DBG( _DBG_ERROR, "cano_AdjustOffset() failed\n" );
		return SANE_FALSE;
	}

	DBG( _DBG_INFO2, "S.dwPixels  = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "dwPixels    = %lu\n", dwPixels );
	DBG( _DBG_INFO2, "dwPhyBytes  = %lu\n", m_ScanParam.Size.dwPhyBytes );
	DBG( _DBG_INFO2, "dwPhyPixels = %lu\n", m_ScanParam.Size.dwPhyPixels );

	tweak_offset[0] =
	tweak_offset[1] =
	tweak_offset[2] = 1;

	for( i = 0, adj = 1; adj != 0; i++ ) {

		if((!usb_ScanBegin(dev, SANE_FALSE)) ||
			(!usb_ScanReadImage(dev,scanbuf,m_ScanParam.Size.dwPhyBytes)) ||
			!usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "cano_AdjustOffset() failed\n" );
			return SANE_FALSE;
		}

		sprintf( tmp, "coarse-off-%u.raw", i );

		dumpPicInit(&m_ScanParam, tmp);
		dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwPhyBytes, 0);

		if(usb_HostSwap())
			usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwPhyBytes );

		if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {

			dwSum[0] = dwSum[1] = dwSum[2] = 0;
			zCount[0] = zCount[1] = zCount[2] = 0;

			for (dw = 0; dw < dwPixels; dw++) {

				if( usb_IsCISDevice(dev)) {

					r = ((u_short*)scanbuf)[dw];
					g = ((u_short*)scanbuf)[dw+m_ScanParam.Size.dwPhyPixels+1];
					b = ((u_short*)scanbuf)[dw+(m_ScanParam.Size.dwPhyPixels+1)*2];

				} else {
					r = ((RGBUShortDef*)scanbuf)[dw].Red;
					g = ((RGBUShortDef*)scanbuf)[dw].Green;
					b = ((RGBUShortDef*)scanbuf)[dw].Blue;
				}

				dwSum[0] += r;
				dwSum[1] += g;
				dwSum[2] += b;

				if (r==0) zCount[0]++;
				if (g==0) zCount[1]++;
				if (b==0) zCount[2]++;
			}

			DBG( _DBG_INFO2, "RedSum   = %lu, ave = %lu, ZC=%lu, %lu%%\n",
			                        dwSum[0], dwSum[0]/dwPixels,
			                        zCount[0], (zCount[0]*100)/dwPixels);
			DBG( _DBG_INFO2, "GreenSum = %lu, ave = %lu, ZC=%lu, %lu%%\n",
			                        dwSum[1], dwSum[1]/dwPixels,
			                        zCount[1], (zCount[1]*100)/dwPixels);
			DBG( _DBG_INFO2, "BlueSum  = %lu, ave = %lu, ZC=%lu, %lu%%\n",
			                        dwSum[2], dwSum[2]/dwPixels,
			                        zCount[2], (zCount[2]*100)/dwPixels);

			/* do averaging for each channel */
			dwSum[0] /= dwPixels;
			dwSum[1] /= dwPixels;
			dwSum[2] /= dwPixels;

			zCount[0] = (zCount[0] * 100)/ dwPixels;
			zCount[1] = (zCount[1] * 100)/ dwPixels;
			zCount[2] = (zCount[2] * 100)/ dwPixels;

			adj  = cano_GetNewOffset(dev, dwSum, 0, low, now, high, zCount);
			adj |= cano_GetNewOffset(dev, dwSum, 1, low, now, high, zCount);
			adj |= cano_GetNewOffset(dev, dwSum, 2, low, now, high, zCount);

			DBG( _DBG_INFO2, "RedOff   = %d/%d/%d\n",
			                            (int)low[0],(int)now[0],(int)high[0]);
			DBG( _DBG_INFO2, "GreenOff = %d/%d/%d\n",
			                            (int)low[1],(int)now[1],(int)high[1]);
			DBG( _DBG_INFO2, "BlueOff  = %d/%d/%d\n",
			                            (int)low[2],(int)now[2],(int)high[2]);

		} else {
			dwSum[0] = 0;
			zCount[0] = 0;

			for( dw = 0; dw < dwPixels; dw++ ) {
				dwSum[0] += ((u_short*)scanbuf)[dw];

				if (((u_short*)scanbuf)[dw] == 0)
					zCount[0]++;
			}

			DBG( _DBG_INFO2, "Sum=%lu, ave=%lu, ZC=%lu, %lu%%\n",
			                  dwSum[0],dwSum[0]/dwPixels,
			                  zCount[0], (zCount[0]*100)/dwPixels);

			dwSum[0] /= dwPixels;
			zCount[0] = (zCount[0] * 100)/ dwPixels;

			adj = cano_GetNewOffset(dev, dwSum, 0, low, now, high, zCount);

			dev->usbDev.a_bRegs[0x3a] =
			dev->usbDev.a_bRegs[0x39] = dev->usbDev.a_bRegs[0x38];

			DBG( _DBG_INFO2, "GrayOff = %d/%d/%d\n",
			                             (int)low[0],(int)now[0],(int)high[0]);
		}

		DBG( _DBG_INFO2, "REG[0x38] = %u\n", dev->usbDev.a_bRegs[0x38] );
		DBG( _DBG_INFO2, "REG[0x39] = %u\n", dev->usbDev.a_bRegs[0x39] );
		DBG( _DBG_INFO2, "REG[0x3a] = %u\n", dev->usbDev.a_bRegs[0x3a] );

		_UIO(sanei_lm983x_write(dev->fd, 0x38, &dev->usbDev.a_bRegs[0x38], 3, SANE_TRUE));
	}

	/* is that really needed?! */
	if( m_ScanParam.bDataType == SCANDATATYPE_Color ) {
		dev->usbDev.a_bRegs[0x38] = now[0] & 0x3f;
		dev->usbDev.a_bRegs[0x39] = now[1] & 0x3f;
		dev->usbDev.a_bRegs[0x3a] = now[2] & 0x3f;
	} else {
		dev->usbDev.a_bRegs[0x38] =
		dev->usbDev.a_bRegs[0x39] =
		dev->usbDev.a_bRegs[0x3a] = now[0] & 0x3f;
	}

	DBG( _DBG_INFO, "cano_AdjustOffset() done.\n" );
	return SANE_TRUE;
}

/** usb_AdjustDarkShading
 * fine calibration part 1
 */
static SANE_Bool
cano_AdjustDarkShading( Plustek_Device *dev, u_short cal_dpi )
{
	char         tmp[40];
	ScanParam   *param   = &dev->scanning.sParam;
	ScanDef     *scan    = &dev->scanning;
	u_long      *scanbuf = scan->pScanBuffer;
	u_short     *bufp;
	unsigned int i, j;
	int          step, stepW, val;
	u_long       red, green, blue, gray;

	DBG( _DBG_INFO, "cano_AdjustDarkShading()\n" );
	if( usb_IsEscPressed())
		return SANE_FALSE;

	usb_PrepareFineCal( dev, &m_ScanParam, cal_dpi );
	m_ScanParam.bCalibration = PARAM_DarkShading;

	sprintf( tmp, "fine-dark.raw" );
	dumpPicInit(&m_ScanParam, tmp);

	usb_SetScanParameters( dev, &m_ScanParam );
	if( usb_ScanBegin( dev, SANE_FALSE ) &&
	    usb_ScanReadImage( dev, scanbuf, m_ScanParam.Size.dwTotalBytes)) {

		dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwTotalBytes, 0);

		if(usb_HostSwap())
			usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwTotalBytes);
	}
	if (!usb_ScanEnd( dev )){
		DBG( _DBG_ERROR, "cano_AdjustDarkShading() failed\n" );
		return SANE_FALSE;
	}

	/* average the n lines, compute reg values */
	if( scan->sParam.bDataType == SCANDATATYPE_Color ) {
		
		stepW = m_ScanParam.Size.dwPhyPixels;
		if( usb_IsCISDevice(dev))
			step = m_ScanParam.Size.dwPhyPixels + 1;
		else
			step = (m_ScanParam.Size.dwPhyPixels*3) + 1;
		
		for( i=0; i<m_ScanParam.Size.dwPhyPixels; i++ ) {

			red   = 0;
			green = 0;
			blue  = 0;
			if( usb_IsCISDevice(dev))
				bufp = ((u_short *)scanbuf)+i;
			else
				bufp = ((u_short *)scanbuf)+(i*3);

			for( j=0; j<m_ScanParam.Size.dwPhyLines; j++ ) {

				if( usb_IsCISDevice(dev)) {
					red   += *bufp; bufp+=step;
					green += *bufp; bufp+=step;
					blue  += *bufp; bufp+=step;
				} else {

					red   += bufp[0];
					green += bufp[1];
					blue  += bufp[2];

					bufp += step;
				}
			}

			val = ((int)(red/m_ScanParam.Size.dwPhyLines) + param->swOffset[0]);
			if( val < 0 ) {
				DBG( _DBG_INFO, "val < 0!!!!\n" );
				val = 0;
			}
			a_wDarkShading[i] = (u_short)val;

			val = ((int)(green/m_ScanParam.Size.dwPhyLines) + param->swOffset[1]);
			if( val < 0 ) {
				DBG( _DBG_INFO, "val < 0!!!!\n" );
				val = 0;
			}
			a_wDarkShading[i+stepW] = (u_short)val;

			val = ((int)(blue/m_ScanParam.Size.dwPhyLines) + param->swOffset[2]);
			if( val < 0 ) {
				DBG( _DBG_INFO, "val < 0!!!!\n" );
				val = 0;
			}
			a_wDarkShading[i+stepW*2] = (u_short)val;
		}

	} else {

		step = m_ScanParam.Size.dwPhyPixels + 1;
		for( i=0; i<m_ScanParam.Size.dwPhyPixels; i++ ) {
			
			gray = 0;
			bufp = ((u_short *)scanbuf)+i;

			for( j=0; j < m_ScanParam.Size.dwPhyLines; j++ ) {
				gray += *bufp;
				bufp += step;
			}
			a_wDarkShading[i]= gray/j + param->swOffset[0];
		}

		memcpy( a_wDarkShading + m_ScanParam.Size.dwPhyPixels,
		        a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2);
		memcpy( a_wDarkShading + m_ScanParam.Size.dwPhyPixels * 2,
		        a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2);
	}

	if(usb_HostSwap())
		usb_Swap(a_wDarkShading, m_ScanParam.Size.dwPhyPixels * 2 * 3);

	usb_line_statistics( "Dark", a_wDarkShading, m_ScanParam.Size.dwPhyPixels,
	                     scan->sParam.bDataType == SCANDATATYPE_Color?1:0);

	DBG( _DBG_INFO, "cano_AdjustDarkShading() done\n" );
	return SANE_TRUE;
}

/** usb_AdjustWhiteShading
 * fine calibration part 2 - read the white calibration area and calculate
 * the gain coefficient for each pixel
 */
static SANE_Bool
cano_AdjustWhiteShading( Plustek_Device *dev, u_short cal_dpi )
{
	char         tmp[40];
	ScanParam   *param   = &dev->scanning.sParam;
	ScanDef     *scan    = &dev->scanning;
	u_long      *scanbuf = scan->pScanBuffer;
	u_short     *bufp;
	unsigned int i, j;
	int          step, stepW;
	u_long       red, green, blue, gray;

	DBG( _DBG_INFO, "cano_AdjustWhiteShading()\n" );
	if( usb_IsEscPressed())
		return SANE_FALSE;

	usb_PrepareFineCal( dev, &m_ScanParam, cal_dpi );
	m_ScanParam.bCalibration = PARAM_WhiteShading;

	sprintf( tmp, "fine-white.raw" );
	DBG( _DBG_INFO2, "FINE WHITE Calibration Strip: %s\n", tmp );
	DBG( _DBG_INFO2, "Lines       = %lu\n", m_ScanParam.Size.dwLines  );
	DBG( _DBG_INFO2, "Pixels      = %lu\n", m_ScanParam.Size.dwPixels );
	DBG( _DBG_INFO2, "Bytes       = %lu\n", m_ScanParam.Size.dwBytes  );
	DBG( _DBG_INFO2, "Origin.X    = %u\n",  m_ScanParam.Origin.x );
	dumpPicInit(&m_ScanParam, tmp);

	if( usb_SetScanParameters( dev, &m_ScanParam ) &&
	    usb_ScanBegin( dev, SANE_FALSE ) &&
	    usb_ScanReadImage( dev, scanbuf, m_ScanParam.Size.dwTotalBytes)) {

		dumpPic(tmp, (u_char*)scanbuf, m_ScanParam.Size.dwTotalBytes, 0);

		if(usb_HostSwap())
			usb_Swap((u_short *)scanbuf, m_ScanParam.Size.dwTotalBytes);

		if (!usb_ScanEnd( dev )) {
			DBG( _DBG_ERROR, "cano_AdjustWhiteShading() failed\n" );
			return SANE_FALSE;
		}
	} else {
		DBG( _DBG_ERROR, "cano_AdjustWhiteShading() failed\n" );
		return SANE_FALSE;
	}

	/* average the n lines, compute reg values */
	if( scan->sParam.bDataType == SCANDATATYPE_Color ) {

		stepW = m_ScanParam.Size.dwPhyPixels;
		if( usb_IsCISDevice(dev))
			step = m_ScanParam.Size.dwPhyPixels + 1;
		else
			step = (m_ScanParam.Size.dwPhyPixels*3) + 1;

		for( i=0; i < m_ScanParam.Size.dwPhyPixels; i++ ) {

			red   = 0;
			green = 0;
			blue  = 0;
			if( usb_IsCISDevice(dev))
				bufp = ((u_short *)scanbuf)+i;
			else
				bufp = ((u_short *)scanbuf)+(i*3);

			for( j=0; j<m_ScanParam.Size.dwPhyLines; j++ ) {

				if( usb_IsCISDevice(dev)) {
					red   += *bufp; bufp+=step;
					green += *bufp; bufp+=step;
					blue  += *bufp; bufp+=step;
				} else {
					red   += bufp[0];
					green += bufp[1];
					blue  += bufp[2];
					bufp  += step;
				}
			}

			/* tweaked by the settings in swGain --> 1000/swGain[r,g,b] */
			red   = (65535.*1000./(double)param->swGain[0]) * 16384.*j/red;
			green = (65535.*1000./(double)param->swGain[1]) * 16384.*j/green;
			blue  = (65535.*1000./(double)param->swGain[2]) * 16384.*j/blue;

			a_wWhiteShading[i]         = (red   > 65535 ? 65535:red  );
			a_wWhiteShading[i+stepW]   = (green > 65535 ? 65535:green);
			a_wWhiteShading[i+stepW*2] = (blue  > 65535 ? 65535:blue );
		}

	} else {

		step = m_ScanParam.Size.dwPhyPixels + 1;
		for( i=0; i<m_ScanParam.Size.dwPhyPixels; i++ ){
			gray = 0;
			bufp = ((u_short *)scanbuf)+i;

			for( j=0; j<m_ScanParam.Size.dwPhyLines; j++ ) {
				gray += *bufp;
				bufp += step;
			}

			gray = (65535.*1000./(double)param->swGain[0]) * 16384.*j/gray;

			a_wWhiteShading[i]= (gray > 65535 ? 65535:gray);
		}

		memcpy( a_wWhiteShading + m_ScanParam.Size.dwPhyPixels,
		        a_wWhiteShading, m_ScanParam.Size.dwPhyPixels * 2);
		memcpy( a_wWhiteShading + m_ScanParam.Size.dwPhyPixels * 2,
		        a_wWhiteShading, m_ScanParam.Size.dwPhyPixels * 2);
	}

	if(usb_HostSwap())
		usb_Swap(a_wWhiteShading, m_ScanParam.Size.dwPhyPixels * 2 * 3 );

	usb_SaveCalSetShading( dev, &m_ScanParam );

	usb_line_statistics( "White", a_wWhiteShading, m_ScanParam.Size.dwPhyPixels,
	                     scan->sParam.bDataType == SCANDATATYPE_Color?1:0);

	DBG( _DBG_INFO, "cano_AdjustWhiteShading() done\n" );
	return SANE_TRUE;
}

/** the entry function for the CIS calibration stuff.
 */
static int
cano_DoCalibration( Plustek_Device *dev )
{
	u_short   dpi, idx, idx_end;
	u_long    save_waf;
	SANE_Bool skip_fine;
	ScanDef  *scan  = &dev->scanning;
	HWDef    *hw    = &dev->usbDev.HwSetting;
	DCapsDef *scaps = &dev->usbDev.Caps;

	if( SANE_TRUE == scan->fCalibrated )
		return SANE_TRUE;

	DBG( _DBG_INFO, "cano_DoCalibration()\n" );

	if( _IS_PLUSTEKMOTOR(hw->motorModel)){
		DBG( _DBG_ERROR, "altCalibration can't work with this "
		                 "Plustek motor control setup\n" );
		return SANE_FALSE; /* can't cal this  */
	}

	/* Don't allow calibration settings from the other driver to confuse our
	 * use of a few of its functions.
	 */
	save_waf = scaps->workaroundFlag;
	scaps->workaroundFlag &= ~_WAF_SKIP_WHITEFINE;
	scaps->workaroundFlag &= ~_WAF_SKIP_FINE;
	scaps->workaroundFlag &= ~_WAF_BYPASS_CALIBRATION;

	if( !dev->adj.cacheCalData && !usb_IsSheetFedDevice(dev))
		usb_SpeedTest( dev );

	/* here we handle that warmup stuff for CCD devices */
	if( !usb_AutoWarmup( dev ))
		return SANE_FALSE;

	/* Set the shading position to undefined */
	strip_state = 0;
	usb_PrepareCalibration( dev );

	usb_SetMCLK( dev, &scan->sParam );

	if( !scan->skipCoarseCalib ) {

		if( !usb_Wait4ScanSample( dev ))
			return SANE_FALSE;

		DBG( _DBG_INFO2, "###### ADJUST LAMP (COARSE)#######\n" );
		if( cano_PrepareToReadWhiteCal(dev, SANE_TRUE))
			return SANE_FALSE;

		dev->usbDev.a_bRegs[0x45] &= ~0x10;
		if( !cano_AdjustLightsource(dev)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return SANE_FALSE;
		}

		DBG( _DBG_INFO2, "###### ADJUST OFFSET (COARSE) ####\n" );
		if(cano_PrepareToReadBlackCal(dev))
			return SANE_FALSE;
		
		if( !cano_AdjustOffset(dev)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return SANE_FALSE;
		}

		DBG( _DBG_INFO2, "###### ADJUST GAIN (COARSE)#######\n" );
		if(cano_PrepareToReadWhiteCal(dev, SANE_FALSE))
			return SANE_FALSE;
		
		if( !cano_AdjustGain(dev)) {
			DBG( _DBG_ERROR, "Coarse Calibration failed!!!\n" );
			return SANE_FALSE;
		}
	} else {
		strip_state = 1;
		DBG( _DBG_INFO2, "###### COARSE calibration skipped #######\n" );
	}

	skip_fine = SANE_FALSE;
	idx_end   = 2;
	if( dev->adj.cacheCalData || usb_IsSheetFedDevice(dev)) {

		skip_fine = usb_FineShadingFromFile(dev);

		/* we recalibrate in any case ! */
		if( usb_InCalibrationMode(dev)) {
			skip_fine = SANE_FALSE;
			idx_end   = DIVIDER+1;

			/* did I say any case? */
			if (scan->sParam.bBitDepth != 8) {
				skip_fine = SANE_TRUE;
				DBG( _DBG_INFO2, "No fine calibration for non-8bit modes!\n" );
			}

		} else if( usb_IsSheetFedDevice(dev)) {

			/* we only do the calibration upon request !*/
			if( !skip_fine ) {
				DBG( _DBG_INFO2, "SHEET-FED device, skip fine calibration!\n" );
				skip_fine = SANE_TRUE;
				scaps->workaroundFlag |= _WAF_BYPASS_CALIBRATION;
			}
		}
	}

	if( !skip_fine ) {

		for( idx = 1; idx < idx_end; idx++ ) {

			dpi = 0;
			if( usb_InCalibrationMode(dev)) {
				dpi = usb_get_res( scaps->OpticDpi.x, idx );

				/* we might should check against device specific limit */
				if(dpi < 50)
					continue;
			}

			DBG( _DBG_INFO2, "###### ADJUST DARK (FINE) ########\n" );
			if(cano_PrepareToReadBlackCal(dev))
				return SANE_FALSE;

			dev->usbDev.a_bRegs[0x45] |= 0x10;
			if( !cano_AdjustDarkShading(dev, dpi)) {
				DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
				return SANE_FALSE;
			}

			DBG( _DBG_INFO2, "###### ADJUST WHITE (FINE) #######\n" );
			if(cano_PrepareToReadWhiteCal(dev, SANE_FALSE))
				return SANE_FALSE;

			if( !usb_IsSheetFedDevice(dev)) {
				if(!usb_ModuleToHome( dev, SANE_TRUE ))
					return SANE_FALSE;

				if( !usb_ModuleMove(dev, MOVE_Forward,
					(u_long)dev->usbDev.pSource->ShadingOriginY)) {
					return SANE_FALSE;
				}
			}
			if( !cano_AdjustWhiteShading(dev, dpi)) {
				DBG( _DBG_ERROR, "Fine Calibration failed!!!\n" );
				return SANE_FALSE;
			}

			/* force to go back */
			strip_state = 0;
		}
	} else {
		DBG( _DBG_INFO2, "###### FINE calibration skipped #######\n" );

		dev->usbDev.a_bRegs[0x45] |= 0x10;
		strip_state = 2;

		m_ScanParam = scan->sParam;
		usb_GetPhyPixels( dev, &m_ScanParam );

		usb_line_statistics( "Dark", a_wDarkShading, m_ScanParam.Size.dwPhyPixels,
		                      m_ScanParam.bDataType == SCANDATATYPE_Color?1:0);
		usb_line_statistics( "White", a_wWhiteShading, m_ScanParam.Size.dwPhyPixels,
		                      m_ScanParam.bDataType == SCANDATATYPE_Color?1:0);
	}

	/* Lamp on if it's not */
	cano_LampOnAfterCalibration(dev);
	strip_state = 0;

	/* home the sensor after calibration
	 */
	if( !usb_IsSheetFedDevice(dev))
		usb_ModuleToHome( dev, SANE_TRUE );
	scan->fCalibrated = SANE_TRUE;
	
	DBG( _DBG_INFO, "cano_DoCalibration() done\n" );
	DBG( _DBG_INFO, "-------------------------\n" );
	DBG( _DBG_INFO, "Static Gain:\n" );
	DBG( _DBG_INFO, "REG[0x3b] = %u\n", dev->usbDev.a_bRegs[0x3b] );
	DBG( _DBG_INFO, "REG[0x3c] = %u\n", dev->usbDev.a_bRegs[0x3c] );
	DBG( _DBG_INFO, "REG[0x3d] = %u\n", dev->usbDev.a_bRegs[0x3d] );
	DBG( _DBG_INFO, "Static Offset:\n" );
	DBG( _DBG_INFO, "REG[0x38] = %i\n", dev->usbDev.a_bRegs[0x38] );
	DBG( _DBG_INFO, "REG[0x39] = %i\n", dev->usbDev.a_bRegs[0x39] );
	DBG( _DBG_INFO, "REG[0x3a] = %i\n", dev->usbDev.a_bRegs[0x3a] );
	DBG( _DBG_INFO, "-------------------------\n" );

	scaps->workaroundFlag |= save_waf;

	return SANE_TRUE;
}

/* END PLUSTEK-USBCAL.C .....................................................*/
