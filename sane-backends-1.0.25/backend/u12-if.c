/** @file u12-if.c
 *  @brief The interface functions to the U12 backend stuff.
 *
 * Copyright (c) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - added model tweaking
 *        - fixed autodetection bug
 *        - added line-scaling stuff
 *        - removed u12if_setScanEnv()
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

#define _DEF_BRIGHTEST_SKIP 3
#define _DEF_DARKEST_SKIP   5

/** useful for description tables
 */
typedef struct {
	int	  id;
	char *desc;
} TabDef;

typedef struct {
	char *vp;
	char *name;
} DevDesc;

#define _PLUSTEK_VENID 0x07B3
#define _KYE_VENID     0x0458
  
/** to allow different vendors...
 */
static TabDef u12Vendors[] = {

	{ _PLUSTEK_VENID, "Plustek"    },
	{ _KYE_VENID,     "KYE/Genius" },
	{ 0xFFFF,         NULL         }
};

/** list of supported devices
 */
static DevDesc u12Devices[] = {
	{ "0x07B3-0x0001", "1212U/U12"     },
	{ "0x0458-0x2004", "Colorpage HR6" },
	{ NULL,            NULL }
};

/** for autodetection */
static SANE_Char USB_devname[1024];

/********************** the USB scanner interface ****************************/

/**
 */
static SANE_Status u12_initDev( U12_Device *dev, int handle, int vendor )
{
	int         i;
	SANE_Status res;
	TimerDef    timer;

	/* well now we patch the vendor string...
	 * if not found, the default vendor will be Plustek
	 */
	for( i = 0; u12Vendors[i].desc != NULL; i++ ) {

		if( u12Vendors[i].id == vendor ) {
			dev->sane.vendor = u12Vendors[i].desc;
			DBG( _DBG_INFO, "Vendor adjusted to: >%s<\n", dev->sane.vendor );
			break;
		}
	}
	dev->fd = handle;

	dev->adj.upNormal   = 0;
	dev->adj.upNegative = 20;
	dev->adj.upPositive = -30;
	dev->adj.leftNormal = 51;

	res = SANE_STATUS_IO_ERROR;
	if( !(u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER)) {

		u12motor_PositionModuleToHome( dev );

		u12io_StartTimer( &timer, _SECOND * 20);
		do {
			if( u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER) {
				res = SANE_STATUS_GOOD;
				break;
			}
		} while( !u12io_CheckTimer( &timer ));
	} else {
		res = u12hw_InitAsic( dev, SANE_FALSE );
	}

	if( res == SANE_STATUS_GOOD )
		u12hw_PutToIdleMode( dev );
	return res;
}

/** will be called upon sane_exit
 */
static void u12if_shutdown( U12_Device *dev  )
{
	SANE_Int handle;
	TimerDef timer;

	DBG( _DBG_INFO, "Shutdown called (dev->fd=%d, %s)\n",
													dev->fd, dev->sane.name );
	if( SANE_STATUS_GOOD == sanei_usb_open( dev->sane.name, &handle )) {
		
    	dev->fd = handle;
		u12io_OpenScanPath( dev );

		u12hw_PutToIdleMode( dev );

		if( !(u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER)) {

			u12motor_PositionModuleToHome( dev );

			u12io_StartTimer( &timer, _SECOND * 20);
			do {
				if( u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER) {
					break;
				}
			} while( !u12io_CheckTimer( &timer ));
		}
		DBG( _DBG_INFO, "* Home position reached.\n" );

		if( 0 != dev->adj.lampOffOnEnd ) {

			DBG( _DBG_INFO, "* Switching lamp off...\n" );
			dev->regs.RD_ScanControl &= ~_SCAN_LAMPS_ON;
			u12io_DataToRegister(dev,REG_SCANCONTROL, dev->regs.RD_ScanControl );
		}

		u12io_CloseScanPath( dev );
		dev->fd = -1;
		sanei_usb_close( handle );
	}

#if 0
	usb_StopLampTimer( dev );
#endif
	DBG( _DBG_INFO, "Shutdown done.\n" );
}

/** This function checks wether a device, described by a given
 * string(vendor and product ID), is supported by this backend or not
 *
 * @param usbIdStr - sting consisting out of product and vendor ID
 *                   format: "0xVVVV-0xPPPP" VVVV = Vendor ID, PPP = Product ID
 * @returns; SANE_TRUE if supported, SANE_FALSE if not
 */
static SANE_Bool u12if_IsDeviceSupported( U12_Device *dev )
{
	int i;

	for( i = 0; NULL != u12Devices[i].name; i++ ) {

		if( !strcmp( dev->usbId, u12Devices[i].vp )) {
			dev->sane.model = u12Devices[i].name;
			return SANE_TRUE;
		}
	}

	return SANE_FALSE;
}

/**
 */
static SANE_Status u12if_usbattach( SANE_String_Const dev_name )
{
	if( USB_devname[0] == '\0' ) {
		DBG( _DBG_INFO, "Found device at >%s<\n", dev_name );
		strncpy( USB_devname, dev_name, 1023 );
		USB_devname[1023] = '\0';
	} else {
		DBG( _DBG_INFO, "Device >%s< ignoring\n", dev_name );
	}

	return SANE_STATUS_GOOD;
}

#ifndef _FAKE_DEVICE
/** here we roam through our list of supported devices and
 * cross check with the ones the system reports...
 * @param vendor  - pointer to receive vendor ID
 * @param product - pointer to receive product ID
 * @return SANE_TRUE if a matching device has been found or
 *         SANE_FALSE if nothing supported found...
 */
static SANE_Bool usbDev_autodetect( SANE_Word *vendor, SANE_Word *product )
{
	int       i;
	SANE_Word p, v;
	
	DBG( _DBG_INFO, "Autodetection...\n" );

	for( i = 0; NULL != u12Devices[i].name; i++ ) {

		v = strtol( &(u12Devices[i].vp)[0], 0, 0 );
		p = strtol( &(u12Devices[i].vp)[7], 0, 0 );
		DBG( _DBG_INFO, "* checking for 0x%04x-0x%04x\n", v, p );

		sanei_usb_find_devices( v, p, u12if_usbattach );

		if( USB_devname[0] != '\0' ) {

			*vendor  = v;
			*product = p;
			DBG( _DBG_INFO, "* using device >%s<\n", USB_devname );
			return SANE_TRUE;
		}
	}

	return SANE_FALSE;
}
#endif

/**
 */
static int u12if_open( U12_Device *dev )
{
	char      devStr[50];
	int       result;
	SANE_Int  handle;
	SANE_Word vendor, product;
	SANE_Bool was_empty;

	DBG( _DBG_INFO, "u12if_open(%s,%s)\n", dev->name, dev->usbId );

	USB_devname[0] = '\0';

#ifdef _FAKE_DEVICE
	dev->name      = strdup( "auto" );
	dev->sane.name = dev->name;
	was_empty      = SANE_FALSE;

	result = SANE_STATUS_UNSUPPORTED;
#else
	if( !strcmp( dev->name, "auto" )) {

		if( dev->usbId[0] == '\0' ) {

			if( !usbDev_autodetect( &vendor, &product )) {
				DBG( _DBG_ERROR, "No supported device found!\n" );
				return -1;
			}

		} else {

			vendor  = strtol( &dev->usbId[0], 0, 0 );
			product = strtol( &dev->usbId[7], 0, 0 );

			sanei_usb_find_devices( vendor, product, u12if_usbattach );

			if( USB_devname[0] == '\0' ) {
				DBG( _DBG_ERROR, "No matching device found!\n" );
        		return -1;
			}
		}

		if( SANE_STATUS_GOOD != sanei_usb_open( USB_devname, &handle )) {
			return -1;
		}

		/* replace the old devname, so we are able to have multiple
         * auto-detected devices
         */
		free( dev->name );
		dev->name      = strdup( USB_devname );
		dev->sane.name = dev->name; 

	} else {

		if( SANE_STATUS_GOOD != sanei_usb_open( dev->name, &handle ))
    		return -1;
	}
	was_empty = SANE_FALSE;

	result = sanei_usb_get_vendor_product( handle, &vendor, &product );
#endif

	if( SANE_STATUS_GOOD == result ) {

		sprintf( devStr, "0x%04X-0x%04X", vendor, product );

		DBG(_DBG_INFO,"Vendor ID=0x%04X, Product ID=0x%04X\n",vendor,product);

		if( dev->usbId[0] != '\0' ) {
		
			if( 0 != strcmp( dev->usbId, devStr )) {
				DBG( _DBG_ERROR, "Specified Vendor and Product ID "
								 "doesn't match with the ones\n"
								 "in the config file\n" );
				sanei_usb_close( handle );
		        return -1;
			}
		} else {
			sprintf( dev->usbId, "0x%04X-0x%04X", vendor, product );
			was_empty = SANE_TRUE;
		}			

	} else {

		DBG( _DBG_INFO, "Can't get vendor & product ID from driver...\n" );

		/* if the ioctl stuff is not supported by the kernel and we have
		 * nothing specified, we have to give up...
		*/
		if( dev->usbId[0] == '\0' ) {
			DBG( _DBG_ERROR, "Cannot autodetect Vendor an Product ID, "
							 "please specify in config file.\n" );
			sanei_usb_close( handle );
			return -1;
		}

		vendor  = strtol( &dev->usbId[0], 0, 0 );
		product = strtol( &dev->usbId[7], 0, 0 );
		DBG( _DBG_INFO, "... using the specified: "
		                "0x%04X-0x%04X\n", vendor, product );
	}

	/* before accessing the scanner, check if supported!
	 */
	if( !u12if_IsDeviceSupported( dev )) {
		DBG( _DBG_ERROR, "Device >%s<, is not supported!\n", dev->usbId );
		sanei_usb_close( handle );
		return -1;
	}

	dev->mode = _PP_MODE_SPP;
	dev->fd   = handle;

	/* is it accessible ? */
	if( SANE_STATUS_GOOD != u12hw_CheckDevice( dev )) {
		dev->fd = -1;
		sanei_usb_close( handle );
		return -1;
	}

	DBG( _DBG_INFO, "Detected vendor & product ID: "
		                "0x%04X-0x%04X\n", vendor, product );

	if( was_empty )
		dev->usbId[0] = '\0';

	/* now initialize the device */
	if( SANE_STATUS_GOOD != u12_initDev( dev, handle, vendor )) {
		dev->fd = -1;
		sanei_usb_close( handle );
		return -1;
	}

	if( _PLUSTEK_VENID == vendor ) {
		if( dev->Tpa )
			dev->sane.model = "UT12";
	}
	
	dev->initialized = SANE_TRUE;
	return handle;
}

/**
 */
static int u12if_close( U12_Device *dev )
{
	DBG( _DBG_INFO, "u12if_close()\n" );
	u12io_CloseScanPath( dev );
	sanei_usb_close( dev->fd );
	dev->fd = -1;
    return 0;
}

/** 
 */
static SANE_Status u12if_getCaps( U12_Device *dev )
{
	int cntr;
	int res_x = 600 ; /*dev->caps.OpticDpi.x */
	DBG( _DBG_INFO, "u12if_getCaps()\n" );

/* FIXME: set dpi_range.max, max_x & max_y on a per model base */
	dev->dpi_max_x       = 600;
	dev->dpi_max_y       = 1200;

	/* A4 devices */
	dev->max_x = 8.5     * (double)_MM_PER_INCH;
	dev->max_y = 11.6934 * (double)_MM_PER_INCH;

	/* limit the range */
	dev->dpi_range.min   = _DEF_DPI;
	dev->dpi_range.max   = dev->dpi_max_y;
	dev->dpi_range.quant = 0;
	dev->x_range.min 	 = 0;
	dev->x_range.max 	 = SANE_FIX(dev->max_x);
	dev->x_range.quant 	 = 0;
	dev->y_range.min 	 = 0;
	dev->y_range.max 	 = SANE_FIX(dev->max_y);
	dev->y_range.quant 	 = 0;

	/* calculate the size of the resolution list +
	 * one more to avoid a buffer overflow, then allocate it...
	 */
	dev->res_list = (SANE_Int *)
					calloc((((res_x * 16)-_DEF_DPI)/25+1),
						sizeof (SANE_Int));

	if (NULL == dev->res_list) {
		DBG( _DBG_ERROR, "alloc fail, resolution problem\n" );
		u12if_close(dev);
		return SANE_STATUS_INVAL;
	}

    /* build up the resolution table */
	dev->res_list_size = 0;
	for( cntr = _DEF_DPI; cntr <= (res_x*16); cntr += 25 ) {
		dev->res_list_size++;
		dev->res_list[dev->res_list_size - 1] = (SANE_Int)cntr;
	}
	
	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Status u12if_startScan( U12_Device *dev )
{
	DBG( _DBG_INFO, "u12if_startScan()\n" );
	u12hw_StopLampTimer( dev );
	u12hw_SetGeneralRegister( dev );
	u12hw_ControlLampOnOff( dev );
	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Status u12if_stopScan( U12_Device *dev )
{
	DBG( _DBG_INFO, "u12if_stopScan()\n" );

#if 0
	u12motor_ToHomePosition( dev, SANE_FALSE );
#else
#if 0
	u12motor_ToHomePosition( dev, SANE_TRUE );
	u12io_SoftwareReset( dev );
#endif
	u12hw_CancelSequence( dev );
#endif
	u12hw_StartLampTimer( dev );
	dev->DataInf.dwAppLinesPerArea = 0;
	dev->DataInf.dwScanFlag &= ~_SCANDEF_SCANNING;
	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Status u12if_prepare( U12_Device *dev )
{
	SANE_Status res;
	
	DBG( _DBG_INFO, "u12if_prepare()\n" );

	u12motor_ToHomePosition( dev, SANE_TRUE );

	res = u12hw_WarmupLamp( dev );
	if( res != SANE_STATUS_GOOD )
		return res;

	res = u12shading_DoCalibration( dev );
	if( res != SANE_STATUS_GOOD )
		return res;

	u12image_PrepareScaling( dev );
                           
	u12motor_ForceToLeaveHomePos( dev );
	if( dev->DataInf.dwScanFlag & _SCANDEF_PREVIEW )
		u12hw_SetupPreviewCondition( dev );
	else	
		u12hw_SetupScanningCondition( dev );

	res = u12motor_WaitForPositionY( dev );

	_DODELAY(100);
	u12io_ResetFifoLen();
	u12io_GetFifoLength(dev);

	dev->scan.bModuleState   = _MotorAdvancing;
	dev->scan.oldScanState   = u12io_GetScanState( dev );
	dev->scan.oldScanState  &= _SCANSTATE_MASK;
	dev->DataInf.dwScanFlag |= _SCANDEF_SCANNING;
	DBG( _DBG_INFO, "* oldScanState = %u\n", dev->scan.oldScanState );
	DBG( _DBG_INFO, "u12if_prepare() done.\n" );
	return res;
}

/**
 */
static SANE_Status u12if_readLine( U12_Device *dev, SANE_Byte *line_buffer )
{
	SANE_Status res;

	DBG( _DBG_READ, "u12if_readLine()\n" );

	if( u12io_IsEscPressed()) {
		DBG( _DBG_INFO, "u12if_readLine() - cancel detected!\n" );
		return SANE_STATUS_CANCELLED;
	}

	if( dev->scaleBuf != NULL ) {
		res = u12image_ReadOneImageLine( dev, dev->scaleBuf );
		if( SANE_STATUS_GOOD != res )
			return res;

		u12image_ScaleX( dev, dev->scaleBuf, line_buffer );

	} else {
		res = u12image_ReadOneImageLine( dev, line_buffer );
		if( SANE_STATUS_GOOD != res )
			return res;
	}
	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Status u12if_SetupBuffer( U12_Device *dev )
{
	void *buffer;

	DBG( _DBG_INFO, "u12if_SetupBuffer()\n" );
	buffer = malloc(_SIZE_TOTAL_BUF_TPA);
	if( buffer == NULL )
		return SANE_STATUS_NO_MEM;

	dev->shade.pHilight   = NULL;
	dev->bufs.b1.pReadBuf = buffer;
	dev->bufs.b2.pSumBuf  = dev->bufs.b1.pReadBuf + _SIZE_DATA_BUF;
	dev->bufs.TpaBuf.pb   = &((SANE_Byte*)dev->bufs.b2.pSumBuf)[_SIZE_SHADING_SUM_BUF];

/* CHECK: We might should play around with these values... */
	dev->shade.skipHilight = _DEF_BRIGHTEST_SKIP;
	dev->shade.skipShadow  = _DEF_DARKEST_SKIP;

	if( dev->shade.skipHilight && dev->shade.skipShadow ) {

		u_long skipSize;

		skipSize = (u_long)((dev->shade.skipHilight + dev->shade.skipShadow)
		                                                 * _SIZE_DATA_BUF * 3);
		dev->shade.pHilight = (RGBUShortDef*)malloc( skipSize );
		if( NULL != dev->shade.pHilight ) {
			dev->shade.dwDiv = (u_long)(32UL - dev->shade.skipHilight -
			                                            dev->shade.skipShadow);
		}
	}
	return SANE_STATUS_GOOD;
}

/* END U12-IF.C .............................................................*/
