/* @file u12_map.c
 * @brief functions to create and manipulate gamma lookup tables.
 *
 * Copyright (C) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - inverting map when scanning binary images
 *        - changed u12map_Adjust()
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

/**
 * @param  s - pointer to the scanner specific structure
 * @return The function always returns SANE_STATUS_GOOD
 */
static SANE_Status u12map_InitGammaSettings( U12_Device *dev )
{
	int    i, j, val;
	double gamma;

	dev->gamma_length      = 4096;
	dev->gamma_range.min   = 0;
	dev->gamma_range.max   = 255;
	dev->gamma_range.quant = 0;

	DBG( _DBG_INFO, "Presetting Gamma tables (len=%u)\n", dev->gamma_length );
	DBG( _DBG_INFO, "----------------------------------\n" );

	/* preset the gamma maps */
	for( i = 0; i < 4; i++ ) {

		switch( i ) {
			case 1:  gamma = dev->adj.rgamma;    break;
			case 2:  gamma = dev->adj.ggamma;    break;
			case 3:  gamma = dev->adj.bgamma;    break;
			default: gamma = dev->adj.graygamma; break;
		}

		for( j = 0; j < dev->gamma_length; j++ ) {

			val = (dev->gamma_range.max *
					    pow((double) j / ((double)dev->gamma_length - 1.0),
						1.0 / gamma ));

			if( val > dev->gamma_range.max )
				val = dev->gamma_range.max;

			dev->gamma_table[i][j] = val;
		}
	}
	return SANE_STATUS_GOOD;
}

/** Check the gamma vectors we got back and limit if necessary
 * @param  s - pointer to the scanner specific structure
 * @return nothing
 */
static void u12map_CheckGammaSettings( U12_Device *dev )
{
	int i, j;

	for( i = 0; i < 4 ; i++ ) {
		for( j = 0; j < dev->gamma_length; j++ ) {
			if( dev->gamma_table[i][j] > dev->gamma_range.max ) {
				dev->gamma_table[i][j] = dev->gamma_range.max;
			}
		}
	}
}

/** adjust acording to brightness and contrast
 */
static void u12map_Adjust( U12_Device *dev, int which, SANE_Byte *buf )
{
	int     i;
	u_long *pdw;
	double  b, c, tmp;

	DBG( _DBG_INFO, "u12map_Adjust(%u)\n", which );

	/* adjust brightness (b) and contrast (c) using the function:
	 *
	 * s'(x,y) = (s(x,y) + b) * c
	 * b = [-127, 127]
	 * c = [0,2]
	 */

	/* scale brightness and contrast...
	 */
	b = ((double)dev->DataInf.siBrightness * 192.0)/100.0;
	c = ((double)dev->DataInf.siContrast   + 100.0)/100.0;

	DBG( _DBG_INFO, "* brightness   = %i -> %i\n",
	                 dev->DataInf.siBrightness, (SANE_Byte)b);
	DBG( _DBG_INFO, "* contrast*100 = %i -> %i\n",
	                 dev->DataInf.siContrast, (int)(c*100));

	for( i = 0; i < dev->gamma_length; i++ ) {

		if((_MAP_MASTER == which) || (_MAP_RED == which)) {
			tmp = ((double)(dev->gamma_table[0][i] + b)) * c;
			if( tmp < 0 )   tmp = 0;
			if( tmp > 255 ) tmp = 255;
			buf[i] = (SANE_Byte)tmp;
		}

		if((_MAP_MASTER == which) || (_MAP_GREEN == which)) {
			tmp = ((double)(dev->gamma_table[1][i] + b)) * c;
			if( tmp < 0 )   tmp = 0;
			if( tmp > 255 ) tmp = 255;
			buf[4096+i] = (SANE_Byte)tmp;
    	}

		if((_MAP_MASTER == which) || (_MAP_BLUE == which)) {
			tmp = ((double)(dev->gamma_table[2][i] + b)) * c;
			if( tmp < 0 )   tmp = 0;
			if( tmp > 255 ) tmp = 255;
			buf[8192+i] = (SANE_Byte)tmp;
		}
	}

	if((dev->DataInf.dwScanFlag & _SCANDEF_Negative) ||
	   (dev->DataInf.wPhyDataType == COLOR_BW)) {
		DBG( _DBG_INFO, "inverting...\n" );

		if((_MAP_MASTER == which) || (_MAP_RED == which)) {

			DBG( _DBG_INFO, "inverting RED map\n" );
			pdw = (u_long*)&buf[0];
		    for( i = dev->gamma_length / 4; i; i--, pdw++ )
				*pdw = ~(*pdw);
		}

		if((_MAP_MASTER == which) || (_MAP_GREEN == which)) {

			DBG( _DBG_INFO, "inverting GREEN map\n" );
			pdw = (u_long*)&buf[4096];
		    for( i = dev->gamma_length / 4; i; i--, pdw++ )
				*pdw = ~(*pdw);
		}

		if((_MAP_MASTER == which) || (_MAP_BLUE == which)) {

			DBG( _DBG_INFO, "inverting BLUE map\n" );
			pdw = (u_long*)&buf[8192];
		    for( i = dev->gamma_length / 4; i; i--, pdw++ )
				*pdw = ~(*pdw);
		}
	}
}

/* END U12-MAP.C ............................................................*/
