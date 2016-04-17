/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usb.c
 *  @brief The interface functions to the USB driver stuff.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2007 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - removed CHECK
 *        - added Canon to the manufacturer list
 * - 0.42 - added warmup stuff
 *        - added setmap function
 *        - changed detection stuff, so we first check whether
 *        - the vendor and product Ids match with the ones in our list
 * - 0.43 - cleanup
 * - 0.44 - changes to integration CIS based devices
 * - 0.45 - added skipFine assignment
 *        - added auto device name detection if only product and vendor id<br>
 *          has been specified
 *        - made 16-bit gray mode work
 *        - added special handling for Genius devices
 *        - added TPA autodetection for EPSON Photo
 *        - fixed bug that causes warmup each time autodetected<br>
 *          TPA on EPSON is used
 *        - removed Genius from PCB-Id check
 *        - added Compaq to the list
 *        - removed the scaler stuff for CIS devices
 *        - removed homeing stuff from readline function
 *        - fixed flag setting in usbDev_startScan()
 * - 0.46 - added additional branch to support alternate calibration
 * - 0.47 - added special handling with 0x400 vendor ID and model override
 *        - removed PATH_MAX
 *        - change usbDev_stopScan and usbDev_open prototype
 *        - cleanup
 * - 0.48 - added function usb_CheckAndCopyAdjs()
 * - 0.49 - changed autodetection
 *        - added support for LiDE25 (pid 0x2220)
 * - 0.50 - minor fix for startup reset
 *          removed unnecessary calls to usbio_ResetLM983x()
 *          1200DPI CIS devices don't use GrayFromColor any longer
 * - 0.51 - added Syscan to the vendor list
 *        - added SCANFLAG_Calibration handling
 * - 0.52 - added _WAF_LOFF_ON_START and _WAF_INC_DARKTGT
 *          handling in usbDev_startScan()
 *          added Visioneer
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

/** useful for description tables
 */
typedef struct {
	int   id;
	char *desc;
	char *desc_alt;
} TabDef, *pTabDef;

/** to allow different vendors...
 */
static TabDef usbVendors[] = {

	{ 0x07B3, "Plustek",         NULL     },
	{ 0x0400, "NSC",             "Mustek" },
	{ 0x0458, "KYE/Genius",      NULL     },
	{ 0x03F0, "Hewlett-Packard", NULL     },
	{ 0x04B8, "Epson",           NULL     },
	{ 0x04A7, "Visioneer",       NULL     },
	{ 0x04A9, "Canon",           NULL     },
	{ 0x1606, "UMAX",            NULL     },
	{ 0x049F, "Compaq",          NULL     },
	{ 0x0A82, "Syscan",          NULL     },
	{ 0x0A53, "PandP Co., Ltd.", NULL     },
	{ 0xFFFF, NULL,              NULL     }
};

/** we use at least 8 megs for scanning... */
#define _SCANBUF_SIZE (8 * 1024 * 1024)

/********************** the USB scanner interface ****************************/

/** remove the slash out of the model-name to obtain a valid filename
 */
static SANE_Bool usb_normFileName( char *fname, char* buffer, u_long max_len )
{
	char *src, *dst;

	if( NULL == fname )
		return SANE_FALSE;

	if( strlen( fname ) >= max_len )
		return SANE_FALSE;

	src = fname;
	dst = buffer;
	while( *src != '\0' ) {

		if((*src == '/') || isspace(*src) || ispunct(*src))
			*dst = '_';
		else
			*dst = *src;

		dst++;
		src++;
	}
	*dst = '\0';

	return SANE_TRUE;	
}

/** do some range checking and copy the adjustment values from the
 * frontend to our internal structures, so that the backend can take
 * care of them.
 */
static void usb_CheckAndCopyAdjs( Plustek_Device *dev )
{
	if( dev->adj.lampOff >= 0 )
		dev->usbDev.dwLampOnPeriod = dev->adj.lampOff;

	if( dev->adj.lampOffOnEnd >= 0 )
		dev->usbDev.bLampOffOnEnd = dev->adj.lampOffOnEnd;

	if( dev->adj.skipCalibration > 0 )
		dev->usbDev.Caps.workaroundFlag |= _WAF_BYPASS_CALIBRATION;

	if( dev->adj.skipFine > 0 )
		dev->usbDev.Caps.workaroundFlag |= _WAF_SKIP_FINE;

	if( dev->adj.skipFineWhite > 0 )
		dev->usbDev.Caps.workaroundFlag |= _WAF_SKIP_WHITEFINE;

	if( dev->adj.incDarkTgt > 0 )
		dev->usbDev.Caps.workaroundFlag |= _WAF_INC_DARKTGT;

	if( dev->adj.skipDarkStrip > 0 )
		dev->usbDev.Caps.Normal.DarkShadOrgY = -1;

	if( dev->adj.invertNegatives > 0 )
		dev->usbDev.Caps.workaroundFlag |= _WAF_INV_NEGATIVE_MAP;
}

/**
 * assign the values to the structures used by the currently found scanner
 */
static void
usb_initDev( Plustek_Device *dev, int idx, int handle, int vendor )
{
	char     *ptr;
	char      tmp_str1[PATH_MAX];
	char      tmp_str2[PATH_MAX];
	int       i;
	ScanParam sParam;
	u_short   tmp = 0;

	DBG( _DBG_INFO, "usb_initDev(%d,0x%04x,%i)\n",
	                idx, vendor, dev->initialized );
	/* save capability flags... */
	if( dev->initialized >= 0 ) {
		tmp = DEVCAPSFLAG_TPA;
	}

	/* copy the original values... */
	memcpy( &dev->usbDev.Caps, Settings[idx].pDevCaps, sizeof(DCapsDef));
	memcpy( &dev->usbDev.HwSetting, Settings[idx].pHwDef, sizeof(HWDef));

	/* restore capability flags... */
	if( dev->initialized >= 0 ) {
		dev->usbDev.Caps.wFlags |= tmp;
	}

	usb_CheckAndCopyAdjs( dev );
	DBG( _DBG_INFO, "Device WAF  : 0x%08lx\n", dev->usbDev.Caps.workaroundFlag );
	DBG( _DBG_INFO, "Transferrate: %lu Bytes/s\n", dev->transferRate );

	/* adjust data origin
	 */
	dev->usbDev.Caps.Positive.DataOrigin.x -= dev->adj.tpa.x;	
	dev->usbDev.Caps.Positive.DataOrigin.y -= dev->adj.tpa.y;	

	dev->usbDev.Caps.Negative.DataOrigin.x -= dev->adj.neg.x;	
	dev->usbDev.Caps.Negative.DataOrigin.y -= dev->adj.neg.y;	

	dev->usbDev.Caps.Normal.DataOrigin.x -= dev->adj.pos.x;	
	dev->usbDev.Caps.Normal.DataOrigin.y -= dev->adj.pos.y;

	/** adjust shading position
	 */
	if( dev->adj.posShadingY >= 0 )
		dev->usbDev.Caps.Normal.ShadingOriginY   = dev->adj.posShadingY;

	if( dev->adj.tpaShadingY >= 0 )
		dev->usbDev.Caps.Positive.ShadingOriginY = dev->adj.tpaShadingY;

	if( dev->adj.negShadingY >= 0 )
		dev->usbDev.Caps.Negative.ShadingOriginY = dev->adj.negShadingY;

	/* adjust the gamma settings... */
	if( dev->adj.rgamma == 1.0 )
		dev->adj.rgamma = dev->usbDev.HwSetting.gamma;
	if( dev->adj.ggamma == 1.0 )
		dev->adj.ggamma = dev->usbDev.HwSetting.gamma;
	if( dev->adj.bgamma == 1.0 )
		dev->adj.bgamma = dev->usbDev.HwSetting.gamma;
	if( dev->adj.graygamma == 1.0 )
		dev->adj.graygamma = dev->usbDev.HwSetting.gamma;

	/* the following you normally get from the registry...
	 */
	bMaxITA = 0;     /* Maximum integration time adjust */

	dev->usbDev.ModelStr = Settings[idx].pModelString;

	dev->fd = handle;

	if( dev->initialized < 0 ) {
		if( usb_HasTPA( dev ))
			dev->usbDev.Caps.wFlags |= DEVCAPSFLAG_TPA;
	}
	DBG( _DBG_INFO, "Device Flags: 0x%08x\n", dev->usbDev.Caps.wFlags );

	/* well now we patch the vendor string...
	 * if not found, the default vendor will be Plustek
	 */
	for( i = 0; usbVendors[i].desc != NULL; i++ ) {

		if( usbVendors[i].id == vendor ) {
			dev->sane.vendor = usbVendors[i].desc;
			if (dev->usbDev.Caps.workaroundFlag & _WAF_USE_ALT_DESC )
				if (usbVendors[i].desc_alt )
					dev->sane.vendor = usbVendors[i].desc_alt;
			DBG( _DBG_INFO, "Vendor adjusted to: >%s<\n", dev->sane.vendor );
			break;
		}
	}

	dev->usbDev.dwTicksLampOn = 0;
	dev->usbDev.currentLamp   = usb_GetLampStatus( dev );
	usb_ResetRegisters( dev );

	if( dev->initialized >= 0 )
		return;

	usb_IsScannerReady( dev );

	sParam.bBitDepth     = 8;
	sParam.bCalibration  = PARAM_Scan;
	sParam.bChannels     = 3;
	sParam.bDataType     = SCANDATATYPE_Color;
	sParam.bSource       = SOURCE_Reflection;
	sParam.Origin.x      = 0;
	sParam.Origin.y      = 0;
	sParam.UserDpi.x     = 150;
	sParam.UserDpi.y     = 150;
	sParam.dMCLK         = 4;
	sParam.Size.dwPixels = 0;

	/* create calibration-filename */
	sprintf( tmp_str2, "%s-%s", 
	         dev->sane.vendor, dev->usbDev.ModelStr );

	if( !usb_normFileName( tmp_str2, tmp_str1, PATH_MAX )) {
		strcpy( tmp_str1, "plustek-default" );
	}
	
	ptr = getenv ("HOME");
	if( NULL == ptr ) {
		sprintf( tmp_str2, "/tmp/%s", tmp_str1 );
	} else {
		sprintf( tmp_str2, "%s/.sane/%s", ptr, tmp_str1 );
	}
	dev->calFile = strdup( tmp_str2 );
	DBG( _DBG_INFO, "Calibration file-names set to:\n" );
	DBG( _DBG_INFO, ">%s-coarse.cal<\n", dev->calFile );
	DBG( _DBG_INFO, ">%s-fine.cal<\n", dev->calFile );

	/* initialize the ASIC registers */
	usb_SetScanParameters( dev, &sParam );

	/* check and move sensor to its home position */
	usb_ModuleToHome( dev, SANE_FALSE );

	/* set the global flag, that we are initialized so far */
	dev->initialized = idx;
}

/**
 * will be used for retrieving a Plustek device
 */
static int usb_CheckForPlustekDevice( int handle, Plustek_Device *dev )
{
	char   tmp[50];
	char   pcbStr[10];
	u_char reg59[3], reg59s[3], pcbID;
	int    i, result;

	/*
	 * Plustek uses the misc IO 12 to get the PCB ID
	 * (PCB = printed circuit board), so it's possible to have one
	 * product ID and up to 7 different devices...
	 */
	DBG( _DBG_INFO, "Trying to get the pcbID of a Plustek device...\n" );

	/* get the PCB-ID */
	result = sanei_lm983x_read( handle, 0x59, reg59s, 3, SANE_TRUE );
	if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
		return -1;
	}

	reg59[0] = 0x22;	/* PIO1: Input, PIO2: Input         */
	reg59[1] = 0x02;	/* PIO3: Input, PIO4: Output as low */
	reg59[2] = 0x03;

	result = sanei_lm983x_write( handle, 0x59,  reg59, 3, SANE_TRUE );
	if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
		return -1;
	}

	result = sanei_lm983x_read ( handle, 0x02, &pcbID, 1, SANE_TRUE );
	if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
		return -1;
	}

	pcbID = (u_char)((pcbID >> 2) & 0x07);

	result = sanei_lm983x_read( handle, 0x59, reg59s, 3, SANE_TRUE );
	if( SANE_STATUS_GOOD != result ) {
		sanei_usb_close( handle );
		return -1;
	}

	DBG( _DBG_INFO, "pcbID=0x%02x\n", pcbID );

	/* now roam through the setting list... */
	strncpy( tmp, dev->usbId, 13 );
	tmp[13] = '\0';
	
	sprintf( pcbStr, "-%u", pcbID );
	strcat ( tmp, pcbStr );

	DBG( _DBG_INFO, "Checking for device >%s<\n", tmp );

	for( i = 0; NULL != Settings[i].pIDString; i++ ) {

		if( 0 == strcmp( Settings[i].pIDString, tmp )) {
			DBG(_DBG_INFO, "Device description for >%s< found.\n", tmp );
			usb_initDev( dev, i, handle, dev->usbDev.vendor );
			return handle;
		}
	}
	
	return -1;
}

/**
 * will be called upon sane_exit
 */
static void usbDev_shutdown( Plustek_Device *dev  )
{
	SANE_Int handle;

	DBG( _DBG_INFO, "Shutdown called (dev->fd=%d, %s)\n",
	                dev->fd, dev->sane.name );
	if( NULL == dev->usbDev.ModelStr ) {
		DBG( _DBG_INFO, "Function ignored!\n" );
		return;
	}

	if( SANE_STATUS_GOOD == sanei_usb_open( dev->sane.name, &handle )) {

		dev->fd = handle;

		DBG( _DBG_INFO, "Waiting for scanner-ready...\n" );
		usb_IsScannerReady( dev );
	
		if( 0 != dev->usbDev.bLampOffOnEnd ) {

			DBG( _DBG_INFO, "Switching lamp off...\n" );
			usb_LampOn( dev, SANE_FALSE, SANE_FALSE );
		}
		dev->fd = -1;
		sanei_usb_close( handle );
	}
	usb_StopLampTimer( dev );
}

/**
 * This function checks wether a device, described by a given
 * string(vendor and product ID), is support by this backend or not
 *
 * @param usbIdStr - sting consisting out of product and vendor ID
 *                   format: "0xVVVVx0xPPPP" VVVV = Vendor ID, PPP = Product ID
 * @returns; SANE_TRUE if supported, SANE_FALSE if not
 */
static SANE_Bool usb_IsDeviceInList( char *usbIdStr )
{
	int i;
	
	for( i = 0; NULL != Settings[i].pIDString; i++ ) {

		if( 0 == strncmp( Settings[i].pIDString, usbIdStr, 13 ))
			return SANE_TRUE;
	}
	return SANE_FALSE;
}

/** get last valid entry of a list
 */
static DevList*
getLast( DevList *l )
{
	if( l == NULL )
		return NULL;

	while( l->next != NULL )
		l = l->next;
	return l;
}

/** add a new entry to our internal device list, when a device is detected
 */
static SANE_Status usb_attach( SANE_String_Const dev_name )
{
	int      len;
	DevList *tmp, *last;

	/* get some memory for the entry... */
	len = sizeof(DevList) + strlen(dev_name) + 1;
	tmp = (DevList*)malloc(len);

	/* initialize and set the values */
	memset(tmp, 0, len );

	/* the device name is part of our structure space */
	tmp->dev_name = &((char*)tmp)[sizeof(DevList)];
	strcpy( tmp->dev_name, dev_name );
	tmp->attached = SANE_FALSE;

	/* root set ? */
	if( usbDevs == NULL ) {
		usbDevs = tmp;
	} else {
		last = getLast(usbDevs); /* append... */
		last->next = tmp;
	}
	return SANE_STATUS_GOOD;
}

/**
 */
static void
usbGetList( DevList **devs )
{
	int        i;
	SANE_Bool  il;
	SANE_Word  v, p;
	DevList   *tmp;

	DBG( _DBG_INFO, "Retrieving all supported and conntected devices\n" );
	for( i = 0; NULL != Settings[i].pIDString; i++ ) {

		v = strtol( &(Settings[i].pIDString)[0], 0, 0 );
		p = strtol( &(Settings[i].pIDString)[7], 0, 0 );

		/* check if this vendor- and product-ID has already been added, needed
		 * for Plustek devices - here one product-ID is used for more than one
		 * device type...
		 */
		il = SANE_FALSE;
		for( tmp = *devs; tmp ; tmp = tmp->next ) {
			if( tmp->device_id == p && tmp->vendor_id == v ) {
				il = SANE_TRUE;
				break;
			}
		} 
		if( il ) {
			DBG( _DBG_INFO2, "Already in list: 0x%04x-0x%04x\n", v, p );
			continue;
		}

		/* get the last entry... */
		tmp = getLast(*devs);
		DBG( _DBG_INFO2, "Checking for 0x%04x-0x%04x\n", v, p );
		sanei_usb_find_devices( v, p, usb_attach );

		if( getLast(*devs) != tmp ) {
			
			if( tmp == NULL )
				tmp = *devs;
			else
				tmp = tmp->next;

			while( tmp != NULL ) {
				tmp->vendor_id = v;
				tmp->device_id = p;
				tmp = tmp->next;
			}
		}
	}

	DBG( _DBG_INFO, "Available and supported devices:\n" );
	if( *devs == NULL )
		DBG( _DBG_INFO, "NONE.\n" );

	for( tmp = *devs; tmp; tmp = tmp->next ) {
		DBG( _DBG_INFO, "Device: >%s< - 0x%04xx0x%04x\n", 
			tmp->dev_name, tmp->vendor_id, tmp->device_id );
	}
}

/**
 */
static int usbDev_open( Plustek_Device *dev, DevList *devs, int keep_lock )
{
	char        dn[512];
	char        devStr[50];
	int         result;
	int         i;
	int         lc;
	SANE_Int    handle;
	SANE_Byte   version;
	SANE_Word   vendor, product;
	SANE_Bool   was_empty;
	SANE_Status status;
	DevList    *tmp;

	DBG( _DBG_INFO, "usbDev_open(%s,%s) - %p\n", 
	    dev->name, dev->usbId, (void*)devs );

	/* preset our internal usb device structure */
	memset( &dev->usbDev, 0, sizeof(DeviceDef));

	/* devs is NULL, when called from sane_start */
	if( devs ) {
	
		dn[0] = '\0';
		if( !strcmp( dev->name, "auto" )) {

			/* use the first "unattached"... */
			for( tmp = devs; tmp; tmp = tmp->next ) {
				if( !tmp->attached ) {
					tmp->attached = SANE_TRUE;
					strcpy( dn, tmp->dev_name );
					break;
				}
			}
		} else {

			vendor  = strtol( &dev->usbId[0], 0, 0 );
			product = strtol( &dev->usbId[7], 0, 0 );

			/* check the first match... */
			for( tmp = devs; tmp; tmp = tmp->next ) {
				if( tmp->vendor_id == vendor && tmp->device_id == product ) {
					if( !tmp->attached ) {
						tmp->attached = SANE_TRUE;
						strcpy( dn, tmp->dev_name );
						break;
					}
				}
			}
		}

		if( !dn[0] ) {
			DBG( _DBG_ERROR, "No supported device found!\n" );
			return -1;
		}

		status = sanei_access_lock( dn, 5 );
		if( SANE_STATUS_GOOD != status ) {
			DBG( _DBG_ERROR, "sanei_access_lock failed: %d\n", status );
			return -1;
		}

		status = sanei_usb_open( dn, &handle );
		if( SANE_STATUS_GOOD != status ) {
			DBG( _DBG_ERROR, "sanei_usb_open failed: %s (%d)\n", 
			     strerror(errno), errno);
			sanei_access_unlock( dev->sane.name );
			return -1;
		}

		/* replace the old devname, so we are able to have multiple
		 * auto-detected devices
		 */
		free( dev->name );
		dev->name      = strdup(dn);
		dev->sane.name = dev->name; 

	} else {

		status = sanei_access_lock( dev->sane.name, 5 );
		if( SANE_STATUS_GOOD != status ) {
			DBG( _DBG_ERROR, "sanei_access_lock failed: %d\n", status );
			return -1;
		}

		status = sanei_usb_open( dev->name, &handle );
		if( SANE_STATUS_GOOD != status ) {
			DBG( _DBG_ERROR, "sanei_usb_open failed: %s (%d)\n", 
			     strerror(errno), errno);
			sanei_access_unlock( dev->sane.name );
			return -1;
		}
	}

	was_empty = SANE_FALSE;

	result = sanei_usb_get_vendor_product( handle, &vendor, &product );

	if( SANE_STATUS_GOOD == result ) {

		sprintf( devStr, "0x%04X-0x%04X", vendor, product );

		DBG(_DBG_INFO,"Vendor ID=0x%04X, Product ID=0x%04X\n",vendor,product);

		if( dev->usbId[0] != '\0' ) {
		
			if( 0 != strcmp( dev->usbId, devStr )) {
				DBG( _DBG_ERROR, "Specified Vendor and Product ID "
								 "doesn't match with the ones\n"
								 "in the config file\n" );
				sanei_access_unlock( dev->sane.name );
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
			sanei_access_unlock( dev->sane.name );
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
	if( !usb_IsDeviceInList( dev->usbId )) {
		DBG( _DBG_ERROR, "Device >%s<, is not supported!\n", dev->usbId );
		sanei_access_unlock( dev->sane.name );
		sanei_usb_close( handle );
		return -1;
	}

	status = usbio_DetectLM983x( handle, &version );
	if( SANE_STATUS_GOOD != status ) {
		sanei_usb_close( handle );
		sanei_access_unlock( dev->sane.name );
		return -1;
	}

	if ((version < 3) || (version > 4)) {
		DBG( _DBG_ERROR, "This is not a LM9831 or LM9832 chip based scanner.\n" );
		sanei_usb_close( handle );
		sanei_access_unlock( dev->sane.name );
		return -1;
	}

	/* need to set the handle and the detected chiptype... */
	dev->fd = handle;
	dev->usbDev.HwSetting.chip = (version==3 ? _LM9831:_LM9832);
	usbio_ResetLM983x( dev );
	dev->fd = -1;

	dev->usbDev.vendor  = vendor;
	dev->usbDev.product = product;

	DBG( _DBG_INFO, "Detected vendor & product ID: "
		                "0x%04X-0x%04X\n", vendor, product );

	/*
	 * Plustek uses the misc IO 1/2 to get the PCB ID
	 * (PCB = printed circuit board), so it's possible to have one
	 * product ID and up to 7 different devices...
	 */
	if( 0x07B3 == vendor ) {
	
		handle = usb_CheckForPlustekDevice( handle, dev );
	
		if( was_empty )
			dev->usbId[0] = '\0';

		if( handle >= 0 ) {
			if( !keep_lock )
				sanei_access_unlock( dev->sane.name );
			return handle;
		}
		
	} else {

		/* now roam through the setting list... */
		lc = 13;
		strncpy( devStr, dev->usbId, lc );
		devStr[lc] = '\0';

		if( 0x400 == vendor ) {
			if((dev->adj.mov < 0) || (dev->adj.mov > 1)) {
				DBG(_DBG_INFO, "BearPaw MOV out of range: %d\n", dev->adj.mov);
				dev->adj.mov = 0;
			}
			sprintf( devStr, "%s-%d", dev->usbId, dev->adj.mov );
			lc = strlen(devStr);
			DBG( _DBG_INFO, "BearPaw device: %s (%d)\n", devStr, lc );
		}

		if( was_empty )
			dev->usbId[0] = '\0';

		/* if we don't use the PCD ID extension...
		 */
		for( i = 0; NULL != Settings[i].pIDString; i++ ) {

			if( 0 == strncmp( Settings[i].pIDString, devStr, lc )) {
				DBG( _DBG_INFO, "Device description for >%s< found.\n", devStr );
				usb_initDev( dev, i, handle, vendor );
				if( !keep_lock )
					sanei_access_unlock( dev->sane.name );
			    return handle;
			}
		}
	}

	sanei_access_unlock( dev->sane.name );
	sanei_usb_close( handle );
	DBG( _DBG_ERROR, "No matching device found >%s<\n", devStr );
	return -1;
}

/**
 */
static int usbDev_close( Plustek_Device *dev )
{
	DBG( _DBG_INFO, "usbDev_close()\n" );
	sanei_usb_close( dev->fd );
	dev->fd = -1;
	return 0;
}

/** convert the stuff
 */
static int usbDev_getCaps( Plustek_Device *dev )
{
	DCapsDef *scaps = &dev->usbDev.Caps;

	DBG( _DBG_INFO, "usbDev_getCaps()\n" );

	dev->caps.dwFlag = 0;

	if(((DEVCAPSFLAG_Positive & scaps->wFlags)  &&
	    (DEVCAPSFLAG_Negative & scaps->wFlags)) ||
		(DEVCAPSFLAG_TPA      & scaps->wFlags)) {
		dev->caps.dwFlag |= SFLAG_TPA;
	}

	dev->caps.wMaxExtentX = scaps->Normal.Size.x;
	dev->caps.wMaxExtentY = scaps->Normal.Size.y;
	return 0;
}

/** usbDev_getCropInfo
 * function to set the image relevant stuff
 */
static int usbDev_getCropInfo( Plustek_Device *dev, CropInfo *ci )
{
	WinInfo size;

	DBG( _DBG_INFO, "usbDev_getCropInfo()\n" );

	usb_GetImageInfo( dev, &ci->ImgDef, &size );

	ci->dwPixelsPerLine = size.dwPixels;
	ci->dwLinesPerArea  = size.dwLines;
	ci->dwBytesPerLine  = size.dwBytes;

	if( ci->ImgDef.dwFlag & SCANFLAG_DWORDBoundary )
		ci->dwBytesPerLine = (ci->dwBytesPerLine + 3UL) & 0xfffffffcUL;

	DBG( _DBG_INFO, "PPL = %lu\n",  ci->dwPixelsPerLine );
	DBG( _DBG_INFO, "LPA = %lu\n",  ci->dwLinesPerArea );
	DBG( _DBG_INFO, "BPL = %lu\n",  ci->dwBytesPerLine );

	return 0;
}

/**
 */
static int usbDev_setMap( Plustek_Device *dev, SANE_Word *map,
                          SANE_Word length, SANE_Word channel )
{
	SANE_Word i, idx;

	DBG(_DBG_INFO,"Setting map[%u] at 0x%08lx\n",channel,(unsigned long)map);

	_VAR_NOT_USED( dev );
		
	if( channel == _MAP_MASTER ) {
	
		for( i = 0; i < length; i++ ) {
			a_bMap[i]            = (SANE_Byte)map[i];
			a_bMap[length +i]    = (SANE_Byte)map[i];
			a_bMap[(length*2)+i] = (SANE_Byte)map[i];
		}

	} else {

		idx = 0;
		if( channel == _MAP_GREEN )
			idx = 1;
		if( channel == _MAP_BLUE )
			idx = 2;

		for( i = 0; i < length; i++ ) {
			a_bMap[(length * idx)+i] = (SANE_Byte)map[i];
		}
	}

	return 0;
}

/**
 */
static int
usbDev_setScanEnv( Plustek_Device *dev, ScanInfo *si )
{
	ScanDef  *scan  = &dev->scanning;
	DCapsDef *caps = &dev->usbDev.Caps;

	DBG( _DBG_INFO, "usbDev_setScanEnv()\n" );

	/* clear all the stuff */
	memset( scan, 0, sizeof(ScanDef));

	if((si->ImgDef.dwFlag & SCANDEF_Adf) &&
	   (si->ImgDef.dwFlag & SCANDEF_ContinuousScan)) {
		scan->sParam.dMCLK = dMCLK_ADF;
	}

    /* Save necessary informations */
	scan->fGrayFromColor = 0;

	/* for some devices and settings, we tweak the physical settings
	 * how to get the image - but not in calibration mode
	 */
	if((si->ImgDef.dwFlag & SCANFLAG_Calibration) == 0) {

		if( si->ImgDef.wDataType == COLOR_256GRAY ) {

			if( !(si->ImgDef.dwFlag & SCANDEF_Adf) && !usb_IsCISDevice(dev) &&
			   (caps->OpticDpi.x == 1200 && si->ImgDef.xyDpi.x <= 300)) {
				scan->fGrayFromColor = 2;
				si->ImgDef.wDataType = COLOR_TRUE24;
				DBG( _DBG_INFO, "* Gray from color set!\n" );
			}

			if( caps->workaroundFlag & _WAF_GRAY_FROM_COLOR ) {
				DBG( _DBG_INFO, "* Gray(8-bit) from color set!\n" );
				scan->fGrayFromColor = 2;
				si->ImgDef.wDataType = COLOR_TRUE24;
			}

		} else if ( si->ImgDef.wDataType == COLOR_GRAY16 ) {
			if( caps->workaroundFlag & _WAF_GRAY_FROM_COLOR ) {
				DBG( _DBG_INFO, "* Gray(16-bit) from color set!\n" );
				scan->fGrayFromColor = 2;
				si->ImgDef.wDataType = COLOR_TRUE48;
			}

		} else if ( si->ImgDef.wDataType == COLOR_BW ) {
			if( caps->workaroundFlag & _WAF_BIN_FROM_COLOR ) {
				DBG( _DBG_INFO, "* Binary from color set!\n" );
				scan->fGrayFromColor = 10;
				si->ImgDef.wDataType = COLOR_TRUE24;
			}
		}
	}

	usb_SaveImageInfo( dev, &si->ImgDef );
	usb_GetImageInfo ( dev, &si->ImgDef, &scan->sParam.Size );

	/* mask the flags */
	scan->dwFlag = si->ImgDef.dwFlag & 
	              (SCANFLAG_bgr | SCANFLAG_BottomUp | SCANFLAG_Calibration |
	               SCANFLAG_DWORDBoundary | SCANFLAG_RightAlign |
	               SCANFLAG_StillModule | SCANDEF_Adf | SCANDEF_ContinuousScan);

	if( !(SCANDEF_QualityScan & si->ImgDef.dwFlag)) {
		DBG( _DBG_INFO, "* Preview Mode set!\n" );
	} else {
		DBG( _DBG_INFO, "* Preview Mode NOT set!\n" );
		scan->dwFlag |= SCANDEF_QualityScan;
	}

	scan->sParam.brightness  = si->siBrightness;
	scan->sParam.contrast    = si->siContrast;

	if( scan->sParam.bBitDepth <= 8 )
		scan->dwFlag &= ~SCANFLAG_RightAlign;

	if( scan->dwFlag & SCANFLAG_DWORDBoundary ) {
		if( scan->fGrayFromColor && scan->fGrayFromColor < 10)
			scan->dwBytesLine = (scan->sParam.Size.dwBytes / 3 + 3) & 0xfffffffcUL;
		else
			scan->dwBytesLine = (scan->sParam.Size.dwBytes + 3UL) & 0xfffffffcUL;

	} else {

		if( scan->fGrayFromColor && scan->fGrayFromColor < 10)
			scan->dwBytesLine = scan->sParam.Size.dwBytes / 3;
		else
			scan->dwBytesLine = scan->sParam.Size.dwBytes;
	}

	/* on CIS based devices we have to reconfigure the illumination
	 * settings for the gray modes
	 */
	usb_AdjustCISLampSettings( dev, SANE_TRUE );

	if( scan->dwFlag & SCANFLAG_BottomUp)
		scan->lBufAdjust = -(long)scan->dwBytesLine;
	else
		scan->lBufAdjust = scan->dwBytesLine;

	/* LM9831 has a BUG in 16-bit mode,
	 * so we generate pseudo 16-bit data from 8-bit
	 */
	if( scan->sParam.bBitDepth > 8 ) {

		if( _LM9831 == dev->usbDev.HwSetting.chip ) {

			scan->sParam.bBitDepth = 8;
			scan->dwFlag |= SCANFLAG_Pseudo48;
			scan->sParam.Size.dwBytes >>= 1;
		}
	}

	/* Source selection */
	if( scan->sParam.bSource == SOURCE_Reflection ) {

		dev->usbDev.pSource = &caps->Normal;
		scan->sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x +
		                                      (u_long)dev->usbDev.Normal.lLeft;
		scan->sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y +
		                                        (u_long)dev->usbDev.Normal.lUp;

	} else if( scan->sParam.bSource == SOURCE_Transparency ) {

		dev->usbDev.pSource = &caps->Positive;
		scan->sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x +
		                                    (u_long)dev->usbDev.Positive.lLeft;
		scan->sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y +
		                                      (u_long)dev->usbDev.Positive.lUp;

	} else if( scan->sParam.bSource == SOURCE_Negative ) {

		dev->usbDev.pSource = &caps->Negative;
		scan->sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x +
		                                    (u_long)dev->usbDev.Negative.lLeft;
		scan->sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y +
		                                      (u_long)dev->usbDev.Negative.lUp;

	} else {

		dev->usbDev.pSource = &dev->usbDev.Caps.Adf;
		scan->sParam.Origin.x += dev->usbDev.pSource->DataOrigin.x +
		                                      (u_long)dev->usbDev.Normal.lLeft;
		scan->sParam.Origin.y += dev->usbDev.pSource->DataOrigin.y +
		                                        (u_long)dev->usbDev.Normal.lUp;
	}

	if( scan->sParam.bSource == SOURCE_ADF ) {

		if( scan->dwFlag & SCANDEF_ContinuousScan )
			dev->usbDev.fLastScanIsAdf = SANE_TRUE;
		else
			dev->usbDev.fLastScanIsAdf = SANE_FALSE;
	}

	return 0;
}

/**
 */
static int
usbDev_stopScan( Plustek_Device *dev )
{
	DBG( _DBG_INFO, "usbDev_stopScan()\n" );

	/* in cancel-mode we first stop the motor */
	usb_ScanEnd( dev );

	dev->scanning.dwFlag = 0;

	if( NULL != dev->scanning.pScanBuffer ) {

		free( dev->scanning.pScanBuffer );
		dev->scanning.pScanBuffer = NULL;
		usb_StartLampTimer( dev );
	}
	return 0;
}

/**
 */
static int
usbDev_startScan( Plustek_Device *dev )
{
	ScanDef *scan = &dev->scanning;
	DBG( _DBG_INFO, "usbDev_startScan()\n" );

/* HEINER: Preview currently not working correctly */
#if 0
	if( scan->dwFlag & SCANDEF_QualityScan )
		dev->usbDev.a_bRegs[0x0a] = 0;
	else
		dev->usbDev.a_bRegs[0x0a] = 1;
#endif
	dev->usbDev.a_bRegs[0x0a] = 0;

	if((scan->dwFlag & SCANDEF_Adf) && (scan->dwFlag & SCANDEF_ContinuousScan)) {
		scan->fCalibrated = SANE_TRUE;
	} else {
		scan->fCalibrated = SANE_FALSE;
	}

	scan->sParam.PhyDpi.x = usb_SetAsicDpiX(dev,scan->sParam.UserDpi.x);
	scan->sParam.PhyDpi.y = usb_SetAsicDpiY(dev,scan->sParam.UserDpi.y);

	/* Allocate shading/scan buffer */
	scan->pScanBuffer = (u_long*)malloc( _SCANBUF_SIZE );

	if( scan->pScanBuffer == NULL )
		return _E_ALLOC;

	scan->dwFlag |= SCANFLAG_StartScan;

	/* some devices (esp. BearPaw) do need a lamp switch off before
	 * switching it on again. Otherwise it might happen that the lamp
	 * remains off
	 */
	if(dev->usbDev.Caps.workaroundFlag & _WAF_LOFF_ON_START) {
		if (usb_GetLampStatus(dev))
			usb_LampOn( dev, SANE_FALSE, SANE_TRUE );
	}

	usb_LampOn( dev, SANE_TRUE, SANE_TRUE );

	m_fStart    = m_fFirst = SANE_TRUE;
	m_fAutoPark = (scan->dwFlag&SCANFLAG_StillModule)?SANE_FALSE:SANE_TRUE;

	if( usb_IsSheetFedDevice(dev))
		if(usb_InCalibrationMode(dev))
			m_fAutoPark = SANE_FALSE;

	usb_StopLampTimer( dev );
	return 0;
}

/**
 * do the reading stuff here...
 * first we perform the calibration step, and then we read the image
 * line for line
 */
static int
usbDev_Prepare( Plustek_Device *dev, SANE_Byte *buf )
{
	int       result;
	SANE_Bool use_alt_cal = SANE_FALSE;
	ScanDef  *scan        = &dev->scanning;
	DCapsDef *scaps       = &dev->usbDev.Caps;
	HWDef    *hw          = &dev->usbDev.HwSetting;

	DBG( _DBG_INFO, "usbDev_PrepareScan()\n" );

	/* check the current position of the sensor and move it back
	 * to it's home position if necessary...
	 */
	if( !usb_IsSheetFedDevice(dev))
		usb_SensorStatus( dev );

	/* CIS devices need special handling... */
	if( usb_IsCISDevice(dev)) {
		use_alt_cal = SANE_TRUE;
		
	} else {

		if( dev->adj.altCalibrate ) 
			use_alt_cal = SANE_TRUE;
	}

	/* for the skip functionality use the "old" calibration functions */
	if( dev->usbDev.Caps.workaroundFlag & _WAF_BYPASS_CALIBRATION ) {
		use_alt_cal = SANE_FALSE;
	}

	if( use_alt_cal ) {
		result = cano_DoCalibration( dev );
	} else {
		result = usb_DoCalibration( dev );
	}

	if( SANE_TRUE != result ) {
		DBG( _DBG_ERROR, "calibration failed!!!\n" );
		return _E_ABORT;
	}

	if( dev->adj.cacheCalData )
		usb_SaveCalData( dev );

	DBG( _DBG_INFO, "calibration done.\n" );
	if( usb_InCalibrationMode(dev))
		return 0;

	if( !(scan->dwFlag & SCANFLAG_Scanning)) {

		usleep( 10 * 1000 );

		/* need to preset that here, as we need it during parameter setting
		 */
		scan->bLinesToSkip = (u_char)(scan->sParam.PhyDpi.y / 50);

		scan->dwLinesDiscard = 0;
		if( scan->sParam.bChannels == 3 ) {
			scan->dwLinesDiscard = (u_long)scaps->bSensorDistance *
			                       scan->sParam.PhyDpi.y / scaps->OpticDpi.y;
			scan->dwLinesDiscard <<= 1;
		}

		if( !usb_SetScanParameters( dev, &scan->sParam )) {
			DBG( _DBG_ERROR, "Setting Scan Parameters failed!\n" );
			return 0;
		}

		/* if we bypass the calibration step, we wait on lamp warmup here...
		 */
		if( scaps->workaroundFlag & _WAF_BYPASS_CALIBRATION ) {
			if( !usb_Wait4Warmup( dev )) {
				DBG( _DBG_INFO, "usbDev_Prepare() - Cancel detected...\n" );
				return 0;
			}
		}

		scan->pbScanBufBegin = (u_char*)scan->pScanBuffer;

		if((dev->caps.dwFlag & SFLAG_ADF) && (scaps->OpticDpi.x == 600))
			scan->dwLinesScanBuf = 8;
		else
			scan->dwLinesScanBuf = 32;

		scan->dwBytesScanBuf     = scan->dwLinesScanBuf *
		                           scan->sParam.Size.dwPhyBytes;

		scan->dwNumberOfScanBufs = _SCANBUF_SIZE / scan->dwBytesScanBuf;
		scan->dwLinesPerScanBufs = scan->dwNumberOfScanBufs *
		                           scan->dwLinesScanBuf;
		scan->pbScanBufEnd       = scan->pbScanBufBegin +
		                           scan->dwLinesPerScanBufs *
		                           scan->sParam.Size.dwPhyBytes;
		scan->dwRedShift   = 0;
		scan->dwBlueShift  = 0;
		scan->dwGreenShift = 0;

		/* CCD scanner */
		if( scan->sParam.bChannels == 3 ) {

			scan->dwLinesDiscard = (u_long)scaps->bSensorDistance *
			                       scan->sParam.PhyDpi.y / scaps->OpticDpi.y;

			switch( scaps->bSensorOrder ) {

			case SENSORORDER_rgb:
				scan->Red.pb   = scan->pbScanBufBegin;
				scan->Green.pb = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes;
				scan->Blue.pb  = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes * 2UL;
				break;

			case SENSORORDER_rbg:
				scan->Red.pb   = scan->pbScanBufBegin;
				scan->Blue.pb  = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes;
				scan->Green.pb = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes * 2UL;
				break;

			case SENSORORDER_gbr:
				scan->Green.pb = scan->pbScanBufBegin;
				scan->Blue.pb  = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes;
				scan->Red.pb   = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes * 2UL;
				break;

			case SENSORORDER_grb:
				scan->Green.pb = scan->pbScanBufBegin;
				scan->Red.pb   = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes;
				scan->Blue.pb  = scan->pbScanBufBegin + scan->dwLinesDiscard *
                                 scan->sParam.Size.dwPhyBytes * 2UL;
				break;

			case SENSORORDER_brg:
				scan->Blue.pb  = scan->pbScanBufBegin;
				scan->Red.pb   = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes;
				scan->Green.pb = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes * 2UL;
				break;

			case SENSORORDER_bgr:
				scan->Blue.pb  = scan->pbScanBufBegin;
				scan->Green.pb = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes;
				scan->Red.pb   = scan->pbScanBufBegin + scan->dwLinesDiscard *
				                 scan->sParam.Size.dwPhyBytes * 2UL;
			}

			/* double it for last channel */
			scan->dwLinesDiscard <<= 1;
			scan->dwGreenShift = (7UL+scan->sParam.bBitDepth) >> 3;
			scan->Green.pb += scan->dwGreenShift;
			scan->Blue.pb  += (scan->dwGreenShift * 2);

			if( scan->dwFlag & SCANFLAG_bgr) {

				u_char *pb = scan->Blue.pb;

				scan->Blue.pb = scan->Red.pb;
				scan->Red.pb  = pb;
				scan->dwBlueShift = 0;
				scan->dwRedShift  = scan->dwGreenShift << 1;
			} else {
				scan->dwRedShift  = 0;
				scan->dwBlueShift = scan->dwGreenShift << 1;
			}

		} else {

			/* CIS section */

			/* this might be a simple gray operation or AFE 1 channel op */
			scan->dwLinesDiscard = 0;
			scan->Green.pb       = scan->pbScanBufBegin;

			if(( scan->sParam.bDataType == SCANDATATYPE_Color ) &&
			   ( hw->bReg_0x26 & _ONE_CH_COLOR )) {

				u_char so;
				u_long len = scan->sParam.Size.dwPhyBytes / 3;

				so = scaps->bSensorOrder;
				if (_WAF_RESET_SO_TO_RGB & scaps->workaroundFlag) {
					if (scaps->bPCB != 0) {
						if (scan->sParam.PhyDpi.x > scaps->bPCB) {
							so = SENSORORDER_rgb;
							DBG(_DBG_INFO, "* Resetting sensororder to RGB\n");
						}
					}
				}

				switch( so ) {

				case SENSORORDER_rgb:
					scan->Red.pb   = scan->pbScanBufBegin;
					scan->Green.pb = scan->pbScanBufBegin + len;
					scan->Blue.pb  = scan->pbScanBufBegin + len * 2UL;
					break;

				case SENSORORDER_gbr:
					scan->Green.pb = scan->pbScanBufBegin;
					scan->Blue.pb  = scan->pbScanBufBegin + len;
					scan->Red.pb   = scan->pbScanBufBegin + len * 2UL;
					break;
				default:
					DBG( _DBG_ERROR, "CIS: This bSensorOrder "
					                 "is not defined\n" );
					return _E_INTERNAL;
				}
			}
		}

		/* set a funtion to process the RAW data... */
		usb_GetImageProc( dev );

		if( scan->sParam.bSource == SOURCE_ADF )
			scan->dwFlag |= SCANFLAG_StillModule;

		DBG( _DBG_INFO, "* scan->dwFlag=0x%08lx\n", scan->dwFlag );

		if( !usb_ScanBegin( dev,
			(scan->dwFlag&SCANFLAG_StillModule) ? SANE_FALSE:SANE_TRUE)) {
	
			return _E_INTERNAL;
		}
		
		scan->dwFlag |= SCANFLAG_Scanning;

		if( scan->sParam.UserDpi.y != scan->sParam.PhyDpi.y ) {
		
			if( scan->sParam.UserDpi.y < scan->sParam.PhyDpi.y ) {
			
				scan->wSumY = scan->sParam.PhyDpi.y - scan->sParam.UserDpi.y;
				scan->dwFlag |= SCANFLAG_SampleY;
				DBG( _DBG_INFO, "SampleY Flag set (%u != %u, wSumY=%u)\n",
				  scan->sParam.UserDpi.y, scan->sParam.PhyDpi.y, scan->wSumY );
			}
		}
	}

	dumpPicInit( &scan->sParam, "plustek-pic.raw" );

	/* here the NT driver uses an extra reading thread...
	 * as the SANE stuff already forked the driver to read data, I think
	 * we should only read data by using a function...
	 */
	scan->dwLinesUser = scan->sParam.Size.dwLines; 
	if( !scan->dwLinesUser )
		return _E_BUFFER_TOO_SMALL;

	if( scan->sParam.Size.dwLines < scan->dwLinesUser )
		scan->dwLinesUser = scan->sParam.Size.dwLines;

	scan->sParam.Size.dwLines -= scan->dwLinesUser;
	if( scan->dwFlag & SCANFLAG_BottomUp )
		scan->UserBuf.pb = buf + (scan->dwLinesUser - 1) * scan->dwBytesLine;
	else
		scan->UserBuf.pb = buf;

	DBG(_DBG_INFO,"Reading the data now!\n" );
	DBG(_DBG_INFO,"PhyDpi.x         = %u\n", scan->sParam.PhyDpi.x  );	
	DBG(_DBG_INFO,"PhyDpi.y         = %u\n", scan->sParam.PhyDpi.y  );	
	DBG(_DBG_INFO,"UserDpi.x        = %u\n", scan->sParam.UserDpi.x );	
	DBG(_DBG_INFO,"UserDpi.y        = %u\n", scan->sParam.UserDpi.y );	
	DBG(_DBG_INFO,"NumberOfScanBufs = %lu\n",scan->dwNumberOfScanBufs);
	DBG(_DBG_INFO,"LinesPerScanBufs = %lu\n",scan->dwLinesPerScanBufs);
	DBG(_DBG_INFO,"dwPhyBytes       = %lu\n",scan->sParam.Size.dwPhyBytes);
	DBG(_DBG_INFO,"dwPhyPixels      = %lu\n",scan->sParam.Size.dwPhyPixels);
	DBG(_DBG_INFO,"dwTotalBytes     = %lu\n",scan->sParam.Size.dwTotalBytes);
	DBG(_DBG_INFO,"dwPixels         = %lu\n",scan->sParam.Size.dwPixels);
	DBG(_DBG_INFO,"dwBytes          = %lu\n",scan->sParam.Size.dwBytes);
	DBG(_DBG_INFO,"dwValidPixels    = %lu\n",scan->sParam.Size.dwValidPixels);
	DBG(_DBG_INFO,"dwBytesScanBuf   = %lu\n",scan->dwBytesScanBuf );
	DBG(_DBG_INFO,"dwLinesDiscard   = %lu\n",scan->dwLinesDiscard );
	DBG(_DBG_INFO,"dwLinesToSkip    = %u\n", scan->bLinesToSkip );
	DBG(_DBG_INFO,"dwLinesUser      = %lu\n",scan->dwLinesUser  );
	DBG(_DBG_INFO,"dwBytesLine      = %lu\n",scan->dwBytesLine  );

	scan->pbGetDataBuf = scan->pbScanBufBegin;

	scan->dwLinesToProcess = usb_ReadData( dev  );
	if( 0 == scan->dwLinesToProcess )
		return _E_DATAREAD;

	return 0;
}

/** as the name says, read one line...
 */
static int
usbDev_ReadLine( Plustek_Device *dev )
{
	int      wrap;
	u_long   cur;
	ScanDef *scan = &dev->scanning;
	HWDef   *hw   = &dev->usbDev.HwSetting;

	cur = scan->dwLinesUser;

	/* we stay within this sample loop until one line has been processed for
	 * the user...
	 */
	while( cur == scan->dwLinesUser ) {

		if( usb_IsEscPressed()) {
			DBG( _DBG_INFO, "readLine() - Cancel detected...\n" );
			return _E_ABORT;
		}

		if( !(scan->dwFlag & SCANFLAG_SampleY))	{

			scan->pfnProcess( dev );

			/* Adjust user buffer pointer */
			scan->UserBuf.pb += scan->lBufAdjust;
			scan->dwLinesUser--;

		} else {

			scan->wSumY += scan->sParam.UserDpi.y;

			if( scan->wSumY >= scan->sParam.PhyDpi.y ) {
				scan->wSumY -= scan->sParam.PhyDpi.y;

				scan->pfnProcess( dev );

				/* Adjust user buffer pointer */
				scan->UserBuf.pb += scan->lBufAdjust;
				scan->dwLinesUser--;
			}
		}

		/* Adjust get buffer pointers */
		wrap = 0;

		if( scan->sParam.bDataType == SCANDATATYPE_Color ) {

			scan->Red.pb += scan->sParam.Size.dwPhyBytes;
			if( scan->Red.pb >= scan->pbScanBufEnd ) {
				scan->Red.pb = scan->pbScanBufBegin + scan->dwRedShift;
				wrap = 1;
			}

			scan->Green.pb += scan->sParam.Size.dwPhyBytes;
			if( scan->Green.pb >= scan->pbScanBufEnd ) {
				scan->Green.pb = scan->pbScanBufBegin + scan->dwGreenShift;
				wrap = 1;
			}

			scan->Blue.pb += scan->sParam.Size.dwPhyBytes;
			if( scan->Blue.pb >= scan->pbScanBufEnd ) {
				scan->Blue.pb = scan->pbScanBufBegin + scan->dwBlueShift;
				wrap = 1;
			}
		} else {
			scan->Green.pb += scan->sParam.Size.dwPhyBytes;
			if( scan->Green.pb >= scan->pbScanBufEnd )
				scan->Green.pb = scan->pbScanBufBegin + scan->dwGreenShift;
		}

		/* on any wrap-around of the get pointers in one channel mode
		 * we have to reset them
		 */
		if( wrap ) {

			u_long len = scan->sParam.Size.dwPhyBytes;

			if( hw->bReg_0x26 & _ONE_CH_COLOR ) {

				if(scan->sParam.bDataType == SCANDATATYPE_Color) {
					len /= 3;
				}
				scan->Red.pb   = scan->pbScanBufBegin;
				scan->Green.pb = scan->pbScanBufBegin + len;
				scan->Blue.pb  = scan->pbScanBufBegin + len * 2UL;
			}
		}

		/* line processed, check if we have to get more...
		 */
		scan->dwLinesToProcess--;

		if( 0 == scan->dwLinesToProcess ) {

			scan->dwLinesToProcess = usb_ReadData( dev );
			if( 0 == scan->dwLinesToProcess ) {

				if( usb_IsEscPressed())
					return _E_ABORT;
			}
		}
	}
	return 0;
}

/* END PLUSTEK-USB.C ........................................................*/
