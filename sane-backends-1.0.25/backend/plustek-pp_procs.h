/** @file plustek-pp_procs.h
 *  @brief here are the prototypes of all exported functions
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson <rick@efn.org>
 *
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - no changes
 * 0.33 - no changes
 * 0.34 - added this history
 * 0.35 - added Kevins´ changes to MiscRestorePort
 *		  changed prototype of MiscReinitStruct
 *		  added prototype for function PtDrvLegalRequested()
 * 0.36 - added prototype for function MiscLongRand()
 *		  removed PtDrvLegalRequested()
 *		  changed prototype of function MiscInitPorts()
 * 0.37 - added io.c and procfs.c
 *        added MiscGetModelName()
 *        added ModelSetA3I()
 * 0.38 - added P12 stuff
 *        removed prototype of IOScannerIdleMode()
 *        removed prototype of IOSPPWrite()
 * 0.39 - moved prototypes for the user space stuff to plustek-share.h
 * 0.40 - no changes
 * 0.41 - no changes
 * 0.42 - added MapAdjust
 * 0.43 - no changes
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
#ifndef __PROCS_H__
#define __PROCS_H__

#ifdef _BACKEND_ENABLED
# define _LOC	static
#else
# define _LOC
#endif

/*
 * implementation in plustek-pp_misc.c
 */
_LOC pScanData MiscAllocAndInitStruct( void );
_LOC int       MiscReinitStruct      ( pScanData ps );

_LOC int  MiscInitPorts  ( pScanData ps, int port );
_LOC void MiscRestorePort( pScanData ps );
_LOC void MiscStartTimer ( pTimerDef timer, unsigned long us );
_LOC int  MiscCheckTimer ( pTimerDef timer );

_LOC int  MiscRegisterPort       ( pScanData ps, int portAddr );
_LOC void MiscUnregisterPort     ( pScanData ps );
_LOC int  MiscClaimPort          ( pScanData ps );
_LOC void MiscReleasePort        ( pScanData ps );
_LOC Long MiscLongRand           ( void );
_LOC const char *MiscGetModelName( UShort id );

#ifdef DEBUG
_LOC Bool MiscAllPointersSet( pScanData ps );
#endif

/*
 * implementation in plustek-pp_detect.c
 */
_LOC int DetectScanner( pScanData ps, int mode );

/*
 * implementation in plustek-pp_p48xx.c
 */
_LOC int P48xxInitAsic( pScanData ps );

/*
 * implementation in plustek-pp_p9636.c
 */
_LOC int P9636InitAsic( pScanData ps );

/*
 * implementation in plustek-pp_p12.c
 */
_LOC int  P12InitAsic          ( pScanData ps );
_LOC void P12SetGeneralRegister( pScanData ps );

/*
 * implementation in plustek-pp_p12ccd.c
 */
_LOC void P12InitCCDandDAC( pScanData ps, Bool shading );

/*
 * implementation in plustek-pp_models.c
 */
_LOC void ModelSet4800 ( pScanData ps );
_LOC void ModelSet4830 ( pScanData ps );
_LOC void ModelSet600  ( pScanData ps );
_LOC void ModelSet12000( pScanData ps );
_LOC void ModelSetA3I  ( pScanData ps );
_LOC void ModelSet9630 ( pScanData ps );
_LOC void ModelSet9636 ( pScanData ps );
_LOC void ModelSetP12  ( pScanData ps );

/*
 * implementation in plustek-pp_dac.c
 */
_LOC int  DacInitialize( pScanData ps );

_LOC void DacP98AdjustDark					   ( pScanData ps );
_LOC void DacP98FillGainOutDirectPort		   ( pScanData ps );
_LOC void DacP98FillShadingDarkToShadingRegister( pScanData ps );

_LOC void DacP96WriteBackToGammaShadingRAM( pScanData ps );

_LOC void DacP98003FillToDAC (pScanData ps, pRGBByteDef regs, pColorByte data);
_LOC void DacP98003AdjustGain(pScanData ps, ULong color, Byte hilight );
_LOC Byte DacP98003SumGains  ( pUChar pb, ULong pixelsLine );

/*
 * implementation in plustek-pp_motor.c
 */
_LOC int  MotorInitialize	 ( pScanData ps );
_LOC void MotorSetConstantMove( pScanData ps, Byte bMovePerStep );
_LOC void MotorToHomePosition ( pScanData ps );

_LOC void MotorP98GoFullStep  ( pScanData ps, ULong dwStep );

_LOC void MotorP96SetSpeedToStopProc( pScanData ps );
_LOC void MotorP96ConstantMoveProc  ( pScanData ps, ULong dwLines );
_LOC Bool MotorP96AheadToDarkArea   ( pScanData ps );
_LOC void MotorP96AdjustCurrentSpeed( pScanData ps, Byte bSpeed );

_LOC void MotorP98003BackToHomeSensor     ( pScanData ps );
_LOC void MotorP98003ModuleForwardBackward( pScanData ps );
_LOC void MotorP98003ForceToLeaveHomePos  ( pScanData ps );
_LOC void MotorP98003PositionYProc        ( pScanData ps, ULong steps);

/*
 * implementation in plustek-pp_map.c
 */
_LOC void MapInitialize ( pScanData ps );
_LOC void MapSetupDither( pScanData ps );
_LOC void MapAdjust     ( pScanData ps, int which );

/*
 * implementation in plustek-pp_image.c
 */
_LOC int ImageInitialize( pScanData ps );

/*
 * implementation in plustek-pp_genericio.c
 */
_LOC int  IOFuncInitialize      ( pScanData ps );
_LOC Byte IOSetToMotorRegister  ( pScanData ps );
_LOC Byte IOGetScanState        ( pScanData ps, Bool fOpenned );
_LOC Byte IOGetExtendedStatus   ( pScanData ps );
_LOC void IOGetCurrentStateCount( pScanData, pScanState pScanStep);
_LOC int  IOIsReadyForScan      ( pScanData ps );

_LOC void  IOSetXStepLineScanTime( pScanData ps, Byte b );
_LOC void  IOSetToMotorStepCount ( pScanData ps );
_LOC void  IOSelectLampSource    ( pScanData ps );
_LOC Bool  IOReadOneShadingLine  ( pScanData ps, pUChar pBuf, ULong len );
_LOC ULong IOReadFifoLength      ( pScanData ps );
_LOC void  IOPutOnAllRegisters   ( pScanData ps );
_LOC void  IOReadColorData       ( pScanData ps, pUChar pBuf, ULong len );

/*
 * implementation in plustek-pp_io.c
 */
_LOC int  IOInitialize        ( pScanData ps );
_LOC void IOMoveDataToScanner ( pScanData ps, pUChar pBuffer, ULong size );
_LOC void IODownloadScanStates( pScanData ps );
_LOC void IODataToScanner     ( pScanData, Byte bValue );
_LOC void IODataToRegister    ( pScanData ps, Byte bReg, Byte bData );
_LOC Byte IODataFromRegister  ( pScanData ps, Byte bReg );
_LOC void IORegisterToScanner ( pScanData ps, Byte bReg );
_LOC void IODataRegisterToDAC ( pScanData ps, Byte bReg, Byte bData );

_LOC Byte IODataRegisterFromScanner( pScanData ps, Byte bReg );
_LOC void IOCmdRegisterToScanner   ( pScanData ps, Byte bReg, Byte bData );
_LOC void IORegisterDirectToScanner( pScanData, Byte bReg );
_LOC void IOSoftwareReset          ( pScanData ps );
_LOC void IOReadScannerImageData   ( pScanData ps, pUChar pBuf, ULong size );

#ifdef __KERNEL__
_LOC void IOOut       ( Byte data, UShort port );
_LOC void IOOutDelayed( Byte data, UShort port );
_LOC Byte IOIn        ( UShort port );
_LOC Byte IOInDelayed ( UShort port );
#endif

/*
 * implementation in plustek-pp_tpa.c
 */
_LOC void TPAP98001AverageShadingData( pScanData ps );
_LOC void TPAP98003FindCenterPointer ( pScanData ps );
_LOC void TPAP98003Reshading         ( pScanData ps );

/*
 * implementation in plustek-pp_scale.c
 */
_LOC void ScaleX( pScanData ps, pUChar inBuf, pUChar outBuf );

/*
 * implementation in plustek-pp_procfs.c (Kernel-mode only)
 */
#ifdef __KERNEL__
int  ProcFsInitialize      ( void );
void ProcFsShutdown        ( void );
void ProcFsRegisterDevice  ( pScanData ps );
void ProcFsUnregisterDevice( pScanData ps );
#endif

#endif	/* guard __PROCS_H__ */

/* END PLUSTEK-PP_PROCS.H ...................................................*/
