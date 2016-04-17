/** @file u12-io.c
 *  @brief The I/O functions to the U12 backend stuff.
 *
 * Copyright (c) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * GeneSys Logic I/O stuff derived from canon630u-common.c which has
 * been written by Nathan Rutman <nathan@gordian.com>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - changed u12io_GetFifoLength() behaviour
 *        - added delays to reset function
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

/** Format:
 * cacheLen[0]  =  ASIC-ID
 * cacheLen[1]  =  SCANSTATE ?
 * cacheLen[2]  =  REG-STATUS ?
 * cacheLen[3]  =  ??
 * cacheLen[4]  =  FIFO-LEN (RED)   HiByte  LW
 * cacheLen[5]  =  FIFO-LEN (RED)   LoByte  LW
 * cacheLen[6]  =  FIFO-LEN (RED)   LoByte  HW
 * cacheLen[7]  =  FIFO-LEN (GREEN) HiByte  LW
 * cacheLen[8]  =  FIFO-LEN (GREEN) LoByte  LW
 * cacheLen[9]  =  FIFO-LEN (GREEN) LoByte  HW
 * cacheLen[10] =  FIFO-LEN (BLUE)  HiByte  LW
 * cacheLen[11] =  FIFO-LEN (BLUE)  LoByte  LW
 * cacheLen[12] =  FIFO-LEN (BLUE)  LoByte  HW
 */
static SANE_Byte cacheLen[13];

/** This function is used to detect a cancel condition,
 * our ESC key is the SIGUSR1 signal. It is sent by the backend when the
 * cancel button has been pressed
 *
 * @param - none
 * @return the function returns SANE_TRUE if a cancel condition has been
 *  detected, if not, it returns SANE_FALSE
 */
static SANE_Bool u12io_IsEscPressed( void )
{
	sigset_t sigs;

	sigpending( &sigs );
	if( sigismember( &sigs, SIGUSR1 )) {
		DBG( _DBG_INFO, "SIGUSR1 is pending --> Cancel detected\n" );
		return SANE_TRUE;
	}

	return SANE_FALSE;
}

/** fall asleep for some micro-seconds...
 */
static void u12io_udelay( unsigned long usec )
{
	struct timeval now, deadline;

	if( usec == 0 )
		return;

	gettimeofday( &deadline, NULL );
	deadline.tv_usec += usec;
	deadline.tv_sec  += deadline.tv_usec / 1000000;
	deadline.tv_usec %= 1000000;

	do {
		gettimeofday( &now, NULL );
	} while ((now.tv_sec < deadline.tv_sec) ||
		(now.tv_sec == deadline.tv_sec && now.tv_usec < deadline.tv_usec));
}

/** Initializes a timer.
 * @param timer - pointer to the timer to start
 * @param us    - timeout value in micro-seconds
 */
static void u12io_StartTimer( TimerDef *timer , unsigned long us )
{
	struct timeval start_time;

	gettimeofday( &start_time, NULL );
	*timer = start_time.tv_sec * 1e6 + start_time.tv_usec + us;
}

/** Checks if a timer has been expired or not.
 * @param timer - pointer to the timer to check
 * @return Function returns  SANE_TRUE when the timer has been expired,
 *         otherwise SANE_FALSE
 */
static SANE_Bool u12io_CheckTimer( TimerDef *timer )
{
	struct timeval current_time;

	gettimeofday(&current_time, NULL);

	if((current_time.tv_sec * 1e6 + current_time.tv_usec) > *timer )
		return SANE_TRUE;

	return SANE_FALSE;
}

/* GL640 communication functions for Genesys Logic GL640USB
 * USB-IEEE1284 parallel port bridge
 */

/* Assign status and verify a good return code */
#define CHK(A) {if( (status = A) != SANE_STATUS_GOOD ) { \
                 DBG( _DBG_ERROR, "Failure on line of %s: %d\n", __FILE__, \
                      __LINE__ ); return A; }}

/** Register codes for the bridge.  These are NOT the registers for the ASIC
 *  on the other side of the bridge.
 */
typedef enum
{
	GL640_BULK_SETUP     = 0x82,
	GL640_EPP_ADDR       = 0x83,
	GL640_EPP_DATA_READ  = 0x84,
	GL640_EPP_DATA_WRITE = 0x85,
	GL640_SPP_STATUS     = 0x86,
	GL640_SPP_CONTROL    = 0x87,
	GL640_SPP_DATA       = 0x88,
	GL640_GPIO_OE        = 0x89,
	GL640_GPIO_READ      = 0x8a,
	GL640_GPIO_WRITE     = 0x8b
} GL640_Request;

/** for setting up bulk transfers */
static SANE_Byte bulk_setup_data[] = { 0, 0x11, 0, 0, 0, 0, 0, 0 };

/** Write to the usb-parallel port bridge.
 */
static SANE_Status
gl640WriteControl(int fd, GL640_Request req, u_char * data, unsigned int size)
{
	SANE_Status status;

	status = sanei_usb_control_msg( fd,
				  /* rqttype */ USB_TYPE_VENDOR |
				  USB_RECIP_DEVICE | USB_DIR_OUT /*0x40 */ ,
				  /* rqt */ (size > 1) ? 0x04 : 0x0C,
				  /* val */ (SANE_Int) req,
				  /* ind */ 0,
				  /* len */ size,
				  /* dat */ data);
	
	if( status != SANE_STATUS_GOOD ) {
		DBG( _DBG_ERROR, "gl640WriteControl error\n");
	}
	return status;
}

/** Read from the usb-parallel port bridge.
 */
static SANE_Status
gl640ReadControl( int fd, GL640_Request req, u_char *data, unsigned int size )
{
	SANE_Status status;

	status = sanei_usb_control_msg( fd,
				  /* rqttype */ USB_TYPE_VENDOR |
				  USB_RECIP_DEVICE | USB_DIR_IN /*0xc0 */ ,
				  /* rqt */ (size > 1) ? 0x04 : 0x0C,
				  /* val */ (SANE_Int) req,
				  /* ind */ 0,
				  /* len */ size,
				  /* dat */ data);

	if( status != SANE_STATUS_GOOD ) {
		DBG( _DBG_ERROR, "gl640ReadControl error\n");
	}
	return status;
}

/** Wrappers to write a single byte to the bridge */
static inline SANE_Status
gl640WriteReq( int fd, GL640_Request req, u_char data )
{
	return gl640WriteControl( fd, req, &data, 1);
}

/** Wrappers to read a single byte from the bridge */
static inline SANE_Status
gl640ReadReq( int fd, GL640_Request req, u_char *data )
{
	return gl640ReadControl( fd, req, data, 1 );
}

/** Write USB bulk data
 * setup is an apparently scanner-specific sequence:
 * {(0=read, 1=write), 0x00, 0x00, 0x00, sizelo, sizehi, 0x00, 0x00}
 * setup[1] = 0x11 --> data to register
 * setup[1] = 0x01 --> data to scanner memory
 */
static SANE_Status
gl640WriteBulk( int fd, u_char *setup, u_char *data, size_t size )
{
	SANE_Status status;

	setup[0] = 1;
	setup[4] = (size) & 0xFF;
	setup[5] = (size >> 8) & 0xFF;
	setup[6] = 0;

	CHK (gl640WriteControl (fd, GL640_BULK_SETUP, setup, 8));

	status = sanei_usb_write_bulk (fd, data, &size);
	if( status != SANE_STATUS_GOOD ) {
		DBG( _DBG_ERROR, "gl640WriteBulk error\n");
	}
	return status;
}

/** Read USB bulk data
 * setup is an apparently scanner-specific sequence:
 * {(0=read, 1=write), 0x00, 0x00, 0x00, sizelo, sizehi, 0x00, 0x00}
 * setup[1] = 0x00 --> data from scanner memory
 * setup[1] = 0x0c --> data from scanner fifo?
 */
static SANE_Status
gl640ReadBulk( int fd, u_char *setup, u_char *data, size_t size, int mod )
{
	SANE_Byte  *len_info;
	size_t      complete, current, toget;
	SANE_Status status;

	setup[0] = 0;
	setup[4] = (size) & 0xFF;
	setup[5] = (size >> 8) & 0xFF;
	setup[6] = mod;

	CHK (gl640WriteControl (fd, GL640_BULK_SETUP, setup, 8));

	len_info = NULL;
	toget    = size;
	if( mod ) {
		toget   *= mod;
		len_info = data + toget;
		toget   += 13;
	}

	for( complete = 0; complete < toget; ) {

		current = toget - complete;
		status = sanei_usb_read_bulk( fd, data, &current );
		if( status != SANE_STATUS_GOOD ) {
			DBG( _DBG_ERROR, "gl640ReadBulk error\n");
			break;
		}
		data     += current;
		complete += current;
	}
	if( len_info ) {
		memcpy( cacheLen, len_info, 13 );
	}
	return status;
}

/* now the functions to access PP registers */

/** read the contents of the status register */
static SANE_Byte
inb_status( int fd )
{
	u_char data = 0xff;

	gl640ReadReq( fd, GL640_SPP_STATUS, &data );
	return data;
}

/** write a byte to the SPP data port */
static SANE_Status
outb_data( int fd, u_char data )
{
	return gl640WriteReq( fd, GL640_SPP_DATA, data);
}

/** write to the parport control register */
static SANE_Status
outb_ctrl( int fd, u_char data )
{
	return gl640WriteReq( fd, GL640_SPP_CONTROL, data);
}

/************************* ASIC access stuff *********************************/

/** write a register number to the ASIC
 */
static void u12io_RegisterToScanner( U12_Device *dev, SANE_Byte reg )
{
	if( dev->mode == _PP_MODE_EPP ) {

		gl640WriteReq( dev->fd, GL640_EPP_ADDR, reg );

	} else {

		/* write register number to read from to SPP data-port
		 */
		outb_data( dev->fd, reg );

		/* signal that to the ASIC */
		outb_ctrl( dev->fd, _CTRL_SIGNAL_REGWRITE );
		_DODELAY(20);
		outb_ctrl( dev->fd, _CTRL_END_REGWRITE );
	}
}

/** as the name says, we switch to SPP mode
 */
static void u12io_SwitchToSPPMode( U12_Device *dev )
{
	dev->mode = _PP_MODE_SPP;
	outb_ctrl( dev->fd, _CTRL_GENSIGNAL );
}

/** as the name says, we switch to SPP mode
 */
static void u12io_SwitchToEPPMode( U12_Device *dev )
{
	u12io_RegisterToScanner( dev, REG_EPPENABLE );
	dev->mode = _PP_MODE_EPP;
}

/** read data from SPP status port
 */
static SANE_Byte u12io_DataFromSPP( U12_Device *dev )
{
	SANE_Byte data, tmp;

	/* read low nibble */
	tmp = inb_status( dev->fd );

	outb_ctrl( dev->fd, (_CTRL_GENSIGNAL + _CTRL_STROBE));

	/* read high nibble */
	data  = inb_status( dev->fd );
	data &= 0xf0;

	/* combine with low nibble */
	data |= (tmp >> 4);
	return data;
}

/** Read the content of specific ASIC register
 */
static SANE_Byte u12io_DataFromRegister( U12_Device *dev, SANE_Byte reg )
{
	SANE_Byte val;

	if( dev->mode == _PP_MODE_EPP ) {
		gl640WriteReq( dev->fd, GL640_EPP_ADDR, reg );
		gl640ReadReq ( dev->fd, GL640_EPP_DATA_READ, &val );
	} else {

		u12io_RegisterToScanner( dev, reg );
		val = u12io_DataFromSPP( dev );
	}
	return val;
}

/**
 */
static void u12io_CloseScanPath( U12_Device *dev )
{
	DBG( _DBG_INFO, "u12io_CloseScanPath()\n" );
/* FIXME: Probaly not needed */
#if 0
	u12io_RegisterToScanner( dev, 0xff );
#endif
	u12io_RegisterToScanner( dev, REG_SWITCHBUS );

	dev->mode = _PP_MODE_SPP;
}

/** try to connect to scanner
 */
static SANE_Bool u12io_OpenScanPath( U12_Device *dev )
{
	u_char tmp;

	DBG( _DBG_INFO, "u12io_OpenScanPath()\n" );

	u12io_SwitchToSPPMode( dev );

	outb_data( dev->fd, _ID_TO_PRINTER );
	_DODELAY(20);

	outb_data( dev->fd, _ID1ST );
	_DODELAY(5);

	outb_data( dev->fd, _ID2ND );
	_DODELAY(5);

	outb_data( dev->fd, _ID3RD );
	_DODELAY(5);

	outb_data( dev->fd, _ID4TH );
	_DODELAY(5);

	tmp = u12io_DataFromRegister( dev, REG_ASICID );
	if( ASIC_ID == tmp ) {
		u12io_SwitchToEPPMode( dev );
		return SANE_TRUE;
	}

	DBG( _DBG_ERROR, "u12io_OpenScanPath() failed!\n" );
	return SANE_FALSE;
}

/** Write data to asic (SPP mode only)
 */
static void u12io_DataToScanner( U12_Device *dev , SANE_Byte bValue )
{
	if( dev->mode != _PP_MODE_SPP ) {
		DBG( _DBG_ERROR, "u12io_DataToScanner() in wrong mode!\n" );
		return;
	}

    /* output data */
	outb_data( dev->fd, bValue );

	/* notify asic there is data */
	outb_ctrl( dev->fd, _CTRL_SIGNAL_DATAWRITE );

	/* end write cycle */
	outb_ctrl( dev->fd, _CTRL_END_DATAWRITE );
}

/** Write data to specific ASIC's register
 */
static SANE_Status u12io_DataToRegister( U12_Device *dev,
                                         SANE_Byte reg, SANE_Byte data )
{
	SANE_Status status;
	SANE_Byte   buf[2];

	if( dev->mode == _PP_MODE_EPP ) {

		buf[0] = reg;
		buf[1] = data;

		bulk_setup_data[1] = 0x11;
		CHK( gl640WriteBulk ( dev->fd, bulk_setup_data, buf, 2 ));

	} else {

		u12io_RegisterToScanner( dev, reg );
		u12io_DataToScanner( dev, data );
	}
	return SANE_STATUS_GOOD;
}

/** Write data-buffer to specific ASIC's register
 *  The format in the buffer is
 *  reg(0),val(0),reg(1),val(1),..., reg(len-1),val(len-1)
 */
static SANE_Status u12io_DataToRegs( U12_Device *dev, SANE_Byte *buf, int len )
{
	SANE_Status status;

	if( dev->mode != _PP_MODE_EPP ) {
		DBG( _DBG_ERROR, "u12io_DataToRegs() in wrong mode!\n" );
		return SANE_STATUS_IO_ERROR;
	}

	bulk_setup_data[1] = 0x11;
	CHK( gl640WriteBulk ( dev->fd, bulk_setup_data, buf, len*2 ));
	return SANE_STATUS_GOOD;
}

/** write data to the DAC 
 */
static void
u12io_DataRegisterToDAC( U12_Device *dev, SANE_Byte reg, SANE_Byte val )
{
	SANE_Byte buf[6];

	buf[0] = REG_ADCADDR;
	buf[1] = reg;
	buf[2] = REG_ADCDATA;
	buf[3] = val;
	buf[4] = REG_ADCSERIALOUT;
 	buf[5] = val;

	u12io_DataToRegs( dev, buf, 3 );
}

/** write data block to scanner
 */
static SANE_Status u12io_MoveDataToScanner( U12_Device *dev,
                                            SANE_Byte *buf, int len )
{
	SANE_Status status;

/*	u12io_RegisterToScanner( dev, REG_INITDATAFIFO ); */
	u12io_RegisterToScanner( dev, REG_WRITEDATAMODE );

	bulk_setup_data[1] = 0x01;
	CHK( gl640WriteBulk( dev->fd, bulk_setup_data, buf, len ));
	bulk_setup_data[1] = 0x11;

	return SANE_STATUS_GOOD;
}

static SANE_Status u12io_ReadData( U12_Device *dev, SANE_Byte *buf, int len )
{
	SANE_Status status;

	u12io_DataToRegister( dev, REG_MODECONTROL, dev->regs.RD_ModeControl );
/*	u12io_RegisterToScanner( dev, REG_INITDATAFIFO ); */
	u12io_RegisterToScanner( dev, REG_READDATAMODE );

	bulk_setup_data[1] = 0x00;
	CHK (gl640ReadBulk( dev->fd, bulk_setup_data, buf, len, 0 ));
	bulk_setup_data[1] = 0x11;

	return SANE_STATUS_GOOD;
}

/** perform a SW reset of ASIC P98003
 */
static void u12io_SoftwareReset( U12_Device *dev )
{
	DBG( _DBG_INFO, "Device reset (%i)!!!\n", dev->fd );

	u12io_DataToRegister( dev, REG_TESTMODE, _SW_TESTMODE );

	outb_data( dev->fd, _ID_TO_PRINTER );
	_DODELAY(20);

	outb_data( dev->fd, _RESET1ST );
	_DODELAY(5);
	outb_data( dev->fd, _RESET2ND );
	_DODELAY(5);
	outb_data( dev->fd, _RESET3RD );
	_DODELAY(5);
	outb_data( dev->fd, _RESET4TH );
	_DODELAY(250);
}

/**
 */
static SANE_Bool u12io_IsConnected( U12_Device *dev )
{
	int       c, mode;
	SANE_Byte tmp, rb[6];

	DBG( _DBG_INFO, "u12io_IsConnected()\n" );
	tmp = inb_status( dev->fd );
	DBG( _DBG_INFO, "* tmp1 = 0x%02x\n", tmp );

	gl640WriteReq( dev->fd, GL640_EPP_ADDR, REG_ASICID );
	gl640ReadReq ( dev->fd, GL640_EPP_DATA_READ, &tmp );
	DBG( _DBG_INFO, "* REG_ASICID = 0x%02x\n", tmp );

	if( tmp != ASIC_ID ) {

		DBG( _DBG_INFO, "* Scanner is NOT connected!\n" );

		tmp = inb_status( dev->fd );
		DBG( _DBG_INFO, "* tmp2 = 0x%02x\n", tmp );

		gl640WriteReq( dev->fd, GL640_EPP_ADDR, REG_ASICID );
		gl640ReadReq ( dev->fd, GL640_EPP_DATA_READ, &tmp );
		DBG( _DBG_INFO, "* REG_ASICID = 0x%02x\n", tmp );

		if( tmp == 0x02 ) {

			mode = dev->mode;
			dev->mode = _PP_MODE_EPP;
			u12io_DataToRegister( dev, REG_ADCADDR, 0x01 );
			u12io_DataToRegister( dev, REG_ADCDATA, 0x00 );
			u12io_DataToRegister( dev, REG_ADCSERIALOUT, 0x00 );

			c = 0;
			_SET_REG( rb, c, REG_MODECONTROL, 0x19 );
			_SET_REG( rb, c, REG_STEPCONTROL, 0xff );
			_SET_REG( rb, c, REG_MOTOR0CONTROL, 0 );
			u12io_DataToRegs( dev, rb, c );
			dev->mode = mode ;
		}
		return SANE_FALSE;
	} 

	u12io_SwitchToEPPMode( dev );
	DBG( _DBG_INFO, "* Scanner is connected!\n" );
	return SANE_TRUE;
}

/**
 */
static SANE_Byte u12io_GetExtendedStatus( U12_Device *dev )
{
	SANE_Byte b;

	b = u12io_DataFromRegister( dev, REG_STATUS2 );
	if( b == 0xff )
		return 0;
	return b;
}

/**
 */
static SANE_Status u12io_ReadMonoData( U12_Device *dev, SANE_Byte *buf, u_long len )
{
	SANE_Status status;

	bulk_setup_data[1] = 0x0c;
	bulk_setup_data[2] = ((dev->regs.RD_ModeControl >> 3) + 1);

	CHK (gl640ReadBulk( dev->fd, bulk_setup_data, buf, len, 1 ));
	bulk_setup_data[1] = 0x11;
	bulk_setup_data[2] = 0;

	return SANE_STATUS_GOOD;
}

/**
 */
static SANE_Status
u12io_ReadColorData( U12_Device *dev, SANE_Byte *buf, u_long len )
{
	SANE_Status status;

	bulk_setup_data[1] = 0x0c;

	CHK (gl640ReadBulk( dev->fd, bulk_setup_data, buf, len, 3 ));
	bulk_setup_data[1] = 0x11;
	return SANE_STATUS_GOOD;
}

/** read the recent state count
 */
static SANE_Byte u12io_GetScanState( U12_Device *dev )
{
	if( cacheLen[0] == 0x83 ) {
		DBG( _DBG_READ, "u12io_GetScanState(cached) = 0x%02x\n", cacheLen[1] );
		return cacheLen[1];
    }
	return u12io_DataFromRegister( dev, REG_GETSCANSTATE );
}

/** download a scanstate-table
 */
static SANE_Status u12io_DownloadScanStates( U12_Device *dev )
{
	SANE_Status status;
	TimerDef    timer;

	u12io_RegisterToScanner( dev, REG_SCANSTATECONTROL );

	bulk_setup_data[1] = 0x01;
	CHK( gl640WriteBulk( dev->fd, bulk_setup_data,
	                     dev->scanStates, _SCANSTATE_BYTES ));
	bulk_setup_data[1] = 0x11;

/* FIXME: refreshState probably always FALSE */	
	if( dev->scan.refreshState ) {

		u12io_RegisterToScanner( dev, REG_REFRESHSCANSTATE );

		u12io_StartTimer( &timer, (_SECOND/2));
		do {

			if (!( u12io_GetScanState( dev ) & _SCANSTATE_STOP))
				break;
		}
		while( !u12io_CheckTimer(&timer));
	}
	return SANE_STATUS_GOOD;
}

/** - initializes the scan states
 *  - sets all necessary registers
 * FIXME: first copy to buffer, then use u12io_DataToRegs()
 */
static void u12io_PutOnAllRegisters( U12_Device *dev )
{
	SANE_Byte *val, reg;
	SANE_Byte *rb, buf[100];
	int        c;

	/* setup scan states */
	u12io_DownloadScanStates( dev );

	c  = 0;
	rb = buf;
    
	*(rb++) = REG_MODECONTROL;
	*(rb++) = dev->regs.RD_ModeControl;
	c++;
	*(rb++) = REG_STEPCONTROL;
	*(rb++) = dev->regs.RD_StepControl;
	c++;
	*(rb++) = REG_MOTOR0CONTROL;
	*(rb++) = dev->regs.RD_Motor0Control;
	c++;
	*(rb++) = REG_LINECONTROL;
	*(rb++) = dev->regs.RD_LineControl;
	c++;
	*(rb++) = REG_XSTEPTIME;
	*(rb++) = dev->regs.RD_XStepTime;
	c++;
	*(rb++) = REG_MODELCONTROL;
	*(rb++) =  dev->regs.RD_ModelControl;
	c++;
	/* the 1st register to write */
	val = (SANE_Byte*)&dev->regs.RD_Dpi;

	/* 0x21 - 0x28 */
	for( reg = REG_DPILO; reg <= REG_THRESHOLDHI; reg++, val++ ) {
		*(rb++) = reg;
		*(rb++) = *val;
		c++;
	}

	u12io_DataToRegs( dev, buf, c );

	u12io_RegisterToScanner( dev, REG_INITDATAFIFO );
	u12io_DataToRegister( dev, REG_MODECONTROL, _ModeScan );
}

/**
 */
static void u12io_ResetFifoLen( void )
{
	memset( cacheLen, 0, 13 );
}

/**
 */
static u_long u12io_GetFifoLength( U12_Device *dev )
{
	SANE_Status status;
	size_t      toget;
	SANE_Byte   data[64];
	u_long      len, len_r, len_g, len_b;

	if( cacheLen[0] == 0x83 ) {

		DBG( _DBG_READ, "Using cached FIFO len\n" );
		memcpy( data, cacheLen, 13 );
		u12io_ResetFifoLen();
		
	} else {

		memset( bulk_setup_data, 0, 8 );
		bulk_setup_data[1] = 0x0c;

		CHK (gl640WriteControl(dev->fd, GL640_BULK_SETUP, bulk_setup_data, 8));

		toget = 13;
		status = sanei_usb_read_bulk( dev->fd, data, &toget );
		if( status != SANE_STATUS_GOOD ) {
			DBG( _DBG_ERROR, "ReadBulk error\n");
			return SANE_FALSE;
		}
		bulk_setup_data[1] = 0x11;

		memcpy( cacheLen, data, 13 );
	}
	len_r = (u_long)data[5]  * 256 + (u_long)data[4];
	len_g = (u_long)data[8]  * 256 + (u_long)data[7];
	len_b = (u_long)data[11] * 256 + (u_long)data[10];

	if( dev->DataInf.wPhyDataType < COLOR_TRUE24 ) {
		len = len_g;
	} else {

		len = len_r;
		if( len_g < len )
			len = len_g;
		if( len_b < len )
			len = len_b;
	}

	DBG( _DBG_READ, "FIFO-LEN: %lu %lu %lu = %lu\n", len_r, len_g, len_b, len );
	return len;
}

/**
 */
static SANE_Bool
u12io_ReadOneShadingLine( U12_Device *dev, SANE_Byte *buf, u_long len )
{
	TimerDef    timer;
	SANE_Status status;

	DBG( _DBG_READ, "u12io_ReadOneShadingLine()\n" );
	u12io_StartTimer( &timer, _SECOND );

	dev->scan.bFifoSelect = REG_GFIFOOFFSET;

	do {
		u12io_ResetFifoLen();
		if( u12io_GetFifoLength( dev ) >= dev->regs.RD_Pixels ) {

			status = u12io_ReadColorData( dev, buf, len );
			if( status != SANE_STATUS_GOOD ) {
				DBG( _DBG_ERROR, "ReadColorData error\n");
				return SANE_FALSE;
			}
			DBG( _DBG_READ, "* done\n" );
			return SANE_TRUE;
		}
	} while( !u12io_CheckTimer( &timer ));

	DBG( _DBG_ERROR, "u12io_ReadOneShadingLine() failed!\n" );
	return SANE_FALSE;
}

/* END U12-IO.C .............................................................*/
