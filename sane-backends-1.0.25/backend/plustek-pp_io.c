/* @file plustekpp-io.c
 * @brief as the name says, here we have all the I/O
 *        functions according to the parallel port hardware
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.37 - initial version
 *        - added Kevins' suggestions
 * - 0.38 - added Asic 98003 stuff and ioP98ReadWriteTest()
 *        - added IODataRegisterToDAC()
 *        - replaced function IOSPPWrite by IOMoveDataToScanner
 *        - modified ioP98OpenScanPath again and reuse V0.36 stuff again
 *        - added IO functions
 * - 0.39 - added IO functions
 *        - added f97003 stuff from A3I code
 * - 0.40 - no changes
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

/*************************** some prototypes *********************************/

static Bool fnEPPRead  ( pScanData ps, pUChar pBuffer, ULong ulSize );
static Bool fnSPPRead  ( pScanData ps, pUChar pBuffer, ULong ulSize );
static Bool fnBiDirRead( pScanData ps, pUChar pBuffer, ULong ulSize );

typedef struct {
	pFnReadData func;
	char       *name;
} ioReadFuncDef;

static ioReadFuncDef ioReadFunc[3] = {
	{ fnEPPRead,   "fnEPPRead"   },
	{ fnSPPRead,   "fnSPPRead"   },
	{ fnBiDirRead, "fnBiDirRead" }
};

/*************************** some definitions ********************************/

#define _MEMTEST_SIZE   1280

/*************************** local functions *********************************/

/** we provide some functions to read data from SPP port according to
 * the speed we have detected (ReadWriteTest!!)
 */
static Byte ioDataFromSPPFast( pScanData ps )
{
	Byte bData, tmp;

	/* notify asic we will read the high nibble data from status port */
	if( _FALSE == ps->f97003 ) {
		_OUTB_CTRL( ps, ps->CtrlReadHighNibble );
		_DO_UDELAY( 1 );
	}

	/* read high nibble */
    bData  = _INB_STATUS( ps );
	bData &= 0xf0;

    _OUTB_CTRL( ps, ps->CtrlReadLowNibble );
	_DO_UDELAY( 1 );

	/* read low nibble */
	tmp = _INB_STATUS( ps );
	
	/* combine with low nibble */
    bData |= (tmp >> 4);

    _OUTB_CTRL( ps, _CTRL_GENSIGNAL );
	_DO_UDELAY( 1 );

    return bData;
}

static Byte ioDataFromSPPMiddle( pScanData ps )
{
	Byte bData, tmp;

	/* notify asic we will read the high nibble data from status port */
	if( _FALSE == ps->f97003 ) {
		_OUTB_CTRL( ps, ps->CtrlReadHighNibble );
		_DO_UDELAY( 1 );
	}

	/* read high nibble */
	_INB_STATUS( ps );
    bData  = _INB_STATUS( ps );
	bData &= 0xf0;

    _OUTB_CTRL( ps, ps->CtrlReadLowNibble );
	_DO_UDELAY( 1 );

	/* read low nibble */
	_INB_STATUS( ps );
	tmp = _INB_STATUS( ps );
	
	/* combine with low nibble */
    bData |= (tmp >> 4);

    _OUTB_CTRL( ps, _CTRL_GENSIGNAL );
	_DO_UDELAY( 1 );

    return bData;
}

static UChar ioDataFromSPPSlow( pScanData ps )
{
	Byte bData, tmp;

	/* notify asic we will read the high nibble data from status port */
	if( _FALSE == ps->f97003 ) {
		_OUTB_CTRL( ps, ps->CtrlReadHighNibble );
		_DO_UDELAY( 2 );
	}

	/* read high nibble */
	_INB_STATUS( ps );
	_INB_STATUS( ps );
    bData  = _INB_STATUS( ps );
	bData &= 0xf0;

    _OUTB_CTRL( ps, ps->CtrlReadLowNibble );
	_DO_UDELAY( 2 );

	/* read low nibble */
	_INB_STATUS( ps );
	_INB_STATUS( ps );
	tmp = _INB_STATUS( ps );
	
	/* combine with low nibble */
    bData |= (tmp >> 4);

    _OUTB_CTRL( ps, _CTRL_GENSIGNAL );
	_DO_UDELAY( 2 );

    return bData;
}

static UChar ioDataFromSPPSlowest( pScanData ps )
{
	Byte bData, tmp;

	/* notify asic we will read the high nibble data from status port */
	if( _FALSE == ps->f97003 ) {
		_OUTB_CTRL( ps, ps->CtrlReadHighNibble );
		_DO_UDELAY( 3 );
	}

	/* read high nibble */
	_INB_STATUS( ps );
	_INB_STATUS( ps );
	_INB_STATUS( ps );
    bData  = _INB_STATUS( ps );
	bData &= 0xf0;

    _OUTB_CTRL( ps, ps->CtrlReadLowNibble );
	_DO_UDELAY( 3 );

	/* read low nibble */
	_INB_STATUS( ps );
	_INB_STATUS( ps );
	_INB_STATUS( ps );
	tmp = _INB_STATUS( ps );
	
	/* combine with low nibble */
    bData |= (tmp >> 4);

    _OUTB_CTRL( ps, _CTRL_GENSIGNAL );
	_DO_UDELAY( 3 );

    return bData;
}

/** Read data from STATUS port. We have to read twice and combine two nibble
 *  data to one byte.
 */
static Bool fnSPPRead( pScanData ps, pUChar pBuffer, ULong ulSize )
{
	switch( ps->IO.delay ) {

		case 0:
			for (; ulSize; ulSize--, pBuffer++)
				*pBuffer = ioDataFromSPPFast( ps );
			break;

		case 1:
			for (; ulSize; ulSize--, pBuffer++)
				*pBuffer = ioDataFromSPPMiddle( ps );
			break;

		case 2:
			for (; ulSize; ulSize--, pBuffer++)
				*pBuffer = ioDataFromSPPSlow( ps );
			break;

		default:
			for (; ulSize; ulSize--, pBuffer++)
				*pBuffer = ioDataFromSPPSlowest( ps );
			break;
	}

    return _TRUE;
}


/** Using buffered I/O to read data from EPP Data Port
 */
static Bool fnEPPRead( pScanData ps, pUChar pBuffer, ULong ulSize )
{
	register ULong i;

	if( _IS_ASIC98(ps->sCaps.AsicID)) {

#ifndef __KERNEL__
		sanei_pp_set_datadir( ps->pardev, SANEI_PP_DATAIN );
#else
		_OUTB_CTRL( ps, (_CTRL_GENSIGNAL + _CTRL_DIRECTION));
		_DO_UDELAY( 1 );
#endif
		for( i = 0; i < ulSize; i++ )
			pBuffer[i] = _INB_EPPDATA( ps );

#ifndef __KERNEL__
		sanei_pp_set_datadir( ps->pardev, SANEI_PP_DATAOUT );
#else
		_OUTB_CTRL( ps, _CTRL_GENSIGNAL );
		_DO_UDELAY( 1 );
#endif
	} else {

		for( i = 0; i < ulSize; i++ )
			pBuffer[i] = _INB_EPPDATA( ps );
	}

	return _TRUE;
}

/**
 */
static Bool fnBiDirRead( pScanData ps, pUChar pBuffer, ULong ulSize )
{
	UChar start, end;

	start = _CTRL_START_BIDIREAD;
	end   = _CTRL_END_BIDIREAD;

#ifndef __KERNEL__
	sanei_pp_set_datadir( ps->pardev, SANEI_PP_DATAIN );

	if( !sanei_pp_uses_directio()) {
		start &= ~_CTRL_DIRECTION;
		end   &= ~_CTRL_DIRECTION;
	}
#else
	if( _IS_ASIC98(ps->sCaps.AsicID)) {
		_OUTB_CTRL( ps, (_CTRL_GENSIGNAL + _CTRL_DIRECTION));
	}
#endif

	switch( ps->IO.delay ) {

		case 0:
		    for( ; ulSize; ulSize--, pBuffer++ ) {
				_OUTB_CTRL( ps, start );   
				*pBuffer = _INB_DATA( ps );
				_OUTB_CTRL( ps, end );   
			}
			break;

		case 1:
			_DO_UDELAY( 1 );
		    for(; ulSize; ulSize--, pBuffer++ ) {
				_OUTB_CTRL( ps, start );
				_DO_UDELAY( 1 );

				*pBuffer = _INB_DATA( ps );

				_OUTB_CTRL( ps, end );
				_DO_UDELAY( 1 );
			}
			break;
		
		default:
			_DO_UDELAY( 2 );
		    for(; ulSize; ulSize--, pBuffer++ ) {
				_OUTB_CTRL( ps, start );
				_DO_UDELAY( 2 );

				*pBuffer = _INB_DATA( ps );

				_OUTB_CTRL( ps, end );
				_DO_UDELAY( 2 );
			}
			break;

	}

#ifndef __KERNEL__
	sanei_pp_set_datadir( ps->pardev, SANEI_PP_DATAOUT );
#else
	if( _IS_ASIC98(ps->sCaps.AsicID)) {
		_OUTB_CTRL( ps, _CTRL_GENSIGNAL );
	}
#endif
	return _TRUE;
}

/** as the name says, we switch to SPP mode
 */
static void ioSwitchToSPPMode( pScanData ps )
{
	/* save the control and data port value
	 */
	ps->IO.bOldControlValue = _INB_CTRL( ps );
	ps->IO.bOldDataValue    = _INB_DATA( ps );

	_OUTB_CTRL( ps, _CTRL_GENSIGNAL );	/* 0xc4 */
	_DO_UDELAY( 2 );
}

/** restore the port settings
 */
static void ioRestoreParallelMode( pScanData ps )
{
	_OUTB_CTRL( ps, ps->IO.bOldControlValue & 0x3f );
	_DO_UDELAY( 1 );

	_OUTB_DATA( ps, ps->IO.bOldDataValue );
	_DO_UDELAY( 1 );
}

/** try to connect to scanner (ASIC 9600x and 98001)
 */
_LOC void ioP98001EstablishScannerConnection( pScanData ps, ULong delTime )
{
	_OUTB_DATA( ps, _ID_TO_PRINTER );
    _DO_UDELAY( delTime );

	_OUTB_DATA( ps, _ID1ST );
    _DO_UDELAY( delTime );

	_OUTB_DATA( ps, _ID2ND );
    _DO_UDELAY( delTime );

	_OUTB_DATA( ps, _ID3RD );
    _DO_UDELAY( delTime );

    _OUTB_DATA( ps, _ID4TH );
    _DO_UDELAY( delTime );
}

/** try to connect to scanner (ASIC 98003)
 */
static void ioP98003EstablishScannerConnection( pScanData ps, ULong delTime )
{
	_OUTB_DATA( ps, _ID1ST );
	_DO_UDELAY( delTime );

	_OUTB_DATA( ps, _ID2ND );
	_DO_UDELAY( delTime );

	_OUTB_DATA( ps, _ID3RD );
	_DO_UDELAY( delTime );

	_OUTB_DATA( ps, _ID4TH );
	_DO_UDELAY( delTime );
}

/** switch the printer interface to scanner
 */
static Bool ioP96OpenScanPath( pScanData ps )
{
	if( 0 == ps->IO.bOpenCount ) {

		/* not established */
		ioSwitchToSPPMode( ps );

		/* Scanner command sequence to open scanner path */
		ioP98001EstablishScannerConnection( ps, 5 );
	}
#ifdef DEBUG
	else
		DBG( DBG_IO, "!!!! Path already open (%u)!!!!\n", ps->IO.bOpenCount );
#endif

	ps->IO.bOpenCount++;			/* increment the opened count */

/*
 * CHECK to we really need that !!
 */
	ps->IO.useEPPCmdMode = _FALSE;
	return _TRUE;
}

/** try to connect to scanner
 */
static Bool ioP98OpenScanPath( pScanData ps )
{
    Byte  tmp;
    ULong dw;
	ULong dwTime = 1;

	if( 0 == ps->IO.bOpenCount ) {

		/* not established */
		ioSwitchToSPPMode( ps );

		for( dw = 10; dw; dw-- ) {

			/*
			 * this seems to be necessary...
			 */
 			if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {
				ioP98001EstablishScannerConnection( ps, dw );
#if 0
				ioP98001EstablishScannerConnection( ps, dw );
				ioP98001EstablishScannerConnection( ps, dw );
#endif
			} else {
				ioP98003EstablishScannerConnection( ps, dw );
			}			

			_INB_STATUS( ps );
			tmp = _INB_STATUS( ps );

			if( 0x50 == ( tmp & 0xf0 )) {

				ps->IO.bOpenCount = 1;

				if( ps->sCaps.AsicID == IODataFromRegister(ps, ps->RegAsicID)) {
					return _TRUE;
				}
				ps->IO.bOpenCount = 0;
			}

			dwTime++;
		}
		DBG( DBG_IO, "ioP98OpenScanPath() failed!\n" );
		return _FALSE;
	}
#ifdef DEBUG
	else
		DBG( DBG_IO, "!!!! Path already open (%u)!!!!\n", ps->IO.bOpenCount );
#endif

	ps->IO.bOpenCount++;			/* increment the opened count */
	return _TRUE;
}

/** Switch back to printer mode.
 * Restore the printer control/data port value.
 */
static void ioCloseScanPath( pScanData ps )
{
	if( ps->IO.bOpenCount && !(--ps->IO.bOpenCount)) {

#ifdef DEBUG
		ps->IO.bOpenCount = 1;
#endif
		IORegisterToScanner( ps, 0xff );

		/*
		 * back to pass-through printer mode
		 */
		IORegisterToScanner( ps, ps->RegSwitchBus );
#ifdef DEBUG
        ps->IO.bOpenCount = 0;
#endif
		ps->IO.useEPPCmdMode = _FALSE;

		ioRestoreParallelMode( ps );
	}
}

/** check the memory to see that the data-transfers will work.
 * (ASIC 9800x only)
 */
static int ioP98ReadWriteTest( pScanData ps )
{	
	UChar  tmp;
	ULong  ul;
	pUChar buffer;
	int	   retval;

	DBG( DBG_LOW, "ioP98ReadWriteTest()\n" );

	/* _MEMTEST_SIZE: Read, _MEMTEST_SIZE:Write */
	buffer = _KALLOC( sizeof(UChar) * _MEMTEST_SIZE*2, GFP_KERNEL );
	if( NULL == buffer )
		return _E_ALLOC;

	/* prepare content */
	for( ul = 0; ul < _MEMTEST_SIZE; ul++ )
	    buffer[ul] = (UChar)ul;	

	ps->OpenScanPath(ps);
	
	/* avoid switching to Lamp0, when previously scanned in transp./neg mode */
	tmp = ps->bLastLampStatus + _SCAN_BYTEMODE;
	IODataToRegister( ps, ps->RegScanControl, tmp );

	IODataToRegister( ps, ps->RegModelControl, (_LED_ACTIVITY | _LED_CONTROL));

	IODataToRegister( ps, ps->RegModeControl, _ModeMappingMem );
	IODataToRegister( ps, ps->RegMemoryLow,  0 );
	IODataToRegister( ps, ps->RegMemoryHigh, 0 );

	/* fill to buffer */
	IOMoveDataToScanner( ps, buffer, _MEMTEST_SIZE );

	IODataToRegister( ps, ps->RegModeControl, _ModeMappingMem );
	IODataToRegister( ps, ps->RegMemoryLow,  0 );
	IODataToRegister( ps, ps->RegMemoryHigh, 0 );
	IODataToRegister( ps, ps->RegWidthPixelsLow,  0 );
	IODataToRegister( ps, ps->RegWidthPixelsHigh, 5 );

	ps->AsicReg.RD_ModeControl = _ModeReadMappingMem;

	if( _ASIC_IS_98001 == ps->sCaps.AsicID )
		ps->CloseScanPath( ps );

	IOReadScannerImageData( ps, buffer + _MEMTEST_SIZE, _MEMTEST_SIZE );

	if( _ASIC_IS_98003 == ps->sCaps.AsicID )
		ps->CloseScanPath( ps );

	/* check the result ! */
	retval = _OK;

	for( ul = 0; ul < _MEMTEST_SIZE; ul++ ) {
		if( buffer[ul] != buffer[ul+_MEMTEST_SIZE] ) {
			DBG( DBG_HIGH, "Error in memory test at pos %u (%u != %u)\n",
				 ul, buffer[ul], buffer[ul+_MEMTEST_SIZE] );
			retval = _E_NO_DEV;
			break;
		}
	}

	_KFREE(buffer);
	return retval;
}

/** Put data to DATA port and trigger hardware through CONTROL port to read it.
 */
static void ioSPPWrite( pScanData ps, pUChar pBuffer, ULong size )
{
	DBG( DBG_IO , "Moving %u bytes to scanner, IODELAY = %u...\n",
					size, ps->IO.delay );
	switch( ps->IO.delay ) {
	
		case 0:
		    for (; size; size--, pBuffer++) {
				_OUTB_DATA( ps, *pBuffer );
				_OUTB_CTRL( ps, _CTRL_START_DATAWRITE );
				_OUTB_CTRL( ps, _CTRL_END_DATAWRITE );
        	}
			break;
		
		case 1:
		case 2:
		    for (; size; size--, pBuffer++) {
				_OUTB_DATA( ps, *pBuffer );
				_DO_UDELAY( 1 );
				_OUTB_CTRL( ps, _CTRL_START_DATAWRITE );
				_DO_UDELAY( 1 );
				_OUTB_CTRL( ps, _CTRL_END_DATAWRITE );
				_DO_UDELAY( 2 );
        	}
			break;

		default:
		    for (; size; size--, pBuffer++) {
				_OUTB_DATA( ps, *pBuffer );
				_DO_UDELAY( 1 );
				_OUTB_CTRL( ps, _CTRL_START_DATAWRITE );
				_DO_UDELAY( 2 );
				_OUTB_CTRL( ps, _CTRL_END_DATAWRITE );
				_DO_UDELAY( 3 );
        	}
			break;
	}
	DBG( DBG_IO , "... done.\n" );
}

/** set the scanner to "read" data mode
 */
static void ioEnterReadMode( pScanData ps )
{
	if( ps->IO.portMode != _PORT_SPP ) {

		_DO_UDELAY( 1 );
		IORegisterToScanner( ps, ps->RegEPPEnable );

		if( _IS_ASIC98( ps->sCaps.AsicID ))
			ps->IO.useEPPCmdMode = _TRUE;
	}

	if( _ASIC_IS_98003 == ps->sCaps.AsicID )
		ps->IO.bOldControlValue = _INB_CTRL( ps );

	/* ask ASIC to enter read mode */
	IORegisterToScanner( ps, ps->RegReadDataMode );	
}

/************************ exported functions *********************************/

/** here we do some init work
 */
_LOC int IOInitialize( pScanData ps )
{
	DBG( DBG_HIGH, "IOInitialize()\n" );

	if( NULL == ps )
		return _E_NULLPTR;

	if( _IS_ASIC98(ps->sCaps.AsicID)) {

		ps->OpenScanPath  = ioP98OpenScanPath;
		ps->ReadWriteTest = ioP98ReadWriteTest;

	} else if( _IS_ASIC96(ps->sCaps.AsicID)) {

		ps->OpenScanPath = ioP96OpenScanPath;

	} else {

		DBG( DBG_HIGH , "NOT SUPPORTED ASIC !!!\n" );
		return _E_NOSUPP;
	}

	ps->CloseScanPath   = ioCloseScanPath;
	ps->Device.ReadData = ioReadFunc[ps->IO.portMode].func;
	DBG( DBG_HIGH, "* using readfunction >%s<\n",
	                  ioReadFunc[ps->IO.portMode].name );
	return _OK;
}

/** Write specific length buffer to scanner
 * The scan path is already established
 */
_LOC void IOMoveDataToScanner( pScanData ps, pUChar pBuffer, ULong size )
{
#ifdef DEBUG
	if( 0 == ps->IO.bOpenCount )
		DBG( DBG_IO, "IOMoveDataToScanner - no connection!\n" );
#endif

	IORegisterToScanner( ps, ps->RegInitDataFifo );
	IORegisterToScanner( ps, ps->RegWriteDataMode );

	ioSPPWrite( ps, pBuffer, size );
}

/** Calling SITUATION: Scanner path is established.
 * download a scanstate-table
 */
_LOC void IODownloadScanStates( pScanData ps )
{
	TimerDef timer;
#ifdef DEBUG
	if( 0 == ps->IO.bOpenCount )
		DBG( DBG_IO, "IODownloadScanStates - no connection!\n" );
#endif

	IORegisterToScanner( ps, ps->RegScanStateControl );

	ioSPPWrite( ps, ps->a_nbNewAdrPointer, _SCANSTATE_BYTES );

	if( ps->Scan.fRefreshState ) {

		IORegisterToScanner( ps, ps->RegRefreshScanState );

		MiscStartTimer( &timer, (_SECOND/2));
		do {

			if (!( IOGetScanState( ps, _TRUE) & _SCANSTATE_STOP))
				break;
		}
		while( !MiscCheckTimer(&timer));
	}
}

/** Calling SITUATION: Scanner path is established.
 * Write a data to asic
 */
_LOC void IODataToScanner( pScanData ps, Byte bValue )
{
	ULong deltime = 4;

#ifdef DEBUG
	if( 0 == ps->IO.bOpenCount )
        DBG( DBG_IO, "IODataToScanner - no connection!\n" );
#endif

	if( ps->IO.delay < 2 )
		deltime = 2;

    /* output data */
   	_OUTB_DATA( ps, bValue );
	_DO_UDELAY( deltime );

	/* notify asic there is data */
    _OUTB_CTRL( ps, _CTRL_START_DATAWRITE );
	_DO_UDELAY( deltime );
						
		/* end write cycle */
   	_OUTB_CTRL( ps, _CTRL_END_DATAWRITE );
	_DO_UDELAY( deltime-1 );
}

/** Calling SITUATION: Scanner path is established.
 * Write a data to specific asic's register
 */
_LOC void IODataToRegister( pScanData ps, Byte bReg, Byte bData )
{
#ifdef DEBUG
    if( 0 == ps->IO.bOpenCount )
        DBG( DBG_IO, "IODataToRegister - no connection!\n" );
#endif

	/* specify register */
    IORegisterToScanner( ps, bReg );	

	/* then write the content */
	IODataToScanner( ps, bData );
}

/** Calling SITUATION: Scanner path is established.
 * Read the content of specific asic's register
 */
_LOC Byte IODataFromRegister( pScanData ps, Byte bReg )
{
	IORegisterToScanner( ps, bReg );

	if( 0 == ps->IO.delay )
		return ioDataFromSPPFast( ps );
	else if( 1 == ps->IO.delay )
		return ioDataFromSPPMiddle( ps );
	else if( 2 == ps->IO.delay )
		return ioDataFromSPPSlow( ps );
	else
		return ioDataFromSPPSlowest( ps );
}

/** Calling SITUATION: Scanner path is established.
 * Write a register to asic (used for a command without parameter)
 */
_LOC void IORegisterToScanner( pScanData ps, Byte bReg )
{
#ifdef DEBUG
    if( 0 == ps->IO.bOpenCount )
        DBG( DBG_IO, "IORegisterToScanner - no connection!\n" );
#endif

    /*
     * write data to port
     */
	_OUTB_DATA( ps, bReg );

    /*
     * depending on the mode, generate the trigger signals
     */
    if( ps->IO.useEPPCmdMode ) {

		_DO_UDELAY( 5 );

		_OUTB_CTRL( ps, _CTRL_EPPSIGNAL_WRITE);	/* 0xc5 */
		_DO_UDELAY( 5 );

		_OUTB_CTRL( ps, _CTRL_EPPTRIG_REGWRITE);/* 0xcd */
		_DO_UDELAY( 5 );

		_OUTB_CTRL( ps, _CTRL_EPPSIGNAL_WRITE);	/* 0xc5 */
		_DO_UDELAY( 5 );

		_OUTB_CTRL( ps, _CTRL_END_REGWRITE);    /* 0xc4 */

    } else {
		if( ps->IO.delay < 2 ) {

			_DO_UDELAY( 1 );
			_OUTB_CTRL( ps, _CTRL_START_REGWRITE);
			_DO_UDELAY( 1 );
			_OUTB_CTRL( ps, _CTRL_END_REGWRITE);
		} else {

			_DO_UDELAY( 2 );
			_OUTB_CTRL( ps, _CTRL_START_REGWRITE);
			_DO_UDELAY( 2 );
			_OUTB_CTRL( ps, _CTRL_END_REGWRITE);
			_DO_UDELAY( 2 );
		}
    }
}

/** write data to the DAC - ASIC 98001/3 only
 */
_LOC void IODataRegisterToDAC( pScanData ps, Byte bReg, Byte bData )
{
    ULong i;

	IODataToRegister( ps, ps->RegADCAddress, bReg );
    IODataToRegister( ps, ps->RegADCData,    bData );
    IODataToRegister( ps, ps->RegADCSerialOutStr, bData );

    /* TEST: ORG was 1 ms for ASIC 98001 */
    _DO_UDELAY( 12 );

    for( i = 4; i; i-- ) {

    	_OUTB_CTRL( ps, _CTRL_START_DATAWRITE );
        _DO_UDELAY( 5 );
    	_OUTB_CTRL( ps, _CTRL_END_DATAWRITE );
        _DO_UDELAY( 12 );
    }
}

/** Calling SITUATION: Scanner path was not established.
 * Read the content of specific asics' register
 */
_LOC Byte IODataRegisterFromScanner( pScanData ps, Byte bReg )
{
    Byte bData;

    ps->OpenScanPath( ps );
    bData = IODataFromRegister( ps, bReg );
    ps->CloseScanPath( ps );

    return bData;
}

/** Calling SITUATION: Scanner path not established.
 * Write a value of register to asic
 */
_LOC void IOCmdRegisterToScanner( pScanData ps, Byte bReg, Byte bData )
{
    ps->OpenScanPath( ps );
    IODataToRegister( ps, bReg, bData );
    ps->CloseScanPath( ps );
}

/** Calling SITUATION: Scanner path not established.
 * Write a register to asic (used for a command without parameter)
 */
_LOC void IORegisterDirectToScanner( pScanData ps, Byte bReg )
{
    ps->OpenScanPath( ps );				/* establish the connection */
    IORegisterToScanner( ps, bReg );	/* write register to asic	*/
    ps->CloseScanPath( ps );			/* disconnect				*/
}

/** perform a SW reset of ASIC 98003 models
 */
_LOC void IOSoftwareReset( pScanData ps )
{
    if( _ASIC_IS_98003 != ps->sCaps.AsicID )
        return;

    ps->OpenScanPath( ps );

    IODataToRegister( ps, ps->RegTestMode, _SW_TESTMODE );

    ioSwitchToSPPMode( ps );

    _OUTB_DATA( ps, _RESET1ST );
    _DODELAY( 5 );

    _OUTB_DATA( ps, _RESET2ND );
    _DODELAY( 5 );

    _OUTB_DATA( ps, _RESET3RD );
    _DODELAY( 5 );

    _OUTB_DATA( ps, _RESET4TH );
    _DODELAY( 5 );

    ioRestoreParallelMode( ps );

    /* reset test mode register */
    IODataToRegister( ps, ps->RegTestMode, 0 );
    IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl );

    ps->CloseScanPath( ps );
}

/** Read specific length data from scanner and the method depends on the
 *  mode defined in registry.
 */
_LOC void IOReadScannerImageData( pScanData ps, pUChar pBuf, ULong size )
{
	if( _ASIC_IS_98003 != ps->sCaps.AsicID )
		ps->OpenScanPath( ps);

	if( _IS_ASIC98( ps->sCaps.AsicID))
		IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl );

	/* enter read mode */
	ioEnterReadMode( ps );

	/* call corresponding read proc */
	ps->Device.ReadData( ps, pBuf, size );

	/* Clear EPP/ECP read mode by simply close scanner path and re-open it */
	ps->CloseScanPath( ps );

	if( _ASIC_IS_98003 == ps->sCaps.AsicID )
		ps->OpenScanPath( ps );
}

#ifdef __KERNEL__

/** the wrapper functions to support delayed and non-delayed I/O
 */
_LOC void IOOut( Byte data, UShort port )
{
	DBG( DBG_IOF, "outb(0x%04x, 0x%02x)\n", port, data );
	outb( data, port );
}

_LOC void IOOutDelayed( Byte data, UShort port )
{
	DBG( DBG_IOF, "outb_p(0x%04x, 0x%02x)\n", port, data );
	outb_p( data, port );
}

_LOC Byte IOIn( UShort port )
{
#ifdef DEBUG
	Byte data = inb( port );

	DBG( DBG_IOF, "inb(0x%04x) = 0x%02x\n", port, data );
	return data;
#else
	return inb( port );
#endif
}

_LOC Byte IOInDelayed( UShort port )
{
#ifdef DEBUG
	Byte data = inb_p( port );

	DBG( DBG_IOF, "inb_p(0x%04x) = 0x%02x\n", port, data );
	return data;
#else
	return inb_p( port );
#endif
}
#endif /* guard __KERNEL__ */

/* END PLUSTEK-PP_IO.C ......................................................*/
