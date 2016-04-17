/* @file plustek-pp_p48xx.c
 * @brief here we have all functionality according to the ASIC96001/3 based
 *        models.
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - fixed a bug for the return value in p48xxDoTest
 *        - added additional debug messages
 *        - added function p48xxCheck4800Memory
 * - 0.32 - added debug messages
 *        - fixed a bug in p48xxDoTest
 *        - disabled RD_WatchDogControl, lamp will be controlled by driver
 * - 0.33 - added function p48xxSetAsicRegisters()
 *        - fixed a bug in p48xxDoTest (reset the ASIC registers)
 *        - removed p48xxPositionLamp
 * - 0.34 - added some comments
 * - 0.35 - added some comments
 * - 0.36 - added function p48xxInitAllModules() to allow reinit of the modules
 *        - switching from Full- to Halfstep at ps->PhysicalDpi now in
 *        - p48xxSetGeneralRegister
 *        - fixed the color-inverse problem for model OP4800
 * - 0.37 - move p48xxOpenScanPath, p48xxCloseScanPath
 *          and p48xxRegisterToScanner to io.c
 *        - removed // comments
 *        - added override for A3I scanner
 * - 0.38 - added function p48xxPutToIdleMode()
 *        - added function p48xxCalibration
 * - 0.39 - added A3I stuff
 * - 0.40 - disabled A3I stuff
 * - 0.41 - no changes
 * - 0.42 - changed include names
 * - 0.43 - no changes
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

/*************************** some definitions ********************************/

#define _TEST_SZ   2048  	  /* always use 2048 for mem size (= one bank)   */
#define _START_VAL 0x12345678 /* pick a non-zero starting value for our long */

#define _BankAndSizeForTest	_MemBankSize2k  /* always use 2k for mem test */

/*************************** local functions *********************************/

/*.............................................................................
 * 1) Set asic to PROGRAM mode
 * 2) Select the memory bank and size
 * 3) Initiate data fifo
 */
static void p48xxSetMemoryBankForProgram( pScanData ps , Byte bBankAndSize )
{
	/* enter program mode */
	IODataToRegister( ps, ps->RegModeControl, _ModeProgram );

	/* bank and size */
    IODataToRegister( ps, ps->RegMemAccessControl, bBankAndSize );

	/* initiate data fifo */
    IORegisterToScanner( ps, ps->RegInitDataFifo );
}

/*.............................................................................
 * use the internal memory of a scanner to find the model
 */
static int p48xxDoTest( pScanData ps )
{
	UChar  tmpByte;
	int	   retval;
	ULong  adder, ul, cntr;
	pULong buffer;

	DBG( DBG_LOW, "p48xxDoTest()\n" );

	buffer = _KALLOC( sizeof(UChar) *  _TEST_SZ, GFP_KERNEL );
	if( NULL == buffer )
		return _E_ALLOC;

	retval = _E_NO_DEV;

	/*
	 * do a memory test to determine how much memory this unit has, in the
     * process we can figure out if it's a 4830 or a 9630.  NOTE: the ram
     * seems to be mirrored such that if you have a unit with only 32k it's
     * mirrored 4 times to fill the 128k (2k * (_MemBankMask + 1)) space,
     * so we will run a 32 bit incrementing pattern over the entire 128k and
     * look for the 1st page (2k) to fail
  	 */
 	adder = 0;
    for (cntr = _BankAndSizeForTest;
         cntr < _BankAndSizeForTest + _MemBanks; cntr++) {

    	ps->OpenScanPath( ps );

		p48xxSetMemoryBankForProgram( ps, cntr );

		/* prepare content, incrementing 32 val */
   		for (ul = 0; ul < _TEST_SZ / sizeof(ULong); ul++)
       		buffer[ul] = ul + adder + _START_VAL;

		/* fill to buffer */
   		IOMoveDataToScanner( ps, (pUChar)buffer, _TEST_SZ );

   		/*
 		 * now check bank 0 to see if it got overwritten
		 * bank 0, size 2k
		 */
		p48xxSetMemoryBankForProgram( ps, _BankAndSizeForTest );

    	ps->CloseScanPath( ps );

		/* read data back */
   		IOReadScannerImageData( ps, (pUChar)buffer, _TEST_SZ );

		/* check */
   		for (ul = 0; ul < _TEST_SZ / sizeof(ULong); ul++) {
      		if (buffer[ul] != ul + _START_VAL) {
       			break;
			}
		}

		/* if fail 	*/
   		if (ul != _TEST_SZ / sizeof (ULong)) {
			DBG( DBG_LOW, "Bank 0 overwritten\n" );
       		break;
		}

   		/* now check current bank */
    	ps->OpenScanPath( ps );
		p48xxSetMemoryBankForProgram( ps, cntr );
    	ps->CloseScanPath( ps );

		/* read data back */
   		IOReadScannerImageData( ps, (pUChar)buffer, _TEST_SZ);

		/* check if fail */
   		for( ul = 0; ul < _TEST_SZ / sizeof(ULong); ul++ ) {
       		if( buffer[ul] != ul + adder + _START_VAL )
       			break;
		}

		/* check if fail */
   		if (ul != _TEST_SZ / sizeof(ULong)) {
			DBG( DBG_LOW, "Bank not present, error at pos %u (%u)\n", ul,
				 (ULong)(_TEST_SZ / sizeof(ULong)));
       		break;
		}

   		adder += _TEST_SZ / sizeof(ULong);
   	}

	_KFREE( buffer );

	DBG( DBG_LOW, "found %d bytes of memory\n",
					_TEST_SZ * (cntr - _BankAndSizeForTest));

	if( cntr == _BankAndSizeForTest ) {
		DBG( DBG_LOW, "No memory ! No scanner...\n" );
		return retval;
	}

#ifdef DEBUG
	tmpByte = IODataRegisterFromScanner( ps, 0x18 );
	DBG( DBG_LOW, "tmpByte[0x18] = 0x%02x\n",tmpByte );
#endif

	tmpByte = IODataRegisterFromScanner( ps, 0x0e );
	DBG( DBG_LOW, "tmpByte = 0x%02x, cntr = %u, AsicId = 0x%02x\n",
				   tmpByte, cntr, ps->sCaps.AsicID );

	/* 128k */
    if ((_TEST_SZ * (cntr - _BankAndSizeForTest) == 1 << 17) &&
								       (ps->sCaps.AsicID ==  _ASIC_IS_96003)) {

        /*
		 * if 128k then must be a 9630 or above
		 * hack, test for 12000P, The 9630 returns an 0x08
		 */
       	if ( tmpByte == 0x02 ) {

            /*
             * as we currently can't automagically detect an A3I we have to
             * use the override switch
             */
            if( _OVR_PLUSTEK_A3I == ps->ModelOverride ) {

        		DBG( DBG_LOW, "Model Override --> A3I\n" );
    			ModelSetA3I( ps );
            } else {
            	ModelSet12000( ps );
	    		DBG( DBG_LOW, "It seems we have a 12000P/96000P\n" );
            }

		}  	else {
        	ModelSet9630( ps );
			DBG( DBG_LOW, "It seems we have a 9630\n" );
		}

		retval = _OK;

	} else {

		DBG( DBG_LOW, "Scanner is not a 9630 or above\n");

      	if ( tmpByte != 0x0f  ) {

			DBG( DBG_LOW, "Looks like a 600!\n" );

			if (( 0x08 == tmpByte ) &&
				((_TEST_SZ * (cntr - _BankAndSizeForTest)) == 32768 )) {
	       		DBG( DBG_LOW, "But it is a 4830P!!! "
							  "(by mkochano@ee.pw.edu.pl)\n" );
				ModelSet4830( ps );
          	} else {
          		ModelSet600( ps );
			}
		}
#ifdef DEBUG
		else
			DBG( DBG_LOW, "It seems we have a 4830\n" );
#endif

		retval = _OK;
	}

	return retval;
}

/*.............................................................................
 * setup ASIC registers and clear all scan states (no stepping)
 */
static void p48xxSetAsicRegisters( pScanData ps )
{
	memset( &ps->AsicReg,   0, sizeof(ps->AsicReg));
	memset( &ps->Asic96Reg, 0, sizeof(ps->Asic96Reg));
    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

   	ps->AsicReg.RD_LineControl       = ps->TimePerLine;
   	ps->AsicReg.RD_ScanControl       = _SCAN_LAMP_ON;
   	ps->AsicReg.RD_ModelControl      = ps->Device.ModelCtrl | _ModelWhiteIs0;
   	ps->AsicReg.RD_Origin 	   	     = 0;
   	ps->AsicReg.RD_Pixels 	   	     = 5110; /*ps->RdPix;*/
   	ps->Asic96Reg.RD_MotorControl    = 0;
   	ps->Asic96Reg.RD_WatchDogControl = 0; /* org. val = 0x8f; */

	IOPutOnAllRegisters( ps );
}

/*.............................................................................
 * use the internal memory of a scanner to find the model
 */
static int p48xxCheck4800Memory( pScanData ps )
{
	int	   retval;
	ULong  ul;
	pUChar buffer;

	DBG( DBG_LOW, "p48xxCheck4800Memory()\n" );

	buffer = _KALLOC( 2560, GFP_KERNEL ); /* 1280: Read,1280:Write */
	if( NULL == buffer )
		return _E_ALLOC;

	retval = _OK;

	/* bank 0, size 2k */
	ps->OpenScanPath( ps );
	p48xxSetMemoryBankForProgram( ps, _BankAndSizeForTest );

	for (ul = 0; ul < 1280; ul++)
	    buffer[ul] = (UChar)ul;			      /* prepare content */

	IOMoveDataToScanner( ps, buffer, 1280 );  /* fill to buffer  */
	p48xxSetMemoryBankForProgram( ps, _BankAndSizeForTest );
	ps->CloseScanPath( ps );

	/* read data back */
	IOReadScannerImageData( ps, buffer + 1280, 1280 );

	for( ul = 0; ul < 1280; ul++ ) {
		if( buffer[ul] != buffer[ul+1280] ) {
			DBG( DBG_HIGH, "Error in memory test at pos %u (%u != %u)\n",
							 ul, buffer[ul], buffer[ul+1280] );
			retval = _E_NO_DEV;
			break;
		}
	}

	_KFREE(buffer);

    return retval;
}

/*.............................................................................
 * call all other modules, to initialize themselves
 */
static int p48xxInitAllModules( pScanData ps )
{
	int result;

	result = DacInitialize( ps );
	if( _OK != result )
		return result;

	result = ImageInitialize( ps );
	if( _OK != result )
		return result;

	result = IOFuncInitialize( ps );
	if( _OK != result )
		return result;

	result = IOInitialize( ps );
	if( _OK != result )
		return result;

	result = MotorInitialize( ps );
	if( _OK != result )
		return result;

	/*
	 * in debug version, check all function pointers
	 */
#ifdef DEBUG
	if(	_FALSE == MiscAllPointersSet( ps ))
		return _E_INTERNAL;
#endif
	return _OK;
}

/*.............................................................................
 *
 */
static int p48xxReadWriteTest( pScanData ps )
{
	int retval;

	DBG( DBG_LOW, "p48xxReadWriteTest()\n" );

	/*
     * determine the model by the ASIC type (except for 4830/9630)
     * might want to make a SetModelCommon() someday for this...
   	 */
	ps->RedDataReady   = 0x01;  /* normal for Red and Green */
  	ps->GreenDataReady = 0x02;
	ps->AsicRedColor   = 0x01;
	ps->AsicGreenColor = 0x03;

	/*
 	 * if not already set, try to find ASIC type (96001 or 96003)
	 */
  	if ( _NO_BASE == ps->sCaps.wIOBase ) {

		/* get copy of asic id */
	    ps->sCaps.AsicID = IODataRegisterFromScanner( ps, ps->RegAsicID );

    	if ( _ASIC_IS_96003 == ps->sCaps.AsicID ) {

			/* actually either a 4830, 9630, 12000, find out later */
       		DBG( DBG_LOW, "Found a 96003 ASIC at Reg 0x%x\n", ps->RegAsicID );
      		ModelSet4830( ps );

    	} else {

      		if ( _ASIC_IS_96001 == ps->sCaps.AsicID ) {
	       		DBG( DBG_LOW, "Found a 96001 ASIC at Reg 0x%x\n",
															   ps->RegAsicID );
        		ModelSet4800( ps );
      		} else {
        		DBG( DBG_LOW, "Can't find your model, asic = 0x%x\n",
														    ps->sCaps.AsicID );
        		return _E_NO_ASIC;
        	}
		}
    }

	/*
	 * set the registers according to the assumptions above
	 */
	p48xxSetAsicRegisters( ps );

   	if ( _ASIC_IS_96003 == ps->sCaps.AsicID ) {
		retval = p48xxDoTest( ps );

		/*
		 * as we may now have detected another model, we have to set
		 * the registers to their new values...
		 * and maybe the modules have to be reset as well
		 */
		if( _OK == retval ) {
			p48xxSetAsicRegisters( ps );
			retval = p48xxInitAllModules( ps );
		}

		return retval;
	}

	/*
	 * this part will be reached only for the 4800 - ASIC 96001
     * we check only the memory as the original driver does
	 */
	return p48xxCheck4800Memory( ps );
}

/*.............................................................................
 * 1) Setup the registers of asic.
 * 2) Determine which type of CCD we are using
 * 3) According to the CCD, prepare the CCD dependent veriables
 *  SONY CCD:
 *	The color exposure sequence: Red, Green (after 11 red lines),
 *	Blue (after 8 green lines)
 *  TOSHIBA CCD:
 *  The color exposure sequence: Red, Blue (after 11 red lines),
 *	Green (after 8 blue lines)
 */
static void p48xxSetupScannerVariables( pScanData ps )
{
	UChar    tmp;
	TimerDef timer;

	DBG( DBG_LOW, "p48xxSetupScannerVariables()\n" );

    ps->OpenScanPath( ps );

	IODataToRegister( ps, ps->RegModelControl2, _Model2ChannelMult );

    if( 2 == IODataFromRegister( ps, ps->RegWriteIOBusDecode1 )) {

		DBG( DBG_LOW, "Scanner has 97003 ASIC too.\n" );
		ps->f97003 = _TRUE;
		ps->b97003DarkR = 8;
		ps->b97003DarkG = 8;
		ps->b97003DarkB = 8;

	    ps->Asic96Reg.u26.RD_ModelControl2 = _Model2ChannelMult;
	} else {

		DBG( DBG_LOW, "No ASIC 97003 found.\n" );
		ps->f97003 = _FALSE;
	    ps->Asic96Reg.u26.RD_ModelControl2 = _Model2DirectOutPort;
	}

	IODataToRegister( ps, ps->RegModelControl2,
				  	  ps->Asic96Reg.u26.RD_ModelControl2 );

	tmp = IODataFromRegister( ps, ps->RegStatus );
	DBG( DBG_LOW, "Status-Register = 0x%02X\n", tmp );
#ifdef DEBUG
	if( tmp & _FLAG_P96_MOTORTYPE ) {
		DBG( DBG_LOW, "Scanner has Full/Half Stepping drive\n" );
	} else {
		DBG( DBG_LOW, "Scanner has Micro Stepping drive\n" );
	}
#endif

    if( tmp & _FLAG_P96_CCDTYPE) {
		ps->fSonyCCD = _FALSE;
		DBG( DBG_LOW, "CCD is NEC/TOSHIBA Type\n" );
	} else {
		ps->fSonyCCD = _TRUE;
		DBG( DBG_LOW, "CCD is SONY Type\n" );
	}

    ps->CloseScanPath( ps );

    ps->b1stColorByte = ps->AsicRedColor;
    ps->b1stColor 	  = ps->RedDataReady;

	if (ps->fSonyCCD) {

		ps->b2ndColorByte = ps->AsicGreenColor;
		ps->b2ndColor 	  = ps->GreenDataReady;
		ps->b3rdColorByte = _ASIC_BLUECOLOR;
		ps->b3rdColor 	  = _BLUE_DATA_READY;

	}  else  { /* NEC/Toshiba CCD */

		ps->b2ndColorByte = _ASIC_BLUECOLOR;
		ps->b2ndColor 	  = _BLUE_DATA_READY;
		ps->b3rdColorByte = ps->AsicGreenColor;
		ps->b3rdColor 	  = ps->GreenDataReady;
	}

    ps->b1stMask = (Byte)~ps->b1stColor;
    ps->b2ndMask = (Byte)~ps->b2ndColor;
	ps->b3rdMask = (Byte)~ps->b3rdColor;

    ps->b1stLinesOffset = 17;
    ps->b2ndLinesOffset = 9;

    /*
	 * calculate I/O Timer
     * if we cannot read 200 lines within 1 second, the I/O time has to add 2
     * CalculateIOTime ()
	 */
    if( _PORT_SPP != ps->IO.portMode ) {

		UShort wLines = 200;
		pUChar pBuf;

		pBuf = _KALLOC((_BUF_SIZE_BASE_CONST * 2), GFP_KERNEL );

		if ( NULL != pBuf ) {

			MiscStartTimer( &timer, _SECOND );

		    do {
				IOReadScannerImageData( ps, pBuf, (_BUF_SIZE_BASE_CONST * 2));

				wLines--;
		    } while (!MiscCheckTimer( &timer) && wLines);

		    if( !wLines )
				ps->bExtraAdd = 0;
		    else
				ps->bExtraAdd = 2;

			_KFREE( pBuf );

		} else {
	    	ps->bExtraAdd = 2;	/* poor resource */
		}
	} else {
		ps->bExtraAdd = 0;
	}
}

/*.............................................................................
 *
 */
static void p48xxSetGeneralRegister( pScanData ps )
{
	if( MODEL_OP_A3I == ps->sCaps.Model ) {
		ps->AsicReg.RD_ModelControl = _ModelDpi400 | _ModelWhiteIs0 |
									  _ModelMemSize128k4;
    }

    ps->AsicReg.RD_ModeControl = _ModeScan;

/* WORK: ps->PhysicalDpi should be correct, but the we have to work
 *       on motor.c again to use other running-tables
 *    if ( ps->DataInf.xyAppDpi.y <= ps->PhysicalDpi ) {
 */
	if ( ps->DataInf.xyAppDpi.y <= 300 ) {
/* HEINER:A3I
 	if ( ps->DataInf.xyAppDpi.y <= ps->PhysicalDpi ) {
*/
		ps->Asic96Reg.RD_MotorControl = (ps->FullStep | ps->IgnorePF |
									     ps->MotorOn | _MotorDirForward);
	} else {
		ps->Asic96Reg.RD_MotorControl = (ps->IgnorePF | ps->MotorOn |
										 _MotorDirForward);
	}

    if ( ps->DataInf.wPhyDataType == COLOR_BW ) {
        ps->AsicReg.RD_ScanControl = ps->bLampOn;

	    if (!(ps->DataInf.dwScanFlag & SCANDEF_Inverse))
    	    ps->AsicReg.RD_ScanControl |= _P96_SCANDATA_INVERT;

    } else {

        ps->AsicReg.RD_ScanControl = ps->bLampOn | _SCAN_BYTEMODE;

	    if (ps->DataInf.dwScanFlag & SCANDEF_Inverse)
    	    ps->AsicReg.RD_ScanControl |=  _P96_SCANDATA_INVERT;
    }

    if (ps->DataInf.xyPhyDpi.x <= 200)
        ps->AsicReg.RD_ScanControl |= _SCAN_1ST_AVERAGE;

	DBG( DBG_LOW, "RD_ModeControl  = 0x%02x\n", ps->AsicReg.RD_ModeControl  );
	DBG( DBG_LOW, "RD_MotorControl = 0x%02x\n", ps->Asic96Reg.RD_MotorControl );
	DBG( DBG_LOW, "RD_ScanControl  = 0x%02x\n", ps->AsicReg.RD_ScanControl  );
}

/*.............................................................................
 *
 */
static void p48xxSetupScanningCondition( pScanData ps )
{
	DBG( DBG_LOW, "p48xxSetupScanningCondition()\n" );

	IORegisterDirectToScanner( ps, ps->RegInitDataFifo );

   /* Cal64kTime (); */
	if( MODEL_OP_A3I == ps->sCaps.Model )
	    ps->wLinesPer64kTime = (UShort)(65555UL / ps->DataInf.dwAsicBytesPerPlane *
				 			        5UL);
    else
	    ps->wLinesPer64kTime = (UShort)(65555UL / ps->DataInf.dwAsicBytesPerPlane *
				 			        10UL / 3UL);

	DBG( DBG_LOW, "wLinesPer64kTime = %u\n", ps->wLinesPer64kTime );

    ps->InitialSetCurrentSpeed( ps );

	DBG( DBG_LOW, "Current Speed = %u\n", ps->bCurrentSpeed );

    ps->bMinReadFifo = (Byte)((ps->DataInf.dwAsicBytesPerPlane + 511) / 512);
	DBG( DBG_LOW, "MinReadFifo = %u\n", ps->bMinReadFifo );

    p48xxSetGeneralRegister( ps );

	/*
	 * if speed is not the fastest and DPI is less than 400, do half steps
	 */
    if( ps->DataInf.wPhyDataType >= COLOR_256GRAY &&
		!(ps->bCurrentSpeed & 1) && (ps->DataInf.xyAppDpi.y <= 300)) {
/* HEINER:A3I
	if( !(ps->bCurrentSpeed & 1) && (ps->DataInf.xyAppDpi.y <= ps->PhysicalDpi)) {
*/
		ps->fHalfStepTableFlag = _TRUE;
		ps->Asic96Reg.RD_MotorControl &= ps->StepMask;
    }

    ps->AsicReg.RD_Dpi = ps->DataInf.xyPhyDpi.x;
	DBG( DBG_LOW, "RD_Dpi = %u\n", ps->AsicReg.RD_Dpi );

    /* SetStartStopRegister (ps) */
    ps->AsicReg.RD_Origin = (UShort)(ps->Offset70 + ps->Device.DataOriginX +
					                 ps->DataInf.crImage.x);

    if (ps->DataInf.wPhyDataType < COLOR_256GRAY) {
		ps->AsicReg.RD_Pixels =
					(UShort)(ps->DataInf.dwAsicPixelsPerPlane + 7) & 0xfff8;
	} else {
		ps->AsicReg.RD_Pixels = (UShort)ps->DataInf.dwAsicPixelsPerPlane;
	}

	DBG( DBG_LOW, "RD_Pixels = %u\n", ps->AsicReg.RD_Pixels );

    /* SetupMotorStart () */
	IORegisterDirectToScanner( ps, ps->RegInitDataFifo);
    ps->SetupMotorRunTable( ps );

    IOSetToMotorRegister( ps );

    ps->pCurrentColorRunTable = ps->pColorRunTable;
    ps->bCurrentLineCount = 0;

    IOPutOnAllRegisters( ps );

    ps->OpenScanPath( ps );

	/*
	 * when using the full-step speed on 600 dpi models, then set
	 * the motor into half-step mode, to avoid that the scanner hits
	 * the back of its cover
	 */
   	if((600 == ps->PhysicalDpi) && (1 == ps->bCurrentSpeed)) {

		ps->Asic96Reg.RD_MotorControl &= ~ps->FullStep;
	}

 	IODataToRegister( ps, ps->RegMotorControl,
					    (Byte)(ps->Asic96Reg.RD_MotorControl & ~ps->MotorOn));
	IODataToRegister( ps, ps->RegMotorControl, ps->Asic96Reg.RD_MotorControl);
    IORegisterToScanner( ps, ps->RegInitDataFifo );

    ps->CloseScanPath( ps );
}

/*.............................................................................
 * switch the motor off and put the scanner into idle mode
 */
static void p48xxPutToIdleMode( pScanData ps )
{
    DBG( DBG_LOW, "Putting Scanner (ASIC 96001/3) into Idle-Mode\n" );

    /*
     * turn off motor
     */
	ps->Asic96Reg.RD_MotorControl = 0;
	IOCmdRegisterToScanner( ps, ps->RegMotorControl, 0 );
}

/*.............................................................................
 * for P96001/3 ASIC
 * do all the preliminary stuff here (calibrate the scanner and move the
 * sensor to it´s start position, also setup the driver for the
 * current run)
 */
static int p48xxCalibration( pScanData ps )
{
	DBG( DBG_LOW, "p48xxCalibration()\n" );

    ps->Scan.bFifoSelect = ps->RegGFifoOffset;

	while (_TRUE) {

		_ASSERT(ps->WaitForShading);
        if (ps->WaitForShading( ps )) {

        	if(!(ps->DataInf.dwScanFlag & SCANDEF_TPA)) {

/* HEINER:A3I disable !! */
	               	MotorP96AheadToDarkArea( ps );

                    if( ps->Scan.fRefreshState ) {
                        ps->Scan.fRefreshState = _FALSE;

          			if (!ps->fReshaded) {
                        ps->fReshaded = _TRUE;

    	    		    if (ps->fColorMoreRedFlag || ps->fColorMoreBlueFlag) {
           					continue;
						}
    	            }
	    	  	}
	   		}
			break;

	    } else {
        	ps->fScanningStatus = _FALSE;
        	ps->DataInf.dwAppLinesPerArea = 0;
        	return _E_TIMEOUT;
	    }
	}

   	if((ps->sCaps.AsicID != _ASIC_IS_96001) &&
							       (ps->DataInf.wPhyDataType != COLOR_BW)) {
		DacP96WriteBackToGammaShadingRAM(ps);
	}

	if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {
       	ps->bExtraMotorCtrl     = 0;
    	ps->Scan.fMotorBackward = _TRUE;
       	MotorP96ConstantMoveProc( ps, 4000 );
	}

	/*
	 * move sensor and setup scanner for grabbing the picture
	 */
	_ASSERT(ps->WaitForPositionY);
   	ps->WaitForPositionY(ps);
	return _OK;
}

/************************ exported functions *********************************/

/*.............................................................................
 * initialize the register values and function calls for the 96001/3 asic
 */
_LOC int P48xxInitAsic( pScanData ps )
{
	DBG( DBG_LOW, "P48xxInitAsic()\n" );

	ps->IO.bOpenCount = 0;

	ps->RegSwitchBus			= 0;
	ps->RegReadDataMode			= 1;
	ps->RegWriteDataMode		= 2;
	ps->RegEPPEnable			= 3;
	ps->RegInitDataFifo			= 4;
	ps->RegForceStep			= 5;
	ps->RegInitScanState		= 6;
	ps->RegRefreshScanState		= 7;
	ps->RegStatus				= 0x10;
	ps->RegFifoOffset			= 0x11;
	ps->RegGetScanState			= 0x12;
	ps->RegAsicID				= 0x13;	/* Determine the asic */
	ps->RegReadIOBufBus			= 0x17;
	ps->RegModeControl			= 0x18;
	ps->RegLineControl			= 0x19;
	ps->RegScanControl			= 0x1a;
	ps->RegMotorControl			= 0x1b;
	ps->RegModelControl			= 0x1c;
	ps->RegMemAccessControl		= 0x1d;
	ps->RegDpiLow				= 0x1e;
	ps->RegDpiHigh				= 0x1f;
	ps->RegScanPosLow			= 0x20;
	ps->RegScanPosHigh			= 0x21;
	ps->RegWidthPixelsLow		= 0x22;
	ps->RegWidthPixelsHigh		= 0x23;
	ps->RegThresholdControl		= 0x24;
	ps->RegWatchDogControl		= 0x25;
	ps->RegModelControl2		= 0x26;
	ps->RegThresholdGapControl	= 0x27;
	ps->RegRedChShadingOffset   = 0x28;
	ps->RegGreenChShadingOffset = 0x29;
	ps->RegRedDCAdjust			= 0x27;  /* not sure why these are dup's */
	ps->RegGreenDCAdjust		= 0x28;
	ps->RegBlueDCAdjust			= 0x29;
	ps->RegBlueChShadingOffset	= 0x2a;
	ps->RegRedChDarkOffset		= 0x2b;
	ps->RegGreenChDarkOffset	= 0x2c;
	ps->RegBlueChDarkOffset		= 0x2d;
	ps->RegWriteIOBusDecode1	= 0x2e;
	ps->RegWriteIOBusDecode2	= 0x2f;
	ps->RegScanStateControl		= 0x30;
	ps->RegRedChEvenOffset		= 0x31;
	ps->RegGreenChEvenOffset	= 0x32;
	ps->RegBlueChEvenOffset		= 0x33;
	ps->RegRedChOddOffset		= 0x34;
	ps->RegGreenChOddOffset		= 0x35;
	ps->RegBlueChOddOffset		= 0x36;
	ps->RegRedGainOutDirect		= 0x37;
	ps->RegGreenGainOutDirect	= 0x38;
	ps->RegBlueGainOutDirect	= 0x39;
	ps->RegLedControl			= 0x3a;
	ps->RegShadingCorrectCtrl	= 0x3b;
	ps->RegScanStateBegin		= 0x40;	/* (0, 1)		*/
	ps->RegScanStateEnd			= 0x5f;	/* (62, 63)     */

	/*
	 * setup function calls
	 */
	ps->ReadWriteTest		   = p48xxReadWriteTest;
	ps->SetupScannerVariables  = p48xxSetupScannerVariables;
	ps->SetupScanningCondition = p48xxSetupScanningCondition;
    ps->PutToIdleMode          = p48xxPutToIdleMode;
    ps->Calibration            = p48xxCalibration;

	/*
	 * setup misc
	 */
	ps->CtrlReadHighNibble = _CTRL_GENSIGNAL + _CTRL_AUTOLF;
	ps->CtrlReadLowNibble  = _CTRL_GENSIGNAL + _CTRL_AUTOLF + _CTRL_STROBE;

	ps->MotorFreeRun = 0x80;
	ps->bLampOn      = _SCAN_LAMP_ON;
	ps->f97003 	     = _FALSE;

	/*
	 * initialize the other modules
	 */
	return p48xxInitAllModules( ps );
}

/* END PLUSTEK-PP_P48xx.C ...................................................*/
