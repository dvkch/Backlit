/* @file plustek-pp_procfs.c
 * @brief this is the interface to the proc filesystem
 *
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.37 - initial version
 * - 0.38 - changes according to generic structure changes
 * - 0.39 - added info about forceMode and slowIO
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - changed include names
 * - 0.43 - replace _PTDRV_VERx by _PTDRV_VERSTR
 *        - cleanup
 * - 0.44 - PROC_FS changes for newer kernel
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
#include <linux/proc_fs.h>

#include "plustek-pp_scan.h"

/* toggled by your kernel configuration */
#ifdef CONFIG_PROC_FS	

/****************************** static vars **********************************/

/** for the proc filesystem
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
extern struct proc_dir_entry proc_root;
#endif
static struct proc_dir_entry *base  = NULL;
static struct proc_dir_entry *binfo = NULL;
static ULong                  devcount;

/** parallel port modes... */
static char *procfsPortModes[] = {
	"EPP",
	"SPP",
	"BiDi (PS/2)",
	"ECP"
	"unknown",
	NULL
};

/** CCD-Types (as for ASIC 98001 based series) */
static TabDef procfsCCDTypes98001[] = {

	{ _CCD_3797, "3797" },
	{ _CCD_3717, "3717" },
	{ _CCD_535,  "535"  },
	{ _CCD_2556, "2556" },
	{ _CCD_518,  "518"  },
	{ _CCD_539,  "539"  },
    { -1 ,       "unknown" }
};

/** CCD-Types (as for ASIC 98003 based series) */
static TabDef procfsCCDTypes98003[] = {

	{ _CCD_3797, "3797" },
	{ _CCD_3799, "3799" },
	{ _CCD_535,  "535"  },
	{ _CCD_2556, "2556" },
	{ _CCD_518,  "518"  },
	{ _CCD_539,  "539"  },
    { _CCD_3777, "3777"	},
    { _CCD_548 , "548"	},
    { -1 ,       "unknown" }
};

/****************************** local functions ******************************/

#ifndef LINUX_24
/** This is called as the fill_inode function when an inode
 * is going into (fill = 1) or out of service (fill = 0).
 *
 * Note: only the top-level directory needs to do this; if
 * a lower level is referenced, the parent will be as well.
 *
 * Here simply a dummy function
 */
static void procfsFillFunc( struct inode *inode, int fill )
{
}
#endif

/** returns a pointer to the port-mode string
 */
static const char* procfsGetMode( int mode )
{
	if((mode < _PORT_EPP) || (mode > _PORT_ECP))
		return procfsPortModes[_PORT_ECP+1];

	return procfsPortModes[mode];
}

/** determines CCD-Type string
 */
static const char* procfsGetCCDType( pScanData ps )
{
	int     i;
	int     ccd_id = ps->Device.bCCDID;
	pTabDef tab = procfsCCDTypes98001;

	if( _IS_ASIC98(ps->sCaps.AsicID)) {

        if(_ASIC_IS_98003 == ps->sCaps.AsicID)
            tab = procfsCCDTypes98003;

        /* seek down the description table */
        for( i = 0; -1 != tab[i].id; i++ ) {

            if( tab[i].id == ccd_id )
                return tab[i].desc;
        }
    } else {

        /* for older scanners only this info is available */
        if( ps->fSonyCCD )
            return "SONY Type";
        else
            return "NEC/TOSHIBA Type";
    }

	/* return the last entry if nothing applies! */
	return tab[(sizeof(procfsCCDTypes98001)/sizeof(TabDef)-1)].desc;
}

/** will be called when reading the proc filesystem:
 * cat /proc/pt_drv/info
 */
static int procfsBInfoReadProc( char *buf, char **start, off_t offset,
                                int count, int *eof, void *data )
{
	int len = 0;

	len += sprintf( buf, "Plustek Flatbed Scanner Driver version "_PTDRV_VERSTR"\n" );
	len += sprintf( buf + len, "IOCTL-Version: 0x%08x\n",_PTDRV_IOCTL_VERSION);
	return len;
}

/** will be called when reading the proc filesystem:
 * cat /proc/pt_drv/deviceX/info
 */
static int procfsInfoReadProc( char *buf, char **start, off_t offset,
                               int count, int *eof, void *data )
{
	int       len = 0;
	pScanData ps  = (pScanData)data;

	/* Tell us something about the device... */
	if( NULL != ps ) {
		len += sprintf( buf+len, "Model       : %s\n",
					    MiscGetModelName(ps->sCaps.Model));
		len += sprintf( buf+len, "Portaddress : 0x%X\n", ps->IO.portBase );
		len += sprintf( buf+len, "Portmode    : %s (%s I/O, %s)\n",
					    procfsGetMode(ps->IO.portMode), 
						(ps->IO.slowIO == _TRUE?"delayed":"fast"),
                        (ps->IO.forceMode == 0?"autodetect":"forced"));
		len += sprintf( buf+len, "Buttons     : %u\n",  ps->Device.buttons);
		len += sprintf( buf+len, "Warmuptime  : %us\n", ps->warmup        );
		len += sprintf( buf+len, "Lamp timeout: %us\n", ps->lampoff       );
		len += sprintf( buf+len, "mov-switch  : %u\n",  ps->ModelOverride );
		len += sprintf( buf+len, "I/O-delay   : %u\n",  ps->IO.delay      );
		len += sprintf( buf+len, "CCD-Type    : %s\n",  procfsGetCCDType(ps));
        len += sprintf( buf+len, "TPA         : %s\n",
                        (ps->sCaps.dwFlag & SFLAG_TPA) ? "yes":"no" );
	}

	return len;
}

/** will be called when reading the proc filesystem:
 * cat /proc/pt_drv/devicex/buttony
 */
static int procfsButtonsReadProc( char *buf, char **start, off_t offset,
                                  int count, int *eof, void *data )
{
	Byte	  b;
	int       bc  = 0;
	int       len = 0;
	pScanData ps  = (pScanData)data;

	if( NULL != ps ) {
		bc = ps->Device.buttons;
	}

	/* Check the buttons... */
	if( 0 != bc ) {

		if ( _ASIC_IS_96003 == ps->sCaps.AsicID ) {
			MiscClaimPort( ps );
			b = IODataRegisterFromScanner( ps, ps->RegStatus );
			if(_FLAG_P96_KEY == (b & _FLAG_P96_KEY))
				b = 0;
			else
				b = 1;
			MiscReleasePort( ps );
			len += sprintf( buf + len, "%u\n", b );
		} else
			bc = 0;
	}

	if( 0 == bc )
		len += sprintf( buf + len, "none\n" );

	return len;
}

/** create a procfs entry
 */
static struct proc_dir_entry *new_entry( const char *name, mode_t mode,
                                         struct proc_dir_entry *parent )
{
#ifndef LINUX_24
	int len;
#endif
	struct proc_dir_entry *ent;

	if (mode == S_IFDIR)
		mode |= S_IRUGO | S_IXUGO;
	else if (mode == 0)
		mode = S_IFREG | S_IRUGO;

#ifndef LINUX_24
	len = strlen(name) + 1;

	/* allocate memory for the entry and the name */
	ent = kmalloc(sizeof(struct proc_dir_entry) + len, GFP_KERNEL);
	if( NULL == ent )
		return NULL;

	memset(ent, 0, sizeof(struct proc_dir_entry));

	/* position pointer of name to end of the structure*/
	ent->name = ((char *) ent) + sizeof(*ent);
	strcpy((char *)ent->name, name );
	
	ent->namelen = strlen(name);
	ent->mode    = mode;

	if (S_ISDIR(mode)) {
		ent->nlink      = 2;
		ent->fill_inode = &procfsFillFunc;
	} else {
		ent->nlink = 1;
	}

	proc_register( parent, ent );
#else
	if (mode == S_IFDIR)
		ent = proc_mkdir( name, parent );
	else
		ent = create_proc_entry( name, mode, parent );
#endif

	return ent;
}

/** shutdown one proc fs entry
 */
static inline void destroy_proc_entry( struct proc_dir_entry *root,
                                       struct proc_dir_entry **d )
{
#ifndef LINUX_24
	proc_unregister( root, (*d)->low_ino );
	kfree(*d);
#else
	DBG(DBG_HIGH, "pt_drv: proc del '%s' root='%s'\n", (*d)->name, root->name);
	
	remove_proc_entry((*d)->name, root );
#endif

	*d = NULL;
}

/** shutdown the proc-tree for one device
 */
static void destroy_proc_tree( pScanData ps )
{
	int i;

	DBG( DBG_HIGH, "pt_drv: destroy_proc_tree !\n" );

	if( ps ) {

		if( ps->procDir.entry ) {

			if( ps->procDir.info )
				destroy_proc_entry( ps->procDir.entry, &ps->procDir.info );

			for( i = 0; i < ps->Device.buttons; i++ ) {

				if( ps->procDir.buttons[i] )
    				destroy_proc_entry(ps->procDir.entry, &ps->procDir.buttons[i]);
			}

			destroy_proc_entry( base, &ps->procDir.entry );
		}
	}
}

/*************************** exported functions ******************************/

/** initialize our proc-fs stuff
 */
int ProcFsInitialize( void )
{
	DBG( DBG_HIGH, "ProcFsInitialize()\n" );

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
	base = new_entry( _DRV_NAME, S_IFDIR, &proc_root );
#else
	base = new_entry( _DRV_NAME, S_IFDIR, NULL );
#endif

	if( NULL != base ) {

		devcount = 0;

		binfo = new_entry( "info", 0, base );
		if( NULL != binfo ) {
			binfo->read_proc = procfsBInfoReadProc;
			binfo->data      = &devcount;
		}
	}

	return _OK;
}

/** cleanup the base entry
 */
void ProcFsShutdown( void )
{
	DBG( DBG_HIGH, "ProcFsShutdown()\n" );

	if( NULL != base ) {

		if( NULL != binfo )
			destroy_proc_entry( base, &binfo );

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
		destroy_proc_entry( &proc_root, &base );
#else
		destroy_proc_entry( NULL, &base );
#endif
	}

	devcount = 0;
}

/** will be called for each device, that has been found
 */
void ProcFsRegisterDevice( pScanData ps )
{
	int	 i;
	char str[20];

	if( NULL == base ) {
	    printk( KERN_ERR "pt_drv : proc not initialised yet!\n");
		return;
	}

	memset( &ps->procDir, 0, sizeof(ProcDirDef));

	sprintf( str, "device%u", ps->devno );
	
	ps->procDir.entry = new_entry( str, S_IFDIR, base );
	if( NULL == ps->procDir.entry )
		goto error_exit;

	ps->procDir.info = new_entry( "info", 0, ps->procDir.entry );
	if( NULL == ps->procDir.info )
		goto error_exit;

	ps->procDir.info->read_proc = procfsInfoReadProc;
	ps->procDir.info->data      = ps;

	for( i = 0; i < ps->Device.buttons; i++ ) {

		sprintf( str, "button%u", i );

		ps->procDir.buttons[i] = new_entry( str, 0, ps->procDir.entry );
		if( NULL == ps->procDir.buttons[i] )
			goto error_exit;

		ps->procDir.buttons[i]->read_proc = procfsButtonsReadProc;
		ps->procDir.buttons[i]->data      = ps;
	}

	devcount++;
	return;


error_exit:

	printk(KERN_ERR "pt_drv: failure registering /proc/ entry %s.\n", str );
	destroy_proc_tree( ps );
}

/** cleanup the proc-fs for a certain device
 */
void ProcFsUnregisterDevice( pScanData ps )
{
	destroy_proc_tree( ps );
}

#else 	/* CONFIG_PROC_FS */

int  ProcFsInitialize( void )
{
	return _OK;
}

void ProcFsShutdown( void )
{
}

void ProcFsRegisterDevice( pScanData ps )
{
}

void ProcFsUnregisterDevice( pScanData ps )
{
}

#endif

#endif /* guard __KERNEL__ */

/* END PLUSTEK-PP_PROCFS.C ..................................................*/
