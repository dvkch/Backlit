/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbhw.c
 *  @brief Functions to control the scanner hardware.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - added EPSON1250 specific stuff
 *        - added alternative usb_IsScannerReady function
 * - 0.42 - added warmup stuff
 *        - added UMAX 3400 stuff
 *        - fixed problem with minimum wait time...
 * - 0.43 - added usb_switchLamp for non-Plustek devices
 * - 0.44 - added bStepsToReverse and active Pixelstart values
 *        - to resetRegister function
 *        - modified getLampStatus function for CIS devices
 *        - added usb_Wait4Warmup()
 *        - moved usb_IsEscPressed to this file
 *        - added usb_switchLampX
 *        - do now not reinitialized MISC I/O pins upon reset registers
 * - 0.45 - added function usb_AdjustLamps() to tweak CIS lamp settings
 *        - fixed NULL pointer problem in lamp-off ISR
 *        - added usb_AdjustCISLampSettings()
 *        - skipping warmup for CIS devices 
 * - 0.46 - fixed problem in usb_GetLampStatus for CIS devices, as we
 *          read back reg[0x29] to wrong position
 *          made it compile without itimer definitions
 * - 0.47 - moved usb_HostSwap() and usb_Swap() to this file.
 *        - fixed lampOff timer for systems w/o setitimer
 *        - added lamp off adjustment for CIS devices
 * - 0.48 - added usb_IsCISDevice()
 *        - added usb_HasTPA()
 *        - changed usb_Wait4Warmup()
 *        - added usb_WaitPos()
 *        - added usb_FillLampRegs() - sets also PWMDutyCylce now
 *        - added UMAX3450 TPA autodetection
 * - 0.49 - a_bRegs is now part of the device structure
 *        - fixed problem in backtracking, when speedup is enabled
 *        - added usb_UpdateButtonStatus()
 * - 0.50 - added button support for Plustek/Genius devices
 *        - changed behaviour of usb_IsScannerReady
 *        - added special misc I/O setup for CIS devices (usb_ResetRegisters)
 * - 0.51 - change usb_AdjustLamps() and use it now in usb_switchLamp() 
 *        - added usb_Wait4ScanSample() and usb_InCalibrationMode()
 *        - tweaked EjectPaper to work correctly with the supported sheet-fed
 *          devices
 *        - fixed button handling for Plustek/Genius devices and added
 *          some more debug output to that code path
 * - 0.52 - changed DCapsDef, lamp -> misc_io
 *        - hammer in output bit, when using misc io pins for lamp switching
 *        - increased wait time for sheet-fed scanner (needed for Q-SCAN A6,
 *          patch by Hiroshi Miura)
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
#ifdef HAVE_SYS_TIME_H 
#include <sys/time.h>
#endif

#define DEV_LampReflection      1
#define DEV_LampTPA             2
#define DEV_LampAll             3
#define DEV_LampPositive        4
#define DEV_LampNegative        5

#define WAIT_TIME_FOR_SCAN_SAMPLE 20   /* 20 seconds maximum wait time */


/** the NatSemi 983x is a big endian chip, and the line protocol data all
 *  arrives big-endian.  This determines if we need to swap to host-order
 */
static SANE_Bool
usb_HostSwap( void )
{
	u_short        pattern  = 0xfeed; /* deadbeef */
	unsigned char *bytewise = (unsigned char *)&pattern;

	if ( bytewise[0] == 0xfe ) {
		DBG( _DBG_READ, "We're big-endian!  No need to swap!\n" );
		return 0;
	}
	DBG( _DBG_READ, "We're little-endian!  NatSemi LM983x is big!\n" );
	DBG( _DBG_READ, "--> Must swap data!\n" );
	return 1;
}

/** as the name says..
 */
static void
usb_Swap( u_short *pw, u_long dwBytes )
{
	for( dwBytes /= 2; dwBytes--; pw++ )
		_SWAP(((u_char*) pw)[0], ((u_char*)pw)[1]);
}

/**
 * This function is used to detect a cancel condition,
 * our ESC key is the SIGUSR1 signal. It is sent by the backend when the
 * cancel button has been pressed
 *
 * @param - none
 * @return the function returns SANE_TRUE if a cancel condition has been
 *  detected, if not, it returns SANE_FALSE
 */
static SANE_Bool
usb_IsEscPressed( void )
{
	sigset_t sigs;

	sigpending( &sigs );
	if( sigismember( &sigs, SIGUSR1 )) {
		DBG( _DBG_INFO, "SIGUSR1 is pending --> Cancel detected\n" );
		return SANE_TRUE;
	}

	return SANE_FALSE;
}

/** usb_GetMotorSet
 * according to the model, the function returns the address of
 * the corresponding entry of the Motor table
 */
static ClkMotorDef*
usb_GetMotorSet( eModelDef model )
{
	int i;

	for( i = 0; i < MODEL_LAST; i++ ) {
		if( model == Motors[i].motorModel ) {
			return &(Motors[i]);
		}
	}

	return NULL;
}

/** switch motor on or off
 * @param handle - handle to open USB device
 * @param fOn    - SANE_TRUE means motor on, SANE_FALSE means motor off
 * @return always SANE_TRUE
 */
static SANE_Bool
usb_MotorOn( Plustek_Device *dev, SANE_Bool fOn )
{
	if( fOn )
		dev->usbDev.a_bRegs[0x45] |= 0x10;
	else
		dev->usbDev.a_bRegs[0x45] &= ~0x10;

	usbio_WriteReg( dev->fd, 0x45, dev->usbDev.a_bRegs[0x45] );
	return SANE_TRUE;
}

/**
 */
static SANE_Bool
usb_IsCISDevice( Plustek_Device *dev )
{
	return ( dev->usbDev.HwSetting.bReg_0x26 & _ONE_CH_COLOR );
}

/**
 */
static SANE_Bool
usb_IsSheetFedDevice( Plustek_Device *dev )
{
	return ( dev->usbDev.Caps.wFlags & DEVCAPSFLAG_SheetFed );
}

/**
 */
static SANE_Bool
usb_InCalibrationMode( Plustek_Device *dev )
{
	if((dev->scanning.dwFlag & SCANFLAG_Calibration) == 0)
		return SANE_FALSE;

	return SANE_TRUE;
}

/** check if scanner is ready
 */
static SANE_Bool
usb_IsScannerReady( Plustek_Device *dev )
{
	u_char         value;
	double         len;	
	long           timeout;
	struct timeval t;
	SANE_Status    res;

	/* time in s = 1000*scanner length in inches/max step speed/in */
	len = (dev->usbDev.Caps.Normal.Size.y/(double)_MEASURE_BASE) + 5;
	len = (1000.0 * len)/dev->usbDev.HwSetting.dMaxMoveSpeed;
	len /= 1000.0;

	/* wait at least 10 seconds... */
	if( len < 10 )
		len = 10;

	gettimeofday( &t, NULL);	
	timeout = t.tv_sec + len;

	do {	
		res = usbio_ReadReg( dev->fd, 7, &value);
		if( res != SANE_STATUS_GOOD ) {
			sleep(1);
		} else {
			if( value == 0 ) {
				_UIO( usbio_ResetLM983x( dev ));
				return SANE_TRUE;
			}

			if((value == 0) || (value >= 0x20) || (value == 0x03)) {

				if( !usbio_WriteReg( dev->fd, 0x07, 0 )) {
					DBG( _DBG_ERROR, "Scanner not ready!!!\n" );
					return SANE_FALSE;
				}
				else {
					return SANE_TRUE;	
				}
			}
		}
		gettimeofday( &t, NULL);
		
	} while( t.tv_sec < timeout );
	
	DBG( _DBG_ERROR, "Scanner not ready!!!\n" );
	return SANE_FALSE;
}

/**
 */
static SANE_Bool
usb_SensorAdf( Plustek_Device *dev )
{
	u_char value;

	if( usb_IsSheetFedDevice(dev))
		return SANE_FALSE;

	usbio_ReadReg( dev->fd, 0x02, &value );

	return (value & 0x20);
}

/**
 */
static SANE_Bool
usb_SensorPaper( Plustek_Device *dev )
{
	DCapsDef   *sc   = &dev->usbDev.Caps;
	u_char val, mask = 0x02;

	usbio_ReadReg( dev->fd, 0x02, &val );

	if( usb_IsSheetFedDevice(dev))
		mask = _GET_PAPERSENSE_PORT(sc->misc_io);

	return (val & mask);
}

/** function for sheet-fed devices, to make sure, that there's
 * something to scan
 */
static SANE_Bool
usb_Wait4ScanSample( Plustek_Device *dev )
{
	struct timeval start_time, t2;

	if( !usb_IsSheetFedDevice(dev))
		return SANE_TRUE;

	DBG( _DBG_INFO2, "Waiting for something to scan...\n" );
	gettimeofday( &start_time, NULL );
	do {

		gettimeofday( &t2, NULL );
		if( t2.tv_sec > start_time.tv_sec + WAIT_TIME_FOR_SCAN_SAMPLE ) {
			DBG( _DBG_ERROR, "Nothing to scan!!!\n" );
			return SANE_FALSE;
		}
		if( usb_IsEscPressed())
			return SANE_FALSE;
	}
	while( !usb_SensorPaper( dev ));

	/* just a little delay, to make sure the paper is taken by the scanner */
	usleep(100* 1000);
	DBG( _DBG_INFO2, "... okay, scanning now!\n" );
	return SANE_TRUE;
}

/** function to move the sensor and to speed it up to a certain speed until
 *  the position is reached
 */
static SANE_Bool
usb_WaitPos( Plustek_Device *dev, u_long to, SANE_Bool stay )
{
	SANE_Bool      retval;
	u_char         value, mclk_div, mch;
	u_char         r[2];
	u_short        ffs, step, min_ffs;
	long           dwTicks;
	double         maxf, fac, speed;
	struct timeval start_time, t2;

	HWDef         *hw   = &dev->usbDev.HwSetting;
	u_char        *regs = dev->usbDev.a_bRegs;

	/* get current master clock divider */
	usbio_ReadReg( dev->fd, 0x08, &value );
	mclk_div = (value >> 1) + 1;

	/* get current channel mode */
	usbio_ReadReg( dev->fd, 0x26, &value );
	mch = ((value & 0x07) > 1) ? 1:3;

	/* calculate the current speed */
	ffs   = regs[0x48] * 256 + regs[0x49];
	speed = ((double)CRYSTAL_FREQ) /(double)((u_long)mclk_div * 32UL *
	                                (u_long)mch * (u_long)ffs * hw->wMotorDpi);

	/* disabled ? */
	if((hw->dHighSpeed == 0.0) || (dev->adj.disableSpeedup != 0)) {
		min_ffs = 0xffff;
		maxf    = 0.0;
		if( !stay )
			return SANE_TRUE;

	} else {
		min_ffs = (u_short)(CRYSTAL_FREQ /((u_long)mclk_div * 32UL *
		                   (u_long)mch * hw->dHighSpeed * hw->wMotorDpi));
		maxf = (ffs - min_ffs)/4;
		if( maxf > 100.0 )
			maxf = 100.0;
		if( maxf < 5.0 )
			maxf = 5.0;
		DBG( _DBG_INFO2, ">>>> CURRENT MCLK_DIV = %u\n", mclk_div );
		DBG( _DBG_INFO2, ">>>> MCH              = %u\n", mch ); 
		DBG( _DBG_INFO2, ">>>> FFS              = %u\n", ffs ); 
		DBG( _DBG_INFO2, ">>>> HIGH-SPEED       = %.3f (%.3f)\n", 
		                  speed, hw->dHighSpeed); 
		DBG( _DBG_INFO2, ">>>> MIN_FFS          = %u (%.3f)\n", min_ffs, maxf);
	}

	gettimeofday( &start_time, NULL );
	dwTicks = start_time.tv_sec + to;
	step    = 1;
	retval  = SANE_FALSE;

	for(;;) {

		usleep( 1000 );
		_UIO( usbio_ReadReg( dev->fd, 0x07, &value ));

		if (!value)
			return SANE_TRUE;

		gettimeofday(&t2, NULL);
	    if( t2.tv_sec > dwTicks )
			break;

		if( min_ffs != 0xffff ) {
			
			fac = maxf/step;
			if( ffs ) {
				if((u_short)fac < ffs ) {
					ffs -= fac;
					if( ffs < min_ffs )
						ffs = min_ffs;
				} else {
					if(ffs != min_ffs )
						ffs = min_ffs;
					else
						ffs = 0;
				}
			}

			if( ffs >= min_ffs ) {
				if((int)fac > 25 )
					usleep( 150 * 1000 );

				r[0] = (u_char)(ffs >> 8);
				r[1] = (u_char)(ffs & 0xFF);
				sanei_lm983x_write(dev->fd, 0x48, r, 2, SANE_TRUE);
				if(ffs == min_ffs )
					ffs = 0;
			} else {
		
				if( !stay ) {
					retval = SANE_TRUE;
					break;
				}
			}
			step++;
		}
	}
	return retval;
}

/** function to move the sensor or if sheet-fed device, to move the paper.
 * In moving backward-mode, the home sensor is always turned on.
 * @param action - what to do
 * @param steps  - steps to move, based on 300dpi, 0 means move forever
 */
static SANE_Bool
usb_ModuleMove( Plustek_Device *dev, u_char action, u_long dwStep )
{
	SANE_Bool    retval, ejected;
	u_char       bReg2, reg7, mclk_div;
	u_short      wFastFeedStepSize;
	double       dMaxMoveSpeed;
	ClkMotorDef *clk;
	HWDef       *hw   = &dev->usbDev.HwSetting;
	u_char      *regs = dev->usbDev.a_bRegs;

	if( action != MOVE_ToPaperSensor   &&
		action != MOVE_EjectAllPapers  &&
		action != MOVE_SkipPaperSensor &&
		action != MOVE_ToShading       && !dwStep ) {

		return SANE_TRUE;
	}

	if( !usb_IsScannerReady( dev )) {
		DBG( _DBG_ERROR, "Sensor-position NOT reached\n" );
		return SANE_FALSE;
	}

	if( action == MOVE_EjectAllPapers ) {

		double d = hw->dMaxMoveSpeed;

		/* FIXME */
		if (hw->motorModel == MODEL_QSCAN_A6){
			DBG( _DBG_INFO2, "Q-SCAN-A6 may not be able to detect ejected papers\n");
			return SANE_TRUE;
		}

		hw->dMaxMoveSpeed += 0.8; /* was 0.6 */

		DBG( _DBG_INFO2, "Ejecting paper...\n" );
		retval  = SANE_TRUE;
		ejected = SANE_FALSE;
		do {
			if( usb_SensorPaper(dev)) {
				if (!usb_ModuleMove(dev,MOVE_SkipPaperSensor, 0 )) {
					hw->dMaxMoveSpeed = d;
					return SANE_FALSE;
				}
				ejected = SANE_TRUE;
			}

			if( usb_SensorAdf(dev) &&
				!usb_ModuleMove(dev,MOVE_ToPaperSensor, 0 )) {
				hw->dMaxMoveSpeed = d;
				return SANE_FALSE;
			}

			if( usb_IsEscPressed()) {
				retval = SANE_FALSE;
				break;
			}
		} while( usb_SensorPaper(dev));

		/* when the paper is beyond the sensor, we move another 300 steps
		 * to make sure, that the scanned sheet is out of the scanner
		 * BUT: not at startup
		 */
		if (dev->initialized >= 0 || ejected) {
			DBG(_DBG_INFO2, "... MORE EJECT...\n");
			if(!usb_ModuleMove( dev, MOVE_Forward, 300 /* *3 */)) {
				hw->dMaxMoveSpeed = d;
				return SANE_FALSE;
			}
		}

		usbio_WriteReg( dev->fd, 0x07, 0);
		usbio_WriteReg( dev->fd, 0x58, regs[0x58]);

		usbio_ReadReg( dev->fd, 0x02, &bReg2 );
		hw->dMaxMoveSpeed = d;
		DBG( _DBG_INFO2, "...done\n" );
		return retval;
	}

	usbio_WriteReg( dev->fd, 0x0a, 0 );

	/* Compute fast feed step size, use equation 3 and equation 8 */
	dMaxMoveSpeed = hw->dMaxMoveSpeed;

	if( action == MOVE_ToShading ) {
		if( hw->dMaxMoveSpeed > 0.5 )
			dMaxMoveSpeed = hw->dMaxMoveSpeed - 0.5;
	}

	clk = usb_GetMotorSet( hw->motorModel );

	mclk_div = clk->mclk_fast;

	wFastFeedStepSize = (u_short)(CRYSTAL_FREQ /
	                    ((u_long)mclk_div * 8UL * 1 *
	                    dMaxMoveSpeed * 4 * hw->wMotorDpi));

	regs[0x48] = (u_char)(wFastFeedStepSize >> 8);
	regs[0x49] = (u_char)(wFastFeedStepSize & 0xFF);

	dwStep = dwStep * hw->wMotorDpi / 300UL;
	regs[0x4a] = _HIBYTE(_LOWORD(dwStep));
	regs[0x4b] = _LOBYTE(_LOWORD(dwStep));

	regs[0x45] |= 0x10;

	DBG( _DBG_INFO2,"MotorDPI=%u, MaxMoveSpeed=%.3f, "
					"FFStepSize=%u, Steps=%lu\n", hw->wMotorDpi,
					hw->dMaxMoveSpeed, wFastFeedStepSize, dwStep );
	DBG( _DBG_INFO2, "MOTOR: "
					"PWM=0x%02x, PWM_DUTY=0x%02x 0x45=0x%02x "
                    "0x48=0x%02x, 0x49=0x%02x \n",
					regs[0x56], regs[0x57], regs[0x45],
					regs[0x48], regs[0x49] );

	DBG( _DBG_INFO2,"MCLK_FFW = %u --> 0x%02x\n", mclk_div, (mclk_div-1)*2 );

	/* The setting for chassis moving is:
	 * MCLK divider = 6, 8 bits/pixel, HDPI divider = 12,
	 * no integration time adjustment and 1 channel grayscale
	 */

	/* MCLK divider = 6 */
	if( !usbio_WriteReg(dev->fd, 0x08, (mclk_div-1)*2 /*0x0A*/))
		return SANE_FALSE;

	/* 8 bits/pixel, HDPI divider = 12 */
	if( !usbio_WriteReg(dev->fd, 0x09, 0x1F))
		return SANE_FALSE;

	/* Turn off integration time adjustment */
	if( !usbio_WriteReg(dev->fd, 0x19, 0))
		return SANE_FALSE;

	/* 1 channel grayscale, green channel */
	if( !usbio_WriteReg(dev->fd, 0x26, 0x0C))
		return SANE_FALSE;

	_UIO(sanei_lm983x_write(dev->fd, 0x48, &regs[0x48], 2, SANE_TRUE));
	_UIO(sanei_lm983x_write(dev->fd, 0x4A, &regs[0x4A], 2, SANE_TRUE));

	/* disable home */
	if( !usbio_WriteReg(dev->fd, 0x58, regs[0x58] & ~7))
		return SANE_FALSE;

	if( !usbio_WriteReg(dev->fd, 0x45, regs[0x45] ))
		return SANE_FALSE;

	if( action == MOVE_Forward || action == MOVE_ToShading )
		reg7 = 5;
	else if( action == MOVE_Backward )
		reg7 = 6;
	else if( action == MOVE_ToPaperSensor || action == MOVE_EjectAllPapers ||
			 action == MOVE_SkipPaperSensor ) {
		reg7 = 1;
	} else {
		return SANE_TRUE;
    }
	
	retval = SANE_FALSE;

	/* start the sensor... */
	if( usbio_WriteReg( dev->fd, 0x07, reg7 )) {

		long           secs;
	    struct timeval start_time, t2;

		/* at least we move 20 seconds before timeout... */
		gettimeofday( &start_time, NULL );
	    secs = start_time.tv_sec + 20;

		if( action == MOVE_ToPaperSensor ) {

			for(;;) {

				if( usb_SensorPaper( dev )) {
					usbio_WriteReg( dev->fd, 0x07, 0 );
					usbio_WriteReg( dev->fd, 0x58, regs[0x58] );
					usbio_ReadReg ( dev->fd, 0x02, &bReg2 );
					return SANE_TRUE;
				}

				gettimeofday(&t2, NULL);	
				if( t2.tv_sec > secs )
					break;	
			}
		} else if( action == MOVE_SkipPaperSensor ) {

			for(;;) {

				if( !usb_SensorPaper( dev )) {
					usbio_WriteReg( dev->fd, 0x07, 0 );
					usbio_WriteReg( dev->fd, 0x58, regs[0x58] );
					usbio_ReadReg ( dev->fd, 0x02, &bReg2 );
					return SANE_TRUE;
				}

				gettimeofday(&t2, NULL);	
				if( t2.tv_sec > secs )
					break;
			}
		} else {

			retval = usb_WaitPos( dev, 200, SANE_TRUE );
		}

		usbio_WriteReg( dev->fd, 0x58, regs[0x58] );
		usbio_ReadReg ( dev->fd, 0x02, &bReg2 );
	}

	if( !retval )
		DBG( _DBG_ERROR, "Position NOT reached\n" );
	return retval;
}

/**
 */
static SANE_Bool
usb_ModuleToHome( Plustek_Device *dev, SANE_Bool fWait )
{
	u_char    mclk_div;
	u_char    value;
	DCapsDef *scaps = &dev->usbDev.Caps;
	HWDef    *hw    = &dev->usbDev.HwSetting;
	u_char   *regs  = dev->usbDev.a_bRegs;

	if( usb_IsSheetFedDevice(dev)) {
		return usb_ModuleMove( dev, MOVE_EjectAllPapers, 0 );
	}

	/* Check if merlin is ready for setting command */
	usbio_WriteReg( dev->fd, 0x58, hw->bReg_0x58 );
	usbio_ReadReg ( dev->fd, 2, &value );
	if( value & 1 ) {
		dev->usbDev.fModFirstHome = SANE_FALSE;
		return SANE_TRUE;
	}

	_UIO( usbio_ReadReg( dev->fd, 0x07, &value ));

	if( dev->usbDev.fModFirstHome ) {
		dev->usbDev.fModFirstHome = SANE_FALSE;
		if( hw->motorModel != MODEL_Tokyo600 )
			usb_ModuleMove( dev, MOVE_Forward, hw->wMotorDpi / 2);
	}

	/* if not homing, do it... */
	if( value != 2 ) {

		u_short wFastFeedStepSize;

		if( hw->motorModel == MODEL_Tokyo600 ) {
			usbio_WriteReg( dev->fd, 0x07, 0 );
		} else {
			_UIO( usbio_ResetLM983x( dev ));
			usleep(200*1000);
		}

		if(!_IS_PLUSTEKMOTOR(hw->motorModel)) {

			ClkMotorDef *clk;

			clk = usb_GetMotorSet( hw->motorModel );

			regs[0x56] = clk->pwm_fast;
			regs[0x57] = clk->pwm_duty_fast;
			mclk_div   = clk->mclk_fast;

		} else {

			mclk_div = 6;

			if( scaps->OpticDpi.x == 1200 || scaps->bPCB == 2) {
				switch( hw->motorModel ) {

				case MODEL_KaoHsiung:
				case MODEL_HuaLien:
				default:
					regs[0x56] = 1;
					regs[0x57] = 63;
					break;
				}
			} else { /* if(Device.Caps.OpticDpi.x == 600) */

				switch( hw->motorModel ) {

				case MODEL_Tokyo600:
					regs[0x56] = 4;
					regs[0x57] = 4;	/* 2; */
					break;
				case MODEL_HuaLien:
					if( dev->caps.dwFlag & SFLAG_ADF ) {
						regs[0x56] = 64;	/* 32; */
						regs[0x57] = 4;	/* 16; */
					} else {
						regs[0x56] = 32;
						regs[0x57] = 16;
					}
					break;
				
				case MODEL_KaoHsiung:
				default:
					regs[0x56] = 64;
					regs[0x57] = 20;
					break;
				}
			}
		}

		/* Compute fast feed step size, use equation 3 and equation 8
		 * assumptions: MCLK = 6, Lineratemode (CM=1)
		 */
		wFastFeedStepSize = (u_short)(CRYSTAL_FREQ / (mclk_div * 8 * 1 *
		                              hw->dMaxMotorSpeed * 4 * hw->wMotorDpi));
		regs[0x48] = (u_char)(wFastFeedStepSize >> 8);
		regs[0x49] = (u_char)(wFastFeedStepSize & 0xFF);
		regs[0x4a] = 0;
		regs[0x4b] = 0;

		regs[0x45] |= 0x10;

		DBG( _DBG_INFO2,"MotorDPI=%u, MaxMotorSpeed=%.3f, FFStepSize=%u\n",
						hw->wMotorDpi, hw->dMaxMotorSpeed, wFastFeedStepSize );
		DBG( _DBG_INFO, "MOTOR: "
						"PWM=0x%02x, PWM_DUTY=0x%02x 0x45=0x%02x "
                        "0x48=0x%02x, 0x49=0x%02x\n",
						regs[0x56], regs[0x57],
						regs[0x45], regs[0x48], regs[0x49] );

		/* The setting for chassis moving is:
		 * MCLK divider = 6, 8 bits/pixel, HDPI divider = 12,
		 * no integration time adjustment and 1 channel grayscale
	 	 */
		/* MCLK divider = 6 */
		value = (u_char)((mclk_div-1) * 2);

		DBG( _DBG_INFO, "MCLK_FFW = %u --> 0x%02x\n", mclk_div, value );

		if( !usbio_WriteReg(dev->fd, 0x08, value))
			return SANE_FALSE;

		/* 8 bits/pixel, HDPI divider = 12 */
		if( !usbio_WriteReg(dev->fd, 0x09, 0x1F))
			return SANE_FALSE;

		/* Turn off integration time adjustment */
		if( !usbio_WriteReg(dev->fd, 0x19, 0))
			return SANE_FALSE;

		/* 1 channel grayscale, green channel */
		if( !usbio_WriteReg(dev->fd, 0x26, 0x8C))
			return SANE_FALSE;

		_UIO(sanei_lm983x_write(dev->fd, 0x48, &regs[0x48], 4, SANE_TRUE));
		_UIO(sanei_lm983x_write(dev->fd, 0x56, &regs[0x56], 3, SANE_TRUE));

		if( !usbio_WriteReg(dev->fd, 0x45, regs[0x45]))
			return SANE_FALSE;

		usbio_WriteReg(dev->fd, 0x0a, 0);

		if( hw->motorModel == MODEL_HuaLien && scaps->OpticDpi.x == 600 )
			usleep(100 * 1000);

		if( !usbio_WriteReg(dev->fd, 0x07, 2))
			return SANE_FALSE;

#if 0
		if( hw->motorModel == MODEL_Tokyo600) {

			u_long	dwSpeedUp = GetTickCount () + 250;
			
			/* while(GetTickCount () < dwSpeedUp) */
			while((int)(dwSpeedUp - GetTickCount ()) > 0)
			{
				Sleep (10);
				if (!ReadRegister (0x07, &value))
					return FALSE;
				if (!value)
					return TRUE;
			}
			wFastFeedStepSize = (WORD)(CRYSTAL_FREQ / 
			    (6UL * 8UL * 1 * Device.HwSetting.dMaxMotorSpeed * 4 *
				Device.HwSetting.wMotorDpi) * 60 / 78);	
			regs[0x48] = (u_char)(wFastFeedStepSize >> 8);
			regs[0x49] = (u_char)(wFastFeedStepSize & 0xFF);
			WriteRegisters(0x48, &regs[0x48], 2);
		}
#endif
	}
	return usb_WaitPos( dev, 150, fWait );
}

/**
 */
static SANE_Bool
usb_MotorSelect( Plustek_Device *dev, SANE_Bool fADF )
{
	DCapsDef *sCaps = &dev->usbDev.Caps;
	HWDef    *hw    = &dev->usbDev.HwSetting;
	u_char   *regs  = dev->usbDev.a_bRegs;

	if(!_IS_PLUSTEKMOTOR(hw->motorModel)) {
		return SANE_TRUE;
	}
	
	if( fADF ) {

		if( sCaps->bCCD == kNEC3778 ) {
		
			hw->wMotorDpi      = 300;
			hw->dMaxMotorSpeed = 1.5;
			hw->dMaxMoveSpeed  = 1.5;
			sCaps->OpticDpi.y  = 600;
		}
		regs[0x5b] |= 0x80;
		
	} else {
	
		if( sCaps->bCCD == kNEC3778 ) {
			
			hw->wMotorDpi      = 600;
			hw->dMaxMotorSpeed = 1.1;
			hw->dMaxMoveSpeed  = 0.9;
			sCaps->OpticDpi.y  = 1200;
		}
		regs[0x5b] &= ~0x80;
	}

	/* To stop the motor moving */
	usbio_WriteReg( dev->fd, 0x07, 0 );
	usleep(10 * 1000);

	usbio_WriteReg( dev->fd, 0x5b, regs[0x5b] );
	return SANE_TRUE;
}

/** function to adjust the lamp settings of a CIS device without tweaking
 *  the driver-device settings
 * @param dev - our almitghty device structure
 * @param on  - switch the lamp on or off
 */
static SANE_Bool
usb_AdjustLamps( Plustek_Device *dev, SANE_Bool on )
{
	HWDef  *hw   = &dev->usbDev.HwSetting;
	u_char *regs = dev->usbDev.a_bRegs;

	if( !usb_IsCISDevice(dev))
		return SANE_TRUE;

	DBG(_DBG_INFO2, "usb_AdjustLamps(%u)\n", on );

	if( on ) {
		regs[0x2c] = _HIBYTE(hw->red_lamp_on);
		regs[0x2d] = _LOBYTE(hw->red_lamp_on);
		regs[0x2e] = _HIBYTE(hw->red_lamp_off);
		regs[0x2f] = _LOBYTE(hw->red_lamp_off);

		regs[0x30] = _HIBYTE(hw->green_lamp_on);
		regs[0x31] = _LOBYTE(hw->green_lamp_on);
		regs[0x32] = _HIBYTE(hw->green_lamp_off);
		regs[0x33] = _LOBYTE(hw->green_lamp_off);

		regs[0x34] = _HIBYTE(hw->blue_lamp_on);
		regs[0x35] = _LOBYTE(hw->blue_lamp_on);
		regs[0x36] = _HIBYTE(hw->blue_lamp_off);
		regs[0x37] = _LOBYTE(hw->blue_lamp_off);

	} else {
		memset( &regs[0x2c], 0, 12 );

		regs[0x2c] = 0x3f;
		regs[0x2d] = 0xff;
		regs[0x30] = 0x3f;
		regs[0x31] = 0xff;
		regs[0x34] = 0x3f;
		regs[0x35] = 0xff;
	}

	return sanei_lm983x_write( dev->fd, 0x2c,
	                           &regs[0x2c], 0x37-0x2c+1, SANE_TRUE );
}

/**
 */
static void
usb_AdjustCISLampSettings( Plustek_Device *dev, SANE_Bool on )
{
	HWDef *hw = &dev->usbDev.HwSetting;

	if( !usb_IsCISDevice(dev))
		return;

	DBG( _DBG_INFO2, "AdjustCISLamps(%u)\n", on );

	if((dev->scanning.sParam.bDataType == SCANDATATYPE_Gray) ||
	   (dev->scanning.sParam.bDataType == SCANDATATYPE_BW)) {

		DBG( _DBG_INFO2, " * setting mono mode\n" );
		hw->bReg_0x29 = hw->illu_mono.mode;

		memcpy( &hw->red_lamp_on,
				&hw->illu_mono.red_lamp_on, sizeof(u_short) * 6 );

	} else {

		DBG( _DBG_INFO2, " * setting color mode\n" );
		hw->bReg_0x29 = hw->illu_color.mode;

		memcpy( &hw->red_lamp_on,
				&hw->illu_color.red_lamp_on, sizeof(u_short) * 6 );
	}

	if( !on ) {

		hw->red_lamp_on    = 0x3fff;
		hw->red_lamp_off   = 0;
		hw->green_lamp_on  = 0x3fff;
		hw->green_lamp_off = 0;
		hw->blue_lamp_on   = 0x3fff;
		hw->blue_lamp_off  = 0;
	} else {

		if( dev->adj.rlampoff > 0 ) {
			hw->red_lamp_off = dev->adj.rlampoff;

			if( hw->red_lamp_off > 0x3fff )
				hw->red_lamp_off = 0x3fff;
			DBG( _DBG_INFO2,
			     " * red_lamp_off adjusted: %u\n", hw->red_lamp_off );
		}

		if( dev->adj.glampoff > 0 ) {
			hw->green_lamp_off = dev->adj.glampoff;

			if( hw->green_lamp_off > 0x3fff )
				hw->green_lamp_off = 0x3fff;
			DBG( _DBG_INFO2,
			     " * green_lamp_off adjusted: %u\n", hw->green_lamp_off );
		}

		if( dev->adj.blampoff > 0 ) {
			hw->blue_lamp_off = dev->adj.blampoff;

			if( hw->blue_lamp_off > 0x3fff )
				hw->blue_lamp_off = 0x3fff;
			DBG( _DBG_INFO2,
			     " * blue_lamp_off adjusted: %u\n", hw->blue_lamp_off );
		}
	}

	dev->usbDev.a_bRegs[0x29] = hw->bReg_0x29;
	usb_AdjustLamps( dev, on );
}

/** according to the flag field, we return the register and
 * it's mask to turn on/off the lamp.
 * @param flag - field to check
 * @param reg  - pointer to a var to receive the register value
 * @param msk  - pointer to a var to receive the mask value
 * @return Nothing
 */
static void
usb_GetLampRegAndMask( u_long flag, SANE_Byte *reg, SANE_Byte *msk )
{
	if( _MIO6 == ( _MIO6 & flag )) {
		*reg = 0x5b;
		*msk = 0x80;

	} else if( _MIO5 == ( _MIO5 & flag )) {
		*reg = 0x5b;
		*msk = 0x08;

	} else if( _MIO4 == ( _MIO4 & flag )) {
		*reg = 0x5a;
		*msk = 0x80;

	} else if( _MIO3 == ( _MIO3 & flag )) {
		*reg = 0x5a;
		*msk = 0x08;

	} else if( _MIO2 == ( _MIO2 & flag )) {
		*reg = 0x59;
		*msk = 0x80;

	} else if( _MIO1 == ( _MIO1 & flag )) {
		*reg = 0x59;
		*msk = 0x08;

	} else {
		*reg = 0;
		*msk = 0;
	}
}

/** usb_GetLampStatus
 * This function returns the current lamp in use.
 * For non Plustek devices, it always returns DEV_LampReflection.
 * @param dev  - pointer to our device structure,
 *               it should contain all we need
 * @return - 0 if the scanner hasn't been used before, DEV_LampReflection
 *           for the normal lamp, or DEV_LampTPA for negative/transparency
 *           lamp
 */
static int
usb_GetLampStatus( Plustek_Device *dev )
{
	int         iLampStatus = 0;
	u_char     *regs = dev->usbDev.a_bRegs;
	HWDef      *hw   = &dev->usbDev.HwSetting;
	DCapsDef   *sc   = &dev->usbDev.Caps;
	SANE_Byte   reg, msk, val;

	if( NULL == hw ) {
		DBG( _DBG_ERROR, "NULL-Pointer detected: usb_GetLampStatus()\n" );
		return -1;
	}

	/* do we use the misc I/O pins for switching the lamp ? */
	if( _WAF_MISC_IO_LAMPS & sc->workaroundFlag ) {

		usb_GetLampRegAndMask( sc->misc_io, &reg, &msk );

		if( 0 == reg ) {
			usbio_ReadReg( dev->fd, 0x29, &reg );
			if( reg & 3 )
				iLampStatus |= DEV_LampReflection;

		} else {

			/* check if the lamp is on */
			usbio_ReadReg( dev->fd, reg, &val );

			DBG( _DBG_INFO2, "LAMP-REG[0x%02x] = 0x%02x (msk=0x%02x)\n",
			                  reg,val,msk);
			if( val & msk )
				iLampStatus |= DEV_LampReflection;

			/* if the device supports a TPA, we check this here */
			if( sc->wFlags & DEVCAPSFLAG_TPA ) {

				usb_GetLampRegAndMask( _GET_TPALAMP(sc->misc_io), &reg, &msk );
				if (reg) {
					usbio_ReadReg( dev->fd, reg, &val );
					DBG( _DBG_INFO2, "TPA-REG[0x%02x] = 0x%02x (msk=0x%02x)\n",
					                  reg,val,msk);
					if( val & msk )
						iLampStatus |= DEV_LampTPA;
				}
			}

			/* CanoScan D660U extra vaganza... */
			if((dev->usbDev.vendor == 0x04A9) && (dev->usbDev.product==0x2208)) {
				sanei_lm983x_read( dev->fd, 0x29, &regs[0x29], 3, SANE_TRUE );
				DBG( _DBG_INFO, "[29]=0x%02x, [2A]=0x%02x, [2B]=0x%02x\n",
				                regs[0x29], regs[0x2a], regs[0x2b] );
			}
		}
	} else {
		sanei_lm983x_read(dev->fd, 0x29,&regs[0x29],0x37-0x29+1,SANE_TRUE);

		if((regs[0x29] & 3) == 1) {

/* HEINER: BETTER define register to check ! */

			if(!_IS_PLUSTEKMOTOR(hw->motorModel)) {
				iLampStatus |= DEV_LampReflection;

			} else {

				if((regs[0x2e] * 256 + regs[0x2f]) > hw->wLineEnd )
					iLampStatus |= DEV_LampReflection;

				if((regs[0x36] * 256 + regs[0x37]) > hw->wLineEnd )
					iLampStatus |= DEV_LampTPA;
			}
		}
	}

	DBG( _DBG_INFO, "LAMP-STATUS: 0x%08x (%s)\n", 
	                 iLampStatus, iLampStatus?"on":"off" );
	return iLampStatus;
}

/** usb_switchLampX
 * used for all devices that use some misc I/O pins to switch the lamp
 */
static SANE_Bool
usb_switchLampX( Plustek_Device *dev, SANE_Bool on, SANE_Bool tpa )
{
	SANE_Byte reg, msk;
	DCapsDef *sc   = &dev->usbDev.Caps;
	u_char   *regs = dev->usbDev.a_bRegs;

	if( tpa )
		usb_GetLampRegAndMask( _GET_TPALAMP(sc->misc_io), &reg, &msk );
	else
		usb_GetLampRegAndMask( sc->misc_io, &reg, &msk );

	if( 0 == reg )
		return SANE_FALSE; /* no need to switch something */

	DBG( _DBG_INFO, "usb_switchLampX(ON=%u,TPA=%u)\n", on, tpa );

	if( on ) {
		/* in fact the output bit should be set by the default settings
		 * but we make sure, that it is set anyway...
		 */
		if (msk & 0x08)
			msk |= 0x01;
		else
			msk |= 0x10;
		regs[reg] |= msk;
	} else {
		regs[reg] &= ~msk;
	}

	DBG( _DBG_INFO, "Switch Lamp: %u, regs[0x%02x] = 0x%02x\n",
	                on, reg, regs[reg] );
	usbio_WriteReg( dev->fd, reg, regs[reg] );
	return SANE_TRUE;
}

/** usb_switchLamp
 * used for all devices that use some misc I/O pins to switch the lamp
 */
static SANE_Bool
usb_switchLamp( Plustek_Device *dev, SANE_Bool on )
{
	SANE_Bool result;

	if((dev->scanning.sParam.bSource == SOURCE_Negative) ||
	   (dev->scanning.sParam.bSource == SOURCE_Transparency)) {
		result = usb_switchLampX( dev, on, SANE_TRUE );
	} else {
		result = usb_switchLampX( dev, on, SANE_FALSE );
	}

	/* to switch off CIS, we need to tweak the lampoff/on regs */
	usb_AdjustLamps( dev, on );
	return result;
}

/** usb_LedOn
 */
static void
usb_LedOn( Plustek_Device *dev, SANE_Bool fOn )
{
	u_char value;

	if( dev->usbDev.HwSetting.motorModel != MODEL_HuaLien )
		return;

	value = dev->usbDev.a_bRegs[0x0d];

	if( fOn )
		value |= 0x10;
	else
		value &= ~0x10;

	dev->usbDev.a_bRegs[0x0d] = value;
	usbio_WriteReg( dev->fd, 0x0d, value );
}

/** usb_FillLampRegs
 * set all the registers controlling the lamps
 */
static void
usb_FillLampRegs( Plustek_Device *dev )
{
	HWDef  *hw   = &dev->usbDev.HwSetting;
	u_char *regs = dev->usbDev.a_bRegs;

	regs[0x2a] = _HIBYTE( hw->wGreenPWMDutyCycleLow );
	regs[0x2b] = _LOBYTE( hw->wGreenPWMDutyCycleLow );

	regs[0x2c] = _HIBYTE( hw->red_lamp_on );
	regs[0x2d] = _LOBYTE( hw->red_lamp_on );
	regs[0x2e] = _HIBYTE( hw->red_lamp_off);
	regs[0x2f] = _LOBYTE( hw->red_lamp_off);

	regs[0x30] = _HIBYTE( hw->green_lamp_on );
	regs[0x31] = _LOBYTE( hw->green_lamp_on );
	regs[0x32] = _HIBYTE( hw->green_lamp_off);
	regs[0x33] = _LOBYTE( hw->green_lamp_off);

	regs[0x34] = _HIBYTE( hw->blue_lamp_on );
	regs[0x35] = _LOBYTE( hw->blue_lamp_on );
	regs[0x36] = _HIBYTE( hw->blue_lamp_off);
	regs[0x37] = _LOBYTE( hw->blue_lamp_off);
}

/** usb_LampOn
 */
static SANE_Bool
usb_LampOn( Plustek_Device *dev, SANE_Bool fOn, SANE_Bool fResetTimer )
{
	DCapsDef      *sc          = &dev->usbDev.Caps;
	ScanDef       *scanning    = &dev->scanning;
	HWDef         *hw          = &dev->usbDev.HwSetting;
	u_char        *regs        = dev->usbDev.a_bRegs;
	int            iLampStatus = usb_GetLampStatus( dev );
	int            lampId      = -1;
	struct timeval t;

	if( NULL == scanning ) {
		DBG( _DBG_ERROR, "NULL-Pointer detected: usb_LampOn()\n" );
		return SANE_FALSE;
	}

	switch( scanning->sParam.bSource ) {

	case SOURCE_Reflection:
	case SOURCE_ADF:
		lampId = DEV_LampReflection;
		break;

	case SOURCE_Transparency:
	case SOURCE_Negative:
		lampId = DEV_LampTPA;
		break;
	}

	if( fOn ) {

		if( iLampStatus != lampId ) {
			
			DBG( _DBG_INFO, "Switching Lamp on\n" );

/* here we might have to switch off the TPA/Main lamp before
 * using the other one
 */
			if( lampId != dev->usbDev.currentLamp ) {
				if( dev->usbDev.currentLamp == DEV_LampReflection )
					usb_switchLampX( dev, SANE_FALSE, SANE_FALSE );
				else
					usb_switchLampX( dev, SANE_FALSE, SANE_TRUE );
			}

			memset( &regs[0x29], 0, (0x37-0x29+1));
			
			regs[0x29] = hw->bReg_0x29;

			if( !usb_switchLamp(dev, SANE_TRUE )) {

				if( lampId == DEV_LampReflection ) {
					regs[0x2e] = 16383 / 256;
					regs[0x2f] = 16383 % 256;
				}

				if( lampId == DEV_LampTPA ) {
					regs[0x36] = 16383 / 256;
					regs[0x37] = 16383 % 256;
				}
			}

			if( _WAF_MISC_IO_LAMPS & sc->workaroundFlag )
				usb_FillLampRegs( dev );

			sanei_lm983x_write( dev->fd, 0x29,
			                    &regs[0x29], 0x37-0x29+1, SANE_TRUE );
			if( lampId != dev->usbDev.currentLamp ) {
			
				dev->usbDev.currentLamp = lampId;
				
				if( fResetTimer ) {
			
					gettimeofday( &t, NULL );	
					dev->usbDev.dwTicksLampOn = t.tv_sec;
					DBG( _DBG_INFO, "Warmup-Timer started\n" );
				}
			}
		}

	} else {

		int iStatusChange = iLampStatus & ~lampId;
		
		if( iStatusChange != iLampStatus ) {

			DBG( _DBG_INFO, "Switching Lamp off\n" );
		
			memset( &regs[0x29], 0, 0x37-0x29+1 );
			if( !usb_switchLamp(dev, SANE_FALSE )) {
			
				if( iStatusChange & DEV_LampReflection ) {
					regs[0x2e] = 16383 / 256;
					regs[0x2f] = 16383 % 256;
				}

				if( iStatusChange & DEV_LampTPA ) {
					regs[0x36] = 16383 / 256;
					regs[0x37] = 16383 % 256;
				}
			}

			if( _WAF_MISC_IO_LAMPS & sc->workaroundFlag )
				usb_FillLampRegs( dev );

			sanei_lm983x_write( dev->fd, 0x29,
			                    &regs[0x29], 0x37-0x29+1, SANE_TRUE );
		}
	}
	if( usb_GetLampStatus(dev))
		usb_LedOn( dev, SANE_TRUE );
	else
		usb_LedOn( dev, SANE_FALSE );
	return SANE_TRUE;
}

/** Function to preset the registers for the specific device, which
 * should never change during the whole operation
 * Affected registers:<br>
 * 0x0b - 0x0e - Sensor settings - directly from the HWDef<br>
 * 0x0f - 0x18 - Sensor Configuration - directly from the HwDef<br>
 * 0x1a - 0x1b - Stepper Phase Correction<br>
 * 0x20 - 0x21 - Line End<br>
 * 0x21 - 0x22 - Data Pixel start<br>
 * 0x23 - 0x24 - Data Pixel end<br>
 * 0x45        - Stepper Motor Mode<br>
 * 0x4c - 0x4d - Full Steps to Scan after PAPER SENSE 2 trips<br>
 * 0x50        - Steps to reverse when buffer is full<br>
 * 0x51        - Acceleration Profile<br>
 * 0x54 - 0x5e - Motor Settings, Paper-Sense Settings and Misc I/O<br>
 *
 * @param dev    - pointer to our device structure,
 *                 it should contain all we need
 * @return - Nothing
 */
static void
usb_ResetRegisters( Plustek_Device *dev )
{
	int linend;

	HWDef  *hw   = &dev->usbDev.HwSetting;
	u_char *regs = dev->usbDev.a_bRegs;

	DBG( _DBG_INFO, "RESETTING REGISTERS(%i) - 0x%02x\n", 
	                dev->initialized, (int) sizeof(dev->usbDev.a_bRegs));
	memset( regs, 0, sizeof(dev->usbDev.a_bRegs));

	memcpy( regs+0x0b, &hw->bSensorConfiguration, 4 );
	memcpy( regs+0x0f, &hw->bReg_0x0f_Color, 10 );
	regs[0x1a] = _HIBYTE( hw->StepperPhaseCorrection );
	regs[0x1b] = _LOBYTE( hw->StepperPhaseCorrection );

/* HEINER: CHECK WHY THIS has been disabled*/
#if 0
	regs[0x1c] = hw->bOpticBlackStart;
	regs[0x1d] = hw->bOpticBlackEnd;

	regs[0x1e] = _HIBYTE( hw->wActivePixelsStart );
	regs[0x1f] = _LOBYTE( hw->wActivePixelsStart );
#endif
	regs[0x20] = _HIBYTE( hw->wLineEnd );
	regs[0x21] = _LOBYTE( hw->wLineEnd );

	regs[0x22] = _HIBYTE( hw->bOpticBlackStart );
	regs[0x23] = _LOBYTE( hw->bOpticBlackStart );

	linend = hw->bOpticBlackStart + hw->wLineEnd;
	if( linend < (hw->wLineEnd-20))
		linend = hw->wLineEnd-20;

	regs[0x24] = _HIBYTE( linend );
	regs[0x25] = _LOBYTE( linend );

	regs[0x2a] = _HIBYTE( hw->wGreenPWMDutyCycleHigh );
	regs[0x2b] = _LOBYTE( hw->wGreenPWMDutyCycleHigh );

	regs[0x45] = hw->bReg_0x45;
	regs[0x4c] = _HIBYTE( hw->wStepsAfterPaperSensor2 );
	regs[0x4d] = _LOBYTE( hw->wStepsAfterPaperSensor2 );
	regs[0x50] = hw->bStepsToReverse;
	regs[0x51] = hw->bReg_0x51;

	/* if already initialized, we ignore the MISC I/O settings as
	 * they are used to determine the current lamp settings...
	 */
	if( dev->initialized >= 0 ) {

		DBG( _DBG_INFO2, "USING MISC I/O settings\n" );
		memcpy( regs+0x54, &hw->bReg_0x54, 0x58 - 0x54 + 1 );
		regs[0x5c] = hw->bReg_0x5c;
		regs[0x5d] = hw->bReg_0x5d;
		regs[0x5e] = hw->bReg_0x5e;
		sanei_lm983x_read( dev->fd, 0x59, &regs[0x59], 3, SANE_TRUE );

	} else {

		DBG( _DBG_INFO2, "SETTING THE MISC I/Os\n" );
		memcpy( regs+0x54, &hw->bReg_0x54, 0x5e - 0x54 + 1 );

		if( usb_IsCISDevice( dev )) {

			/* this sequence seems to be needed at least for the
			 * CanoScan devices to work properly after power-up
			 */
			sanei_lm983x_write_byte( dev->fd, 0x5b, regs[0x5b] );
			sanei_lm983x_write_byte( dev->fd, 0x59, regs[0x59] );
			sanei_lm983x_write_byte( dev->fd, 0x5a, regs[0x5a] );
		} else {
			sanei_lm983x_write( dev->fd, 0x59, &regs[0x59], 3, SANE_TRUE );
		}
	}
	DBG( _DBG_INFO, "MISC I/O after RESET: 0x%02x, 0x%02x, 0x%02x\n",
	                        regs[0x59], regs[0x5a], regs[0x5b] );
}

/** function which checks if we are already in home position or not.
 * 
 */
static SANE_Bool
usb_SensorStatus( Plustek_Device *dev )
{
	u_char value;
	HWDef *hw = &dev->usbDev.HwSetting;

/* HEINER: Maybe needed to avoid recalibration!!! */
#if 0
	if( dev->scanning.fCalibrated )
		return SANE_TRUE;
#endif

	/* read the status register */
	_UIO( usbio_ReadReg( dev->fd, 2, &value ));
	if( value & 1 ) {

		_UIO( usbio_ReadReg( dev->fd, 0x7, &value));
		if( value ) {

			usbio_WriteReg( dev->fd, 0x07, 0 );
			usbio_WriteReg( dev->fd, 0x07, 0x20 );
			usbio_WriteReg( dev->fd, 0x07, 0 );
			
			sanei_lm983x_write( dev->fd, 0x58,
								&hw->bReg_0x58, 0x5b-0x58+1, SANE_TRUE );
			usbio_ReadReg( dev->fd, 2, &value );
			usbio_ReadReg( dev->fd, 2, &value );
		}

		usb_MotorOn( dev, SANE_FALSE );
		return SANE_TRUE;
	}

	_UIO( usbio_ReadReg( dev->fd, 0x7, &value ));

	if( !(value & 2)) {
		usb_ModuleToHome( dev, SANE_FALSE );
	}

	return SANE_FALSE;
}

/**
 */
static void
usb_LampSwitch( Plustek_Device *dev, SANE_Bool sw )
{
	int handle = -1;

	if( -1 == dev->fd ) {

		if( SANE_STATUS_GOOD == sanei_usb_open(dev->sane.name, &handle)) {
			dev->fd = handle;
		}
	}

	/* needs to be recalibrated */
	dev->scanning.fCalibrated = SANE_FALSE;

	if( -1 != dev->fd )
		usb_LampOn( dev, sw, SANE_FALSE );

	if( -1 != handle ) {
		dev->fd = -1;
		sanei_usb_close( handle );
	}
}

/* HEINER: replace!!! */
static Plustek_Device *dev_xxx = NULL;

/** ISR to switch lamp off after time has elapsed
 */
static void
usb_LampTimerIrq( int sig )
{
	if( NULL == dev_xxx )
		return;

	_VAR_NOT_USED( sig );
	DBG( _DBG_INFO, "LAMP OFF!!!\n" );

	usb_LampSwitch( dev_xxx, SANE_FALSE );
}

/** usb_StartLampTimer
 */
static void
usb_StartLampTimer( Plustek_Device *dev )
{
	sigset_t         block, pause_mask;
	struct sigaction s;
#ifdef HAVE_SETITIMER
	struct itimerval interval;
#endif
	/* block SIGALRM */
	sigemptyset( &block );
	sigaddset  ( &block, SIGALRM );
	sigprocmask( SIG_BLOCK, &block, &pause_mask );

	/* setup handler */
	sigemptyset( &s.sa_mask );
	sigaddset  ( &s.sa_mask, SIGALRM );
	s.sa_flags   = 0;
	s.sa_handler = usb_LampTimerIrq;

	if( sigaction( SIGALRM, &s, NULL ) < 0 )
		DBG( _DBG_ERROR, "Can't setup timer-irq handler\n" );

	sigprocmask( SIG_UNBLOCK, &block, &pause_mask );

#ifdef HAVE_SETITIMER
	/* define a one-shot timer */
	interval.it_value.tv_usec    = 0;
	interval.it_value.tv_sec     = dev->usbDev.dwLampOnPeriod;
	interval.it_interval.tv_usec = 0;
	interval.it_interval.tv_sec  = 0;

	if( 0 != dev->usbDev.dwLampOnPeriod ) {
		dev_xxx = dev;
		setitimer( ITIMER_REAL, &interval, &dev->saveSettings );
		DBG( _DBG_INFO, "Lamp-Timer started (using ITIMER)\n" );
	}
#else
	if( 0 != dev->usbDev.dwLampOnPeriod ) {
		dev_xxx = dev;
		alarm( dev->usbDev.dwLampOnPeriod );
		DBG( _DBG_INFO, "Lamp-Timer started (using ALARM)\n" );
	}
#endif
}

/** usb_StopLampTimer
 */
static void
usb_StopLampTimer( Plustek_Device *dev )
{
	sigset_t block, pause_mask;

	/* block SIGALRM */
	sigemptyset( &block );
	sigaddset  ( &block, SIGALRM );
	sigprocmask( SIG_BLOCK, &block, &pause_mask );

	dev_xxx = NULL;

#ifdef HAVE_SETITIMER
	if( 0 != dev->usbDev.dwLampOnPeriod )
		setitimer( ITIMER_REAL, &dev->saveSettings, NULL );
#else
	_VAR_NOT_USED( dev );
	alarm( 0 );
#endif
	DBG( _DBG_INFO, "Lamp-Timer stopped\n" );
}

/** wait until warmup has been done
 */
static SANE_Bool
usb_Wait4Warmup( Plustek_Device *dev )
{
	u_long         dw;
	struct timeval t;

	if( usb_IsCISDevice(dev)) {
		DBG(_DBG_INFO,"Warmup: skipped for CIS devices\n" );
		return SANE_TRUE;
	}
	
	if( dev->adj.warmup < 0 )
		return SANE_TRUE;

	/*
	 * wait until warmup period has been elapsed
	 */
	gettimeofday( &t, NULL);
	dw = t.tv_sec - dev->usbDev.dwTicksLampOn;
	if( dw < (u_long)dev->adj.warmup )
		DBG(_DBG_INFO,"Warmup: Waiting %d seconds\n", dev->adj.warmup );

	do {

		gettimeofday( &t, NULL);

		dw = t.tv_sec - dev->usbDev.dwTicksLampOn;

		if( usb_IsEscPressed()) {
			return SANE_FALSE;
		}

	} while( dw < (u_long)dev->adj.warmup );

	return SANE_TRUE;
}

/** function for TPA autodection (EPSON & UMAX devices)
 */
static SANE_Bool
usb_HasTPA( Plustek_Device *dev )
{
	static char model[] = { "3450" };
	u_char val;

	if( dev->usbDev.vendor == 0x04B8 ) {         /* the EPSON section */

		usb_switchLampX( dev, SANE_FALSE, SANE_TRUE );
		usbio_WriteReg ( dev->fd, 0x58, 0x1d );
		usbio_WriteReg ( dev->fd, 0x59, 0x49 );
		usbio_ReadReg  ( dev->fd, 0x02, &val );
		usbio_WriteReg ( dev->fd, 0x58, dev->usbDev.HwSetting.bReg_0x58 );
		usbio_WriteReg ( dev->fd, 0x59, dev->usbDev.HwSetting.bReg_0x59 );

		DBG( _DBG_INFO, "REG[0x02] = 0x%02x\n", val );

		if( val & 0x02 ) {
			DBG( _DBG_INFO, "EPSON-TPA detected\n" );
			return SANE_TRUE;
		} else
			DBG( _DBG_INFO, "EPSON-TPA NOT detected\n" );

		if( dev->adj.enableTpa ) {
			DBG( _DBG_INFO, "EPSON-TPA usage forced\n" );
			return SANE_TRUE;
		}

	} else if( dev->usbDev.vendor == 0x1606 ) { /* the UMAX section   */

		if((dev->usbDev.product == 0x0050) || (dev->usbDev.product == 0x0060)) {

			usbio_ReadReg  ( dev->fd, 0x02, &val );
			DBG( _DBG_INFO, "REG[0x02] = 0x%02x\n", val );

			usbio_WriteReg ( dev->fd, 0x58, dev->usbDev.HwSetting.bReg_0x58 );
			usbio_WriteReg ( dev->fd, 0x5a, dev->usbDev.HwSetting.bReg_0x5a );
			usbio_WriteReg ( dev->fd, 0x5b, dev->usbDev.HwSetting.bReg_0x5b );
			
			usbio_ReadReg  ( dev->fd, 0x02, &val );
			DBG( _DBG_INFO, "REG[0x02] = 0x%02x\n", val );

			if( val & 0x02 ) {
				DBG( _DBG_INFO, "UMAX-TPA detected\n" );
				dev->usbDev.ModelStr = model;
				return SANE_TRUE;
			} else 
				DBG( _DBG_INFO, "UMAX-TPA NOT detected\n" );

			if( dev->adj.enableTpa ) {
				DBG( _DBG_INFO, "UMAX-TPA usage forced\n" );
				dev->usbDev.ModelStr = model;
				return SANE_TRUE;
			}
		}
	} 
	return SANE_FALSE;
}

/** function for reading the button states
 */
static SANE_Bool
usb_UpdateButtonStatus( Plustek_Scanner *s )
{
	u_char          mio[3];
	SANE_Byte       val, mask;
	int             i, j, bc;
	int             handle = -1;
	SANE_Status     status;
	Plustek_Device *dev  = s->hw;
	DCapsDef       *caps = &dev->usbDev.Caps;

	if (caps->bButtons == 0)
		return SANE_FALSE;

	status = sanei_access_lock( dev->sane.name, 3 );
	if( SANE_STATUS_GOOD != status ) 
		return SANE_FALSE;

	if( -1 == dev->fd ) {

		status = sanei_usb_open(dev->sane.name, &handle);
		if( SANE_STATUS_GOOD != status ) {
			sanei_access_unlock( dev->sane.name );
			return SANE_FALSE;
		}
		dev->fd = handle;
	}

	/* we have to check all 6 misc I/O ports for input configuration*/
	mio[0] = dev->usbDev.HwSetting.bReg_0x59;
	mio[1] = dev->usbDev.HwSetting.bReg_0x5a;
	mio[2] = dev->usbDev.HwSetting.bReg_0x5b;
	
	usbio_ReadReg( dev->fd, 0x07, &val );
	if( val == 0 ) {

		/* first read clears the status... */
		usbio_ReadReg( dev->fd, 0x02, &val );

		/* Plustek and KYE/Genius use altnernative button handling */
		if((dev->usbDev.vendor == 0x07B3) || (dev->usbDev.vendor == 0x0458)) {

			DBG( _DBG_INFO2, "Button Value=0x%02x\n", val );

			/* no button pressed so far */
			for( i = 0; i < caps->bButtons; i++ )
				s->val[OPT_BUTTON_0 + i].w = 0;

			if (caps->bButtons == 2 || caps->bButtons == 5) {
				val >>= 2;
				val &= 0x07;
				DBG( _DBG_INFO2, "Button Value=0x%02x (2/5)\n", val );

				switch( val ) {
					case 1: s->val[OPT_BUTTON_1].w = 1; break;
					case 2: s->val[OPT_BUTTON_0].w = 1; break;
					case 3: s->val[OPT_BUTTON_2].w = 1; break;
					case 4: s->val[OPT_BUTTON_3].w = 1; break;
					case 6: s->val[OPT_BUTTON_4].w = 1; break;
				}
			} else if (caps->bButtons == 4 ) {
				val >>= 5;
				val &= 0x07;
				DBG( _DBG_INFO2, "Button Value=0x%02x (4)\n", val );

				switch( val ) {
					case 1: s->val[OPT_BUTTON_0].w = 1; break;
					case 2: s->val[OPT_BUTTON_1].w = 1; break;
					case 4: s->val[OPT_BUTTON_2].w = 1; break;
					case 6: s->val[OPT_BUTTON_3].w = 1; break;
				}
			} else {
				DBG( _DBG_INFO2, "Hmm, could not handle this!\n" );
			}

		} else {

			/* the generic section... */
			val >>= 2;
			bc    = 0;

			/* only use the "valid" ports, that reflect a button */
			if( _WAF_MISC_IO_BUTTONS & caps->workaroundFlag ) {
				if((caps->misc_io & _PORT0) == 0)
					mio[0] = 0xff;
				if((caps->misc_io & _PORT1) == 0)
					mio[1] = 0xff;
				if((caps->misc_io & _PORT2) == 0)
					mio[2] = 0xff;
			}

			for( i = 0; i < 3; i++ ) {
	
				DBG( _DBG_INFO2, "Checking MISC IO[%u]=0x%02x\n", i, mio[i] );
				mask = 0x01;

				for( j = 0; j < 2; j++ ) {

					if((mio[i] & mask) == 0) {
						DBG( _DBG_INFO2, "* port %u configured as input,"
						     " status: %s (%u)\n", (i*2)+j+1, 
							 ((val & 1)?"PRESSED":"RELEASED"), (OPT_BUTTON_0 + bc));
						s->val[OPT_BUTTON_0 + bc].w = val & 1; 
						bc++;
					}
					val  >>= 1;
					mask <<= 4;
				}
			}
		}
	} else {
		DBG( _DBG_INFO2, "Scanner NOT idle: 0x%02x\n", val );
	}

	if( -1 != handle ) {
		dev->fd = -1;
		sanei_usb_close( handle );
	}

	sanei_access_unlock( dev->sane.name );
	return SANE_TRUE;
}

/* END PLUSTEK-USBHW.C ......................................................*/
