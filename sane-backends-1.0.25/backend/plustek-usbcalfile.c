/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbcalfile.c
 *  @brief Functions for saving/restoring calibration settings
 *
 * Copyright (C) 2001-2007 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.46 - first version
 * - 0.47 - no changes
 * - 0.48 - no changes
 * - 0.49 - a_bRegs is now part of the device structure
 * - 0.50 - cleanup
 * - 0.51 - added functions for saving, reading and restoring
 *          fine calibration data
 * - 0.52 - no changes
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

typedef struct {
	u_long red_light_on;
	u_long red_light_off;
	u_long green_light_on;
	u_long green_light_off;
	u_long blue_light_on;
	u_long blue_light_off;
	u_long green_pwm_duty;
	
} LightCtrl;

typedef struct {
	u_short   version;

	u_short   red_gain;
	u_short   green_gain;
	u_short   blue_gain;

	u_short   red_offs;
	u_short   green_offs;
	u_short   blue_offs;

	LightCtrl light;

} CalData;

/* our shading buffers */
static u_short a_wWhiteShading[_SHADING_BUF] = {0};
static u_short a_wDarkShading[_SHADING_BUF]  = {0};

/* the version the calibration files */
#define _PT_CF_VERSION 0x0002

/** function to read a text file and returns the string which starts which
 *  'id' string.
 *  no duplicate entries where detected, always the first occurance will be
 *  red.
 * @param fp  - file pointer of file to read
 * @param id  - what to search for
 * @param res - where to store the result upon success
 * @return SANE_TRUE on success, SANE_FALSE on any error
 */
static SANE_Bool
usb_ReadSpecLine( FILE *fp, char *id, char* res )
{
	char  tmp[1024];
	char *ptr;
	
	/* rewind file pointer */
	if( 0 != fseek( fp, 0L, SEEK_SET)) {
		DBG( _DBG_ERROR, "fseek: %s\n", strerror(errno));
		return SANE_FALSE;
	}

	/* roam through the file and examine each line... */
	while( !feof( fp )) {

		memset( tmp, 0, sizeof(tmp));
		if( NULL != fgets( tmp, 1024, fp )) {

			if( 0 == strncmp( tmp, id, strlen(id))) {

				ptr = &tmp[strlen(id)];
        			if( '\0' == *ptr )
					break;
					
				strcpy( res, ptr );
				res[strlen(res)-1] = '\0';
				return SANE_TRUE;
			}
		}
	}
	return SANE_FALSE;
}

/** function to read data from a file and excluding certain stuff like
 *  the version lines
 * @param fp      - file pointer of file to read
 * @param except  - what to exclude
 * @return Pointer to the allocated memory for the data, NULL on any error.
 */
static char*
usb_ReadOtherLines( FILE *fp, char *except )
{
	char  tmp[1024];
	char *ptr, *ptr_base;
	int   ignore;
	int   len;

	if( 0 != fseek( fp, 0L, SEEK_END))
		return NULL;

	len = ftell(fp);

	/* rewind file pointer */
	if( 0 != fseek( fp, 0L, SEEK_SET))
		return NULL;

	if( len == 0 )
		return NULL;

	ptr = (char*)malloc(len);
	if( NULL == ptr )
		return NULL;

	ptr_base = ptr;
	*ptr     = '\0';
	ignore   = 0;

	/* roam through the file and examine each line... */
	while( !feof( fp )) {

		if( NULL != fgets( tmp, 1024, fp )) {

			/* we ignore the version line... */
			if( 0 == strncmp( tmp, "version=", 8 ))
				continue;
	
			if( !ignore ) {
				if(0 != strncmp(tmp, except, strlen(except))) {

					if( strlen( tmp ) > 0 ) {
						strcpy( ptr, tmp );
						ptr += strlen(tmp);
						*ptr = '\0';
					}
				} else {
					ignore = 1;
				}
			}

			/* newline in tmp string resets ignore flag */
			if( strrchr(tmp, '\n')) {
				ignore = 0;
			}
		}
	}
	return ptr_base;
}

/**
 */
static SANE_Bool
usb_ReadSamples( FILE *fp, char *which, u_long *dim, u_short *buffer )
{
	char  *p, *next, *rb;
	char   tmp[1024+30];
	int    ignore, diml, c;
	u_long val;

	/* rewind file pointer */
	if( 0 != fseek( fp, 0L, SEEK_SET))
		return SANE_FALSE;

	ignore = 0;
	diml   = 0;
	c      = 0;
	rb     = tmp;
	*dim   = 0;

	/* roam through the file and examine each line... */
	while( !feof( fp )) {

		if( NULL != fgets( rb, 1024, fp )) {

			/* we ignore the version line... */
			if( 0 == strncmp( tmp, "version=", 8 )) 
				continue;
	
			p = tmp;
			if( !ignore && diml == 0) {
				if(0 == strncmp(tmp, which, strlen(which))) {

					/* get dimension */
					diml = strtol(&tmp[strlen(which)], NULL, 10);
					p = strchr( &tmp[strlen(which)], ':' );
					p++;
				} else {
					ignore = 1;
				}
			}

			/* parse the values... */
			if( !ignore ) {

				rb = tmp;
				while( *p ) {
					val = strtoul( p, &next, 10 );

					/* check for error condition */
					if( val == 0 ) {
						if( p == next ) {
							if( c+1 == diml ) {
								*dim = diml;
								return SANE_TRUE;
							}
							break;
						}
					}

					buffer[c] = (u_short)val;

					/* more values? */
					if( *next == ',') {
						p = next+1;
						c++;
					} else {
						p = next;
					}
					/* reached the end? */
					if( *next == '\0' ) {

						/* we probably have only parsed a part of a value 
						 * so we copy that back to the input buffer and
						 * parse it the next time...
						 */
						if( c < diml ) {
							sprintf( tmp, "%u", buffer[c] );
							rb = &tmp[strlen(tmp)];
						}
					}
				}
			}

			/* newline in tmp string resets ignore flag */
			if( strrchr(tmp, '\n')) {
				ignore = 0;
			}
		}
	}
	return SANE_FALSE;
}

/**
 */
static void
usb_RestoreCalData( Plustek_Device *dev, CalData *cal )
{
	HWDef  *hw   = &dev->usbDev.HwSetting;
	u_char *regs = dev->usbDev.a_bRegs;

	regs[0x3b] = (u_char)cal->red_gain;
	regs[0x3c] = (u_char)cal->green_gain;
	regs[0x3d] = (u_char)cal->blue_gain;
	regs[0x38] = (u_char)cal->red_offs;
	regs[0x39] = (u_char)cal->green_offs;
	regs[0x3a] = (u_char)cal->blue_offs;

	regs[0x2a] = _HIBYTE((u_short)cal->light.green_pwm_duty);
	regs[0x2b] = _LOBYTE((u_short)cal->light.green_pwm_duty);

	regs[0x2c] = _HIBYTE((u_short)cal->light.red_light_on);
	regs[0x2d] = _LOBYTE((u_short)cal->light.red_light_on);
	regs[0x2e] = _HIBYTE((u_short)cal->light.red_light_off);
	regs[0x2f] = _LOBYTE((u_short)cal->light.red_light_off);

	regs[0x30] = _HIBYTE((u_short)cal->light.green_light_on);
	regs[0x31] = _LOBYTE((u_short)cal->light.green_light_on);
	regs[0x32] = _HIBYTE((u_short)cal->light.green_light_off);
	regs[0x33] = _LOBYTE((u_short)cal->light.green_light_off);

	regs[0x34] = _HIBYTE((u_short)cal->light.blue_light_on);
	regs[0x35] = _LOBYTE((u_short)cal->light.blue_light_on);
	regs[0x36] = _HIBYTE((u_short)cal->light.blue_light_off);
	regs[0x37] = _LOBYTE((u_short)cal->light.blue_light_off);

	hw->red_lamp_on    = (u_short)cal->light.red_light_on;
	hw->red_lamp_off   = (u_short)cal->light.red_light_off;
	hw->green_lamp_on  = (u_short)cal->light.green_light_on;
	hw->green_lamp_off = (u_short)cal->light.green_light_off;
	hw->blue_lamp_on   = (u_short)cal->light.blue_light_on;
	hw->blue_lamp_off  = (u_short)cal->light.blue_light_off;
}

/**
 */
static void
usb_CreatePrefix( Plustek_Device *dev, char *pfx, SANE_Bool add_bitdepth )
{
	char       bd[5];
	ScanDef   *scanning = &dev->scanning;
	ScanParam *param    = &scanning->sParam;

	switch( scanning->sParam.bSource ) {

		case SOURCE_Transparency: strcpy( pfx, "tpa-" ); break;
		case SOURCE_Negative:     strcpy( pfx, "neg-" ); break;
		case SOURCE_ADF:          strcpy( pfx, "adf-" ); break;
	    default:                  pfx[0] = '\0'; break;
	}

	sprintf( bd, "%u=", param->bBitDepth );
	if( param->bDataType == SCANDATATYPE_Color )
		strcat( pfx, "color" );
	else
		strcat( pfx, "gray" );

	if( add_bitdepth )
		strcat( pfx, bd );
}

/** function to read and set the calibration data from external file
 */
static SANE_Bool
usb_ReadAndSetCalData( Plustek_Device *dev )
{
	char       pfx[20];
	char       tmp[1024];
	u_short    version;
	int        res;
	FILE      *fp;
	CalData    cal;
	SANE_Bool  ret;
	
	DBG( _DBG_INFO, "usb_ReadAndSetCalData()\n" );

	if( usb_InCalibrationMode(dev)) {
		DBG( _DBG_INFO, "- we are in calibration mode!\n" );
		return SANE_FALSE;
	}

	if( NULL == dev->calFile ) {
		DBG( _DBG_ERROR, "- No calibration filename set!\n" );
		return SANE_FALSE;
	}

	sprintf( tmp, "%s-coarse.cal", dev->calFile );
	DBG( _DBG_INFO, "- Reading coarse calibration data from file\n");
	DBG( _DBG_INFO, "  %s\n", tmp );
	
	fp = fopen( tmp, "r" );
	if( NULL == fp ) {
		DBG( _DBG_ERROR, "File %s not found\n", tmp );
		return SANE_FALSE;
	}

	/* check version */
	if( !usb_ReadSpecLine( fp, "version=", tmp )) {
		DBG( _DBG_ERROR, "Could not find version info!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	DBG( _DBG_INFO, "- Calibration file version: %s\n", tmp );
	if( 1 != sscanf( tmp, "0x%04hx", &version )) {
		DBG( _DBG_ERROR, "Could not decode version info!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	if( version != _PT_CF_VERSION ) {
		DBG( _DBG_ERROR, "Versions do not match!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	usb_CreatePrefix( dev, pfx, SANE_TRUE );
	
	ret = SANE_FALSE;
	if( usb_ReadSpecLine( fp, pfx, tmp )) {
		DBG( _DBG_INFO, "- Calibration data: %s\n", tmp );

		res = sscanf( tmp, "%hu,%hu,%hu,%hu,%hu,%hu,"
						   "%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
						&cal.red_gain,   &cal.red_offs,
						&cal.green_gain, &cal.green_offs,
						&cal.blue_gain,  &cal.blue_offs,
						&cal.light.red_light_on,   &cal.light.red_light_off,
						&cal.light.green_light_on, &cal.light.green_light_off,
						&cal.light.blue_light_on,  &cal.light.blue_light_off,
						&cal.light.green_pwm_duty );

		if( 13 == res ) {
			usb_RestoreCalData( dev, &cal );
			ret = SANE_TRUE;
		} else {
			DBG( _DBG_ERROR, "Error reading coarse-calibration data, only "
			                 "%d elements available!\n", res );
		}
	} else {
		DBG( _DBG_ERROR, "Error reading coarse-calibration data for "
		                 "PFX: >%s<!\n", pfx );
	}

	fclose( fp );
	DBG( _DBG_INFO, "usb_ReadAndSetCalData() done -> %u\n", ret );
		
	return ret;
}

/**
 */
static void
usb_PrepCalData( Plustek_Device *dev, CalData *cal )
{
	u_char *regs = dev->usbDev.a_bRegs;

	memset( cal, 0, sizeof(CalData));
	cal->version = _PT_CF_VERSION;

	cal->red_gain   = (u_short)regs[0x3b];
	cal->green_gain = (u_short)regs[0x3c];
	cal->blue_gain  = (u_short)regs[0x3d];
	cal->red_offs   = (u_short)regs[0x38];
	cal->green_offs = (u_short)regs[0x39];
	cal->blue_offs  = (u_short)regs[0x3a];

	cal->light.green_pwm_duty  = regs[0x2a] * 256 + regs[0x2b];

	cal->light.red_light_on    = regs[0x2c] * 256 + regs[0x2d];
	cal->light.red_light_off   = regs[0x2e] * 256 + regs[0x2f];
	cal->light.green_light_on  = regs[0x30] * 256 + regs[0x31];
	cal->light.green_light_off = regs[0x32] * 256 + regs[0x33];
	cal->light.blue_light_on   = regs[0x34] * 256 + regs[0x35];
	cal->light.blue_light_off  = regs[0x36] * 256 + regs[0x37];
}

/** function to save/update the calibration data
 */
static void
usb_SaveCalData( Plustek_Device *dev )
{
	char       pfx[20];
	char       fn[1024];
	char       tmp[1024];
	char       set_tmp[1024];
	char      *other_tmp;
	u_short    version;
	FILE      *fp;
	CalData    cal;
	ScanDef   *scanning = &dev->scanning;

	DBG( _DBG_INFO, "usb_SaveCalData()\n" );

	/* no new data, so skip this step too */
	if( SANE_TRUE == scanning->skipCoarseCalib ) {
		DBG( _DBG_INFO, "- No calibration data to save!\n" );
		return;
	}

	if( NULL == dev->calFile ) {
		DBG( _DBG_ERROR, "- No calibration filename set!\n" );
		return;
	}

	sprintf( fn, "%s-coarse.cal", dev->calFile );
	DBG( _DBG_INFO, "- Saving coarse calibration data to file\n" );
	DBG( _DBG_INFO, "  %s\n", fn );

	usb_PrepCalData ( dev, &cal );
	usb_CreatePrefix( dev, pfx, SANE_TRUE );
	DBG( _DBG_INFO2, "- PFX: >%s<\n", pfx );

	sprintf( set_tmp, "%s%u,%u,%u,%u,%u,%u,"
	                  "%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", pfx,
	                  cal.red_gain,   cal.red_offs,
	                  cal.green_gain, cal.green_offs,
	                  cal.blue_gain,  cal.blue_offs,
	                  cal.light.red_light_on,   cal.light.red_light_off,
	                  cal.light.green_light_on, cal.light.green_light_off,
	                  cal.light.blue_light_on,  cal.light.blue_light_off,
	                  cal.light.green_pwm_duty );

	/* read complete old file if compatible... */
	other_tmp = NULL;
	fp = fopen( fn, "r+" );
	if( NULL != fp ) {
	
		if( usb_ReadSpecLine( fp, "version=", tmp )) {
			DBG( _DBG_INFO, "- Calibration file version: %s\n", tmp );

			if( 1 == sscanf( tmp, "0x%04hx", &version )) {

				if( version == cal.version ) {

					DBG( _DBG_INFO, "- Versions do match\n" );

					/* read the rest... */
					other_tmp = usb_ReadOtherLines( fp, pfx );
				} else {
					DBG( _DBG_INFO2, "- Versions do not match (0x%04x)\n", version );
				}
			} else {
				DBG( _DBG_INFO2, "- cannot decode version\n" );
			}
		} else {
			DBG( _DBG_INFO2, "- Version not found\n" );
		}
		fclose( fp );
	}
	fp = fopen( fn, "w+" );
	if( NULL == fp ) {
		DBG( _DBG_ERROR, "- Cannot create file %s\n", fn );
		DBG( _DBG_ERROR, "- -> %s\n", strerror(errno));
		if( other_tmp ) 
			free( other_tmp );
		return;
	}

	/* rewrite the file again... */
	fprintf( fp, "version=0x%04X\n", cal.version );
	if( strlen( set_tmp ))
		fprintf( fp, "%s", set_tmp );

	if( other_tmp ) {
		fprintf( fp, "%s", other_tmp );
		free( other_tmp );
	}
	fclose( fp );
	DBG( _DBG_INFO, "usb_SaveCalData() done.\n" );
}

/**
 */
static void
usb_SaveFineCalData( Plustek_Device *dev, int dpi,
                     u_short *dark, u_short *white, u_long vals )
{
	char     pfx[30];
	char     fn[1024];
	char     tmp[1024];
	char    *other_tmp;
	u_short  version;
	u_long   i;
	FILE    *fp;

	if( NULL == dev->calFile ) {
		DBG( _DBG_ERROR, "- No calibration filename set!\n" );
		return;
	}

	sprintf( fn, "%s-fine.cal", dev->calFile );
	DBG( _DBG_INFO, "- Saving fine calibration data to file\n" );
	DBG( _DBG_INFO, "  %s\n", fn );

	usb_CreatePrefix( dev, pfx, SANE_FALSE );
	sprintf( tmp, "%s:%u", pfx, dpi );
	strcpy( pfx, tmp );
	DBG( _DBG_INFO2, "- PFX: >%s<\n", pfx );

	/* read complete old file if compatible... */
	other_tmp = NULL;
	fp = fopen( fn, "r+" );
	if( NULL != fp ) {
	
		if( usb_ReadSpecLine( fp, "version=", tmp )) {
			DBG( _DBG_INFO, "- Calibration file version: %s\n", tmp );

			if( 1 == sscanf( tmp, "0x%04hx", &version )) {

				if( version == _PT_CF_VERSION ) {
					DBG( _DBG_INFO, "- Versions do match\n" );

					/* read the rest... */
					other_tmp = usb_ReadOtherLines( fp, pfx );
				} else {
					DBG( _DBG_INFO2, "- Versions do not match (0x%04x)\n", version );
				}
			} else {
				DBG( _DBG_INFO2, "- cannot decode version\n" );
			}
		} else {
			DBG( _DBG_INFO2, "- Version not found\n" );
		}
		fclose( fp );
	}

	fp = fopen( fn, "w+" );
	if( NULL == fp ) {
		DBG( _DBG_ERROR, "- Cannot create file %s\n", fn );
		return;
	}

	/* rewrite the file again... */
	fprintf( fp, "version=0x%04X\n", _PT_CF_VERSION );

	if( other_tmp ) {
		fprintf( fp, "%s", other_tmp );
		free( other_tmp );
	}

	fprintf( fp, "%s:dark:dim=%lu:", pfx, vals );
	for( i=0; i<vals-1; i++ )
		fprintf( fp, "%u,", dark[i]);
	fprintf( fp, "%u\n", dark[vals-1]);

	fprintf( fp, "%s:white:dim=%lu:", pfx, vals );
	for( i=0; i<vals-1; i++ )
		fprintf( fp, "%u,", white[i]);
	fprintf( fp, "%u\n", white[vals-1]);

	fclose( fp );
}

/** function to read and set the calibration data from external file
 */
static SANE_Bool
usb_ReadFineCalData( Plustek_Device *dev, int dpi,
                     u_long *dim_d, u_short *dark,
                     u_long *dim_w, u_short *white )
{
	char       pfx[30];
	char       tmp[1024];
	u_short    version;
	FILE      *fp;
	
	DBG( _DBG_INFO, "usb_ReadFineCalData()\n" );
	if( usb_InCalibrationMode(dev)) {
		DBG( _DBG_INFO, "- we are in calibration mode!\n" );
		return SANE_FALSE;
	}

	if( NULL == dev->calFile ) {
		DBG( _DBG_ERROR, "- No calibration filename set!\n" );
		return SANE_FALSE;
	}

	sprintf( tmp, "%s-fine.cal", dev->calFile );
	DBG( _DBG_INFO, "- Reading fine calibration data from file\n");
	DBG( _DBG_INFO, "  %s\n", tmp );

	*dim_d = *dim_w = 0;

	fp = fopen( tmp, "r" );
	if( NULL == fp ) {
		DBG( _DBG_ERROR, "File %s not found\n", tmp );
		return SANE_FALSE;
	}

	/* check version */
	if( !usb_ReadSpecLine( fp, "version=", tmp )) {
		DBG( _DBG_ERROR, "Could not find version info!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	DBG( _DBG_INFO, "- Calibration file version: %s\n", tmp );
	if( 1 != sscanf( tmp, "0x%04hx", &version )) {
		DBG( _DBG_ERROR, "Could not decode version info!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	if( version != _PT_CF_VERSION ) {
		DBG( _DBG_ERROR, "Versions do not match!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	usb_CreatePrefix( dev, pfx, SANE_FALSE );

	sprintf( tmp, "%s:%u:%s:dim=", pfx, dpi, "dark" );
	if( !usb_ReadSamples( fp, tmp, dim_d, dark )) {
		DBG( _DBG_ERROR, "Error reading dark-calibration data!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	sprintf( tmp, "%s:%u:%s:dim=", pfx, dpi, "white" );
	if( !usb_ReadSamples( fp, tmp, dim_w, white )) {
		DBG( _DBG_ERROR, "Error reading white-calibration data!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	fclose( fp );
	return SANE_TRUE;
}

/**
 */
static void
usb_get_shading_part(u_short *buf, u_long offs, u_long src_len, int dst_len)
{
	u_short *p_src, *p_dst;
	int      i, j;

	if (src_len == 0 || dst_len == 0)
		return;

	p_dst = buf;
	for (i=0; i<3; i++) {

		p_src = buf + src_len * i + offs;

		for (j=0; j<dst_len; j++) {

			*(p_dst++) = *(p_src++);
		}
	}
}

/** function to read the fine calibration results from file
 * and to set the correct part of the calibration buffers for
 * storing in the device
 * @param dev - the almigthy device structure
 * @returns SANE_FALSE when the reading fails or SANE_TRUE on success
 */
static SANE_Bool
usb_FineShadingFromFile( Plustek_Device *dev )
{
	ScanDef   *scan = &dev->scanning;
	ScanParam *sp   = &scan->sParam;
	u_short    xdpi;
	u_long     dim_w, dim_d, offs;

	xdpi = usb_SetAsicDpiX( dev, sp->UserDpi.x );

	if( !usb_ReadFineCalData( dev, xdpi, &dim_d, a_wDarkShading,
	                                     &dim_w, a_wWhiteShading)) {
		return SANE_FALSE;
	}

	/* now we need to get the correct part of the line... */
	dim_d /= 3;
	dim_w /= 3;

	offs = ((u_long)sp->Origin.x * xdpi) / 300;

	usb_GetPhyPixels( dev, sp );

	DBG( _DBG_INFO2, "FINE Calibration from file:\n" );
	DBG( _DBG_INFO2, "XDPI      = %u\n",  xdpi   );
	DBG( _DBG_INFO2, "Dim       = %lu\n", dim_d  );
	DBG( _DBG_INFO2, "Pixels    = %lu\n", sp->Size.dwPixels );
	DBG( _DBG_INFO2, "PhyPixels = %lu\n", sp->Size.dwPhyPixels );
	DBG( _DBG_INFO2, "Origin.X  = %u\n",  sp->Origin.x );
	DBG( _DBG_INFO2, "Offset    = %lu\n", offs );

	usb_get_shading_part(a_wDarkShading,  offs, dim_d, sp->Size.dwPhyPixels);
	usb_get_shading_part(a_wWhiteShading, offs, dim_w, sp->Size.dwPhyPixels);

	return SANE_TRUE;
}

/** function to save the fine calibration results and to set the correct part
 * of the calibration buffers for storing in the device
 * @param dev    - the almigthy device structure
 * @param tmp_sp - intermediate scan parameter
 */
static void
usb_SaveCalSetShading( Plustek_Device *dev, ScanParam *tmp_sp )
{
	ScanParam *sp = &dev->scanning.sParam;
	u_short    xdpi;
	u_long     offs;

	if( !dev->adj.cacheCalData )
		return;

	/* save the values */
	xdpi = usb_SetAsicDpiX( dev, tmp_sp->UserDpi.x );

	usb_SaveFineCalData( dev, xdpi, a_wDarkShading,
	                     a_wWhiteShading, tmp_sp->Size.dwPixels*3 );

	/* now we need to get the correct part of the line... */
	xdpi = usb_SetAsicDpiX( dev, sp->UserDpi.x );
	offs = ((u_long)sp->Origin.x * xdpi) / 300;
	usb_GetPhyPixels( dev, sp );

	DBG( _DBG_INFO2, "FINE Calibration area after saving:\n" );
	DBG( _DBG_INFO2, "XDPI      = %u\n",  xdpi );
	DBG( _DBG_INFO2, "Dim       = %lu\n", tmp_sp->Size.dwPixels );
	DBG( _DBG_INFO2, "Pixels    = %lu\n", sp->Size.dwPixels );
	DBG( _DBG_INFO2, "PhyPixels = %lu\n", sp->Size.dwPhyPixels );
	DBG( _DBG_INFO2, "Origin.X  = %u\n",  sp->Origin.x );
	DBG( _DBG_INFO2, "Offset    = %lu\n", offs );

	if (!usb_InCalibrationMode(dev)) {

		usb_get_shading_part( a_wDarkShading, offs,
		                      tmp_sp->Size.dwPixels, sp->Size.dwPhyPixels );
		usb_get_shading_part( a_wWhiteShading, offs,
	    	                  tmp_sp->Size.dwPixels, sp->Size.dwPhyPixels );

		memcpy( tmp_sp, sp, sizeof(ScanParam));
		tmp_sp->bBitDepth = 16;

		usb_GetPhyPixels( dev, tmp_sp );
	}
}

/* END PLUSTEK-USBCALFILE.C .................................................*/
