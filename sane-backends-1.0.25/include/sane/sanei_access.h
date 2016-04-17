/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Gerhard Jaeger <gerhard@gjaeger.de>

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

/** @file sanei_access.h
 *  Interface file for the sanei_access functions.
 *
 *  The idea is to provide some simple locking/unlocking mecanism, which
 *  can be used to protect device access from more than one frontend 
 *  simultaneously.
 */

#ifndef sanei_access_h
#define sanei_access_h

#include "../include/sane/config.h"
#include "../include/sane/sane.h"

/** Initialize sanei_access.
 *
 * This function must be called before any other sanei_access function.
 *
 * @param backend - backend name, who uses this lib
 */
extern void sanei_access_init( const char * backend );

/** Set a lock.
 *
 * The function tries to open/create exclusively a lock file in 
 * $PATH_SANE_LOCK_DIR.
 * If the file could be created successfully, the function fills in the
 * process ID.
 * The complete filename of the lockfile is created as follows:
 * $PATH_SANE_LOCK_DIR/LCK..&lt;devicename&gt;
 * If the lock could not be set, the function tries it until the timeout
 * period has been elapsed.
 *
 * @param devicename - unique part of the lockfile name
 * @param timeout    - time in seconds to try to set a lock
 * @return  
 * - SANE_STATUS_GOOD          - if the lock has been successfully set
 * - SANE_STATUS_ACCESS_DENIED - the lock could not set
 */
extern SANE_Status sanei_access_lock( const char * devicename, SANE_Word timeout );

/** Unlock a previously set lock.
 *
 * The function tries to unlock a previously created lock. The lockfile will be
 * closed and removed.
 *
 * @param devicename - part of the lockfile name, use for sanei_acess_lock()
 * @return
 * - SANE_STATUS_GOOD - currently the one and only return value
 */
extern SANE_Status sanei_access_unlock( const char * devicename );

#endif /* sanei_access_h */
