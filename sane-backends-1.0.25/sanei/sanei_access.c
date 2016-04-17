/* sane - Scanner Access Now Easy.
   Copyright (C) 2005 Gerhard Jaeger
   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <fcntl.h>

#define BACKEND_NAME sanei_access     /**< name of this module for debugging */

#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_access.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#if defined(_WIN32) || defined(HAVE_OS2_H)
# define PATH_SEP	'\\'
#else
# define PATH_SEP	'/'
#endif

#define REPLACEMENT_CHAR '_'

#define PID_BUFSIZE  50

#define PROCESS_SELF  0
#define PROCESS_DEAD -1
#define PROCESS_OTHER 1


#ifdef ENABLE_LOCKING
/** get the status/owner of a lock file
 *
 * The function tries to open an existing lockfile. On success, it reads out 
 * the pid which is stored inside and tries to find out more about the status
 * of the process with the corresponding PID.
 * 
 * @param fn - the complete filename of the lockfile to check
 * @return
 * - PROCESS_SELF  - the calling process is owner of the lockfile
 * - PROCESS_DEAD  - the process who created the lockfile is already dead
 * - PROCESS_OTHER - the process who created the lockfile is still alive
 */
static int
get_lock_status( char *fn )
{
	char  pid_buf[PID_BUFSIZE];
	int   fd, status;
	pid_t pid;
	
	fd = open( fn, O_RDONLY );
	if( fd < 0 ) {
		DBG( 2, "does_process_exist: open >%s< failed: %s\n", 
		        fn, strerror(errno));
		return PROCESS_OTHER;
	}
	read( fd, pid_buf, (PID_BUFSIZE-1));
	pid_buf[PID_BUFSIZE-1] = '\0';
	close( fd );

	pid_buf[24] = '\0';
	pid = strtol( pid_buf, NULL, 10 );
	DBG( 2, "does_process_exist: PID %i\n", pid );

	status = kill( pid, 0 );
	if( status == -1 ) {
		if( errno == ESRCH ) {
			DBG( 2, "does_process_exist: process %i does not exist!\n", pid );
			return PROCESS_DEAD;
		}
		DBG( 1, "does_process_exist: kill failed: %s\n", strerror(errno));
	} else {
		DBG( 2, "does_process_exist: process %i does exist!\n", pid );
		if( pid == getpid()){
			DBG( 2, "does_process_exist: it's me!!!\n" );
			return PROCESS_SELF;
		}
	}
	return PROCESS_OTHER;
}

static void
create_lock_filename( char *fn, const char *devname )
{
	char *p;

	strcpy( fn, STRINGIFY(PATH_SANE_LOCK_DIR)"/LCK.." );
	p = &fn[strlen(fn)];

	strcat( fn, devname );

	while( *p != '\0' ) {
		if( *p == PATH_SEP )
			*p = REPLACEMENT_CHAR;
		p++;
	}
	DBG( 2, "sanei_access: lockfile name >%s<\n", fn );
}
#endif

void
sanei_access_init( const char *backend )
{
	DBG_INIT();

	DBG( 2, "sanei_access_init: >%s<\n", backend);
}

SANE_Status 
sanei_access_lock( const char *devicename, SANE_Word timeout )
{
#ifdef ENABLE_LOCKING
	char fn[PATH_MAX];
	char pid_buf[PID_BUFSIZE];
	int  fd, to, i;
#endif

	DBG( 2, "sanei_access_lock: devname >%s<, timeout: %u\n", 
	        devicename, timeout );
#ifndef ENABLE_LOCKING
	return SANE_STATUS_GOOD;
#else
	to = timeout;
	if (to <= 0)
		to = 1;

	create_lock_filename( fn, devicename );

	for (i = 0; i < to; i++) {

		fd = open( fn, O_CREAT | O_EXCL | O_WRONLY, 0644 );
		if (fd < 0) {
	
			if (errno == EEXIST) {
				switch( get_lock_status( fn )) {
				case PROCESS_DEAD:
					DBG( 2, "sanei_access_lock: "
					        "deleting old lock file, retrying...\n" );
					unlink( fn );
					continue;
					break;
				case PROCESS_SELF:
					DBG( 2, "sanei_access_lock: success\n" );
					return SANE_STATUS_GOOD;
					break;
				default:
					break;
				}
				DBG( 2, "sanei_access_lock: lock exists, waiting...\n" );
				sleep(1);
			} else {
				DBG( 1, "sanei_access_lock: open >%s< failed: %s\n", 
				        fn, strerror(errno));
				return SANE_STATUS_ACCESS_DENIED;
			}
		} else {
			DBG( 2, "sanei_access_lock: success\n" );
			sprintf( pid_buf, "% 11i sane\n", getpid());
			write(fd, pid_buf, strlen(pid_buf));
			close( fd );
			return SANE_STATUS_GOOD;
		}
	}
	DBG( 1, "sanei_access_lock: timeout!\n");
	return SANE_STATUS_ACCESS_DENIED;
#endif
}

SANE_Status 
sanei_access_unlock( const char *devicename )
{
#ifdef ENABLE_LOCKING
	char fn[PATH_MAX];
#endif
	DBG( 2, "sanei_access_unlock: devname >%s<\n", devicename );
#ifdef ENABLE_LOCKING
	create_lock_filename( fn, devicename );
	unlink( fn );
#endif
	return SANE_STATUS_GOOD;
}

/* END sanei_access.c .......................................................*/
