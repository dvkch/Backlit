/* @file plustek-pp_map.c
 * @brief functions to create and manipulate lookup tables.
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - brightness and contrast is working (see mapAdjust)
 * - 0.32 - no changes
 * - 0.33 - disabled a few functions
 * - 0.34 - added new dither matrix for checking
 * - 0.35 - no changes
 * - 0.36 - activated Dithermap 1
 *        - removed some unused functions
 *        - added additional SCANDEF_Inverse check to MapSetupDither()
 *        - fixed the double inversion bug, the map always compensates
 *        - the scanner hw-settings
 * - 0.37 - code cleanup
 * - 0.38 - added P12 stuff
 * - 0.39 - no changes
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - made MapAdjust global
 *        - changed include names
 * - 0.43 - cleanup, removed floating point stuff
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

/*WORK:
 *    create other thresholds for the dither maps
 */
static Byte mapDitherMatrix0[_DITHERSIZE] =
{
     0, 32, 8,  40, 2,  34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44, 4,  36, 14, 46, 6,  38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43, 1,  33, 9,  41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47, 7,  39, 13, 45, 5,  37,
	63, 31, 55, 23, 61, 29, 53, 21
};

static Byte mapDitherMatrix1[_DITHERSIZE] =
{
     2, 60, 16, 56,  3, 57, 13, 53,
    34, 18, 48, 32, 35, 19, 45, 29,
    10, 50,  6, 63, 11, 51,  7, 61,
    42, 26, 38, 22, 43, 27, 39, 23,
     4, 58, 14, 54,  1, 59, 15, 55,
    36, 20, 46, 30, 33, 17, 47, 31,
    12, 52,  8, 62,  9, 49,  5, 63,
    44, 28, 40, 24, 41, 25, 37, 21
};

/*************************** local functions *********************************/

/** set the selected dither maps...
 */
static void mapSetDitherMap( pScanData ps )
{
	ULong  i;
	pUChar pDitherSource;
	pUChar pDither = ps->a_bDitherPattern;

	if( 0 == ps->DataInf.wDither ) {
		DBG( DBG_LOW, "Using Dithermatrix 0\n" );
		pDitherSource = mapDitherMatrix0;
	} else {
		DBG( DBG_LOW, "Using Dithermatrix 1\n" );
		pDitherSource = mapDitherMatrix1;
	}

	for( i = 0; i < _DITHERSIZE; i++ ) {
		pDither[i] = pDitherSource[i];
	}
}

/** nothing more to say
 */
static void mapInvertMap( pScanData ps )
{
	pULong pdw;
    ULong  dw, size;

	DBG( DBG_LOW, "mapInvertMap()\n" );

	if( _IS_ASIC98(ps->sCaps.AsicID)) {
		size = 4096;
	} else {
		size = 256;
    }

    for (pdw = (pULong)ps->a_bMapTable, dw = size * 3 / 4; dw; dw--, pdw++) {
		*pdw = ~(*pdw);
	}
}

/** as the name says...
 */
static void mapInvertDitherMap( pScanData ps )
{
	if( ps->DataInf.dwScanFlag & SCANDEF_Inverse ) {

		ULong  dw;
		pULong pDither = (pULong)ps->a_bDitherPattern;

		DBG( DBG_LOW, "mapInvertDitherMap()\n" );

		mapInvertMap( ps );
		for (dw = 0; dw < 16; dw++) {
	    	pDither[dw] = ~pDither[dw];
		}
    }
}

/** build linear map...
 */
static void mapBuildLinearMap( pScanData ps )
{
	ULong i;
		
	DBG( DBG_LOW, "mapBuildLinearMap()\n" );

	if( _IS_ASIC98(ps->sCaps.AsicID)) {

		for( i = 0; i < 4096; i++ ) {
			ps->a_bMapTable[i] 	    = (UChar)(i >> 4);		
			ps->a_bMapTable[4096+i] = (UChar)(i >> 4);		
			ps->a_bMapTable[8192+i] = (UChar)(i >> 4);		
		}

	} else {

		for( i = 0; i < 256; i++ ) {
			ps->a_bMapTable[i] 	   = (UChar)(i & 0xff);		
			ps->a_bMapTable[256+i] = (UChar)(i & 0xff);		
			ps->a_bMapTable[512+i] = (UChar)(i & 0xff);	
		}
	}
}

/************************ exported functions *********************************/

/** create a mapping table
 *  in the original code this map will be created by the TWAIN application
 *  and the is being downloaded to the driver, as I don't have the code
 *  we have to try to build up such a table here
 */
_LOC void MapInitialize( pScanData ps )
{
	mapBuildLinearMap( ps );
	MapAdjust( ps, _MAP_MASTER );
}

/** setup dither maps
 */
_LOC void MapSetupDither( pScanData ps )
{
	DBG( DBG_LOW, "MapSetupDither() - %u\n", ps->DataInf.wAppDataType );

	if( COLOR_HALFTONE == ps->DataInf.wAppDataType ) {

		mapSetDitherMap( ps );
	    if (ps->DataInf.dwScanFlag & SCANDEF_Inverse)
			mapInvertDitherMap( ps );
	}
}

/** adjust acording to brightness and contrast
 */
_LOC void MapAdjust( pScanData ps, int which )
{
	ULong  i, tabLen, dw;
	ULong *pdw;
	long   b, c, tmp;

	DBG( DBG_LOW, "MapAdjust(%u)\n", which );
		
	if( _IS_ASIC98(ps->sCaps.AsicID)) {
		tabLen = 4096;
	} else {
		tabLen = 256;
	}

	/* adjust brightness (b) and contrast (c) using the function:
	 * s'(x,y) = (s(x,y) + b) * c
	 * b = [-127, 127]
	 * c = [0,2]
	 */

	/* scale brightness and contrast...
	 */
	b = ps->wBrightness * 192;
	c = ps->wContrast   + 100;

	DBG( DBG_LOW, "brightness   = %i -> %i\n", ps->wBrightness, (UChar)(b/100));
	DBG( DBG_LOW, "contrast*100 = %i -> %i\n", ps->wContrast, (int)(c));

	for( i = 0; i < tabLen; i++ ) {

		if((_MAP_MASTER == which) || (_MAP_RED == which)) {
			tmp = ((((long)ps->a_bMapTable[i] * 100) + b) *c ) / 10000;
			if( tmp < 0 )   tmp = 0;
			if( tmp > 255 ) tmp = 255;
			ps->a_bMapTable[i] = (UChar)tmp;
		}

		if((_MAP_MASTER == which) || (_MAP_GREEN == which)) {
			tmp = ((((long)ps->a_bMapTable[tabLen+i] * 100) + b) * c) / 10000;
			if( tmp < 0 )   tmp = 0;
			if( tmp > 255 ) tmp = 255;
			ps->a_bMapTable[tabLen+i] = (UChar)tmp;
    	}
    	
		if((_MAP_MASTER == which) || (_MAP_BLUE == which)) {
			tmp = ((((long)ps->a_bMapTable[tabLen*2+i] * 100) + b) * c) / 10000;
			if( tmp < 0 )   tmp = 0;
			if( tmp > 255 ) tmp = 255;
			ps->a_bMapTable[tabLen*2+i] = (UChar)tmp;
		}
	}

    if( ps->DataInf.dwScanFlag & SCANDEF_Negative ) {
		DBG( DBG_LOW, "inverting...\n" );
		
		if((_MAP_MASTER == which) || (_MAP_RED == which)) {
	
			DBG( DBG_LOW, "inverting RED map\n" );
			
			pdw = (pULong)ps->a_bMapTable;
			
		    for( dw = tabLen / 4; dw; dw--, pdw++ )
				*pdw = ~(*pdw);
    	}
	
		if((_MAP_MASTER == which) || (_MAP_GREEN == which)) {
			
			DBG( DBG_LOW, "inverting GREEN map\n" );
			
			pdw = (pULong)&ps->a_bMapTable[tabLen];
			
		    for( dw = tabLen / 4; dw; dw--, pdw++ )
				*pdw = ~(*pdw);
    	}
		
		if((_MAP_MASTER == which) || (_MAP_BLUE == which)) {
			
			DBG( DBG_LOW, "inverting BLUE map\n" );
			
			pdw = (pULong)&ps->a_bMapTable[tabLen*2];
			
		    for( dw = tabLen / 4; dw; dw--, pdw++ )
				*pdw = ~(*pdw);
    	}
	}
}

/* END PLUSTEK-PP_MAP.C .....................................................*/
