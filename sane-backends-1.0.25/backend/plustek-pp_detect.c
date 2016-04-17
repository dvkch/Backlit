/* @file plustek-pp_detect.c
 * @brief automatic scanner detection
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
 * - 0.33 - added portmode check
 * - 0.34 - no changes
 * - 0.35 - no changes
 * - 0.36 - added some debug messages
 *        - replace the old _OUTB/_INB macros
 * - 0.37 - cosmetic changes
 *        - added speed-test for the parallel-port
 * - 0.38 - added P12 stuff - replaced detectP9636 by detectAsic9800x
 *        - added detectResetPort() function
 * - 0.39 - fixed problem in ASIC9800x detection
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - changed include names
 * - 0.43 - cleanup
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

/************************** local definitions ********************************/

/*************************** local functions *********************************/

/** as the name says...
 */
static void detectResetPort( pScanData ps )
{
	UChar control;

	DBG( DBG_HIGH, "ResetPort()\n" );

	control = _INB_CTRL( ps );
	_DO_UDELAY( 2 );

	_OUTB_CTRL( ps, _CTRL_RESERVED );   /* reset, 0xc0      */
	_DO_UDELAY( 2 );

	_OUTB_CTRL( ps, control );          /* and restore...   */
	_DO_UDELAY( 2 );
}

/** Check: will the status port changed between printer/scanner path changed?
 *  Write out data and read in to compare
 */
static int detectScannerConnection( pScanData ps )
{
	UChar data, control, status;
	int   retval = _E_NO_CONN;

#ifdef __KERNEL__
	DBG( DBG_LOW, "Dataport = 0x%04x\n", ps->IO.pbSppDataPort );
	DBG( DBG_LOW, "Ctrlport = 0x%04x\n", ps->IO.pbControlPort );
#endif

	detectResetPort( ps );

	/*
	 * as we're called during InitPorts, we can be sure
	 * to operate in EPP-mode (hopefuly ;-)
	 */
	control = _INB_CTRL( ps );

	/*
	 * go ahead and do some checks
	 */
	_OUTB_CTRL( ps, _CTRL_GENSIGNAL );
	_DO_UDELAY( 5 );

	_OUTB_DATA( ps, 0x55 );
	_DO_UDELAY( 5 );

	data = _INB_DATA( ps );

	if (0x55 == data) {

		DBG( DBG_HIGH, "Test 0x55\n" );

    	_OUTB_DATA( ps, 0xAA );
		_DO_UDELAY( 5 );

	   	data = _INB_DATA( ps );

    	if (0xAA == data) {

			DBG( DBG_HIGH, "Test 0xAA\n" );

			_OUTB_DATA( ps, 0x0 );
			_DO_UDELAY( 5 );

			data = _INB_STATUS( ps );

			ps->OpenScanPath( ps );

			_OUTB_DATA( ps, 0x0 );
			_DO_UDELAY( 5 );

			status = _INB_STATUS( ps );

			ps->CloseScanPath( ps );

			/*
	 		 * so we're done 'til now...
			 */
			DBG( DBG_HIGH, "Compare data=0x%x and status=0x%x, port=0x%x\n",
		  				  data, status, ps->IO.portBase );

			if( data != status ) {

				_ASSERT( ps->ReadWriteTest );

				/*
				 * here we try to detect the operation speed of our parallel 
				 * port if we have tested all the stuff and had no success, 
				 * retval will contain the error-code
                 */
				for( ps->IO.delay = 0; ps->IO.delay < 5; ps->IO.delay++ ) {

					retval = ps->ReadWriteTest( ps );

					/* break on OK or when the ASIC detection fails */
					if((_OK == retval) ||  (_E_NO_ASIC == retval))
						break;
				}
			}
		}
	}

	/* work on the result */
	if ( _OK == retval ) {
#ifdef __KERNEL__
		ps->sCaps.wIOBase = ps->IO.pbSppDataPort;
#else
		ps->sCaps.wIOBase = ps->pardev;
#endif
		ps->PutToIdleMode( ps );

	} else {
    	ps->sCaps.wIOBase = _NO_BASE;
	}

	/*
	 * restore control port value
	 */
	_OUTB_CTRL( ps, control );
	_DO_UDELAY( 5 );

	DBG( DBG_HIGH, "detectScannerConnection() returns %i.\n", retval );

	return retval;
}

/** we need some memory...
 */
static int detectSetupBuffers( pScanData ps )
{
	DBG( DBG_LOW, "*** setupBuffers ***\n" );

    /* bad news ?
     */
    if ( 0 == ps->TotalBufferRequire ) {

#ifdef __KERNEL__    
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif        
        "pt_drv: asic 0x%x probably not supported\n", ps->sCaps.AsicID);

        return _E_ALLOC;  /* Out of memory */

    } else {

		/*
		 * allocate and clear
		 */
		DBG(DBG_LOW,"Driverbuf(%u bytes) needed !\n", ps->TotalBufferRequire);
        ps->driverbuf = (pUChar)_VMALLOC(ps->TotalBufferRequire);

        if ( NULL == ps->driverbuf ) {

#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
             "pt_drv: Not enough kernel memory %d\n",
                    ps->TotalBufferRequire);
            return _E_ALLOC;  /* Out of memory */
        }

		memset( ps->driverbuf, 0, ps->TotalBufferRequire );
    }

    ps->pPrescan16   = ps->driverbuf;
    ps->pPrescan8    = ps->pPrescan16 + ps->BufferFor1stColor;
    ps->pScanBuffer1 = ps->pPrescan8  + ps->BufferFor2ndColor;

/* CHECK: Should we adjust that !!!
*/
    ps->pEndBufR       = ps->pPrescan8;
    ps->pEndBufG       = ps->pScanBuffer1;
    ps->pColorRunTable = ps->pScanBuffer1 + ps->BufferForDataRead1;

	DBG( DBG_LOW, "pColorRunTab = 0x%0lx - 0x%0lx\n",
			(unsigned long)ps->pColorRunTable,
			(unsigned long)((pUChar)ps->driverbuf + ps->TotalBufferRequire));

    if ( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

		DBG( DBG_LOW, "Adjust for 98001 ASIC\n" );

        ps->pScanBuffer2   = ps->pPrescan16;
        ps->pScanBuffer1   = ps->pScanBuffer2 + _LINE_BUFSIZE1;
        ps->pColorRunTable = ps->pScanBuffer1 + _LINE_BUFSIZE * 2UL;
        ps->pProcessingBuf = ps->pColorRunTable + ps->BufferForColorRunTable;
        DBG( DBG_LOW, "sb2 = 0x%lx, sb1 = 0x%lx, Color = 0x%lx\n",
					(unsigned long)ps->pScanBuffer2,
					(unsigned long)ps->pScanBuffer1,
					(unsigned long)ps->pColorRunTable );
        DBG( DBG_LOW, "Pro = 0x%lx, size = %d\n",
					(unsigned long)ps->pProcessingBuf, ps->TotalBufferRequire );

        ps->dwShadow = (_DEF_BRIGHTEST_SKIP + _DEF_DARKEST_SKIP) * 5400UL * 2UL * 3UL;

        ps->Shade.pHilight = _VMALLOC( ps->dwShadow );

        if ( NULL != ps->Shade.pHilight ) {

			memset( ps->Shade.pHilight, 0, ps->dwShadow );

            ps->dwHilight   = _DEF_BRIGHTEST_SKIP * 5400UL * 3UL;
            ps->dwShadow    = _DEF_DARKEST_SKIP   * 5400UL * 3UL;
            ps->pwShadow    = (pUShort)ps->Shade.pHilight + ps->dwHilight;
            ps->Shade.dwDiv = 32UL - _DEF_BRIGHTEST_SKIP - _DEF_DARKEST_SKIP;

            ps->dwHilightCh = ps->dwHilight / 3UL;
            ps->dwShadowCh  = ps->dwShadow  / 3UL;
        }
    } else if ( _ASIC_IS_98003 == ps->sCaps.AsicID ) {

		DBG( DBG_LOW, "Adjust for 98003 ASIC\n" );

        ps->Bufs.b1.pReadBuf = ps->driverbuf;
    	ps->Bufs.b2.pSumBuf  = ps->Bufs.b1.pReadBuf + _SizeDataBuf;
        ps->Bufs.TpaBuf.pb   = &((pUChar)ps->Bufs.b2.pSumBuf)[_SizeShadingSumBuf];

/* CHECK: We might should play around with these values... */
        ps->Shade.skipHilight = _DEF_BRIGHTEST_SKIP;
        ps->Shade.skipShadow  = _DEF_DARKEST_SKIP;

    	if( ps->Shade.skipHilight && ps->Shade.skipShadow ) {

            ULong skipSize;

	        skipSize = (ULong)((ps->Shade.skipHilight + ps->Shade.skipShadow)
                                                           * _SizeDataBuf * 3);

    	    ps->Shade.pHilight = _VMALLOC( skipSize );

            if( NULL != ps->Shade.pHilight ) {
    	        ps->Shade.dwDiv = (ULong)(32UL - ps->Shade.skipHilight -
                                                        ps->Shade.skipShadow);
            }
        } else
    	    ps->Shade.pHilight = NULL;
    }

    return _OK;
}

/** model 48xx detection or any other model using the 96001/3 ASIC
 */
static int detectP48xx( pScanData ps )
{
	int result;

	DBG( DBG_LOW, "************ DETECTP48xx ************\n" );

	/* increase the delay-time */
	ps->IO.delay = 4;

	ModelSet4800( ps );

	result = P48xxInitAsic( ps );
	if( _OK != result )
		return result;

	return detectScannerConnection( ps );
}

/** ASIC 98003 model detection
 */
static int detectAsic98003( pScanData ps )
{
	int  result;

	DBG( DBG_LOW, "************* ASIC98003 *************\n" );

	/* increase the delay-time */
	ps->IO.delay = 4;

   	ModelSetP12( ps );

    result = P12InitAsic( ps );
	if( _OK != result )
		return result;

    IOSoftwareReset( ps );

	return detectScannerConnection( ps );
}

/** ASIC 98001 model detection
 */
static int detectAsic98001( pScanData ps )
{
	int  result;

	DBG( DBG_LOW, "************* ASIC98001 *************\n" );

	/* increase the delay-time */
	ps->IO.delay = 4;

    ModelSet9636( ps );

   	result = P9636InitAsic( ps );
#ifndef _ASIC_98001_SIM
	if( _OK != result )
		return result;

	return detectScannerConnection( ps );
#else
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
			"!!!! WARNING, have a look at function detectAsic98001() !!!!\n" );
   	ps->sCaps.AsicID  =  _ASIC_IS_98001;
  	ps->sCaps.wIOBase = ps->IO.pbSppDataPort;
    return _OK;
#endif
}

/************************ exported functions *********************************/

/** here we try to find the scanner, depending on the mode
 */
_LOC int DetectScanner( pScanData ps, int mode )
{
    Byte asic;
	int  result = _E_INTERNAL;

	/*
	 * before doing anything else, check the port-mode
	 */
	if((ps->IO.portMode != _PORT_EPP) && (ps->IO.portMode != _PORT_SPP) &&
	   (ps->IO.portMode != _PORT_BIDI)) {

		DBG( DBG_LOW, "!!! Portmode (%u)not supported !!!\n", ps->IO.portMode );
		return _E_INTERNAL;
	}

	/* autodetection ?
	 */
	if( 0 == mode ) {

		DBG( DBG_HIGH, "Starting Scanner-Autodetection\n" );

		/* try to find a 48xx Scanner 
		 * (or even a scanner based on the 96001/3) ASIC
		 */
		result = detectP48xx( ps );

		if( _OK != result ) {

        	DBG( DBG_LOW, "************* ASIC9800x *************\n" );

            /* get the ASIC ID by using the OpenScanPath stuff from Asic9600x based
             * models - only difference: change the ReadHigh/ReadLow signals before
             */
        	ps->CtrlReadHighNibble = _CTRL_GENSIGNAL+_CTRL_AUTOLF+_CTRL_STROBE;
            ps->CtrlReadLowNibble  = _CTRL_GENSIGNAL+_CTRL_AUTOLF;

            /* read Register 0x18 (AsicID Register) of Asic9800x based devices */
#ifdef _ASIC_98001_SIM
#ifdef __KERNEL__
			_PRINT(
#else
			DBG( DBG_HIGH,
#endif
						"!!!! WARNING, SW-Emulation active !!!!\n" );
            asic = _ASIC_IS_98001;
#else
            detectResetPort( ps );

            /* do some presettings to make IODataRegisterFromScanner() work */
            ps->RegAsicID        = 0x18;
            ps->IO.useEPPCmdMode = _FALSE;
            ps->sCaps.AsicID     = _ASIC_IS_98001;
            IOInitialize( ps );

            asic = IODataRegisterFromScanner( ps, ps->RegAsicID );

            DBG( DBG_HIGH, "ASIC = 0x%02X\n", asic  );
#endif

            /* depending on what we have found, perform some extra tests */
   	        switch( asic ) {

       	    case _ASIC_IS_98001:
           	    result = detectAsic98001( ps );
               	break;

            case _ASIC_IS_98003:

                /* as the reading of the ASIC ID causes trouble,
                 * we reset the device
                 */
            	ps->IO.useEPPCmdMode = _FALSE;
            	ps->sCaps.AsicID     = _ASIC_IS_98003;
            	IOInitialize( ps );
			    IOSoftwareReset( ps );

   	            result = detectAsic98003( ps );
       	        break;

            default:
   				DBG( DBG_HIGH, "Unknown ASIC-ID\n" );
       	        result = _E_NO_DEV;
           	    break;
            }
		}

	} else {

        /* this will be called each time before operating on a previously
         * detected device, to make sure we are still operating on the same one
         */
		if( _ASIC_IS_98001 == mode ) {

			DBG( DBG_HIGH, "Starting Scanner-detection (ASIC 98001)\n" );
			result = detectAsic98001( ps );

        } else if( _ASIC_IS_98003 == mode ) {

			DBG( DBG_HIGH, "Starting Scanner-detection (ASIC 98003)\n" );
			result = detectAsic98003( ps );

		} else {

			DBG( DBG_HIGH, "Starting Scanner-detection (ASIC 96001/3)\n" );
			result = detectP48xx( ps );
		}
	}

	if( _OK == result ) {

		_ASSERT( ps->SetupScannerVariables );
		ps->SetupScannerVariables( ps );

		detectSetupBuffers( ps );

	} else {
/* CHECK - we should not need that anymore - paranoia code ??!!!!
*/
		ps->sCaps.wIOBase = _NO_BASE;
	}

	DBG( DBG_LOW, "*** DETECTION DONE, result: %i ***\n", result );
	return result;
}

/* END PLUSTEK-PP_DETECT.C ..................................................*/
