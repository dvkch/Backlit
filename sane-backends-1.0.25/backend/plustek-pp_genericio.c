/* @file plustek-pp_genericio.c
 * @brief all i/o functions
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - moved ioP96ReadScannerImageData and ioP98ReadScannerImageData
 *          into this file
 *        - added SPP-read functions
 * - 0.32 - changes in function ioControlLampOnOff()
 *        - made IOReadingImage a local function -> ioP98ReadingImage()
 *        - rewritten function ioP96ReadScannerImageData()
 *        - moved function IOSetStartStopRegister to p9636.c
 * - 0.33 - added debug messages to IOPutOnAllRegisters
 *        - fixed a bug in ioP96InitialSetCurrentSpeed
 * - 0.34 - no changes
 * - 0.35 - no changes
 * - 0.36 - removed some warning conditions
 * - 0.37 - moved functions IOSPPWrite(), IODataToScanner(), IODataToRegister(),
 *          IODataFromRegister() to io.c
 *        - moved the data read functions to io.c
 *        - renamed IOInitialize to IOFuncInitialize
 * - 0.38 - moved some functions to io.c
 *        - added P12 stuff
 * - 0.39 - no changes
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - changed include names
 * - 0.43 - fixed a problem in ioP96InitialSetCurrentSpeed(), for COLOR_BW
 *          at least, used the setting for A3I
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

/*************************** local vars **************************************/

/* WORK COMMENT THIS */
typedef void (*pFnSpeed_Set)(pScanData);

static ModeTypeVar a_FilmSettings[18] = {
	/* SppNegFilmPos */
	{0xa0, 1782, 96, _QuarterStep, 0, 0},
	{0xb7, 1782, 96, _QuarterStep, 0, 0},
	{0xb7, 1782, 96, _QuarterStep, 0, 0},
	/* BppNegFilmPos */
	{0xa9, 1782, 96, _QuarterStep, 0, 0},
	{0xbf, 1782, 96, _QuarterStep, 0, 0},
	{0xbf, 1782, 96, _QuarterStep, 0, 0},
	/* EppNegFilmPos */
	{0x95, 1782, 96, _QuarterStep, 0, 0},
	{0xa6, 1782, 96, _QuarterStep, 0, 0},
	{0xa6, 1782, 96, _QuarterStep, 0, 0},
	/* SppPosFilmPos */
	{0x50, 1782, 96, _QuarterStep, 0, 0},
	{0x67, 1782, 96, _QuarterStep, 0, 0},
	{0x67, 1782, 96, _QuarterStep, 0, 0},
	/* BppPosFilmPos */
	{0x59, 1782, 96, _QuarterStep, 0, 0},
	{0x6f, 1782, 96, _QuarterStep, 0, 0},
	{0x6f, 1782, 96, _QuarterStep, 0, 0},
	/* EppPosFilmPos */
	{0x45, 1782, 96, _QuarterStep, 0, 0},
	{0x56, 1782, 96, _QuarterStep, 0, 0},
	{0x56, 1782, 96, _QuarterStep, 0, 0}
};

static ModeTypeVar a_BwSettings[12] = {
	{_Home_BE75,   890, 96, _HalfStep,    2, 1},
	{_Home_BE150, 1780, 88, _QuarterStep, 2, 0},
	{_Home_BE300, 3542, 96, _QuarterStep, 2, 0},
	{_Home_BE600, 7070, 96, _QuarterStep, 2, 0},
	{_Home_BB75,   890, 96, _HalfStep,    2, 1},
	{_Home_BB150, 1780, 88, _QuarterStep, 2, 0},
	{_Home_BB300, 3542, 96, _QuarterStep, 2, 0},
	{_Home_BB600, 7070, 96, _QuarterStep, 2, 0},
	{_Home_BS75,   890, 96, _HalfStep,    2, 1},
	{_Home_BS150, 1780, 88, _QuarterStep, 2, 0},
	{_Home_BS300, 3542, 96, _QuarterStep, 2, 0},
	{_Home_BS600, 7070, 96, _QuarterStep, 2, 0}
};

static ModeTypeVar a_GraySettings[12] = {
	{_Home_GE75,   890, 96, _HalfStep,    2, 0},
	{_Home_GE150, 1780, 88, _QuarterStep, 2, 0},
	{_Home_GE300, 3542, 88, _QuarterStep, 2, 0},
	{_Home_GE600, 7070, 88, _QuarterStep, 2, 0},
	{_Home_GB75,   890, 96, _HalfStep,    2, 0},
	{_Home_GB150, 1780, 88, _QuarterStep, 2, 0},
	{_Home_GB300, 3542, 88, _QuarterStep, 2, 0},
	{_Home_GB600, 7070, 88, _QuarterStep, 2, 0},
	{_Home_GS75,   890, 96, _HalfStep,    2, 0},
	{_Home_GS150, 1782, 96, _QuarterStep, 2, 0},
	{_Home_GS300, 3549, 88, _QuarterStep, 2, 0},
	{_Home_GS600, 7070, 88, _QuarterStep, 2, 0}
};

static ModeTypeVar a_ColorSettings[15] = {
	{_Home_CE50,   720,  60, _HalfStep,	   1, 1},
	{_Home_CE100, 1782,  48, _QuarterStep, 1, 0},
	{_Home_CE150, 1782,  88, _QuarterStep, 0, 0},
	{_Home_CE300, 3549,  96, _QuarterStep, 0, 0},
	{_Home_CE600, 7082,  96, _QuarterStep, 0, 0},
	{_Home_CB50,   720, 120, _QuarterStep, 0, 1},
	{_Home_CB100, 1782,  96, _QuarterStep, 0, 0},
	{_Home_CB150, 1782,  96, _QuarterStep, 0, 0},
	{_Home_CB300, 3549,  96, _QuarterStep, 0, 0},
	{_Home_CB600, 7082,  96, _QuarterStep, 0, 0},
	{_Home_CS50,   720, 120, _QuarterStep, 0, 1},
	{_Home_CS100, 1782,  96, _QuarterStep, 0, 0},
	{_Home_CS150, 1782,  96, _QuarterStep, 0, 0},
	{_Home_CS300, 3549,  96, _QuarterStep, 0, 0},
	{_Home_CS600, 7082,  96, _QuarterStep, 0, 0}
};

static DiffModeVar a_tabDiffParam[] ={
	/* BPP/EPP B/W */
	{0, 1, 11}, /* Bpp/Epp B/W, Dpi <= 150		 ;(0) */
	{0, 1, 24}, /* Bpp/Epp B/W, Dpi <= 300		 ;(1) */
	{0, 1, 48}, /* Bpp/Epp B/W, Dpi > 300		 ;(2) */
   	/* SPP B/W */
	{0, 1, 11}, /* Spp B/W, Dpi <= 150		 ;(3) */
	{0, 1, 24}, /* Spp B/W, Dpi <= 300		 ;(4) */
    {0, 1, 48}, /* Spp B/W, Dpi > 300		 ;(5) */
	/* EPP Gray */
	/* The difference for this DPI:
     * if pixels <=        | 3000 | Others
     * --------------------+------+-------------
     *  VarFullStateSpeed  |   0  |   1
     *  VarCurrentSpeed    |   1  |   2
     *  VarStepSpeed       |  44  |  88
     */
		{0, 1, 12}, /* Epp Gray, Dpi <= 150		 ;(6)         */
		{1, 2, 80}, /* Epp Gray, Dpi <= 300		 ;(7)         */
		{0, 1, 80}, /* Epp Gray, Dpi > 300, Px <= 3000	 ;(8) */
		{0, 1, 80}, /* Epp Gray, Dpi > 300, Px > 3000	 ;(9) */

   /* BPP Gray */
		{0, 1, 11}, /* Bpp Gray, Dpi <= 150		 ; 10 */
	/* The difference for this DPI:
	 *     if pixels <=    | 1600 | Others
	 * --------------------+------+-------------
	 *   VarFullStateSpeed |   0  |   1
	 *   VarCurrentSpeed   |   1  |   2
	 *   VarStepSpeed      |  24  |  48
     */
		{0, 1, 24}, /* Bpp Gray, Dpi <= 300, Px <= 1600  ; 11 */
		{1, 2, 48}, /* Bpp Gray, Dpi <= 300, Px > 1600	 ; 12 */
	/* The difference for this DPI:
	 *     if pixels <=    | 1600 | 3200 | Others
	 * --------------------+-----+-------+----------------------
	 *   VarFullStateSpeed |  0  |	 1   |	 2
	 *   VarCurrentSpeed   |  1  |	 2   |	 4
	 *	 VarStepSpeed      | 44  |	88   |	88
	 */
		{0, 1, 44}, /* Bpp Gray, Dpi > 300, Px <= 1600	 ; 13 */
		{1, 2, 88}, /* Bpp Gray, Dpi > 300, Px <= 3200	 ; 14 */
		{2, 4, 88}, /* Bpp Gray, Dpi > 300, Px > 3200	 ; 15 */
    /* SPP Gray */
	/* The difference for this DPI:
	 *     if pixels <=    | 800 | Others
	 * --------------------+-----+-------------
	 *   VarFullStateSpeed |  0  |	 1
	 *   VarCurrentSpeed   |  1  |	 2
	 *	 VarStepSpeed      | 12  |	24
	 */
		{0, 1, 12}, /* Spp Gray, Dpi <= 150, Px <= 800	 ; 16 */
		{1, 2, 24}, /* Spp Gray, Dpi <= 150, Px > 800	 ; 17 */
	/* The difference for this DPI:
	 *     if pixels <=    |  800 | 1600 | Others
	 * --------------------+-----+-------+----------------------
     *   VarFullStateSpeed |  0  |	 1   |	 1
	 *   VarCurrentSpeed   |  1  |	 2   |	 4
	 *   VarStepSpeed      | 22  |	88   |	88
	 */
		{0, 1, 22}, /* Spp Gray, Dpi <= 300, Px <= 800	 ; 18 */
		{1, 2, 88}, /* Spp Gray, Dpi <= 300, Px <= 1600  ; 19 */
		{1, 4, 88}, /* Spp Gray, Dpi <= 300, Px > 1600	 ; 20 */
	/* The difference for this DPI:
	 *     if pixels <=    | 800 | 1600 | 3200 | Others
	 * --------------------+-----+------+------+---------------
	 *   VarFullStateSpeed |  0  |	 1  |	2  |   3
	 *   VarCurrentSpeed   |  1  |	 2  |	4  |   6
	 *   VarStepSpeed      | 44  |	88  |  88  |  88
	 */
		{0, 1, 44}, /* Spp Gray, Dpi > 300, Px <= 800	 ; 21 */
		{1, 2, 88}, /* Spp Gray, Dpi > 300, Px <= 1600	 ; 22 */
		{2, 4, 88}, /* Spp Gray, Dpi > 300, Px <= 3200	 ; 23 */
		{3, 6, 88}, /* Spp Gray, Dpi > 300, Px > 3200	 ; 24 */
	/* EPP Color */
		{0, 1, 6},  /* Epp Color, Dpi <= 60/100 	 ; 25 */
		{0, 1, 11}, /* Epp Color, Dpi <= 150		 ; 26 */
	/* The difference for this DPI:
	 *     if pixels <=    | 1200 | Others
	 * --------------------+------+-------------
	 *   VarFullStateSpeed |   0  |   1
	 *   VarCurrentSpeed   |   1  |   2
	 *	 VarStepSpeed      |  24  |  48
	 */
		{0, 1, 24}, /* Epp Color, Dpi <= 300, Px <= 1400 ; 27 */
		{1, 2, 48}, /* Epp Color, Dpi <= 300, Px > 1400  ; 28 */
	/* The difference for this DPI:
	 *     if pixels <=    | 1400 | 2800 | 4000 | Others
	 * --------------------+------+------+------+---------------
	 *   VarFullStateSpeed |   0  |   1  |	 2  |	3
	 *   VarCurrentSpeed   |   1  |   2  |	 4  |	6
	 *  VarStepSpeed       |  48  |  96  |	88  |  88
	 *     VarExposureTime |  96  |  96  |	88  |  88
	 */
		{0, 1, 48}, /* Epp Color, Dpi > 300, Px <= 1400  ; 29 */
		{1, 2, 96}, /* Epp Color, Dpi > 300, Px <= 2800  ; 30 */
		{2, 4, 88}, /* Epp Color, Dpi > 300, Px <= 4000  ; 31 */
		{4, 8, 88}, /* Epp Color, Dpi > 300, Px >  4000  ; 32 */
    /* BPP Color */
		{0, 1, 6},  /* Bpp/Spp Color, Dpi <= 60 	 ; 33 */
		{0, 1, 12}, /* Bpp/Spp Color, Dpi <= 100	 ; 34 */
	/*     if pixels <=    | 800 | Others
	 * --------------------+-----+-------------
	 *   VarFullStateSpeed |  0  |	 1
     *     VarCurrentSpeed |  1  |	 2
	 *	 VarStepSpeed      | 12  |	24
	 */
		{0, 1, 12}, /* Bpp/Spp Color, Dpi <= 150, Px <= 800  ; 35 */
		{1, 2, 24}, /* Bpp/Spp Color, Dpi <= 150, Px > 800   ; 36 */
	/* The difference for this DPI:
	 *     if pixels <=    |  800 | 1600 | Others
	 * --------------------+-----+-------+----------------------
	 *   VarFullStateSpeed |  0  |	 1   |	 1
	 *   VarCurrentSpeed   |  1  |	 2   |	 4
	 *	 VarStepSpeed      | 24  |	48   |	96
	 */
		{0, 1, 24}, /* Bpp Color, Dpi <= 300, Px <= 800  ; 37 */
		{1, 2, 48}, /* Bpp Color, Dpi <= 300, Px <= 1600 ; 38 */
		{1, 4, 96}, /* Bpp Color, Dpi <= 300, Px > 1600  ; 39 */
	/* The difference for this DPI:
	 *     if pixels <=    | 800 | 1600 | 3200 | Others
	 * --------------------+-----+------+------+---------------
	 *   VarFullStateSpeed |  0  |	 1  |	2  |   4
	 *   VarCurrentSpeed   |  1  |	 2  |	4  |   8
	 *   VarStepSpeed      | 48  |	96  |  96  |  96
     */
		{0, 1, 48}, /* Bpp Color, Dpi > 300, Px <= 800	 ; 40 */
		{1, 2, 48}, /* Bpp Color, Dpi > 300, Px <= 1600  ; 41 */
		{2, 4, 96}, /* Bpp Color, Dpi > 300, Px <= 3200  ; 42 */
		{4, 8, 96}, /* Bpp Color, Dpi > 300, Px > 3200	 ; 43 */
    /* SPP Color */
	/* The difference for this DPI:
	 *     if pixels <=    | 500 | 1000 | 2000 | Others
	 * --------------------+-----+------+------+---------------
	 *   VarFullStateSpeed |  0  |	 1  |	1  |   2
	 *   VarCurrentSpeed   |  1  |	 2  |	4  |   8
	 *	 VarStepSpeed      | 24  |	48  |  96  |  96
	 */
		{0, 1, 24}, /* Spp Color, Dpi <= 300, Px <= 500  ; 44 */
		{1, 2, 48}, /* Spp Color, Dpi <= 300, Px <= 1000 ; 45 */
		{1, 4, 96}, /* Spp Color, Dpi <= 300, Px <= 2000 ; 46 */
		{2, 8, 96}, /* Spp Color, Dpi <= 300, Px > 2000  ; 47 */
	/* The difference for this DPI:
	 *     if pixels <=    | 500 | 1000 | 2000 | 4000 | Others
	 * --------------------+-----+------+------+------+--------
	 *   VarFullStateSpeed |  0  |	 1  |	2  |   4  |   5
	 *   VarCurrentSpeed   |  1  |	 2  |	4  |   8  |  10
	 *	 VarStepSpeed      | 48  |	96  |  96  |  96  |  96
     */
		{0, 1, 48},  /* Spp Color, Dpi > 300, Px <= 500   ; 48 */
		{1, 2, 96},  /* Spp Color, Dpi > 300, Px <= 1000  ; 49 */
		{2, 4, 96},  /* Spp Color, Dpi > 300, Px <= 2000  ; 50 */
		{4, 8, 96},  /* Spp Color, Dpi > 300, Px <= 4000  ; 51 */
		{5, 10, 96}, /* Spp Color, Dpi > 300, Px > 4000   ; 52 */

    /* Negative & Transparency   */
    /* EPP/SPP/BPP               */
/* for exposure time = 96 */
		{0, 1, 12},  /* Spp/EPP Color, Dpi <= 150	  ; 60 */
		{0, 1, 24},  /* Spp Color, Dpi <= 300		  ; 61 */
		{0, 1, 48},  /* Spp Color, Dpi > 300		  ; 62 */
		{0, 1, 12},  /* Bpp/Epp B/W, Dpi <= 75		  ; 56 */

/* for exposure time = 144 */
		{0, 1, 18},  /* Spp/EPP Color, Dpi <= 150	  ; 57 */
		{0, 1, 36},  /* Spp Color, Dpi <= 300		  ; 58 */
		{0, 1, 72},  /* Spp Color, Dpi > 300		  ; 59 */

/* for exposure time = 192 */
		{0, 1, 24},  /* Spp/EPP Color, Dpi <= 150	  ; 53 */
		{0, 1, 48},  /* Spp Color, Dpi <= 300		  ; 54 */
		{0, 1, 96},  /* Spp Color, Dpi > 300		  ; 55 */

/* for 48 bits color */
		{1, 2, 12}, /* Epp Color, Dpi <= 100, Px > 1400   ; 63 */
		{1, 2, 22}, /* Epp Color, Dpi <= 150, Px > 1900   ; 64 */
		{2, 4, 48}, /* Epp Color, Dpi <= 300, Px > 4000   ; 65 */
		{5, 10, 88},/* Epp Color, Dpi >  300, Px > 9600   ; 66 */
		{3, 12, 96} /* Spp Color, Dpi <= 300, Px > 3000   ; 67 */
};


static pModeTypeVar	pModeType;
static pDiffModeVar	pModeDiff;

/*
 * prototypes for the speed procs (ASIC 98001), EPP, SPP and BIDI
 */
static void fnLineArtSpeed( pScanData ps );
static void fnGraySpeed   ( pScanData ps );
static void fnColorSpeed  ( pScanData ps );

static void fnSppLineArtSpeed( pScanData ps );
static void fnSppGraySpeed   ( pScanData ps );
static void fnSppColorSpeed  ( pScanData ps );

static void fnBppLineArtSpeed( pScanData ps );
static void fnBppGraySpeed   ( pScanData ps );
static void fnBppColorSpeed  ( pScanData ps );

/*
 * some procedures for the different modes
 */
static pFnSpeed_Set a_fnSpeedProcs[5] = {
	fnLineArtSpeed,
	fnGraySpeed,
	fnGraySpeed,
	fnColorSpeed,
	fnColorSpeed
};

static pFnSpeed_Set a_fnSppSpeedProcs[5] = {
	fnSppLineArtSpeed,
	fnSppGraySpeed,
	fnSppGraySpeed,
	fnSppColorSpeed,
	fnSppColorSpeed
};

static pFnSpeed_Set a_fnBppSpeedProcs[5] = {
	fnBppLineArtSpeed,
	fnBppGraySpeed,
	fnBppGraySpeed,
	fnBppColorSpeed,
	fnBppColorSpeed
};


/*************************** local functions *********************************/

/*.............................................................................
 *
 */
static void ioP96InitialSetCurrentSpeed( pScanData ps )
{
	DBG( DBG_LOW, "ioP96InitialSetCurrentSpeed()\n" );

	switch ( ps->DataInf.wPhyDataType ) {

	case COLOR_BW:
		ps->bCurrentSpeed = (ps->DataInf.dwAsicPixelsPerPlane >
			                      _BUF_SIZE_BASE_CONST * 2) ? 2 : 1;
		break;

	case COLOR_256GRAY:
		if ( COLOR_256GRAY == ps->DataInf.wAppDataType ) {

			ps->bCurrentSpeed = (Byte)(ps->a_wGrayInitTime[ps->IO.portMode] /
			                           ps->wLinesPer64kTime);
			if (!ps->bCurrentSpeed)
			    ps->bCurrentSpeed = 1;

			if ((ps->DataInf.dwAsicPixelsPerPlane>=1500) && (ps->bCurrentSpeed==1))
			    ps->bCurrentSpeed = 2;

			if ( ps->DataInf.xyAppDpi.x > 1200) {
			    ps->bCurrentSpeed += 2; 			/* 1201-2400 */

			    if ( ps->DataInf.xyAppDpi.x > 2400 )
					ps->bCurrentSpeed += 2;			/* >= 2401   */
			}

			MotorP96AdjustCurrentSpeed( ps, ps->bCurrentSpeed );

	    } else {

			if ( _PORT_SPP != ps->IO.portMode ) {
			    if( ps->DataInf.dwAsicPixelsPerPlane <= 1280 )
					ps->bCurrentSpeed = 1;	/* <= 1280 pixels */

			    else if( ps->DataInf.dwAsicPixelsPerPlane <= 1720 )
				    ps->bCurrentSpeed = 2;	/* 1281-1720 */

				else if( ps->DataInf.dwAsicPixelsPerPlane <= 3780 )
					ps->bCurrentSpeed = 4;	/* 1721-3780 */

			    else
					ps->bCurrentSpeed = 6;	/* >= 3780 */

			} else {

			    if( ps->DataInf.dwAsicPixelsPerPlane <= 400 )
					ps->bCurrentSpeed = 1;			/* <= 400 pixels */

			    else if( ps->DataInf.dwAsicPixelsPerPlane <= 853 )
				    ps->bCurrentSpeed = 2;		/* 401-853 */

				else if( ps->DataInf.dwAsicPixelsPerPlane <= 1280 )
					ps->bCurrentSpeed = 4;	/* 854-1280 */

			    else if( ps->DataInf.dwAsicPixelsPerPlane <= 1728 )
				    ps->bCurrentSpeed = 6;	/* 1281-1728 */

				else if( ps->DataInf.dwAsicPixelsPerPlane <= 3780 )
					ps->bCurrentSpeed = 8;	/* 1729-3780 */

			    else
					ps->bCurrentSpeed = 10;	/* > 3780 */
			}
	    }
	    break;

	case COLOR_TRUE24:
	    ps->bCurrentSpeed = (Byte)(ps->a_wColorInitTime[ps->IO.portMode] /
								   ps->wLinesPer64kTime);

	    if( 0 == ps->bCurrentSpeed ) {
			DBG( DBG_LOW, "Initially set to 1\n" );
			ps->bCurrentSpeed = 1;
		}

	    if (ps->DataInf.xyAppDpi.x > 150)  {
			if (ps->bCurrentSpeed < 4)
			    ps->bCurrentSpeed = 4;
	    } else {
/*
// HEINER:A3I
//			if (ps->DataInf.xyAppDpi.x > 100)
*/
			if (ps->DataInf.xyAppDpi.x > 75)
			    if (ps->bCurrentSpeed < 2)
					ps->bCurrentSpeed = 2;
		}

	    if( 1 != ps->bCurrentSpeed )
			ps->bCurrentSpeed += ps->bExtraAdd;

    	if (ps->DataInf.xyAppDpi.x > ps->PhysicalDpi) {
			if (ps->DataInf.xyAppDpi.x <= 600)
			    ps->bCurrentSpeed += 2;
			else if (ps->DataInf.xyAppDpi.x <= 1200)
				ps->bCurrentSpeed += 2;
    		else if (ps->DataInf.xyAppDpi.x <= 2400)
			    ps->bCurrentSpeed += 2;
			else
			    ps->bCurrentSpeed += 2;
	    }

		MotorP96AdjustCurrentSpeed( ps, ps->bCurrentSpeed );
    }

	DBG( DBG_LOW, "Current Speed = %u\n", ps->bCurrentSpeed );
}

/*.............................................................................
 *
 */
static void fnLineArtSpeed( pScanData ps )
{
    pModeType = a_BwSettings + _FixParamEppBw;
    pModeDiff = a_tabDiffParam + _BwEpp75;

    if (ps->DataInf.xyAppDpi.y > 75) {
		pModeType++;
		pModeDiff = a_tabDiffParam + _BwEpp150;
    }

    if (ps->DataInf.xyAppDpi.y > 150) {
		if (ps->DataInf.xyAppDpi.y <= 300) {
		    pModeType++;
		    pModeDiff = a_tabDiffParam + _BwEpp300;
		} else {
		    pModeType += 2;
		    pModeDiff = a_tabDiffParam + _BwEpp600;
		}
	}
}

/*.............................................................................
 *
 */
static void fnGraySpeed( pScanData ps )
{
    pModeType = a_GraySettings + _FixParamEppGray;
    pModeDiff = a_tabDiffParam + _GrayEpp75;

    if (ps->DataInf.xyAppDpi.y > 75) {
		pModeType++;
		pModeDiff = a_tabDiffParam + _GrayEpp150;
    }

    if ( ps->DataInf.xyAppDpi.y > 150) {
		if (ps->DataInf.xyAppDpi.y <= 300) {
		    pModeType++;
		    pModeDiff = a_tabDiffParam + _GrayEpp300;
		} else {
		    pModeType += 2;
		    pModeDiff = a_tabDiffParam + _GrayEpp600;
	    	if (ps->DataInf.dwAsicPixelsPerPlane > 3000)
				pModeDiff++;
		}
	}
}

/*.............................................................................
 *
 */
static void fnColorSpeed( pScanData ps )
{
	DBG( DBG_LOW, "fnColorSpeed();\n" );

    pModeType = a_ColorSettings + _FixParamEppColor;

    if ( ps->DataInf.xyAppDpi.y <= ps->wMinCmpDpi ) {
		/* DPI <= 60 */
		pModeDiff = a_tabDiffParam + _ColorEpp60;

	} else {

		if (ps->DataInf.xyAppDpi.y <= 100) {
		    pModeType++;
		    pModeDiff = a_tabDiffParam + _ColorEpp100;

	    	if (ps->DataInf.dwAsicBytesPerPlane > 1400)
				pModeDiff = a_tabDiffParam + _ColorEpp100_1400;
		} else {
		    if (ps->DataInf.xyAppDpi.y <= 150) {
				pModeType += 2;
				pModeDiff = a_tabDiffParam + _ColorEpp150;

				if (ps->DataInf.dwAsicBytesPerPlane > 1900)
				    pModeDiff = a_tabDiffParam + _ColorEpp150_1900;
		    } else {
				if (ps->DataInf.xyAppDpi.y <= 300) {
				    pModeType += 3;
				    pModeDiff = a_tabDiffParam + _ColorEpp300_1200;
				    if (ps->DataInf.dwAsicBytesPerPlane <= 1200)
						pModeDiff --;
				    else {
						if (ps->DataInf.dwAsicBytesPerPlane > 4000)
						    pModeDiff = a_tabDiffParam + _ColorEpp300_4000;
					}
				} else {
				    pModeType += 4;
				    pModeDiff = a_tabDiffParam + _ColorEpp600_4000;
				    pModeType->bExposureTime = 88;

				    if (ps->DataInf.dwAsicBytesPerPlane <= 4000) {
						pModeDiff--;
						if (ps->DataInf.dwAsicBytesPerPlane <= 2800) {
						    pModeType->bExposureTime = 96;
						    pModeDiff--;
						    if (ps->DataInf.dwAsicBytesPerPlane <= 1200)
								pModeDiff--;
						}
		    		} else {
						if (ps->DataInf.dwAsicBytesPerPlane >= 9600)
						    pModeDiff = a_tabDiffParam + _ColorEpp600_9600;
					}
				}
			}
     	}
	}
}

/*.............................................................................
 *
 */
static void fnSppLineArtSpeed( pScanData ps )
{
    pModeType = a_BwSettings + _FixParamSppBw;
    pModeDiff = a_tabDiffParam + _BwSpp75;

    if (ps->DataInf.xyAppDpi.y > 75) {

		pModeType++;
		pModeDiff = a_tabDiffParam + _BwSpp150;
    }

    if (ps->DataInf.xyAppDpi.y > 150) {
		if (ps->DataInf.xyAppDpi.y <= 300) {
		    pModeType++;
		    pModeDiff = a_tabDiffParam + _BwSpp300;
		} else {
		    pModeType += 2;
		    pModeDiff = a_tabDiffParam + _BwSpp600;
		}
	}
}

/*.............................................................................
 *
 */
static void fnSppGraySpeed( pScanData ps )
{
    pModeType = a_GraySettings + _FixParamSppGray;
    pModeDiff = a_tabDiffParam + _GraySpp75;

    if (ps->DataInf.xyAppDpi.y > 75) {
		pModeType++;
		pModeDiff = a_tabDiffParam + _GraySpp150_800;

	    if (ps->DataInf.xyAppDpi.y > 150) {

			if (ps->DataInf.xyAppDpi.y <= 300) {
			    pModeType ++;
	    		pModeDiff = a_tabDiffParam + _GraySpp300_1600;
			} else {

			    pModeType += 2;
	    		pModeDiff = a_tabDiffParam + _GraySpp600_3200;

			    if (ps->DataInf.dwAsicPixelsPerPlane <= 3200)
					pModeDiff--;
			}

			if (ps->DataInf.dwAsicPixelsPerPlane <= 1600)
			    pModeDiff--;
	    }

		if (ps->DataInf.dwAsicPixelsPerPlane <= 800)
			pModeDiff--;
    }
}

/*.............................................................................
 *
 */
static void fnSppColorSpeed( pScanData ps )
{
    pModeType = a_ColorSettings + _FixParamSppColor;
    pModeDiff = a_tabDiffParam + _ColorSpp60;

    if (ps->DataInf.xyAppDpi.y > ps->wMinCmpDpi) {
		pModeType ++;
		pModeDiff = a_tabDiffParam + _ColorSpp100;

		if (ps->DataInf.xyAppDpi.y > 100) {
		    pModeType ++;
		    pModeDiff = a_tabDiffParam + _ColorSpp150_800;

	    	if (ps->DataInf.xyAppDpi.y > 150) {
				pModeType ++;
				pModeDiff = a_tabDiffParam + _ColorSpp300_2000;

				if (ps->DataInf.xyAppDpi.y > 300) {

				    pModeType ++;
				    pModeDiff = a_tabDiffParam + _ColorSpp600_4000;

				    if (ps->DataInf.dwAsicBytesPerPlane > 4000)
						return;
		    		else
						pModeDiff--;
				} else {
				    if (ps->DataInf.dwAsicBytesPerPlane > 3000)
						pModeDiff = a_tabDiffParam + _ColorSpp300_3000;
		    		return;
				}

				if (ps->DataInf.dwAsicBytesPerPlane <= 2000) {
				    pModeDiff--;
				    if (ps->DataInf.dwAsicBytesPerPlane <= 1000) {
						pModeDiff--;

						if (ps->DataInf.dwAsicBytesPerPlane <= 500)
						    pModeDiff--;
		    		}
				}
	    	} else {

				if (ps->DataInf.dwAsicBytesPerPlane <= 800)
				    pModeDiff--;
	    	}
		}
    }
}

/*.............................................................................
 *
 */
static void fnBppLineArtSpeed( pScanData ps )
{
    pModeType = a_BwSettings + _FixParamBppBw;
/* (+) Micky, 7-14-1998
 *   pModeDiff = a_tabDiffParam + _BwBpp150;
 */
    pModeDiff = a_tabDiffParam + _BwBpp75;

    if( ps->DataInf.xyAppDpi.y > 75 ) {
		pModeType++;
		pModeDiff = a_tabDiffParam + _BwBpp150;
    }
/* (-) Micky, 7-14-1998 */
    if( ps->DataInf.xyAppDpi.y > 150 ) {
		if( ps->DataInf.xyAppDpi.y <= 300 ) {
		    pModeType++;
		    pModeDiff = a_tabDiffParam + _BwBpp300;
		} else {
		    pModeType += 2;
		    pModeDiff = a_tabDiffParam + _BwBpp600;
		}
	}
}

/*.............................................................................
 *
 */
static void fnBppGraySpeed( pScanData ps )
{
    pModeType = a_GraySettings + _FixParamBppGray;
/* (+) Micky, 7-14-1998
 *   pModeDiff = a_tabDiffParam + _GrayBpp150;
 */
    pModeDiff = a_tabDiffParam + _GrayBpp75;

    if( ps->DataInf.xyAppDpi.y > 75 ) {
		pModeType++;
		pModeDiff = a_tabDiffParam + _GrayBpp150;
    }
/* (-) Micky, 7-14-1998 */
    if( ps->DataInf.xyAppDpi.y > 150 ) {
		pModeType ++;
		pModeDiff = a_tabDiffParam + _GrayBpp300_1600;
		if( ps->DataInf.xyAppDpi.y > 300 ) {
		    pModeType ++;
		    pModeDiff = a_tabDiffParam + _GrayBpp600_3200;
	    	if( ps->DataInf.dwAsicPixelsPerPlane <= 3200 )
				pModeDiff --;
		}
		if( ps->DataInf.dwAsicPixelsPerPlane <= 1600 )
		    pModeDiff --;
    }
}

/*.............................................................................
 *
 */
static void fnBppColorSpeed( pScanData ps )
{
    pModeType = a_ColorSettings + _FixParamBppColor;
    pModeDiff = a_tabDiffParam + _ColorBpp60;

    if (ps->DataInf.xyAppDpi.y > ps->wMinCmpDpi ) {
		pModeType ++;
		pModeDiff = a_tabDiffParam + _ColorBpp100;

		if( ps->DataInf.xyAppDpi.y > 100 ) {
		    pModeType ++;
		    pModeDiff = a_tabDiffParam + _ColorBpp150_800;
	    	if( ps->DataInf.xyAppDpi.y > 150 ) {
				pModeType ++;
				pModeDiff = a_tabDiffParam + _ColorBpp300_1600;
				if( ps->DataInf.xyAppDpi.y > 300 ) {
				    pModeType ++;
				    pModeDiff = a_tabDiffParam + _ColorBpp600_3200;
				    if( ps->DataInf.dwAsicBytesPerPlane <= 3200 )
						return;
		    		else
						pModeDiff--;
				}

				if( ps->DataInf.dwAsicBytesPerPlane <= 1600 )
				    pModeDiff--;
	    	}

		    if( ps->DataInf.dwAsicBytesPerPlane <= 800 )
				pModeDiff--;
		}
    }
}

/*.............................................................................
 *
 */
static void ioP98SppNegativeProcs( pScanData ps )
{
	if ( ps->DataInf.dwScanFlag & SCANDEF_Negative )
		pModeType = a_FilmSettings + _FixParamSppNegative;
    else
		pModeType = a_FilmSettings + _FixParamSppPositive;

    pModeDiff = a_tabDiffParam + _NegativeSpp150;

    if (ps->DataInf.xyAppDpi.y > 150) {
		if (ps->DataInf.xyAppDpi.y < 300) {
		    pModeType ++;
		    pModeDiff ++;
		} else {
		    pModeType += 2;
		    pModeDiff += 2;
		}
	}

    if (ps->DataInf.dwScanFlag & SCANDEF_Negative) {
		if (ps->AsicReg.RD_LineControl == 144)
		    pModeDiff += 4;
		else
	    	if (ps->AsicReg.RD_LineControl == 192)
			pModeDiff += 7;
    }
}

/*.............................................................................
 *
 */
static void ioP98EppNegativeProcs( pScanData ps )
{
    if (ps->DataInf.dwScanFlag & SCANDEF_Negative)
		pModeType = a_FilmSettings + _FixParamEppNegative;
    else
		pModeType = a_FilmSettings + _FixParamEppPositive;

	pModeDiff = a_tabDiffParam + _NegativeEpp150;

    if (ps->DataInf.xyAppDpi.y > 150) {
		if (ps->DataInf.xyAppDpi.y < 300)
		{
	    	pModeType ++;
		    pModeDiff ++;
		} else {
		    pModeType += 2;
		    pModeDiff += 2;
		}
	}

    if (ps->DataInf.dwScanFlag & SCANDEF_Negative) {
		if (ps->AsicReg.RD_LineControl == 144)
		    pModeDiff += 4;
		else
		    if (ps->AsicReg.RD_LineControl == 192)
				pModeDiff += 7;
    }
}

/*.............................................................................
 *
 */
static void ioP98BppNegativeProcs( pScanData ps )
{
    if( ps->DataInf.dwScanFlag & SCANDEF_Negative) {
		pModeType = a_FilmSettings + _FixParamBppNegative;
	} else {
		pModeType = a_FilmSettings + _FixParamBppPositive;
	}

    pModeDiff = a_tabDiffParam + _NegativeBpp150;

    if( ps->DataInf.xyAppDpi.y > 150 ) {
		if( ps->DataInf.xyAppDpi.y < 300 ) {
		    pModeType ++;
		    pModeDiff ++;
		} else {
		    pModeType += 2;
		    pModeDiff += 2;
		}
	}

    if ( ps->DataInf.dwScanFlag & SCANDEF_Negative ) {
		if( ps->AsicReg.RD_LineControl == 144 ) {
		    pModeDiff += 4;
		} else {
		    if( ps->AsicReg.RD_LineControl == 192 )
				pModeDiff += 7;
	    }
	}
}

/*.............................................................................
 *
 */
static void ioControlLampOnOff( pScanData ps )
{
	Byte lampStatus;

	ps->fWarmupNeeded = _TRUE;

	if( _IS_ASIC98(ps->sCaps.AsicID)) {

	    lampStatus = ps->AsicReg.RD_ScanControl & _SCAN_LAMPS_ON;

    	if (ps->bLastLampStatus != lampStatus) {

			DBG( DBG_LOW, "Using OTHER Lamp !\n" );
			ps->bLastLampStatus = lampStatus;

			IOCmdRegisterToScanner( ps, ps->RegScanControl,
									ps->AsicReg.RD_ScanControl);
			return;
	    }
	} else {

	    lampStatus = ps->AsicReg.RD_ScanControl & _SCAN_LAMP_ON;

		if (ps->DataInf.dwScanFlag&(SCANDEF_Transparency + SCANDEF_Negative)) {
	    	ps->bLampOn = 0;
		} else {
	    	ps->bLampOn = _SCAN_LAMP_ON;
		}

    	if (ps->bLastLampStatus != lampStatus) {
			DBG( DBG_LOW, "Using OTHER Lamp !\n" );
			ps->bLastLampStatus = lampStatus;
			return;
		}
	}

	ps->fWarmupNeeded = _FALSE;
	DBG( DBG_LOW, "Using SAME Lamp !\n" );
}

/*.............................................................................
 *
 */
static void ioP98InitialSetCurrentSpeed( pScanData ps )
{
	DBG( DBG_LOW, "ioP98InitialSetCurrentSpeed()\n" );

    if( ps->DataInf.dwScanFlag & SCANDEF_TPA) {

		switch (ps->IO.portMode)
		{
	    	case _PORT_SPP:  ioP98SppNegativeProcs( ps ); break;
		    case _PORT_BIDI: ioP98BppNegativeProcs( ps ); break;

		    default: ioP98EppNegativeProcs( ps ); break;
		}
    } else {

		switch (ps->IO.portMode) {
		    case _PORT_SPP:
				a_fnSppSpeedProcs[ps->DataInf.wAppDataType](ps);
				break;

		    case _PORT_BIDI:
				a_fnBppSpeedProcs[ps->DataInf.wAppDataType](ps);
				break;

		    default:
				a_fnSpeedProcs[ps->DataInf.wAppDataType](ps);
				break;
		}
    }

    ps->wInitialStep = pModeType->wHomePos;
    ps->wMaxMoveStep = pModeType->wMaxSteps;

	ps->AsicReg.RD_LineControl = pModeType->bExposureTime;

    if (ps->DataInf.dwScanFlag & SCANDEF_Negative)
		ps->AsicReg.RD_LineControl = 144;

#ifdef DEBUG
    if( pModeType->bFlagScanMode != ps->Shade.bIntermediate )
        DBG( DBG_HIGH, "bSetScanModeFlag != bIntermediate\n" );
#endif

    ps->bHpMotor 		 = pModeType->bMotorStep;
    ps->bSetScanModeFlag = pModeType->bFlagScanMode;
    ps->bShadingTimeFlag = pModeType->bTimesShading;

    ps->dwFullStateSpeed = pModeDiff->dwFullSpeed;
    ps->bCurrentSpeed    = pModeDiff->bCurrentSpeed;
    ps->bStepSpeed       = pModeDiff->bStepSpeed;

    if( ps->DataInf.xyAppDpi.y > 600 ) {
		if( ps->dwFullStateSpeed )
		    ps->dwFullStateSpeed = 0;
		else
		    ps->bStepSpeed <<= 1;
		ps->wMaxMoveStep <<= 1;
    }
}

/************************ exported functions *********************************/

/*.............................................................................
 * here we do some init work
 */
_LOC int IOFuncInitialize( pScanData ps )
{
	DBG( DBG_HIGH, "IOFuncInitialize()\n" );

	if( NULL == ps )
		return _E_NULLPTR;

	ps->lpEppColorHomePos  = &a_ColorSettings[0];
	ps->lpEppColorExposure = &a_ColorSettings[4];
	ps->lpBppColorHomePos  = &a_ColorSettings[5];
	ps->lpSppColorHomePos  = &a_ColorSettings[10];
	ps->a_tabDiffParam	   = a_tabDiffParam;
	ps->a_ColorSettings	   = a_ColorSettings;

	/*
	 * depending on the asic, we set some functions
	 */
	if( _IS_ASIC98(ps->sCaps.AsicID)) {

		ps->InitialSetCurrentSpeed = ioP98InitialSetCurrentSpeed;

  	} else 	if( _IS_ASIC96(ps->sCaps.AsicID)) {

		ps->InitialSetCurrentSpeed = ioP96InitialSetCurrentSpeed;

 	} else {

		DBG( DBG_HIGH , "NOT SUPPORTED ASIC !!!\n" );
		return _E_NOSUPP;
	}

	return _OK;
}

/*.............................................................................
 * 1) Fill scan states to asic.
 * 2) Refresh the scan states if necessary
 * 3) Wait for motor running within half-second period.
 */
_LOC Byte IOSetToMotorRegister( pScanData ps )
{
    ps->OpenScanPath( ps );

    IORegisterToScanner( ps, ps->RegInitScanState );

    IODownloadScanStates( ps );

    ps->CloseScanPath( ps );

	if( _ASIC_IS_98001 != ps->sCaps.AsicID ) {
		return 0;
	}

	ps->Scan.bOldScanState = IOGetScanState( ps, _FALSE );

    return ps->Scan.bOldScanState;
}

/*.............................................................................
 * 1) If scanner path is not established, connect it
 * 2) Read the recent state count
 * 3) Disconnect the path if necessary
 */
_LOC Byte IOGetScanState( pScanData ps, Bool fOpenned )
{
    Byte bScanState, bScanStateNow;

    if( !fOpenned && (_ASIC_IS_98003 != ps->sCaps.AsicID))
		ps->OpenScanPath( ps );

    bScanState    = IODataFromRegister( ps, ps->RegGetScanState );
    bScanStateNow = IODataFromRegister( ps, ps->RegGetScanState );

    if((bScanState != bScanStateNow)
		|| ((ps->sCaps.AsicID == _ASIC_IS_98001 && bScanState & 0x40))) {
		bScanState = IODataFromRegister( ps, ps->RegGetScanState);
	}

    if( !fOpenned && (_ASIC_IS_98003 != ps->sCaps.AsicID))
		ps->CloseScanPath( ps );

    return bScanState;
}

/*.............................................................................
 * ASIC 98003 specific function to read status 2 regiser
 */
_LOC Byte IOGetExtendedStatus( pScanData ps )
{
    Byte b;

    b = IODataFromRegister( ps, ps->RegStatus2 );

    if( b == 0xff )
    	return 0;
    return b;
}

/*.............................................................................
 * Read the scan state. Return the count with status bit, and count.
 */
_LOC void IOGetCurrentStateCount( pScanData ps, pScanState pScanStep )
{
    pScanStep->bStatus = IOGetScanState( ps, _FALSE );
    pScanStep->bStep   = pScanStep->bStatus & _SCANSTATE_MASK;
}

/*.............................................................................
 * 1) If scanner connection is not established, return error
 * 2) If paper not ready, return error
 * 3) If scanning environment is not prepared, return error
 * 4) Setup the buffers for reassembler the CCD incoming lines.
 * 5) Initiate the registers of asic.
 * [NOTE]
 *	This routine combines from SetupAsicDependentVariables & IsReadyForScan
 *	routines in assembly source.
 */
_LOC int IOIsReadyForScan( pScanData ps )
{
	ULong  dw;
	pULong pdwTable;

    if((_NO_BASE != ps->sCaps.wIOBase) &&
							   (ps->DataInf.dwVxdFlag & _VF_ENVIRONMENT_READY)) {

		if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {

			IOSelectLampSource( ps );
			ioControlLampOnOff( ps );
			ps->AsicReg.RD_Motor0Control = 0;	    /* motor off */
			ps->AsicReg.RD_Motor1Control = 0;	    /* motor off */
			ps->AsicReg.RD_ModelControl = (_ModelDpi600 +
										   _LED_ACTIVITY + _LED_CONTROL);
			ps->AsicReg.RD_Origin = 0;
			ps->AsicReg.RD_Pixels = 5110;

        } else if( _ASIC_IS_98003 == ps->sCaps.AsicID ) {

            ps->OpenScanPath( ps );
            P12SetGeneralRegister( ps );
            ps->CloseScanPath( ps );

			ioControlLampOnOff( ps );

		} else {

			ioControlLampOnOff( ps );

			/* SetupAsicDependentVariables */
			ps->pPutBufR = ps->pGetBufR = ps->pPrescan16; /* 1st color plane */
			ps->pPutBufG = ps->pGetBufG = ps->pPrescan8;  /* 2nd color plane */

			ps->AsicReg.RD_ScanControl    = ps->bLampOn;
			ps->Asic96Reg.RD_MotorControl = 0;
			ps->AsicReg.RD_Origin         = 0;
			ps->AsicReg.RD_ModelControl   = ps->Device.ModelCtrl | _ModelWhiteIs0;
			ps->AsicReg.RD_Pixels         = 5110; /* ps->RdPix; */
			IOPutOnAllRegisters( ps );
		}

		/*
		 * MotorInitiate
		 */
        if( _ASIC_IS_98003 != ps->sCaps.AsicID ) {
    		for (dw = _SCANSTATE_BYTES,
	    	     pdwTable = (pULong)ps->a_wMoveStepTable; dw; dw--, pdwTable++) {
	        	*pdwTable = 0x10001;
    		}

	    	memset( ps->a_bColorByteTable, 0, _NUMBER_OF_SCANSTEPS );
        }

		return _OK;
    }

	return _E_SEQUENCE;
}

/*.............................................................................
 *
 */
_LOC void IOSetXStepLineScanTime( pScanData ps, Byte b )
{
    ps->AsicReg.RD_LineControl = b;
    ps->bSpeed1  = b;
    ps->bSpeed2  = b >> 1;
    ps->bSpeed4  = b >> 2;
	ps->bSpeed8  = b >> 3;
    ps->bSpeed16 = b >> 4;
    ps->bSpeed32 = b >> 5;
    ps->bSpeed24 = b / 24;
    ps->bSpeed12 = b / 12;
    ps->bSpeed6  = b / 6;
    ps->bSpeed3  = b / 3;
}

/*.............................................................................
 * 1) Reset and fill all new scan states (Mode = Scan)
 * 2) Refresh scan state
 * 3) Wait for motor running within half second.
 */
_LOC void IOSetToMotorStepCount( pScanData ps )
{
	ULong	 dw;
    pUChar	 pb;
	TimerDef timer;

  	ps->OpenScanPath( ps );

	if( _ASIC_IS_98001 == ps->sCaps.AsicID ) {
	    IORegisterToScanner( ps, ps->RegInitScanState );
	} else {
	    ps->AsicReg.RD_ModeControl = _ModeScan;
	    IODataToRegister( ps, ps->RegModeControl, _ModeScan );
	}
    IORegisterToScanner( ps, ps->RegScanStateControl );

    for (dw =  _SCANSTATE_BYTES, pb = ps->a_nbNewAdrPointer; dw; dw--, pb++)
		IODataToScanner( ps, *pb );

    IORegisterToScanner( ps, ps->RegRefreshScanState );

	MiscStartTimer( &timer, (_SECOND/2));
    do {

		if (!( IOGetScanState( ps, _TRUE) & _SCANSTATE_STOP))
		    break;
    }
    while( !MiscCheckTimer(&timer));

/* CHECK - this line has been added by Rick ? Why ?
 *   return (pScanData->bOldTempScanState = GetScanState (pScanData, FALSE));
 */
	ps->Scan.bOldScanState = IOGetScanState( ps, _TRUE );

    ps->CloseScanPath( ps );
}

/*.............................................................................
 *
 */
_LOC void IOSelectLampSource( pScanData ps )
{
 	ps->AsicReg.RD_ScanControl &= (~_SCAN_LAMPS_ON);

    if (ps->DataInf.dwScanFlag & (SCANDEF_TPA)) {
		ps->AsicReg.RD_ScanControl |= _SCAN_TPALAMP_ON;
	} else {
		ps->AsicReg.RD_ScanControl |= _SCAN_NORMALLAMP_ON;
	}
}

/*.............................................................................
 *
 */
_LOC Bool IOReadOneShadingLine( pScanData ps, pUChar pBuf, ULong len )
{
	TimerDef timer;

	MiscStartTimer( &timer, _SECOND );

    if( _ASIC_IS_98003 == ps->sCaps.AsicID )
        ps->Scan.bFifoSelect = ps->RegGFifoOffset;

    do {
    	if( IOReadFifoLength( ps ) >= ps->AsicReg.RD_Pixels ) {

            IOReadColorData( ps, pBuf, len );
	        return _TRUE;
    	}
    } while( _OK == MiscCheckTimer( &timer ));

    return _FALSE;
}

/*.............................................................................
 *
 */
_LOC ULong IOReadFifoLength( pScanData ps )
{
	DataType Data;

    Data.dwValue  = 0;

    if( _ASIC_IS_98003 != ps->sCaps.AsicID )
    	ps->OpenScanPath( ps );

    IODataToRegister( ps, ps->RegBitDepth, _BIT0_7 );
    Data.dwOverlap.w1st.b1st = IODataFromRegister( ps, ps->Scan.bFifoSelect );

    IODataToRegister( ps, ps->RegBitDepth, _BIT8_15 );
    Data.dwOverlap.w1st.b2nd = IODataFromRegister( ps, ps->Scan.bFifoSelect );

    IODataToRegister( ps, ps->RegBitDepth, _BIT16_20 );
    Data.dwOverlap.w2nd.b1st = (IODataFromRegister( ps, ps->Scan.bFifoSelect) & 0x0f);

    if( _ASIC_IS_98003 != ps->sCaps.AsicID )
        ps->CloseScanPath( ps );

    return Data.dwValue;
}

/*.............................................................................
 * 1) Initiates the scan states
 * 2) Write the contents to corresponding registers (from ps->RegModeControl to
 *    ps->RegGreenGainOutDirect (P9363) or from ps->RegModeControl to
 *	  ps->RegModeControl2 (48xx)
 */
_LOC void IOPutOnAllRegisters( pScanData ps )
{
	pUChar	pValue;
    Byte	bReg;

	/* setup scan states */
    if( _ASIC_IS_98003 == ps->sCaps.AsicID )
        IODownloadScanStates( ps );
    else {
    	IOSetToMotorRegister( ps );
    	ps->OpenScanPath( ps );
    }

	if( _IS_ASIC98(ps->sCaps.AsicID)) {

		IODataToRegister(ps, ps->RegStepControl, ps->AsicReg.RD_StepControl);
    	IODataToRegister(ps, ps->RegMotor0Control,
						 ps->AsicReg.RD_Motor0Control);

	    if( _ASIC_IS_98003 == ps->sCaps.AsicID )
            IODataToRegister(ps,ps->RegLineControl,ps->AsicReg.RD_LineControl);

	    IODataToRegister(ps, ps->RegXStepTime, 	  ps->AsicReg.RD_XStepTime);
    	IODataToRegister(ps, ps->RegModelControl, ps->AsicReg.RD_ModelControl);

		/* the 1st register to write */
    	pValue = (pUChar)&ps->AsicReg.RD_Dpi;

		/* 0x21 - 0x28 */
    	for (bReg = ps->RegDpiLow;
			 bReg <= ps->RegThresholdHigh; bReg++, pValue++) {

			IODataToRegister( ps, bReg, *pValue);
		}

    	IORegisterToScanner( ps, ps->RegInitDataFifo );
	    IORegisterToScanner( ps, ps->RegRefreshScanState );

	    if( _ASIC_IS_98003 == ps->sCaps.AsicID )
            IODataToRegister( ps, ps->RegModeControl, _ModeScan );
        else
            IODataToRegister( ps, ps->RegModeControl, (_ModeScan + _ModeFifoRSel));

	} else {

		/*
		 * the original driver uses a loop, starting at RegModeControl
		 * 0x18 - 0x26
		 * as we use the Asic96Reg structure only  for the differences
		 * to the AsicReg struct, we have to write to each register by hand
		 */
		IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl );
		IODataToRegister( ps, ps->RegLineControl, ps->AsicReg.RD_LineControl );
		IODataToRegister( ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl );
		IODataToRegister( ps, ps->RegMotorControl,
											   ps->Asic96Reg.RD_MotorControl );
		IODataToRegister( ps, ps->RegModelControl,
												 ps->AsicReg.RD_ModelControl );
		IODataToRegister( ps, ps->RegMemAccessControl,
		 				  	  		       ps->Asic96Reg.RD_MemAccessControl );

#if 0
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
							  ps->RegModeControl, ps->AsicReg.RD_ModeControl );
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
							  ps->RegLineControl, ps->AsicReg.RD_LineControl );
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
							  ps->RegScanControl, ps->AsicReg.RD_ScanControl );
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
						  ps->RegMotorControl, ps->Asic96Reg.RD_MotorControl );
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
					   		ps->RegModelControl, ps->AsicReg.RD_ModelControl );
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
				  ps->RegMemAccessControl, ps->Asic96Reg.RD_MemAccessControl );
#endif

    	pValue = (pUChar)&ps->AsicReg.RD_Dpi;

		/* 0x21 - 0x26 */
    	for (bReg = ps->RegDpiLow;
			 bReg <= ps->RegWidthPixelsHigh; bReg++, pValue++) {

			IODataToRegister( ps, bReg, *pValue );
#if 0
			DBG( DBG_LOW, "[0x%02x] = 0x%02x\n", bReg, *pValue );
#endif
		}

		/* the rest */
		IODataToRegister( ps, ps->RegThresholdControl,
									 (Byte)ps->AsicReg.RD_ThresholdControl );

		IODataToRegister( ps, ps->RegWatchDogControl,
									 (Byte)ps->Asic96Reg.RD_WatchDogControl );

		IODataToRegister( ps, ps->RegModelControl2,
										  ps->Asic96Reg.u26.RD_ModelControl2 );

#if 0
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
					ps->RegThresholdControl, ps->AsicReg.RD_ThresholdControl );
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
					ps->RegWatchDogControl, ps->Asic96Reg.RD_WatchDogControl );
		DBG( DBG_LOW, "[0x%02x] = 0x%02x\n",
					ps->RegModelControl2, ps->Asic96Reg.u26.RD_ModelControl2 );
#endif
		IORegisterToScanner( ps, ps->RegInitDataFifo );
	}

    if( _ASIC_IS_98003 != ps->sCaps.AsicID )
        ps->CloseScanPath( ps );
}

/*.............................................................................
 *
 */
_LOC void IOReadColorData( pScanData ps, pUChar pBuf, ULong len )
{
   	ps->AsicReg.RD_ModeControl = _ModeFifoRSel;
    IOReadScannerImageData( ps, pBuf, len );

    ps->AsicReg.RD_ModeControl = _ModeFifoGSel;
    IOReadScannerImageData( ps, pBuf + len, len );

  	ps->AsicReg.RD_ModeControl = _ModeFifoBSel;
    IOReadScannerImageData( ps, pBuf + len * 2,  len );
}

/* END PLUSTEK-PP_GENERICIO.C ...............................................*/
