/* @file plustek-pp_motor.c
 * @brief all functions for motor control
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - slight cosmetic changes
 *        - added function MotorToHomePosition()
 * - 0.33 - added additional debug-messages
 *        - increased speed for returning to homeposition for Models >= 9630
 *          (and ASIC 96003)
 * - 0.34 - added FIFO overflow check in motorP96SetSpeed
 *        - removed WaitBack() function from pScanData structure
 *        - removed wStayMaxStep from pScanData structure
 * - 0.35 - changed motorP96UpdateDataCurrentReadLine() to handle proper
 *        - work of ASIC96003 based 600dpi models
 * - 0.36 - merged motorP96WaitBack and motorP98WaitBack to motorWaitBack
 *        - merged motorP96SetSpeed and motorP98SetSpedd to motorSetSpeed
 *        - added Sensor-Check in function MotorToHomePosition
 *        - reduced number of forward steps for MotorToHomePosition
 * - 0.37 - removed function motorP96GetStartStopGap() - no longer used
 *        - removed a_bStepDown1Table and a_bStepUp1Table
 *        - removed // comments
 *        - added A3I stuff
 * - 0.38 - cosmetic changes
 *        - added P12 stuff
 * - 0.39 - Did some finetuning in MotorP98003ModuleForwardBackward()
 *        - Fixed a problem, that could cause the driver to throw a
 *          kernel exception
 * - 0.40 - changed back to build 0.39-3 (disabled A3I stuff)
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

/*************************** some definitons *********************************/

/* #define _A3I_EN */

/*
 * adjustments for scanning in negative and tranparency mode
 */
#define _NEG_SCANNINGPOS 	    770
#define _POS_SCANNINGPOS 	    660        /* original value was 710 */

#define _P98_BACKMOVES			0x3d
#define _P98_FORWARDMOVES		0x3b		/* Origin = 3c */

#define _P98003_BACKSTEPS	    120
#define _P98003_FORWARDSTEPS    120

#define _P96_BACKMOVES			130
#define _P96_FORWARDMOVES		87   /* 95 */
#define _P96_FIFOOVERFLOWTHRESH	180


#define _COLORRUNTABLE_RED		0x11
#define _COLORRUNTABLE_GREEN	0x22
#define _COLORRUNTABLE_BLUE		0x44

#define	_BW_ORIGIN				0x0D
#define	_GRAY_ORIGIN			0x0B
#define	_COLOR_ORIGIN			0x0B

#define _P98003_YOFFSET			300

/**************************** local vars *************************************/

static TimerDef p98003MotorTimer;

static UShort 	a_wMoveStepTable [_NUMBER_OF_SCANSTEPS];
static Byte		a_bScanStateTable[_SCANSTATE_TABLE_SIZE];
static Byte		a_bHalfStepTable [_NUMBER_OF_SCANSTEPS];
static Byte		a_bColorByteTable[_NUMBER_OF_SCANSTEPS];
static Byte		a_bColorsSum[8] = {0, 1, 1, 2, 1, 2, 2, 3};

static pUShort	pwEndMoveStepTable  = a_wMoveStepTable  + _NUMBER_OF_SCANSTEPS;
static pUChar	pbEndColorByteTable = a_bColorByteTable + _NUMBER_OF_SCANSTEPS;
static pUChar	pbEndHalfStepTable  = a_bHalfStepTable  + _NUMBER_OF_SCANSTEPS;

/*
 * for the 96001/3 based units
 */
static UShort wP96BaseDpi = 0;

static Byte	a_bStepDown1Table[20]  = {3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static Byte a_bStepUp1Table[20]    = {4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static Byte	a_bMotorDown2Table[20] = {0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};

#ifndef _A3I_EN
static Byte	a_bHalfStep2Table[32] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
									 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static Byte	a_bHalfStep4Table[16] = {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
static Byte	a_bHalfStep6Table[12] = {3,3,3,3,3,3,3,3,3,3,3,3};
static Byte	a_bHalfStep8Table[8]  = {4,4,4,4,4,4,4,4};
static Byte	a_bHalfStep10Table[8] = {5,5,5,5,5,5,5,5};
static Byte	a_bHalfStep12Table[6] = {6,6,6,6,6,6};
static Byte	a_bHalfStep14Table[6] = {7,7,7,7,7,7};
static Byte	a_bHalfStep16Table[4] = {8,8,8,8};
static Byte	a_bHalfStep18Table[4] = {9,9,9,9};
static Byte	a_bHalfStep20Table[4] = {10,10,10,10};
static Byte	a_bHalfStep22Table[4] = {11,11,11,11};
static Byte	a_bHalfStep24Table[4] = {12,12,12,12};
static Byte	a_bHalfStep26Table[4] = {13,13,13,13};
static Byte	a_bHalfStep28Table[4] = {14,14,14,14};
static Byte	a_bHalfStep30Table[4] = {15,15,15,15};
static Byte	a_bHalfStep32Table[2] = {16,16};
static Byte	a_bHalfStep34Table[2] = {17,17};
static Byte	a_bHalfStep36Table[2] = {18,18};
static Byte	a_bHalfStep38Table[2] = {19,19};
static Byte	a_bHalfStep40Table[2] = {20,20};


static pUChar a_pbHalfStepTables[20] = {
	a_bHalfStep2Table,  a_bHalfStep4Table,
	a_bHalfStep6Table,  a_bHalfStep8Table,
	a_bHalfStep10Table, a_bHalfStep12Table,
	a_bHalfStep14Table, a_bHalfStep16Table,
	a_bHalfStep18Table, a_bHalfStep20Table,
	a_bHalfStep22Table, a_bHalfStep24Table,
	a_bHalfStep26Table, a_bHalfStep28Table,
	a_bHalfStep30Table, a_bHalfStep32Table,
	a_bHalfStep34Table, a_bHalfStep36Table,
	a_bHalfStep38Table, a_bHalfStep40Table
};
#endif

/*************************** local functions *********************************/

/*.............................................................................
 *
 */
static void motorP96GetStartStopGap( pScanData ps, Bool fCheckState )
{
	UChar bMotorCountDownIndex;

    if( fCheckState ) {

		ps->bMotorStepTableNo = 0xff;
		if( ps->Scan.bModuleState == _MotorInNormalState )
	    	return;
    }

    bMotorCountDownIndex = ps->bMotorSpeedData / 2;

    if( ps->bCurrentSpeed == 4 && ps->AsicReg.RD_Dpi < 80 )
		ps->bMotorStepTableNo = 4;
    else
		if( ps->Scan.bModuleState == _MotorGoBackward )
		    ps->bMotorStepTableNo = a_bStepUp1Table[bMotorCountDownIndex];
		else
	    	ps->bMotorStepTableNo = a_bStepDown1Table[bMotorCountDownIndex];
}



/*.............................................................................
 * wait for the ScanState stop or ScanState reachs the dwScanStateCount
 */
static Bool motorCheckMotorPresetLength( pScanData ps )
{
    Byte	 bScanState;
	TimerDef timer;

	MiscStartTimer( &timer, (_SECOND * 4));
    do {

		bScanState = IOGetScanState( ps, _FALSE );

		if (ps->fFullLength) {
		    if (!(bScanState & _SCANSTATE_STOP)) /* still running */
				if ((ULong)(bScanState & _SCANSTATE_MASK) != ps->dwScanStateCount )
				    continue;
		    return ps->fFullLength;
		}

 		if (bScanState & _SCANSTATE_STOP)
		    break;

		/*
		 * the code may work for all units
		 */
		if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

			if (bScanState < ps->bOldStateCount)
			    bScanState += _NUMBER_OF_SCANSTEPS;

			bScanState -= ps->bOldStateCount;

			if (bScanState >= 40)
			    return ps->fFullLength;
		}

    } while ( !MiscCheckTimer( &timer ));

	_DODELAY(1);			 /* delay one ms */

    return ps->fFullLength;
}

/*.............................................................................
 * 1) Keep the valid content of a_bColorByteTable, and fill others to 0:
 * BeginFill = ((bCurrentLineCount + DL) < 64) ? bCurrentLineCount + DL :
 *						  bCurrentLineCount + DL - 64;
 *	FillLength = 64 - DL
 *	[NOTE] Keep the content of a_bColorByteTable that begin at
 *	       bCurrentLineCount and in DL bytes
 * 2) Keep the valid content of a_bHalfStepTable, and fill the others to 0:
 * BeginFill = ((bCurrentLineCount + bCurrentSpeed / 2 + 1) < 64) ?
 *		      bCurrentLineCount + bCurrentSpeed / 2 + 1 :
 *		      bCurrentLineCount + bCurrentSpeed / 2 + 1 - 64;
 *	FillLength = 64 - (bMotorSpeedData / 2 + 1);
 */
static void motorClearColorByteTableLoop0( pScanData ps, Byte bColors )
{
	ULong  dw;
   	pUChar pb;

    if ((ps->bCurrentLineCount + bColors) >= _NUMBER_OF_SCANSTEPS) {
		pb = a_bColorByteTable + (ULong)(ps->bCurrentLineCount + bColors -
							 _NUMBER_OF_SCANSTEPS);
	} else {
		pb = a_bColorByteTable + (ULong)(ps->bCurrentLineCount + bColors);
	}

    for (dw = _NUMBER_OF_SCANSTEPS - bColors; dw; dw--) {

		*pb++ = 0;
		if (pb >= pbEndColorByteTable)
	    	pb = a_bColorByteTable;
    }

    if ((ps->bCurrentLineCount+ps->bCurrentSpeed/2+1) >= _NUMBER_OF_SCANSTEPS) {

		pb = a_bHalfStepTable + (ULong)(ps->bCurrentLineCount +
								ps->bCurrentSpeed / 2 + 1 - _NUMBER_OF_SCANSTEPS);
	} else {
		pb = a_bHalfStepTable +
			 (ULong)(ps->bCurrentLineCount + ps->bCurrentSpeed / 2 + 1);
	}

    for (dw = _NUMBER_OF_SCANSTEPS - ps->bMotorSpeedData / 2 - 1; dw; dw--) {
		*pb++ = 0;
		if (pb >= pbEndHalfStepTable)
	    	pb = a_bHalfStepTable;
    }
}

/*.............................................................................
 * motorClearColorByteTableLoop1 ()
 *   1) Adjust bNewGap:
 *	bNewGap = (bNewGap <= bNewCurrentLineCountGap) ?
 *			0 : bNewGap - bNewCurrentLineCount - 1;
 *   2) Fill the 0 to a_bColorByteTable:
 *	FillIndex = ((bCurrentLineCount + bNewGap + 1) < 64) ?
 *				 bCurrentLineCount + bNewGap + 1 :
 *				 bCurrentLineCount + bNewGap + 1 - 64;
 *	FillCount = 64 - bNewGap - 1;
 *   3) Adjust bNewGap:
 *	bNewGap = (bCurrentLineCount <= bNewCurrentLineCountGap) ?
 *			0 : bNewGap - bNewCurrentLineCount - 1;
 *   4) Fill the a_bHalfStepTable:
 *	FillIndex = ((bCurrentLineCount + bNewGap + 1) < 64) ?
 *				 bCurrentLineCount + bNewGap + 1 :
 *				 bCurrentLineCount + bNewGap + 1 - 64;
 *	FillCount = 64 - bNewGap - 1;
 */
static void motorClearColorByteTableLoop1( pScanData ps )
{
	ULong  dw = _NUMBER_OF_SCANSTEPS - 1;
    pUChar pb;

    if (ps->bNewGap > ps->bNewCurrentLineCountGap) {
		ps->bNewGap = ps->bNewGap - ps->bNewCurrentLineCountGap - 1;
		dw -= (ULong)ps->bNewGap;
    } else {
		ps->bNewGap = 0;
	}

    if ((ps->bCurrentLineCount + ps->bNewGap + 1) >= _NUMBER_OF_SCANSTEPS) {
		pb = a_bColorByteTable +
				(ULong)(ps->bCurrentLineCount+ps->bNewGap+1-_NUMBER_OF_SCANSTEPS);
    } else {
		pb = a_bColorByteTable +
							(ULong)(ps->bCurrentLineCount + ps->bNewGap + 1);
	}

    for (; dw; dw--) {
		*pb++ = 0;
		if (pb >= pbEndColorByteTable)
	    	pb = a_bColorByteTable;
    }

   	if (ps->bCurrentSpeed > ps->bNewCurrentLineCountGap) {
		ps->bNewGap = ps->bCurrentSpeed - ps->bNewCurrentLineCountGap;
		dw = _NUMBER_OF_SCANSTEPS - 1 - (ULong)ps->bNewGap;
    } else {
		dw = _NUMBER_OF_SCANSTEPS - 1;
		ps->bNewGap = 0;
    }

   	if ((ps->bCurrentLineCount + ps->bNewGap + 1) >= _NUMBER_OF_SCANSTEPS) {
		pb = a_bHalfStepTable + (ULong)(ps->bCurrentLineCount +
								ps->bNewGap + 1 - _NUMBER_OF_SCANSTEPS);
	} else {
		pb = a_bHalfStepTable + (ULong)(ps->bCurrentLineCount+ps->bNewGap +1);
	}

    for (; dw; dw--) {
		*pb++ = 0;
		if (pb >= pbEndHalfStepTable)
	    	pb = a_bHalfStepTable;
    }
}

/*.............................................................................
 * According the flag to set motor direction
 */
static void motorSetRunPositionRegister( pScanData ps )
{
	Byte bData;

	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {
	    if( ps->Scan.fMotorBackward ) {
			bData = ps->AsicReg.RD_Motor0Control & ~_MotorDirForward;
		} else {
			bData = ps->AsicReg.RD_Motor0Control | _MotorDirForward;
		}

	    IOCmdRegisterToScanner( ps, ps->RegMotor0Control, bData );

	} else {

		if( ps->Scan.fMotorBackward ) {
			bData = ps->Asic96Reg.RD_MotorControl & ~_MotorDirForward;
		} else {
			bData = ps->Asic96Reg.RD_MotorControl | _MotorDirForward;
		}

	    IOCmdRegisterToScanner( ps, ps->RegMotorControl, bData );
	}
}

/*.............................................................................
 *
 */
static void motorPauseColorMotorRunStates( pScanData ps )
{
	memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES);

	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

	    ps->a_nbNewAdrPointer[2] = 0x77;	/* Read color at the same time */

	} else {
	    ps->a_nbNewAdrPointer[2] = 1;
	    ps->a_nbNewAdrPointer[3] = 3;
    	ps->a_nbNewAdrPointer[4] = 2;
	}

	MotorSetConstantMove( ps, 0 );
}

/*.............................................................................
 * Setup the a_nbNewAdrPointer for ASIC stepping register
 */
static void motorP98FillDataToColorTable( pScanData ps,
										  Byte bIndex, ULong dwSteps)
{
	pUChar	pb;
    pUShort	pw;
    Byte	bColor;
    UShort	w;

    for ( pb = &a_bColorByteTable[bIndex],
				 		pw = &a_wMoveStepTable[bIndex]; dwSteps; dwSteps-- ) {

		if (*pw) {				/* valid state */

			if( *pw >= ps->BufferForColorRunTable ) {
				DBG( DBG_LOW, "*pw = %u > %u !!\n",
						*pw, ps->BufferForColorRunTable );
			} else {
			    bColor = ps->pColorRunTable[*pw];  		/* get the colors 	 */
			    if (a_bColorsSum[bColor & 7])		    /* need to read data */
					*pb = bColor & 7;
			}
		}

		if (++pw >= pwEndMoveStepTable)	{
		    pw = a_wMoveStepTable;
		    pb = a_bColorByteTable;
		} else
		    pb++;
    }

    /* ToCondense */
    pb = a_bColorByteTable;

    for (w = 0; w < _SCANSTATE_BYTES; w++, pb += 2)
		ps->a_nbNewAdrPointer[w] = (Byte)((*pb & 7) + ((*(pb + 1) & 7) << 4));

    /* ToCondenseMotor */
    for (pb = a_bHalfStepTable, w = 0; w < _SCANSTATE_BYTES; w++) {
		if (*pb++)
			ps->a_nbNewAdrPointer [w] |= 8;

		if (*pb++)
		    ps->a_nbNewAdrPointer [w] |= 0x80;
    }
}

/*.............................................................................
 *
 */
static void motorP98FillHalfStepTable( pScanData ps )
{
	pUChar	 pbHalfStepTbl, pb;
    pUShort	 pwMoveStep;
    DataType Data;
    ULong	 dw;

    if (1 == ps->bMotorSpeedData) {
		for (dw = 0; dw < _NUMBER_OF_SCANSTEPS; dw++)
		    a_bHalfStepTable [dw] =
						(a_wMoveStepTable [dw] > ps->wMaxMoveStep) ? 0: 1;
    } else {
		pwMoveStep 	  = &a_wMoveStepTable[ps->bCurrentLineCount];
		pbHalfStepTbl = &a_bHalfStepTable[ps->bCurrentLineCount];

		if (ps->DataInf.wAppDataType >= COLOR_TRUE24)
		    Data.dwValue = _NUMBER_OF_SCANSTEPS - 1;
		else
		    Data.dwValue = _NUMBER_OF_SCANSTEPS;

		for (; Data.dwValue; Data.dwValue--, pbHalfStepTbl++, pwMoveStep++ ) {

		    if (pwMoveStep >= pwEndMoveStepTable) {
				pbHalfStepTbl = a_bHalfStepTable;
				pwMoveStep    = a_wMoveStepTable;
		    }

		    if (*pwMoveStep) {		/* need to exposure */

				dw = (ULong)ps->bMotorSpeedData;
				if (Data.bValue < ps->bMotorSpeedData)
				    *pwMoveStep = 0;
				else {
				    *pbHalfStepTbl = 1;

				    if (ps->dwFullStateSpeed) {
						dw -= ps->dwFullStateSpeed;
						for (pb = pbHalfStepTbl; dw;
												dw -= ps->dwFullStateSpeed) {
						    pb += ps->dwFullStateSpeed;
						    if (pb >= pbEndHalfStepTable)
								pb -= _NUMBER_OF_SCANSTEPS;
			    			*pb = 1;
						}
				    }
				}
		    }
		}
    }
}

/*.............................................................................
 *
 */
static void motorP98FillBackColorDataTable( pScanData ps )
{
	Byte bIndex;

    if ((bIndex = ps->bCurrentLineCount + ps->bNewCurrentLineCountGap + 1) >=
                        							     _NUMBER_OF_SCANSTEPS) {
		bIndex -= _NUMBER_OF_SCANSTEPS;
	}

	motorP98FillDataToColorTable( ps, bIndex, (ULong)(_NUMBER_OF_SCANSTEPS -
								  ps->bNewCurrentLineCountGap));
}

/*.............................................................................
 * i/p:
 *	pScanStep = pColorRunTable if forward, a_bScanStateTable if backward
 *	dwState = how many states is requested to process
 * NOTE:
 *	The content of pScanStep contain:
 *  0: Idle state
 *  0xff: End mark
 *  others: The motor speed value
 */
static void motorP98FillBackLoop( pScanData ps,
								  pUChar pScanStep, ULong dwStates )
{
    for (ps->fFullLength = _FALSE; dwStates; dwStates--) {

		if (*pScanStep == 0xff ) {

		    ULong dw = ps->dwScanStateCount;

		    for (; dwStates; dwStates--) {
				ps->a_nbNewAdrPointer [dw / 2] &= ((dw & 1) ? 0x7f: 0xf7);
				dw = (dw + 1U) & _SCANSTATE_MASK;
		    }
		    if (!ps->dwScanStateCount)
				ps->dwScanStateCount = _NUMBER_OF_SCANSTEPS;

		    ps->dwScanStateCount--;
		    ps->fFullLength = _TRUE;
	    	break;
		} else {
		    ps->a_nbNewAdrPointer [ps->dwScanStateCount / 2] |=
					       ((ps->dwScanStateCount & 1) ? 0x80 : 0x08);
	    	if (++ps->dwScanStateCount == _NUMBER_OF_SCANSTEPS)
				ps->dwScanStateCount = 0;	/* reset to begin */

			pScanStep++;
		}
    }
    IOSetToMotorStepCount( ps );		/* put all scan states to ASIC */
}

/*.............................................................................
 *
 */
static void motorP98SetRunFullStep( pScanData ps )
{
    ps->OpenScanPath( ps );

	ps->AsicReg.RD_StepControl = _MOTOR0_SCANSTATE;
    IODataToRegister( ps, ps->RegStepControl,
					  ps->AsicReg.RD_StepControl );
    IODataToRegister( ps, ps->RegLineControl, 96 );

    if ( ps->bFastMoveFlag == _FastMove_Low_C75_G150_Back ) {
		IODataToRegister( ps, ps->RegMotor0Control,
						  (_MotorHQuarterStep + _MotorOn + _MotorDirBackward));
	} else {
		IODataToRegister( ps, ps->RegMotor0Control,
						  (_MotorHQuarterStep + _MotorOn + _MotorDirForward));
	}

    if (ps->bFastMoveFlag == _FastMove_Film_150) {
		ps->AsicReg.RD_XStepTime = 12;
	} else {
		if (ps->bFastMoveFlag == _FastMove_Fast_C50_G100) {
		    ps->AsicReg.RD_XStepTime =
				((ps->DataInf.wPhyDataType >= COLOR_TRUE24) ? 4 : 8);
		} else {
		    ps->AsicReg.RD_XStepTime =
				((ps->DataInf.wPhyDataType >= COLOR_TRUE24) ? 6 : 12);
		}
	}

	DBG( DBG_LOW, "XStepTime = %u\n", ps->AsicReg.RD_XStepTime );
	IODataToRegister( ps, ps->RegXStepTime, ps->AsicReg.RD_XStepTime);
    ps->CloseScanPath( ps );
}

/*.............................................................................
 * moves the sensor back home...
 */
static int motorP98BackToHomeSensor( pScanData ps )
{
	int		 result = _OK;
	TimerDef timer;

    MotorSetConstantMove( ps, 1 );

	ps->OpenScanPath( ps );

	ps->AsicReg.RD_StepControl =
                         (_MOTOR_FREERUN + _MOTOR0_SCANSTATE+ _MOTOR0_ONESTEP);
    IODataToRegister( ps, ps->RegStepControl, ps->AsicReg.RD_StepControl);

    ps->AsicReg.RD_ModeControl = _ModeScan;
    IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl );

    ps->AsicReg.RD_Motor0Control = _MotorHQuarterStep +
												_MotorOn + _MotorDirBackward;
    IODataToRegister( ps, ps->RegMotor0Control, ps->AsicReg.RD_Motor0Control );


    if( ps->DataInf.wPhyDataType >= COLOR_TRUE24) {
		ps->AsicReg.RD_XStepTime = ps->bSpeed24;
	} else {
		ps->AsicReg.RD_XStepTime = ps->bSpeed12;
	}

	DBG( DBG_HIGH, "XStepTime = %u\n", ps->AsicReg.RD_XStepTime );

	IODataToRegister( ps, ps->RegXStepTime, ps->AsicReg.RD_XStepTime );
    IORegisterToScanner( ps, ps->RegRefreshScanState );

	/* CHANGE: We allow up to 25 seconds for returning (org. val was 10) */
	MiscStartTimer( &timer, _SECOND * 25 );

    do {
		if (IODataFromRegister( ps, ps->RegStatus) & _FLAG_P98_PAPER ) {
		    IODataToRegister( ps, ps->RegModelControl,
				   	(Byte)(ps->AsicReg.RD_ModelControl | _HOME_SENSOR_POLARITY));
		    if(!(IODataFromRegister(ps, ps->RegStatus) & _FLAG_P98_PAPER))
				break;
		}
		_DODELAY( 10 );			/* delay 10 ms */

    } while ( !(result = MiscCheckTimer( &timer )));

    ps->CloseScanPath( ps );

	if( _OK != result )
		return result;

    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

	IOSetToMotorRegister( ps );

	return _OK;
}

/*.............................................................................
 * 1) Clear scan states
 * 2) Adjust the new scan state
 */
static void motorP98FillRunNewAdrPointer1( pScanData ps )
{
    ScanState sState;
    Byte	  bTemp;

    IOGetCurrentStateCount( ps, &sState);
    bTemp = sState.bStep;
    if (sState.bStep < ps->bOldStateCount) {
		sState.bStep += _NUMBER_OF_SCANSTEPS;/* over table (table just can	 */
	}										 /* holds 64 step, then reset to */
											 /* 0, so if less than means over*/
											 /* the table)                   */
    sState.bStep   -= ps->bOldStateCount;	 /* how many states passed   	 */
    ps->pScanState += sState.bStep;

    /*
	 * if current state != no stepped or stepped a cycle, fill the table with
     * 1 in NOT STEPPED length. (1 means to this state has to be processing).
	 */
	ps->bOldStateCount   = bTemp;
	ps->dwScanStateCount = (ULong)((bTemp + 1) & _SCANSTATE_MASK);

	motorP98FillBackLoop( ps, ps->pScanState, _NUMBER_OF_SCANSTEPS );
}

/*.............................................................................
 *
 */
static void motorP98FillRunNewAdrPointer( pScanData ps )
{
    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

    motorP98FillRunNewAdrPointer1( ps );
}

/*.............................................................................
 * move the sensor to a specific Y-position
 */
static void motorP98PositionYProc( pScanData ps, ULong dwStates )
{
    ScanState sState;

    memset( ps->pColorRunTable, 1, dwStates );
    memset( ps->pColorRunTable + dwStates, 0xff, (3800UL - dwStates));

	IOGetCurrentStateCount( ps, &sState);

	ps->bOldStateCount = sState.bStep;

    ps->OpenScanPath( ps );
    IODataToRegister( ps, ps->RegMotor0Control,
					  (Byte)(_MotorOn + _MotorHEightStep +(ps->Scan.fMotorBackward)?
						_MotorDirBackward :  _MotorDirForward));

	DBG( DBG_LOW, "XStepTime = %u\n", ps->bSpeed6 );
    IODataToRegister( ps, ps->RegXStepTime, ps->bSpeed6 );
	ps->CloseScanPath( ps );

    ps->pScanState = ps->pColorRunTable;

    ps->FillRunNewAdrPointer( ps );

    while(!motorCheckMotorPresetLength( ps ))
		motorP98FillRunNewAdrPointer1( ps );
}

/*.............................................................................
 * checks if the sensor is in it´s home position and moves it back if necessary
 */
static int motorP98CheckSensorInHome( pScanData ps )
{
	int result;

    if (!(IODataRegisterFromScanner(ps,ps->RegStatus) & _FLAG_P98_PAPER)){

		MotorSetConstantMove( ps, 1 );
		ps->Scan.fMotorBackward  = _FALSE;
		ps->bExtraMotorCtrl = 0;
		motorP98PositionYProc( ps, 20 );

		result = motorP98BackToHomeSensor( ps );
		if( _OK != result )
			return result;

		_DODELAY( 250 );
    }

	return _OK;
}

/*.............................................................................
 * move the sensor to the scan-start position
 */
static void motorP98WaitForPositionY( pScanData ps )
{
	ULong dw;
    ULong dwBX, dwDX;

    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {

		motorP98BackToHomeSensor( ps );
		_DODELAY( 100 );

/* CHECK do we need this block ? was test code in the original source code */
		ps->OpenScanPath( ps );
		IODataToRegister( ps, ps->RegModelControl, ps->AsicReg.RD_ModelControl);
		IODataToRegister( ps, ps->RegStepControl, (Byte)(_MOTOR_FREERUN +
                                         _MOTOR0_SCANSTATE + _MOTOR0_ONESTEP));
		IODataToRegister( ps, ps->RegMotor0Control, (Byte)(_MotorOn +
						  _MotorHQuarterStep + _MotorDirForward));
		ps->CloseScanPath( ps );

		for (dw=1000; dw; dw--) {
	    	if (IODataRegisterFromScanner( ps, ps->RegStatus) & _FLAG_P98_PAPER) {
				IORegisterDirectToScanner( ps, ps->RegForceStep );
				_DODELAY( 1000 / 400 );
	    	}
		}
/*-*/
		ps->AsicReg.RD_ModeControl = _ModeScan;
		IOCmdRegisterToScanner( ps, ps->RegModeControl,
								ps->AsicReg.RD_ModeControl );

		memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

		ps->Scan.fMotorBackward	= _FALSE;
		ps->bExtraMotorCtrl = 0;
		ps->bFastMoveFlag 	= _FastMove_Film_150;

		if (ps->DataInf.dwScanFlag & SCANDEF_Negative) {
	    	MotorP98GoFullStep(ps, (ps->DataInf.crImage.y+_NEG_SCANNINGPOS)/2);
		} else {
		    MotorP98GoFullStep(ps, (ps->DataInf.crImage.y+_POS_SCANNINGPOS)/2);
		}

		return;
    }

    ps->AsicReg.RD_ModeControl = _ModeScan;

    IOCmdRegisterToScanner( ps, ps->RegModeControl,
							ps->AsicReg.RD_ModeControl );

    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

    ps->Scan.fMotorBackward = _FALSE;
    ps->bExtraMotorCtrl     = 0;

    /* SetStartPoint */
	dw = ps->wInitialStep + ps->DataInf.crImage.y;

	/*
	 * CHANGE: when checking out the values from the NT registry
	 *		   I found that the values are NOT 0
	 */
    switch (ps->DataInf.wPhyDataType) {
		case COLOR_BW:		dw += _BW_ORIGIN;    break;
		case COLOR_256GRAY:	dw += _GRAY_ORIGIN;	 break;
		default:			dw += _COLOR_ORIGIN; break;
    }

    if (dw & 0x80000000)
		dw = 0; 		/* negative */

    if (dw > 180) {
		if (ps->bSetScanModeFlag & _ScanMode_Mono) {
		    dwBX = 90;				/* go via 150 dpi, so 180 / 2 = 90 */
		    dwDX = (dw -180) % 3;
	    	dw   = (dw -180) / 3; 	/* 100 dpi */
		} else {
		    dwBX = 45;				/* go via 75 dpi, so 180 / 4 = 45 */
		    dwDX = (dw -180) % 6;
	    	dw 	 = (dw -180) / 6; 	/* 50 dpi */
		}

		dwDX = (dwDX * 3 + 1) / 2 + dwBX;

		/*
		 * 100/50 dpi lines is 3/2 times of 150/75
		 * eax = (remainder * 3 + 1) / 2 + 180 / (2 or 4) lines
		 */
		ps->bFastMoveFlag = _FastMove_Low_C75_G150;
		MotorP98GoFullStep( ps, dwDX);

		if (dw) {
			DBG( DBG_LOW, "FAST MOVE MODE !!!\n" );
		    ps->bFastMoveFlag = _FastMove_Fast_C50_G100;
	    	MotorP98GoFullStep( ps, dw);
		}
    } else {
		dwBX = ((ps->bSetScanModeFlag & _ScanMode_Mono) ? 2: 4);
		dw 	 = (dw + dwBX/2) / dwBX;
		ps->bFastMoveFlag = _FastMove_Low_C75_G150;

		MotorP98GoFullStep(ps, dw);
    }
}

/*.............................................................................
 * PreMove/EndMove
 * PreMove is only in ADF and CFB mode
 * In ADF version, there is a distance gap between Paper flag and Real initial
 * position and it need premove to real initial position and turn the motor
 * Inverse and start Fast move to start scan position
 * In CFB version , PreMove 1 mm to avoid bouncing of PaperFlag sensor then
 * fast move
 * In CIS and CSP although there have not PreMove but there have End move
 * when paper out there still have several mm need to read,
 * So it need EndMove 2mm and set Inverse Paper
 */
static Bool motorP98GotoShadingPosition( pScanData ps )
{
	int result;

	DBG( DBG_LOW, "motorP98GotoShadingPosition()\n" );

	/* Modify Lamp Back to Home step for Scan twice in short time */
	result =  motorP98CheckSensorInHome( ps );

	if( _OK != result )
		return _FALSE;

    MotorSetConstantMove( ps, 0 );		/* clear scan states */

    IOCmdRegisterToScanner( ps, ps->RegModelControl,
							ps->AsicReg.RD_ModelControl );

	ps->Scan.fMotorBackward = _FALSE;	/* forward */
    ps->bExtraMotorCtrl     = 0;

    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {

		ps->bFastMoveFlag = _FastMove_Low_C75_G150;
		MotorP98GoFullStep( ps, 0x40 );

		ps->bFastMoveFlag = _FastMove_Middle_C75_G150;
		MotorP98GoFullStep( ps, ps->Device.dwModelOriginY );
    }

	memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
    IOSetToMotorRegister( ps );

    return _TRUE;
}

/*.............................................................................
 * round to the next physical available value
 */
static void motorP98SetMaxDpiAndLength( pScanData ps,
									    pUShort wLengthY, pUShort wBaseDpi )
{
    if (ps->DataInf.xyAppDpi.y > 600)
		*wLengthY = ps->LensInf.rExtentY.wMax * 4 + 200;
    else
		*wLengthY = ps->LensInf.rExtentY.wMax * 2 + 200;

    if ((ps->DataInf.wPhyDataType >= COLOR_TRUE24) &&
							     (ps->DataInf.xyAppDpi.y <= ps->wMinCmpDpi)) {
		*wBaseDpi = ps->wMinCmpDpi;
	} else {
		if ((ps->DataInf.wPhyDataType < COLOR_TRUE24) &&
											(ps->DataInf.xyAppDpi.y <= 75)) {
	    	*wBaseDpi = 75;
		} else {
		    if (ps->DataInf.xyAppDpi.y <= 150) {
				*wBaseDpi = 150;
			} else {
				if (ps->DataInf.xyAppDpi.y <= 300) {
				    *wBaseDpi = 300;
				} else {
				    if (ps->DataInf.xyAppDpi.y <= 600)
						*wBaseDpi = 600;
		    		else
						*wBaseDpi = 1200;
				}
			}
		}
	}

	DBG( DBG_LOW, "wBaseDPI = %u, %u\n", *wBaseDpi, ps->wMinCmpDpi );
}

/*.............................................................................
 *
 */
static void motorP98FillGBColorRunTable( pScanData ps, pUChar pTable,
									     Byte bHi, Byte bLo, UShort wBaseDpi )
{

    if( ps->Device.f0_8_16 ) {

		if (wBaseDpi == ps->wMinCmpDpi) {
		    *pTable |= bHi;
		    *(pTable + 1) |= bLo;
		} else {
			switch (wBaseDpi) {
		    case 150:
				*(pTable + 2) |= bHi;
				*(pTable + 4) |= bLo;
				break;

		    case 300:
				*(pTable + 4) |= bHi;
				*(pTable + 8) |= bLo;
				break;

		    case 600:
				*(pTable + 8) |= bHi;
				*(pTable + 16) |= bLo;
				break;

		    default:
				*(pTable + 16) |= bHi;
				*(pTable + 32) |= bLo;
				break;
			}
		}
    } else {

		if (wBaseDpi == ps->wMinCmpDpi) {
		    *pTable |= bHi;
		    *(pTable + 1) |= bLo;
		} else {
		    switch(wBaseDpi) {

			case 150:
			    *(pTable + 1) |= bHi;
			    *(pTable + 2) |= bLo;
		    	break;

			case 300:
			    *(pTable + 2) |= bHi;
			    *(pTable + 4) |= bLo;
			    break;

			case 600:
			    *(pTable + 4) |= bHi;
			    *(pTable + 8) |= bLo;
		    	break;

			default:
			    *(pTable + 8) |= bHi;
			    *(pTable + 16) |= bLo;
			    break;
		    }
		}
    }
}

/*.............................................................................
 *
 */
static void motorP98SetupRunTable( pScanData ps )
{
	UShort wDpi, w, wBaseDpi, wLengthY;
	pUChar pTable;

	motorP98SetMaxDpiAndLength( ps, &wLengthY, &wBaseDpi );

	/*ClearColorRunTable(); */
    memset( ps->pColorRunTable, 0, ps->BufferForColorRunTable );

    wDpi = wBaseDpi;
    w 	 = wLengthY + 1000;
    pTable = ps->pColorRunTable + (_NUMBER_OF_SCANSTEPS / 4);

    if( ps->DataInf.wPhyDataType >= COLOR_TRUE24) {

		for(; w; w--, pTable++) {
		    if((short)(wDpi -= ps->DataInf.xyPhyDpi.y) <= 0) {
				wDpi += wBaseDpi;
				*pTable |= 0x44;
				motorP98FillGBColorRunTable( ps, pTable, 0x22, 0x11, wBaseDpi );
		    }
		}
    } else {
		for(; w; w--, pTable++) {
		    if((short)(wDpi -= ps->DataInf.xyPhyDpi.y) <= 0) {
				wDpi += wBaseDpi;
				*pTable = 0x22;
		    }
		}
    }
	ps->dwColorRunIndex = 0;
}

/*.............................................................................
 *
 */
static void motorP98UpdateDataCurrentReadLine( pScanData ps )
{
    if(!(ps->Scan.bNowScanState & _SCANSTATE_STOP)) {

		Byte b;

		if (ps->Scan.bNowScanState >= ps->bCurrentLineCount)
		    b = ps->Scan.bNowScanState - ps->bCurrentLineCount;
		else
	    	b = ps->Scan.bNowScanState + _NUMBER_OF_SCANSTEPS - ps->bCurrentLineCount;

		if (b < 40)
		    return;
    }

	ps->SetMotorSpeed( ps, ps->bCurrentSpeed, _TRUE );
    IOSetToMotorRegister( ps );

    ps->Scan.bModuleState = _MotorAdvancing;
}

/*.............................................................................
 *	Byte - Scan State Index (0-63) and StopStep bit
 *	pScanState->bStep - Scan State Index (0-63)
 *	pScanState->bStatus - Scanner Status Register value
 */
static void motorP96GetScanStateAndStatus( pScanData ps, pScanState pScanStep )
{
    ps->OpenScanPath( ps );

    pScanStep->bStep   = IOGetScanState(ps, _TRUE);
	pScanStep->bStep  &= _SCANSTATE_MASK;		/* org was. ~_ScanStateStop; */
    pScanStep->bStatus = IODataFromRegister( ps, ps->RegStatus );

    ps->CloseScanPath( ps );
}

/*.............................................................................
 * Capture the image data and average them.
 */
static Byte motorP96ReadDarkData( pScanData ps )
{
	Byte	 bFifoOffset;
    UShort	 wSum, w;
    TimerDef timer;

	MiscStartTimer( &timer, _SECOND/2);

    do {

		bFifoOffset = IODataRegisterFromScanner( ps, ps->RegFifoOffset );

		/* stepped 1 block */
		if( bFifoOffset ) {

			/* read data */
			IOReadScannerImageData( ps, ps->pScanBuffer1, 512UL);

		    /* 320 = 192 + 128 (128 is size to fetch data) */
		    for (w = 192, wSum = 0; w < 320; w++)
				wSum += (UShort)ps->pScanBuffer1[w];/* average data from 	  */
													/* offset 192 and size 128*/
		    return (Byte)(wSum >> 7);				/* divided by 128 		  */
		}

    } while (!MiscCheckTimer(&timer));

    return 0xff;					/* timed-out */
}

/*.............................................................................
 * move the sensor to a specific Y-position
 */
static void motorP96PositionYProc( pScanData ps, ULong dwStates )
{
	ScanState sState;

	memset( ps->pColorRunTable, 1, dwStates );

#ifdef DEBUG
	if( dwStates > 800UL )
		DBG( DBG_HIGH, "!!!!! RUNTABLE OVERFLOW !!!!!\n" );
#endif
	memset( ps->pColorRunTable + dwStates, 0xff, 800UL - dwStates );

	IOGetCurrentStateCount( ps, &sState );
    ps->bOldStateCount = sState.bStep;

    /* TurnOnMotorAndSetDirection (); */
    if( ps->Scan.fMotorBackward ) {
		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
										(Byte)(ps->IgnorePF | ps->MotorOn));
	} else {
		IOCmdRegisterToScanner( ps, ps->RegMotorControl,
				      (Byte)(ps->IgnorePF | ps->MotorOn | _MotorDirForward));
	}

    ps->pScanState = ps->pColorRunTable;
    do {
		ps->FillRunNewAdrPointer( ps );

    } while (!motorCheckMotorPresetLength( ps ));
}

/*.............................................................................
 * move the sensor to the scan-start position
 */
static void motorP96WaitForPositionY( pScanData ps )
{
/* scheint OKAY zu sein fuer OP4830 */
#ifdef _A3I_EN
#warning "compiling for A3I"
	ULong     dw;
	ScanState sState;

	memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

    ps->Asic96Reg.RD_MotorControl = ps->IgnorePF|ps->MotorOn|_MotorDirForward;
    ps->Scan.fMotorBackward = _FALSE;
    ps->bExtraMotorCtrl     = ps->IgnorePF;

    if( ps->DataInf.xyAppDpi.y <= ps->PhysicalDpi )
		dw = 30UL;
    else
		dw = 46UL;

    dw = (dw + ps->DataInf.crImage.y) * 4 / 3;

    if( ps->DataInf.wPhyDataType == COLOR_TRUE24 )
		dw += 99; /* dwStepsForColor; */

    else if( ps->DataInf.wPhyDataType == COLOR_256GRAY )
	    dw += 99; /* dwStepsForGray; */
	else
	    dw += 99; /* dwStepsForBW; */

    if( dw >= 130UL ) {

		dw -= 100UL;
		dw <<= 1;
	    /* GoFullStep (dw); */

		memset( ps->pColorRunTable, 1, dw );
		memset( ps->pColorRunTable + dw, 0xff, ps->BufferForColorRunTable - dw );

		IOGetCurrentStateCount( ps, &sState );
		ps->bOldStateCount = sState.bStep;

		/* AdjustMotorTime () */
		IOCmdRegisterToScanner( ps, ps->RegLineControl, 31 );

		/* SetRunHalfStep () */
		if( ps->Scan.fMotorBackward )
		    IOCmdRegisterToScanner( ps, ps->RegMotorControl, _Motor1FullStep |
								    ps->IgnorePF | ps->MotorOn );
		else
		    IOCmdRegisterToScanner( ps, ps->RegMotorControl, ps->IgnorePF |
										    ps->MotorOn | _MotorDirForward );

		ps->pScanState = ps->pColorRunTable;

		do {
			ps->FillRunNewAdrPointer( ps );

		} while (!motorCheckMotorPresetLength(ps));

		/* RestoreMotorTime () */
		IOCmdRegisterToScanner( ps, ps->RegLineControl,
								ps->AsicReg.RD_LineControl );

		dw = 100UL;
    }

    if( ps->DataInf.wPhyDataType != COLOR_TRUE24 )
		dw += 20;

	motorP96PositionYProc( ps, dw );

#else

	ULong	  dw;
    ScanState sState;

	TimerDef  timer;

	MiscStartTimer( &timer, _SECOND / 4);
    while (!MiscCheckTimer( &timer ));

	memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );

    ps->Asic96Reg.RD_MotorControl = ps->IgnorePF|ps->MotorOn|_MotorDirForward;
    ps->Scan.fMotorBackward = _FALSE;
    ps->bExtraMotorCtrl     = ps->IgnorePF;

    if ((ps->DataInf.wPhyDataType >= COLOR_TRUE24) ||
											(ps->DataInf.xyAppDpi.y <= 300)) {
		dw = 6UL;

    } else {

		if (ps->DataInf.xyAppDpi.y <= 600) {
			/* 50 is from 6/300 */
		    dw = (ULong)ps->DataInf.xyAppDpi.y / 50UL + 3UL;
		} else
		    dw = 15;	/* 6UL * 600UL / 300UL + 3; */
    }

    dw += ps->DataInf.crImage.y;

    if (dw >= 180UL) {

		dw -= 180UL;
    	/* GoFullStep (ps, dw);----------------------------------------------*/
		memset( ps->pColorRunTable, 1, dw );
#ifdef DEBUG
		if( dw > 8000UL )
			DBG( DBG_HIGH, "!!!!! RUNTABLE OVERFLOW !!!!!\n" );
#endif
		memset( ps->pColorRunTable + dw, 0xff, 8000UL - dw );

		IOGetCurrentStateCount( ps, &sState );
		ps->bOldStateCount = sState.bStep;

		/* SetRunFullStep (ps) */
		if( ps->Scan.fMotorBackward ) {
		    IOCmdRegisterToScanner( ps, ps->RegMotorControl,
						  (Byte)(ps->FullStep | ps->IgnorePF | ps->MotorOn));
		} else {
		    IOCmdRegisterToScanner( ps, ps->RegMotorControl,
						  (Byte)(ps->FullStep | ps->IgnorePF | ps->MotorOn |
															_MotorDirForward));
		}

		ps->pScanState = ps->pColorRunTable;

		do {
			ps->FillRunNewAdrPointer (ps);

		} while (!motorCheckMotorPresetLength(ps));

		/*-------------------------------------------------------------------*/

		dw = 180UL;
    }

	if (ps->DataInf.wPhyDataType != COLOR_TRUE24)
		dw = dw * 2 + 16;
    else
		dw *= 2;

	motorP96PositionYProc( ps, dw );
#endif
}

/*.............................................................................
 * Position Scan Module to specified line number (Forward or Backward & wait
 * for paper flag ON)
 */
static void motorP96ConstantMoveProc1( pScanData ps, ULong dwLines )
{
	Byte	  bRemainder, bLastState;
    UShort	  wQuotient;
    ULong	  dwDelayMaxTime;
    ScanState StateStatus;
	TimerDef  timer;
	Bool	  fTimeout = _FALSE;

	/* state cycles */
    wQuotient  = (UShort)(dwLines / _NUMBER_OF_SCANSTEPS);
    bRemainder = (Byte)(dwLines % _NUMBER_OF_SCANSTEPS);

	/* 3.3 ms per line */
#ifdef _A3I_EN
    dwDelayMaxTime = dwLines * _MOTOR_ONE_LINE_TIME + _SECOND * 2;
#else
    dwDelayMaxTime = dwLines * _MOTOR_ONE_LINE_TIME + _SECOND * 20;
#endif

	/* step every time */
	MotorSetConstantMove( ps, 1 );

    ps->OpenScanPath( ps );

    ps->AsicReg.RD_ModeControl = _ModeScan;
    IODataToRegister( ps, ps->RegModeControl, _ModeScan );

    ps->Asic96Reg.RD_MotorControl = ps->MotorFreeRun |
											ps->MotorOn | _MotorDirForward;
	IODataToRegister( ps, ps->RegMotorControl, ps->Asic96Reg.RD_MotorControl );

    ps->CloseScanPath( ps );

    bLastState = 0;

	MiscStartTimer( &timer, dwDelayMaxTime );

    do {

		/* GetStatusAndScanStateAddr () */
		motorP96GetScanStateAndStatus( ps, &StateStatus );
		if (StateStatus.bStatus & _FLAG_P96_PAPER ) {
		    if (wQuotient)  {

				if (StateStatus.bStep != bLastState) {
				    bLastState = StateStatus.bStep;

				    if (!bLastState)
						wQuotient--;
				}
		    } else
				if (StateStatus.bStep >= bRemainder)
				    break;
			} else
			    break;
    } while (!(fTimeout = MiscCheckTimer( &timer )));

    if (!fTimeout) {
		memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
		IOSetToMotorRegister( ps );
    }
}

/*.............................................................................
 * PreMove/EndMove
 * PreMove is only in ADF and CFB mode
 * In ADF version, there is a distance gap between Paper flag and Real initial
 *   position and it need premove to real initial position and turn the motor
 *   Inverse and start Fast move to start scan position
 * In CFB version , PreMove 1 mm to avoid bouncing of PaperFlag sensor then
 *   fast move
 * In CIS and CSP although there have not PreMove but there have End move
 *   when paper out there still have several mm need to read,
 *   So it need EndMove 2mm and set Inverse Paper
 */
static Bool motorP96GotoShadingPosition( pScanData ps )
{
	DBG( DBG_LOW, "motorP96GotoShadingPosition()\n" );

	MotorSetConstantMove( ps, 0 );		/* clear scan states */
    ps->Scan.fMotorBackward = _FALSE;	/* forward			 */
    ps->bExtraMotorCtrl     = ps->IgnorePF;

	MotorP96ConstantMoveProc( ps, 15 * 12 ); 	/* forward 180 lines */

    if (IODataRegisterFromScanner(ps, ps->RegStatus) & _FLAG_P96_PAPER ) {
		ps->Asic96Reg.RD_MotorControl = 0;
		IOCmdRegisterToScanner( ps, ps->RegMotorControl, 0 );

		DBG( DBG_LOW, "motorP96GotoShadingPosition() failed\n" );
		return _FALSE;
    }
    ps->Scan.fMotorBackward = _TRUE;	/* backward					 */
    ps->bExtraMotorCtrl     = 0;	    /* no extra action for motor */

	/* backward a few thousand lines to touch sensor */
 	MotorP96ConstantMoveProc( ps, ps->BackwardSteps );

	_DODELAY( 250 );

	IOCmdRegisterToScanner( ps, ps->RegModelControl,
					  (Byte)(ps->AsicReg.RD_ModelControl | _ModelInvertPF));

    ps->Scan.fMotorBackward = _FALSE;				/* forward 			*/
    motorP96ConstantMoveProc1( ps, 14 * 12 * 2);	/* ahead 336 lines	*/

	if( MODEL_OP_A3I == ps->sCaps.Model ) {

		motorP96PositionYProc( ps, 80 );

	} else {
		/* forward 24 + pScanData->wOverBlue lines */
    	if (!ps->fColorMoreRedFlag)
			motorP96PositionYProc( ps, ps->wOverBlue + 12 * 2);
	}

    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {
		ps->Scan.fMotorBackward = _FALSE;
		ps->bExtraMotorCtrl     = ps->IgnorePF;
		MotorP96ConstantMoveProc( ps, 1200 );
    }

	IOCmdRegisterToScanner( ps, ps->RegModelControl,
												ps->AsicReg.RD_ModelControl );
    return _TRUE;
}

/*.............................................................................
 *
 */
static void motorP96FillHalfStepTable( pScanData ps )
{
#ifdef _A3I_EN
	if ( ps->Scan.bModuleState == _MotorInStopState ) {

		/* clear the table and get the step value */
		memset( a_bHalfStepTable, 0, _NUMBER_OF_SCANSTEPS );
		ps->bMotorStepTableNo = a_bMotorDown2Table[(ps->bMotorSpeedData-1)/2];
    }

	/* the fastest stepping */
    if( ps->bMotorSpeedData & 1 ) {

		memset( a_bHalfStepTable,
		       ((ps->Scan.bModuleState != _MotorInStopState) ? 1 : 0),
														_NUMBER_OF_SCANSTEPS );
	} else {

		pUChar   pbHalfStepTbl;
		Byte     bHalfSteps;
		pUShort	 pwMoveStep;
		DataType Data;

		bHalfSteps    = ps->bMotorSpeedData / 2;
		pwMoveStep    = &a_wMoveStepTable[ps->bCurrentLineCount];
		pbHalfStepTbl = &a_bHalfStepTable[ps->bCurrentLineCount];

		if( ps->DataInf.wAppDataType == COLOR_TRUE24)
		    Data.dwValue = _NUMBER_OF_SCANSTEPS - 1;
		else
		    Data.dwValue = _NUMBER_OF_SCANSTEPS;

		/* FillDataToHalfStepTable */
		for(; Data.dwValue; Data.dwValue-- ) {

		    if( *pwMoveStep ) {		/* need to exposure */

				if( Data.bValue >= bHalfSteps ) {

				    pUChar pb = pbHalfStepTbl + bHalfSteps;

				    /* AdjustHalfStepStart */
				    if( ps->DataInf.wAppDataType == COLOR_TRUE24 ) {

						if (bHalfSteps >= 2)
						    pb -= (bHalfSteps - 1);
				    }
				    if( pb >= pbEndHalfStepTable )
						pb -= _NUMBER_OF_SCANSTEPS;

				    if( wP96BaseDpi <= ps->PhysicalDpi && *pwMoveStep != 2 )
						*pb = 1;

		    		pb += bHalfSteps;

				    if( pb >= pbEndHalfStepTable )
						pb -= _NUMBER_OF_SCANSTEPS;

				    *pb = 1;
				}
				else
				    *pwMoveStep = 0;		/* idle state */
		    }
		    if( ++pwMoveStep >= pwEndMoveStepTable ) {
				pwMoveStep = a_wMoveStepTable;

				pbHalfStepTbl = a_bHalfStepTable;
		    }
		    else
				pbHalfStepTbl++;
		}
    }

#else

#ifdef DEBUG
	if( 0 == wP96BaseDpi )
		DBG( DBG_HIGH, "!!!! WARNING - motorP96FillHalfStepTable(), "
					   "wP96BaseDpi == 0 !!!!\n" );
#endif

	if ( ps->Scan.bModuleState == _MotorInStopState ) {

		/* clear the table and get the step value */
		memset( a_bHalfStepTable, 0, _NUMBER_OF_SCANSTEPS );
		ps->bMotorStepTableNo = a_bMotorDown2Table[(ps->bMotorSpeedData-1)/2];
    }

	/* the fastest stepping */
    if( ps->bMotorSpeedData & 1 ) {

		memset( a_bHalfStepTable,
		       ((ps->Scan.bModuleState != _MotorInStopState) ? 1 : 0),
														_NUMBER_OF_SCANSTEPS );
	} else {

		pUChar   pbHalfStepTbl, pbHalfStepContent;
		pUShort	 pwMoveStep;
		DataType Data;

		pbHalfStepContent = a_pbHalfStepTables[ps->bMotorSpeedData / 2 - 1];

		pwMoveStep    = &a_wMoveStepTable[ps->bCurrentLineCount];
		pbHalfStepTbl = &a_bHalfStepTable[ps->bCurrentLineCount];

		if (ps->DataInf.wAppDataType == COLOR_TRUE24)
		    Data.dwValue = _NUMBER_OF_SCANSTEPS - 1;
		else
		    Data.dwValue = _NUMBER_OF_SCANSTEPS;

		/* FillDataToHalfStepTable */
		for (; Data.dwValue; Data.dwValue--) {

		    if (*pwMoveStep) {		/* need to exposure */

				if (Data.bValue >= *pbHalfStepContent) {

				    pUChar pb = pbHalfStepTbl + *pbHalfStepContent;

					if (pb >= pbEndHalfStepTable)
						pb -= _NUMBER_OF_SCANSTEPS;

				    /* JudgeStep1 () */
				    if ((wP96BaseDpi != 600) && (*pwMoveStep != 2)) {
						if (ps->Scan.bModuleState != _MotorInStopState) {
						    *pb = 1;
						} else {
						    if (ps->bMotorStepTableNo) {
								ps->bMotorStepTableNo--;
								*pb = 1;
			    			}
						}
					}

				    pb += *pbHalfStepContent;
				    if (pb >= pbEndHalfStepTable)
						pb -= _NUMBER_OF_SCANSTEPS;

				    /* JudgeStep2 () */
				    if (ps->Scan.bModuleState == _MotorInStopState) {
						if (ps->bMotorStepTableNo) {
						    ps->bMotorStepTableNo--;
						    *pb = 1;
						}
				    } else {
						*pb = 1;
					}

					pbHalfStepContent++;
				} else {
				    *pwMoveStep = 0;		/* idle state */
				}
			}

		    if (++pwMoveStep >= pwEndMoveStepTable) {
				pwMoveStep = a_wMoveStepTable;
				pbHalfStepTbl = a_bHalfStepTable;
		    } else {
				pbHalfStepTbl++;
			}
		}
    }
#endif
}

/*.............................................................................
 *
 */
static void motorP96FillDataToColorTable( pScanData ps,
										  Byte bIndex, ULong dwSteps)
{
    Byte	 bColor, bColors;
	pUChar	 pb, pb1;
	pUShort	 pw;
    DataType Data;

    for (pb = &a_bColorByteTable[bIndex],
		 pw = &a_wMoveStepTable[bIndex]; dwSteps; dwSteps--) {

		if (*pw) {				/* valid state */

			if( *pw >= ps->BufferForColorRunTable ) {
				DBG( DBG_LOW, "*pw = %u > %u !!\n",
						*pw, ps->BufferForColorRunTable );
			} else {

			    bColor  = ps->pColorRunTable [*pw];	/* get the colors	*/
			    bColors = a_bColorsSum [bColor & 7];/* number of colors */

			    if (bColors) {						/* need to read data */
					if (dwSteps >= bColors) { 		/* enough room 		 */

		    			/* separate the colors to byte */
					    pb1 = pb;
				    	if (bColor & ps->b1stColor) {

							*pb1 = ps->b1stColorByte;
							if (++pb1 >= pbEndColorByteTable)
							    pb1 = a_bColorByteTable;
					    }

					    if (bColor & ps->b2ndColor) {

							*pb1 = ps->b2ndColorByte;
							if (++pb1 >= pbEndColorByteTable)
							    pb1 = a_bColorByteTable;
					    }

				    	if (bColor & ps->b3rdColor)
							*pb1 = ps->b3rdColorByte;
					} else
					    *pw = 0;
	    		}
			}
		}

		if (++pw >= pwEndMoveStepTable) {
		    pw = a_wMoveStepTable;
		    pb = a_bColorByteTable;
		} else
		    pb++;
    }

/*     ps->bOldSpeed = ps->bMotorRunStatus; non functional */

    /* ToCondense, CondenseColorByteTable */
    for (dwSteps = _SCANSTATE_BYTES, pw = (pUShort)a_bColorByteTable,
		 pb = ps->a_nbNewAdrPointer; dwSteps; dwSteps--, pw++, pb++) {

		Data.wValue = *pw & 0x0303;
		*pb = Data.wOverlap.b1st | (Data.wOverlap.b2nd << 4);
    }

    /* ToCondenseMotor */
    for (dwSteps = _SCANSTATE_BYTES, pb1 = a_bHalfStepTable,
		 pb = ps->a_nbNewAdrPointer; dwSteps; dwSteps--, pb1++, pb++) {

		if (*pb1++)
		    *pb |= 4;

		if (*pb1)
		    *pb |= 0x40;
    }
}

/*.............................................................................
 *
 */
static void motorP96FillBackColorDataTable( pScanData ps )
{
	Byte	bIndex;
	ULong	dw;

    if ((ps->bCurrentLineCount + ps->bNewCurrentLineCountGap + 1) >=
    													_NUMBER_OF_SCANSTEPS){
		bIndex = ps->bCurrentLineCount + ps->bNewCurrentLineCountGap + 1 -
				 _NUMBER_OF_SCANSTEPS;
	} else {
		bIndex = ps->bCurrentLineCount + ps->bNewCurrentLineCountGap + 1;
	}
    dw = _NUMBER_OF_SCANSTEPS - ps->bNewCurrentLineCountGap;

    motorP96FillDataToColorTable( ps, bIndex, dw );
}

/*.............................................................................
 * i/p:
 *    pScanStep = pScanData->pColorRunTable if forward, a_bScanStateTable if backward
 *    dwState = how many states is requested to process
 * NOTE:
 *    The content of pScanStep contain:
 *    0: Idle state
 *    0xff: End mark
 *    others: The motor speed value
 */
static void motorP96FillBackLoop( pScanData ps,
								  pUChar pScanStep, ULong dwStates )
{
    for (; dwStates; dwStates--) {

		if (*pScanStep == 0xff)
		    break;					/* end of states */

		if (*pScanStep) {
		    if (*pScanStep == 1) {	/* speed == 1, this state has to step */

				if (ps->dwScanStateCount & 1)
				    ps->a_nbNewAdrPointer[ps->dwScanStateCount / 2] |= 0x40;
				else
				    ps->a_nbNewAdrPointer[ps->dwScanStateCount / 2] |= 0x04;
			}

		    *pScanStep -= 1;		/* speed decrease by 1 */

		    if (!(*pScanStep))
				pScanStep++;		/* state processed */
		} else
		    pScanStep++;			/* skip this state */

		if (++ps->dwScanStateCount == _NUMBER_OF_SCANSTEPS)
		    ps->dwScanStateCount = 0;					/* reset to begin */
	}

    if (*pScanStep != 0xff)
		ps->fFullLength = _FALSE;
    else
		ps->fFullLength = _TRUE;

    IOSetToMotorStepCount( ps );		/* put all scan states to ASIC */
}

/*.............................................................................
 * 1) Clear scan states
 * 2) Adjust the new scan state
 */
static void motorP96FillRunNewAdrPointer( pScanData ps )
{
	ScanState sState;

	memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES);

	IOGetCurrentStateCount( ps, &sState );

    if (sState.bStep < ps->bOldStateCount )
		sState.bStep += _NUMBER_OF_SCANSTEPS;/* over table (table just can   */
											 /* holds 64 step, then reset to */
											 /* 0, so if less than means over*/
											 /* the table)					*/

	sState.bStep -= ps->bOldStateCount;		 /* how many states passed */
    ps->pScanState += sState.bStep;

    /*
	 * if current state != no stepped or stepped a cycle, fill the table with
	 * 1 in NOT STEPPED length. (1 means to this state has to be processing).
	 */
    if (sState.bStep && sState.bStep != (_NUMBER_OF_SCANSTEPS - 1))
		memset( ps->pScanState, 1, _NUMBER_OF_SCANSTEPS - sState.bStep - 1 );

	IOGetCurrentStateCount( ps, &sState);
    ps->bOldStateCount   = sState.bStep;			/* update current state */
    ps->dwScanStateCount = (ULong)((sState.bStep + 1) & (_NUMBER_OF_SCANSTEPS - 1));

	/* fill begin at next step */
 	motorP96FillBackLoop( ps, ps->pScanState, (_NUMBER_OF_SCANSTEPS - 1));
}

/*.............................................................................
 *
 */
static void motorP96SetupRunTable( pScanData ps )
{
	Short		siSum;
	UShort		wLoop;
	UShort		wLengthY;
	DataPointer	p;

	DBG( DBG_LOW, "motorP96SetupRunTable()\n" );

    /* SetMaxDpiAndLength (ps) */
#ifdef _A3I_EN
    if( ps->DataInf.xyPhyDpi.y > ps->PhysicalDpi ) {

		wLengthY    = 6800 * 2;
		wP96BaseDpi = ps->PhysicalDpi *2;
    } else {
		wLengthY    = 6800;
		wP96BaseDpi = ps->LensInf.rDpiY.wPhyMax >> 1;
    }
#else
    if( ps->DataInf.xyPhyDpi.y > (ps->LensInf.rDpiY.wPhyMax / 2)) {

		wLengthY    = ps->LensInf.rExtentY.wMax << 1;
		wP96BaseDpi = ps->LensInf.rDpiY.wPhyMax;
    } else {
		wLengthY    = ps->LensInf.rExtentY.wMax;
		wP96BaseDpi = ps->LensInf.rDpiY.wPhyMax >> 1;
    }
#endif

	DBG( DBG_LOW, "wLengthY = %u, wP96BaseDpi = %u\n", wLengthY, wP96BaseDpi );

    /* ClearColorRunTable (ps) */
	memset( ps->pColorRunTable, 0, ps->BufferForColorRunTable ); /*wLengthY + 0x60 ); */

    p.pb = ps->pColorRunTable + _SCANSTATE_BYTES;
#ifdef _A3I_EN
    wLoop = wLengthY + 200;
#else
    wLoop = wLengthY + 0x20;
#endif
    siSum = (Short)wP96BaseDpi;

    if (ps->DataInf.wPhyDataType != COLOR_TRUE24) {
		for (; wLoop; wLoop--, p.pb++)	{
		    if ((siSum -= (Short)ps->DataInf.xyPhyDpi.y) <= 0) {
				siSum += (Short)wP96BaseDpi;
				*p.pb = _COLORRUNTABLE_GREEN;
	    	}
		}

#ifdef _A3I_EN
		memset( ps->pColorRunTable + _NUMBER_OF_SCANSTEPS / 8 + wLengthY,
																0x77, 0x100 );
#endif
    } else {
		/* CalColorRunTable */
		DataType Data;

		if (ps->fSonyCCD) {

			if((ps->sCaps.Model == MODEL_OP_12000P) ||
                                           (ps->sCaps.Model == MODEL_OP_A3I)) {
				Data.wValue = (_COLORRUNTABLE_RED << 8) | _COLORRUNTABLE_BLUE;
			} else {
				Data.wValue = (_COLORRUNTABLE_GREEN << 8) | _COLORRUNTABLE_BLUE;
			}
		} else {
			Data.wValue = (_COLORRUNTABLE_BLUE << 8) | _COLORRUNTABLE_GREEN;
		}

		for (; wLoop; wLoop--, p.pb++)	{

		    if ((siSum -= (Short)ps->DataInf.xyPhyDpi.y) <= 0) {
				siSum += (Short)wP96BaseDpi;

    			if((ps->sCaps.Model == MODEL_OP_12000P)|| 
                                           (ps->sCaps.Model == MODEL_OP_A3I)) {
					*p.pb |= _COLORRUNTABLE_GREEN;
                } else {
					*p.pb |= _COLORRUNTABLE_RED;
                }

				/* Sony:Green,Toshiba:Blue */
				*(p.pb + 8)  |= Data.wOverlap.b2nd;
				*(p.pb + 16) |= Data.wOverlap.b1st;
		    }
		}

#ifdef _A3I_EN
		memset( ps->pColorRunTable + _NUMBER_OF_SCANSTEPS / 8 + wLengthY,
																0x77, 0x100 );
#endif
		if (ps->DataInf.xyPhyDpi.y < 100) {

		    Byte bColor;

		    /* CheckColorTable () */
		    if (ps->fSonyCCD)
				Data.wValue = 0xdd22;
		    else
				Data.wValue = 0xbb44;

		    for (wLoop = wLengthY - _SCANSTATE_BYTES,
				 p.pb = ps->pColorRunTable + _SCANSTATE_BYTES;
												 wLoop; wLoop--, p.pb++) {
				bColor = 0;

				switch (a_bColorsSum[*p.pb & 0x0f]) {

			    case 3:
					if (*(p.pb + 2))
					    bColor = 1;
			    case 2:
					if (*(p.pb + 1))
			    		bColor++;
					if (bColor == 2) {
					    *p.pb &= ~_COLORRUNTABLE_RED;
					    *(p.pb - 2) = _COLORRUNTABLE_RED;
					}

					if (bColor) {
				    	if (*p.pb & ps->RedDataReady) {
							*p.pb &= ~_COLORRUNTABLE_RED;
							*(p.pb - 1) = _COLORRUNTABLE_RED;
					    } else {
							*p.pb &= Data.wOverlap.b2nd;
							*(p.pb - 1) = Data.wOverlap.b1st;
				    	}
					}
				}
	    	}
		}
	}
}

/*.............................................................................
 *
 */
static void motorP96UpdateDataCurrentReadLine( pScanData ps )
{
	ScanState	State1, State2;
	TimerDef	timer;

	IOGetCurrentStateCount( ps, &State1 );
	IOGetCurrentStateCount( ps, &State2 );

    if (State1.bStatus == State2.bStatus) {

		if (!(State2.bStatus & _SCANSTATE_STOP)) {

		    /* motor still running */
		    if (State2.bStep < ps->bCurrentLineCount) {
				State2.bStep = State2.bStep + _NUMBER_OF_SCANSTEPS -
												       ps->bCurrentLineCount;
			} else
				State2.bStep -= ps->bCurrentLineCount;

		    if (State2.bStep >= (_NUMBER_OF_SCANSTEPS - 3)) {

				MiscStartTimer( &timer, _SECOND );

				do {
				    State2.bStatus = IOGetScanState( ps, _FALSE );

				} while (!(State2.bStatus & _SCANSTATE_STOP) &&
						 !MiscCheckTimer( &timer ));
		    } else
				if (State2.bStep < 40)
				    return;
		}

#ifdef _A3I_EN
		if( ps->bFifoCount >= 140) {
#else
		if( ps->bFifoCount >= 20) {
#endif
		    if( 1 == ps->bCurrentSpeed ) {
				ps->bCurrentSpeed *= 2;
		    } else {
				if( COLOR_TRUE24 == ps->DataInf.wPhyDataType )
		    		ps->bCurrentSpeed += 4;
				else
				    ps->bCurrentSpeed += 2;
			}

	    	MotorP96AdjustCurrentSpeed( ps, ps->bCurrentSpeed );
		}

		/*
		 * when using the full-step speed on 600 dpi models, then set
		 * the motor into half-step mode, to avoid that the scanner hits
		 * the back of its bed
		 */
/* HEINER:A3I */
#if 1
    	if((600 == ps->PhysicalDpi) && (1 == ps->bCurrentSpeed)) {

			if( ps->Asic96Reg.RD_MotorControl & ps->FullStep ) {
				ps->Asic96Reg.RD_MotorControl &= ~ps->FullStep;
    			IOCmdRegisterToScanner( ps, ps->RegMotorControl,
											   ps->Asic96Reg.RD_MotorControl );
			}
		}
#endif
		ps->SetMotorSpeed( ps, ps->bCurrentSpeed, _TRUE );

		IOSetToMotorRegister( ps);
    }
}

/*.............................................................................
 * 1) Save the current scan state
 * 2) Set the motor direction
 */
static void motorGoHalfStep1( pScanData ps )
{
	ScanState sState;

    IOGetCurrentStateCount( ps, &sState );

	ps->bOldStateCount = sState.bStep;

	motorSetRunPositionRegister(ps);
   	ps->pScanState = a_bScanStateTable;

	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

		ps->FillRunNewAdrPointer( ps );

		while (!motorCheckMotorPresetLength(ps))
			motorP98FillRunNewAdrPointer1( ps );
	} else {

	    while (!motorCheckMotorPresetLength( ps ))
			ps->FillRunNewAdrPointer( ps );
	}
}

/*.............................................................................
 * when loosing data, we use this function to go back some lines and read them
 * again...
 */
static void motorP96WaitBack( pScanData ps )
{
	DataPointer	p;
	DataType	Data;
	ULong		dw;
	UShort		w;
	UShort		wStayMaxStep;

    /* FindMaxMoveStepIndex () */

    p.pw = a_wMoveStepTable;

    for( Data.dwValue = _NUMBER_OF_SCANSTEPS, wStayMaxStep = 1; Data.dwValue;
													 Data.dwValue--, p.pw++ )
		if( *p.pw > wStayMaxStep )
		    wStayMaxStep = *p.pw;	/* save the largest step number */

    if( ps->DataInf.xyPhyDpi.y > ps->PhysicalDpi )
		wStayMaxStep -= 40;
    else
		wStayMaxStep -= 20;

    IORegisterDirectToScanner( ps, ps->RegInitDataFifo );

    memset( a_bScanStateTable, 1, _P96_BACKMOVES );
    memset(&a_bScanStateTable[_P96_BACKMOVES], 0xff, 250 - _P96_BACKMOVES );

    ps->Scan.fMotorBackward = _TRUE;
    motorGoHalfStep1( ps );			/* backward 130 lines */

    _DODELAY(200);			/* let the motor stable */

    if( ps->DataInf.xyPhyDpi.y <= ps->PhysicalDpi ) {
		if( ps->DataInf.wPhyDataType == COLOR_TRUE24 ) {
		    dw = _P96_FORWARDMOVES - 1;
		} else {
	    	dw = _P96_FORWARDMOVES - 2;
		}
    } else {
		dw = _P96_FORWARDMOVES;
	}
	
	memset( a_bScanStateTable, 1, dw );
    memset(&a_bScanStateTable[dw], 0xff, 250 - dw );

    ps->Scan.fMotorBackward = _FALSE;
    motorGoHalfStep1( ps );			/* move forward */

    /* GetNowStepTable () */
    ps->bCurrentLineCount = IOGetScanState( ps, _FALSE ) & _SCANSTATE_MASK;
    ps->bNewCurrentLineCountGap = 0;

    /* ClearColorByteTable () */
    memset( a_bColorByteTable, 0, _NUMBER_OF_SCANSTEPS );

    /* ClearHalfStepTable () */
    memset( a_bHalfStepTable, 0, _NUMBER_OF_SCANSTEPS );

    /* FillWaitMoveStepTable () */
    p.pw = &a_wMoveStepTable[((ps->bCurrentLineCount + 1) & 0x3f)];
    *p.pw = 1;
    p.pw++;

    for(w = wStayMaxStep, Data.bValue = ps->bMotorSpeedData, dw = 60;dw;dw--) {

		if( p.pw >= pwEndMoveStepTable ) /* make sure pointer in range */
		    p.pw = a_wMoveStepTable;

		if (--Data.bValue)
		    *p.pw = 0;			    			/* don't step */
		else {
		    Data.bValue = ps->bMotorSpeedData;  /* speed value       		 */
		    *p.pw = w;			    			/* the ptr to pColorRunTable */
		    w++;			    				/* pointer++ 				 */
		}

		p.pw++; 			    				/* to next entry */
    }
	motorP96FillHalfStepTable( ps );
	motorP96FillBackColorDataTable( ps );
}

/*.............................................................................
 * when loosing data, we use this function to go back some lines and read them
 * again...
 */
static void motorP98WaitBack( pScanData ps )
{
	DataPointer	p;
	DataType	Data;
	ULong		dw;
	UShort		w;
	UShort		wStayMaxStep;
	UShort		back, forward;

    p.pw = &a_wMoveStepTable[ps->bCurrentLineCount];

    if (0 == *p.pw) {

		for (w = _NUMBER_OF_SCANSTEPS; w && !*p.pw; w--) {
		    p.pw--;
		    if (p.pw < a_wMoveStepTable)
				p.pw = &a_wMoveStepTable[_NUMBER_OF_SCANSTEPS - 1];
		}
		wStayMaxStep = *p.pw + 1;
    } else {
		wStayMaxStep = *p.pw;	    /* save the largest step number */
	}

	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

		forward = _P98_FORWARDMOVES;
		back	= _P98_BACKMOVES;
	} else {
		forward = _P96_FORWARDMOVES;
		back	= _P96_BACKMOVES;
	}

	/*
	 * Off to avoid the problem of block data re-read/lost
	 */
	memset( a_bScanStateTable, 1, back );
	memset( &a_bScanStateTable[back], 0xff, (_SCANSTATE_TABLE_SIZE - back));
	ps->Scan.fMotorBackward = _TRUE;
    motorGoHalfStep1( ps );

    _DODELAY(200);			/* let the motor stable */

    memset(a_bScanStateTable, 1, forward );
    memset(&a_bScanStateTable[forward], 0xff, (_SCANSTATE_TABLE_SIZE-forward));
	ps->Scan.fMotorBackward = _FALSE;
	motorGoHalfStep1( ps );

    ps->bNewCurrentLineCountGap = 0;

    memset( a_bColorByteTable, 0, _NUMBER_OF_SCANSTEPS );
	memset( a_bHalfStepTable,  0, _NUMBER_OF_SCANSTEPS );

	ps->bCurrentLineCount = (ps->bCurrentLineCount + 1) & 0x3f;

    p.pw = &a_wMoveStepTable[ps->bCurrentLineCount];

    for (w = wStayMaxStep, Data.bValue = ps->bMotorSpeedData,
							    			dw = _NUMBER_OF_SCANSTEPS; dw; dw--) {
		if (--Data.bValue) {
		    *p.pw = 0;			    	/* don't step */
		} else {

			/* speed value */
		    Data.bValue = ps->bMotorSpeedData;
	    	*p.pw = w;	    		    	/* the pointer to pColorRunTable*/
		    w++;			    			/* pointer++                    */
	}
	/* make sure pointer in range */
	if (++p.pw >= pwEndMoveStepTable)
	    p.pw = a_wMoveStepTable;
    }

	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {
		motorP98FillHalfStepTable( ps );
		motorP98FillBackColorDataTable( ps );
	} else {
		motorP96FillHalfStepTable( ps );
		motorP96FillBackColorDataTable( ps );
	}
}

/*.............................................................................
 *
 */
static void motorFillMoveStepTable( pScanData ps,
								    UShort wIndex, Byte bStep, pUShort pw )
{
    UShort w;
    Byte   b;

    if (++pw >= pwEndMoveStepTable )
		pw = a_wMoveStepTable;

    wIndex++;

    b = ps->bMotorSpeedData;

    for (w = _NUMBER_OF_SCANSTEPS - bStep; w; w--) {
		if (b == 1) {
		    b = ps->bMotorSpeedData;
		    *pw = wIndex;
	    	wIndex++;
		} else {
		    b--;
		    *pw = 0;
		}
		if (++pw >= pwEndMoveStepTable)
	    	pw = a_wMoveStepTable;
    }

	if( _ASIC_IS_98001 == ps->sCaps.AsicID )
		motorP98FillHalfStepTable( ps );
	else
		motorP96FillHalfStepTable( ps );

    if ((ps->bCurrentLineCount + 1) >= _NUMBER_OF_SCANSTEPS) {
		b = ps->bCurrentLineCount + 1 - _NUMBER_OF_SCANSTEPS;
    } else {
		b = ps->bCurrentLineCount + 1;
	}

	if( _ASIC_IS_98001 == ps->sCaps.AsicID )
		motorP98FillDataToColorTable( ps, b, _NUMBER_OF_SCANSTEPS - 1);
	else
		motorP96FillDataToColorTable( ps, b, _NUMBER_OF_SCANSTEPS - 1);
}

/*.............................................................................
 *
 */
static void noMotorRunStatusStop( pScanData ps, Byte bScanState )
{
    Byte  	b, b1, bCur;
    pUShort pw;
    pByte	pb;
    UShort  w;

    ps->bCurrentLineCount = (bScanState & _SCANSTATE_MASK);

    ps->Scan.fRefreshState  = _FALSE;

    IORegisterDirectToScanner( ps, ps->RegRefreshScanState );

    bCur = ps->bCurrentLineCount;
    pw 	 = &a_wMoveStepTable[bCur];
    pb 	 = ps->pColorRunTable;
	b    = 0;
    b1 	 = 0;
    w 	 = _NUMBER_OF_SCANSTEPS;

    if (*pw) {
		b = a_bColorsSum[pb [*pw] >> 4];
		if (b) {
		    motorClearColorByteTableLoop0( ps, b );
		    ps->bNewCurrentLineCountGap = b;

	    	motorFillMoveStepTable( ps, *pw, 1, pw );
		    return;
		}
		b1++;
		bCur--;

		if (--pw < a_wMoveStepTable) {
		    pw = &a_wMoveStepTable [_NUMBER_OF_SCANSTEPS - 1];
		    bCur = _NUMBER_OF_SCANSTEPS - 1;
		}
    }

    for(; w; w--) {
		if (*pw) {
		    if (*pw < _SCANSTATE_BYTES) {
				b = 0;
				break;
		    } else
				if ((b = a_bColorsSum [pb [*pw] >> 4]))
				    break;
		}
		b1++;
		bCur--;

		if (--pw < a_wMoveStepTable) {
		    pw = &a_wMoveStepTable [_NUMBER_OF_SCANSTEPS - 1];
	   		bCur = _NUMBER_OF_SCANSTEPS - 1;
		}
	}

	if (b1 == _NUMBER_OF_SCANSTEPS) {
		ps->bNewCurrentLineCountGap = 0;
		ps->bNewGap = 0;
	} else {
		ps->bNewCurrentLineCountGap = b1;
		ps->bNewGap = b;
	}

    motorClearColorByteTableLoop1( ps );

	motorFillMoveStepTable( ps, *pw, 0, pw);
}

/*.............................................................................
 *
 */
static void motorP96SetSpeed( pScanData ps, Byte bSpeed, Bool fSetRunState )
{
#if 0
    PUCHAR	pb;
    Byte	 bScanState;
#endif
	Byte     bState, bData;
	UShort   wMoveStep;
	pUShort  pw;
	ULong    dw;
	TimerDef timer;

    if( fSetRunState )
		ps->Scan.bModuleState = _MotorInNormalState;

    ps->bMotorSpeedData = bSpeed;

    if( ps->bMoveDataOutFlag == _DataAfterRefreshState) {

		ps->bMoveDataOutFlag = _DataInNormalState;

		MiscStartTimer( &timer, (_SECOND /2));

		while (!MiscCheckTimer(&timer)) {
		    if ((bState = IOGetScanState( ps, _FALSE)) & _SCANSTATE_STOP) {
				ps->bCurrentLineCount = bState & ~_SCANSTATE_STOP;
				motorP96WaitBack( ps );
				return;
	    	}
		}
    }

    bState = IOGetScanState( ps, _FALSE );

    if((ps->Scan.bModuleState != _MotorInStopState) ||
												!(bState & _SCANSTATE_STOP)) {

		/* Try to find the available step for all rest steps.
		 * 1) if current step is valid (with data request), fill all steps
		 *    after it
		 * 2) if current step is NULL (for delay purpose), backward search the
		 *    valid entry, then fill all steps after it
		 * 3) if no step is valid, fill all entries
    	 */
		Byte   bColors = 0;
   		UShort w;

		/* NoMotorRunStatusStop () */
		ps->bCurrentLineCount  = (bState &= _SCANSTATE_MASK);
		ps->Scan.fRefreshState = _TRUE;

		IORegisterDirectToScanner( ps, ps->RegRefreshScanState );

		pw     = &a_wMoveStepTable[bState];
		bData  = 0;
		bState = ps->bCurrentLineCount;
		dw     = _NUMBER_OF_SCANSTEPS;

		if( (wMoveStep = *pw) ) {

		    bColors = a_bColorsSum[ ps->pColorRunTable[*pw] / 16];

	    	if( bColors ) {

				motorClearColorByteTableLoop0( ps, bColors );
				ps->bNewCurrentLineCountGap = bColors;
				bColors = 1;
				goto FillMoveStepTable;

		    } else {

				bData++;
				dw--;
				if( --pw < a_wMoveStepTable ) {
				    pw     = a_wMoveStepTable + _NUMBER_OF_SCANSTEPS - 1;
				    bState = _NUMBER_OF_SCANSTEPS - 1;
				}
				else
				    bState--;
		    }
		}

		/* FindNextStep */
		while( dw-- ) {

		    if( (wMoveStep = *pw) ) {
				if (wMoveStep < (_NUMBER_OF_SCANSTEPS / 2)) {
				    bColors = 0;
				    break;
				}
				if((bColors = a_bColorsSum [ps->pColorRunTable[wMoveStep] / 16]))
				    break;
		    }

		    bData++;
	    	if( --pw < a_wMoveStepTable ) {
				pw     = a_wMoveStepTable + _NUMBER_OF_SCANSTEPS - 1;
				bState = _NUMBER_OF_SCANSTEPS - 1;
		    }
	    	else
				bState--;
		}

		if (bData == _NUMBER_OF_SCANSTEPS )
		    bData = bColors = 0;

		ps->bNewCurrentLineCountGap = bData;
		ps->bNewGap = bColors;
		motorClearColorByteTableLoop1( ps );
		bColors = 0;

		/* use pw (new pointer) and new speed to recreate MoveStepTable
		 * wMoveStep = Index number from a_wMoveStepTable ([esi])
		 * bColors = number of steps should be reserved
		 * pw = where to fill
		 */

FillMoveStepTable:

		motorP96GetStartStopGap( ps, _TRUE );

		if( !ps->bMotorStepTableNo )
		    ps->bMotorStepTableNo = 1;

		if( ps->bMotorStepTableNo != 0xff && ps->IO.portMode == _PORT_SPP &&
													    ps->DataInf.xyPhyDpi.y <= 200) {
	    	ps->bMotorStepTableNo++;
		}

		if (++pw >= pwEndMoveStepTable)
		    pw = a_wMoveStepTable;

		for( dw = _NUMBER_OF_SCANSTEPS - bColors, wMoveStep++,
								     bData = ps->bMotorSpeedData; dw; dw-- ) {
		    if( bData == 1 ) {

				bData = ps->bMotorSpeedData;
				if( ps->bMotorStepTableNo ) {

					ps->bMotorStepTableNo--;
				    w = wMoveStep;
				    wMoveStep++;

				} else {
				    bData--;
				    w = 0;
				}
		    } else {
				bData--;
				w = 0;
		    }
		    *pw = w;

		    if (++pw >= pwEndMoveStepTable)
				pw = a_wMoveStepTable;
		}

		motorP96FillHalfStepTable( ps );

		/* FillColorBytesTable */
		if((ps->bCurrentLineCount + 1) < _NUMBER_OF_SCANSTEPS )
		    bState = ps->bCurrentLineCount + 1;
		else
		    bState = ps->bCurrentLineCount + 1 - _NUMBER_OF_SCANSTEPS;

		motorP96FillDataToColorTable( ps, bState, _NUMBER_OF_SCANSTEPS - 1);
	}
}

/*.............................................................................
 *
 */
static void motorP98SetSpeed( pScanData ps, Byte bSpeed, Bool fSetRunState )
{
	static Byte lastFifoState = 0;

	Bool overflow;
	Byte bOld1ScanState, bData;

    if( fSetRunState )
		ps->Scan.bModuleState = _MotorInNormalState;

    ps->bMotorSpeedData  = bSpeed;
	overflow 			 = _FALSE;

	if( _ASIC_IS_98001 != ps->sCaps.AsicID ) {
		ps->bMoveDataOutFlag = _DataInNormalState;

		bData = IODataRegisterFromScanner( ps, ps->RegFifoOffset );

		if((lastFifoState > _P96_FIFOOVERFLOWTHRESH) &&
													(bData < lastFifoState)) {
			DBG( DBG_HIGH, "FIFO OVERFLOW, loosing data !!\n" );
			overflow = _TRUE;
        }
        lastFifoState = bData;
	}

    bOld1ScanState = IOGetScanState( ps, _FALSE );

    if(!(bOld1ScanState & _SCANSTATE_STOP) && !overflow)
		noMotorRunStatusStop( ps, bOld1ScanState );
    else {
		ps->bCurrentLineCount = (bOld1ScanState & 0x3f);
		ps->Scan.bModuleState = _MotorGoBackward;

		motorP98WaitBack( ps );

		if( overflow )
	        lastFifoState = 0;

		if( _ASIC_IS_98001 != ps->sCaps.AsicID )
			ps->bMoveDataOutFlag = _DataFromStopState;
    }
}

/*.............................................................................
 *
 */
static void motorP98003ModuleFreeRun( pScanData ps, ULong steps )
{
    IODataToRegister( ps, ps->RegMotorFreeRunCount1, (_HIBYTE(steps)));
    IODataToRegister( ps, ps->RegMotorFreeRunCount0, (_LOBYTE(steps)));
    IORegisterToScanner( ps, ps->RegMotorFreeRunTrigger );
}

/*.............................................................................
 *
 */
static void motorP98003ModuleToHome( pScanData ps )
{
    if(!(IODataFromRegister( ps, ps->RegStatus ) & _FLAG_P98_PAPER)) {

    	IODataToRegister( ps, ps->RegMotor0Control,
            			(Byte)(ps->AsicReg.RD_Motor0Control|_MotorDirForward));

    	MotorP98003PositionYProc( ps, 40 );
    	MotorP98003BackToHomeSensor( ps );
	    _DODELAY( 250 );
    }
}

/*.............................................................................
 *
 */
static void motorP98003DownloadNullScanStates( pScanData ps )
{
    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
    IODownloadScanStates( ps );
}

/*.............................................................................
 *
 */
static void motorP98003Force16Steps( pScanData ps )
{
    ULong dw;

    IODataToRegister( ps, ps->RegStepControl, _MOTOR0_ONESTEP);
    IODataToRegister( ps, ps->RegMotor0Control, _FORWARD_MOTOR );

    for(dw = 16; dw; dw--) {
    	IORegisterToScanner( ps, ps->RegForceStep );
    	_DODELAY( 10 );
    }

    IODataToRegister( ps, ps->RegStepControl, _MOTOR0_SCANSTATE );
}

/*.............................................................................
 *
 */
static void motorP98003WaitForPositionY( pScanData ps )
{
    Byte  bXStep;
    ULong dwBeginY;

    dwBeginY = (ULong)ps->DataInf.crImage.y * 4 + ps->Scan.dwScanOrigin;

    if( ps->DataInf.wPhyDataType <= COLOR_256GRAY ) {
    	if( ps->Device.f0_8_16 )
	        dwBeginY += 16;
    	else
	        dwBeginY += 8;
    }

    bXStep = (Byte)((ps->DataInf.wPhyDataType <= COLOR_256GRAY) ?
				      ps->Device.XStepMono : ps->Device.XStepColor);

    if( ps->Shade.bIntermediate & _ScanMode_AverageOut )
    	bXStep = 8;

    motorP98003Force16Steps( ps);
    dwBeginY -= 16;

    if (dwBeginY > (_RFT_SCANNING_ORG + _P98003_YOFFSET) &&
                						  bXStep < ps->AsicReg.RD_XStepTime) {

    	IODataToRegister( ps, ps->RegMotorDriverType, ps->Scan.motorPower );
        _DODELAY( 12 );
    	IODataToRegister( ps, ps->RegXStepTime, bXStep);
	    IODataToRegister( ps, ps->RegExtendedXStep, 0 );
    	IODataToRegister( ps, ps->RegScanControl1,
	            		(UChar)(ps->AsicReg.RD_ScanControl1 & ~_MFRC_RUNSCANSTATE));
    	MotorP98003PositionYProc( ps, dwBeginY - 64 );
	    dwBeginY = 64;
    }

    IODataToRegister( ps, ps->RegFifoFullLength0, _LOBYTE(ps->AsicReg.RD_BufFullSize));
    IODataToRegister( ps, ps->RegFifoFullLength1, _HIBYTE(ps->AsicReg.RD_BufFullSize));
    IODataToRegister( ps, ps->RegFifoFullLength2, _LOBYTE(_HIWORD(ps->AsicReg.RD_BufFullSize)));

    IODataToRegister( ps, ps->RegMotorDriverType, ps->AsicReg.RD_MotorDriverType);
    _DODELAY( 12 );

    if(!ps->Device.f2003 || (ps->Shade.bIntermediate & _ScanMode_AverageOut) ||
		    (  ps->DataInf.xyAppDpi.y <= 75 &&
                                  ps->DataInf.wPhyDataType <= COLOR_256GRAY)) {
	    IODataToRegister( ps, ps->RegMotorDriverType,
                   (Byte)(ps->Scan.motorPower & (_MOTORR_MASK | _MOTORR_STRONG)));
    } else {
    	IODataToRegister( ps, ps->RegMotorDriverType,
                          ps->AsicReg.RD_MotorDriverType );
    }

    IODataToRegister( ps, ps->RegXStepTime,     ps->AsicReg.RD_XStepTime );
    IODataToRegister( ps, ps->RegExtendedXStep, ps->AsicReg.RD_ExtXStepTime );
    IODataToRegister( ps, ps->RegScanControl1,
                    (Byte)(ps->AsicReg.RD_ScanControl1 & ~_MFRC_RUNSCANSTATE));

    if( ps->DataInf.dwVxdFlag & _VF_PREVIEW ) {

        TimerDef timer;

	    motorP98003ModuleFreeRun( ps, dwBeginY );
        _DODELAY( 15 );

    	MiscStartTimer( &timer, (_SECOND * 20));

	    while(( IOGetExtendedStatus( ps ) & _STILL_FREE_RUNNING) &&
                                                      !MiscCheckTimer(&timer));
        	IODataToRegister( ps, ps->RegModeControl, _ModeScan );
    } else {
    	MotorP98003PositionYProc( ps, dwBeginY );
        IORegisterToScanner( ps, ps->RegRefreshScanState );
    }
}

/*.............................................................................
 * move the sensor to the appropriate shading position
 */
static Bool motorP98003GotoShadingPosition( pScanData ps )
{
    motorP98003ModuleToHome( ps );

    /* position to somewhere under the transparency adapter */
    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {

    	MotorP98003ForceToLeaveHomePos( ps );
        motorP98003DownloadNullScanStates( ps );

    	IODataToRegister( ps, ps->RegStepControl, _MOTOR0_SCANSTATE );
	    IODataToRegister( ps, ps->RegModeControl, _ModeScan);
    	IODataToRegister( ps, ps->RegMotor0Control, _FORWARD_MOTOR );
	    IODataToRegister( ps, ps->RegXStepTime, 6);
    	IODataToRegister( ps, ps->RegExtendedXStep, 0);
	    IODataToRegister( ps, ps->RegScanControl1, _MFRC_BY_XSTEP);

    	MotorP98003PositionYProc( ps, _TPA_P98003_SHADINGORG );
    }

    return _TRUE;
}

/*.............................................................................
 * initialize this module and setup the correct function pointer according
 * to the ASIC
 */
static void motorP98003PositionModuleToHome( pScanData ps )
{
    Byte save, saveModel;

    saveModel = ps->AsicReg.RD_ModelControl;

    ps->Scan.fRefreshState = _FALSE;
	motorP98003DownloadNullScanStates( ps );

	_DODELAY( 1000UL / 8UL);

	save = ps->Shade.bIntermediate;

	ps->Shade.bIntermediate = _ScanMode_AverageOut;
	ps->ReInitAsic( ps, _FALSE );
    ps->Shade.bIntermediate = save;

	IODataToRegister( ps, ps->RegModeControl, _ModeScan );
	IORegisterToScanner( ps, ps->RegResetMTSC );
	IODataToRegister( ps, ps->RegScanControl1, 0 );

	IODataToRegister( ps, ps->RegModelControl,
                          ps->Device.ModelCtrl | _ModelDpi300 );

	IODataToRegister( ps, ps->RegLineControl, 80);
	IODataToRegister( ps, ps->RegXStepTime, ps->Device.XStepBack);
	IODataToRegister( ps, ps->RegMotorDriverType, ps->Scan.motorPower);

	_DODELAY( 12 );

	IODataToRegister( ps, ps->RegMotor0Control,
                			(_MotorHHomeStop | _MotorOn | _MotorHQuarterStep |
							 _MotorPowerEnable));
	IODataToRegister( ps, ps->RegStepControl,
                                        (_MOTOR0_SCANSTATE | _MOTOR_FREERUN));

    memset( ps->a_nbNewAdrPointer, 0x88, _SCANSTATE_BYTES );
	IODownloadScanStates( ps );
	IORegisterToScanner( ps, ps->RegRefreshScanState );

    ps->AsicReg.RD_ModelControl = saveModel;
}

/************************ exported functions *********************************/

/*.............................................................................
 * initialize this module and setup the correct function pointer according
 * to the ASIC
 */
_LOC int MotorInitialize( pScanData ps )
{
	DBG( DBG_HIGH, "MotorInitialize()\n" );

	if( NULL == ps )
		return _E_NULLPTR;

	ps->a_wMoveStepTable  = a_wMoveStepTable;
	ps->a_bColorByteTable =	a_bColorByteTable;
	wP96BaseDpi 		  = 0;

	ps->PauseColorMotorRunStates = motorPauseColorMotorRunStates;

	/*
	 * depending on the asic, we set some functions
	 */
	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

		ps->WaitForPositionY	 	  = motorP98WaitForPositionY;
		ps->GotoShadingPosition	 	  = motorP98GotoShadingPosition;
		ps->FillRunNewAdrPointer   	  = motorP98FillRunNewAdrPointer;
		ps->SetupMotorRunTable	      = motorP98SetupRunTable;
		ps->UpdateDataCurrentReadLine = motorP98UpdateDataCurrentReadLine;
		ps->SetMotorSpeed 		 	  = motorP98SetSpeed;

	} else if( _ASIC_IS_98003 == ps->sCaps.AsicID ) {

		ps->WaitForPositionY    = motorP98003WaitForPositionY;
		ps->GotoShadingPosition	= motorP98003GotoShadingPosition;
		ps->SetMotorSpeed 	    = motorP98SetSpeed;

    } else if( _IS_ASIC96(ps->sCaps.AsicID)) {

		ps->WaitForPositionY		  = motorP96WaitForPositionY;
		ps->GotoShadingPosition	 	  = motorP96GotoShadingPosition;
		ps->FillRunNewAdrPointer   	  = motorP96FillRunNewAdrPointer;
		ps->SetupMotorRunTable	      = motorP96SetupRunTable;
		ps->UpdateDataCurrentReadLine = motorP96UpdateDataCurrentReadLine;
		ps->SetMotorSpeed 		 	  = motorP96SetSpeed;

	} else {

		DBG( DBG_HIGH , "NOT SUPPORTED ASIC !!!\n" );
		return _E_NOSUPP;
	}
	return _OK;
}

/*.............................................................................
 *
 */
_LOC void MotorSetConstantMove( pScanData ps, Byte bMovePerStep )
{
	DataPointer p;
    ULong	    dw;

    p.pb = ps->a_nbNewAdrPointer;

    switch( bMovePerStep )
    {
	case 0: 	/* doesn't move at all */
	    for (dw = _NUMBER_OF_SCANSTEPS / 8; dw; dw--, p.pdw++) {

			if( _ASIC_IS_98001 == ps->sCaps.AsicID )
				*p.pdw &= 0x77777777;
			else
				*p.pdw &= 0xbbbbbbbb;
		}
	    break;

	case 1:
	    for (dw = _NUMBER_OF_SCANSTEPS / 8; dw; dw--, p.pdw++) {
			if( _ASIC_IS_98001 == ps->sCaps.AsicID )
				*p.pdw |= 0x88888888;
			else
				*p.pdw |= 0x44444444;
		}
	    break;

	case 2:
	    for (dw = _NUMBER_OF_SCANSTEPS / 8; dw; dw--, p.pdw++) {
			if( _ASIC_IS_98001 == ps->sCaps.AsicID )
				*p.pdw |= 0x80808080;
			else
				*p.pdw |= 0x40404040;
		}
	    break;

	default:
	    {
			Byte bMoves = bMovePerStep;

			for (dw = _SCANSTATE_BYTES; dw; dw--, p.pb++) {
		    	if (!(--bMoves)) {
					if( _ASIC_IS_98001 == ps->sCaps.AsicID )
						*p.pb |= 8;
					else
						*p.pb |= 4;
					bMoves = bMovePerStep;
			    }
			    if (!(--bMoves)) {
					if( _ASIC_IS_98001 == ps->sCaps.AsicID )
						*p.pb |= 0x80;
					else
						*p.pb |= 0x40;
					bMoves = bMovePerStep;
			    }
			}
	    }
    }
	IOSetToMotorRegister( ps );
}

/*.............................................................................
 * function to bring the sensor back home
 */
_LOC void MotorToHomePosition( pScanData ps )
{
    TimerDef    timer;
    ScanState	StateStatus;

	DBG( DBG_HIGH, "Waiting for Sensor to be back in position\n" );
	_DODELAY( 250 );

	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

    	if (!(IODataRegisterFromScanner(ps,ps->RegStatus) & _FLAG_P98_PAPER)){
			ps->GotoShadingPosition( ps );
		}
    } else if( _ASIC_IS_98003 == ps->sCaps.AsicID ) {

        ps->OpenScanPath( ps );

	    if( !(IODataFromRegister( ps, ps->RegStatus ) & _FLAG_P98_PAPER)) {

            motorP98003PositionModuleToHome( ps );

            MiscStartTimer( &timer, _SECOND * 20);
            do {

                if( IODataFromRegister( ps, ps->RegStatus ) & _FLAG_P98_PAPER)
                    break;
            } while( !MiscCheckTimer( &timer));
        }
        ps->CloseScanPath( ps );

	} else {

		if( ps->sCaps.Model >= MODEL_OP_9630P ) {
			if( ps->sCaps.Model == MODEL_OP_A3I )
				IOCmdRegisterToScanner( ps, ps->RegLineControl, 0x34 );
			else
				IOCmdRegisterToScanner( ps, ps->RegLineControl, 0x30 );
		}

		ps->bExtraMotorCtrl     = 0;
    	ps->Scan.fMotorBackward = _FALSE;
		MotorP96ConstantMoveProc( ps, 25 );

	    ps->Scan.fMotorBackward = _TRUE;
		for(;;) {

			motorP96GetScanStateAndStatus( ps, &StateStatus );

			if ( StateStatus.bStatus & _FLAG_P96_PAPER ) {
			    break;
			}

 			MotorP96ConstantMoveProc( ps, 50000 );
		}

    	ps->Scan.fMotorBackward = _FALSE;
		ps->Asic96Reg.RD_MotorControl = 0;
		IOCmdRegisterToScanner( ps, ps->RegMotorControl, 0 );

	    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
		IOSetToMotorRegister( ps );
		_DODELAY(250);

	    ps->Asic96Reg.RD_LedControl = 0;
    	IOCmdRegisterToScanner(ps, ps->RegLedControl, ps->Asic96Reg.RD_LedControl);
	}

	DBG( DBG_HIGH, "- done !\n" );
}

/*.............................................................................
 *
 */
_LOC void MotorP98GoFullStep( pScanData ps, ULong dwStep )
{
	memset( ps->pColorRunTable, 1, dwStep );
    memset( ps->pColorRunTable + dwStep, 0xff, 0x40);

    ps->bOldStateCount = IOGetScanState( ps, _FALSE ) & _SCANSTATE_MASK;
    motorP98SetRunFullStep( ps );

    ps->pScanState = ps->pColorRunTable;
    ps->FillRunNewAdrPointer( ps );

    while(!motorCheckMotorPresetLength( ps ))
		motorP98FillRunNewAdrPointer1( ps );
}

/*.............................................................................
 *
 */
_LOC void MotorP96SetSpeedToStopProc( pScanData ps )
{
	Byte	 bData;
    TimerDef timer;

	MiscStartTimer( &timer, _SECOND);
    while( !MiscCheckTimer( &timer )) {

		bData = IODataRegisterFromScanner( ps, ps->RegFifoOffset );

		if ((bData > ps->bMinReadFifo) && (bData != ps->bFifoCount))
		    break;
    }
    bData = IOGetScanState( ps, _FALSE );

    if (!(bData & _SCANSTATE_STOP)) {

		MiscStartTimer( &timer, (_SECOND / 2));

		while (!MiscCheckTimer( &timer )) {

		    if (IOGetScanState( ps, _FALSE) != bData )
				break;
		}
    }

    ps->Scan.bModuleState = _MotorInStopState;
    ps->SetMotorSpeed( ps, ps->bCurrentSpeed, _FALSE );

	IOSetToMotorRegister( ps );
}

/*.............................................................................
 * Position Scan Module to specified line number (Forward or Backward & wait
 * for paper flag ON)
 */
_LOC void MotorP96ConstantMoveProc( pScanData ps, ULong dwLines )
{
	Byte		bRemainder, bLastState;
    UShort		wQuotient;
    ULong		dwDelayMaxTime;
    ScanState	StateStatus;
	TimerDef	timer;
	Bool		fTimeout = _FALSE;

    wQuotient  = (UShort)(dwLines / _NUMBER_OF_SCANSTEPS);	/* state cycles */
    bRemainder = (Byte)(dwLines % _NUMBER_OF_SCANSTEPS);

	/* 3.3 ms per line */
    dwDelayMaxTime = dwLines * _MOTOR_ONE_LINE_TIME + _SECOND * 2;

	MotorSetConstantMove( ps, 1 );	/* step every time */

    ps->OpenScanPath( ps );

    ps->AsicReg.RD_ModeControl = _ModeScan;
    IODataToRegister( ps, ps->RegModeControl, _ModeScan );

    if( ps->Scan.fMotorBackward ) {
		ps->Asic96Reg.RD_MotorControl = (ps->MotorFreeRun | ps->MotorOn |
										   ps->FullStep | ps->bExtraMotorCtrl);
    } else {
		ps->Asic96Reg.RD_MotorControl = (ps->MotorFreeRun | ps->MotorOn |
									  _MotorDirForward | ps->bExtraMotorCtrl);
	}

	IODataToRegister( ps, ps->RegMotorControl, ps->Asic96Reg.RD_MotorControl );
    ps->CloseScanPath( ps );

    bLastState = 0;

	MiscStartTimer( &timer, dwDelayMaxTime );

    do {

		motorP96GetScanStateAndStatus( ps, &StateStatus );

		if( ps->Scan.fMotorBackward && (StateStatus.bStatus&_FLAG_P96_PAPER)) {
		    break;
		} else {

		    /*
			 * 1) Forward will not reach the sensor.
			 * 2) Backwarding, doesn't reach the sensor
			 */
		    if (wQuotient) {

				/* stepped */
				if (StateStatus.bStep != bLastState) {
				    bLastState = StateStatus.bStep;
				    if (!bLastState)	/* done a cycle! */
						wQuotient--;
				}
		    } else {
				if (StateStatus.bStep >= bRemainder) {
			    	break;
				}
			}
		}

		fTimeout = MiscCheckTimer( &timer );

	} while ( _OK == fTimeout );

    if ( _OK == fTimeout ) {
		memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES );
		IOSetToMotorRegister( ps );
    }
}

/*.............................................................................
 *
 */
_LOC Bool MotorP96AheadToDarkArea( pScanData ps )
{
	Byte	 bDark;
	UShort   wTL;
    UShort 	 wTotalLastLine;
    TimerDef timer;

    ps->fColorMoreRedFlag  = _FALSE;
	ps->fColorMoreBlueFlag = _FALSE;
    ps->wOverBlue = 0;

    /* FillToDarkCounter () */
    memset( ps->a_nbNewAdrPointer, 0x30, _SCANSTATE_BYTES);
    MotorSetConstantMove( ps, 2 );

    /* SetToDarkRegister () */
    ps->AsicReg.RD_ModeControl    = _ModeScan;
    ps->AsicReg.RD_ScanControl    = ps->bLampOn | _SCAN_BYTEMODE;
    ps->Asic96Reg.RD_MotorControl = _MotorDirForward | ps->FullStep;
	ps->AsicReg.RD_ModelControl   = ps->Device.ModelCtrl | _ModelWhiteIs0;

    ps->AsicReg.RD_Dpi = 300;
	wTL = 296;
/*	if( MODEL_OP_A3I == ps->sCaps.Model ) { */
	if( ps->PhysicalDpi > 300 ) {
		wTL = 400;
	    ps->AsicReg.RD_Origin = (UShort)(ps->Offset70 + 64 + 8 + 2048);
	} else {
	    ps->AsicReg.RD_Origin = (UShort)(ps->Offset70 + 64 + 8 + 1024);
	}
    ps->AsicReg.RD_Pixels = 512;

	IOPutOnAllRegisters( ps );

    ps->Asic96Reg.RD_MotorControl = (ps->MotorFreeRun | ps->IgnorePF |
											 ps->MotorOn | _MotorDirForward );

	IOCmdRegisterToScanner( ps, ps->RegMotorControl,
											  ps->Asic96Reg.RD_MotorControl );

	MiscStartTimer( &timer, _SECOND * 2 );
    wTotalLastLine = 0;

#ifdef _A3I_EN
    while( !MiscCheckTimer( &timer )) {

		bDark = motorP96ReadDarkData( ps );

		wTotalLastLine++;
		if((bDark < 0x80) || (wTotalLastLine==wTL)) {

		    IOCmdRegisterToScanner( ps, ps->RegMotorControl, 0 );
	    	return _TRUE;
		}
    }
#else
    while (!MiscCheckTimer( &timer )) {

		bDark = motorP96ReadDarkData( ps );

		wTotalLastLine++;
		if (((ps->sCaps.AsicID == _ASIC_IS_96001) && (bDark > 0x80)) ||
			((ps->sCaps.AsicID != _ASIC_IS_96001) && (bDark < 0x80)) ||
                                            (wTotalLastLine==wTL)) {

		    IOCmdRegisterToScanner( ps, ps->RegModeControl, _ModeProgram );

		    if (wTotalLastLine <= 24)
				ps->fColorMoreRedFlag = _TRUE;
		    else
				if (wTotalLastLine >= 120) {
				    ps->wOverBlue = wTotalLastLine - 80;
				    ps->fColorMoreBlueFlag = _TRUE;
				}

	    	return _TRUE;
		}
    }
#endif

    return _FALSE;	/* already timed out */
}

/*.............................................................................
 * limit the speed settings for 96001/3 based models
 */
_LOC void MotorP96AdjustCurrentSpeed( pScanData ps, Byte bSpeed )
{
    if (bSpeed != 1) {

		if (bSpeed > 34)
		    ps->bCurrentSpeed = 34;
		else
		    ps->bCurrentSpeed = (bSpeed + 1) & 0xfe;
    }
}

/*.............................................................................
 *
 */
_LOC void MotorP98003ForceToLeaveHomePos( pScanData ps )
{
    TimerDef timer;

    IODataToRegister( ps, ps->RegStepControl, _MOTOR0_ONESTEP );
    IODataToRegister( ps, ps->RegMotor0Control, _FORWARD_MOTOR );

	MiscStartTimer( &timer, _SECOND );

    do {
        if( !(IODataFromRegister( ps, ps->RegStatus ) & _FLAG_P98_PAPER))
    	    break;

        IORegisterToScanner( ps, ps->RegForceStep );
    	_DODELAY( 10 );

    } while( _OK == MiscCheckTimer( &timer ));

    IODataToRegister( ps, ps->RegStepControl, _MOTOR0_SCANSTATE );
}

/*.............................................................................
 *
 */
_LOC void MotorP98003BackToHomeSensor( pScanData ps )
{
    TimerDef timer;

	DBG( DBG_HIGH, "MotorP98003BackToHomeSensor()\n" );

    IODataToRegister( ps, ps->RegStepControl, _MOTOR0_SCANSTATE );
    IODataToRegister( ps, ps->RegModeControl, _ModeScan );

    /* stepping every state */
    memset( ps->a_nbNewAdrPointer, 0x88, _SCANSTATE_BYTES );
    IODownloadScanStates( ps );

	MiscStartTimer( &timer, _SECOND * 2 );

    while(!(IOGetScanState( ps, _TRUE ) & _SCANSTATE_STOP) &&
                                                    !MiscCheckTimer( &timer ));

	_DODELAY( 1000UL );

    ps->AsicReg.RD_ModeControl = _ModeScan;

    if (!(ps->DataInf.dwScanFlag & SCANDEF_TPA)) {
        IODataToRegister( ps, ps->RegLineControl, _LOBYTE(ps->Shade.wExposure));
        IODataToRegister( ps, ps->RegXStepTime,   _LOBYTE(ps->Shade.wXStep));

    } else {
    	IODataToRegister( ps, ps->RegLineControl, _DEFAULT_LINESCANTIME );
	    IODataToRegister( ps, ps->RegXStepTime, 6);
    }

    IODataToRegister( ps, ps->RegStepControl,
                                         (_MOTOR_FREERUN | _MOTOR0_SCANSTATE));
    IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl );
    IODataToRegister( ps, ps->RegMotor0Control,
                  	    (_MotorHQuarterStep | _MotorOn | _MotorDirBackward |
                                    	_MotorPowerEnable | _MotorHHomeStop));
    IORegisterToScanner( ps, ps->RegRefreshScanState );

	MiscStartTimer( &timer, _SECOND * 5 );

    do {
        if( IODataFromRegister( ps, ps->RegStatus ) & _FLAG_P98_PAPER )
    	    break;

    	_DODELAY( 55 );

    } while( !MiscCheckTimer( &timer ));

    IODataToRegister( ps, ps->RegLineControl, ps->AsicReg.RD_LineControl);
    IODataToRegister( ps, ps->RegXStepTime, ps->AsicReg.RD_XStepTime);

   	DBG( DBG_HIGH, "LineCtrl=%u, XStepTime=%u\n",
                    ps->AsicReg.RD_LineControl, ps->AsicReg.RD_XStepTime );

    motorP98003DownloadNullScanStates( ps );
}

/*.............................................................................
 *
 */
_LOC void MotorP98003ModuleForwardBackward( pScanData ps )
{
    switch( ps->Scan.bModuleState ) {

	case _MotorInNormalState:
	    ps->Scan.bModuleState = _MotorGoBackward;
	    IODataToRegister( ps, ps->RegScanControl1,
    			   (UChar)(ps->AsicReg.RD_ScanControl1 & ~_MFRC_RUNSCANSTATE));
	    IODataToRegister( ps, ps->RegMotor0Control,
    			    (UChar)(ps->AsicReg.RD_Motor0Control & ~_MotorDirForward));
	    motorP98003ModuleFreeRun( ps, _P98003_BACKSTEPS );
	    MiscStartTimer( &p98003MotorTimer, (15 * _MSECOND));
	    break;

	case _MotorGoBackward:
	    if( MiscCheckTimer(& p98003MotorTimer)) {
    		if (!(IOGetExtendedStatus( ps ) & _STILL_FREE_RUNNING )) {
    		    ps->Scan.bModuleState = _MotorInStopState;
	    	    MiscStartTimer( &p98003MotorTimer, (50 *_MSECOND));
		    }
	    }
	    break;

	case _MotorInStopState:
	    if( MiscCheckTimer(&p98003MotorTimer)) {

    		if( IOReadFifoLength( ps ) < ps->Scan.dwMaxReadFifo ) {
    		    ps->Scan.bModuleState = _MotorAdvancing;
	    	    IODataToRegister( ps, ps->RegScanControl1, ps->AsicReg.RD_ScanControl1);
		        IODataToRegister( ps, ps->RegMotor0Control, ps->AsicReg.RD_Motor0Control);
        	    motorP98003ModuleFreeRun( ps, _P98003_FORWARDSTEPS );
        	    MiscStartTimer( &p98003MotorTimer, (15 * _MSECOND));
		    }
	    }
	    break;

	case _MotorAdvancing:
	    if( MiscCheckTimer(&p98003MotorTimer)) {
    		if( !(IOGetScanState( ps, _TRUE ) & _SCANSTATE_STOP))
	    	    ps->Scan.bModuleState = _MotorInNormalState;
    		else {
    		    if (!(IOGetExtendedStatus( ps ) & _STILL_FREE_RUNNING )) {
        			IORegisterToScanner( ps, ps->RegRefreshScanState );
		        	ps->Scan.bModuleState = _MotorInNormalState;
                }
		    }
		}
        break;
    }
}

/*.............................................................................
 *
 */
_LOC void MotorP98003PositionYProc( pScanData ps, ULong steps)
{
    TimerDef timer;

	DBG( DBG_HIGH, "MotorP98003PositionYProc()\n" );

	MiscStartTimer( &timer, _SECOND * 5 );

    while(!(IOGetScanState( ps, _TRUE ) & _SCANSTATE_STOP) &&
                                                (!MiscCheckTimer( &timer )));

    _DODELAY( 12 );

    motorP98003ModuleFreeRun( ps, steps );

    _DODELAY( 15 );

	MiscStartTimer( &timer, _SECOND * 30 );

    do  {
    	if (!(IOGetExtendedStatus( ps ) & _STILL_FREE_RUNNING) ||
                    	        !(IOGetScanState( ps, _TRUE ) & _SCANSTATE_STOP))
	    break;

    } while( !MiscCheckTimer( &timer ));

	DBG( DBG_HIGH, "MotorP98003PositionYProc() - done\n" );
}

/* END PLUSTEK-PP_MOTOR.C ...................................................*/
