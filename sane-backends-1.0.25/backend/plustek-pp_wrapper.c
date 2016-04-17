/** @file plustek-pp_wrapper.c
 *  @brief The interface to the parport driver-code and the kernel module.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - initial version
 * - 0.41 - added _PTDRV_ADJUST call
 * - 0.42 - added setmap function
 *        - fixed the stopscan problem, that causes a crash in the kernel module
 * - 0.43 - added initialized setting
 *        - cleanup
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

/******************* wrapper functions for parport device ********************/

#ifndef _BACKEND_ENABLED
  
static int PtDrvInit( char *dev_name, unsigned short model_override )
{
	_VAR_NOT_USED( dev_name );
	_VAR_NOT_USED( model_override );
	DBG( _DBG_ERROR, "Backend does not support direct I/O!\n" );
	return -1;
}

static int PtDrvShutdown( void )
{
	return 0;
}

static int PtDrvOpen( void )
{
	DBG( _DBG_ERROR, "Backend does not support direct I/O!\n" );
	return -1;
}

static int PtDrvClose( void )
{
	DBG( _DBG_ERROR, "Backend does not support direct I/O!\n" );
	return 0;
}

static int PtDrvIoctl( unsigned int cmd, void *arg )
{
	DBG( _DBG_ERROR, "Backend does not support direct I/O!\n" );
	_VAR_NOT_USED( cmd );
	if( arg == NULL )
		return -2;
	return -1;
}

static int PtDrvRead( unsigned char *buffer, int count )
{
	DBG( _DBG_ERROR, "Backend does not support direct I/O!\n" );
	_VAR_NOT_USED( count );
	if( buffer == NULL )
		return -2;
	return -1;
}
#endif /* _BACKEND_ENABLED */

/**
 */
static int ppDev_open( const char *dev_name, void *misc )
{
	int 		    result;
	int			    handle;
	CompatAdjDef    compatAdj;
	PPAdjDef        adj;
	unsigned short  version = _PTDRV_IOCTL_VERSION;
	Plustek_Device *dev     = (Plustek_Device *)misc;

	if( dev->adj.direct_io ) {
		result = PtDrvInit( dev_name, dev->adj.mov );
		if( 0 != result ) {
			DBG( _DBG_ERROR, "open: PtDrvInit failed: %d\n", result );
			return -1;
		}
	}

	if( dev->adj.direct_io )
		handle = PtDrvOpen();
	else
		handle = open( dev_name, O_RDONLY );
	
	if ( handle  < 0 ) {
	    DBG( _DBG_ERROR, "open: can't open %s as a device\n", dev_name );
    	return handle;
	}

	if( dev->adj.direct_io )
		result = PtDrvIoctl( _PTDRV_OPEN_DEVICE, &version );
	else
		result = ioctl( handle, _PTDRV_OPEN_DEVICE, &version );
		
	if( result < 0 ) {

        if( -9019 == result ) {

			DBG( _DBG_INFO, "Version 0x%04x not supported, trying "
                            "compatibility version 0x%04x\n",
                            _PTDRV_IOCTL_VERSION, _PTDRV_COMPAT_IOCTL_VERSION);

			version = _PTDRV_COMPAT_IOCTL_VERSION;

			if( dev->adj.direct_io )
				result = PtDrvIoctl( _PTDRV_OPEN_DEVICE, &version );
			else
				result = ioctl( handle, _PTDRV_OPEN_DEVICE, &version );
			
			if( result < 0 ) {
				
				if( dev->adj.direct_io )
					PtDrvClose();
				else
					close( dev->fd );
					
				DBG( _DBG_ERROR,
					 "ioctl PT_DRV_OPEN_DEVICE failed(%d)\n", result );

		        if( -9019 == result ) {
	    			DBG( _DBG_ERROR,
						 "Version problem, please recompile driver!\n" );
				}
			} else {

				DBG( _DBG_INFO, "Using compatibility version\n" );

				compatAdj.lampOff      = dev->adj.lampOff;
				compatAdj.lampOffOnEnd = dev->adj.lampOffOnEnd;
				compatAdj.warmup       = dev->adj.warmup;

				memcpy( &compatAdj.pos, &dev->adj.pos, sizeof(OffsDef));
				memcpy( &compatAdj.neg, &dev->adj.neg, sizeof(OffsDef));
				memcpy( &compatAdj.tpa, &dev->adj.tpa, sizeof(OffsDef));

				if( dev->adj.direct_io )
					PtDrvIoctl( _PTDRV_ADJUST, &compatAdj );
				else
					ioctl( handle, _PTDRV_ADJUST, &compatAdj );
				return handle;
			}
		}
		return result;
    }

	memset( &adj, 0, sizeof(PPAdjDef));

	adj.lampOff      = dev->adj.lampOff;
	adj.lampOffOnEnd = dev->adj.lampOffOnEnd;
	adj.warmup       = dev->adj.warmup;

	memcpy( &adj.pos, &dev->adj.pos, sizeof(OffsDef));
	memcpy( &adj.neg, &dev->adj.neg, sizeof(OffsDef));
	memcpy( &adj.tpa, &dev->adj.tpa, sizeof(OffsDef));

	adj.rgamma    = dev->adj.rgamma;
	adj.ggamma    = dev->adj.ggamma;
	adj.bgamma    = dev->adj.bgamma;
	adj.graygamma = dev->adj.graygamma;

	if( dev->adj.direct_io )
		PtDrvIoctl( _PTDRV_ADJUST, &adj );
	else
		ioctl( handle, _PTDRV_ADJUST, &adj );

	dev->initialized = SANE_TRUE;
	return handle;
}

/**
 */
static int ppDev_close( Plustek_Device *dev )
{
	if( dev->adj.direct_io )
		return PtDrvClose();
	else
		return close( dev->fd );
}

/**
 */
static int ppDev_getCaps( Plustek_Device *dev )
{
	if( dev->adj.direct_io )
		return PtDrvIoctl( _PTDRV_GET_CAPABILITIES, &dev->caps );
	else
		return ioctl( dev->fd, _PTDRV_GET_CAPABILITIES, &dev->caps );
}

/**
 */
static int ppDev_getLensInfo( Plustek_Device *dev, pLensInfo lens )
{
	if( dev->adj.direct_io )
		return  PtDrvIoctl( _PTDRV_GET_LENSINFO, lens );
	else
		return ioctl( dev->fd, _PTDRV_GET_LENSINFO, lens );
}

/**
 */
static int ppDev_getCropInfo( Plustek_Device *dev, pCropInfo crop )
{
	if( dev->adj.direct_io )
		return PtDrvIoctl( _PTDRV_GET_CROPINFO, crop );
	else
		return ioctl( dev->fd, _PTDRV_GET_CROPINFO, crop );
}

/**
 */
static int ppDev_putImgInfo( Plustek_Device *dev, pImgDef img )
{
	if( dev->adj.direct_io )
		return PtDrvIoctl( _PTDRV_PUT_IMAGEINFO, img );
	else
		return ioctl( dev->fd, _PTDRV_PUT_IMAGEINFO, img );
}

/**
 */
static int ppDev_setScanEnv( Plustek_Device *dev, pScanInfo sinfo )
{
	if( dev->adj.direct_io )
		return PtDrvIoctl( _PTDRV_SET_ENV, sinfo );
	else
		return ioctl( dev->fd, _PTDRV_SET_ENV, sinfo );
}

/**
 */
static int ppDev_startScan( Plustek_Device *dev, pStartScan start )
{
	if( dev->adj.direct_io )
		return PtDrvIoctl( _PTDRV_START_SCAN, start );
	else
		return ioctl( dev->fd, _PTDRV_START_SCAN, start );
}

/** function to send a gamma table to the kernel module. As the default table
 *  entry is 16-bit, but the maps are 8-bit, we have to copy the values...
 */
static int ppDev_setMap( Plustek_Device *dev, SANE_Word *map,
						 SANE_Word length, SANE_Word channel )
{
	SANE_Byte *buf;
	SANE_Word  i;
	MapDef     m;

	m.len    = length;
	m.map_id = channel;

	m.map = (void *)map;
		
	DBG(_DBG_INFO,"Setting map[%u] at 0x%08lx\n", channel, (unsigned long)map);

	buf = (SANE_Byte*)malloc( m.len );
	
	if( !buf )
		return _E_ALLOC;
	
	for( i = 0; i < m.len; i++ ) {
		buf[i] = (SANE_Byte)map[i];
		
		if( map[i] > 0xFF )
			buf[i] = 0xFF;
	}
	
	m.map = buf;
	
	if( dev->adj.direct_io )
		PtDrvIoctl( _PTDRV_SETMAP, &m );
	else
		ioctl( dev->fd, _PTDRV_SETMAP, &m );

	/* we ignore the return values */
	free( buf );
	return 0;
}

/**
 */
static int ppDev_stopScan( Plustek_Device *dev, short *mode )
{
	int retval, tmp;

	/* save this one... */
	tmp = *mode;

	if( dev->adj.direct_io )
		retval = PtDrvIoctl( _PTDRV_STOP_SCAN, mode );
	else
		retval = ioctl( dev->fd, _PTDRV_STOP_SCAN, mode );
	
	/* ... and use it here */
	if( 0 == tmp ) {
		if( dev->adj.direct_io )
			PtDrvIoctl( _PTDRV_CLOSE_DEVICE, 0 );
		else
			ioctl( dev->fd, _PTDRV_CLOSE_DEVICE, 0);
	}else
		sleep( 1 );

	return retval;
}

/**
 */
static int ppDev_readImage( Plustek_Device *dev,
                            SANE_Byte *buf, unsigned long data_length )
{
	if( dev->adj.direct_io )
		return PtDrvRead( buf, data_length );
	else
		return read( dev->fd, buf, data_length );
}

/* END PLUSTEK-PP_WRAPPER.C .................................................*/
