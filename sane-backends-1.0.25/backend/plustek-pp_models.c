/* @file plustek-pp_models.c
 * @brief model specific stuff
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - no changes
 * - 0.33 - no changes
 * - 0.34 - no changes
 * - 0.35 - added some comments
 *        - did some fine tuning on the 9630P and 12000P/9600P models
 *        - moved function initPageSettings() to this module
 * - 0.36 - as the ps->MaxWideLineBlks and ps->MaxWideLineLen are only used
 *          for the OP 4800, it has been removed from pScanData
 *        - changed settings of OP600 according to the Primax Direct 4800 tests
 *        - removed dwPreferSize from struct ScannerCaps
 *        - fixed the 5seconds bed-hit problem for ASIC 96001/3 based models
 *        - changes, due to define renaming
 * - 0.37 - added ButtonCount init
 *        - added A3I model
 *        - added functions modelInitCaps(), modelInitMotor() and
 *          modelSetBufferSizes()
 * - 0.38 - added P12 stuff
 *        - code cleanup
 * - 0.39 - no changes
 * - 0.40 - changed back to build 0.39-3 (disabled A3I stuff)
 * - 0.41 - added _OVR_PLUSTEK_4800P switch
 * - 0.42 - added SFLAG_CUSTOM_GAMMA to capabilities
 *        - added _OVR_PRIMAX_4800D30 switch
 *        - changed include names
 * - 0.43 - no changes
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

/*************************** local functions *********************************/

/*.............................................................................
 * initialize the extension according to the page size...
 */
static void modelInitPageSettings( pScanData ps )
{
	DBG(DBG_LOW, "modelInitPageSettings()\n" );

	if( MODEL_OP_9630PL == ps->sCaps.Model )
		ps->dwScannerSize = _SCANSIZE_LEGAL;
	else if( MODEL_OP_A3I == ps->sCaps.Model )
		ps->dwScannerSize = _SCANSIZE_A3;
	else
		ps->dwScannerSize = _SCANSIZE_A4;

    /* default width for all but A3 - 8.5"* 300dpi (_MEASURE_BASE) */
	ps->sCaps.wMaxExtentX = 2550;

    /* this applies to all scanners but the A3 model */
	ps->LensInf.rExtentX.wMin 	 = 150;
	ps->LensInf.rExtentX.wDef 	 = 2550;
	ps->LensInf.rExtentX.wMax 	 = 2550;
	ps->LensInf.rExtentX.wPhyMax = 2500;

	ps->LensInf.rExtentY.wMin 	 = 150;

	ps->LensInf.wBeginX 		 = 0;
	ps->LensInf.wBeginY 		 = 0;

    switch( ps->dwScannerSize ) {

	case _SCANSIZE_A4:
		/* 11.69 inches */
		DBG( DBG_LOW, "A4 set\n" );
	    ps->sCaps.wMaxExtentY     =
        ps->LensInf.rExtentY.wDef =
		ps->LensInf.rExtentY.wMax =
	    ps->LensInf.rExtentY.wPhyMax = _MEASURE_BASE * 11.6934;	
	    break;

	case _SCANSIZE_A3:
		/* 17 inches */
		DBG( DBG_LOW, "A3 set\n" );
	    ps->sCaps.wMaxExtentY     =
		ps->LensInf.rExtentY.wMax =
        ps->LensInf.rExtentY.wDef =
	    ps->LensInf.rExtentY.wPhyMax = _MEASURE_BASE * 17;	

        /* _MEASURE_BASE * 11.69 */
    	ps->sCaps.wMaxExtentX     =
	    ps->LensInf.rExtentX.wDef =
    	ps->LensInf.rExtentX.wMax = 3507;
    	ps->LensInf.rExtentX.wPhyMax = 3500;
	    break;

	case _SCANSIZE_LETTER:
        /* 11 inches */
		DBG( DBG_LOW, "Letter set\n" );
	    ps->sCaps.wMaxExtentY     =
        ps->LensInf.rExtentY.wDef =
		ps->LensInf.rExtentY.wMax =
	    ps->LensInf.rExtentY.wPhyMax = _MEASURE_BASE * 11;	
	    break;

	case _SCANSIZE_LEGAL:
        /* 14 inches */
		DBG( DBG_LOW, "Legal set\n" );
	    ps->sCaps.wMaxExtentY     =
        ps->LensInf.rExtentY.wDef =
		ps->LensInf.rExtentY.wMax =
	    ps->LensInf.rExtentY.wPhyMax = _MEASURE_BASE * 14;	
    }

	/*
	 * add this value to avoid the problems in binary mode
	 */
	ps->LensInf.rExtentY.wMax += 64;

    /* set the DPI stuff */
	ps->LensInf.rDpiX.wMin 	  = 16;
	ps->LensInf.rDpiX.wDef 	  = 50;
	ps->LensInf.rDpiX.wMax 	  = (ps->PhysicalDpi * 16);
	ps->LensInf.rDpiX.wPhyMax = ps->PhysicalDpi;
	ps->LensInf.rDpiY.wMin 	  = 16;
	ps->LensInf.rDpiY.wDef 	  = 50;
	ps->LensInf.rDpiY.wMax 	  = (ps->PhysicalDpi * 16);
	ps->LensInf.rDpiY.wPhyMax = (ps->PhysicalDpi * 2);
}

/*.............................................................................
 * set the scanner capabilities
 */
static void modelInitCaps( pScanData ps )
{
	ps->sCaps.wIOBase = _NO_BASE;
    ps->sCaps.dwFlag  = SFLAG_CUSTOM_GAMMA;
}

/*.............................................................................
 * set the motor stuff
 */
static void modelInitMotor( pScanData ps )
{
    if(	_ASIC_IS_96001 == ps->sCaps.AsicID ) {
    	ps->FullStep = _MotorFullStep96001;
	    ps->MotorOn  = _MotorOn96001;
    	ps->IgnorePF = _MotorIgnorePF96001;
    	ps->StepMask = ~ps->FullStep;
    } else {
    	ps->FullStep = _Motor1FullStep;
    	ps->MotorOn	 = _MotorOn;
	    ps->IgnorePF = _MotorIgnorePF;
    	ps->StepMask = _MotorStepMask;
    }

	ps->BackwardSteps = 4000;
}

/*.............................................................................
 * according to the models' capabilities, set the buffer stuff
 */
static void modelSetBufferSizes( pScanData ps )
{
    /* should depend on the scan-area !!!! */
    if( 400 == ps->PhysicalDpi ) {

        /* assuming a A3I */
    	ps->BufferSizeBase		   = 3517;
    	ps->BufferForColorRunTable = (5500 * 4); /* might be 17" * 800dpi !!! */

    } else if( 600 == ps->PhysicalDpi ) {

    	ps->BufferSizeBase		   = 2560;
    	ps->BufferForColorRunTable = (5500 * 4);

    } else {
    	ps->BufferSizeBase 	   	   = 1280;
	    ps->BufferForColorRunTable = 9000;
    }

 	ps->BufferSizePerModel = ps->BufferSizeBase * 2;
	ps->BufferForDataRead1 = ps->BufferSizePerModel * 3;

	/* patch that for the 600 DPI models OP9630 etc.*/
	if(( 300 != ps->PhysicalDpi) && (_ASIC_IS_96003 == ps->sCaps.AsicID))
		ps->BufferForDataRead1 += 300;

	ps->BufferFor1stColor  = (ps->BufferSizePerModel * 17);
	ps->BufferFor2ndColor  = (ps->BufferSizePerModel * 9);
	ps->TotalBufferRequire = (ps->BufferFor1stColor  +
                          	  ps->BufferFor2ndColor  +
                              ps->BufferForDataRead1 +
                              ps->BufferForColorRunTable );
}

/************************ exported functions *********************************/

/*.............................................................................
 * set the model to 4800
 */
_LOC void ModelSet4800( pScanData ps )
{
	DBG( DBG_LOW, "ModelSet4800()\n" );

	/* has 96001 ASIC */
	ps->sCaps.AsicID     = _ASIC_IS_96001;
	ps->sCaps.Model      = MODEL_OP_4800P;
	ps->Device.buttons   = 0;
	ps->Device.ModelCtrl = (_ModelDpi300 | _ModelMemSize32k96001 | _ModelWhiteIs0);
	ps->Device.DataOriginX = 72;

	ps->PhysicalDpi  = 300;
	ps->TimePerLine  = 0x30;
	ps->Offset70     = 70;

    modelSetBufferSizes( ps );

	ps->a_wGrayInitTime[0]  = 220;	/* _EppTimeForOthers	*/
	ps->a_wGrayInitTime[1]  = 720;	/* _SppTimeForOthers	*/
	ps->a_wGrayInitTime[2]  = 360;	/* _BidirTimeForOthers	*/
	ps->a_wColorInitTime[0] = 500;	/* _EppTimeForColor		*/
	ps->a_wColorInitTime[1] = 1680;	/* _SppTimeForColor		*/
	ps->a_wColorInitTime[2] = 1100;	/* _BidirTimeForColor	*/

	ps->AsicRedColor   = _ASIC_REDCOLOR;
	ps->AsicGreenColor = _ASIC_GREENCOLOR;
	ps->RedDataReady   = _RED_DATA_READY;
	ps->GreenDataReady = _GREEN_DATA_READY;

    /*
     * used for shading stuff (see dac.c)
     */
	ps->FBKScanLineBlks	    = 5;
	ps->FBKScanLineLenBase  = 1024;
	ps->FBKScanLineLen	    = (ps->FBKScanLineLenBase * 3);

	ps->ShadingBufferSize	= ps->FBKScanLineLen;
	ps->ShadingBankSize		= (ps->FBKScanLineLenBase * 4);
	ps->ShadingBankRed		= (_MemBankSize4k96001 | 0x3a);
	ps->ShadingBankGreen	= (_MemBankSize4k96001 | 0x3e);
	ps->ShadingBankBlue		= (_MemBankSize4k96001 | 0x3c);
	ps->ShadingScanLineBlks = 6;
	ps->ShadingScanLineLen  = (ps->BufferSizeBase * 3);
	ps->OneScanLineLen      = (ps->BufferSizePerModel * 3);

    modelInitMotor( ps );
    modelInitCaps ( ps );
	modelInitPageSettings( ps );

	DBG( DBG_LOW, "ModelSet4800() done.\n" );
}

/*.............................................................................
 * set the model to 4830
 */
_LOC void ModelSet4830( pScanData ps )
{
	DBG( DBG_LOW, "ModelSet4830()\n" );

	/* has 96003 ASIC */
	ps->sCaps.Model = MODEL_OP_4830P;
	if( _OVR_PRIMAX_4800D30 == ps->ModelOverride ) {
		DBG( DBG_LOW, "Model Override --> Primax 4800D 30\n" );
		ps->sCaps.Model = MODEL_PMX_4800D3;
	}
	ps->sCaps.AsicID     = _ASIC_IS_96003;
	ps->Device.buttons   = 1;
	ps->Device.ModelCtrl = (_ModelDpi300 | _ModelMemSize32k3 | _ModelWhiteIs0);
	ps->Device.DataOriginX = 72;

	ps->PhysicalDpi 		= 300;
	ps->TimePerLine 		= 0x30;
	ps->Offset70 		    = 70;

    modelSetBufferSizes( ps );

	ps->a_wGrayInitTime[0]  = 220;	/* _EppTimeForOthers 	*/
	ps->a_wGrayInitTime[1]  = 720;	/* _SppTimeForOthers 	*/
	ps->a_wGrayInitTime[2]  = 360;	/* _BidirTimeForOthers 	*/
	ps->a_wColorInitTime[0] = 500;	/* _EppTimeForColor 	*/
	ps->a_wColorInitTime[1] = 1680;	/* _SppTimeForColor 	*/
	ps->a_wColorInitTime[2] = 1100;  /* _BidirTimeForColor 	*/

	ps->AsicRedColor   = _ASIC_REDCOLOR;
	ps->AsicGreenColor = _ASIC_GREENCOLOR;
	ps->RedDataReady   = _RED_DATA_READY;
	ps->GreenDataReady = _GREEN_DATA_READY;

    /*
     * used for shading stuff (see dac.c)
     */
	ps->FBKScanLineBlks	    = 5;
	ps->FBKScanLineLenBase  = 1024;
	ps->FBKScanLineLen	    = (ps->FBKScanLineLenBase * 3);

  	ps->ShadingBufferSize	= ps->FBKScanLineLen;
	ps->ShadingBankSize		= (ps->FBKScanLineLenBase * 4);
	ps->ShadingBankRed		= (_MemBankSize4k | 0x3a);
	ps->ShadingBankGreen	= (_MemBankSize4k | 0x3e);
	ps->ShadingBankBlue		= (_MemBankSize4k | 0x3c);
	ps->ShadingScanLineBlks = 6;
	ps->ShadingScanLineLen  = (ps->BufferSizeBase * 3);
	ps->OneScanLineLen      = (ps->BufferSizePerModel * 3);

    modelInitMotor( ps );
    modelInitCaps ( ps );
	modelInitPageSettings( ps );

	DBG( DBG_LOW, "ModelSet4830() done.\n" );
}

/*.............................................................................
 * set the model to 600, tested on a Primax Direct 4800 and OP600
 */
_LOC void ModelSet600( pScanData ps )
{
	DBG( DBG_LOW, "ModelSet600()\n" );

	/*
 	 * set to 4830 first, then do the differences
	 */
 	ModelSet4830( ps );
	ps->Device.buttons = 0;

	if( _OVR_PLUSTEK_4800P == ps->ModelOverride ) {

		DBG( DBG_LOW, "Model Override --> OpticPro4800\n" );
		ps->sCaps.Model = MODEL_OP_4800P;

	} else if( _OVR_PRIMAX_4800D == ps->ModelOverride ) {

		DBG( DBG_LOW, "Model Override --> Primax 4800D\n" );
		ps->sCaps.Model = MODEL_PMX_4800D;

	} else {

		ps->sCaps.Model = MODEL_OP_600P;

		/* for Plustek OpticPro 600P it's necessary to swap Red and Green
		 * changed by mh moloch@nikocity.de
		 */
	 	ps->AsicRedColor   = _ASIC_GREENCOLOR;
	 	ps->AsicGreenColor = _ASIC_REDCOLOR;
	}

	DBG( DBG_LOW, "ModelSet600() done.\n" );
}

/*.............................................................................
 * set the model to 12000P, 96000P (tested on a OP96000P)
 */
_LOC void ModelSet12000( pScanData ps )
{
	DBG( DBG_LOW, "ModelSet12000()\n" );

	/*
	 * set to 9630 first, then do the differences
	 */
	ModelSet9630( ps );
	ps->Device.buttons = 0;
	ps->sCaps.Model    = MODEL_OP_12000P;

	/*
	 * swapped Red and Green for 12000P/96000P
	 */
	ps->AsicRedColor   = _ASIC_GREENCOLOR;
	ps->AsicGreenColor = _ASIC_REDCOLOR;
    ps->RedDataReady   = _GREEN_DATA_READY;
	ps->GreenDataReady = _RED_DATA_READY;

	DBG( DBG_LOW, "ModelSet12000() done.\n" );
}

/*.............................................................................
 * set the model to A3I
 */
_LOC void ModelSetA3I( pScanData ps )
{
	DBG( DBG_LOW, "ModelSetA3I()\n" );

	/*
 	 * has 96003 ASIC
	 */
	ps->Device.buttons = 1;
	ps->sCaps.Model    = MODEL_OP_A3I;
	ps->sCaps.AsicID   = _ASIC_IS_96003;

	ps->Device.ModelCtrl = (_ModelDpi400 | _ModelMemSize128k4 | _ModelWhiteIs0);
	ps->Device.DataOriginX = 164;

	ps->PhysicalDpi = 400;
	ps->TimePerLine = 0x50;
	ps->Offset70 	= 145;

    modelSetBufferSizes( ps );

	ps->a_wGrayInitTime[0]  = 133;  /* _EppTimeForOthers	*/
	ps->a_wGrayInitTime[1]  = 720;  /* _SppTimeForOthers	*/
	ps->a_wGrayInitTime[2]  = 300;  /* _BidirTimeForOthers	*/
	ps->a_wColorInitTime[0] = 400;  /* _EppTimeForColor		*/
	ps->a_wColorInitTime[1] = 1800; /* _SppTimeForColor		*/
	ps->a_wColorInitTime[2] = 800;  /* _BidirTimeForColor	*/

	ps->AsicRedColor   = _ASIC_GREENCOLOR;
	ps->AsicGreenColor = _ASIC_REDCOLOR;
    ps->RedDataReady   = _GREEN_DATA_READY;
	ps->GreenDataReady = _RED_DATA_READY;

	ps->FBKScanLineBlks	   = 10;
	ps->FBKScanLineLenBase = 2048;
	ps->FBKScanLineLen	   = (ps->FBKScanLineLenBase * 3);

	ps->ShadingBufferSize	= (1024 * 7);
	ps->ShadingBankSize		= 8192;
	ps->ShadingBankRed		= (_MemBankSize8k | 0x34);
	ps->ShadingBankGreen	= (_MemBankSize8k | 0x3c);
	ps->ShadingBankBlue		= (_MemBankSize8k | 0x38);
	ps->ShadingScanLineBlks = 10;
	ps->ShadingScanLineLen  = (ps->BufferSizeBase * 3);
	ps->OneScanLineLen      = (ps->ShadingScanLineLen * 2);

    modelInitMotor( ps );
	ps->BackwardSteps = 9000;

    modelInitCaps( ps );
	modelInitPageSettings( ps );

	/*
	 * need to double the vals
	 */
	ps->LensInf.rExtentX.wMax 	 *= 2;
	ps->LensInf.rExtentX.wPhyMax *= 2;
	ps->LensInf.rExtentY.wMax 	 *= 2;
	ps->LensInf.rExtentY.wPhyMax *= 2;

	DBG( DBG_LOW, "ModelSetA3I() done.\n" );
}

/*.............................................................................
 * set the model to 9630
 */
_LOC void ModelSet9630( pScanData ps )
{
	DBG( DBG_LOW, "ModelSet9360()\n" );

	/*
 	 * has 96003 ASIC
	 */
	if( _OVR_PLUSTEK_9630PL == ps->ModelOverride ) {
		DBG( DBG_LOW, "Model Override --> 9630PL\n" );
		ps->sCaps.Model = MODEL_OP_9630PL;
	} else {
		ps->sCaps.Model = MODEL_OP_9630P;
	}

	ps->Device.buttons = 1;
	ps->sCaps.AsicID   = _ASIC_IS_96003;

	ps->Device.ModelCtrl = (_ModelDpi600 | _ModelMemSize128k4 | _ModelWhiteIs0);
	ps->Device.DataOriginX = 64;

	ps->PhysicalDpi = 600;
	ps->TimePerLine = 0x5a;
	ps->Offset70 	= 95;

    modelSetBufferSizes( ps );

	ps->a_wGrayInitTime[0]  = 133;  /* _EppTimeForOthers	*/
	ps->a_wGrayInitTime[1]  = 720;  /* _SppTimeForOthers	*/
	ps->a_wGrayInitTime[2]  = 300;  /* _BidirTimeForOthers	*/
	ps->a_wColorInitTime[0] = 400;  /* _EppTimeForColor		*/
	ps->a_wColorInitTime[1] = 1800; /* _SppTimeForColor		*/
	ps->a_wColorInitTime[2] = 800;  /* _BidirTimeForColor	*/

	ps->AsicRedColor   = _ASIC_REDCOLOR;
	ps->AsicGreenColor = _ASIC_GREENCOLOR;
	ps->RedDataReady   = _RED_DATA_READY;
	ps->GreenDataReady = _GREEN_DATA_READY;

	ps->ShadingBufferSize	= (1024 * 7);
	ps->ShadingBankSize		= 8192;
	ps->ShadingBankRed		= (_MemBankSize8k | 0x34);
	ps->ShadingBankGreen	= (_MemBankSize8k | 0x3c);
	ps->ShadingBankBlue		= (_MemBankSize8k | 0x38);
	ps->ShadingScanLineBlks = 10;
	ps->ShadingScanLineLen  = (2560 * 3);

	ps->FBKScanLineBlks	   = 10;
	ps->FBKScanLineLenBase = 2048;
	ps->FBKScanLineLen	   = (ps->FBKScanLineLenBase * 6);

	ps->OneScanLineLen = (5120 * 3);

    modelInitMotor( ps );
	ps->BackwardSteps = 9000;

    modelInitCaps( ps );
	modelInitPageSettings( ps );

	/*
	 * need to double the vals
	 */
	ps->LensInf.rExtentX.wMax 	 *= 2;
	ps->LensInf.rExtentX.wPhyMax *= 2;
	ps->LensInf.rExtentY.wMax 	 *= 2;
	ps->LensInf.rExtentY.wPhyMax *= 2;

	DBG( DBG_LOW, "ModelSet9630() done.\n" );
}

/*.............................................................................
 * set the model to 9636 (ASIC 98001 models)
 * works for 9636P Turbo and 9636T /12000T
 */
_LOC void ModelSet9636( pScanData ps )
{
	DBG( DBG_LOW, "ModelSet9636()\n" );

	/*
	 *set to 9630 first, then do the differences
	 */
	ModelSet9630( ps );
	ps->Device.buttons = 0;

	/*
	 * has 98001 ASIC
	 */
	if( _OVR_PLUSTEK_9636 == ps->ModelOverride ) {
		DBG( DBG_LOW, "Model Override --> 9636P+/Turbo\n" );
		ps->sCaps.Model = MODEL_OP_9636PP;
	} else if( _OVR_PLUSTEK_9636P == ps->ModelOverride ) {
		DBG( DBG_LOW, "Model Override --> 9636P\n" );
		ps->sCaps.Model = MODEL_OP_9636P;
	} else {
		ps->sCaps.Model   = MODEL_OP_9636T;
        ps->sCaps.dwFlag |= SFLAG_TPA;
	}

	ps->Device.DataOriginX = 72;
	ps->sCaps.AsicID = _ASIC_IS_98001;

	ps->TotalBufferRequire = _LINE_BUFSIZE * 2 + _LINE_BUFSIZE1 +
			 					ps->BufferForColorRunTable + _PROCESS_BUFSIZE;

    /* do it again, as ModelSet9630() changes the result of this function !*/
	modelInitPageSettings( ps );

	DBG( DBG_LOW, "ModelSet9636() done.\n" );
}

/*.............................................................................
 * set the model to P12 (ASIC 98003 models)
 */
_LOC void ModelSetP12( pScanData ps )
{
	DBG( DBG_LOW, "ModelSetP12()\n" );

	/*
	 * set to 9630 first, then do the differences
	 */
	ModelSet9630( ps );
	ps->Device.DataOriginX = 72;
    ps->sCaps.Model  = MODEL_OP_PT12;
	ps->sCaps.AsicID = _ASIC_IS_98003;

	ps->TotalBufferRequire = _SizeTotalBufTpa;

    /* do it again, as ModelSet9630() changes the result of this function !*/
	modelInitPageSettings( ps );

	DBG( DBG_LOW, "ModelSetP12() done.\n" );
}

/* END PLUSTEK-PP_MODEL.C ...................................................*/
