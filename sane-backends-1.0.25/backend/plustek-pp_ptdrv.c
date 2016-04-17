/* @file plustek-pp_ptdrv.c
 * @brief this is the driver interface
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - Added some comments
 *        - added claiming/release of parallel port resources for this driver
 *        - added scaling function for high resolution modes where dpix < dpiy
 * - 0.32 - Revised lamp-off behaviour
 *        - removed function ptdrvIsLampOn
 *        - fixed misbehaviour when using cat /dev/pt_drv
 *        - moved parport-functions to module misc.c
 * - 0.33 - added parameter lOffonEnd
 *        - revised parport concurrency
 *        - removed calls to ps->PositionLamp
 * - 0.34 - no changes
 * - 0.35 - removed _PTDRV_PUT_SCANNER_MODEL from ioctl interface
 *        - added Kevins' changes (MiscRestorePort)
 *        - added parameter legal and function PtDrvLegalRequested()
 * - 0.36 - removed a bug in the shutdown function
 *        - removed all OP600P specific stuff because of the Primax tests
 *        - added version code to ioctl interface
 *        - added new parameter mov - model override
 *        - removed parameter legal
 *        - removed function PtDrvLegalRequested
 *        - changes, due to define renaming
 *        - patch for OpticPro 4800P
 *        - added multiple device support
 *        - added proc fs support/also for Kernel2.4
 * - 0.37 - cleanup work, moved the procfs stuff to file procfs.c
 *        - and some definitions to plustek_scan.h
 *        - moved MODELSTR to misc.c
 *        - output of the error-code after initialization
 * - 0.38 - added P12 stuff
 *        - removed function ptdrvIdleMode
 *        - moved function ptdrvP96Calibration() to p48xxCalibration
 *        - moved function ptdrvP98Calibration() to p9636Calibration
 *        - added devfs support (patch by Gordon Heydon <gjheydon@bigfoot.com>)
 * - 0.39 - added schedule stuff after reading one line to have a better
 *          system response in SPP modes
 *        - added forceMode switch
 * - 0.40 - added MODULE_LICENSE stuff
 * - 0.41 - added _PTDRV_ADJUST functionality
 *        - changed ioctl call to PutImage
 * - 0.42 - added _PTDRV_SETMAP functionality
 *        - improved the cancel functionality
 * - 0.43 - added LINUX_26 stuff
 *        - changed include names
 *        - changed version string stuff
 * - 0.44 - added support for more recent kernels
 *        - fix format string issues, as Long types default to int32_t
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
#ifdef __KERNEL__
# include <linux/module.h>
# include <linux/version.h>

# ifdef CONFIG_DEVFS_FS
#  include <linux/devfs_fs_kernel.h>
#  if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,69))
#   define DEVFS_26_STYLE
#  endif
# endif
#endif

#include "plustek-pp_scan.h"

#ifdef __KERNEL__
# include <linux/param.h>
#endif

/****************************** static vars **********************************/

/* default port is at 0x378 */
static int port[_MAX_PTDEVS] = { 0x378, 0, 0, 0 };

#ifdef __KERNEL__
static pScanData PtDrvDevices[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = NULL};

/* default is 180 secs for lamp switch off */
static int lampoff[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = 180 };

/* warmup period for lamp (30 secs) */
static int warmup[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = 30 };

/* switch lamp off on unload (default = no)*/
static int lOffonEnd[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = 0 };

/* model override (0-->none) */
static UShort mov[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = 0 };

/* forceMode (0--> auto, 1: SPP, 2:EPP, others: auto) */
static UShort forceMode[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = 0 };

/* to use delayed I/O for each device */
static Bool slowIO[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = _FALSE };

#else

static pScanData PtDrvDevices[_MAX_PTDEVS]= { NULL,   NULL,   NULL,   NULL   };
static int       lampoff[_MAX_PTDEVS]     = { 180,    180,    180,    180    };
static int       warmup[_MAX_PTDEVS]      = { 30,     30,     30,     30     };
static int       lOffonEnd[_MAX_PTDEVS]   = { 0,      0,      0,      0      };
static UShort    mov[_MAX_PTDEVS]         = { 0,      0,      0,      0      };
static UShort    forceMode[_MAX_PTDEVS]   = { 0,      0,      0,      0      };

#endif

/* timers for warmup checks */
static TimerDef toTimer[_MAX_PTDEVS];

#ifndef __KERNEL__
static Bool	PtDrvInitialized = _FALSE;
#ifdef HAVE_SETITIMER
static struct itimerval saveSettings;
#endif
#else
static Bool deviceScanning = _FALSE;

static struct timer_list tl[_MAX_PTDEVS];

/* for calculation of the timer expiration */
extern volatile unsigned long jiffies;

/* the parameter interface
 */
#if ((LINUX_VERSION_CODE > 0x020111) && defined(MODULE))
MODULE_AUTHOR("Gerhard Jaeger <gerhard@gjaeger.de>");
MODULE_DESCRIPTION("Plustek parallelport-scanner driver");

/* addresses this 'new' license feature... */
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
MODULE_PARM(port, "1-" __MODULE_STRING(_MAX_PTDEVS) "i");
MODULE_PARM(lampoff, "1-" __MODULE_STRING(_MAX_PTDEVS) "i");
MODULE_PARM(warmup,"1-" __MODULE_STRING(_MAX_PTDEVS) "i");
MODULE_PARM(lOffonEnd, "1-" __MODULE_STRING(_MAX_PTDEVS) "i");
MODULE_PARM(mov, "1-" __MODULE_STRING(_MAX_PTDEVS) "i");
MODULE_PARM(slowIO,"1-" __MODULE_STRING(_MAX_PTDEVS) "i");
MODULE_PARM(forceMode,"1-" __MODULE_STRING(_MAX_PTDEVS) "i");

#else

static int array_len = _MAX_PTDEVS;

module_param_array(port,      int,    &array_len, 0);
module_param_array(lampoff,   int,    &array_len, 0);
module_param_array(warmup,    int,    &array_len, 0);
module_param_array(lOffonEnd, int,    &array_len, 0);
module_param_array(mov,       ushort, &array_len, 0);
module_param_array(slowIO,    int,    &array_len, 0);
module_param_array(forceMode, ushort, &array_len, 0);

#endif


MODULE_PARM_DESC(port, "I/O base address of parport");
MODULE_PARM_DESC(lampoff, "Lamp-Off timer preset in seconds");
MODULE_PARM_DESC(warmup, "Minimum warmup time in seconds");
MODULE_PARM_DESC(lOffonEnd, "1 - switchoff lamp on unload");
MODULE_PARM_DESC(mov, "Modell-override switch");
MODULE_PARM_DESC(slowIO, "0 = Fast I/O, 1 = Delayed I/O");
MODULE_PARM_DESC(forceMode, "0 = use auto detection, "
                            "1 = use SPP mode, 2 = use EPP mode");
#endif

#if defined (CONFIG_DEVFS_FS)
# ifndef (DEVFS_26_STYLE)
	static devfs_handle_t devfs_handle = NULL;
# endif
#else
# ifdef LINUX_26
	static class_t *ptdrv_class;
# endif
#endif

/*
 * the module interface
 */
static int 		 pt_drv_open ( struct inode *, struct file *);
static CLOSETYPE pt_drv_close( struct inode *, struct file *);

#ifdef LINUX_20
  static int pt_drv_read(  struct inode*, struct file*, char*, int );
  static int pt_drv_write( struct inode*, struct file*, const char*, int );
#else
  static ssize_t pt_drv_read ( struct file *file,
							 char *buffer, size_t count, loff_t *);
  static ssize_t pt_drv_write( struct file *file,
							 const char *buffer, size_t tmp,loff_t *count);
#endif

#ifdef NOLOCK_IOCTL
  static long pt_drv_ioctl( struct file *, UInt, unsigned long );
#else
  static int pt_drv_ioctl( struct inode *, struct file *, UInt, unsigned long );
#endif
  
  
/*
 * the driver interface
 */
#ifdef LINUX_20

static struct file_operations pt_drv_fops =
{
	NULL,			/* seek 				*/
	pt_drv_read,	/* read 				*/
	pt_drv_write,	/* write 				*/
	NULL,			/* readdir 				*/
	NULL,			/* select 				*/
	pt_drv_ioctl,  	/* ioctl 				*/
	NULL,   		/* mmap 				*/
	pt_drv_open,    /* open 				*/
	pt_drv_close,	/* release 				*/
	NULL,			/* fsync 				*/
	NULL,			/* fasync 				*/
	NULL,			/* check_media_change 	*/
	NULL			/* revalidate 			*/
};

#else	/* 2.2.x and higher stuff */

static struct file_operations pt_drv_fops = {
#ifdef LINUX_24
	owner:		THIS_MODULE,
#endif
	read:		pt_drv_read,
	write:		pt_drv_write,
	IOCTL:		pt_drv_ioctl,
	open:		pt_drv_open,
	release:	pt_drv_close,
};

#endif

#endif	/* guard __KERNEL */

/****************************** some prototypes ******************************/

static void ptdrvStartLampTimer( pScanData ps );

/****************************** local functions ******************************/

#ifdef __KERNEL__
/** depending on the device, return the data structure
 */
static pScanData get_pt_from_inode(struct inode *ip)
{
    int minor = _MINOR(ip);

    /*
     * unit out of range
     */
    if (minor >=  _MAX_PTDEVS )	
        return NULL;

    return( PtDrvDevices[minor] );
}
#endif

/** copy user-space data into kernel memory
 */
static int getUserPtr(const pVoid useraddr, pVoid where, UInt size )
{
	int err = _OK;

	/* do parameter checks */
	if((NULL == useraddr) || ( 0 == size))
		return _E_INVALID;

#ifdef __KERNEL__
	if ((err = verify_area_20(VERIFY_READ, useraddr, size)))
		return err;
#endif

	switch (size) {
#ifdef __KERNEL__
	case sizeof(u_char):
		GET_USER_RET(*(u_char *)where, (u_char *) useraddr, -EFAULT);
		break;

	case sizeof(u_short):
		GET_USER_RET(*(u_short *)where, (u_short *) useraddr, -EFAULT);
		break;

	case sizeof(u_long):
		GET_USER_RET(*(u_long *)where, (u_long *) useraddr, -EFAULT);
		break;

	default:
		if (copy_from_user(where, useraddr, size))
			return -EFAULT;
#else
	default:
		memcpy( where, useraddr, size );
#endif
	}
	return err;
}

/** copy kernel data into user mode address space
 */
static int putUserPtr( const pVoid ptr, pVoid useraddr, UInt size )
{
	int err = _OK;

	if (NULL == useraddr)
    	return _E_INVALID;

#ifdef __KERNEL__
	if ((err = verify_area_20(VERIFY_WRITE, useraddr, size)))
		return err;

	if (copy_to_user(useraddr, ptr, size ))
		return -EFAULT;
#else
	memcpy( useraddr, ptr, size );
#endif

	return err;
}

#ifndef __KERNEL__
static unsigned long copy_from_user( pVoid dest, pVoid src, unsigned long len )
{
	memcpy( dest, src, len );
	return 0;
}

static unsigned long copy_to_user( pVoid dest, pVoid src, unsigned long len )
{
	memcpy( dest, src, len );
	return 0;
}
#endif

/**
 */
static int putUserVal(const ULong value, pVoid useraddr, UInt size)
{
#ifdef __KERNEL__
	int err;
#endif

	if (NULL == useraddr)
    	return _E_INVALID;

#ifdef __KERNEL__
	if ((err = verify_area_20(VERIFY_WRITE, useraddr, size)))
    	return err;
#endif

	switch (size) {

#ifdef __KERNEL__
	case sizeof(u_char):
    	PUT_USER_RET((u_char)value, (u_char *) useraddr, -EFAULT);
    	break;
  	case sizeof(u_short):
    	PUT_USER_RET((u_short)value, (u_short *) useraddr, -EFAULT);
    	break;
  	case sizeof(u_long):
    	PUT_USER_RET((u_long)value, (u_long *) useraddr, -EFAULT);
    	break;
#else
	case sizeof(UChar):
		*(pUChar)useraddr = (UChar)value;
		break;
	case sizeof(UShort):
		*(pUShort)useraddr = (UShort)value;
		break;
	case sizeof(ULong):
		*(pULong)useraddr = (ULong)value;
		break;

#endif
  	default:
    	return _E_INVALID;
	}
	return 0;
}

/** switch lamp 0 on
 */
static void ptDrvSwitchLampOn( pScanData ps )
{
	DBG( DBG_LOW, "Switching lamp 0 on.\n" );

	if( _IS_ASIC98(ps->sCaps.AsicID)) {

		ps->AsicReg.RD_ScanControl |= _SCAN_NORMALLAMP_ON;

		ps->bLastLampStatus = _SCAN_NORMALLAMP_ON;

	} else {
		
		ps->AsicReg.RD_ScanControl |= ps->bLampOn;
		ps->bLastLampStatus = ps->bLampOn;
	}

	IOCmdRegisterToScanner(ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);
}

/** check the lamp warmup
 */
static void ptdrvLampWarmup( pScanData ps )
{
	Bool	 warmupNeeded;
	TimerDef timer;

	if( 0 == ps->warmup )
		return;

	warmupNeeded = _FALSE;

	/*
	 * do we have to warmup again ? Timer has not elapsed...
	 */
	if( _OK == MiscCheckTimer( &toTimer[ps->devno] )) {

		DBG( DBG_LOW, "Startup warmup needed!\n" );
		warmupNeeded = _TRUE;
	} else {

		warmupNeeded = ps->fWarmupNeeded;
	}

	if( warmupNeeded ) {

		/*
		 * correct lamp should have been switched on but
		 * before doing anything else wait until warmup has been done
		 */
		DBG( DBG_LOW, "Waiting on warmup - %u s\n", ps->warmup );

		MiscStartTimer( &timer, _SECOND * ps->warmup );
		while( !MiscCheckTimer( &timer )) {
		
			/* on break, we setup the initial timer again... */
			if( _FALSE == ps->fScanningStatus ) {
				MiscStartTimer( &toTimer[ps->devno], (_SECOND * ps->warmup));
				return;		
			}	
		};

	}
#ifdef DEBUG
	else {
		DBG( DBG_LOW, "No warm-up needed \n" );
	}
#endif

	/*
	 * start a timer here again with only a second timeout
	 * because we need this one only for startup (Force timeout!!)
	 */
	MiscStartTimer( &toTimer[ps->devno], _SECOND );
}

/**
 */
#ifdef __KERNEL__
static void ptdrvLampTimerIrq( unsigned long ptr )
#else
static void ptdrvLampTimerIrq( int sig_num )
#endif
{
	pScanData ps;

	DBG( DBG_HIGH, "!! IRQ !! Lamp-Timer stopped.\n" );
	
#ifdef __KERNEL__
	ps = (pScanData)ptr;
#else
    _VAR_NOT_USED( sig_num );
	ps = PtDrvDevices[0];
#endif

	/*
	 * paranoia check!
	 */
	if( NULL == ps )
		return;

	if( _NO_BASE == ps->sCaps.wIOBase )
		return;

	if( _IS_ASIC98(ps->sCaps.AsicID)) {
	    ps->AsicReg.RD_ScanControl &= ~_SCAN_LAMPS_ON;
	} else {
		ps->AsicReg.RD_ScanControl &= ~_SCAN_LAMP_ON;
	}
	
	/* force warmup... */
	ps->bLastLampStatus = 0xFF;
	
	/*
	 * claim parallel port if necessary...
	 * if the port is busy, restart the timer
	 */
	if( _OK != MiscClaimPort(ps)) {
		ptdrvStartLampTimer( ps );
		return;
	}

	IOCmdRegisterToScanner( ps, ps->RegScanControl,
							    ps->AsicReg.RD_ScanControl );
	MiscReleasePort(ps);
}

/**
 */
static void ptdrvStartLampTimer( pScanData ps )
{
#ifndef __KERNEL__
	sigset_t 		 block, pause_mask;
	struct sigaction s;
#ifdef HAVE_SETITIMER
	struct itimerval interval;
#endif

	/* block SIGALRM */
	sigemptyset( &block );
	sigaddset  ( &block, SIGALRM );
	sigprocmask( SIG_BLOCK, &block, &pause_mask );

	/* setup handler */
	sigemptyset( &s.sa_mask );
	sigaddset  (  &s.sa_mask, SIGINT );
	s.sa_flags   = 0;
	s.sa_handler = ptdrvLampTimerIrq;

	if(	sigaction( SIGALRM, &s, NULL ) < 0 ) {
		DBG(DBG_HIGH,"pt_drv%u: Can't setup timer-irq handler\n",ps->devno);
	}

	sigprocmask( SIG_UNBLOCK, &block, &pause_mask );

#ifdef HAVE_SETITIMER
	/*
	 * define a one-shot timer
	 */
	interval.it_value.tv_usec = 0;
	interval.it_value.tv_sec  = ps->lampoff;
	interval.it_interval.tv_usec = 0;
	interval.it_interval.tv_sec  = 0;

	if( 0 != ps->lampoff )
		setitimer( ITIMER_REAL, &interval, &saveSettings );
#else
	alarm( ps->lampoff );
#endif
#else
	init_timer( &tl[ps->devno] );

	/* timeout val in seconds */
	tl[ps->devno].expires  =  jiffies + ps->lampoff * HZ;
	tl[ps->devno].data     = (unsigned long)ps;
	tl[ps->devno].function = ptdrvLampTimerIrq;

	if( 0 != ps->lampoff )
		add_timer( &tl[ps->devno] );
#endif

	DBG( DBG_HIGH, "Lamp-Timer started!\n" );
}

/**
 */
static void ptdrvStopLampTimer( pScanData ps )
{
#ifndef __KERNEL__
	sigset_t block, pause_mask;

	/* block SIGALRM */
	sigemptyset( &block );
	sigaddset  ( &block, SIGALRM );
	sigprocmask( SIG_BLOCK, &block, &pause_mask );
#ifdef HAVE_SETITIMER
	if( 0 != ps->lampoff )
		setitimer( ITIMER_REAL, &saveSettings, NULL );
#else
	_VAR_NOT_USED( ps );
	alarm(0);
#endif
#else
	if( 0 != ps->lampoff )
		del_timer( &tl[ps->devno] );
#endif

	DBG( DBG_HIGH, "Lamp-Timer stopped!\n" );
}

/** claim and initialize the requested port
 */
static int ptdrvOpen( pScanData ps, int portBase )
{
	int retval;

	DBG( DBG_HIGH, "ptdrvOpen(port=0x%x)\n", (int32_t)portBase );
	if( NULL == ps )
		return _E_NULLPTR;

	/*
	 * claim port resources...
	 */
	retval = MiscClaimPort(ps);

	if( _OK != retval )
		return retval;

	return MiscInitPorts( ps, portBase );
}

/** free used memory (if necessary)
 * restore the parallel port settings and release the port
 */
static int ptdrvClose( pScanData ps )
{
	DBG( DBG_HIGH, "ptdrvClose()\n" );
	if( NULL == ps )
		return _E_NULLPTR;

	/*
	 * should be cleared by ioctl(close)
	 */
    if ( NULL != ps->driverbuf ) {
		DBG( DBG_LOW, "*** cleanup buffers ***\n" );
        _VFREE( ps->driverbuf );
        ps->driverbuf = NULL;
    }

    if ( NULL != ps->Shade.pHilight ) {
        _VFREE( ps->Shade.pHilight );
        ps->Shade.pHilight = NULL;
    }

	/*
	 * restore/release port resources...
	 */
	MiscRestorePort( ps );
	MiscReleasePort( ps );

	return _OK;
}

/** will be called during OPEN_DEVICE ioctl call
 */
static int ptdrvOpenDevice( pScanData ps )
{
	int    retval, iobase;
	UShort asic;
	UChar  lastStat;
	UShort lastMode;
	ULong  devno;

#ifdef __KERNEL__
	UShort            flags;
	struct pardevice *pd;
	struct parport   *pp;
	ProcDirDef        procDir;
#else
    int pd;
#endif

	/*
	 * push some values from the struct
     */
#ifdef __KERNEL__
	flags    = ps->flags;
	pp       = ps->pp;
	procDir  = ps->procDir;
#endif
	pd       = ps->pardev;
	iobase   = ps->sCaps.wIOBase;
	asic     = ps->sCaps.AsicID;
	lastStat = ps->bLastLampStatus;
	lastMode = ps->IO.lastPortMode;
	devno    = ps->devno;

	/*
	 * reinit the show
	 */
	ptdrvStopLampTimer( ps );
	MiscReinitStruct  ( ps );

	/*
	 * pop the val(s)
	 */
#ifdef __KERNEL__
	ps->flags   = flags;
	ps->pp      = pp;
	ps->procDir = procDir;
#endif
	ps->pardev          = pd;
	ps->bLastLampStatus = lastStat;
	ps->IO.lastPortMode = lastMode;
	ps->devno           = devno;

#ifdef __KERNEL__
	if( _TRUE == slowIO[devno] ) {
		DBG( DBG_LOW, "Using slow I/O\n" );
		ps->IO.slowIO = _TRUE;
		ps->IO.fnOut  = IOOutDelayed;
		ps->IO.fnIn   = IOInDelayed;
	} else {
		DBG( DBG_LOW, "Using fast I/O\n" );
		ps->IO.slowIO = _FALSE;
		ps->IO.fnOut  = IOOut;
		ps->IO.fnIn   = IOIn;
	}
#endif
	ps->ModelOverride = mov[devno];
	ps->warmup        = warmup[devno];
	ps->lampoff		  = lampoff[devno];
	ps->lOffonEnd	  = lOffonEnd[devno];
	ps->IO.forceMode  = forceMode[devno];

	/*
	 * try to find scanner again
	 */
	retval = ptdrvOpen( ps, iobase );

	if( _OK == retval )
		retval = DetectScanner( ps, asic );
	else
		ptdrvStartLampTimer( ps );

	return retval;
}

/*.............................................................................
 * initialize the driver
 * allocate memory for the ScanData structure and do some presets
 */
static int ptdrvInit( int devno )
{
	int       retval;
	pScanData ps;

	DBG( DBG_HIGH, "ptdrvInit(%u)\n", devno );

	if( devno >= _MAX_PTDEVS )
		return _E_NO_DEV;

	/*
	 * allocate memory for our large ScanData-structure
	 */
	ps = MiscAllocAndInitStruct();
	if( NULL == ps ) {
		return _E_ALLOC;
	}

#ifdef __KERNEL__
	if( _TRUE == slowIO[devno] ) {
		DBG( DBG_LOW, "Using slow I/O\n" );
		ps->IO.slowIO = _TRUE;
		ps->IO.fnOut  = IOOutDelayed;
		ps->IO.fnIn   = IOInDelayed;
	} else {
		DBG( DBG_LOW, "Using fast I/O\n" );
		ps->IO.slowIO = _FALSE;
		ps->IO.fnOut  = IOOut;
		ps->IO.fnIn   = IOIn;
	}
#endif
	ps->ModelOverride = mov[devno];
	ps->warmup        = warmup[devno];
	ps->lampoff       = lampoff[devno];
	ps->lOffonEnd     = lOffonEnd[devno];
	ps->IO.forceMode  = forceMode[devno];
	ps->devno         = devno;

	/* assign it right here, to allow correct shutdown */
	PtDrvDevices[devno] = ps;

	/*
	 * try to register the port
	 */
	retval = MiscRegisterPort( ps, port[devno] );

	if( _OK == retval ) {
		retval = ptdrvOpen( ps, port[devno] );
	}

	/*
	 * try to detect a scanner...
	 */
	if( _OK == retval ) {
		retval = DetectScanner( ps, 0 );

		/* do this here before releasing the port */
		if( _OK == retval ) {
			ptDrvSwitchLampOn( ps );
		}
		ptdrvClose( ps );
	}

	if( _OK == retval ) {

#ifdef __KERNEL__	
		_PRINT( "pt_drv%u: %s found on port 0x%04x\n",
			 devno, MiscGetModelName(ps->sCaps.Model), ps->IO.pbSppDataPort );
#else
		DBG( DBG_LOW, "pt_drv%u: %s found\n",
									 devno, MiscGetModelName(ps->sCaps.Model));
#endif

		/*
		 * initialize the timespan timer
	     */
		MiscStartTimer( &toTimer[ps->devno], (_SECOND * ps->warmup));

		if( 0 == ps->lampoff )
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_LOW,
#endif
					"pt_drv%u: Lamp-Timer switched off.\n", devno );
		else {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_LOW,
#endif
					"pt_drv%u: Lamp-Timer set to %u seconds.\n",
														devno, ps->lampoff );
		}

#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_LOW,
#endif
				"pt_drv%u: WarmUp period set to %u seconds.\n",
														devno, ps->warmup );

		if( 0 == ps->lOffonEnd ) {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_LOW,
#endif
				"pt_drv%u: Lamp untouched on driver unload.\n", devno );
		} else {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_LOW,
#endif
				"pt_drv%u: Lamp switch-off on driver unload.\n", devno );
		}

		ptdrvStartLampTimer( ps );
	}

	return retval;
}

/*.............................................................................
 * shutdown the driver:
 * switch the lights out
 * stop the motor
 * free memory
 */
static int ptdrvShutdown( pScanData ps )
{
	int devno;

	DBG( DBG_HIGH, "ptdrvShutdown()\n" );

	if( NULL == ps )
		return _E_NULLPTR;

	devno = ps->devno;

	DBG( DBG_HIGH, "cleanup device %u\n", devno );

	if( _NO_BASE != ps->sCaps.wIOBase ) {

		ptdrvStopLampTimer( ps );

		if( _OK == MiscClaimPort(ps)) {

			ps->PutToIdleMode( ps );

			if( 0 != ps->lOffonEnd ) {
				if( _IS_ASIC98(ps->sCaps.AsicID)) {
		            ps->AsicReg.RD_ScanControl &= ~_SCAN_LAMPS_ON;
				} else {
					ps->AsicReg.RD_ScanControl &= ~_SCAN_LAMP_ON;
	        	}
				IOCmdRegisterToScanner( ps, ps->RegScanControl,
											  ps->AsicReg.RD_ScanControl );
			}
		}
		MiscReleasePort( ps );
	}

	/* unregister the driver
	 */
	MiscUnregisterPort( ps );

	_KFREE( ps );
	if( devno < _MAX_PTDEVS )
		PtDrvDevices[devno] = NULL;

	return _OK;
}

/*.............................................................................
 * the IOCTL interface
 */
static int ptdrvIoctl( pScanData ps, UInt cmd, pVoid arg )
{
	UShort dir;
	UShort version;
	UInt   size;
	ULong  argVal;
	int    cancel;
	int    retval;

	/*
 	 * do the preliminary stuff here
	 */
	if( NULL == ps )
		return _E_NULLPTR;

	retval = _OK;

	dir  = _IOC_DIR(cmd);
	size = _IOC_SIZE(cmd);

	if ((_IOC_WRITE == dir) && size && (size <= sizeof(ULong))) {

    	if (( retval = getUserPtr( arg, &argVal, size))) {
			DBG( DBG_HIGH, "ioctl() failed - result = %i\n", retval );
      		return retval;
		}
	}

	switch( cmd ) {

	/* open */
    case _PTDRV_OPEN_DEVICE:
		DBG( DBG_LOW, "ioctl(_PTDRV_OPEN_DEVICE)\n" );
   	  	if (copy_from_user(&version, arg, sizeof(UShort)))
			return _E_FAULT;

		if( _PTDRV_IOCTL_VERSION != version ) {
			DBG( DBG_HIGH, "Version mismatch: Backend=0x%04X(0x%04X)",
							version, _PTDRV_IOCTL_VERSION );
			return _E_VERSION;
		}

		retval = ptdrvOpenDevice( ps );
      	break;

	/* close */
	case _PTDRV_CLOSE_DEVICE:
		DBG( DBG_LOW,  "ioctl(_PTDRV_CLOSE_DEVICE)\n" );

	    if ( NULL != ps->driverbuf ) {
			DBG( DBG_LOW, "*** cleanup buffers ***\n" );
	        _VFREE( ps->driverbuf );
    	    ps->driverbuf = NULL;
	    }

    	if ( NULL != ps->Shade.pHilight ) {
        	_VFREE( ps->Shade.pHilight );
	        ps->Shade.pHilight = NULL;
    	}

		ps->PutToIdleMode( ps );
		ptdrvStartLampTimer( ps );
      	break;

	/* get caps - no scanner connection necessary */
    case _PTDRV_GET_CAPABILITIES:
		DBG( DBG_LOW, "ioctl(_PTDRV_GET_CAPABILITES)\n" );

    	return putUserPtr( &ps->sCaps, arg, size);
      	break;

	/* get lens-info - no scanner connection necessary */
    case _PTDRV_GET_LENSINFO:
		DBG( DBG_LOW, "ioctl(_PTDRV_GET_LENSINFO)\n" );

      	return putUserPtr( &ps->LensInf, arg, size);
      	break;

	/* put the image info - no scanner connection necessary */
    case _PTDRV_PUT_IMAGEINFO:
      	{
            short  tmpcx, tmpcy;
      		ImgDef img;

			DBG( DBG_LOW, "ioctl(_PTDRV_PUT_IMAGEINFO)\n" );
			if (copy_from_user( &img, (pImgDef)arg, size))
				return _E_FAULT;

            tmpcx = (short)img.crArea.cx;
            tmpcy = (short)img.crArea.cy;

			if(( 0 >= tmpcx ) || ( 0 >= tmpcy )) {
				DBG( DBG_LOW, "CX or CY <= 0!!\n" );
				return _E_INVALID;
			}

			_ASSERT( ps->GetImageInfo );
	      	ps->GetImageInfo( ps, &img );
		}
      	break;

	/* get crop area - no scanner connection necessary */
    case _PTDRV_GET_CROPINFO:
    	{
      		CropInfo	outBuffer;
      		pCropInfo	pcInf = &outBuffer;

			DBG( DBG_LOW, "ioctl(_PTDRV_GET_CROPINFO)\n" );

			memset( pcInf, 0, sizeof(CropInfo));

	      	pcInf->dwPixelsPerLine = ps->DataInf.dwAppPixelsPerLine;
    	  	pcInf->dwBytesPerLine  = ps->DataInf.dwAppBytesPerLine;
      		pcInf->dwLinesPerArea  = ps->DataInf.dwAppLinesPerArea;
      		return putUserPtr( pcInf, arg, size );
      	}
      	break;

	/* adjust the driver settings */
	case _PTDRV_ADJUST:
		{
			PPAdjDef adj;

			DBG( DBG_LOW, "ioctl(_PTDRV_ADJUST)\n" );

			if (copy_from_user(&adj, (pPPAdjDef)arg, sizeof(PPAdjDef)))
				return _E_FAULT;

			DBG( DBG_LOW, "Adjusting device %u\n", ps->devno );
			DBG( DBG_LOW, "warmup:       %i\n", adj.warmup );
			DBG( DBG_LOW, "lampOff:      %i\n", adj.lampOff );
			DBG( DBG_LOW, "lampOffOnEnd: %i\n", adj.lampOffOnEnd );

			if( ps->devno < _MAX_PTDEVS ) {

				if( adj.warmup >= 0 ) {
					warmup[ps->devno] = adj.warmup;	
					ps->warmup        = adj.warmup;	
				}					

				if( adj.lampOff >= 0 ) {
					lampoff[ps->devno] = adj.lampOff;
					ps->lampoff        = adj.lampOff;
				}					

				if( adj.lampOffOnEnd >= 0 ) {
					lOffonEnd[ps->devno] = adj.lampOffOnEnd;
					ps->lOffonEnd        = adj.lampOffOnEnd;
				}					
			}
		}
		break;

	/* set a specific map (r,g,b or gray) */
	case _PTDRV_SETMAP:
		{
			int     i, x_len;
			MapDef  map;

			DBG( DBG_LOW, "ioctl(_PTDRV_SETMAP)\n" );

			if (copy_from_user( &map, (pMapDef)arg, sizeof(MapDef)))
				return _E_FAULT;

			DBG( DBG_LOW, "maplen=%u, mapid=%u, addr=0x%08lx\n",
							map.len, map.map_id, (u_long)map.map );

			x_len = 256;
			if( _IS_ASIC98(ps->sCaps.AsicID))
				x_len = 4096;
			
			/* check for 0 pointer and len */
			if((NULL == map.map) || (x_len != map.len)) {
				DBG( DBG_LOW, "map pointer == 0, or map len invalid!!\n" );
				return _E_INVALID;
			}	
			
    		if( _MAP_MASTER == map.map_id ) {

				for( i = 0; i < 3; i++ ) {
					if (copy_from_user((pVoid)&ps->a_bMapTable[x_len * i],
					                    map.map, x_len )) {
						return _E_FAULT;
					}
				}
			} else {

				u_long idx = 0;
				if( map.map_id == _MAP_GREEN )
					idx = 1;
				if( map.map_id == _MAP_BLUE )
					idx = 2;

				if (copy_from_user((pVoid)&ps->a_bMapTable[x_len * idx],
				                   map.map, x_len )) {
						return _E_FAULT;
				}
			}
			
			/* here we adjust the maps according to
			 * the brightness and contrast settings
			 */
			MapAdjust( ps, map.map_id );
		}
		break;

	/* set environment - no scanner connection necessary */
    case _PTDRV_SET_ENV:
      	{
			ScanInfo sInf;

			DBG( DBG_LOW, "ioctl(_PTDRV_SET_ENV)\n" );

			if (copy_from_user(&sInf, (pScanInfo)arg, sizeof(ScanInfo)))
				return _E_FAULT;

			/*
			 * to make the OpticPro 4800P work, we need to invert the
			 * Inverse flag
			 */
			if( _ASIC_IS_96001 == ps->sCaps.AsicID ) {
				if( SCANDEF_Inverse & sInf.ImgDef.dwFlag )
					sInf.ImgDef.dwFlag &= ~SCANDEF_Inverse;
				else
					sInf.ImgDef.dwFlag |= SCANDEF_Inverse;
			}

			_ASSERT( ps->SetupScanSettings );
      		retval = ps->SetupScanSettings( ps, &sInf );

			/* CHANGE preset map here */
			if( _OK == retval ) {
				MapInitialize ( ps );
				MapSetupDither( ps );

				ps->DataInf.dwVxdFlag |= _VF_ENVIRONMENT_READY;

				if (copy_to_user((pScanInfo)arg, &sInf, sizeof(ScanInfo)))
					return _E_FAULT;
			}
		}
		break;

	/* start scan */
	case _PTDRV_START_SCAN:
		{
			StartScan  outBuffer;
			pStartScan pstart = (pStartScan)&outBuffer;

			DBG( DBG_LOW, "ioctl(_PTDRV_START_SCAN)\n" );

			retval = IOIsReadyForScan( ps );
			if( _OK == retval ) {

				ps->dwDitherIndex      = 0;
				ps->fScanningStatus    = _TRUE;
				pstart->dwBytesPerLine = ps->DataInf.dwAppBytesPerLine;
				pstart->dwLinesPerScan = ps->DataInf.dwAppLinesPerArea;
				pstart->dwFlag 		   = ps->DataInf.dwScanFlag;

				ps->DataInf.dwVxdFlag |= _VF_FIRSTSCANLINE;
				ps->DataInf.dwScanFlag&=~(_SCANNER_SCANNING|_SCANNER_PAPEROUT);

				if (copy_to_user((pStartScan)arg, pstart, sizeof(StartScan)))
					return _E_FAULT;
			}
		}
		break;

	/* stop scan */
    case _PTDRV_STOP_SCAN:

		DBG( DBG_LOW, "ioctl(_PTDRV_STOP_SCAN)\n" );

		if (copy_from_user(&cancel, arg, sizeof(short)))
			return _E_FAULT;

		/* we may use this to abort scanning! */
		ps->fScanningStatus = _FALSE;

		/* when using this to cancel, then that's all */
		if( _FALSE == cancel ) {

			MotorToHomePosition( ps );

    		ps->DataInf.dwAppLinesPerArea = 0;
      		ps->DataInf.dwScanFlag &= ~_SCANNER_SCANNING;

			/* if environment was never set */
    	  	if (!(ps->DataInf.dwVxdFlag & _VF_ENVIRONMENT_READY))
        		retval = _E_SEQUENCE;

	      	ps->DataInf.dwVxdFlag &= ~_VF_ENVIRONMENT_READY;
		
		} else {
			DBG( DBG_LOW, "CANCEL Mode set\n" );
		}
		retval = putUserVal(retval, arg, size);
      	break;

	/* read the flag status register, when reading the action button, you must
	 * only do this call and none of the other ioctl's
     * like open, etc or it will always show up as "1"
	 */
	case _PTDRV_ACTION_BUTTON:
		DBG( DBG_LOW, "ioctl(_PTDRV_ACTION_BUTTON)\n" );
		IODataRegisterFromScanner( ps, ps->RegStatus );
      	retval = putUserVal( argVal, arg, size );
		break;

	default:
		retval = _E_NOSUPP;
      	break;
	}

	return retval;
}

/*.............................................................................
 * read the data
 */
static int ptdrvRead( pScanData ps, pUChar buffer, int count )
{
	pUChar	scaleBuf;
	ULong	dwLinesRead = 0;
	int 	retval      = _OK;

#ifdef _ASIC_98001_SIM
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_LOW,
#endif
					"pt_drv : Software-Emulation active, can't read!\n" );
	return _E_INVALID;
#endif

	if((NULL == buffer) || (NULL == ps)) {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
						"pt_drv :  Internal NULL-pointer!\n" );
		return _E_NULLPTR;
	}

	if( 0 == count ) {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
			"pt_drv%u: reading 0 bytes makes no sense!\n", ps->devno );
		return _E_INVALID;
	}

	if( _FALSE == ps->fScanningStatus )
		return _E_ABORT;
		
	/*
	 * has the environment been set ?
	 * this should prevent the driver from causing a seg-fault
	 * when using the cat /dev/pt_drv command!
	 */
   	if (!(ps->DataInf.dwVxdFlag & _VF_ENVIRONMENT_READY)) {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
			"pt_drv%u:  Cannot read, driver not initialized!\n",ps->devno);
		return _E_SEQUENCE;
	}

	/*
	 * get some memory
	 */
	ps->Scan.bp.pMonoBuf = _KALLOC( ps->DataInf.dwAppPhyBytesPerLine, GFP_KERNEL);

	if ( NULL == ps->Scan.bp.pMonoBuf ) {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
			"pt_drv%u:  Not enough memory available!\n", ps->devno );
    	return _E_ALLOC;
	}

	/* if we have to do some scaling, we need another buffer... */
	if( ps->DataInf.XYRatio > 1000 ) {

		scaleBuf = _KALLOC( ps->DataInf.dwAppPhyBytesPerLine, GFP_KERNEL);
		if ( NULL == scaleBuf ) {
			_KFREE( ps->Scan.bp.pMonoBuf );
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
			"pt_drv%u:  Not enough memory available!\n", ps->devno );
    		return _E_ALLOC;
		}
	} else {
		scaleBuf = NULL;
	}

	DBG( DBG_LOW, "PtDrvRead(%u bytes)*****************\n", count );
	DBG( DBG_LOW, "MonoBuf = 0x%08lx[%u], scaleBuf = 0x%lx\n",
			(unsigned long)ps->Scan.bp.pMonoBuf,
            ps->DataInf.dwAppPhyBytesPerLine, (unsigned long)scaleBuf );

	/*	
	 * in case of a previous problem, move the sensor back home
	 */
  	MotorToHomePosition( ps );

	if( _FALSE == ps->fScanningStatus ) {
		retval = _E_ABORT;
		goto ReadFinished;
	}	
  	
	dwLinesRead = 0;

	/*
	 * first of all calibrate the show
	 */
	ps->bMoveDataOutFlag   = _DataInNormalState;
   	ps->fHalfStepTableFlag = _FALSE;
    ps->fReshaded          = _FALSE;
   	ps->fScanningStatus    = _TRUE;

    if( _ASIC_IS_98003 == ps->sCaps.AsicID )
        ps->Scan.fRefreshState = _FALSE;
    else
        ps->Scan.fRefreshState = _TRUE;

	ptdrvLampWarmup( ps );

	if( _FALSE == ps->fScanningStatus ) {
		retval = _E_ABORT;
		goto ReadFinished;
	}

    retval = ps->Calibration( ps );
	if( _OK != retval ) {
#ifdef __KERNEL__
		_PRINT(
#else
		DBG( DBG_HIGH,
#endif
			"pt_drv%u: calibration failed, result = %i\n",
														ps->devno, retval );
		goto ReadFinished;
	}

    if( _ASIC_IS_98003 == ps->sCaps.AsicID ) {

        ps->OpenScanPath( ps );

    	MotorP98003ForceToLeaveHomePos( ps );
    }

	_ASSERT(ps->SetupScanningCondition);
  	ps->SetupScanningCondition(ps);

    if( _ASIC_IS_98003 != ps->sCaps.AsicID ) {
    	ps->SetMotorSpeed( ps, ps->bCurrentSpeed, _TRUE );
        IOSetToMotorRegister( ps );
    } else {

        ps->WaitForPositionY( ps );
    	_DODELAY( 70 );
    	ps->Scan.bOldScanState = IOGetScanState( ps, _TRUE ) & _SCANSTATE_MASK;
    }

    ps->DataInf.dwScanFlag |= _SCANNER_SCANNING;

	if( _FALSE == ps->fScanningStatus ) {
		DBG( DBG_HIGH, "read aborted!\n" );
		retval = _E_ABORT;
		goto ReadFinished;
	}

	/*
	 * now get the picture data
	 */
	DBG( DBG_HIGH, "dwAppLinesPerArea = %d\n", ps->DataInf.dwAppLinesPerArea);
	DBG( DBG_HIGH, "dwAppBytesPerLine = %d\n", ps->DataInf.dwAppBytesPerLine);

/* HEINER: A3I
	ps->bMoveDataOutFlag = _DataFromStopState;
*/
  	if ( 0 != ps->DataInf.dwAppLinesPerArea ) {

	   	ps->Scan.dwLinesToRead = count / ps->DataInf.dwAppBytesPerLine;

    	if( ps->Scan.dwLinesToRead ) {

        	DBG( DBG_HIGH, "dwLinesToRead = %d\n", ps->Scan.dwLinesToRead );

      		if( ps->Scan.dwLinesToRead > ps->DataInf.dwAppLinesPerArea )
        		ps->Scan.dwLinesToRead = ps->DataInf.dwAppLinesPerArea;
      	
			ps->DataInf.dwAppLinesPerArea -= ps->Scan.dwLinesToRead;

      		if (ps->DataInf.dwScanFlag & SCANDEF_BmpStyle)
        		buffer += ((ps->Scan.dwLinesToRead - 1) *
                   				ps->DataInf.dwAppBytesPerLine);

			if (ps->DataInf.dwVxdFlag & _VF_DATATOUSERBUFFER)
        		ps->DataInf.pCurrentBuffer = ps->Scan.bp.pMonoBuf;

			while(ps->fScanningStatus && ps->Scan.dwLinesToRead) {

        		_ASSERT(ps->ReadOneImageLine);
		   		if (!ps->ReadOneImageLine(ps)) {
        			ps->fScanningStatus = _FALSE;
            		DBG( DBG_HIGH, "ReadOneImageLine() failed at line %u!\n",
                                    dwLinesRead );
					break;
				}

				/*
				 * as we might scan images that exceed the CCD-capabilities
				 * in x-resolution, we have to enlarge the line data
				 * i.e.: scanning at 1200dpi generates on a P9636 600 dpi in
				 *       x-direction but 1200dpi in y-direction...
				 */
				if( NULL != scaleBuf ) {
					ScaleX( ps, ps->Scan.bp.pMonoBuf, scaleBuf );
	    	    	if (copy_to_user( buffer, scaleBuf,
									ps->DataInf.dwAppPhyBytesPerLine)) {
						return _E_FAULT;
					}
				} else {
	    	    	if (copy_to_user( buffer, ps->Scan.bp.pMonoBuf,
								  ps->DataInf.dwAppPhyBytesPerLine)) {
						return _E_FAULT;
					}
				}

				buffer += ps->Scan.lBufferAdjust;
				dwLinesRead++;
				ps->Scan.dwLinesToRead--;

				/* needed, esp. to avoid freezing the system in SPP mode */
#ifdef __KERNEL__
				schedule();
/*#else
				sched_yield();
*/
#endif
        	}

			if (ps->fScanningStatus) {

            	if( _IS_ASIC96(ps->sCaps.AsicID))
          			MotorP96SetSpeedToStopProc(ps);

			} else {
        		if (ps->DataInf.dwScanFlag & (SCANDEF_StopWhenPaperOut |
                    	                         SCANDEF_UnlimitLength)) {
          			ps->DataInf.dwAppLinesPerArea = 0;
				} else {
        			if (ps->DataInf.dwScanFlag & SCANDEF_BmpStyle)
            			buffer -= (ps->DataInf.dwAppBytesPerLine *
                	    		   (ps->Scan.dwLinesToRead - 1));
          			memset( buffer, 0xff,
          		   			ps->Scan.dwLinesToRead * ps->DataInf.dwAppBytesPerLine );
        	  		dwLinesRead += ps->Scan.dwLinesToRead;
    	    	}
	        }

      	} else {
      		retval = _E_INTERNAL;
		}
	}

	if( _FALSE == ps->fScanningStatus ) {
		DBG( DBG_HIGH, "read aborted!\n" );
		retval = _E_ABORT;
	}

ReadFinished:


    if( _ASIC_IS_98003 == ps->sCaps.AsicID )
        ps->CloseScanPath( ps );

	if( NULL != ps->Scan.bp.pMonoBuf )
		_KFREE( ps->Scan.bp.pMonoBuf );

	if( NULL != scaleBuf )
		_KFREE( scaleBuf );

	/*
	 * on success return number of bytes red
	 */
	if ( _OK == retval )
    	return (ps->DataInf.dwAppPhyBytesPerLine * dwLinesRead);

   	return retval;
}

/*************************** the module interface ****************************/

#ifdef __KERNEL__		/* the kernel module interface */

/* Designed to be used as a module */
#ifdef MODULE

/*.............................................................................
 * gets called upon module initialization
 */
#ifdef LINUX_26
static int __init ptdrv_init( void )
#else
int init_module( void )
#endif
{
    UInt devCount;
    UInt i;
    int  retval = _OK;
    int  result = _OK;
#if (defined(CONFIG_DEVFS_FS) && !defined(DEVFS_26_STYLE))
    char controlname[24];
#endif
# ifdef LINUX_26
    char devname[20];
#endif

    DBG( DBG_HIGH, "*********************************************\n" );
    DBG( DBG_HIGH, "pt_drv: init_module()\n" );

#if (defined(CONFIG_DEVFS_FS) && !defined(DEVFS_26_STYLE))
	devfs_handle = devfs_mk_dir(NULL, "scanner", NULL);
	if( devfs_register_chrdev(_PTDRV_MAJOR, _DRV_NAME, &pt_drv_fops)) {
#else
	if( register_chrdev(_PTDRV_MAJOR, _DRV_NAME, &pt_drv_fops)) {
#endif

		_PRINT(KERN_INFO "pt_drv: unable to get major %d for pt_drv devices\n",
		       _PTDRV_MAJOR);
		return -EIO;
	}
	printk( KERN_INFO "pt_drv : driver version "_PTDRV_VERSTR"\n" );

#if !defined (CONFIG_DEVFS_FS) && defined (LINUX_26)
	ptdrv_class = class_create(THIS_MODULE, "scanner");
	if (IS_ERR(ptdrv_class))
		goto out_devfs;
#endif

	/* register the proc_fs */
	ProcFsInitialize();

	/* go through the list of defined ports and try to find a device
	 */
	devCount = 0;
	for( i = 0; i < _MAX_PTDEVS; i++ ) {

		if( 0 != port[i] ) {
			result = ptdrvInit( i );

			if ( _OK == result ) {
		    	PtDrvDevices[i]->flags |= _PTDRV_INITALIZED;

#ifdef CONFIG_DEVFS_FS
# ifndef DEVFS_26_STYLE
				sprintf( controlname, "scanner/pt_drv%d", devCount );
				devfs_register( NULL, controlname,
				                DEVFS_FL_DEFAULT, _PTDRV_MAJOR, 0,
			                    (S_IFCHR | S_IRUGO | S_IWUGO | S_IFCHR),
				                &pt_drv_fops, NULL );
# else /* DEVFS_26_STYLE */
				devfs_mk_cdev(MKDEV(_PTDRV_MAJOR, devCount), 
				    (S_IFCHR | S_IRUGO | S_IWUGO | S_IFCHR),
				    "scanner/pt_drv%d", devCount);
# endif
#else
# ifdef LINUX_26
				sprintf(devname, "pt_drv%d", devCount);
				CLASS_DEV_CREATE(ptdrv_class,
				                 MKDEV(_PTDRV_MAJOR, devCount), NULL,
				                 devname);

# endif /* LINUX_26 */
#endif /* CONFIG_DEVFS_FS */
				ProcFsRegisterDevice( PtDrvDevices[i] );
				devCount++;
			} else {
				retval = result;
				ptdrvShutdown( PtDrvDevices[i] );
				PtDrvDevices[i] = NULL;
			}
		}
	}

	/* * if something went wrong, shutdown all... */
	if( devCount == 0 ) {

#if !defined (CONFIG_DEVFS_FS) && defined (LINUX_26)
out_devfs:
		class_destroy(ptdrv_class);
#endif

#if (defined(CONFIG_DEVFS_FS) && !defined(DEVFS_26_STYLE))
		devfs_unregister_chrdev( _PTDRV_MAJOR, _DRV_NAME );
#else
		unregister_chrdev( _PTDRV_MAJOR, _DRV_NAME );
#endif
		ProcFsShutdown();

#ifdef __KERNEL__
		_PRINT( KERN_INFO "pt_drv : no device(s) detected, (%i)\n", retval );
#endif

	} else {

		DBG( DBG_HIGH, "pt_drv : init done, %u device(s) found\n", devCount );
		retval = _OK;
	}
	DBG( DBG_HIGH, "---------------------------------------------\n" );

	deviceScanning = _FALSE;
	return retval;
}

/*.............................................................................
 * cleanup the show
 */
#ifdef LINUX_26
static void __exit ptdrv_exit( void )
#else
void cleanup_module( void )
#endif
{
	UInt      i;
	pScanData ps;
#if (defined(CONFIG_DEVFS_FS) && !defined(DEVFS_26_STYLE))
	char           controlname[24];
	devfs_handle_t master;
#endif

	DBG( DBG_HIGH, "pt_drv: cleanup_module()\n" );

	for ( i = 0; i < _MAX_PTDEVS; i++ ) {

		ps = PtDrvDevices[i];
		PtDrvDevices[i] = NULL;

		if ( NULL != ps ) {
#ifdef CONFIG_DEVFS_FS
# ifndef DEVFS_26_STYLE
			sprintf( controlname, "scanner/pt_drv%d", i );
			master = devfs_find_handle( NULL,controlname, 0, 0,
			                            DEVFS_SPECIAL_CHR, 0 );
			devfs_unregister( master );
# else
			devfs_remove("scanner/pt_drv%d", i);
# endif
#else
# ifdef LINUX_26
			CLASS_DEV_DESTROY(ptdrv_class, MKDEV(_PTDRV_MAJOR, i));
# endif /* LINUX_26 */
#endif /* CONFIG_DEVFS_FS */
			ptdrvShutdown( ps );
			ProcFsUnregisterDevice( ps );
		}
	}

#if (defined(CONFIG_DEVFS_FS) && !defined(DEVFS_26_STYLE))
	devfs_unregister_chrdev( _PTDRV_MAJOR, _DRV_NAME );
#else
	unregister_chrdev( _PTDRV_MAJOR, _DRV_NAME );
#endif
	ProcFsShutdown();

#if !defined (CONFIG_DEVFS_FS) && defined (LINUX_26)
	class_destroy(ptdrv_class);
#endif

	DBG( DBG_HIGH, "pt_drv: cleanup done.\n" );
	DBG( DBG_HIGH, "*********************************************\n" );
}

#ifdef LINUX_26
module_init(ptdrv_init);
module_exit(ptdrv_exit);
#endif

#endif /*MODULE*/


/*.............................................................................
 * device open...
 */
static int pt_drv_open(struct inode *inode, struct file *file)
{
	pScanData ps;

	DBG( DBG_HIGH, "pt_drv_open()\n" );

	ps = get_pt_from_inode(inode);

	if ( NULL == ps ) {
		return(-ENXIO);
	}

	/* device not found ? */
	if (!(ps->flags & _PTDRV_INITALIZED)) {
		return(-ENXIO);
	}

	/* device is busy ? */
	if (ps->flags & _PTDRV_OPEN) {
		return(-EBUSY);
	}

#ifdef LINUX_26
	if (!try_module_get(THIS_MODULE))
		return -EAGAIN;
#else
	MOD_INC_USE_COUNT;
#endif    
	ps->flags |= _PTDRV_OPEN;

	return _OK;
}

/*.............................................................................
 * device close...
 */
static CLOSETYPE pt_drv_close(struct inode * inode, struct file * file)
{
	pScanData ps;

	DBG( DBG_HIGH, "pt_drv_close()\n" );

	if ((ps = get_pt_from_inode(inode)) ) {

		ptdrvClose( ps );

    	ps->flags &= ~_PTDRV_OPEN;
#ifdef LINUX_26
		module_put(THIS_MODULE);
#else
    	MOD_DEC_USE_COUNT;
#endif     
	    CLOSERETURN(0);
	} else {

		DBG( DBG_HIGH, "pt_drv: - close failed!\n" );
		CLOSERETURN(-ENXIO);
	}
}

/*.............................................................................
 * read data from device
 */
#ifdef LINUX_20
static int pt_drv_read(struct inode *inode, struct file *file,
                       char *buffer, int count)
{
	int		  result;
	pScanData ps;

	if ( !(ps = get_pt_from_inode(inode)))
    	return(-ENXIO);
#else
static ssize_t pt_drv_read( struct file *file,
                             char *buffer, size_t count, loff_t *tmp )
{
	int       result;
	pScanData ps;

	if ( !(ps = get_pt_from_inode(file->f_dentry->d_inode)) )
		return(-ENXIO);
#endif
	if ((result = verify_area_20(VERIFY_WRITE, buffer, count)))
		return result;

	/*
	 * as the driver contains some global vars, it is not
	 * possible to scan simultaenously with two or more devices
	 */
	if( _TRUE == deviceScanning ) {
	    printk( KERN_INFO "pt_drv: device %u busy!!!\n", ps->devno );
		return(-EBUSY);		
	}

	deviceScanning = _TRUE;

	result = ptdrvRead( ps, buffer, count );

	deviceScanning = _FALSE;
	return result;
}

/*.............................................................................
 * writing makes no sense
 */
#ifdef LINUX_20
static int pt_drv_write(struct inode * inode, struct file * file,
                        const char * buffer, int count)
{
  return -EPERM;
}
#else
 static ssize_t pt_drv_write( struct file * file,const char * buffer,
                              size_t tmp,loff_t* count)
{
  return -EPERM;
}
#endif

/*.............................................................................
 * the ioctl interface
 */
#ifdef NOLOCK_IOCTL
static long pt_drv_ioctl( struct file *file, UInt cmd, unsigned long arg )
{
	pScanData ps;

	if ( !(ps = get_pt_from_inode(file->f_dentry->d_inode)) )
    	return(-ENXIO);

  	return ptdrvIoctl( ps, cmd, (pVoid)arg);
}
#else
static int pt_drv_ioctl( struct inode *inode, struct file *file,
                         UInt cmd, unsigned long arg )
{
	pScanData ps;

	if ( !(ps = get_pt_from_inode(inode)) )
    	return(-ENXIO);

  	return ptdrvIoctl( ps, cmd, (pVoid)arg);
}
#endif

#else	/* the user-mode interface */

/*.............................................................................
 * here we only have wrapper functions
 */
static int PtDrvInit( const char *dev_name, UShort model_override )
{
	int fd;
	int result = _OK;

	if( _TRUE == PtDrvInitialized )
		return _OK;

	result = sanei_pp_open( dev_name, &fd );
	if( SANE_STATUS_GOOD != result )
		return result;

	port[0] = fd;
	mov[0]  = model_override;
	
	result = ptdrvInit( 0 );

	if( _OK == result ) {
		PtDrvInitialized = _TRUE;
	} else {
		ptdrvShutdown( PtDrvDevices[0] );
	}

	return result;
}

static int PtDrvShutdown( void )
{
	int result;

	if( _FALSE == PtDrvInitialized )
		return _E_NOT_INIT;

	result = ptdrvShutdown( PtDrvDevices[0] );

	PtDrvInitialized = _FALSE;

	return result;
}

static int PtDrvOpen( void )
{
	if( _FALSE == PtDrvInitialized )
		return _E_NOT_INIT;

	return _OK;
}

static int PtDrvClose( void )
{
	if( _FALSE == PtDrvInitialized )
		return _E_NOT_INIT;

	return ptdrvClose( PtDrvDevices[0] );
}

static int PtDrvIoctl( UInt cmd, pVoid arg )
{
	if( _FALSE == PtDrvInitialized )
		return _E_NOT_INIT;

	return ptdrvIoctl( PtDrvDevices[0], cmd, arg);
}

static int PtDrvRead ( pUChar buffer, int count )
{
	if( _FALSE == PtDrvInitialized )
		return _E_NOT_INIT;

	return ptdrvRead( PtDrvDevices[0], buffer, count );
}

#endif /* guard __KERNEL__ */

/* END PLUSTEK-PP_PTDRV.C ...................................................*/
