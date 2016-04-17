/* @file u12-motor.c
 * @brief all functions for motor control
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - no changes
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

/*************************** some definitons *********************************/

#define _BACKSTEPS     120
#define _FORWARDSTEPS  120

/**************************** local vars *************************************/

static TimerDef u12motor_Timer;


/*************************** local functions *********************************/

/**
 */
static void u12motor_DownloadNullScanStates( U12_Device *dev )
{
	memset( dev->scanStates, 0, _SCANSTATE_BYTES );
	u12io_DownloadScanStates( dev );
}

/**
 */
static void u12motor_Force16Steps( U12_Device *dev, int dir )
{
	u_long dw;

	if( dir == _DIR_FW )
		u12io_DataToRegister( dev, REG_MOTOR0CONTROL, _FORWARD_MOTOR );
	else if( dir == _DIR_BW )
		u12io_DataToRegister( dev, REG_MOTOR0CONTROL, _BACKWARD_MOTOR );

	for( dw = 16; dw; dw-- ) {
		u12io_RegisterToScanner( dev, REG_FORCESTEP );
		_DODELAY( 10 );
	}
}

/**
 */
static void u12motor_ModuleFreeRun( U12_Device *dev, u_long steps )
{
	SANE_Byte rb[6];
	
	rb[0] = REG_MOTORFREERUNCOUNT1;  rb[1] = _HIBYTE(steps);
	rb[2] = REG_MOTORFREERUNCOUNT0;  rb[3] = _LOBYTE(steps);
	rb[4] = REG_MOTORFREERUNTRIGGER; rb[5] = 0;

	u12io_DataToRegs( dev, rb, 3 );
}

/**
 */
static SANE_Status u12motor_PositionYProc( U12_Device *dev, u_long steps )
{
	TimerDef timer;

	DBG( _DBG_INFO, "u12motor_PositionYProc()\n" );
	u12io_StartTimer( &timer, _SECOND * 5 );

	u12io_ResetFifoLen();
	while(!(u12io_GetScanState( dev ) & _SCANSTATE_STOP) &&
                                                (!u12io_CheckTimer( &timer )));
	_DODELAY( 12 );
	u12motor_ModuleFreeRun( dev, steps );
	_DODELAY( 15 );

	u12io_StartTimer( &timer, _SECOND * 30 );
	do  {
		if( !(u12io_GetExtendedStatus( dev ) & _STILL_FREE_RUNNING)) {
			break;
		}

		if( u12io_IsEscPressed()) {
			DBG( _DBG_INFO, "* CANCEL detected!\n" );
			return SANE_STATUS_CANCELLED;
		}   

	} while( !u12io_CheckTimer( &timer ));
	DBG( _DBG_INFO, "u12motor_PositionYProc() - done\n" );
	return SANE_STATUS_GOOD;
}

/** initialize this module and setup the correct function pointer according
 * to the ASIC
 */
static void u12motor_PositionModuleToHome( U12_Device *dev )
{
	SANE_Byte rb[50];
	SANE_Byte save, saveModel;
	int       c = 0;

	DBG( _DBG_INFO, "u12motor_PositionModuleToHome()\n" );
	saveModel = dev->regs.RD_ModelControl;

	dev->scan.refreshState = SANE_FALSE;
	u12motor_DownloadNullScanStates( dev );

	_DODELAY( 125 );
	save = dev->shade.intermediate;

	dev->shade.intermediate = _ScanMode_AverageOut;
	u12hw_InitAsic( dev, SANE_FALSE );
	dev->shade.intermediate = save;

	_SET_REG( rb, c, REG_MODECONTROL, _ModeScan );
	_SET_REG( rb, c, REG_RESETMTSC, 0 );
	_SET_REG( rb, c, REG_SCANCONTROL1, 0 );
	_SET_REG( rb, c, REG_MODELCONTROL, (dev->ModelCtrl | _MODEL_DPI300));
	_SET_REG( rb, c, REG_LINECONTROL, 80 );
	_SET_REG( rb, c, REG_XSTEPTIME, dev->XStepBack );
	_SET_REG( rb, c, REG_MOTORDRVTYPE, dev->MotorPower );
	_SET_REG( rb, c, REG_MOTOR0CONTROL, (_MotorHHomeStop | _MotorOn |
	                                  _MotorHQuarterStep | _MotorPowerEnable));
	_SET_REG( rb, c, REG_STEPCONTROL, (_MOTOR0_SCANSTATE | _MOTOR_FREERUN));
	u12io_DataToRegs( dev, rb, c );

	memset( dev->scanStates, 0x88, _SCANSTATE_BYTES );
	u12io_DownloadScanStates( dev );

	u12io_RegisterToScanner( dev, REG_REFRESHSCANSTATE );
	dev->regs.RD_ModelControl = saveModel;
}

/** function to bring the sensor back home
 */
static void u12motor_ToHomePosition( U12_Device *dev, SANE_Bool wait )
{
	TimerDef  timer;

	DBG( _DBG_INFO, "Waiting for Sensor to be back in position\n" );
	if( !(u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER)) {

		u12motor_PositionModuleToHome( dev );

		if( wait ) {
			u12io_StartTimer( &timer, _SECOND * 20);
			do {
				if( u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER)
					break;
			} while( !u12io_CheckTimer( &timer ));
		}
	}
	DBG( _DBG_INFO, "- done !\n" );
}

/**
 */
static SANE_Status u12motor_BackToHomeSensor( U12_Device *dev )
{
	SANE_Byte rb[20];
	int       c;
	TimerDef  timer;

	DBG( _DBG_INFO, "u12Motor_BackToHomeSensor()\n" );

	c = 0;
	_SET_REG( rb, c, REG_STEPCONTROL, _MOTOR0_SCANSTATE );
	_SET_REG( rb, c, REG_MODECONTROL, _ModeScan );

	u12io_DataToRegs( dev, rb, c );

	u12motor_Force16Steps( dev, _DIR_NONE );

	/* stepping every state */
	memset( dev->scanStates, 0x88, _SCANSTATE_BYTES );
	u12io_DownloadScanStates( dev );
	_DODELAY(50);

	u12io_StartTimer( &timer, _SECOND * 2 );

	u12io_ResetFifoLen();
	while(!(u12io_GetScanState( dev ) & _SCANSTATE_STOP) &&
                                                  !u12io_CheckTimer( &timer )) {
		if( u12io_IsEscPressed()) {
			DBG( _DBG_INFO, "* CANCEL detected!\n" );
			return SANE_STATUS_CANCELLED;
		}
	}

	u12motor_Force16Steps( dev, _DIR_BW );
	dev->regs.RD_ModeControl = _ModeScan;

	c = 0;

	if(!(dev->DataInf.dwScanFlag & _SCANDEF_TPA)) {
		_SET_REG( rb, c, REG_LINECONTROL, _LOBYTE(dev->shade.wExposure));
		_SET_REG( rb, c, REG_XSTEPTIME,   _LOBYTE(dev->shade.wXStep));
	} else {
		_SET_REG( rb, c, REG_LINECONTROL, _DEFAULT_LINESCANTIME );
		_SET_REG( rb, c, REG_XSTEPTIME,   6 );
	}

	_SET_REG( rb, c, REG_STEPCONTROL, (_MOTOR_FREERUN | _MOTOR0_SCANSTATE));
	_SET_REG( rb, c, REG_MOTOR0CONTROL,
	                  (_MotorHQuarterStep | _MotorOn | _MotorDirBackward |
                                    	_MotorPowerEnable | _MotorHHomeStop));
	_SET_REG( rb, c, REG_REFRESHSCANSTATE, 0);
	u12io_DataToRegs( dev, rb, c );

	u12io_StartTimer( &timer, _SECOND * 5 );
	do {
		if( u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER )
			break;

		if( u12io_IsEscPressed()) {
			DBG( _DBG_INFO, "* CANCEL detected!\n" );
			return SANE_STATUS_CANCELLED;
		}

		_DODELAY( 55 );

	} while( !u12io_CheckTimer( &timer ));

	c = 0;
	_SET_REG( rb, c, REG_LINECONTROL, dev->regs.RD_LineControl);
	_SET_REG( rb, c, REG_XSTEPTIME,   dev->regs.RD_XStepTime);
	u12io_DataToRegs( dev, rb, c );

	DBG( _DBG_INFO, "* LineCtrl=0x%02x, XStepTime=0x%02x\n",
	                dev->regs.RD_LineControl, dev->regs.RD_XStepTime );

	u12motor_DownloadNullScanStates( dev );
	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Status u12motor_ModuleToHome( U12_Device *dev )
{
	SANE_Status res;

	DBG( _DBG_INFO, "u12motor_ModuleToHome()\n" );
	if(!(u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER)) {

		u12io_DataToRegister( dev, REG_MOTOR0CONTROL,
		             (SANE_Byte)(dev->regs.RD_Motor0Control|_MotorDirForward));

		res = u12motor_PositionYProc( dev, 40 );
		if( SANE_STATUS_GOOD != res )
			return res;
			
		res = u12motor_BackToHomeSensor( dev );
		if( SANE_STATUS_GOOD != res )
			return res;

		_DODELAY( 250 );
	}
	DBG( _DBG_INFO, "* done.\n" );
	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Status u12motor_WaitForPositionY( U12_Device *dev )
{
	SANE_Byte   rb[20];
	SANE_Status res;
	SANE_Byte   bXStep;
	int         c;
	u_long      dwBeginY;

	c        = 0;
	dwBeginY = (u_long)dev->DataInf.crImage.y * 4 + dev->scan.dwScanOrigin;

	if( dev->DataInf.wPhyDataType <= COLOR_256GRAY ) {
		if( dev->f0_8_16 )
			dwBeginY += 16;
    	else
			dwBeginY += 8;
	}

	bXStep = (SANE_Byte)((dev->DataInf.wPhyDataType <= COLOR_256GRAY) ?
	                      dev->XStepMono : dev->XStepColor);

	if( dev->shade.intermediate & _ScanMode_AverageOut )
		bXStep = 8;

	u12motor_Force16Steps( dev, _DIR_NONE );
	dwBeginY -= 16;

	if( dwBeginY > (_RFT_SCANNING_ORG + _YOFFSET) &&
	                                        bXStep < dev->regs.RD_XStepTime ) {

		u12io_DataToRegister( dev, REG_MOTORDRVTYPE, dev->MotorPower );
		_DODELAY( 12 );
		u12io_DataToRegister( dev, REG_XSTEPTIME, bXStep);
		u12io_DataToRegister( dev, REG_EXTENDEDXSTEP, 0 );
		u12io_DataToRegister( dev, REG_SCANCONTROL1,
		         (SANE_Byte)(dev->regs.RD_ScanControl1 & ~_MFRC_RUNSCANSTATE));
		res = u12motor_PositionYProc( dev, dwBeginY - 64 );
		if( SANE_STATUS_GOOD != res )
			return res;
		dwBeginY = 64;
	} else {
		_SET_REG(rb, c, REG_SCANCONTROL1, dev->regs.RD_ScanControl1 );
	}

	_SET_REG( rb, c, REG_FIFOFULLEN0, _LOBYTE(dev->regs.RD_BufFullSize));
	_SET_REG( rb, c, REG_FIFOFULLEN1, _HIBYTE(dev->regs.RD_BufFullSize));
	_SET_REG( rb, c, REG_FIFOFULLEN2, _LOBYTE(_HIWORD(dev->regs.RD_BufFullSize)));
	u12io_DataToRegs( dev, rb, c );

	u12io_DataToRegister( dev, REG_MOTORDRVTYPE, dev->regs.RD_MotorDriverType);
	_DODELAY( 12 );

	if(!dev->f2003 || (dev->shade.intermediate & _ScanMode_AverageOut) ||
		    (  dev->DataInf.xyAppDpi.y <= 75 &&
	                              dev->DataInf.wPhyDataType <= COLOR_256GRAY)) {
		u12io_DataToRegister( dev, REG_MOTORDRVTYPE,
	                (SANE_Byte)(dev->MotorPower & (_MOTORR_MASK | _MOTORR_STRONG)));
	} else {
		u12io_DataToRegister( dev, REG_MOTORDRVTYPE,
	                          dev->regs.RD_MotorDriverType );
	}

	c = 0;
	_SET_REG( rb, c, REG_XSTEPTIME,     dev->regs.RD_XStepTime );
	_SET_REG( rb, c, REG_EXTENDEDXSTEP, dev->regs.RD_ExtXStepTime );
	_SET_REG( rb, c, REG_SCANCONTROL1,
                    (SANE_Byte)(dev->regs.RD_ScanControl1 & ~_MFRC_RUNSCANSTATE));
	u12io_DataToRegs( dev, rb, c );

	if( dev->DataInf.dwScanFlag & _SCANDEF_PREVIEW ) {

		TimerDef timer;

		u12motor_ModuleFreeRun( dev, dwBeginY );
		_DODELAY( 15 );

		u12io_StartTimer( &timer, (_SECOND * 20));

		while(( u12io_GetExtendedStatus( dev ) & _STILL_FREE_RUNNING) &&
		                                             !u12io_CheckTimer(&timer));
			u12io_DataToRegister( dev, REG_MODECONTROL, _ModeScan );
	} else {
		u12motor_PositionYProc( dev, dwBeginY );
		u12io_RegisterToScanner( dev, REG_REFRESHSCANSTATE );
	}
	return SANE_STATUS_GOOD;
}

/**
 */
static void u12motor_ForceToLeaveHomePos( U12_Device *dev )
{
	SANE_Byte rb[4];
	TimerDef  timer;

	DBG( _DBG_INFO, "u12motor_ForceToLeaveHomePos()\n" );
	rb[0] = REG_STEPCONTROL;
	rb[1] = _MOTOR0_ONESTEP;
	rb[2] = REG_MOTOR0CONTROL;
	rb[3] = _FORWARD_MOTOR;
	u12io_DataToRegs( dev, rb, 2 );

	u12io_StartTimer( &timer, _SECOND );

	do {
		if( !(u12io_DataFromRegister( dev, REG_STATUS ) & _FLAG_PAPER))
			break;

		u12io_RegisterToScanner( dev, REG_FORCESTEP );
		_DODELAY( 10 );

	} while( !u12io_CheckTimer( &timer ));

	u12io_DataToRegister( dev, REG_STEPCONTROL, _MOTOR0_SCANSTATE );
}

/** move the sensor to the appropriate shading position
 */
static SANE_Status u12motor_GotoShadingPosition( U12_Device *dev )
{
	SANE_Byte   rb[20];
	SANE_Status res;
	int         c;
	
	DBG( _DBG_INFO, "u12motor_GotoShadingPosition()\n" );
	res = u12motor_ModuleToHome( dev );
	if( SANE_STATUS_GOOD == res )
		return res;

	/* position to somewhere under the transparency adapter */
	if( dev->DataInf.dwScanFlag & _SCANDEF_TPA ) {

		u12motor_ForceToLeaveHomePos( dev );
		u12motor_DownloadNullScanStates( dev );

		c = 0;
		_SET_REG( rb, c, REG_STEPCONTROL, _MOTOR0_SCANSTATE );
		_SET_REG( rb, c, REG_MODECONTROL, _ModeScan);
		_SET_REG( rb, c, REG_MOTOR0CONTROL, _FORWARD_MOTOR );
		_SET_REG( rb, c, REG_XSTEPTIME, 6);
		_SET_REG( rb, c, REG_EXTENDEDXSTEP, 0);
		_SET_REG( rb, c, REG_SCANCONTROL1, _MFRC_BY_XSTEP);
		u12io_DataToRegs( dev, rb, c );

		res = u12motor_PositionYProc( dev, _TPA_SHADINGORG );
		if( SANE_STATUS_GOOD != res )
			return res;
	}
	DBG( _DBG_INFO, "* Position reached\n" );
	return SANE_STATUS_GOOD;
}

/**
 */
static void u12motor_ModuleForwardBackward( U12_Device *dev )
{
	DBG( _DBG_INFO, "u12motor_ModuleForwardBackward()\n" );

	switch( dev->scan.bModuleState ) {

	case _MotorInNormalState:
		DBG( _DBG_INFO, "* _MotorInNormalState\n" );
		dev->scan.bModuleState = _MotorGoBackward;
		u12io_DataToRegister( dev, REG_SCANCONTROL1,
			     (SANE_Byte)(dev->regs.RD_ScanControl1 & ~_MFRC_RUNSCANSTATE));
		u12io_DataToRegister( dev, REG_MOTOR0CONTROL,
			      (SANE_Byte)(dev->regs.RD_Motor0Control & ~_MotorDirForward));

		u12motor_ModuleFreeRun( dev, _BACKSTEPS );
		u12io_StartTimer( &u12motor_Timer, (15 * _MSECOND));
		break;

	case _MotorGoBackward:
		DBG( _DBG_INFO, "* _MotorGoBackward\n" );
		if( u12io_CheckTimer( &u12motor_Timer)) {
			if(!(u12io_GetExtendedStatus( dev ) & _STILL_FREE_RUNNING )) {
				dev->scan.bModuleState = _MotorInStopState;
				u12io_StartTimer( &u12motor_Timer, (50 *_MSECOND));
			}
		}
		break;

	case _MotorInStopState:
		DBG( _DBG_INFO, "* _MotorInStopState\n" );
		if( u12io_CheckTimer( &u12motor_Timer )) {

			if( u12io_GetFifoLength( dev ) < dev->scan.dwMaxReadFifo ) {
				dev->scan.bModuleState = _MotorAdvancing;
				u12io_DataToRegister( dev, REG_SCANCONTROL1,
				                                    dev->regs.RD_ScanControl1);
				u12io_DataToRegister( dev, REG_MOTOR0CONTROL,
				                                    dev->regs.RD_Motor0Control);
				u12motor_ModuleFreeRun( dev, _FORWARDSTEPS );
				u12io_StartTimer( &u12motor_Timer, (15 * _MSECOND));
			}
		}
		break;

	case _MotorAdvancing:
		DBG( _DBG_INFO, "* _MotorAdvancing\n" );
		if( u12io_CheckTimer( &u12motor_Timer)) {
			if( !(u12io_GetScanState( dev ) & _SCANSTATE_STOP))
				dev->scan.bModuleState = _MotorInNormalState;
			else {
				if (!(u12io_GetExtendedStatus( dev ) & _STILL_FREE_RUNNING )) {
					u12io_RegisterToScanner( dev, REG_REFRESHSCANSTATE );
					dev->scan.bModuleState = _MotorInNormalState;
				}
			}
		}
		break;
	}
}

/* END U12-MOTOR.C ..........................................................*/
