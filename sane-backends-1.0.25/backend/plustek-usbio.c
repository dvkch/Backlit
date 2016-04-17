/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbio.c
 *  @brief Some I/O stuff.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2007 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - moved some functions to a sane library (sanei_lm983x.c)
 * - 0.42 - no changes
 * - 0.43 - no changes
 * - 0.44 - added dump registers and dumpPic functions
 *        - beautyfied output of ASIC detection
 * - 0.45 - fixed dumpRegs
 *        - added dimension stuff to dumpPic
 * - 0.46 - disabled reset prior to the detection of Merlin
 * - 0.47 - no changes
 * - 0.48 - cleanup
 * - 0.49 - no changes
 * - 0.50 - usbio_DetectLM983x() now returns error if register 
 *          could not be red
 *        - usbio_ResetLM983x() checks for reg7 value before writing
 * - 0.51 - allow dumpRegs to be called without valid fd
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
 
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_lm983x.h"

#define _UIO(func)                            \
    {                                         \
        SANE_Status status;                   \
        status = func;                        \
        if (status != SANE_STATUS_GOOD) {     \
            DBG( _DBG_ERROR, "UIO error\n" ); \
            return SANE_FALSE;                \
        }                                     \
    }

#define usbio_ReadReg(fd, reg, value) \
  sanei_lm983x_read (fd, reg, value, 1, 0)

typedef struct {

	u_char depth;
	u_long x;
	u_long y;
} PicDef, *pPicDef;

static PicDef dPix;

/**
 */
static void dumpPic( char* name, SANE_Byte *buffer, u_long len, int is_gray )
{
	u_short type;
	FILE   *fp;

	if( DBG_LEVEL < _DBG_DPIC )
		return;

	if( NULL == buffer ) {

		DBG( _DBG_DPIC, "Creating file '%s'\n", name );

		fp = fopen( name, "w+b" );

		if( NULL != fp ) {

			if( 0 != dPix.x ) {

				if (is_gray)
					type = 5;
				else
					type = 6;

				DBG( _DBG_DPIC, "> X=%lu, Y=%lu, depth=%u\n",
				                 dPix.x, dPix.y, dPix.depth );
				if( dPix.depth > 8 )
					fprintf( fp, "P%u\n%lu %lu\n65535\n", type, dPix.x, dPix.y);
    			else
					fprintf( fp, "P%u\n%lu %lu\n255\n", type, dPix.x, dPix.y);
			}
    	}
	} else {
		fp = fopen( name, "a+b" );
	}

	if( NULL == fp ) {
		DBG( _DBG_DPIC, "Can not open file '%s'\n", name );
		return;
	}

	fwrite( buffer, 1, len, fp );
	fclose( fp );
}

/**
 */
static void dumpPicInit( ScanParam *sd, char* name )
{
	dPix.x = sd->Size.dwPhyBytes;

	if( sd->bDataType == SCANDATATYPE_Color )
		dPix.x /= 3;

	if( sd->bBitDepth > 8 )
		dPix.x /= 2;

	dPix.y	   = sd->Size.dwLines;
	dPix.depth = sd->bBitDepth;

	if( sd->bDataType == SCANDATATYPE_Color )
		dumpPic(name, NULL, 0, 0);
	else
		dumpPic(name, NULL, 0, 1);
}

/**
 * dump the LM983x registers
 */
static void dumpregs( int fd, SANE_Byte *cmp )
{
	char      buf[256], b2[10];
	SANE_Byte regs[0x80];
	int       i;

	if( DBG_LEVEL < _DBG_DREGS )
		return;

	buf[0] = '\0';

	if( fd >= 0 ) {
		usbio_ReadReg(fd, 0x01, &regs[0x01]);
		usbio_ReadReg(fd, 0x02, &regs[0x02]);
		usbio_ReadReg(fd, 0x03, &regs[0x03]);
		usbio_ReadReg(fd, 0x04, &regs[0x04]);
		usbio_ReadReg(fd, 0x07, &regs[0x07]);

		sanei_lm983x_read( fd, 0x08, &regs[0x8], 0x80-0x8, SANE_TRUE );

		for( i = 0x0; i < 0x80; i++ ) {

			if((i%16) ==0 ) {

				if( buf[0] )
					DBG( _DBG_DREGS, "%s\n", buf );
				sprintf( buf, "0x%02x:", i );
			}

			if((i%8)==0)
				strcat( buf, " ");

			/* the dataport read returns with "0 Bytes read", of course. */
			if((i == 0) || (i == 5) || (i == 6))
				strcat( buf, "XX ");
			else {

				sprintf( b2, "%02x ", regs[i]);
				strcat( buf, b2 );
			}
		}
		DBG( _DBG_DREGS, "%s\n", buf );
	}

	if( cmp ) {

		buf[0] = '\0';

		DBG( _DBG_DREGS, "Internal setting:\n" );
		for( i = 0x0; i < 0x80; i++ ) {

			if((i%16) ==0 ) {

				if( buf[0] )
					DBG( _DBG_DREGS, "%s\n", buf );
				sprintf( buf, "0x%02x:", i );
			}

			if((i%8)==0)
				strcat( buf, " ");

			if((i == 0) || (i == 5) || (i == 6))
				strcat( buf, "XX ");
			else {
				sprintf( b2, "%02x ", cmp[i]);
				strcat( buf, b2 );
			}
		}
		DBG( _DBG_DREGS, "%s\n", buf );
	}
}

/**
 * function to read the contents of a LM983x register and regarding some
 * extra stuff, like flushing register 2 when writing register 0x58, etc
 *
 * @param handle -
 * @param reg    -
 * @param value  -
 * @return
 */
static SANE_Bool usbio_WriteReg( SANE_Int handle,
                                 SANE_Byte reg, SANE_Byte value )
{
	int       i;
	SANE_Byte data;

	/* retry loop... */
	for( i = 0; i < 100; i++ ) {

		sanei_lm983x_write_byte( handle, reg, value );
	
		/* Flush register 0x02 when register 0x58 is written */
		if( 0x58 == reg ) {
			_UIO( usbio_ReadReg( handle, 2, &data ));
			_UIO( usbio_ReadReg( handle, 2, &data ));
		}

		if( reg != 7 )
			return SANE_TRUE;

		/* verify register 7 */
		_UIO( usbio_ReadReg( handle, 7, &data ));
		if( data == value ) {
			return SANE_TRUE;
		}
	}

	return SANE_FALSE;
}

/** try and read register 0x69 from a LM983x to find out which version we have.
 */
static SANE_Status usbio_DetectLM983x( SANE_Int fd, SANE_Byte *version )
{
	char        buf[256];
	SANE_Byte   value;
	SANE_Status res;

	DBG( _DBG_INFO, "usbio_DetectLM983x\n");

	res = usbio_ReadReg(fd, 0x69, &value);
	if( res != SANE_STATUS_GOOD ) {
		DBG( _DBG_ERROR, " * could not read version register!\n");
		return res;
	}

	value &= 7;
	if (version)
		*version = value;

	res = SANE_STATUS_GOOD;

	sprintf( buf, "usbio_DetectLM983x: found " );

	switch((SANE_Int)value ) {
		
		case 4:	 strcat( buf, "LM9832/3" ); break;
		case 3:	 strcat( buf, "LM9831" );   break;
		case 2:	 strcat( buf, "LM9830 --> unsupported!!!" );
				 res =  SANE_STATUS_INVAL;
				 break;
		default: DBG( _DBG_INFO, "Unknown chip v%d", value );
				 res = SANE_STATUS_INVAL;
				 break;
	}

	DBG( _DBG_INFO, "%s\n", buf );
	return res;
}

/** well, this is more or less a reset function, for LM9831 based devices
 * we issue a real reset command, while for LM9832/3 based devices, checking
 * and resetting register 7 will be enough...
 */
static SANE_Status usbio_ResetLM983x( Plustek_Device *dev )
{
	SANE_Byte value;
	HWDef    *hw = &dev->usbDev.HwSetting;

	if( _LM9831 == hw->chip ) {

		DBG( _DBG_INFO," * resetting LM9831 device!\n");
		_UIO( sanei_lm983x_write_byte( dev->fd, 0x07, 0));
		_UIO( sanei_lm983x_write_byte( dev->fd, 0x07,0x20));
		_UIO( sanei_lm983x_write_byte( dev->fd, 0x07, 0));
		_UIO( usbio_ReadReg( dev->fd, 0x07, &value));
		if (value != 0) {
 			DBG( _DBG_ERROR, "usbio_ResetLM983x: reset was not "
			                 "successful, status=%d\n", value );
			return SANE_STATUS_INVAL;
		}

	} else {
		_UIO( usbio_ReadReg( dev->fd, 0x07, &value));
		if (value != 0 ) {
			DBG( _DBG_INFO," * setting device to idle state!\n");
			_UIO( sanei_lm983x_write_byte( dev->fd, 0x07, 0));
		}
	}
	return SANE_STATUS_GOOD;
}

/* END PLUSTEK-USBIO.C ......................................................*/
