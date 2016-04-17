/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek.c
 *  @brief SANE backend for Plustek scanner
 *
 * Based on Kazuhiro Sasayama previous work on
 * plustek.[ch] file from the SANE package.<br>
 * Original code taken from sane-0.71<br>
 * Copyright (C) 1997 Hypercore Software Design, Ltd.<br>
 * Also based on the work done by Rick Bronson<br>
 * Copyright (C) 2000-2007 Gerhard Jaeger <gerhard@gjaeger.de><br>
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - no changes
 * - 0.33 - no changes
 * - 0.34 - moved some definitions and typedefs to plustek.h
 * - 0.35 - removed Y-correction for 12000P model<br>
 *        - getting Y-size of scan area from driver
 * - 0.36 - disabled Dropout, as this does currently not work<br>
 *        - enabled Halftone selection only for Halftone-mode<br>
 *        - made the cancel button work by using a child process during read<br>
 *        - added version code to driver interface<br>
 *        - cleaned up the code<br>
 *        - fixed sane compatibility problems<br>
 *        - added multiple device support<br>
 *        - 12bit color-depth are now available for scanimage
 * - 0.37 - removed X/Y autocorrection, now correcting the stuff
 *          before scanning
 *        - applied Michaels' patch to solve the sane_get_parameter problem<br>
 *        - getting X-size of scan area from driver<br>
 *        - applied Michaels patch for OPT_RESOLUTION (SANE_INFO_INEXACT stuff)
 * - 0.38 - now using the information from the driver
 *        - some minor fixes<br>
 *        - removed dropout stuff<br>
 *        - removed some warning conditions
 * - 0.39 - added stuff to use the backend completely in user mode<br>
 *        - fixed a potential buffer problem<br>
 *        - removed function attach_one()<br>
 *        - added USB interface stuff
 * - 0.40 - USB scanning works now
 * - 0.41 - added some configuration stuff and also changed .conf file<br>
 *        - added call to sanei_usb_init() and sanei_lm983x_init()
 * - 0.42 - added adjustment stuff<br>
 *        - added custom gamma tables<br>
 *        - fixed a problem with the "size-sliders"<br>
 *        - fixed a bug that causes segfault when using the autodetection for
 *          USB devices<br>
 *        - added OS/2 switch to disable the USB stuff for OS/2
 * - 0.43 - added support for PREVIEW flag
 * - 0.44 - added _DBG_DUMP debug level<br>
 *        - fixed a bug, that stops our lamp timer
 * - 0.45 - added additional flags
 *        - added WIFSIGNALED to check result of child termination
 *        - changed readImage interface for USB devices
 *        - homeing of USB scanner is now working correctly
 * - 0.46 - added plustek-usbcal.c for extra CIS device calibration
 *          based on Montys' great work
 *        - added altCalibration option
 *        - removed parallelport support --> new backend: plustek_pp
 *        - cleanup
 *        - added sanei_thread support
 * - 0.47 - added mov-option (model override)
 *        - removed drvOpen
 *        - added call to usb_StartLampTimer, when we're using
 *          SIGALRM for lamp timer
 *        - closing now writer pipe, when reader_process is done
 * - 0.48 - added additional options
 *          split scanmode and bit-depth
 * - 0.49 - improved multi-device capability
 *        - tweaked some device settings
 *        - added button support
 *        - moved AFE stuff to enhanced options
 * - 0.50 - cleanup
 *        - activated IPC stuff
 *        - added _DBG_DCALDATA for fine calibration data logging
 *        - added OPT_SPEEDUP handling
 *        - fixed constraint_type for OPT_BUTTON
 * - 0.51 - added fine calibration caching
 *        - removed #define _PLUSTEK_USB
 * - 0.52 - added skipDarkStrip and OPT_LOFF4DARK to frontend options
 *        - fixed batch scanning
 *.
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

/** @mainpage
 *  @verbinclude Plustek-USB.txt
 */
  
#ifdef _AIX
# include "../include/lalloca.h" 
#endif

#include "../include/sane/config.h"
#include "../include/lalloca.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <sys/ioctl.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_VERSION "0.52-12"

#define BACKEND_NAME    plustek
#include "../include/sane/sanei_access.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_thread.h"

#define USE_IPC

#include "plustek-usb.h"
#include "plustek.h"

/*********************** the debug levels ************************************/

#define _DBG_FATAL      0
#define _DBG_ERROR      1
#define _DBG_WARNING    3
#define _DBG_INFO       5
#define _DBG_PROC       7
#define _DBG_SANE_INIT 10
#define _DBG_INFO2     15
#define _DBG_DREGS     20
#define _DBG_DCALDATA  22
#define _DBG_DPIC      25
#define _DBG_READ      30

/*****************************************************************************/

#define _SECTION        "[usb]"
#define _DEFAULT_DEVICE "auto"

/** to disable the backend... */

/* declare it here, as it's used in plustek-usbscan.c too :-( */
static SANE_Bool  cancelRead;
static DevList   *usbDevs;

/* the USB-stuff... I know this is in general no good idea, but it works */
#include "plustek-usbio.c"
#include "plustek-usbdevs.c"
#include "plustek-usbhw.c"
#include "plustek-usbmap.c"
#include "plustek-usbscan.c"
#include "plustek-usbimg.c"
#include "plustek-usbcalfile.c"
#include "plustek-usbshading.c"
#include "plustek-usbcal.c"
#include "plustek-usb.c"

/************************** global vars **************************************/

static int                 num_devices;
static Plustek_Device     *first_dev;
static Plustek_Scanner    *first_handle;
static const SANE_Device **devlist = 0;
static unsigned long       tsecs   = 0;
static Plustek_Scanner    *sc = NULL;

static const SANE_Int bpp_lm9832_list [] = { 2, 8, 14 };
static const SANE_Int bpp_lm9833_list [] = { 2, 8, 16 };

static const SANE_String_Const mode_list[] =
{
	SANE_VALUE_SCAN_MODE_LINEART,
	SANE_VALUE_SCAN_MODE_GRAY,
	SANE_VALUE_SCAN_MODE_COLOR,
	NULL
};

static const SANE_String_Const source_list[] =
{
	SANE_I18N("Normal"),
	SANE_I18N("Transparency"),
	SANE_I18N("Negative"),
	NULL
};

static const SANE_Range percentage_range =
{
	-100 << SANE_FIXED_SCALE_SHIFT, /* minimum      */
	 100 << SANE_FIXED_SCALE_SHIFT, /* maximum      */
	   1 << SANE_FIXED_SCALE_SHIFT  /* quantization */
};

static const SANE_Range warmup_range   = { -1,   999, 1 };
static const SANE_Range offtimer_range = {  0,   999, 1 };
static const SANE_Range gain_range     = { -1,    63, 1 };
static const SANE_Range loff_range     = { -1, 16363, 1 };

/* authorization stuff */
static SANE_Auth_Callback auth = NULL;

/* prototype... */
static SANE_Status local_sane_start(Plustek_Scanner *, int);

/****************************** the backend... *******************************/

#define _YN(x) (x?"yes":"no")

/** function to display the configuration options for the current device
 * @param cnf - pointer to the configuration structure whose content should be
 *              displayed
 */
static void show_cnf( CnfDef *cnf )
{
	DBG( _DBG_SANE_INIT,"Device configuration:\n" );
	DBG( _DBG_SANE_INIT,"device name  : >%s<\n",cnf->devName                 );
	DBG( _DBG_SANE_INIT,"USB-ID       : >%s<\n",cnf->usbId                   );
	DBG( _DBG_SANE_INIT,"model ovr.   : %d\n",  cnf->adj.mov                 );
	DBG( _DBG_SANE_INIT,"warmup       : %ds\n", cnf->adj.warmup              );
	DBG( _DBG_SANE_INIT,"lampOff      : %d\n",  cnf->adj.lampOff             );
	DBG( _DBG_SANE_INIT,"lampOffOnEnd : %s\n",  _YN(cnf->adj.lampOffOnEnd   ));
	DBG( _DBG_SANE_INIT,"cacheCalData : %s\n",  _YN(cnf->adj.cacheCalData   ));
	DBG( _DBG_SANE_INIT,"altCalibrate : %s\n",  _YN(cnf->adj.altCalibrate   ));
	DBG( _DBG_SANE_INIT,"skipCalibr.  : %s\n",  _YN(cnf->adj.skipCalibration));
	DBG( _DBG_SANE_INIT,"skipFine     : %s\n",  _YN(cnf->adj.skipFine       ));
	DBG( _DBG_SANE_INIT,"skipFineWhite: %s\n",  _YN(cnf->adj.skipFineWhite  ));
	DBG( _DBG_SANE_INIT,"skipDarkStrip: %s\n",  _YN(cnf->adj.skipDarkStrip   ));
	DBG( _DBG_SANE_INIT,"incDarkTarget: %s\n",  _YN(cnf->adj.incDarkTgt      ));
	DBG( _DBG_SANE_INIT,"invertNegs.  : %s\n",  _YN(cnf->adj.invertNegatives));
	DBG( _DBG_SANE_INIT,"dis.Speedup  : %s\n",  _YN(cnf->adj.disableSpeedup ));
	DBG( _DBG_SANE_INIT,"pos_x        : %d\n",  cnf->adj.pos.x               );
	DBG( _DBG_SANE_INIT,"pos_y        : %d\n",  cnf->adj.pos.y               );
	DBG( _DBG_SANE_INIT,"pos_shading_y: %d\n",  cnf->adj.posShadingY         );
	DBG( _DBG_SANE_INIT,"neg_x        : %d\n",  cnf->adj.neg.x               );
	DBG( _DBG_SANE_INIT,"neg_y        : %d\n",  cnf->adj.neg.y               );
	DBG( _DBG_SANE_INIT,"neg_shading_y: %d\n",  cnf->adj.negShadingY         );
	DBG( _DBG_SANE_INIT,"tpa_x        : %d\n",  cnf->adj.tpa.x               );
	DBG( _DBG_SANE_INIT,"tpa_y        : %d\n",  cnf->adj.tpa.y               );
	DBG( _DBG_SANE_INIT,"tpa_shading_y: %d\n",  cnf->adj.tpaShadingY         );
	DBG( _DBG_SANE_INIT,"red gain     : %d\n",  cnf->adj.rgain               );
	DBG( _DBG_SANE_INIT,"green gain   : %d\n",  cnf->adj.ggain               );
	DBG( _DBG_SANE_INIT,"blue gain    : %d\n",  cnf->adj.bgain               );
	DBG( _DBG_SANE_INIT,"red offset   : %d\n",  cnf->adj.rofs                );
	DBG( _DBG_SANE_INIT,"green offset : %d\n",  cnf->adj.gofs                );
	DBG( _DBG_SANE_INIT,"blue offset  : %d\n",  cnf->adj.bofs                );
	DBG( _DBG_SANE_INIT,"red lampoff  : %d\n",  cnf->adj.rlampoff            );
	DBG( _DBG_SANE_INIT,"green lampoff: %d\n",  cnf->adj.glampoff            );
	DBG( _DBG_SANE_INIT,"blue lampoff : %d\n",  cnf->adj.blampoff            );
	DBG( _DBG_SANE_INIT,"red Gamma    : %.2f\n",cnf->adj.rgamma              );
	DBG( _DBG_SANE_INIT,"green Gamma  : %.2f\n",cnf->adj.ggamma              );
	DBG( _DBG_SANE_INIT,"blue Gamma   : %.2f\n",cnf->adj.bgamma              );
	DBG( _DBG_SANE_INIT,"gray Gamma   : %.2f\n",cnf->adj.graygamma           );
	DBG( _DBG_SANE_INIT,"---------------------\n" );
}

/** Calls the device specific stop and close functions.
 * @param  dev - pointer to the device specific structure
 * @return The function always returns SANE_STATUS_GOOD
 */
static SANE_Status
drvclose( Plustek_Device *dev )
{
	if( dev->fd >= 0 ) {

		DBG( _DBG_INFO, "drvclose()\n" );

		if( 0 != tsecs ) {
			DBG( _DBG_INFO, "TIME END 1: %lus\n", time(NULL)-tsecs);
		}

		/* don't check the return values, simply do it */
		usbDev_stopScan( dev );
		usbDev_close   ( dev );
		sanei_access_unlock( dev->sane.name );
	}
	dev->fd = -1;

	return SANE_STATUS_GOOD;
}

/** according to the mode and source we return the corresponding scanmode and
 *  bit-depth per pixel
 */
static int
getScanMode( Plustek_Scanner *scanner )
{
	int mode;
	int scanmode;
	
	/* are we in TPA-mode? */
	mode = scanner->val[OPT_MODE].w;
	if( scanner->val[OPT_EXT_MODE].w != 0 )
		mode += 2;

	scanner->params.depth = scanner->val[OPT_BIT_DEPTH].w;

	if( mode == 0 ) {
		scanmode = COLOR_BW;
		scanner->params.depth = 1;
	} else if( scanner->params.depth == 8 ) {

		if( mode == 1 )
			scanmode = COLOR_256GRAY;
		else
			scanmode = COLOR_TRUE24;
	} else {
		scanner->params.depth = 16;
		if( mode == 1 )
			scanmode = COLOR_GRAY16;
		else
			scanmode = COLOR_TRUE48;
	}
	return scanmode;
}

/** return the len of the largest string in the array
 */
static size_t
max_string_size (const SANE_String_Const strings[])
{
	size_t size, max_size = 0;
	SANE_Int i;

	for (i = 0; strings[i]; ++i) {
		size = strlen (strings[i]) + 1;
		if (size > max_size)
			max_size = size;
	}
	return max_size;
}

/** shutdown open pipes
 */
static SANE_Status 
close_pipe( Plustek_Scanner *scanner )
{
	if( scanner->r_pipe >= 0 ) {

		DBG( _DBG_PROC, "close_pipe (r_pipe)\n" );
		close( scanner->r_pipe );
		scanner->r_pipe = -1;
	}
	if( scanner->w_pipe >= 0 ) {

		DBG( _DBG_PROC, "close_pipe (w_pipe)\n" );
		close( scanner->w_pipe );
		scanner->w_pipe = -1;
	}
	return SANE_STATUS_EOF;
}

/** will be called when a child is going down
 */
static void
sig_chldhandler( int signo )
{
	DBG( _DBG_PROC, "(SIG) Child is down (signal=%d)\n", signo );
	if (sc) {
		sc->calibrating = SANE_FALSE;
		sc = NULL;
	}
}

/** signal handler to kill the child process
 */
static RETSIGTYPE
reader_process_sigterm_handler( int signo )
{
	DBG( _DBG_PROC, "(SIG) reader_process: terminated by signal %d\n", signo );
	_exit( SANE_STATUS_GOOD );
}

static RETSIGTYPE
usb_reader_process_sigterm_handler( int signo )
{
	DBG( _DBG_PROC, "(SIG) reader_process: terminated by signal %d\n", signo );
	cancelRead = SANE_TRUE;
}

static RETSIGTYPE
sigalarm_handler( int signo )
{
	_VAR_NOT_USED( signo );
	DBG( _DBG_PROC, "ALARM!!!\n" );
}

/**
 */
static void
thread_entry(void)
{
	struct SIGACTION act;
	sigset_t         ignore_set;

	sigfillset ( &ignore_set );
	sigdelset  ( &ignore_set, SIGTERM );
#if defined (__APPLE__) && defined (__MACH__)
	sigdelset  ( &ignore_set, SIGUSR2 );
#endif
	sigprocmask( SIG_SETMASK, &ignore_set, 0 );

	memset(&act, 0, sizeof (act));
	sigaction( SIGTERM, &act, 0 );

	cancelRead = SANE_FALSE;

	/* install the signal handler */
	sigemptyset(&(act.sa_mask));
	act.sa_flags = 0;

	act.sa_handler = reader_process_sigterm_handler;
	sigaction( SIGTERM, &act, 0 );

	act.sa_handler = usb_reader_process_sigterm_handler;
	sigaction( SIGUSR1, &act, 0 );
}

/** executed as a child process
 * read the data from the driver and send them to the parent process
 */
static int
reader_process( void *args )
{
	int              line, lerrn;
	unsigned char   *buf;
	unsigned long    status;
	unsigned long    data_length;
	Plustek_Scanner *scanner = (Plustek_Scanner *)args;
	Plustek_Device  *dev = scanner->hw;
#ifdef USE_IPC
	IPCDef           ipc;
#endif

	if( sanei_thread_is_forked()) {
		DBG( _DBG_PROC, "reader_process started (forked)\n" );
		close( scanner->r_pipe );
		scanner->r_pipe = -1;
	} else {
		DBG( _DBG_PROC, "reader_process started (as thread)\n" );
	}

	thread_entry();

	data_length = scanner->params.lines * scanner->params.bytes_per_line;

	DBG( _DBG_PROC, "reader_process:"
					"starting to READ data (%lu bytes)\n", data_length );
	DBG( _DBG_PROC, "buf = 0x%08lx\n", (unsigned long)scanner->buf );

	if( NULL == scanner->buf ) {
		DBG( _DBG_FATAL, "NULL Pointer !!!!\n" );
		return SANE_STATUS_IO_ERROR;
	}

	/* prepare for scanning: speed-test, warmup, calibration */
	buf    = scanner->buf;
	status = usbDev_Prepare( scanner->hw, buf );

#ifdef USE_IPC
	/* prepare IPC structure */
	memset(&ipc, 0, sizeof(ipc));
	ipc.transferRate = DEFAULT_RATE;

	if( dev->transferRate > 0 && dev->transferRate != DEFAULT_RATE )
		ipc.transferRate = dev->transferRate;

	/* write ipc back to parent in any case... */
	write( scanner->w_pipe, &ipc, sizeof(ipc));
#endif

	/* on success, we read all data from the driver... */
	if( 0 == status ) {

		if( !usb_InCalibrationMode(dev)) {

			DBG( _DBG_INFO, "reader_process: READING....\n" );

			for( line = 0; line < scanner->params.lines; line++ ) {

				status = usbDev_ReadLine( scanner->hw );
				if((int)status < 0 ) {
					break;
				}
				write( scanner->w_pipe, buf, scanner->params.bytes_per_line );
				buf += scanner->params.bytes_per_line;
			}
		}
	}
	/* on error, there's no need to clean up, as this is done by the parent */
	lerrn = errno;

	close( scanner->w_pipe );
	scanner->w_pipe = -1;

	if((int)status < 0 ) {
		DBG( _DBG_ERROR,"reader_process: read failed, status = %i, errno %i\n",
                                                          (int)status, lerrn );
		if( _E_ABORT == (int)status )
			return SANE_STATUS_CANCELLED;
		
		if( lerrn == EBUSY )
			return SANE_STATUS_DEVICE_BUSY;

		return SANE_STATUS_IO_ERROR;
	}

	DBG( _DBG_PROC, "reader_process: finished reading data\n" );
	return SANE_STATUS_GOOD;
}

/** stop the current scan process
 */
static SANE_Status
do_cancel( Plustek_Scanner *scanner, SANE_Bool closepipe )
{
	struct SIGACTION act;
	SANE_Pid         res;

	DBG( _DBG_PROC,"do_cancel\n" );
	scanner->scanning = SANE_FALSE;

	if( scanner->reader_pid != -1 ) {

		DBG( _DBG_PROC, ">>>>>>>> killing reader_process <<<<<<<<\n" );

		cancelRead = SANE_TRUE;
		scanner->calibrating = SANE_FALSE;

		sigemptyset(&(act.sa_mask));
		act.sa_flags = 0;

		act.sa_handler = sigalarm_handler;
		sigaction( SIGALRM, &act, 0 );

		/* kill our child process and wait until done */
		sanei_thread_sendsig( scanner->reader_pid, SIGUSR1 );

		/* give'em 10 seconds 'til done...*/
		alarm(10);
		res = sanei_thread_waitpid( scanner->reader_pid, 0 );
		alarm(0);

		if( res != scanner->reader_pid ) {
			DBG( _DBG_PROC,"sanei_thread_waitpid() failed !\n");

			/* do it the hard way...*/
#ifdef USE_PTHREAD
			sanei_thread_kill( scanner->reader_pid );
#else
			sanei_thread_sendsig( scanner->reader_pid, SIGKILL );
#endif
		}

		scanner->reader_pid = -1;
		DBG( _DBG_PROC,"reader_process killed\n");
#ifndef HAVE_SETITIMER
		usb_StartLampTimer( scanner->hw );
#endif
	}
	scanner->calibrating = SANE_FALSE;

	if( SANE_TRUE == closepipe ) {
		close_pipe( scanner );
	}

	drvclose( scanner->hw );

	if( tsecs != 0 ) {
		DBG( _DBG_INFO, "TIME END 2: %lus\n", time(NULL)-tsecs);
		tsecs = 0;
	}

	return SANE_STATUS_CANCELLED;
}

/** As we support only LM9831/2/3 chips we use the same
 * sizes for each device...
 * @param  s - pointer to the scanner specific structure
 * @return The function always returns SANE_STATUS_GOOD
 */
static SANE_Status
initGammaSettings( Plustek_Scanner *s )
{
	int    i, j, val;
	double gamma;

	s->gamma_length      = 4096;
	s->gamma_range.min   = 0;
	s->gamma_range.max   = 255;
	s->gamma_range.quant = 0;

	DBG( _DBG_INFO, "Presetting Gamma tables (len=%u)\n", s->gamma_length );

	/* preset the gamma maps
	 */
	for( i = 0; i < 4; i++ ) {

		switch( i ) {
			case 1:  gamma = s->hw->adj.rgamma;    break;
			case 2:  gamma = s->hw->adj.ggamma;    break;
			case 3:  gamma = s->hw->adj.bgamma;    break;
			default: gamma = s->hw->adj.graygamma; break;
		}
		DBG( _DBG_INFO, "* Channel[%u], gamma %.3f\n", i, gamma );

		for( j = 0; j < s->gamma_length; j++ ) {

			val = (s->gamma_range.max *
			       pow((double)j / (double)(s->gamma_length-1.0),
			       1.0 / gamma ));

			if( val > s->gamma_range.max )
				val = s->gamma_range.max;

			s->gamma_table[i][j] = val;
		}
	}
	DBG( _DBG_INFO, "----------------------------------\n" );
	return SANE_STATUS_GOOD;
}

/** Check the gamma vectors we got back and limit if necessary
 * @param  s - pointer to the scanner specific structure
 * @return nothing
 */
static void
checkGammaSettings( Plustek_Scanner *s )
{
	int i, j;

	DBG( _DBG_INFO, "Maps changed...\n" );
	for( i = 0; i < 4 ; i++ ) {
		for( j = 0; j < s->gamma_length; j++ ) {
			if( s->gamma_table[i][j] > s->gamma_range.max ) {
				s->gamma_table[i][j] = s->gamma_range.max;
			}
		}
	}
}

/** initialize the options for the backend according to the device we have
 */
static SANE_Status
init_options( Plustek_Scanner *s )
{
	int i;
	Plustek_Device *dev  = s->hw;
	AdjDef         *adj  = &dev->adj;
	DCapsDef       *caps = &dev->usbDev.Caps;

	memset(s->opt, 0, sizeof(s->opt));

	for( i = 0; i < NUM_OPTIONS; ++i ) {
		s->opt[i].size = sizeof (SANE_Word);
		s->opt[i].cap  = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
		s->opt[i].unit = SANE_UNIT_NONE;
	}

	s->opt[OPT_NUM_OPTS].name  = SANE_NAME_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].desc  = SANE_DESC_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].type  = SANE_TYPE_INT;
	s->opt[OPT_NUM_OPTS].cap   = SANE_CAP_SOFT_DETECT;
	s->opt[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
	s->val[OPT_NUM_OPTS].w     = NUM_OPTIONS;

	/* "Scan Mode" group: */
	s->opt[OPT_MODE_GROUP].title = SANE_I18N("Scan Mode");
	s->opt[OPT_MODE_GROUP].desc  = "";
	s->opt[OPT_MODE_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_MODE_GROUP].cap   = 0;

	/* scan mode */
	s->opt[OPT_MODE].name  = SANE_NAME_SCAN_MODE;
	s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
	s->opt[OPT_MODE].desc  = SANE_DESC_SCAN_MODE;
	s->opt[OPT_MODE].type  = SANE_TYPE_STRING;
	s->opt[OPT_MODE].size  = max_string_size(mode_list);
	s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_MODE].constraint.string_list = mode_list;
	s->val[OPT_MODE].w = 2; /* Color */

	/* bit depth */
	s->opt[OPT_BIT_DEPTH].name  = SANE_NAME_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].desc  = SANE_DESC_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].type  = SANE_TYPE_INT;
	s->opt[OPT_BIT_DEPTH].unit  = SANE_UNIT_BIT;
	s->opt[OPT_BIT_DEPTH].size  = sizeof (SANE_Word);
	s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	if( _LM9833 == dev->usbDev.HwSetting.chip )
		s->opt[OPT_BIT_DEPTH].constraint.word_list = bpp_lm9833_list;
	else
		s->opt[OPT_BIT_DEPTH].constraint.word_list = bpp_lm9832_list;
	s->val[OPT_BIT_DEPTH].w = 8;
	
	if (caps->workaroundFlag & _WAF_ONLY_8BIT)
		_DISABLE(OPT_BIT_DEPTH);

	/* scan source */
	s->opt[OPT_EXT_MODE].name  = SANE_NAME_SCAN_SOURCE;
	s->opt[OPT_EXT_MODE].title = SANE_TITLE_SCAN_SOURCE;
	s->opt[OPT_EXT_MODE].desc  = SANE_DESC_SCAN_SOURCE;
	s->opt[OPT_EXT_MODE].type  = SANE_TYPE_STRING;
	s->opt[OPT_EXT_MODE].size  = max_string_size(source_list);
	s->opt[OPT_EXT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_EXT_MODE].constraint.string_list = source_list;
	s->val[OPT_EXT_MODE].w     = 0; /* Normal */

	/* brightness */
	s->opt[OPT_BRIGHTNESS].name  = SANE_NAME_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].desc  = SANE_DESC_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].type  = SANE_TYPE_FIXED;
	s->opt[OPT_BRIGHTNESS].unit  = SANE_UNIT_PERCENT;
	s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
	s->val[OPT_BRIGHTNESS].w     = 0;

	/* contrast */
	s->opt[OPT_CONTRAST].name  = SANE_NAME_CONTRAST;
	s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
	s->opt[OPT_CONTRAST].desc  = SANE_DESC_CONTRAST;
	s->opt[OPT_CONTRAST].type  = SANE_TYPE_FIXED;
	s->opt[OPT_CONTRAST].unit  = SANE_UNIT_PERCENT;
	s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
	s->val[OPT_CONTRAST].w     = 0;

	/* resolution */
	s->opt[OPT_RESOLUTION].name  = SANE_NAME_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].desc  = SANE_DESC_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].type  = SANE_TYPE_INT;
	s->opt[OPT_RESOLUTION].unit  = SANE_UNIT_DPI;
	s->opt[OPT_RESOLUTION].constraint_type  = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
	s->val[OPT_RESOLUTION].w = s->hw->dpi_range.min;

	/* custom-gamma table */
	s->opt[OPT_CUSTOM_GAMMA].name  = SANE_NAME_CUSTOM_GAMMA;
	s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
	s->opt[OPT_CUSTOM_GAMMA].desc  = SANE_DESC_CUSTOM_GAMMA;
	s->opt[OPT_CUSTOM_GAMMA].type  = SANE_TYPE_BOOL;
	s->val[OPT_CUSTOM_GAMMA].w     = SANE_FALSE;

	/* preview */
	s->opt[OPT_PREVIEW].name  = SANE_NAME_PREVIEW;
	s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
	s->opt[OPT_PREVIEW].desc  = SANE_DESC_PREVIEW;
	s->opt[OPT_PREVIEW].cap   = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
	s->val[OPT_PREVIEW].w     = 0;

	/* "Geometry" group: */
	s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N("Geometry");
	s->opt[OPT_GEOMETRY_GROUP].desc  = "";
	s->opt[OPT_GEOMETRY_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_GEOMETRY_GROUP].cap   = SANE_CAP_ADVANCED;

	/* top-left x */
	s->opt[OPT_TL_X].name  = SANE_NAME_SCAN_TL_X;
	s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
	s->opt[OPT_TL_X].desc  = SANE_DESC_SCAN_TL_X;
	s->opt[OPT_TL_X].type  = SANE_TYPE_FIXED;
	s->opt[OPT_TL_X].unit  = SANE_UNIT_MM;
	s->opt[OPT_TL_X].constraint_type  = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
	s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_TLX);

	/* top-left y */
	s->opt[OPT_TL_Y].name  = SANE_NAME_SCAN_TL_Y;
	s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
	s->opt[OPT_TL_Y].desc  = SANE_DESC_SCAN_TL_Y;
	s->opt[OPT_TL_Y].type  = SANE_TYPE_FIXED;
	s->opt[OPT_TL_Y].unit  = SANE_UNIT_MM;
	s->opt[OPT_TL_Y].constraint_type  = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
	s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_TLY);

	/* bottom-right x */
	s->opt[OPT_BR_X].name  = SANE_NAME_SCAN_BR_X;
	s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
	s->opt[OPT_BR_X].desc  = SANE_DESC_SCAN_BR_X;
	s->opt[OPT_BR_X].type  = SANE_TYPE_FIXED;
	s->opt[OPT_BR_X].unit  = SANE_UNIT_MM;
	s->opt[OPT_BR_X].constraint_type  = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
	s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_BRX);

	/* bottom-right y */
	s->opt[OPT_BR_Y].name  = SANE_NAME_SCAN_BR_Y;
	s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
	s->opt[OPT_BR_Y].desc  = SANE_DESC_SCAN_BR_Y;
	s->opt[OPT_BR_Y].type  = SANE_TYPE_FIXED;
	s->opt[OPT_BR_Y].unit  = SANE_UNIT_MM;
	s->opt[OPT_BR_Y].constraint_type  = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
	s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_BRY);

	/* "Enhancement" group: */
	s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N("Enhancement");	
	s->opt[OPT_ENHANCEMENT_GROUP].desc  = "";
	s->opt[OPT_ENHANCEMENT_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_ENHANCEMENT_GROUP].cap   = 0;
	s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

	initGammaSettings( s );

	/* grayscale gamma vector */
	s->opt[OPT_GAMMA_VECTOR].name  = SANE_NAME_GAMMA_VECTOR;
	s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
	s->opt[OPT_GAMMA_VECTOR].desc  = SANE_DESC_GAMMA_VECTOR;
	s->opt[OPT_GAMMA_VECTOR].type  = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
	s->val[OPT_GAMMA_VECTOR].wa   = &(s->gamma_table[0][0]);
	s->opt[OPT_GAMMA_VECTOR].constraint.range = &(s->gamma_range);
	s->opt[OPT_GAMMA_VECTOR].size = s->gamma_length * sizeof(SANE_Word);

	/* red gamma vector */
	s->opt[OPT_GAMMA_VECTOR_R].name  = SANE_NAME_GAMMA_VECTOR_R;
	s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
	s->opt[OPT_GAMMA_VECTOR_R].desc  = SANE_DESC_GAMMA_VECTOR_R;
	s->opt[OPT_GAMMA_VECTOR_R].type  = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
	s->val[OPT_GAMMA_VECTOR_R].wa   = &(s->gamma_table[1][0]);
	s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &(s->gamma_range);
	s->opt[OPT_GAMMA_VECTOR_R].size = s->gamma_length * sizeof(SANE_Word);

	/* green gamma vector */
	s->opt[OPT_GAMMA_VECTOR_G].name  = SANE_NAME_GAMMA_VECTOR_G;
	s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
	s->opt[OPT_GAMMA_VECTOR_G].desc  = SANE_DESC_GAMMA_VECTOR_G;
	s->opt[OPT_GAMMA_VECTOR_G].type  = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
	s->val[OPT_GAMMA_VECTOR_G].wa   = &(s->gamma_table[2][0]);
	s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &(s->gamma_range);
	s->opt[OPT_GAMMA_VECTOR_G].size = s->gamma_length * sizeof(SANE_Word);

	/* blue gamma vector */
	s->opt[OPT_GAMMA_VECTOR_B].name  = SANE_NAME_GAMMA_VECTOR_B;
	s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
	s->opt[OPT_GAMMA_VECTOR_B].desc  = SANE_DESC_GAMMA_VECTOR_B;
	s->opt[OPT_GAMMA_VECTOR_B].type  = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
	s->val[OPT_GAMMA_VECTOR_B].wa   = &(s->gamma_table[3][0]);
	s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &(s->gamma_range);
	s->opt[OPT_GAMMA_VECTOR_B].size = s->gamma_length * sizeof(SANE_Word);

	/* GAMMA stuff is disabled per default */	
	_DISABLE(OPT_GAMMA_VECTOR);
	_DISABLE(OPT_GAMMA_VECTOR_R);
	_DISABLE(OPT_GAMMA_VECTOR_G);
	_DISABLE(OPT_GAMMA_VECTOR_B);

	/* disable extended mode list for devices without TPA */
	if( 0 == (s->hw->caps.dwFlag & SFLAG_TPA))
		_DISABLE(OPT_EXT_MODE);

	/* "Device settings" group: */
	s->opt[OPT_DEVICE_GROUP].title = SANE_I18N("Device-Settings");
	s->opt[OPT_DEVICE_GROUP].desc  = "";
	s->opt[OPT_DEVICE_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_DEVICE_GROUP].cap   = 0;
	s->opt[OPT_DEVICE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

	s->opt[OPT_LAMPSWITCH].name  = "lamp-switch";
	s->opt[OPT_LAMPSWITCH].title = SANE_I18N("Lampswitch");;
	s->opt[OPT_LAMPSWITCH].desc  = SANE_I18N("Manually switching the lamp(s).");
	s->opt[OPT_LAMPSWITCH].type  = SANE_TYPE_BOOL;
	s->val[OPT_LAMPSWITCH].w     = SANE_FALSE;

	s->opt[OPT_LOFF4DARK].name  = "lamp-off-during-dcal";
	s->opt[OPT_LOFF4DARK].title = SANE_I18N("Lamp off during dark calibration");;
	s->opt[OPT_LOFF4DARK].desc  = SANE_I18N("Always switches lamp off when doing dark calibration.");
	s->opt[OPT_LOFF4DARK].type  = SANE_TYPE_BOOL;
	s->val[OPT_LOFF4DARK].w     = adj->skipDarkStrip;

	if (dev->usbDev.Caps.Normal.DarkShadOrgY < 0)
		_DISABLE(OPT_LOFF4DARK);

	s->opt[OPT_CACHECAL].name  = "calibration-cache";
	s->opt[OPT_CACHECAL].title = SANE_I18N("Calibration data cache");;
	s->opt[OPT_CACHECAL].desc  = SANE_I18N("Enables or disables calibration data cache.");
	s->opt[OPT_CACHECAL].type  = SANE_TYPE_BOOL;
	s->val[OPT_CACHECAL].w     = adj->cacheCalData;

	s->opt[OPT_CALIBRATE].name  = "calibrate";
	s->opt[OPT_CALIBRATE].title = SANE_I18N("Calibrate");;
	s->opt[OPT_CALIBRATE].desc  = SANE_I18N("Performs calibration");
	s->opt[OPT_CALIBRATE].type  = SANE_TYPE_BUTTON;
	s->opt[OPT_CALIBRATE].size = sizeof (SANE_Word);
	s->opt[OPT_CALIBRATE].constraint_type = SANE_CONSTRAINT_NONE;
	s->opt[OPT_CALIBRATE].constraint.range = 0;
	s->opt[OPT_CALIBRATE].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT |
	                            SANE_CAP_AUTOMATIC;
	_ENABLE(OPT_CALIBRATE);
	if( !adj->cacheCalData )
		_DISABLE(OPT_CALIBRATE);
	s->val[OPT_CALIBRATE].w = 0;

	/* it's currently not available for CCD devices */
	if( !usb_IsCISDevice(dev) && !dev->adj.altCalibrate)
		_DISABLE(OPT_CALIBRATE);

	s->opt[OPT_SPEEDUP].name  = "speedup-switch";
	s->opt[OPT_SPEEDUP].title = SANE_I18N("Speedup sensor");;
	s->opt[OPT_SPEEDUP].desc  = SANE_I18N("Enables or disables speeding up sensor movement.");
	s->opt[OPT_SPEEDUP].type  = SANE_TYPE_BOOL;
	s->val[OPT_SPEEDUP].w     = !(adj->disableSpeedup);

	if( s->hw->usbDev.HwSetting.dHighSpeed == 0.0 )
		_DISABLE(OPT_SPEEDUP);

	s->opt[OPT_LAMPOFF_ONEND].name  = SANE_NAME_LAMP_OFF_AT_EXIT;
	s->opt[OPT_LAMPOFF_ONEND].title = SANE_TITLE_LAMP_OFF_AT_EXIT;
	s->opt[OPT_LAMPOFF_ONEND].desc  = SANE_DESC_LAMP_OFF_AT_EXIT;
	s->opt[OPT_LAMPOFF_ONEND].type  = SANE_TYPE_BOOL;
	s->val[OPT_LAMPOFF_ONEND].w     = adj->lampOffOnEnd;

	s->opt[OPT_WARMUPTIME].name  = "warmup-time";
	s->opt[OPT_WARMUPTIME].title = SANE_I18N("Warmup-time");;
	s->opt[OPT_WARMUPTIME].desc  = SANE_I18N("Warmup-time in seconds.");
	s->opt[OPT_WARMUPTIME].type  = SANE_TYPE_INT;
	s->opt[OPT_WARMUPTIME].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_WARMUPTIME].constraint.range = &warmup_range;
	s->val[OPT_WARMUPTIME].w     = adj->warmup;
	/* only available for CCD devices*/
	if( usb_IsCISDevice( dev )) {
		_DISABLE(OPT_WARMUPTIME);
		s->val[OPT_WARMUPTIME].w = 0;
	}

	s->opt[OPT_LAMPOFF_TIMER].name  = "lampoff-time";
	s->opt[OPT_LAMPOFF_TIMER].title = SANE_I18N("Lampoff-time");;
	s->opt[OPT_LAMPOFF_TIMER].desc  = SANE_I18N("Lampoff-time in seconds.");
	s->opt[OPT_LAMPOFF_TIMER].type  = SANE_TYPE_INT;
	s->opt[OPT_LAMPOFF_TIMER].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_LAMPOFF_TIMER].constraint.range = &offtimer_range;
	s->val[OPT_LAMPOFF_TIMER].w     = adj->lampOff;

	/* "Analog Frontend" group*/
	s->opt[OPT_AFE_GROUP].title = SANE_I18N("Analog frontend");
	s->opt[OPT_AFE_GROUP].desc  = "";
	s->opt[OPT_AFE_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_AFE_GROUP].cap   = SANE_CAP_ADVANCED;
	
	s->opt[OPT_OVR_REDGAIN].name  = "red-gain";
	s->opt[OPT_OVR_REDGAIN].title = SANE_I18N("Red gain");
	s->opt[OPT_OVR_REDGAIN].desc  = SANE_I18N("Red gain value of the AFE");
	s->opt[OPT_OVR_REDGAIN].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_REDGAIN].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_REDGAIN].constraint.range = &gain_range;
	s->val[OPT_OVR_REDGAIN].w     = adj->rgain;

	s->opt[OPT_OVR_REDOFS].name  = "red-offset";
	s->opt[OPT_OVR_REDOFS].title = SANE_I18N("Red offset");
	s->opt[OPT_OVR_REDOFS].desc  = SANE_I18N("Red offset value of the AFE");
	s->opt[OPT_OVR_REDOFS].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_REDOFS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_REDOFS].constraint.range = &gain_range;
	s->val[OPT_OVR_REDOFS].w     = adj->rofs;

	s->opt[OPT_OVR_GREENGAIN].name  = "green-gain";
	s->opt[OPT_OVR_GREENGAIN].title = SANE_I18N("Green gain");
	s->opt[OPT_OVR_GREENGAIN].desc  = SANE_I18N("Green gain value of the AFE");
	s->opt[OPT_OVR_GREENGAIN].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_GREENGAIN].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_GREENGAIN].constraint.range = &gain_range;
	s->val[OPT_OVR_GREENGAIN].w     = adj->ggain;

	s->opt[OPT_OVR_GREENOFS].name  = "green-offset";
	s->opt[OPT_OVR_GREENOFS].title = SANE_I18N("Green offset");
	s->opt[OPT_OVR_GREENOFS].desc  = SANE_I18N("Green offset value of the AFE");
	s->opt[OPT_OVR_GREENOFS].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_GREENOFS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_GREENOFS].constraint.range = &gain_range;
	s->val[OPT_OVR_GREENOFS].w     = adj->gofs;

	s->opt[OPT_OVR_BLUEGAIN].name  = "blue-gain";
	s->opt[OPT_OVR_BLUEGAIN].title = SANE_I18N("Blue gain");
	s->opt[OPT_OVR_BLUEGAIN].desc  = SANE_I18N("Blue gain value of the AFE");
	s->opt[OPT_OVR_BLUEGAIN].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_BLUEGAIN].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_BLUEGAIN].constraint.range = &gain_range;
	s->val[OPT_OVR_BLUEGAIN].w     = adj->bgain;

	s->opt[OPT_OVR_BLUEOFS].name  = "blue-offset";
	s->opt[OPT_OVR_BLUEOFS].title = SANE_I18N("Blue offset");
	s->opt[OPT_OVR_BLUEOFS].desc  = SANE_I18N("Blue offset value of the AFE");
	s->opt[OPT_OVR_BLUEOFS].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_BLUEOFS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_BLUEOFS].constraint.range = &gain_range;
	s->val[OPT_OVR_BLUEOFS].w     = adj->bofs;

	s->opt[OPT_OVR_RED_LOFF].name  = "redlamp-off";
	s->opt[OPT_OVR_RED_LOFF].title = SANE_I18N("Red lamp off");
	s->opt[OPT_OVR_RED_LOFF].desc  = SANE_I18N("Defines red lamp off parameter");
	s->opt[OPT_OVR_RED_LOFF].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_RED_LOFF].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_RED_LOFF].constraint.range = &loff_range;
	s->val[OPT_OVR_RED_LOFF].w     = adj->rlampoff;

	s->opt[OPT_OVR_GREEN_LOFF].name  = "greenlamp-off";
	s->opt[OPT_OVR_GREEN_LOFF].title = SANE_I18N("Green lamp off");
	s->opt[OPT_OVR_GREEN_LOFF].desc  = SANE_I18N("Defines green lamp off parameter");
	s->opt[OPT_OVR_GREEN_LOFF].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_GREEN_LOFF].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_GREEN_LOFF].constraint.range = &loff_range;
	s->val[OPT_OVR_GREEN_LOFF].w     = adj->glampoff;

	s->opt[OPT_OVR_BLUE_LOFF].name  = "bluelamp-off";
	s->opt[OPT_OVR_BLUE_LOFF].title = SANE_I18N("Blue lamp off");
	s->opt[OPT_OVR_BLUE_LOFF].desc  = SANE_I18N("Defines blue lamp off parameter");
	s->opt[OPT_OVR_BLUE_LOFF].type  = SANE_TYPE_INT;
	s->opt[OPT_OVR_BLUE_LOFF].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_OVR_BLUE_LOFF].constraint.range = &loff_range;
	s->val[OPT_OVR_BLUE_LOFF].w     = adj->blampoff;

	/* only available for CIS devices*/
	if( !usb_IsCISDevice( dev )) {
		_DISABLE(OPT_OVR_RED_LOFF);
		_DISABLE(OPT_OVR_GREEN_LOFF);
		_DISABLE(OPT_OVR_BLUE_LOFF);
	}

	/* "Button" group*/
	s->opt[OPT_BUTTON_GROUP].title = SANE_I18N("Buttons");
	s->opt[OPT_BUTTON_GROUP].desc  = "";
	s->opt[OPT_BUTTON_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_BUTTON_GROUP].cap   = SANE_CAP_ADVANCED;

	/* scanner buttons */
	for( i = OPT_BUTTON_0; i <= OPT_BUTTON_LAST; i++ ) {

		char name [12];
		char title [128];

		sprintf (name, "button %d", i - OPT_BUTTON_0);
		sprintf (title, "Scanner button %d", i - OPT_BUTTON_0);

		s->opt[i].name  = strdup(name);
		s->opt[i].title = strdup(title);
		s->opt[i].desc  = SANE_I18N("This option reflects the status "
		                            "of the scanner buttons.");
		s->opt[i].type = SANE_TYPE_BOOL;
		s->opt[i].cap  = SANE_CAP_SOFT_DETECT |
		                 SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
		if (i - OPT_BUTTON_0 >= dev->usbDev.Caps.bButtons )
			_DISABLE(i);

		s->opt[i].unit = SANE_UNIT_NONE;
		s->opt[i].size = sizeof (SANE_Word);
		s->opt[i].constraint_type = SANE_CONSTRAINT_NONE;
		s->opt[i].constraint.range = 0;
		s->val[i].w = SANE_FALSE;
	}

	usb_UpdateButtonStatus( s );
	return SANE_STATUS_GOOD;
}

/** Function to retrieve the vendor and product id from a given string
 * @param src  - string, that should be investigated
 * @param dest - pointer to a string to receive the USB ID
 */
static void
decodeUsbIDs( char *src, char **dest )
{		
	const char *name;	
	char       *tmp = *dest;
	int         len = strlen(_SECTION);

	if( isspace(src[len])) {
		strncpy( tmp, &src[len+1], (strlen(src)-(len+1)));
        tmp[(strlen(src)-(len+1))] = '\0';
	}

	name = tmp;
	name = sanei_config_skip_whitespace( name );

	if( '\0' == name[0] ) {
		DBG( _DBG_SANE_INIT, "next device uses autodetection\n" );
	} else {
			
		u_short pi = 0, vi = 0;

		if( *name ) {
	
			name = sanei_config_get_string( name, &tmp );
			if( tmp ) {
		    	vi = strtol( tmp, 0, 0 );
			    free( tmp );
			}
	    }

		name = sanei_config_skip_whitespace( name );
		if( *name ) {

			name = sanei_config_get_string( name, &tmp );
			if( tmp ) {
				pi = strtol( tmp, 0, 0 );
				free( tmp );
			}
		}

		/* create what we need to go through our device list...*/
		sprintf( *dest, "0x%04X-0x%04X", vi, pi );
		DBG( _DBG_SANE_INIT, "next device is a USB device (%s)\n", *dest );
	}
}

#define _INT   0
#define _FLOAT 1

/** function to decode an value and give it back to the caller.
 * @param src    -  pointer to the source string to check
 * @param opt    -  string that keeps the option name to check src for
 * @param what   - _FLOAT or _INT
 * @param result -  pointer to the var that should receive our result
 * @param def    - default value that result should be in case of any error
 * @return The function returns SANE_TRUE if the option has been found,
 *         if not, it returns SANE_FALSE
 */
static SANE_Bool
decodeVal( char *src, char *opt, int what, void *result, void *def )
{
	char       *tmp, *tmp2;
	const char *name;

	/* skip the option string */
	name = (const char*)&src[strlen("option")];

	/* get the name of the option */
	name = sanei_config_get_string( name, &tmp );

	if( tmp ) {

		/* on success, compare wiht the given one */
		if( 0 == strcmp( tmp, opt )) {

			DBG( _DBG_SANE_INIT, "Decoding option >%s<\n", opt );

			if( _INT == what ) {

				/* assign the default value for this option... */
				*((int*)result) = *((int*)def);

				if( *name ) {

					/* get the configuration value and decode it */
					name = sanei_config_get_string( name, &tmp2 );

					if( tmp2 ) {
						*((int*)result) = strtol( tmp2, 0, 0 );
						free( tmp2 );
					}
				}
				free( tmp );	
				return SANE_TRUE;

			} else if( _FLOAT == what ) {

				/* assign the default value for this option... */
				*((double*)result) = *((double*)def);

				if( *name ) {

					/* get the configuration value and decode it */
					name = sanei_config_get_string( name, &tmp2 );

					if( tmp2 ) {
						*((double*)result) = strtod( tmp2, 0 );
						free( tmp2 );
					}
				}
				free( tmp );	
				return SANE_TRUE;
			}
		}
		free( tmp );
	}
	return SANE_FALSE;
}

/** function to retrive the device name of a given string
 * @param src  -  string that keeps the option name to check src for
 * @param dest -  pointer to the string, that should receive the detected
 *                devicename
 * @return The function returns SANE_TRUE if the devicename has been found,
 *         if not, it returns SANE_FALSE
 */
static SANE_Bool
decodeDevName( char *src, char *dest )
{
	char       *tmp;	
	const char *name;

	if( 0 == strncmp( "device", src, 6 )) {

		name = (const char*)&src[strlen("device")];
		name = sanei_config_skip_whitespace( name );

		DBG( _DBG_SANE_INIT, "Decoding device name >%s<\n", name );

		if( *name ) {
			name = sanei_config_get_string( name, &tmp );
			if( tmp ) {

				strcpy( dest, tmp );
				free( tmp );
				return SANE_TRUE;
			}
		}
	}
	return SANE_FALSE;
}

/** attach a device to the backend
 */
static SANE_Status
attach( const char *dev_name, CnfDef *cnf, Plustek_Device **devp )
{
	int             cntr;
	int             result;
	int             handle;
	Plustek_Device *dev;

	DBG( _DBG_SANE_INIT, "attach (%s, %p, %p)\n",
	                                      dev_name, (void *)cnf, (void *)devp);
	/* already attached ?*/
	for( dev = first_dev; dev; dev = dev->next ) {

		if( 0 == strcmp( dev->sane.name, dev_name )) {
			if( devp )
				*devp = dev;

			return SANE_STATUS_GOOD;
		}
	}

	/* allocate some memory for the device */
	dev = malloc( sizeof (*dev));
	if( NULL == dev )
		return SANE_STATUS_NO_MEM;

	/* assign all the stuff we need fo this device... */

	memset(dev, 0, sizeof (*dev));

	dev->fd           = -1;
	dev->name         = strdup(dev_name);    /* hold it double to avoid   */
	dev->sane.name    = dev->name;           /* compiler warnings         */
	dev->sane.vendor  = "Plustek";
	dev->initialized  = -1;                  /* will be used as index too */
	dev->calFile      = NULL;
	dev->transferRate = DEFAULT_RATE;

	memcpy( &dev->adj, &cnf->adj, sizeof(AdjDef));

	show_cnf( cnf );

	strncpy( dev->usbId, cnf->usbId, _MAX_ID_LEN );

	if( cnf->adj.lampOff >= 0 )
		dev->usbDev.dwLampOnPeriod = cnf->adj.lampOff;

	if( cnf->adj.lampOffOnEnd >= 0 )
		dev->usbDev.bLampOffOnEnd = cnf->adj.lampOffOnEnd;

	/* go ahead and open the scanner device */
	handle = usbDev_open( dev, usbDevs, SANE_FALSE );
	if( handle < 0 ) {
		DBG( _DBG_ERROR,"open failed: %d\n", handle );
		return SANE_STATUS_IO_ERROR;
	}

	/* okay, so assign the handle and the scanner type */
	dev->fd = handle;
	if( usb_IsSheetFedDevice( dev ))
		dev->sane.type = SANE_I18N("sheetfed scanner");
	else
		dev->sane.type = SANE_I18N("flatbed scanner");

	result = usbDev_getCaps( dev );
	if( result < 0 ) {
		DBG( _DBG_ERROR, "usbDev_getCaps() failed(%d)\n", result);
		usbDev_close(dev);
		return SANE_STATUS_IO_ERROR;
	}

	/* save the info we got from the driver */
	DBG( _DBG_INFO, "Scanner information:\n" );
	if( NULL != dev->usbDev.ModelStr )
		dev->sane.model = dev->usbDev.ModelStr;
	else
		dev->sane.model = "USB-Device";

	DBG( _DBG_INFO, "Vendor : %s\n",      dev->sane.vendor  );
	DBG( _DBG_INFO, "Model  : %s\n",      dev->sane.model   );
	DBG( _DBG_INFO, "Flags  : 0x%08lx\n", dev->caps.dwFlag  );

	dev->max_x = dev->caps.wMaxExtentX*MM_PER_INCH/_MEASURE_BASE;
	dev->max_y = dev->caps.wMaxExtentY*MM_PER_INCH/_MEASURE_BASE;

	/* calculate the size of the resolution list +
	 * one more to avoid a buffer overflow, then allocate it...
	 */
	dev->res_list = (SANE_Int *)
	                 calloc((((dev->usbDev.Caps.OpticDpi.x*16)-_DEF_DPI)/25+1),
	                 sizeof (SANE_Int));  

	if (NULL == dev->res_list) {
		DBG( _DBG_ERROR, "calloc failed: %s\n", strerror(errno));
		usbDev_close(dev);
		return SANE_STATUS_INVAL;
	}

	/* build up the resolution table */
	dev->res_list_size = 0;
	for(cntr = _DEF_DPI; cntr <= (dev->usbDev.Caps.OpticDpi.x*16); cntr += 25){
		dev->res_list_size++;
		dev->res_list[dev->res_list_size - 1] = (SANE_Int)cntr;
	}

	/* set the limits */
	dev->dpi_range.min = _DEF_DPI;
	dev->dpi_range.max = dev->usbDev.Caps.OpticDpi.x * 2;
	dev->x_range.max   = SANE_FIX(dev->max_x);
	dev->y_range.max   = SANE_FIX(dev->max_y);

	dev->fd = handle;
	drvclose( dev );

	DBG( _DBG_SANE_INIT, "attach: model = >%s<\n", dev->sane.model );

	++num_devices;
	dev->next = first_dev;
	first_dev = dev;

	if (devp)
		*devp = dev;

	return SANE_STATUS_GOOD;
}

/** function to preset a configuration structure
 * @param cnf - pointer to the structure that should be initialized
 */
static void
init_config_struct( CnfDef *cnf )
{
	memset(cnf, 0, sizeof(CnfDef));

	cnf->adj.warmup       = -1;
	cnf->adj.lampOff      = -1;
	cnf->adj.lampOffOnEnd = -1;

	cnf->adj.posShadingY  = -1;
	cnf->adj.tpaShadingY  = -1;
	cnf->adj.negShadingY  = -1;
	cnf->adj.rgain        = -1;
	cnf->adj.ggain        = -1;
	cnf->adj.bgain        = -1;
	cnf->adj.rofs         = -1;
	cnf->adj.gofs         = -1;
	cnf->adj.bofs         = -1;
	cnf->adj.rlampoff     = -1;
	cnf->adj.glampoff     = -1;
	cnf->adj.blampoff     = -1;

	cnf->adj.incDarkTgt = 1;

	cnf->adj.graygamma = 1.0;
	cnf->adj.rgamma    = 1.0;
	cnf->adj.ggamma    = 1.0;
	cnf->adj.bgamma    = 1.0;
}

/** intialize the backend
 */
SANE_Status
sane_init( SANE_Int *version_code, SANE_Auth_Callback authorize )
{
	char     str[PATH_MAX] = _DEFAULT_DEVICE;
	CnfDef   config;
	size_t   len;
	FILE    *fp;

	DBG_INIT();

	sanei_usb_init();
	sanei_lm983x_init();
	sanei_thread_init();
	sanei_access_init(STRINGIFY(BACKEND_NAME));

#if defined PACKAGE && defined VERSION
	DBG( _DBG_INFO, "Plustek backend V"BACKEND_VERSION", part of "
	                                      PACKAGE " " VERSION "\n");
#else
	DBG( _DBG_INFO, "Plustek backend V"BACKEND_VERSION"\n" );
#endif

	/* do some presettings... */
	auth         = authorize;
	first_dev    = NULL;
	first_handle = NULL;
	num_devices  = 0;
	usbDevs      = NULL;

	/* initialize the configuration structure */
	init_config_struct( &config );

	/* try and get a list of all connected AND supported devices */
	usbGetList( &usbDevs );

	if( version_code != NULL )
		*version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR, 0);

	fp = sanei_config_open( PLUSTEK_CONFIG_FILE );

	/* default to _DEFAULT_DEVICE instead of insisting on config file */
	if( NULL == fp ) {
		return attach( _DEFAULT_DEVICE, &config, 0 );
	}

	while( sanei_config_read( str, sizeof(str), fp)) {

		DBG( _DBG_SANE_INIT, ">%s<\n", str );
		if( str[0] == '#')		/* ignore line comments */
			continue;

		len = strlen(str);
		if( 0 == len )
			continue;     /* ignore empty lines */

		/* check for options */
		if( 0 == strncmp(str, "option", 6)) {

			int    ival;
			double dval;

			ival = -1;
			decodeVal( str, "warmup",    _INT, &config.adj.warmup,      &ival);
			decodeVal( str, "lampOff",   _INT, &config.adj.lampOff,     &ival);
			decodeVal( str, "lOffOnEnd", _INT, &config.adj.lampOffOnEnd,&ival);
			decodeVal( str, "posShadingY",_INT, &config.adj.posShadingY,&ival);
			decodeVal( str, "tpaShadingY",_INT, &config.adj.tpaShadingY,&ival);
			decodeVal( str, "negShadingY",_INT, &config.adj.negShadingY,&ival);
			decodeVal( str, "red_gain",   _INT, &config.adj.rgain,      &ival);
			decodeVal( str, "green_gain", _INT, &config.adj.ggain,      &ival);
			decodeVal( str, "blue_gain",  _INT, &config.adj.bgain,      &ival);
			decodeVal( str, "red_offset",    _INT, &config.adj.rofs,    &ival);
			decodeVal( str, "green_offset" , _INT, &config.adj.gofs,    &ival);
			decodeVal( str, "blue_offset",   _INT, &config.adj.bofs,    &ival);
			decodeVal( str, "red_lampoff",   _INT, &config.adj.rlampoff,&ival);
			decodeVal( str, "green_lampoff", _INT, &config.adj.glampoff,&ival);
			decodeVal( str, "blue_lampoff",  _INT, &config.adj.blampoff,&ival);

			ival = 0;
			decodeVal( str, "enableTPA", _INT, &config.adj.enableTpa, &ival);
			decodeVal( str, "cacheCalData",
									     _INT, &config.adj.cacheCalData,&ival);
			decodeVal( str, "altCalibration",
									     _INT, &config.adj.altCalibrate,&ival);
			decodeVal( str, "skipCalibration",
									  _INT, &config.adj.skipCalibration,&ival);
			decodeVal( str, "skipFine",
									  _INT, &config.adj.skipFine,&ival);
			decodeVal( str, "skipFineWhite",
									  _INT, &config.adj.skipFineWhite,&ival);
			decodeVal( str, "skipDarkStrip",
									  _INT, &config.adj.skipDarkStrip,&ival);
			decodeVal( str, "incDarkTarget",
									  _INT, &config.adj.incDarkTgt,&ival);
			decodeVal( str, "invertNegatives",
									  _INT, &config.adj.invertNegatives,&ival);
			decodeVal( str, "disableSpeedup",
									  _INT, &config.adj.disableSpeedup,&ival);

			decodeVal( str, "posOffX", _INT, &config.adj.pos.x, &ival );
			decodeVal( str, "posOffY", _INT, &config.adj.pos.y, &ival );

			decodeVal( str, "negOffX", _INT, &config.adj.neg.x, &ival );
			decodeVal( str, "negOffY", _INT, &config.adj.neg.y, &ival );

			decodeVal( str, "tpaOffX", _INT, &config.adj.tpa.x, &ival );
			decodeVal( str, "tpaOffY", _INT, &config.adj.tpa.y, &ival );

			decodeVal( str, "mov", _INT, &config.adj.mov, &ival);

			dval = 1.0;
			decodeVal( str, "grayGamma",  _FLOAT, &config.adj.graygamma,&dval);
			decodeVal( str, "redGamma",   _FLOAT, &config.adj.rgamma, &dval );
			decodeVal( str, "greenGamma", _FLOAT, &config.adj.ggamma, &dval );
			decodeVal( str, "blueGamma",  _FLOAT, &config.adj.bgamma, &dval );
			continue;

		/* check for sections: */
		} else if( 0 == strncmp( str, _SECTION, strlen(_SECTION))) {

		    char *tmp;

			/* new section, try and attach previous device */
			if( config.devName[0] != '\0' ) {
				attach( config.devName, &config, 0 );
			} else {
				if( first_dev != NULL ) {
					DBG( _DBG_WARNING, "section contains no device name,"
					                   " ignored!\n" );
				 }
			}

			/* re-initialize the configuration structure */
			init_config_struct( &config );
		
			tmp = config.usbId;
			decodeUsbIDs( str, &tmp );
		
			DBG( _DBG_SANE_INIT, "... next device\n" );
			continue;

		} else if( SANE_TRUE == decodeDevName( str, config.devName )) {
			continue;
		}

		/* ignore other stuff... */
		DBG( _DBG_SANE_INIT, "ignoring >%s<\n", str );
	}
	fclose (fp);

	/* try to attach the last device in the config file... */
	if( config.devName[0] != '\0' )
		attach( config.devName, &config, 0 );

	return SANE_STATUS_GOOD;
}

/** cleanup the backend...
 */
void
sane_exit( void )
{
	DevList        *tmp;
	Plustek_Device *dev, *next;

	DBG( _DBG_SANE_INIT, "sane_exit\n" );

	for( dev = first_dev; dev; ) {

		next = dev->next;

		/* call the shutdown function of each device... */
		usbDev_shutdown( dev );

		/* we're doin' this to avoid compiler warnings as dev->sane.name
		 * is defined as const char*
		 */
		if( dev->sane.name )
			free( dev->name );

		if( dev->calFile )
			free( dev->calFile );
		
        if( dev->res_list )
			free( dev->res_list );
		free( dev );

		dev = next;
	}

	if( devlist )
		free( devlist );

	while( usbDevs ) {
		tmp = usbDevs->next;
		free( usbDevs );
		usbDevs = tmp;
	}

	usbDevs      = NULL;
	devlist      = NULL;
	auth         = NULL;
	first_dev    = NULL;
	first_handle = NULL;
}

/** return a list of all devices
 */
SANE_Status
sane_get_devices(const SANE_Device ***device_list, SANE_Bool local_only )
{
	int             i;
	Plustek_Device *dev;

	DBG(_DBG_SANE_INIT, "sane_get_devices (%p, %ld)\n",
                                       (void *)device_list, (long) local_only);

	/* already called, so cleanup */
	if( devlist )
		free( devlist );

	devlist = malloc((num_devices + 1) * sizeof (devlist[0]));
	if ( NULL == devlist )
		return SANE_STATUS_NO_MEM;

	i = 0;
	for (dev = first_dev; i < num_devices; dev = dev->next)
		devlist[i++] = &dev->sane;
	devlist[i++] = 0;

	*device_list = devlist;
	return SANE_STATUS_GOOD;
}

/** open the sane device
 */
SANE_Status
sane_open( SANE_String_Const devicename, SANE_Handle* handle )
{
	SANE_Status      status;
	Plustek_Device  *dev;
	Plustek_Scanner *s;
	CnfDef           config;

	DBG( _DBG_SANE_INIT, "sane_open - %s\n", devicename );

	if( devicename[0] ) {
		for( dev = first_dev; dev; dev = dev->next ) {
			if( strcmp( dev->sane.name, devicename ) == 0 )
				break;
		}

		if( !dev ) {

			memset(&config, 0, sizeof(CnfDef));

			status = attach( devicename, &config, &dev );
			if( SANE_STATUS_GOOD != status )
				return status;
		}
	} else {
		/* empty devicename -> use first device */
		dev = first_dev;
	}

	if( !dev )
		return SANE_STATUS_INVAL;

	s = malloc (sizeof (*s));
	if( NULL == s )
		return SANE_STATUS_NO_MEM;

	memset(s, 0, sizeof (*s));
	s->r_pipe      = -1;
	s->w_pipe      = -1;
	s->hw          = dev;
	s->scanning    = SANE_FALSE;
	s->calibrating = SANE_FALSE;

	init_options(s);

	/* insert newly opened handle into list of open handles: */
	s->next      = first_handle;
	first_handle = s;
	*handle      = s;

	return SANE_STATUS_GOOD;
}

/**
 */
void
sane_close( SANE_Handle handle )
{
	Plustek_Scanner *prev, *s = handle;

	DBG( _DBG_SANE_INIT, "sane_close\n" );

	if( s->calibrating )
		do_cancel( s, SANE_FALSE );

	/* remove handle from list of open handles: */
	prev = 0;

	for( s = first_handle; s; s = s->next ) {
		if( s == handle )
			break;
		prev = s;
	}

	if (!s) {
		DBG( _DBG_ERROR, "close: invalid handle %p\n", handle);
		return;
	}

	close_pipe( s );

	if( NULL != s->buf )
		free(s->buf);

	drvclose( s->hw );

	if (prev)
		prev->next = s->next;
	else
		first_handle = s->next;

	free(s);
}

/** goes through a string list and returns the start-address of the string
 * that has been found, or NULL on error
 */
static const SANE_String_Const*
search_string_list( const SANE_String_Const *list, SANE_String value )
{
	while( *list != NULL && strcmp(value, *list) != 0 )
		++list;

	if( *list == NULL )
		return NULL;

	return list;
}

/**
 */
static int
do_calibration( void *args )
{
	Plustek_Scanner *s    = (Plustek_Scanner *)args;
	Plustek_Device  *dev  = s->hw;
	DCapsDef        *caps = &dev->usbDev.Caps;
	int              scanmode, rc;
	int              modes[] = { COLOR_BW, COLOR_256GRAY, COLOR_GRAY16,
	                             COLOR_TRUE24, COLOR_TRUE48 };

	thread_entry();

	/* if the device does only support color scanning, there's no need
	 * to calibrate the gray modes
	 */
	if (caps->workaroundFlag & _WAF_GRAY_FROM_COLOR)
		scanmode = 3;
	else
		scanmode = 0;

	for ( ; scanmode < 5; scanmode++ ) {

		if (caps->workaroundFlag & _WAF_ONLY_8BIT) {

			if ((modes[scanmode] == COLOR_GRAY16) ||
			    (modes[scanmode] == COLOR_TRUE48)) {
				continue;
			}
		}
 
		dev->scanning.dwFlag |= SCANFLAG_Calibration;

		if (SANE_STATUS_GOOD == local_sane_start(s, modes[scanmode])) {

			/* prepare for scanning: speed-test, warmup, calibration */
			rc = usbDev_Prepare( dev, s->buf );
			if( rc != 0 || scanmode == 4) {
				if (rc != 0 )
					DBG(_DBG_INFO,"Calibration canceled!\n");
				m_fStart    = SANE_TRUE;
				m_fAutoPark = SANE_TRUE;
			}

			drvclose( dev );
			if( rc != 0 )
				break;
		} else {
			DBG(_DBG_ERROR, "local_sane_start() failed!\n");
			break;
		}
	}

	/* restore the settings */
	dev->scanning.dwFlag &= ~SCANFLAG_Calibration;
	s->calibrating = SANE_FALSE;
	return 0;
}

/** return or set the parameter values, also do some checks
 */
SANE_Status
sane_control_option( SANE_Handle handle, SANE_Int option,
                     SANE_Action action, void *value, SANE_Int *info )
{
	Plustek_Scanner         *s    = (Plustek_Scanner *)handle;
	Plustek_Device          *dev  = s->hw;
	AdjDef                  *adj  = &dev->adj;
	DCapsDef                *caps = &dev->usbDev.Caps;
	SANE_Status              status;
	const SANE_String_Const *optval;
	int                      scanmode;

	if (s->scanning)
		return SANE_STATUS_DEVICE_BUSY;

	/* in calibration mode, we do not allow setting any value! */
	if(s->calibrating) {
		if (action == SANE_ACTION_SET_VALUE) {
			if (option == OPT_CALIBRATE) {
				if( NULL != info )
					*info |= SANE_INFO_RELOAD_OPTIONS;
				do_cancel(s, SANE_TRUE);
				return SANE_STATUS_GOOD;
			}

			/* okay, we need some exceptions */
			switch (option) {
				case OPT_TL_X:
				case OPT_TL_Y:
				case OPT_BR_X:
				case OPT_BR_Y: break;
				default:       return SANE_STATUS_DEVICE_BUSY;
			}
		}
	}

	if ((option < 0) || (option >= NUM_OPTIONS))
		return SANE_STATUS_INVAL;

	if (NULL != info)
		*info = 0;

	switch( action ) {

		case SANE_ACTION_GET_VALUE:

			switch (option) {
			case OPT_PREVIEW:
			case OPT_NUM_OPTS:
			case OPT_RESOLUTION:
			case OPT_BIT_DEPTH:
			case OPT_TL_X:
			case OPT_TL_Y:
			case OPT_BR_X:
			case OPT_BR_Y:
			case OPT_LAMPSWITCH:
			case OPT_CUSTOM_GAMMA:
			case OPT_LAMPOFF_ONEND:
			case OPT_LOFF4DARK:
			case OPT_CACHECAL:
			case OPT_SPEEDUP:
			case OPT_OVR_REDGAIN:
			case OPT_OVR_GREENGAIN:
			case OPT_OVR_BLUEGAIN:
			case OPT_OVR_REDOFS:
			case OPT_OVR_GREENOFS:
			case OPT_OVR_BLUEOFS:
			case OPT_OVR_RED_LOFF:
			case OPT_OVR_GREEN_LOFF:
			case OPT_OVR_BLUE_LOFF:
			case OPT_LAMPOFF_TIMER:
			case OPT_WARMUPTIME:
				*(SANE_Word *)value = s->val[option].w;
				break;

			case OPT_BUTTON_0:
				if(!s->calibrating)
					usb_UpdateButtonStatus(s);
			case OPT_BUTTON_1:
			case OPT_BUTTON_2:
			case OPT_BUTTON_3:
			case OPT_BUTTON_4:
				/* copy the button state */
				*(SANE_Word*)value = s->val[option].w;
				/* clear the button state */
				s->val[option].w = SANE_FALSE;
				break;

			case OPT_CONTRAST:
			case OPT_BRIGHTNESS:
				*(SANE_Word *)value =
				                  (s->val[option].w << SANE_FIXED_SCALE_SHIFT);
				break;

			case OPT_MODE:
			case OPT_EXT_MODE:
				strcpy ((char *) value,
					  s->opt[option].constraint.string_list[s->val[option].w]);
				break;
	
			/* word array options: */
			case OPT_GAMMA_VECTOR:
				DBG( _DBG_INFO, "Reading MASTER gamma.\n" );
				memcpy( value, s->val[option].wa, s->opt[option].size );
				break;

			case OPT_GAMMA_VECTOR_R:
				DBG( _DBG_INFO, "Reading RED gamma.\n" );
				memcpy( value, s->val[option].wa, s->opt[option].size );
				break;

			case OPT_GAMMA_VECTOR_G:
				DBG( _DBG_INFO, "Reading GREEN gamma.\n" );
				memcpy( value, s->val[option].wa, s->opt[option].size );
				break;

			case OPT_GAMMA_VECTOR_B:
				DBG( _DBG_INFO, "Reading BLUE gamma.\n" );
				memcpy( value, s->val[option].wa, s->opt[option].size );
				break;
			default:
				return SANE_STATUS_INVAL;
		}
		break;

		case SANE_ACTION_SET_VALUE:
			status = sanei_constrain_value( s->opt + option, value, info );
			if( SANE_STATUS_GOOD != status )
				return status;

			optval = NULL;
			if( SANE_CONSTRAINT_STRING_LIST == s->opt[option].constraint_type ) {

				optval = search_string_list( s->opt[option].constraint.string_list,
								         (char *) value);
				if( NULL == optval )
					return SANE_STATUS_INVAL;
			}

			switch (option) {

				case OPT_RESOLUTION: {
					int n;
					int min_d = dev->res_list[dev->res_list_size - 1];
					int v     = *(SANE_Word *)value;
					int best  = v;

					for( n = 0; n < dev->res_list_size; n++ ) {
						int d = abs(v - dev->res_list[n]);

						if( d < min_d ) {
							min_d = d;
							best  = dev->res_list[n];
						}
					}

					s->val[option].w = (SANE_Word)best;

					if( v != best )
						*(SANE_Word *)value = best;

					if( NULL != info ) {
						if( v != best )	
							*info |= SANE_INFO_INEXACT;
						*info |= SANE_INFO_RELOAD_PARAMS;
					}
					break;
				}

				case OPT_PREVIEW:
				case OPT_BIT_DEPTH:
				case OPT_TL_X:
				case OPT_TL_Y:
				case OPT_BR_X:
				case OPT_BR_Y:
					s->val[option].w = *(SANE_Word *)value;
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_PARAMS;
					break;

				case OPT_CACHECAL:
					s->val[option].w = *(SANE_Word *)value;
					dev->adj.cacheCalData = s->val[option].w;
					if( !dev->adj.cacheCalData )
						_DISABLE(OPT_CALIBRATE);
					else {
						if( usb_IsCISDevice(dev) || dev->adj.altCalibrate)
							_ENABLE(OPT_CALIBRATE);
					}
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_OPTIONS;
					break;

				case OPT_CALIBRATE:
					if (s->calibrating) {
						do_cancel( s, SANE_FALSE );
						s->calibrating = SANE_FALSE;
					} else {
						sc = s;
						s->r_pipe      = -1;
						s->w_pipe      = -1;
						s->reader_pid  = sanei_thread_begin(do_calibration, s);
						s->calibrating = SANE_TRUE;
						signal( SIGCHLD, sig_chldhandler );
					}
					if (NULL != info)
						*info |= SANE_INFO_RELOAD_OPTIONS;
					break;

				case OPT_SPEEDUP:
					s->val[option].w = *(SANE_Word *)value;
					dev->adj.disableSpeedup = !(s->val[option].w);
					break;

				case OPT_LOFF4DARK:
					s->val[option].w = *(SANE_Word *)value;
					dev->adj.skipDarkStrip = !(s->val[option].w);
					break;

				case OPT_LAMPSWITCH:
					s->val[option].w = *(SANE_Word *)value;
					usb_LampSwitch( dev, s->val[option].w );
					if( s->val[option].w == 0 )
						usb_StopLampTimer( dev );
					else
						usb_StartLampTimer( dev );
					break;

				case OPT_LAMPOFF_ONEND:
					s->val[option].w = *(SANE_Word *)value;
					dev->adj.lampOffOnEnd = s->val[option].w;
					usb_CheckAndCopyAdjs( dev );
					break;

				case OPT_CUSTOM_GAMMA:
					s->val[option].w = *(SANE_Word *)value;
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

					scanmode = getScanMode( s );

					_DISABLE(OPT_GAMMA_VECTOR);
					_DISABLE(OPT_GAMMA_VECTOR_R);
					_DISABLE(OPT_GAMMA_VECTOR_G);
					_DISABLE(OPT_GAMMA_VECTOR_B);

					if( SANE_TRUE == s->val[option].w ) {
						DBG( _DBG_INFO, "Using custom gamma settings.\n" );
						if((scanmode == COLOR_256GRAY) ||
						   (scanmode == COLOR_GRAY16)) {
							_ENABLE(OPT_GAMMA_VECTOR);
						} else {
							_ENABLE(OPT_GAMMA_VECTOR_R);
							_ENABLE(OPT_GAMMA_VECTOR_G);
							_ENABLE(OPT_GAMMA_VECTOR_B);
						}
					} else {

						DBG( _DBG_INFO, "NOT using custom gamma settings.\n" );
						initGammaSettings( s );

						if((scanmode == COLOR_256GRAY) ||
						   (scanmode == COLOR_GRAY16)) {
							_DISABLE(OPT_GAMMA_VECTOR);
						} else {
							_DISABLE(OPT_GAMMA_VECTOR_R);
							_DISABLE(OPT_GAMMA_VECTOR_G);
							_DISABLE(OPT_GAMMA_VECTOR_B);
						}
					}
					break;

				case OPT_LAMPOFF_TIMER:
					s->val[option].w = (*(SANE_Word *)value);
					adj->lampOff     = (*(SANE_Word *)value);
					usb_CheckAndCopyAdjs( dev );
					break;

				case OPT_WARMUPTIME:
					s->val[option].w = (*(SANE_Word *)value);
					adj->warmup      = (*(SANE_Word *)value);
					usb_CheckAndCopyAdjs( dev );
					break;

				case OPT_OVR_REDGAIN:
					s->val[option].w = (*(SANE_Word *)value);
					adj->rgain       = (*(SANE_Word *)value);
					break;
				case OPT_OVR_GREENGAIN:
					s->val[option].w = (*(SANE_Word *)value);
					adj->ggain       = (*(SANE_Word *)value);
					break;
				case OPT_OVR_BLUEGAIN:
					s->val[option].w = (*(SANE_Word *)value);
					adj->bgain       = (*(SANE_Word *)value);
					break;
				case OPT_OVR_REDOFS:
					s->val[option].w = (*(SANE_Word *)value);
					adj->rofs        = (*(SANE_Word *)value);
					break;
				case OPT_OVR_GREENOFS:
					s->val[option].w = (*(SANE_Word *)value);
					adj->gofs       = (*(SANE_Word *)value);
					break;
				case OPT_OVR_BLUEOFS:
					s->val[option].w = (*(SANE_Word *)value);
					adj->bofs        = (*(SANE_Word *)value);
					break;
				case OPT_OVR_RED_LOFF:
					s->val[option].w = (*(SANE_Word *)value);
					adj->rlampoff    = (*(SANE_Word *)value);
					break;
				case OPT_OVR_GREEN_LOFF:
					s->val[option].w = (*(SANE_Word *)value);
					adj->glampoff    = (*(SANE_Word *)value);
					break;
				case OPT_OVR_BLUE_LOFF:
					s->val[option].w = (*(SANE_Word *)value);
					adj->blampoff    = (*(SANE_Word *)value);
					break;

				case OPT_CONTRAST:
				case OPT_BRIGHTNESS:
					s->val[option].w =
					     ((*(SANE_Word *)value) >> SANE_FIXED_SCALE_SHIFT);
					break;

				case OPT_MODE: 
					s->val[option].w = optval - s->opt[option].constraint.string_list;
					scanmode = getScanMode( s );
					
					_ENABLE(OPT_CONTRAST);
					_ENABLE(OPT_BIT_DEPTH);
					_ENABLE(OPT_CUSTOM_GAMMA);
					if (scanmode == COLOR_BW) {
						_DISABLE(OPT_CONTRAST);
						_DISABLE(OPT_CUSTOM_GAMMA);
						_DISABLE(OPT_BIT_DEPTH);
					}

					if (caps->workaroundFlag & _WAF_ONLY_8BIT)
						_DISABLE(OPT_BIT_DEPTH);

					_DISABLE(OPT_GAMMA_VECTOR);
					_DISABLE(OPT_GAMMA_VECTOR_R);
					_DISABLE(OPT_GAMMA_VECTOR_G);
					_DISABLE(OPT_GAMMA_VECTOR_B);

					if( s->val[OPT_CUSTOM_GAMMA].w &&
						!(s->opt[OPT_CUSTOM_GAMMA].cap & SANE_CAP_INACTIVE)) {

						if((scanmode == COLOR_256GRAY) ||
						   (scanmode == COLOR_GRAY16)) {
							_ENABLE(OPT_GAMMA_VECTOR);
						} else {
							_ENABLE(OPT_GAMMA_VECTOR_R);
							_ENABLE(OPT_GAMMA_VECTOR_G);
							_ENABLE(OPT_GAMMA_VECTOR_B);
						}
					}
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
					break;

				case OPT_EXT_MODE: {
					s->val[option].w = optval - s->opt[option].constraint.string_list;

					/* change the area and mode_list when changing the source
					 */
					if( s->val[option].w == 0 ) {
						dev->scanning.sParam.bSource = SOURCE_Reflection;

						dev->dpi_range.min = _DEF_DPI;

						dev->x_range.max = SANE_FIX(dev->max_x);
						dev->y_range.max = SANE_FIX(dev->max_y);
						s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_TLX);
						s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_TLY);
						s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_BRX);
						s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_BRY);

						s->opt[OPT_MODE].constraint.string_list = mode_list;
						s->val[OPT_MODE].w = 2; /* HEINER COLOR_TRUE24;*/

					} else {

						dev->dpi_range.min = _TPAMinDpi;

						if( s->val[option].w == 1 ) {

							dev->scanning.sParam.bSource = SOURCE_Transparency;
							if( dev->usbDev.Caps.wFlags & DEVCAPSFLAG_LargeTPA ) {
								dev->x_range.max = SANE_FIX(_SCALE(_TPALargePageWidth));
								dev->y_range.max = SANE_FIX(_SCALE(_TPALargePageHeight));
							} else {
								dev->x_range.max = SANE_FIX(_SCALE(_TPAPageWidth));
								dev->y_range.max = SANE_FIX(_SCALE(_TPAPageHeight));
							}
							s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_TP_TLX);
							s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_TP_TLY);
							s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_TP_BRX);
							s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_TP_BRY);

						} else {
							dev->scanning.sParam.bSource = SOURCE_Negative;
							if( dev->usbDev.Caps.wFlags & DEVCAPSFLAG_LargeTPA ) {
								dev->x_range.max = SANE_FIX(_SCALE(_NegLargePageWidth));
								dev->y_range.max = SANE_FIX(_SCALE(_NegLargePageHeight));
							} else {
								dev->x_range.max = SANE_FIX(_SCALE(_NegPageWidth));
								dev->y_range.max = SANE_FIX(_SCALE(_NegPageHeight));
							}
							s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_NEG_TLX);
							s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_NEG_TLY);
							s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_NEG_BRX);
							s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_NEG_BRY);
						}
						s->opt[OPT_MODE].constraint.string_list = &mode_list[2];
						s->val[OPT_MODE].w = 0;  /* COLOR_24 is the default */
					}
					if( s->val[OPT_LAMPSWITCH].w != 0 ) {
						usb_LampSwitch( dev, s->val[OPT_LAMPSWITCH].w );
						if( s->val[OPT_LAMPSWITCH].w == 0 )
							usb_StopLampTimer( dev );
						else
							usb_StartLampTimer( dev );
					}

					_ENABLE(OPT_CONTRAST);
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
					break;
				}
				case OPT_GAMMA_VECTOR:
					DBG( _DBG_INFO, "Setting MASTER gamma.\n" );
					memcpy( s->val[option].wa, value, s->opt[option].size );
					checkGammaSettings(s);
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_PARAMS;
					break;

				case OPT_GAMMA_VECTOR_R:
					DBG( _DBG_INFO, "Setting RED gamma.\n" );
					memcpy( s->val[option].wa, value, s->opt[option].size );
					checkGammaSettings(s);
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_PARAMS;
					break;

				case OPT_GAMMA_VECTOR_G:
					DBG( _DBG_INFO, "Setting GREEN gamma.\n" );
					memcpy( s->val[option].wa, value, s->opt[option].size );
					checkGammaSettings(s);
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_PARAMS;
					break;

				case OPT_GAMMA_VECTOR_B:
					DBG( _DBG_INFO, "Setting BLUE gamma.\n" );
					memcpy( s->val[option].wa, value, s->opt[option].size );
					checkGammaSettings(s);
					if( NULL != info )
						*info |= SANE_INFO_RELOAD_PARAMS;
					break;
				default:
					return SANE_STATUS_INVAL;
			}
			break;

		default:
			return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
}

/** according to the option number, return a pointer to a descriptor
 */
const SANE_Option_Descriptor*
sane_get_option_descriptor( SANE_Handle handle, SANE_Int option )
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	if((option < 0) || (option >= NUM_OPTIONS))
		return NULL;

	return &(s->opt[option]);
}

/** return the current parameter settings
 */
SANE_Status
sane_get_parameters( SANE_Handle handle, SANE_Parameters *params )
{
	int ndpi;
	int scanmode;
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	/* if we're calling from within, calc best guess
	 * do the same, if sane_get_parameters() is called
	 * by a frontend before sane_start() is called
	 */
	if((NULL == params) || (s->scanning != SANE_TRUE)) {

		memset(&s->params, 0, sizeof (SANE_Parameters));

		ndpi = s->val[OPT_RESOLUTION].w;

		s->params.pixels_per_line = SANE_UNFIX(s->val[OPT_BR_X].w -
		                      s->val[OPT_TL_X].w) / MM_PER_INCH * ndpi;

		s->params.lines = SANE_UNFIX( s->val[OPT_BR_Y].w -
		                      s->val[OPT_TL_Y].w) / MM_PER_INCH * ndpi;

		/* pixels_per_line seems to be 8 * n.  */
		/* s->params.pixels_per_line = s->params.pixels_per_line & ~7; debug only */

		s->params.last_frame = SANE_TRUE;
		scanmode = getScanMode( s );

		if( scanmode == COLOR_TRUE24 || scanmode == COLOR_TRUE48 ) {
			s->params.format = SANE_FRAME_RGB;
			s->params.bytes_per_line = 3 * s->params.pixels_per_line;
		} else {
			s->params.format = SANE_FRAME_GRAY;
			if (s->params.depth == 1)
				s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
			else
				s->params.bytes_per_line = s->params.pixels_per_line *
				                                           s->params.depth / 8;
		}

		/* if sane_get_parameters() was called before sane_start() */
		/* pass new values to the caller                           */
		if ((NULL != params) && (s->scanning != SANE_TRUE))
			*params = s->params;
	} else {
		*params = s->params;
	}
	return SANE_STATUS_GOOD;
}

/** initiate the scan process
 */
static SANE_Status
local_sane_start(Plustek_Scanner *s, int scanmode )
{
	Plustek_Device *dev;

	int       result;
	int       ndpi;
	int       left, top;
	int       width, height;
	double    dpi_x, dpi_y;
	CropInfo  crop;
	ScanInfo  sinfo;
	SANE_Word tmp;

	/* clear it out just in case */
	memset(&crop, 0, sizeof(crop));

	dev = s->hw;

	/* check if we're called from the option dialog! */
	if (usb_InCalibrationMode(dev))
		crop.ImgDef.dwFlag = SCANFLAG_Calibration;

	/* open the driver and get some information about the scanner
	 */
	dev->fd = usbDev_open( dev, NULL, SANE_TRUE );
	if( dev->fd < 0 ) {
		DBG( _DBG_ERROR, "sane_start: open failed: %d\n", errno);

		if( errno == EBUSY )
			return SANE_STATUS_DEVICE_BUSY;

		return SANE_STATUS_IO_ERROR;
	}

	result = usbDev_getCaps( dev );
	if( result < 0 ) {
		DBG( _DBG_ERROR, "usbDev_getCaps() failed(%d)\n", result);
		sanei_access_unlock( dev->sane.name );
		usbDev_close( dev );
		return SANE_STATUS_IO_ERROR;
	}

	/* All ready to go.  Set image def and see what the scanner
	 * says for crop info.
	 */
	ndpi = s->val[OPT_RESOLUTION].w;

	/* exchange the values as we can't deal with
	 * negative heights and so on...*/
	tmp = s->val[OPT_TL_X].w;
	if( tmp > s->val[OPT_BR_X].w ) {
		DBG( _DBG_INFO, "exchanging BR-X - TL-X\n" );
		s->val[OPT_TL_X].w = s->val[OPT_BR_X].w;
		s->val[OPT_BR_X].w = tmp;
	}

	tmp = s->val[OPT_TL_Y].w;
	if( tmp > s->val[OPT_BR_Y].w ) {
		DBG( _DBG_INFO, "exchanging BR-Y - TL-Y\n" );
		s->val[OPT_TL_Y].w = s->val[OPT_BR_Y].w;
		s->val[OPT_BR_Y].w = tmp;
	}

	/* position and extent are always relative to 300 dpi */
	dpi_x = (double)dev->usbDev.Caps.OpticDpi.x; 
	dpi_y = (double)dev->usbDev.Caps.OpticDpi.x * 2;
	
	left   = (int)(SANE_UNFIX (s->val[OPT_TL_X].w)*dpi_x/
	                                            (MM_PER_INCH*(dpi_x/300.0)));
	top    = (int)(SANE_UNFIX (s->val[OPT_TL_Y].w)*dpi_y/
	                                            (MM_PER_INCH*(dpi_y/300.0)));
	width  = (int)(SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w) *
	                                    dpi_x / (MM_PER_INCH *(dpi_x/300.0)));
	height = (int)(SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w) *
	                                    dpi_y / (MM_PER_INCH *(dpi_y/300.0)));

	/* adjust mode list according to the model we use and the
	 * source we have
	 */
	DBG( _DBG_INFO, "scanmode = %u\n", scanmode );

	crop.ImgDef.xyDpi.x   = ndpi;
	crop.ImgDef.xyDpi.y   = ndpi;
	crop.ImgDef.crArea.x  = left;  /* offset from left edge to area you want to scan */
	crop.ImgDef.crArea.y  = top;   /* offset from top edge to area you want to scan  */
	crop.ImgDef.crArea.cx = width; /* always relative to 300 dpi */
	crop.ImgDef.crArea.cy = height;
	crop.ImgDef.wDataType = scanmode;
	crop.ImgDef.dwFlag   |= SCANDEF_QualityScan;

	switch( s->val[OPT_EXT_MODE].w ) {
		case 1: crop.ImgDef.dwFlag |= SCANDEF_Transparency; break;
		case 2: crop.ImgDef.dwFlag |= SCANDEF_Negative;     break;
		default: break;
	}

	result = usbDev_getCropInfo( dev, &crop );
	if( result < 0 ) {
		DBG( _DBG_ERROR, "usbDev_getCropInfo() failed(%d)\n", result );
		usbDev_close( dev );
		sanei_access_unlock( dev->sane.name );
		return SANE_STATUS_IO_ERROR;
	}

	/* DataInf.dwAppPixelsPerLine = crop.dwPixelsPerLine;  */
	s->params.pixels_per_line = crop.dwPixelsPerLine;
	s->params.bytes_per_line  = crop.dwBytesPerLine;
	s->params.lines           = crop.dwLinesPerArea;

	/* build a SCANINFO block and get ready to scan it */
	crop.ImgDef.dwFlag |= SCANDEF_QualityScan;

	/* remove that for preview scans */
	if( s->val[OPT_PREVIEW].w )
		crop.ImgDef.dwFlag &= (~SCANDEF_QualityScan);

	/* set adjustments for brightness and contrast */
	sinfo.siBrightness = s->val[OPT_BRIGHTNESS].w;
	sinfo.siContrast   = s->val[OPT_CONTRAST].w;

	memcpy( &sinfo.ImgDef, &crop.ImgDef, sizeof(ImgDef));
	
	DBG( _DBG_SANE_INIT, "brightness %i, contrast %i\n",
	                      sinfo.siBrightness, sinfo.siContrast );

	result = usbDev_setScanEnv( dev, &sinfo );
	if( result < 0 ) {
		DBG( _DBG_ERROR, "usbDev_setScanEnv() failed(%d)\n", result );
		usbDev_close( dev );
		sanei_access_unlock( dev->sane.name );
		return SANE_STATUS_IO_ERROR;
	}

	/* download gamma correction tables... */
	if( scanmode <= COLOR_GRAY16 ) {
		usbDev_setMap( dev, s->gamma_table[0], s->gamma_length, _MAP_MASTER);
	} else {
		usbDev_setMap( dev, s->gamma_table[1], s->gamma_length, _MAP_RED   );
		usbDev_setMap( dev, s->gamma_table[2], s->gamma_length, _MAP_GREEN );
		usbDev_setMap( dev, s->gamma_table[3], s->gamma_length, _MAP_BLUE  );
	}

	tsecs = 0; /* reset timer */

	result = usbDev_startScan( dev );
	if( result < 0 ) {
		DBG( _DBG_ERROR, "usbDev_startScan() failed(%d)\n", result );
		usbDev_close( dev );
		sanei_access_unlock( dev->sane.name );
		return SANE_STATUS_IO_ERROR;
	}

	DBG( _DBG_SANE_INIT, "dwflag = 0x%lx dwBytesLine = %ld\n",
	                      dev->scanning.dwFlag, dev->scanning.dwBytesLine );
	DBG( _DBG_SANE_INIT, "Lines          = %d\n", s->params.lines);
	DBG( _DBG_SANE_INIT, "Bytes per Line = %d\n", s->params.bytes_per_line );
	DBG( _DBG_SANE_INIT, "Bitdepth       = %d\n", s->params.depth );

	if (usb_InCalibrationMode(dev)) {
		if (s->buf)
			free(s->buf);
		s->buf = NULL;
	} else {

		if (s->params.lines == 0 || s->params.bytes_per_line == 0) {
			DBG( _DBG_ERROR, "nothing to scan!\n" );
			usbDev_close( dev );
			sanei_access_unlock( dev->sane.name );
			return SANE_STATUS_INVAL;
		}

		s->buf = realloc( s->buf, (s->params.lines) * s->params.bytes_per_line );
		if( NULL == s->buf ) {
			DBG( _DBG_ERROR, "realloc failed\n" );
			usbDev_close( dev );
			sanei_access_unlock( dev->sane.name );
			return SANE_STATUS_NO_MEM;
		}
	}

	tsecs = (unsigned long)time(NULL);
	DBG( _DBG_INFO, "TIME START\n" );

	DBG( _DBG_SANE_INIT, "local_sane_start done\n" );
	return SANE_STATUS_GOOD;
}

/** initiate the scan process
 */
SANE_Status
sane_start( SANE_Handle handle )
{
	Plustek_Scanner *s   = (Plustek_Scanner *)handle;
	Plustek_Device  *dev = s->hw;
	SANE_Status      status;
	int              fds[2];

	DBG( _DBG_SANE_INIT, "sane_start\n" );

	if (s->scanning)
		return SANE_STATUS_DEVICE_BUSY;

	/* in the end we wait until the calibration is done... */
	if (s->calibrating) {
		while (s->calibrating) {
			sleep(1);
		}

		/* we have been cancelled? */
		if (cancelRead)
			return SANE_STATUS_CANCELLED;
	}

	status = sane_get_parameters (handle, NULL);
	if (status != SANE_STATUS_GOOD) {
		DBG( _DBG_ERROR, "sane_get_parameters failed\n" );
		return status;
	}

	status = local_sane_start(s, getScanMode(s));
	if (status != SANE_STATUS_GOOD) {
		return status;
	}

	s->scanning = SANE_TRUE;

	/*
	 * everything prepared, so start the child process and a pipe to communicate
	 * pipe --> fds[0]=read-fd, fds[1]=write-fd
	 */
	if( pipe(fds) < 0 ) {
		DBG( _DBG_ERROR, "ERROR: could not create pipe\n" );
	    s->scanning = SANE_FALSE;
		usbDev_close( dev );
		return SANE_STATUS_IO_ERROR;
	}

	/* create reader routine as new process */
	s->bytes_read    = 0;
	s->r_pipe        = fds[0];
	s->w_pipe        = fds[1];
	s->ipc_read_done = SANE_FALSE;
	s->reader_pid    = sanei_thread_begin( reader_process, s );

	cancelRead = SANE_FALSE;
	
	if( s->reader_pid == -1 ) {
		DBG( _DBG_ERROR, "ERROR: could not start reader task\n" );
		s->scanning = SANE_FALSE;
		usbDev_close( dev );
		return SANE_STATUS_IO_ERROR;
	}

	signal( SIGCHLD, sig_chldhandler );

	if( sanei_thread_is_forked()) {
		close( s->w_pipe );
		s->w_pipe = -1;
	}

	DBG( _DBG_SANE_INIT, "sane_start done\n" );
	return SANE_STATUS_GOOD;
}

/** function to read the data from our child process
 */
SANE_Status
sane_read( SANE_Handle handle, SANE_Byte *data,
           SANE_Int max_length, SANE_Int *length )
{
	Plustek_Scanner *s = (Plustek_Scanner*)handle;
	ssize_t          nread;
#ifdef USE_IPC
	static	 IPCDef       ipc;
	unsigned char        *buf;
	static unsigned long  c = 0;
#endif

	*length = 0;

#ifdef USE_IPC
	/* first try and read IPC... */
	if( !s->ipc_read_done ) {

		buf = (unsigned char*)&ipc;
		for( c = 0; c < sizeof(ipc); ) {
			nread = read( s->r_pipe, buf, sizeof(ipc));
			if( nread < 0 ) {
				if( EAGAIN != errno ) {
					do_cancel( s, SANE_TRUE );
					return SANE_STATUS_IO_ERROR;
				} else {
					return SANE_STATUS_GOOD;
				}
			} else {
				c   += nread;
				buf += nread;
				if( c == sizeof(ipc)) {
					s->ipc_read_done = SANE_TRUE;
					break;
				}
			}
		}
		s->hw->transferRate = ipc.transferRate;
		DBG( _DBG_INFO, "IPC: Transferrate = %lu Bytes/s\n", 
		     ipc.transferRate );
	}
#endif
	/* here we read all data from the driver... */
	nread = read( s->r_pipe, data, max_length );
	DBG( _DBG_READ, "sane_read - read %ld bytes\n", (long)nread );
	if (!(s->scanning)) {
		return do_cancel( s, SANE_TRUE );
	}

	if( nread < 0 ) {

		if( EAGAIN == errno ) {

			/* if we already had red the picture, so it's okay and stop */
			if( s->bytes_read ==
				(unsigned long)(s->params.lines * s->params.bytes_per_line)) {
				sanei_thread_waitpid( s->reader_pid, 0 );
				s->reader_pid = -1;
				s->scanning = SANE_FALSE;
				drvclose( s->hw );
				return close_pipe(s);
			}

			/* else force the frontend to try again*/
			return SANE_STATUS_GOOD;

		} else {
			DBG( _DBG_ERROR, "ERROR: errno=%d\n", errno );
			do_cancel( s, SANE_TRUE );
			return SANE_STATUS_IO_ERROR;
		}
	}

	*length        = nread;
	s->bytes_read += nread;

	/* nothing red means that we're finished OR we had a problem... */
	if( 0 == nread ) {

		drvclose( s->hw );
		s->exit_code = sanei_thread_get_status( s->reader_pid );

		if( SANE_STATUS_GOOD != s->exit_code ) {
			close_pipe(s);
			return s->exit_code;
		}
		s->reader_pid = -1;
		s->scanning = SANE_FALSE;
		return close_pipe(s);
	}
	return SANE_STATUS_GOOD;
}

/** cancel the scanning process
 */
void
sane_cancel( SANE_Handle handle )
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	DBG( _DBG_SANE_INIT, "sane_cancel\n" );

	if (s->scanning || s->calibrating)
		do_cancel( s, SANE_FALSE );
}

/** set the pipe to blocking/non blocking mode
 */
SANE_Status
sane_set_io_mode( SANE_Handle handle, SANE_Bool non_blocking )
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	DBG( _DBG_SANE_INIT, "sane_set_io_mode: non_blocking=%d\n",non_blocking );

	if ( !s->scanning ) {
		DBG( _DBG_ERROR, "ERROR: not scanning !\n" );
		return SANE_STATUS_INVAL;
	}

	if( -1 == s->r_pipe ) {
		DBG( _DBG_ERROR, "ERROR: not supported !\n" );
		return SANE_STATUS_UNSUPPORTED;
	}
	
	if( fcntl (s->r_pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0) {
		DBG( _DBG_ERROR, "ERROR: could not set to non-blocking mode !\n" );
		return SANE_STATUS_IO_ERROR;
	}

	DBG( _DBG_SANE_INIT, "sane_set_io_mode done\n" );
	return SANE_STATUS_GOOD;
}

/** return the descriptor if available
 */
SANE_Status
sane_get_select_fd( SANE_Handle handle, SANE_Int * fd )
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	DBG( _DBG_SANE_INIT, "sane_get_select_fd\n" );

	if( !s->scanning ) {
		DBG( _DBG_ERROR, "ERROR: not scanning !\n" );
		return SANE_STATUS_INVAL;
	}

	*fd = s->r_pipe;

	DBG( _DBG_SANE_INIT, "sane_get_select_fd done\n" );
	return SANE_STATUS_GOOD;
}

/* END PLUSTEK.C ............................................................*/
